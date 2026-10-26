#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/BrushRendering.hpp"
#include "gui/controls/RichText.hpp"
#include "gui/controls/TextBlockLayout.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Documents.hpp>
#include <cstdio>
#include <Aero/Controls.hpp>
#include <Aero/Controls/Selectors.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/TryCast.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/Describe.hpp"
#include "gui/controls/ControlPropertyValidators.hpp"
#include <Aero/Media/Brushes.hpp>

namespace Aero::Controls {

struct TextBlockDocumentHelper {
public:
    static bool IsTextBlock(const Base::Object& owner) noexcept {
        return owner.RuntimeType() == Controls::TextBlock::StaticTypeId() ||
            DependencyObjectAccess::PropertyRegistry((static_cast<const ::Aero::DependencyObject&>(owner)))
                .Types().IsDerivedFrom(
                    owner.RuntimeType(), Controls::TextBlock::StaticTypeId());
    }

    static bool IsSpan(const Base::Object& owner) noexcept {
        return owner.RuntimeType() == Documents::Span::StaticTypeId() ||
            DependencyObjectAccess::PropertyRegistry((static_cast<const ::Aero::DependencyObject&>(owner)))
                .Types().IsDerivedFrom(
                    owner.RuntimeType(), Documents::Span::StaticTypeId());
    }

    static std::uint32_t GetCount(const Base::Object& owner) noexcept {
        if (IsTextBlock(owner)) {
            return static_cast<const Controls::TextBlock&>(owner)
                .ownedInlines_.Size();
        }
        if (IsSpan(owner)) {
            return static_cast<const Documents::Span&>(owner)
                .inlines_.Size();
        }
        return 0U;
    }

    static Documents::Inline* At(
        Base::Object& owner,
        std::uint32_t index) noexcept {
        if (IsTextBlock(owner)) {
            auto& text = static_cast<Controls::TextBlock&>(owner);
            if (index >= text.ownedInlines_.Size()) return nullptr;
            return static_cast<Documents::Inline*>(
                text.ownedInlines_[index].Get());
        }
        if (IsSpan(owner)) {
            auto& span = static_cast<Documents::Span&>(owner);
            return index < span.inlines_.Size()
                ? span.inlines_[index].Get() : nullptr;
        }
        return nullptr;
    }

    static const Documents::Inline* At(
        const Base::Object& owner,
        std::uint32_t index) noexcept {
        return At(
            const_cast<Base::Object&>(owner), index);
    }

    static Controls::TextBlock* Host(Base::Object& owner) noexcept {
        if (IsTextBlock(owner)) {
            return &static_cast<Controls::TextBlock&>(owner);
        }
        if (IsSpan(owner)) {
            ::Aero::DependencyObject* current =
                static_cast<Documents::Span&>(owner).GetParent();
            while (current != nullptr) {
                if (DependencyObjectAccess::PropertyRegistry(current).Types().IsDerivedFrom(
                        current->RuntimeType(),
                        Controls::TextBlock::StaticTypeId())) {
                    return static_cast<Controls::TextBlock*>(current);
                }
                if (!DependencyObjectAccess::PropertyRegistry(current).Types().IsDerivedFrom(
                        current->RuntimeType(),
                        ContentElement::StaticTypeId())) {
                    break;
                }
                current = static_cast<ContentElement*>(current)->GetParent();
            }
        }
        return nullptr;
    }

    static bool Contains(
        const Documents::Inline& root,
        const Documents::Inline& candidate,
        std::uint32_t depth = 0U) noexcept {
        if (&root == &candidate) return true;
        if (depth >= 1024U) return true;
        const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((root)).Types();
        if (!types.IsDerivedFrom(
                root.RuntimeType(), Documents::Span::StaticTypeId())) {
            return false;
        }
        const auto& span = static_cast<const Documents::Span&>(root);
        for (const Base::Ref<Documents::Inline>& child : span.inlines_) {
            if (child && Contains(*child, candidate, depth + 1U)) return true;
        }
        return false;
    }

