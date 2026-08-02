#include "gui/MetadataInternal.hpp"
#include <Aero/Documents.hpp>

#include "gui/ElementInternal.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Internal {

class DocumentPrivate {
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
        ElementPrivate::Attach(
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
        ElementPrivate::Detach(inlineValue);
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

} // namespace Aero::Internal

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
        Aero::Internal::DocumentPrivate::GetLength(*this);
    return Documents::TextPointer(
        *this,
        length ? length.Value() : 0U,
        Documents::LogicalDirection::Backward);
}

} // namespace Aero::Controls

namespace Aero::Documents {

std::uint32_t InlineCollectionView::GetCount() const noexcept {
    return owner_ != nullptr
        ? Aero::Internal::DocumentPrivate::GetCount(*owner_)
        : 0U;
}

const Inline* InlineCollectionView::At(
    std::uint32_t index) const noexcept {
    return owner_ != nullptr
        ? Aero::Internal::DocumentPrivate::At(*owner_, index)
        : nullptr;
}

std::uint32_t InlineCollection::GetCount() const noexcept {
    return owner_ != nullptr
        ? Aero::Internal::DocumentPrivate::GetCount(*owner_)
        : 0U;
}

Inline* InlineCollection::At(std::uint32_t index) const noexcept {
    return owner_ != nullptr
        ? Aero::Internal::DocumentPrivate::At(*owner_, index)
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
    return Aero::Internal::DocumentPrivate::Add(
        *owner_, std::move(value));
}

Base::Result<bool> InlineCollection::Remove(
    Inline& value) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "InlineCollection is not bound to an owner");
    }
    return Aero::Internal::DocumentPrivate::Remove(*owner_, value);
}

void InlineCollection::Clear() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    (void)Aero::Internal::DocumentPrivate::Clear(*owner_);
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
    Aero::Internal::ElementPrivate::Attach(
        *value, this, GetContentHost(), nullptr);
    pendingInline_ = std::move(value);
    Controls::TextBlock* host = Aero::Internal::DocumentPrivate::Host(*this);
    return host != nullptr ? host->InvalidateMeasure() : Base::Result<void>{};
}

void Span::ClearOwnedInlines() noexcept {
    for (Base::Ref<Inline>& value : inlines_) {
        if (value) Aero::Internal::ElementPrivate::Detach(*value);
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
    return Aero::Internal::DocumentPrivate::AppendText(
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
        Aero::Internal::DocumentPrivate::GetLength(*container_);
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
        Aero::Internal::DocumentPrivate::IsUtf8Boundary(
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
    return Aero::Internal::DocumentPrivate::PositionFromPoint(
        container, point, snapToText);
}

Base::Result<Aero::Rect> GetCharacterRect(
    const TextPointer& position) noexcept {
    return Aero::Internal::DocumentPrivate::CharacterRect(position);
}

Base::StringView Hyperlink::GetNavigateUri() const noexcept {
    return GetValueOr(NavigateUriProperty, Base::StringView{});
}

Input::ICommand* Hyperlink::GetCommand() const noexcept {
    return GetValueOr(
        CommandProperty,
        Base::Ref<Input::ICommand>{}).Get();
}

Base::Ref<Base::Object> Hyperlink::GetCommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Base::Ref<Base::Object>{});
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
    Base::Ref<Base::Object> parameter) noexcept {
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
