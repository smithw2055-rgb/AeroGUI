#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Documents.hpp>


#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Controls {

struct TextBlockDocumentHelper {
public:
    static bool IsTextBlock(const Base::Object& owner) noexcept {
        return owner.RuntimeType() == Controls::TextBlock::StaticTypeId() ||
            static_cast<const ::Aero::DependencyObject&>(owner)
                .PropertyRegistry().Types().IsDerivedFrom(
                    owner.RuntimeType(), Controls::TextBlock::StaticTypeId());
    }

    static bool IsSpan(const Base::Object& owner) noexcept {
        return owner.RuntimeType() == Documents::Span::StaticTypeId() ||
            static_cast<const ::Aero::DependencyObject&>(owner)
                .PropertyRegistry().Types().IsDerivedFrom(
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
                if (current->PropertyRegistry().Types().IsDerivedFrom(
                        current->RuntimeType(),
                        Controls::TextBlock::StaticTypeId())) {
                    return static_cast<Controls::TextBlock*>(current);
                }
                if (!current->PropertyRegistry().Types().IsDerivedFrom(
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
        const Meta::TypeRegistry& types = root.PropertyRegistry().Types();
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
        AeroGuiInternal::Attach(
            inlineValue,
            &parent,
            host,
            nullptr);
        const Meta::TypeRegistry& types = inlineValue.PropertyRegistry().Types();
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
        const Meta::TypeRegistry& types = inlineValue.PropertyRegistry().Types();
        if (types.IsDerivedFrom(
                inlineValue.RuntimeType(), Documents::Span::StaticTypeId())) {
            auto& span = static_cast<Documents::Span&>(inlineValue);
            for (Base::Ref<Documents::Inline>& child : span.inlines_) {
                if (child) ClearHost(*child);
            }
        }
        AeroGuiInternal::Detach(inlineValue);
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

        Base::Result<void> added;
        if (IsTextBlock(owner)) {
            added = static_cast<Controls::TextBlock&>(owner)
                .AddOwnedInline(Base::Ref<Base::Object>(value));
        } else {
            added = static_cast<Documents::Span&>(owner)
                .AddOwnedInline(value);
        }
        if (!added) return added.GetStatus();

        return {};
    }

    static Base::Result<bool> Remove(
        Base::Object& owner,
        Documents::Inline& value) noexcept {
        Base::Result<void> access =
            static_cast<::Aero::DependencyObject&>(owner).VerifyAccess();
        if (!access) return access.GetStatus();
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
                Base::Result<void> invalidated = text.InvalidateMeasure();
                if (!invalidated) return invalidated.GetStatus();
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
                Base::Result<void> invalidated = host->InvalidateMeasure();
                if (!invalidated) return invalidated.GetStatus();
            }
            return true;
        }
        return false;
    }

    static Base::Result<void> Clear(Base::Object& owner) noexcept {
        while (GetCount(owner) != 0U) {
            Documents::Inline* value = At(owner, GetCount(owner) - 1U);
            if (value == nullptr) break;
            Base::Result<bool> removed = Remove(owner, *value);
            if (!removed) return removed.GetStatus();
            if (!removed.Value()) break;
        }
        return {};
    }

    static Base::Result<void> AppendInline(
        const Documents::Inline& value,
        Base::String& output,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document inline nesting exceeds the supported depth");
        }
        const Meta::TypeRegistry& types = value.PropertyRegistry().Types();
        if (types.IsDerivedFrom(
                value.RuntimeType(), Documents::Run::StaticTypeId())) {
            return output.Append(
                static_cast<const Documents::Run&>(value).GetText());
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
                    AppendInline(*child, output, depth + 1U);
                if (!appended) return appended.GetStatus();
            }
        }
        return {};
    }

    static Base::Result<void> AppendText(
        const Controls::TextBlock& owner,
        Base::String& output) noexcept {
        Base::Result<void> appended = output.Append(owner.GetText());
        if (!appended) return appended.GetStatus();
        for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
            if (!item) continue;
            appended = AppendInline(
                *static_cast<const Documents::Inline*>(item.Get()),
                output);
            if (!appended) return appended.GetStatus();
        }
        return {};
    }

    static Base::Result<std::uint32_t> GetLength(
        const Controls::TextBlock& owner) noexcept {
        Base::String flattened;
        Base::Result<void> copied = AppendText(owner, flattened);
        if (!copied) return copied.GetStatus();
        return flattened.SizeBytes();
    }

    static Base::Result<bool> IsUtf8Boundary(
        const Controls::TextBlock& owner,
        std::uint32_t offset) noexcept {
        Base::String flattened;
        Base::Result<void> copied = AppendText(owner, flattened);
        if (!copied) return copied.GetStatus();
        if (offset > flattened.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "TextPointer offset exceeds the document text");
        }
        if (offset == 0U || offset == flattened.SizeBytes()) return true;
        const unsigned char byte = static_cast<unsigned char>(
            flattened.View().Data()[offset]);
        return (byte & 0xC0U) != 0x80U;
    }

    static Base::Result<Documents::TextPointer> PositionFromPoint(
        Controls::TextBlock& owner,
        Aero::Point point,
        bool snap) noexcept {
        if (!owner.GetIsMeasureValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Document text positions require a valid measure pass");
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
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Point does not resolve to document text");
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

    static Base::Result<Aero::Rect> CharacterRect(
        const Documents::TextPointer& position) noexcept {
        if (!position.container_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "TextPointer is not bound to a container");
        }
        if (!position.container_->GetIsMeasureValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Document character rectangles require a valid measure pass");
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
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TextPointer has no arranged character rectangle");
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

Base::Result<void> InlineCollection::Add(
    Base::Ref<Inline> value) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "InlineCollection is not bound to an owner");
    }
    return Aero::Controls::TextBlockDocumentHelper::Add(
        *owner_, std::move(value));
}