    static void PropagateHost(
        Documents::Inline& inlineValue,
        ::Aero::DependencyObject& parent,
        Controls::TextBlock* host) noexcept {
        (inlineValue).Attach(
            &parent,
            host,
            nullptr);
        const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((inlineValue)).Types();
        if (!types.IsDerivedFrom(
                inlineValue.RuntimeType(), Documents::Span::StaticTypeId())) {
            return;
        }
        auto& span = static_cast<Documents::Span&>(inlineValue);
        for (Base::Ref<Documents::Inline>& child : span.inlines_) {
            if (child) PropagateHost(*child, span, host);
        }
    }

    static void ClearHost(Documents::Inline& inlineValue) noexcept {
        const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((inlineValue)).Types();
        if (types.IsDerivedFrom(
                inlineValue.RuntimeType(), Documents::Span::StaticTypeId())) {
            auto& span = static_cast<Documents::Span&>(inlineValue);
            for (Base::Ref<Documents::Inline>& child : span.inlines_) {
                if (child) ClearHost(*child);
            }
        }
        (inlineValue).Detach();
    }

    static Base::Result<void> Add(
        Base::Object& owner,
        Base::Ref<Documents::Inline> value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "InlineCollection value cannot be null");
        }
        if (!IsTextBlock(owner) && !IsSpan(owner)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "InlineCollection owner is not a TextBlock or Span");
        }
        if (value->GetParent() != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Inline already belongs to a logical parent");
        }
        if (IsSpan(owner) &&
            Contains(*value, static_cast<Documents::Span&>(owner))) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "InlineCollection cannot create a document cycle");
        }

        if (IsTextBlock(owner)) {
            static_cast<Controls::TextBlock&>(owner)
                .AddOwnedInline(Base::Ref<Base::Object>(value));
        } else {
            static_cast<Documents::Span&>(owner)
                .AddOwnedInline(value);
        }

        return {};
    }

    static bool Remove(
        Base::Object& owner,
        Documents::Inline& value) noexcept {
        Base::Result<void> access =
            static_cast<::Aero::DependencyObject&>(owner).VerifyAccess();
        if (!access) return false;
        if (value.GetParent() != &owner) return false;

        if (IsTextBlock(owner)) {
            auto& text = static_cast<Controls::TextBlock&>(owner);
            for (std::uint32_t index = 0U;
                 index < text.ownedInlines_.Size(); ++index) {
                if (text.ownedInlines_[index].Get() != &value) continue;
                ClearHost(value);
                for (std::uint32_t move = index + 1U;
                     move < text.ownedInlines_.Size(); ++move) {
                    text.ownedInlines_[move - 1U] =
                        std::move(text.ownedInlines_[move]);
                }
                text.ownedInlines_.PopBack();
                text.pendingInline_ = text.ownedInlines_.Empty()
                    ? Base::Ref<Base::Object>{}
                    : text.ownedInlines_.Back();
                text.InvalidateDocumentText();
                return true;
            }
            return false;
        }

        auto& span = static_cast<Documents::Span&>(owner);
        for (std::uint32_t index = 0U;
             index < span.inlines_.Size(); ++index) {
            if (span.inlines_[index].Get() != &value) continue;
            ClearHost(value);
            for (std::uint32_t move = index + 1U;
                 move < span.inlines_.Size(); ++move) {
                span.inlines_[move - 1U] =
                    std::move(span.inlines_[move]);
            }
            span.inlines_.PopBack();
            span.pendingInline_ = span.inlines_.Empty()
                ? Base::Ref<Documents::Inline>{}
                : span.inlines_.Back();
            Controls::TextBlock* host = Host(owner);
            if (host != nullptr) {
                host->InvalidateDocumentText();
            }
            return true;
        }
        return false;
    }

    static void Clear(Base::Object& owner) noexcept {
        while (GetCount(owner) != 0U) {
            Documents::Inline* value = At(owner, GetCount(owner) - 1U);
            if (value == nullptr) break;
            if (!Remove(owner, *value)) break;
        }
    }

    struct InlineStyleContext {
        Base::Color foreground{};
        bool hasForeground = false;
        bool bold = false;
        bool italic = false;
    };

    static Base::Result<void> AppendInline(
        const Controls::TextBlock& owner,
        const Documents::Inline& value,
        Base::String& output,
        Base::Vector<Controls::TextBlock::RichTextStyleRange>& ranges,
        InlineStyleContext context,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document inline nesting exceeds the supported depth");
        }
        const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((value)).Types();

        if (types.IsDerivedFrom(value.RuntimeType(), Documents::Bold::StaticTypeId()) ||
            value.GetFontWeight() == FontWeight::Bold ||
            value.GetFontWeight() == FontWeight::SemiBold) {
            context.bold = true;
        }
        if (types.IsDerivedFrom(value.RuntimeType(), Documents::Italic::StaticTypeId()) ||
            value.GetFontStyle() != FontStyle::Normal) {
            context.italic = true;
        }
        const bool isHyperlink = types.IsDerivedFrom(value.RuntimeType(), Documents::Hyperlink::StaticTypeId());
        if (value.GetForeground()) {
            const Base::Color sampled = ::Aero::Media::SampleBrush(value.GetForeground());
            if (sampled.alpha > 0.001F) {
                context.foreground = sampled;
                context.hasForeground = true;
            }
        }
        if (!context.hasForeground && isHyperlink) {
            Media::Brush* raw = nullptr;
            ResourceValue res = owner.TryFindResource(Base::StringView("Brush.Hyperlink.Normal"));
            if (!res.IsUnset() && res.Kind() == Meta::ValueKind::Object && res.AsObject()) {
                raw = ::Aero::TryCast<Media::Brush>(res.AsObject().Get());
            }
            if (raw != nullptr) {
                Base::Ref<Media::Brush> brush = Base::Ref<Media::Brush>::FromBorrowed(*raw);
                context.foreground = ::Aero::Media::SampleBrush(brush);
            }
            if (context.foreground.alpha <= 0.001F) {
                context.foreground = Base::Color{0.0F, 0.73725F, 0.97647F, 1.0F};
            }
            context.hasForeground = true;
        }

        if (types.IsDerivedFrom(
                value.RuntimeType(), Documents::Run::StaticTypeId())) {
            const Base::StringView text =
                static_cast<const Documents::Run&>(value).GetText();
            const std::uint32_t startOffset = output.SizeBytes();
            Base::Result<void> appended = output.Append(text);
            if (!appended) return appended.GetStatus();
            if (context.hasForeground || context.bold || context.italic) {
                Controls::TextBlock::RichTextStyleRange range;
                range.start = startOffset;
                range.length = text.SizeBytes();
                range.foreground = context.foreground;
                range.hasForeground = context.hasForeground;
                range.bold = context.bold;
                range.italic = context.italic;
                static_cast<void>(ranges.PushBack(range));
            }
            return {};
        }
        if (types.IsDerivedFrom(
                value.RuntimeType(), Documents::LineBreak::StaticTypeId())) {
            return output.Append(Base::StringView("\n"));
        }
        if (types.IsDerivedFrom(
                value.RuntimeType(), Documents::Span::StaticTypeId())) {
            const auto& span = static_cast<const Documents::Span&>(value);
            for (const Base::Ref<Documents::Inline>& child : span.inlines_) {
                if (!child) continue;
                Base::Result<void> appended =
                    AppendInline(owner, *child, output, ranges, context, depth + 1U);
                if (!appended) return appended.GetStatus();
            }
            return {};
        }
        if (types.IsDerivedFrom(
                value.RuntimeType(),
                Documents::InlineUIContainer::StaticTypeId())) {
            // U+FFFC object replacement; do not flatten the hosted UI to text.
            static constexpr char kObjectReplacement[] = {
                '\xEF', '\xBF', '\xBC'};
            return output.Append(Base::StringView(
                kObjectReplacement, 3U));
        }
        return {};
    }

    static void AppendText(
        const Controls::TextBlock& owner,
        Base::String& output) noexcept {
        if (!owner.flatTextValid_) {
            owner.flatText_.Clear();
            if (owner.GetValue(RichText::TextProperty).Empty()) {
                owner.richTextStyleRanges_.Clear();
            }
            owner.flatText_.Append(owner.GetText());
            InlineStyleContext defaultContext{};
            for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
                if (!item) continue;
                AppendInline(
                    owner,
                    *static_cast<const Documents::Inline*>(item.Get()),
                    owner.flatText_,
                    owner.richTextStyleRanges_,
                    defaultContext);
            }
            owner.flatTextValid_ = true;
        }
        output.Append(owner.flatText_.View());
    }

    static std::uint32_t GetLength(
        const Controls::TextBlock& owner) noexcept {
        Base::String flattened;
        AppendText(owner, flattened);
        return flattened.SizeBytes();
    }

    static bool IsUtf8Boundary(
        const Controls::TextBlock& owner,
        std::uint32_t offset) noexcept {
        Base::String flattened;
        AppendText(owner, flattened);
        if (offset > flattened.SizeBytes()) {
            return false;
        }
        if (offset == 0U || offset == flattened.SizeBytes()) return true;
        const unsigned char byte = static_cast<unsigned char>(
            flattened.View().Data()[offset]);
        return (byte & 0xC0U) != 0x80U;
    }

    static Documents::TextPointer PositionFromPoint(
        Controls::TextBlock& owner,
        Aero::Point point,
        bool snap) noexcept {
        if (!owner.GetIsMeasureValid()) {
            return {};
        }
        const TextHitRegion* best = nullptr;
        double bestDistance = 0.0;
        for (const TextHitRegion& hit : owner.textHitRegions_) {
            const Aero::Rect rect{
                static_cast<double>(hit.x),
                static_cast<double>(hit.y),
                static_cast<double>(std::max(hit.width, 1.0F)),
                static_cast<double>(std::max(hit.height, 1.0F))};
            const bool inside = point.x >= rect.x &&
                point.x <= rect.x + rect.width &&
                point.y >= rect.y &&
                point.y <= rect.y + rect.height;
            if (!inside && !snap) continue;
            const double clampedX = std::max(
                rect.x, std::min(point.x, rect.x + rect.width));
            const double clampedY = std::max(
                rect.y, std::min(point.y, rect.y + rect.height));
            const double dx = point.x - clampedX;
            const double dy = point.y - clampedY;
            const double distance = dx * dx + dy * dy;
            if (best != nullptr && distance >= bestDistance) continue;
            best = &hit;
            bestDistance = distance;
        }
        if (best == nullptr) {
            return {};
        }
        const bool trailing = point.x >
            static_cast<double>(best->x + best->width * 0.5F);
        const std::uint32_t offset = best->textOffset +
            (trailing ? best->textLength : 0U);
        return Documents::TextPointer(
            owner,
            offset,
            Documents::LogicalDirection::Forward);
    }

    static Aero::Rect CharacterRect(
        const Documents::TextPointer& position) noexcept {
        if (!position.container_ || !position.container_->GetIsMeasureValid()) {
            return {};
        }
        const TextHitRegion* selected = nullptr;
        for (const TextHitRegion& hit :
             position.container_->textHitRegions_) {
            if (position.offset_ >= hit.textOffset &&
                position.offset_ <= hit.textOffset + hit.textLength) {
                selected = &hit;
                break;
            }
        }
        if (selected == nullptr &&
            !position.container_->textHitRegions_.Empty()) {
            selected = &position.container_->textHitRegions_.Back();
        }
        if (selected == nullptr) {
            return {};
        }
        return Aero::Rect{
            selected->x,
            selected->y,
            std::max(static_cast<double>(selected->width), 1.0),
            std::max(static_cast<double>(selected->height), 1.0)};
    }
};

} // namespace Aero::Controls

