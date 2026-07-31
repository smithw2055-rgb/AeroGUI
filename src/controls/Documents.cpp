#include <Aero/Documents.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Detail {

class DocumentTextAccess final {
public:
    static std::uint32_t Count(
        const Controls::TextBlock& owner) noexcept {
        return owner.ownedInlines_.Size();
    }

    static Documents::Inline* At(
        Controls::TextBlock& owner,
        std::uint32_t index) noexcept {
        if (index >= owner.ownedInlines_.Size()) return nullptr;
        Base::Object* object = owner.ownedInlines_[index].Get();
        return object != nullptr
            ? static_cast<Documents::Inline*>(object)
            : nullptr;
    }

    static const Documents::Inline* At(
        const Controls::TextBlock& owner,
        std::uint32_t index) noexcept {
        if (index >= owner.ownedInlines_.Size()) return nullptr;
        const Base::Object* object = owner.ownedInlines_[index].Get();
        return object != nullptr
            ? static_cast<const Documents::Inline*>(object)
            : nullptr;
    }

    static bool Contains(
        const Controls::TextBlock& root,
        const Controls::TextBlock& candidate,
        std::uint32_t depth = 0U) noexcept {
        if (&root == &candidate) return true;
        if (depth >= 1024U) return true;
        for (const Base::Ref<Base::Object>& item : root.ownedInlines_) {
            if (!item) continue;
            const auto* inlineValue =
                static_cast<const Documents::Inline*>(item.Get());
            if (Contains(*inlineValue, candidate, depth + 1U)) return true;
        }
        return false;
    }

    static Base::Result<void> Add(
        Controls::TextBlock& owner,
        Base::Ref<Documents::Inline> value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "InlineCollection value cannot be null");
        }
        if (!owner.LayoutChildren().Empty() || owner.GetIsLoaded()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Mounted inline collections require a MountService transaction");
        }
        if (Contains(*value, owner)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "InlineCollection cannot create a document cycle");
        }
        Base::Ref<Base::Object> object(value);
        return owner.AddOwnedInline(object, *value);
    }

    static Base::Result<bool> Remove(
        Controls::TextBlock& owner,
        Documents::Inline& value) noexcept {
        Base::Result<void> access = owner.VerifyAccess();
        if (!access) return access.GetStatus();
        if (!owner.LayoutChildren().Empty() || owner.GetIsLoaded()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Mounted inline collections require a MountService transaction");
        }
        for (std::uint32_t index = 0U;
             index < owner.ownedInlines_.Size(); ++index) {
            if (owner.ownedInlines_[index].Get() != &value) continue;
            for (std::uint32_t move = index + 1U;
                 move < owner.ownedInlines_.Size(); ++move) {
                owner.ownedInlines_[move - 1U] =
                    std::move(owner.ownedInlines_[move]);
            }
            owner.ownedInlines_.PopBack();
            owner.pendingInline_ = owner.ownedInlines_.Empty()
                ? Base::Ref<Base::Object>{}
                : owner.ownedInlines_.Back();
            Base::Result<void> invalidated = owner.InvalidateMeasure();
            if (!invalidated) return invalidated.GetStatus();
            return true;
        }
        return false;
    }

    static Base::Result<void> Clear(
        Controls::TextBlock& owner) noexcept {
        if (owner.GetIsLoaded()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Mounted inline collections require a MountService transaction");
        }
        return owner.ClearOwnedInlines();
    }

    static Base::Result<void> AppendText(
        const Controls::TextBlock& owner,
        Base::String& output,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document inline nesting exceeds the supported depth");
        }
        Base::Result<void> appended = output.TryAppend(owner.Text());
        if (!appended) return appended.GetStatus();
        for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
            if (!item) continue;
            if (item->RuntimeType() == Documents::LineBreak::StaticTypeId()) {
                appended = output.TryAppend(Base::StringView("\n"));
            } else {
                appended = AppendText(
                    *static_cast<const Documents::Inline*>(item.Get()),
                    output, depth + 1U);
            }
            if (!appended) return appended.GetStatus();
        }
        return {};
    }

    static Base::Result<std::uint32_t> Length(
        const Controls::TextBlock& owner,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document inline nesting exceeds the supported depth");
        }
        std::uint64_t total = owner.Text().SizeBytes();
        for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
            if (!item) continue;
            if (item->RuntimeType() == Documents::LineBreak::StaticTypeId()) {
                ++total;
            } else {
                Base::Result<std::uint32_t> nested = Length(
                    *static_cast<const Documents::Inline*>(item.Get()),
                    depth + 1U);
                if (!nested) return nested.GetStatus();
                total += nested.Value();
            }
            if (total > UINT32_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Flattened document text is too large");
            }
        }
        return static_cast<std::uint32_t>(total);
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

    static Base::Span<const Text::TextHitRegion> Hits(
        const Controls::TextBlock& owner) noexcept {
        return owner.textHitRegions_.AsSpan();
    }

    static bool IsMeasured(
        const Controls::TextBlock& owner) noexcept {
        return owner.GetIsMeasureValid();
    }