Base::Result<bool> InlineCollection::Remove(
    Inline& value) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "InlineCollection is not bound to an owner");
    }
    return Aero::Controls::TextBlockDocumentHelper::Remove(*owner_, value);
}

void InlineCollection::Clear() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    (void)Aero::Controls::TextBlockDocumentHelper::Clear(*owner_);
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
        if (!PropertyRegistry().Types().IsDerivedFrom(
                object->RuntimeType(), Inline::StaticTypeId())) {
            return;
        }
        pendingInline_ = Base::Ref<Inline>::FromBorrowed(
            *static_cast<Inline*>(object.Get()));
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        return;
    }
    Base::Result<Base::Ref<Run>> created = Base::MakeRef<Run>();
    if (!created) return;
    created.Value()->SetText(value.AsString());
    pendingInline_ = Base::Ref<Inline>(created.Value());
    return;
}

Base::Result<void> Span::AddOwnedInline(Base::Ref<Inline> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Span inline cannot be null");
    }
    for (const Base::Ref<Inline>& current : inlines_) {
        if (current.Get() == value.Get()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Span already owns the inline");
        }
    }
    Base::Result<void> appended = inlines_.PushBack(value);
    if (!appended) return appended.GetStatus();
    AeroGuiInternal::Attach(
        *value, this, GetContentHost(), nullptr);
    pendingInline_ = std::move(value);
    Controls::TextBlock* host = Aero::Controls::TextBlockDocumentHelper::Host(*this);
    return host != nullptr ? host->InvalidateMeasure() : Base::Result<void>{};
}

void Span::ClearOwnedInlines() noexcept {
    for (Base::Ref<Inline>& value : inlines_) {
        if (value) AeroGuiInternal::Detach(*value);
    }
    inlines_.Clear();
    pendingInline_.Reset();
    Controls::TextBlock* host = GetContentHost() != nullptr
        ? static_cast<Controls::TextBlock*>(GetContentHost())
        : nullptr;
    if (host != nullptr) (void)host->InvalidateMeasure();
}

Base::Result<void> CopyText(
    const Controls::TextBlock& container,
    Base::String& output) noexcept {
    output.Clear();
    return Aero::Controls::TextBlockDocumentHelper::AppendText(
        container, output);
}

Base::Result<std::int32_t> TextPointer::CompareTo(
    const TextPointer& other) const noexcept {
    if (!GetIsValid() || !other.GetIsValid() ||
        container_ != other.container_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextPointer comparison requires one document container");
    }
    return offset_ < other.offset_ ? -1 :
        (offset_ > other.offset_ ? 1 : 0);
}