namespace Aero::Controls {

} // namespace Aero::Controls

namespace Aero::Controls {

Documents::InlineCollection TextBlock::GetInlines() noexcept {
    return Documents::InlineCollection(*this);
}

Documents::InlineCollectionView TextBlock::GetInlines() const noexcept {
    return Documents::InlineCollectionView(*this);
}


Documents::TextPointer TextBlock::GetContentStart() noexcept {
    return Documents::TextPointer(
        *this, 0U, Documents::LogicalDirection::Forward);
}

Documents::TextPointer TextBlock::GetContentEnd() noexcept {
    Base::Result<std::uint32_t> length =
        Aero::Controls::TextBlockDocumentHelper::GetLength(*this);
    return Documents::TextPointer(
        *this,
        length ? length.Value() : 0U,
        Documents::LogicalDirection::Backward);
}

} // namespace Aero::Controls

namespace Aero::Documents {

std::uint32_t InlineCollectionView::GetCount() const noexcept {
    return owner_ != nullptr
        ? Aero::Controls::TextBlockDocumentHelper::GetCount(*owner_)
        : 0U;
}

const Inline* InlineCollectionView::GetItem(
    std::uint32_t index) const noexcept {
    return owner_ != nullptr
        ? Aero::Controls::TextBlockDocumentHelper::At(*owner_, index)
        : nullptr;
}

std::uint32_t InlineCollection::GetCount() const noexcept {
    return owner_ != nullptr
        ? Aero::Controls::TextBlockDocumentHelper::GetCount(*owner_)
        : 0U;
}

Inline* InlineCollection::GetItem(std::uint32_t index) const noexcept {
    return owner_ != nullptr
        ? Aero::Controls::TextBlockDocumentHelper::At(*owner_, index)
        : nullptr;
}

InlineCollectionView InlineCollection::GetView() const noexcept {
    return owner_ != nullptr
        ? InlineCollectionView(*owner_)
        : InlineCollectionView{};
}

void InlineCollection::Add(
    Base::Ref<Inline> value) noexcept {
    if (owner_ == nullptr) { AERO_ASSERT(false); return; }
    Base::Result<void> added = Aero::Controls::TextBlockDocumentHelper::Add(
        *owner_, std::move(value));
    if (!added) { AERO_ASSERT(false); return; }
}

bool InlineCollection::Remove(
    Inline& value) noexcept {
    if (owner_ == nullptr) {
        return false;
    }
    return Aero::Controls::TextBlockDocumentHelper::Remove(*owner_, value);
}

void InlineCollection::Clear() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    Aero::Controls::TextBlockDocumentHelper::Clear(*owner_);
}