private:
    struct Candidate final {
        bool found = false;
        double distance = 0.0;
        std::uint32_t offset = 0U;
        Aero::Rect rect;
    };

    static void Consider(
        Candidate& best,
        Aero::Point point,
        Aero::Rect rect,
        std::uint32_t leading,
        std::uint32_t trailing,
        bool snap) noexcept {
        const bool inside = point.x >= rect.x &&
            point.x <= rect.x + rect.width &&
            point.y >= rect.y &&
            point.y <= rect.y + rect.height;
        if (!inside && !snap) return;
        const double clampedX = std::max(
            rect.x, std::min(point.x, rect.x + rect.width));
        const double clampedY = std::max(
            rect.y, std::min(point.y, rect.y + rect.height));
        const double dx = point.x - clampedX;
        const double dy = point.y - clampedY;
        const double distance = dx * dx + dy * dy;
        if (best.found && distance >= best.distance) return;
        best.found = true;
        best.distance = distance;
        best.offset = point.x > rect.x + rect.width * 0.5
            ? trailing : leading;
        best.rect = rect;
    }

    static Base::Result<std::uint32_t> HitRecursive(
        Controls::TextBlock& owner,
        Aero::Point point,
        std::uint32_t baseOffset,
        bool snap,
        Candidate& best,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document hit testing exceeded the nesting limit");
        }
        const Base::StringView ownText = owner.Text();
        for (const Text::TextHitRegion& hit : owner.textHitRegions_) {
            Aero::Rect rect{
                static_cast<double>(hit.x),
                static_cast<double>(hit.y),
                static_cast<double>(std::max(hit.width, 1.0F)),
                static_cast<double>(std::max(hit.height, 1.0F))};
            const std::uint32_t leading = baseOffset + hit.textOffset;
            const std::uint32_t trailing = leading + hit.textLength;
            Consider(best, point, rect, leading, trailing, snap);
        }
        std::uint32_t cursor = baseOffset + ownText.SizeBytes();
        for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
            if (!item) continue;
            auto& inlineValue =
                *static_cast<Documents::Inline*>(item.Get());
            const Aero::Rect slot = inlineValue.GetLayoutSlot();
            if (item->RuntimeType() == Documents::LineBreak::StaticTypeId()) {
                const Aero::Rect rect{
                    slot.x, slot.y, 1.0,
                    std::max(owner.FontSize() * 1.2, 1.0)};
                Consider(best, point, rect, cursor, cursor + 1U, snap);
                ++cursor;
                continue;
            }
            Aero::Point local{
                point.x - slot.x, point.y - slot.y};
            Base::Result<std::uint32_t> childLength =
                HitRecursive(inlineValue, local, cursor, snap, best, depth + 1U);
            if (!childLength) return childLength.GetStatus();
            cursor += childLength.Value();
        }
        return cursor - baseOffset;
    }

    static Base::Result<bool> RectRecursive(
        const Controls::TextBlock& owner,
        std::uint32_t requested,
        std::uint32_t baseOffset,
        Aero::Point origin,
        Aero::Rect& output,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document rectangle lookup exceeded the nesting limit");
        }
        const std::uint32_t ownEnd = baseOffset + owner.Text().SizeBytes();
        if (requested <= ownEnd && !owner.textHitRegions_.Empty()) {
            const Text::TextHitRegion* selected = &owner.textHitRegions_.Back();
            for (const Text::TextHitRegion& hit : owner.textHitRegions_) {
                if (requested >= baseOffset + hit.textOffset &&
                    requested <= baseOffset + hit.textOffset + hit.textLength) {
                    selected = &hit;
                    break;
                }
            }
            output = {
                origin.x + selected->x,
                origin.y + selected->y,
                std::max(static_cast<double>(selected->width), 1.0),
                std::max(static_cast<double>(selected->height), 1.0)};
            return true;
        }
        std::uint32_t cursor = ownEnd;
        for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
            if (!item) continue;
            const auto& inlineValue =
                *static_cast<const Documents::Inline*>(item.Get());
            const Aero::Rect slot = inlineValue.GetLayoutSlot();
            if (item->RuntimeType() == Documents::LineBreak::StaticTypeId()) {
                if (requested == cursor || requested == cursor + 1U) {
                    output = {
                        origin.x + slot.x,
                        origin.y + slot.y,
                        1.0,
                        std::max(owner.FontSize() * 1.2, 1.0)};
                    return true;
                }
                ++cursor;
                continue;
            }
            Base::Result<std::uint32_t> length = Length(inlineValue);
            if (!length) return length.GetStatus();
            if (requested <= cursor + length.Value()) {
                return RectRecursive(
                    inlineValue, requested, cursor,
                    {origin.x + slot.x, origin.y + slot.y},
                    output, depth + 1U);
            }
            cursor += length.Value();
        }
        return false;
    }