Base::Result<TextPointer> TextPointer::GetPositionAtOffset(
    std::int32_t delta,
    LogicalDirection direction) const noexcept {
    if (!container_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextPointer is not bound to a container");
    }
    Base::Result<std::uint32_t> length =
        Aero::Controls::TextBlockDocumentHelper::GetLength(*container_);
    if (!length) return length.GetStatus();
    const std::int64_t destination =
        static_cast<std::int64_t>(offset_) + delta;
    if (destination < 0 ||
        destination > static_cast<std::int64_t>(length.Value())) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "TextPointer offset leaves the document range");
    }
    const std::uint32_t resolved =
        static_cast<std::uint32_t>(destination);
    Base::Result<bool> boundary =
        Aero::Controls::TextBlockDocumentHelper::IsUtf8Boundary(
            *container_, resolved);
    if (!boundary) return boundary.GetStatus();
    if (!boundary.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextPointer offset splits a UTF-8 sequence");
    }
    return TextPointer(*container_, resolved, direction);
}

Base::Result<TextRange> TextRange::Create(
    TextPointer start, TextPointer end) noexcept {
    Base::Result<std::int32_t> order = start.CompareTo(end);
    if (!order) return order.GetStatus();
    if (order.Value() > 0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextRange start must not follow its end");
    }
    return TextRange(start, end);
}

Base::Result<Base::String> TextRange::GetText() const noexcept {
    Base::String text;
    Base::Result<void> copied = CopyText(text);
    return copied ? Base::Result<Base::String>(std::move(text)) : Base::Result<Base::String>(copied.GetStatus());
}

Base::Result<void> TextRange::CopyText(
    Base::String& output) const noexcept {
    if (!GetIsValid() || start_.container_ != end_.container_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextRange is not valid");
    }
    Base::String flattened;
    Base::Result<void> copied = Documents::CopyText(
        *start_.container_, flattened);
    if (!copied) return copied.GetStatus();
    if (end_.offset_ > flattened.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "TextRange exceeds the document text");
    }
    output.Clear();
    return output.Assign(flattened.View().Substr(
        start_.offset_, GetLength()));
}

Base::Result<TextPointer> GetPositionFromPoint(
    Controls::TextBlock& container,
    Aero::Point point,
    bool snapToText) noexcept {
    return Aero::Controls::TextBlockDocumentHelper::PositionFromPoint(
        container, point, snapToText);
}

Base::Result<Aero::Rect> GetCharacterRect(
    const TextPointer& position) noexcept {
    return Aero::Controls::TextBlockDocumentHelper::CharacterRect(position);
}

Base::StringView Hyperlink::GetNavigateUri() const noexcept {
    return GetValueOr(NavigateUriProperty, Base::StringView{});
}

Input::ICommand* Hyperlink::GetCommand() const noexcept {
    return GetValueOr(
        CommandProperty,
        Base::Ref<Input::ICommand>{}).Get();
}

Value Hyperlink::GetCommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Value::NullObject(Meta::TypeOf<Base::Object>()));
}

Aero::UIElement* Hyperlink::GetCommandTarget() const noexcept {
    return GetValueOr(
        CommandTargetProperty,
        Base::Ref<Aero::UIElement>{}).Get();
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

Base::Result<void> NavigationService::Attach(
    Aero::UIElement& root) noexcept {
    if (root_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "NavigationService is already attached");
    }
    if (!handler_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "NavigationService requires a navigation handler");
    }
    root.AddHandlerChecked(Hyperlink::RequestNavigateEvent, requestHandler_, true);
    root_ = &root;
    return {};
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

} // namespace Aero::Documents
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/media/BrushRendering.hpp"
#include "gui/media/MediaState.hpp"
#include <Aero/Documents.hpp>
#include "RichText.hpp"

#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>