Span::~Span() {
    ClearOwnedInlines();
}

Meta::Value Span::GetMetadataInlines() const noexcept {
    if (pendingInline_) {
        return Meta::Value::FromObject(
            pendingInline_->RuntimeType(),
            Base::Ref<Base::Object>(pendingInline_));
    }
    return Meta::Value::NullObject(Meta::TypeOf<Base::Object>());
}

void Span::SetInlineValue(Meta::Value value) noexcept {
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() && value.AsObject()) {
        Base::Ref<Base::Object> object = value.AsObject();
        if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                object->RuntimeType(), Inline::StaticTypeId())) {
            return;
        }
        AddOwnedInline(Base::Ref<Inline>::FromBorrowed(
            *static_cast<Inline*>(object.Get())));
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        return;
    }
    Base::Ref<Run> created = Base::MakeRef<Run>();
    if (!created) return;
    created->SetText(value.AsString());
    AddOwnedInline(Base::Ref<Inline>(created));
}

void Span::AddOwnedInline(Base::Ref<Inline> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    for (const Base::Ref<Inline>& current : inlines_) {
        if (current.Get() == value.Get()) { AERO_ASSERT(false); return; }
    }
    inlines_.PushBack(value);
}

void Span::ClearOwnedInlines() noexcept {
    for (Base::Ref<Inline>& value : inlines_) {
        if (value) (*value).Detach();
    }
    inlines_.Clear();
    pendingInline_.Reset();
    Controls::TextBlock* host = GetContentHost() != nullptr
        ? static_cast<Controls::TextBlock*>(GetContentHost())
        : nullptr;
    if (host != nullptr) host->InvalidateDocumentText();
}