public:
    static Base::Result<Documents::TextPointer> PositionFromPoint(
        Controls::TextBlock& owner,
        Aero::Point point,
        bool snap) noexcept {
        if (!IsMeasured(owner)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Document text positions require a valid measure pass");
        }
        Candidate best;
        Base::Result<std::uint32_t> length =
            HitRecursive(owner, point, 0U, snap, best);
        if (!length) return length.GetStatus();
        if (!best.found) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Point does not resolve to document text");
        }
        best.offset = std::min(best.offset, length.Value());
        return Documents::TextPointer(
            owner, best.offset, Documents::LogicalDirection::Forward);
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
        Aero::Rect rect;
        Base::Result<bool> found = RectRecursive(
            *position.container_, position.offset_, 0U, {}, rect);
        if (!found) return found.GetStatus();
        if (!found.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TextPointer has no arranged character rectangle");
        }
        return rect;
    }
};

} // namespace Aero::Detail

namespace Aero::Controls {

Documents::InlineCollection TextBlock::Inlines() noexcept {
    return Documents::InlineCollection(*this);
}

Documents::InlineCollectionView TextBlock::Inlines() const noexcept {
    return Documents::InlineCollectionView(*this);
}

Documents::InlineCollection TextBlock::GetInlines() noexcept {
    return Inlines();
}

Documents::InlineCollectionView TextBlock::GetInlines() const noexcept {
    return Inlines();
}

Documents::TextPointer TextBlock::ContentStart() noexcept {
    return Documents::TextPointer(
        *this, 0U, Documents::LogicalDirection::Forward);
}

Documents::TextPointer TextBlock::ContentEnd() noexcept {
    Base::Result<std::uint32_t> length =
        Aero::Detail::DocumentTextAccess::Length(*this);
    return Documents::TextPointer(
        *this, length ? length.Value() : 0U,
        Documents::LogicalDirection::Backward);
}

} // namespace Aero::Controls