namespace Aero::Controls {
using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;

namespace {

bool IsValidTextSize(Size value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}
::Aero::Controls::TextBlockLayout* TextLayoutFor(
    const ::Aero::Media::Visual& visual) noexcept {
    return AeroGuiInternal::TypedTextLayoutRuntime<::Aero::Controls::TextBlockLayout>(visual);
}

} // namespace

TextBlock::TextBlock() noexcept
    : TextBlock(StaticTypeId()) {}

TextBlock::TextBlock(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType),
      textHitRegions_(),
      ownedInlines_(),
      richTextStyleRanges_(),
      pendingInline_() {}

TextBlock::~TextBlock() {
    ClearOwnedInlines();
    ReleaseServiceGlyphRun();
}
Base::StringView TextBlock::GetText() const noexcept {
    return GetValueOr(TextProperty, Base::StringView());
}
Base::Ref<Brush> TextBlock::GetForeground() const noexcept {
    return GetValueOr(
        ForegroundProperty, Base::Ref<Brush>{});
}
Base::Ref<Brush> TextBlock::GetBackground() const noexcept {
    return GetValueOr(
        BackgroundProperty, Base::Ref<Brush>{});
}
double TextBlock::GetFontSize() const noexcept {
    return GetValueOr(FontSizeProperty, 16.0);
}
Base::Ref<Media::FontFamily> TextBlock::GetFontFamily() const noexcept {
    return FrameworkElement::GetFontFamily();
}
FontWeight TextBlock::GetFontWeight() const noexcept {
    if (RuntimeType() == Documents::Bold::StaticTypeId()) {
        return FontWeight::Bold;
    }
    return GetValueOr(
        FontWeightProperty,
        FontWeight::Normal);
}
FontStyle TextBlock::GetFontStyle() const noexcept {
    if (RuntimeType() == Documents::Italic::StaticTypeId()) {
        return FontStyle::Italic;
    }
    return GetValueOr(
        FontStyleProperty,
        FontStyle::Normal);
}
TextDecorations TextBlock::GetTextDecorations() const noexcept {
    if (RuntimeType() == Documents::Underline::StaticTypeId()) {
        return TextDecorations::Underline;
    }
    return GetValueOr(
        TextDecorationsProperty,
        TextDecorations::None);
}
TextWrapping TextBlock::GetTextWrapping() const noexcept {
    return GetValueOr(
        TextWrappingProperty,
        TextWrapping::NoWrap);
}
TextTrimming TextBlock::GetTextTrimming() const noexcept {
    return GetValueOr(
        TextTrimmingProperty,
        TextTrimming::None);
}
TextAlignment TextBlock::GetTextAlignment() const noexcept {
    return GetValueOr(
        TextAlignmentProperty,
        TextAlignment::Left);
}
double TextBlock::GetLineHeight() const noexcept {
    return GetValueOr(LineHeightProperty, 0.0);
}
void TextBlock::SetText(Base::StringView value) noexcept {
    SetValue(TextProperty, value);
    textHitRegions_.Clear();
}
void TextBlock::SetForeground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        ForegroundProperty, std::move(value));
}
void TextBlock::SetBackground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BackgroundProperty, std::move(value));
}
void TextBlock::SetFontSize(double value) noexcept {
    SetValue(FontSizeProperty, value);
}
void TextBlock::SetFontFamily(
    Base::Ref<Media::FontFamily> value) noexcept {
    FrameworkElement::SetFontFamily(std::move(value));
}
Base::Result<void> TextBlock::SetFontFamily(
    Base::StringView value) noexcept {
    return FrameworkElement::SetFontFamily(value);
}
void TextBlock::SetFontWeight(
    FontWeight value) noexcept {
    SetValue(FontWeightProperty, value);
}
void TextBlock::SetFontStyle(
    FontStyle value) noexcept {
    SetValue(FontStyleProperty, value);
}
void TextBlock::SetTextDecorations(
    TextDecorations value) noexcept {
    SetValue(TextDecorationsProperty, value);
}
void TextBlock::SetTextWrapping(
    TextWrapping value) noexcept {
    SetValue(TextWrappingProperty, value);
}
void TextBlock::SetTextTrimming(
    TextTrimming value) noexcept {
    SetValue(TextTrimmingProperty, value);
}
void TextBlock::SetTextAlignment(
    TextAlignment value) noexcept {
    SetValue(TextAlignmentProperty, value);
}
void TextBlock::SetLineHeight(double value) noexcept {
    SetValue(LineHeightProperty, value);
}
void TextBlock::SetRichTextStyleRanges(
    Base::Span<const RichTextStyleRange> ranges) noexcept {
    bool changed = richTextStyleRanges_.Size() != ranges.Size();
    if (!changed) {
        for (std::uint32_t index = 0U; index < ranges.Size(); ++index) {
            const RichTextStyleRange& left = richTextStyleRanges_[index];
            const RichTextStyleRange& right = ranges[index];
            if (left.start != right.start || left.length != right.length ||
                left.hasForeground != right.hasForeground ||
                left.bold != right.bold || left.italic != right.italic ||
                left.foreground.red != right.foreground.red ||
                left.foreground.green != right.foreground.green ||
                left.foreground.blue != right.foreground.blue ||
                left.foreground.alpha != right.foreground.alpha) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) return;
    Base::Vector<RichTextStyleRange> next;
    if (!next.Append(ranges)) return;
    richTextStyleRanges_ = std::move(next);
    (void)InvalidateVisual();
}
Meta::Value TextBlock::GetMetadataInlines() const noexcept {
    if (pendingInline_) {
        return Meta::Value::FromObject(
            pendingInline_->RuntimeType(),
            pendingInline_);
    }
    return Meta::Value::NullObject(
        Meta::TypeOf<Base::Object>());
}
void TextBlock::SetInlineValue(
    Meta::Value value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Ref<Base::Object> inlineObject = value.AsObject();
        if (!PropertyRegistry().Types().IsDerivedFrom(
                inlineObject->RuntimeType(),
                Documents::Inline::StaticTypeId())) {
            return;
        }
        (void)AddOwnedInline(inlineObject);
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        return;
    }
    Base::Result<Base::Ref<Documents::Run>> created =
        Base::MakeRef<Documents::Run>();
    if (!created) return;
    created.Value()->SetText(value.AsString());
    pendingInline_ = Base::Ref<Base::Object>(
        created.Value());
    return;
}
Base::Result<void> TextBlock::AddOwnedInline(
    const Base::Ref<Base::Object>& inlineObject) noexcept {
    if (!inlineObject) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock inline cannot be null");
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    const TypeRegistry& types = PropertyRegistry().Types();
    const TypeId type = inlineObject->RuntimeType();
    const bool supported = types.IsDerivedFrom(
        type, Documents::Inline::StaticTypeId());
    if (!supported) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock content must derive from Documents::Inline");
    }
    for (const Base::Ref<Base::Object>& owned :
         ownedInlines_) {
        if (owned.Get() == inlineObject.Get()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "TextBlock already owns the inline");
        }
    }
    Base::Result<void> appended =
        ownedInlines_.PushBack(inlineObject);
    if (!appended) return appended.GetStatus();
    auto& inlineValue = *static_cast<Documents::Inline*>(inlineObject.Get());
    AeroGuiInternal::Attach(
        inlineValue, this, this, nullptr);
    pendingInline_ = inlineObject;
    return InvalidateMeasure();
}
void TextBlock::ClearOwnedInlines() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    for (Base::Ref<Base::Object>& item : ownedInlines_) {
        if (item) {
            AeroGuiInternal::Detach(
                *static_cast<Documents::Inline*>(item.Get()));
        }
    }
    ownedInlines_.Clear();
    pendingInline_.Reset();
    (void)InvalidateMeasure();
}
Base::StringView TextBlock::EffectiveFontFamily() const noexcept {
    const Base::Ref<Media::FontFamily> family = GetFontFamily();
    const Base::StringView configured = family
        ? family->GetSource()
        : Base::StringView{};
    const bool defaultFamily =
        configured.Empty() ||
        configured == Base::StringView("Segoe UI");
    if (!defaultFamily) return configured;
    const bool bold =
        GetFontWeight() == FontWeight::Bold ||
        GetFontWeight() == FontWeight::SemiBold;
    const bool italic =
        GetFontStyle() != FontStyle::Normal;
    if (bold && italic) {
        return Base::StringView("Segoe UI Bold Italic");
    }
    if (bold) return Base::StringView("Segoe UI Bold");
    if (italic) return Base::StringView("Segoe UI Italic");
    return configured;
}
void TextBlock::SetGlyphRun(
    RenderGlyphRunId glyphRun, Size size) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!IsValidTextSize(size) ||
        (glyphRun == InvalidRenderGlyphRunId &&
            (size.width != 0.0 || size.height != 0.0))) {
        return;
    }
    if (glyphRuns_.Size() == 1U && glyphRuns_[0] == glyphRun &&
        glyphRunSize_.width == size.width &&
        glyphRunSize_.height == size.height &&
        !serviceOwnsGlyphRun_) return;
    ReleaseServiceGlyphRun();
    glyphRuns_.Clear();
    textHitRegions_.Clear();
    if (glyphRun != InvalidRenderGlyphRunId) {
        Base::Result<void> appended =
            glyphRuns_.PushBack(glyphRun);
        if (!appended) return;
    }
    glyphRunSize_ = size;
    (void)InvalidateMeasure();
    (void)InvalidateVisual();
}
Size TextBlock::MeasureOverride(Size availableSize) noexcept {
    if (!GetValueOr(
            RichText::TextProperty,
            Base::StringView{}).Empty()) {
        // RichText can be populated while a template is still detached from
        // its inherited DataContext. Refresh at the first real measure so
        // inline bindings such as MusicLevel observe the mounted model.
        ApplyRichText(*this);
    }
    Base::String flattened;
    Base::Result<void> copied = Documents::CopyText(*this, flattened);
    if (!copied) return Size{};

    auto* layoutService = TextLayoutFor(*this);
    if (layoutService != nullptr) {
        const Base::StringView text = flattened.View();
        if (text.Empty()) {
            const bool changed =
                !glyphRuns_.Empty() ||
                glyphRunSize_.width != 0.0 ||
                glyphRunSize_.height != 0.0;
            ReleaseServiceGlyphRun();
            glyphRuns_.Clear();
            glyphRunSize_ = {};
            textHitRegions_.Clear();
            if (changed) {
                Base::Result<void> invalidated = InvalidateVisual();
                if (!invalidated) return Size{};
            }
        } else {
            ::Aero::Controls::TextLayoutRequest request;
            request.text = text;
            request.availableSize = availableSize;
            request.dpiScale = GetDpiScale();
            request.pixelSize = static_cast<float>(GetFontSize());
            request.lineHeight = static_cast<float>(GetLineHeight());
            request.fontFamily = EffectiveFontFamily();
            request.fontWeight = GetFontWeight();
            request.fontStyle = GetFontStyle();
            request.wrapping = GetTextWrapping();
            request.trimming = GetTextTrimming();
            request.alignment = GetTextAlignment();
            request.arrangeToAvailableWidth = arrangingText_;
            request.direction = GetFlowDirection() == FlowDirection::RightToLeft
                ? Text::TextDirection::RightToLeft
                : Text::TextDirection::LeftToRight;
            ::Aero::Controls::TextLayoutResult output;
            Base::Result<void> prepared =
                layoutService->ShapeAndPrepare(request, output);
            if (!prepared) return Size{};
            bool validGlyphRuns = true;
            for (RenderGlyphRunId glyphRun : output.glyphRuns) {
                if (glyphRun == InvalidRenderGlyphRunId) {
                    validGlyphRuns = false;
                    break;
                }
            }
            if (!IsValidTextSize(output.desiredSize) || !validGlyphRuns) {
                for (RenderGlyphRunId glyphRun : output.glyphRuns) {
                    if (glyphRun != InvalidRenderGlyphRunId) {
                        layoutService->ReleaseGlyphRun(glyphRun);
                    }
                }
                return Size{};
            }

            bool changed =
                glyphRuns_.Size() != output.glyphRuns.Size() ||
                glyphRunSize_.width != output.desiredSize.width ||
                glyphRunSize_.height != output.desiredSize.height ||
                !serviceOwnsGlyphRun_;
            if (!changed) {
                for (std::uint32_t index = 0U;
                     index < glyphRuns_.Size(); ++index) {
                    if (glyphRuns_[index] != output.glyphRuns[index]) {
                        changed = true;
                        break;
                    }
                }
            }
            ReleaseServiceGlyphRun();
            glyphRuns_ = std::move(output.glyphRuns);
            textHitRegions_ = std::move(output.hitRegions);
            glyphRunSize_ = output.desiredSize;
            serviceOwnsGlyphRun_ = !glyphRuns_.Empty();
            if (changed) {
                Base::Result<void> invalidated = InvalidateVisual();
                if (!invalidated) return Size{};
            }
        }
    }
    return Size{
        std::min(glyphRunSize_.width, availableSize.width),
        std::min(glyphRunSize_.height, availableSize.height)};
}
Size TextBlock::ArrangeOverride(Size finalSize) noexcept {
    const bool needsAlignment = GetTextAlignment() != TextAlignment::Left ||
        GetFlowDirection() == FlowDirection::RightToLeft;
    if (needsAlignment && finalSize.width > 0.0) {
        arrangingText_ = true;
        static_cast<void>(MeasureOverride(finalSize));
        arrangingText_ = false;
    }
    return finalSize;
}
void TextBlock::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    const Size renderSize = GetRenderSize();
    if (renderSize.width <= 0.0 || renderSize.height <= 0.0) return;
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const Color background = ::Aero::Media::SampleBrush(GetBackground());
    if (background.alpha > 0.0F) {
        Base::Result<void> filled =
            builder.FillRect(
                {0.0, 0.0,
                 GetRenderSize().width,
                 GetRenderSize().height},
                background);
        if (!filled) return;
    }
    struct RichTextLineClip {
        float x = 0.0F;
        float y = 0.0F;
        float right = 0.0F;
        float height = 0.0F;
    };

    bool hasStyledRanges = false;
    for (const RichTextStyleRange& range : richTextStyleRanges_) {
        if (range.length > 0U &&
            (range.hasForeground || range.italic ||
             (range.bold && GetFontWeight() != FontWeight::Bold))) {
            hasStyledRanges = true;
            break;
        }
    }

    if (!hasStyledRanges || textHitRegions_.Empty()) {
        for (RenderGlyphRunId glyphRun : glyphRuns_) {
            Base::Result<void> drawn =
                builder.DrawGlyphRun(glyphRun, ::Aero::Media::SampleBrush(GetForeground(), 0.5,
                    Color{0.0F, 0.0F, 0.0F, 1.0F}));
            if (!drawn) return;
        }
    } else {
        struct StyleSpan {
            std::uint32_t start = 0U;
            std::uint32_t length = 0U;
            Color foreground{};
            bool hasForeground = false;
            bool bold = false;
            bool italic = false;
        };

        std::uint32_t maxTextEnd = 0U;
        for (const TextHitRegion& region : textHitRegions_) {
            maxTextEnd = std::max(
                maxTextEnd,
                static_cast<std::uint32_t>(region.textOffset + region.textLength));
        }
        for (const RichTextStyleRange& range : richTextStyleRanges_) {
            maxTextEnd = std::max(maxTextEnd, range.start + range.length);
        }

        Base::Vector<StyleSpan> spans;
        std::uint32_t currentOffset = 0U;
        for (const RichTextStyleRange& range : richTextStyleRanges_) {
            if (range.length == 0U) continue;
            if (range.start > currentOffset) {
                StyleSpan unstyled;
                unstyled.start = currentOffset;
                unstyled.length = range.start - currentOffset;
                if (!spans.PushBack(unstyled)) return;
            }
            const bool isStyled = range.hasForeground || range.italic ||
                (range.bold && GetFontWeight() != FontWeight::Bold);
            if (isStyled) {
                StyleSpan styled;
                styled.start = range.start;
                styled.length = range.length;
                styled.foreground = range.foreground;
                styled.hasForeground = range.hasForeground;
                styled.bold = range.bold && GetFontWeight() != FontWeight::Bold;
                styled.italic = range.italic;
                if (!spans.PushBack(styled)) return;
            } else {
                StyleSpan unstyled;
                unstyled.start = range.start;
                unstyled.length = range.length;
                if (!spans.PushBack(unstyled)) return;
            }
            currentOffset = std::max(currentOffset, range.start + range.length);
        }
        if (currentOffset < maxTextEnd) {
            StyleSpan unstyled;
            unstyled.start = currentOffset;
            unstyled.length = maxTextEnd - currentOffset;
            if (!spans.PushBack(unstyled)) return;
        }

        for (const StyleSpan& span : spans) {
            if (span.length == 0U) continue;
            Base::Vector<RichTextLineClip> clips;
            const std::uint64_t spanEnd =
                static_cast<std::uint64_t>(span.start) + span.length;
            for (const TextHitRegion& region : textHitRegions_) {
                const std::uint64_t regionEnd =
                    static_cast<std::uint64_t>(region.textOffset) +
                    region.textLength;
                if (region.textLength == 0U ||
                    regionEnd <= span.start ||
                    region.textOffset >= spanEnd) {
                    continue;
                }
                RichTextLineClip* line = nullptr;
                for (RichTextLineClip& candidate : clips) {
                    if (std::fabs(candidate.y - region.y) < 0.01F) {
                        line = &candidate;
                        break;
                    }
                }
                if (line == nullptr) {
                    RichTextLineClip added;
                    added.x = region.x;
                    added.y = region.y;
                    added.right = region.x + region.width;
                    added.height = region.height;
                    Base::Result<void> appended = clips.PushBack(added);
                    if (!appended) return;
                } else {
                    line->x = std::min(line->x, region.x);
                    line->right = std::max(
                        line->right, region.x + region.width);
                    line->height = std::max(line->height, region.height);
                }
            }
            if (clips.Empty()) continue;

            const Color tint = span.hasForeground
                ? span.foreground
                : ::Aero::Media::SampleBrush(
                    GetForeground(), 0.5,
                    Color{0.0F, 0.0F, 0.0F, 1.0F});

            for (const RichTextLineClip& line : clips) {
                const double extraRight = span.italic
                    ? std::max(3.0, 0.25 * line.height + 2.0)
                    : (span.bold ? 1.0 : 0.5);
                const double extraLeft = span.italic ? 1.0 : 0.0;
                const Rect clip{
                    std::max(0.0, static_cast<double>(line.x) - extraLeft),
                    std::max(0.0, static_cast<double>(line.y)),
                    std::max(0.0, static_cast<double>(line.right - line.x) + extraLeft + extraRight),
                    std::max(0.0, static_cast<double>(line.height))};
                if (!builder.PushClip(clip)) return;
                bool skewed = false;
                if (span.italic) {
                    Base::Transform2D italic;
                    italic.m21 = -0.16;
                    italic.dx = 0.16 * (line.y + line.height);
                    if (!builder.PushTransform(italic)) return;
                    skewed = true;
                }
                for (RenderGlyphRunId glyphRun : glyphRuns_) {
                    if (!builder.DrawGlyphRun(glyphRun, tint)) return;
                }
                if (span.bold) {
                    Base::Transform2D embolden;
                    embolden.dx = 0.4;
                    if (!builder.PushTransform(embolden)) return;
                    for (RenderGlyphRunId glyphRun : glyphRuns_) {
                        if (!builder.DrawGlyphRun(glyphRun, tint)) return;
                    }
                    if (!builder.PopTransform()) return;
                }
                if (skewed && !builder.PopTransform()) return;
                if (!builder.PopClip()) return;
            }
        }
    }
    if (GetTextDecorations() ==
            TextDecorations::Underline &&
        glyphRunSize_.width > 0.0) {
        const double thickness =
            std::max(1.0, GetFontSize() * 0.06);
        const double y = std::max(
            0.0,
            glyphRunSize_.height - thickness * 1.5);
        static_cast<void>(builder.FillRect(
            {0.0, y, glyphRunSize_.width, thickness},
            ::Aero::Media::SampleBrush(GetForeground(), 0.5,
                Color{0.0F, 0.0F, 0.0F, 1.0F})));
    }
    return;
}
void TextBlock::ReleaseServiceGlyphRun() noexcept {
    auto* layoutService = TextLayoutFor(*this);
    if (serviceOwnsGlyphRun_ && layoutService != nullptr) {
        for (RenderGlyphRunId glyphRun : glyphRuns_) {
            layoutService->ReleaseGlyphRun(glyphRun);
        }
    }
    serviceOwnsGlyphRun_ = false;
}

} // namespace Aero