void CopyText(
    const Controls::TextBlock& container,
    Base::String& output) noexcept {
    output.Clear();
    Aero::Controls::TextBlockDocumentHelper::AppendText(
        container, output);
}

std::int32_t TextPointer::CompareTo(
    const TextPointer& other) const noexcept {
    if (!GetIsValid() || !other.GetIsValid() ||
        container_ != other.container_) {
        return 0;
    }
    return offset_ < other.offset_ ? -1 :
        (offset_ > other.offset_ ? 1 : 0);
}

TextPointer TextPointer::GetPositionAtOffset(
    std::int32_t delta,
    LogicalDirection direction) const noexcept {
    if (!container_) {
        return {};
    }
    const std::uint32_t length =
        Aero::Controls::TextBlockDocumentHelper::GetLength(*container_);
    const std::int64_t destination =
        static_cast<std::int64_t>(offset_) + delta;
    if (destination < 0 ||
        destination > static_cast<std::int64_t>(length)) {
        return {};
    }
    const std::uint32_t resolved =
        static_cast<std::uint32_t>(destination);
    if (!Aero::Controls::TextBlockDocumentHelper::IsUtf8Boundary(
            *container_, resolved)) {
        return {};
    }
    return TextPointer(*container_, resolved, direction);
}

TextRange TextRange::Create(
    TextPointer start, TextPointer end) noexcept {
    if (!start.GetIsValid() || !end.GetIsValid() ||
        start.container_ != end.container_) {
        return {};
    }
    const std::int32_t order = start.CompareTo(end);
    if (order > 0) {
        return {};
    }
    return TextRange(start, end);
}