namespace Aero::Documents {

std::uint32_t InlineCollectionView::Count() const noexcept {
    return owner_ != nullptr
        ? Aero::Detail::DocumentTextAccess::Count(*owner_)
        : 0U;
}

const Inline* InlineCollectionView::At(
    std::uint32_t index) const noexcept {
    return owner_ != nullptr
        ? Aero::Detail::DocumentTextAccess::At(*owner_, index)
        : nullptr;
}

std::uint32_t InlineCollection::Count() const noexcept {
    return owner_ != nullptr
        ? Aero::Detail::DocumentTextAccess::Count(*owner_)
        : 0U;
}

Inline* InlineCollection::At(std::uint32_t index) const noexcept {
    return owner_ != nullptr
        ? Aero::Detail::DocumentTextAccess::At(*owner_, index)
        : nullptr;
}

InlineCollectionView InlineCollection::View() const noexcept {
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
    return Aero::Detail::DocumentTextAccess::Add(
        *owner_, std::move(value));
}

Base::Result<bool> InlineCollection::Remove(
    Inline& value) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "InlineCollection is not bound to an owner");
    }
    return Aero::Detail::DocumentTextAccess::Remove(*owner_, value);
}

Base::Result<void> InlineCollection::Clear() noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "InlineCollection is not bound to an owner");
    }
    return Aero::Detail::DocumentTextAccess::Clear(*owner_);
}

Base::Result<void> CopyText(
    const Controls::TextBlock& container,
    Base::String& output) noexcept {
    output.Clear();
    return Aero::Detail::DocumentTextAccess::AppendText(
        container, output);
}

Base::Result<std::int32_t> TextPointer::CompareTo(
    const TextPointer& other) const noexcept {
    if (!IsValid() || !other.IsValid() ||
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
        Aero::Detail::DocumentTextAccess::Length(*container_);
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
        Aero::Detail::DocumentTextAccess::IsUtf8Boundary(
            *container_, resolved);
    if (!boundary) return boundary.GetStatus();
    if (!boundary.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextPointer offset splits a UTF-8 sequence");
    }
    return TextPointer(*container_, resolved, direction);
}

Base::Result<TextRange> TextRange::TryCreate(
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
    if (!IsValid() || start_.container_ != end_.container_) {
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
    return output.TryAssign(flattened.View().Substr(
        start_.offset_, Length()));
}

Base::Result<TextPointer> GetPositionFromPoint(
    Controls::TextBlock& container,
    Aero::Point point,
    bool snapToText) noexcept {
    return Aero::Detail::DocumentTextAccess::PositionFromPoint(
        container, point, snapToText);
}

Base::Result<Aero::Rect> GetCharacterRect(
    const TextPointer& position) noexcept {
    return Aero::Detail::DocumentTextAccess::CharacterRect(position);
}

Base::StringView Hyperlink::NavigateUri() const noexcept {
    return GetValueOr(NavigateUriProperty, Base::StringView{});
}

Input::ICommand* Hyperlink::GetCommand() const noexcept {
    return GetValueOr(
        CommandProperty,
        Base::Ref<Input::ICommand>{}).Get();
}

Base::Ref<Base::Object> Hyperlink::CommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Base::Ref<Base::Object>{});
}

Aero::UIElement* Hyperlink::CommandTarget() const noexcept {
    return GetValueOr(
        CommandTargetProperty,
        Base::Ref<Aero::UIElement>{}).Get();
}

Base::Result<void> Hyperlink::SetNavigateUri(
    Base::StringView value) noexcept {
    return SetValue(NavigateUriProperty, value);
}

Base::Result<void> Hyperlink::SetCommand(
    Base::Ref<Input::ICommand> command) noexcept {
    return SetValue(CommandProperty, std::move(command));
}

Base::Result<void> Hyperlink::SetCommandParameter(
    Base::Ref<Base::Object> parameter) noexcept {
    return SetValue(CommandParameterProperty, std::move(parameter));
}

Base::Result<void> Hyperlink::SetCommandTarget(
    Base::Ref<Aero::UIElement> target) noexcept {
    return SetValue(CommandTargetProperty, std::move(target));
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
    Base::Result<void> attached = root.TryAddHandler(
        Hyperlink::RequestNavigateEvent,
        requestHandler_, true);
    if (!attached) return attached.GetStatus();
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
    if (args.handled || !handler_ ||
        args.hyperlink == nullptr || args.uri.Empty()) {
        return;
    }
    if (handler_(args.uri, *args.hyperlink)) {
        args.handled = true;
    }
}

} // namespace Aero::Documents