Base::String TextRange::GetText() const noexcept {
    Base::String text;
    CopyText(text);
    return text;
}

void TextRange::CopyText(
    Base::String& output) const noexcept {
    output.Clear();
    if (!GetIsValid() || start_.container_ != end_.container_) {
        return;
    }
    Base::String flattened;
    Documents::CopyText(
        *start_.container_, flattened);
    if (end_.offset_ > flattened.SizeBytes()) {
        return;
    }
    output.Assign(flattened.View().Substr(
        start_.offset_, GetLength()));
}

TextPointer GetPositionFromPoint(
    Controls::TextBlock& container,
    Aero::Point point,
    bool snapToText) noexcept {
    return Aero::Controls::TextBlockDocumentHelper::PositionFromPoint(
        container, point, snapToText);
}

Aero::Rect GetCharacterRect(
    const TextPointer& position) noexcept {
    return Aero::Controls::TextBlockDocumentHelper::CharacterRect(position);
}

Base::StringView Hyperlink::GetNavigateUri() const noexcept {
    return GetValue(NavigateUriProperty);
}

Input::ICommand* Hyperlink::GetCommand() const noexcept {
    return GetValue(CommandProperty).Get();
}

Value Hyperlink::GetCommandParameter() const noexcept {
    return GetValue(CommandParameterProperty);
}

Aero::UIElement* Hyperlink::GetCommandTarget() const noexcept {
    return GetValue(CommandTargetProperty).Get();
}

void Hyperlink::SetNavigateUri(
    Base::StringView value) noexcept {
    SetValue(NavigateUriProperty, value);
}

void Hyperlink::SetCommand(
    Base::Ref<Input::ICommand> command) noexcept {
    SetValue(CommandProperty, std::move(command));
}

void Hyperlink::SetCommandParameter(
    Value parameter) noexcept {
    SetValue(CommandParameterProperty, std::move(parameter));
}

void Hyperlink::SetCommandTarget(
    Base::Ref<Aero::UIElement> target) noexcept {
    SetValue(CommandTargetProperty, std::move(target));
}

NavigationService::NavigationService(
    NavigationHandler handler) noexcept
    : handler_(std::move(handler)),
      requestHandler_(this, &NavigationService::OnRequestNavigate) {}

NavigationService::~NavigationService() noexcept {
    static_cast<void>(Detach());
}

bool NavigationService::Attach(
    Aero::UIElement& root) noexcept {
    if (root_ != nullptr || !handler_) {
        return false;
    }
    root.AddHandler(Hyperlink::RequestNavigateEvent, requestHandler_, true);
    root_ = &root;
    return true;
}

bool NavigationService::Detach() noexcept {
    if (root_ == nullptr) return false;
    const bool removed = root_->RemoveHandler(
        Hyperlink::RequestNavigateEvent,
        requestHandler_);
    root_ = nullptr;
    return removed;
}

void NavigationService::OnRequestNavigate(
    Base::Object*,
    RequestNavigateEventArgs& args) noexcept {
    if (args.GetHandled() || !handler_ ||
        args.GetHyperlink() == nullptr || args.GetUri().Empty()) {
        return;
    }
    if (handler_(args.GetUri(), *args.GetHyperlink())) {
        args.SetHandled(true);
    }
}

namespace {

void AddSpanInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    auto& span = static_cast<Span&>(owner);
    if (!DependencyObjectAccess::PropertyRegistry((span)).Types().IsDerivedFrom(
            child->RuntimeType(),
            Inline::StaticTypeId())) {
        return;
    }
    span.AddOwnedInline(
        Base::Ref<Inline>::FromBorrowed(
            *static_cast<Inline*>(child.Get())));
}

void ClearSpanInlines(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Span&>(owner).ClearOwnedInlines();
}

} // namespace

void PopulateDocumentsMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    using namespace Aero::Media;
    const Base::Ref<Brush> black = MakeSolidColorBrush({0.0F, 0.0F, 0.0F, 1.0F});

    Register<TextElement>(context, TypeFlags::Abstract)
        .AddOwner(TextElement::FontFamilyProperty, Aero::FrameworkElement::FontFamilyProperty, Base::Ref<FontFamily>{}, Inherits | AffectsMeasure)
        .Property(TextElement::FontWeightProperty, FontWeight::Normal, Inherits | AffectsMeasure)
        .AddOwner(TextElement::ForegroundProperty, Aero::FrameworkElement::ForegroundProperty, black, Inherits)
        .Property(TextElement::FontSizeProperty, 15.0, Inherits, &Controls::ValidatePositiveFiniteDouble)
        .Property(TextElement::FontStyleProperty, FontStyle::Normal, Inherits)
        .Property(TextElement::TextDecorationsProperty, Controls::TextDecorations::None, Inherits);

    Register<Inline>(context, TypeFlags::Abstract);

    Register<Run>(context)
        .Property(Run::TextProperty, FrameworkPropertyMetadata(Base::String{}).Structural())
        .Content(Run::TextProperty.Id())
        .Factory();

    Register<Span>(context)
        .Property<Value, &Span::GetMetadataInlines, &Span::SetInlineValue>("Inlines", PropertyFlags::AnyValue | PropertyFlags::Collection | PropertyFlags::Structural)
        .ContentAccessor(MakeMemberId(Span::StaticTypeId(), MemberKind::Property, "Inlines"), ContentKind::Collection, &AddSpanInline, &ClearSpanInlines, ContentFlags::None)
        .Factory();

    Register<Bold>(context)
        .Override(TextElement::FontWeightProperty, FontWeight::Bold, AffectsMeasure)
        .Factory();

    Register<Italic>(context)
        .Override(TextElement::FontStyleProperty, FontStyle::Italic, AffectsMeasure)
        .Factory();

    Register<Underline>(context)
        .Override(TextElement::TextDecorationsProperty, Controls::TextDecorations::Underline, AffectsRender)
        .Factory();

    Register<LineBreak>(context)
        .Factory();

    Register<RequestNavigateEventArgs>(context);

    Register<Hyperlink>(context)
        .Event(Hyperlink::ClickEvent)
        .Event(Hyperlink::RequestNavigateEvent)
        .Property(Hyperlink::NavigateUriProperty, Base::String{})
        .Property(Hyperlink::CommandProperty, Base::Ref<Input::ICommand>{})
        .Property(Hyperlink::CommandParameterProperty, Value::NullObject(TypeOf<Base::Object>()))
        .Property(Hyperlink::CommandTargetProperty, Base::Ref<UIElement>{})
        .Override(TextElement::TextDecorationsProperty, Controls::TextDecorations::Underline, AffectsRender)
        .Factory();

    Register<InlineUIContainer>(context)
        .Property(InlineUIContainer::ChildProperty, Base::Ref<UIElement>{})
        .Factory();

    Register<Adorner>(context)
        .Factory();

    Register<AdornerLayer>(context)
        .Factory();

    Register<AdornerDecorator>(context)
        .Factory();
}

} // namespace Aero::Documents
