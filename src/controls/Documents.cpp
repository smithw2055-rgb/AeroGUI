#include <Aero/Documents/Documents.hpp>
#include <Aero/Platform/Clipboard.hpp>

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
        if (!owner.LayoutChildren().Empty() || owner.IsLoaded()) {
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
        if (!owner.LayoutChildren().Empty() || owner.IsLoaded()) {
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
            Base::Result<void> coerced = owner.CoerceDocumentSelection();
            if (!coerced) return coerced.GetStatus();
            return true;
        }
        return false;
    }

    static Base::Result<void> Clear(
        Controls::TextBlock& owner) noexcept {
        if (owner.IsLoaded()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Mounted inline collections require a MountService transaction");
        }
        Base::Result<void> cleared = owner.ClearOwnedInlines();
        return cleared ? owner.CoerceDocumentSelection() : cleared;
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

    static Base::Result<Documents::TextPointer> NextInsertion(
        const Documents::TextPointer& position,
        Documents::LogicalDirection direction) noexcept {
        if (!position.container_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "TextPointer is not bound to a container");
        }
        Base::String text;
        Base::Result<void> copied = AppendText(*position.container_, text);
        if (!copied) return copied.GetStatus();
        std::uint32_t offset = position.offset_;
        if (offset > text.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "TextPointer exceeds the document text");
        }
        if (direction == Documents::LogicalDirection::Forward) {
            if (offset == text.SizeBytes()) return position;
            ++offset;
            while (offset < text.SizeBytes() &&
                (static_cast<unsigned char>(text.View()[offset]) & 0xC0U) == 0x80U) {
                ++offset;
            }
        } else {
            if (offset == 0U) return position;
            --offset;
            while (offset > 0U &&
                (static_cast<unsigned char>(text.View()[offset]) & 0xC0U) == 0x80U) {
                --offset;
            }
        }
        return Documents::TextPointer(
            *position.container_, offset, direction);
    }

    static Base::Span<const Text::TextHitRegion> Hits(
        const Controls::TextBlock& owner) noexcept {
        return owner.textHitRegions_.AsSpan();
    }

    static bool IsMeasured(
        const Controls::TextBlock& owner) noexcept {
        return owner.IsMeasureValid();
    }

private:
    struct Candidate final {
        bool found = false;
        double distance = 0.0;
        std::uint32_t offset = 0U;
        Presentation::Rect rect;
    };

    static void Consider(
        Candidate& best,
        Presentation::Point point,
        Presentation::Rect rect,
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
        Presentation::Point point,
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
            Presentation::Rect rect{
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
            const Presentation::Rect slot = inlineValue.LayoutSlot();
            if (item->RuntimeType() == Documents::LineBreak::StaticTypeId()) {
                const Presentation::Rect rect{
                    slot.x, slot.y, 1.0,
                    std::max(owner.FontSize() * 1.2, 1.0)};
                Consider(best, point, rect, cursor, cursor + 1U, snap);
                ++cursor;
                continue;
            }
            Presentation::Point local{
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
        Presentation::Point origin,
        Presentation::Rect& output,
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
            const Presentation::Rect slot = inlineValue.LayoutSlot();
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

    static Base::Result<void> AppendRangeRect(
        Base::Vector<Presentation::Rect>& output,
        Presentation::Rect rect) noexcept {
        if (!output.Empty()) {
            Presentation::Rect& previous = output.Back();
            if (std::abs(previous.y - rect.y) < 0.5 &&
                std::abs(previous.height - rect.height) < 0.5 &&
                std::abs(previous.x + previous.width - rect.x) < 0.5) {
                previous.width += rect.width;
                return {};
            }
        }
        return output.TryPushBack(rect);
    }

    static Base::Result<std::uint32_t> RangeRectsRecursive(
        const Controls::TextBlock& owner,
        std::uint32_t selectionStart,
        std::uint32_t selectionEnd,
        std::uint32_t baseOffset,
        Presentation::Point origin,
        Base::Vector<Presentation::Rect>& output,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document selection geometry exceeded the nesting limit");
        }
        for (const Text::TextHitRegion& hit : owner.textHitRegions_) {
            const std::uint32_t start = baseOffset + hit.textOffset;
            const std::uint32_t end = start + hit.textLength;
            if (end <= selectionStart || start >= selectionEnd) continue;
            Base::Result<void> appended = AppendRangeRect(
                output, {origin.x + hit.x, origin.y + hit.y,
                    std::max(static_cast<double>(hit.width), 1.0),
                    std::max(static_cast<double>(hit.height), 1.0)});
            if (!appended) return appended.GetStatus();
        }
        std::uint32_t cursor = baseOffset + owner.Text().SizeBytes();
        for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
            if (!item) continue;
            const auto& inlineValue =
                *static_cast<const Documents::Inline*>(item.Get());
            const Presentation::Rect slot = inlineValue.LayoutSlot();
            if (item->RuntimeType() == Documents::LineBreak::StaticTypeId()) {
                if (cursor >= selectionStart && cursor < selectionEnd) {
                    Base::Result<void> appended = AppendRangeRect(
                        output, {origin.x + slot.x, origin.y + slot.y, 1.0,
                            std::max(owner.FontSize() * 1.2, 1.0)});
                    if (!appended) return appended.GetStatus();
                }
                ++cursor;
                continue;
            }
            Base::Result<std::uint32_t> length = Length(inlineValue);
            if (!length) return length.GetStatus();
            const std::uint32_t childEnd = cursor + length.Value();
            if (childEnd > selectionStart && cursor < selectionEnd) {
                Base::Result<std::uint32_t> nested = RangeRectsRecursive(
                    inlineValue, selectionStart, selectionEnd, cursor,
                    {origin.x + slot.x, origin.y + slot.y}, output, depth + 1U);
                if (!nested) return nested.GetStatus();
            }
            cursor = childEnd;
        }
        return cursor - baseOffset;
    }

    static Base::Result<bool> CaretRectRecursive(
        const Controls::TextBlock& owner,
        std::uint32_t requested,
        Documents::LogicalDirection direction,
        std::uint32_t baseOffset,
        Presentation::Point origin,
        Presentation::Rect& output,
        std::uint32_t depth = 0U) noexcept {
        if (depth >= 1024U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Document caret geometry exceeded the nesting limit");
        }
        const std::uint32_t ownEnd = baseOffset + owner.Text().SizeBytes();
        if (!owner.textHitRegions_.Empty() &&
            (requested < ownEnd ||
             (requested == ownEnd &&
              direction == Documents::LogicalDirection::Backward))) {
            const Text::TextHitRegion* selected = &owner.textHitRegions_.Back();
            for (const Text::TextHitRegion& hit : owner.textHitRegions_) {
                const std::uint32_t leading = baseOffset + hit.textOffset;
                const std::uint32_t trailing = leading + hit.textLength;
                if (requested >= leading && requested <= trailing) {
                    selected = &hit;
                    break;
                }
            }
            const std::uint32_t leading = baseOffset + selected->textOffset;
            const std::uint32_t trailing = leading + selected->textLength;
            const bool trailingEdge = requested >= trailing ||
                (requested > leading &&
                 direction == Documents::LogicalDirection::Forward);
            output = {origin.x + selected->x +
                    (trailingEdge ? selected->width : 0.0F),
                origin.y + selected->y, 1.0,
                std::max(static_cast<double>(selected->height), 1.0)};
            return true;
        }
        std::uint32_t cursor = ownEnd;
        for (const Base::Ref<Base::Object>& item : owner.ownedInlines_) {
            if (!item) continue;
            const auto& inlineValue =
                *static_cast<const Documents::Inline*>(item.Get());
            const Presentation::Rect slot = inlineValue.LayoutSlot();
            if (item->RuntimeType() == Documents::LineBreak::StaticTypeId()) {
                if (requested == cursor || requested == cursor + 1U) {
                    output = {origin.x + slot.x, origin.y + slot.y, 1.0,
                        std::max(owner.FontSize() * 1.2, 1.0)};
                    return true;
                }
                ++cursor;
                continue;
            }
            Base::Result<std::uint32_t> length = Length(inlineValue);
            if (!length) return length.GetStatus();
            if (requested <= cursor + length.Value()) {
                return CaretRectRecursive(
                    inlineValue, requested, direction, cursor,
                    {origin.x + slot.x, origin.y + slot.y}, output, depth + 1U);
            }
            cursor += length.Value();
        }
        if (requested == cursor) {
            output = {origin.x, origin.y, 1.0,
                std::max(owner.FontSize() * 1.2, 1.0)};
            return true;
        }
        return false;
    }

public:
    static Base::Result<Documents::TextPointer> PositionFromPoint(
        Controls::TextBlock& owner,
        Presentation::Point point,
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

    static Base::Result<Presentation::Rect> CharacterRect(
        const Documents::TextPointer& position) noexcept {
        if (!position.container_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "TextPointer is not bound to a container");
        }
        if (!position.container_->IsMeasureValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Document character rectangles require a valid measure pass");
        }
        Presentation::Rect rect;
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

    static Base::Result<Presentation::Rect> CaretRect(
        const Documents::TextPointer& position) noexcept {
        if (!position.container_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "TextPointer is not bound to a container");
        }
        if (!position.container_->IsMeasureValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Document caret rectangles require a valid measure pass");
        }
        Presentation::Rect rect;
        Base::Result<bool> found = CaretRectRecursive(
            *position.container_, position.offset_, position.direction_,
            0U, {}, rect);
        if (!found) return found.GetStatus();
        if (!found.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TextPointer has no arranged caret rectangle");
        }
        return rect;
    }

    static Base::Result<void> RangeRects(
        const Documents::TextRange& range,
        Base::Vector<Presentation::Rect>& output) noexcept {
        if (!range.IsValid() || range.Start().Container() == nullptr ||
            range.Start().Container() != range.End().Container()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "TextRange selection geometry requires one document");
        }
        output.Clear();
        if (range.IsEmpty()) return {};
        Base::Result<std::uint32_t> traversed = RangeRectsRecursive(
            *range.Start().Container(), range.Start().Offset(),
            range.End().Offset(), 0U, {}, output);
        return traversed ? Base::Result<void>()
                         : Base::Result<void>(traversed.GetStatus());
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

Documents::TextPointer TextBlock::SelectionAnchor() const noexcept {
    auto& owner = const_cast<TextBlock&>(*this);
    return Documents::TextPointer(
        owner, selectionAnchor_, Documents::LogicalDirection::Forward);
}

Documents::TextPointer TextBlock::CaretPosition() const noexcept {
    auto& owner = const_cast<TextBlock&>(*this);
    return Documents::TextPointer(
        owner, selectionCaret_,
        selectionCaret_ < selectionAnchor_
            ? Documents::LogicalDirection::Backward
            : Documents::LogicalDirection::Forward);
}

Documents::TextSelection TextBlock::Selection() const noexcept {
    return Documents::TextSelection(SelectionAnchor(), CaretPosition());
}

Base::Result<void> TextBlock::SetSelectionOffsets(
    std::uint32_t anchor, std::uint32_t caret) noexcept {
    Base::Result<std::uint32_t> length =
        Aero::Detail::DocumentTextAccess::Length(*this);
    if (!length) return length.GetStatus();
    if (anchor > length.Value() || caret > length.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Text selection exceeds the document text");
    }
    Base::Result<bool> anchorBoundary =
        Aero::Detail::DocumentTextAccess::IsUtf8Boundary(*this, anchor);
    if (!anchorBoundary) return anchorBoundary.GetStatus();
    Base::Result<bool> caretBoundary =
        Aero::Detail::DocumentTextAccess::IsUtf8Boundary(*this, caret);
    if (!caretBoundary) return caretBoundary.GetStatus();
    if (!anchorBoundary.Value() || !caretBoundary.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text selection must use UTF-8 boundaries");
    }
    if (selectionAnchor_ == anchor && selectionCaret_ == caret) {
        return SetCaretBlinkVisible(true);
    }
    selectionAnchor_ = anchor;
    selectionCaret_ = caret;
    caretBlinkVisible_ = true;
    return InvalidateRender();
}

Base::Result<void> TextBlock::SetSelection(
    const Documents::TextPointer& anchor,
    const Documents::TextPointer& caret) noexcept {
    if (anchor.Container() != this || caret.Container() != this) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock selection pointers must belong to this document");
    }
    return SetSelectionOffsets(anchor.Offset(), caret.Offset());
}

Base::Result<void> TextBlock::SelectAll() noexcept {
    Base::Result<std::uint32_t> length =
        Aero::Detail::DocumentTextAccess::Length(*this);
    return length ? SetSelectionOffsets(0U, length.Value())
                  : Base::Result<void>(length.GetStatus());
}

Base::Result<void> TextBlock::ClearSelection() noexcept {
    return SetSelectionOffsets(selectionCaret_, selectionCaret_);
}

Base::Result<void> TextBlock::CopySelection(
    Platform::IClipboard& clipboard) const noexcept {
    Documents::TextSelection selection = Selection();
    if (selection.IsEmpty()) return {};
    Base::String text;
    Base::Result<void> copied = selection.CopyText(text);
    return copied ? clipboard.WriteText(text.View()) : copied;
}

Base::Result<Rect> TextBlock::CaretRectangle() const noexcept {
    return Documents::GetCaretRect(CaretPosition());
}

Base::Result<void> TextBlock::CoerceDocumentSelection() noexcept {
    Base::Result<std::uint32_t> length =
        Aero::Detail::DocumentTextAccess::Length(*this);
    if (!length) return length.GetStatus();
    std::uint32_t anchor = std::min(selectionAnchor_, length.Value());
    std::uint32_t caret = std::min(selectionCaret_, length.Value());
    while (anchor > 0U) {
        Base::Result<bool> boundary =
            Aero::Detail::DocumentTextAccess::IsUtf8Boundary(*this, anchor);
        if (!boundary) return boundary.GetStatus();
        if (boundary.Value()) break;
        --anchor;
    }
    while (caret > 0U) {
        Base::Result<bool> boundary =
            Aero::Detail::DocumentTextAccess::IsUtf8Boundary(*this, caret);
        if (!boundary) return boundary.GetStatus();
        if (boundary.Value()) break;
        --caret;
    }
    if (anchor == selectionAnchor_ && caret == selectionCaret_) return {};
    selectionAnchor_ = anchor;
    selectionCaret_ = caret;
    caretBlinkVisible_ = true;
    return InvalidateRender();
}

Base::Result<void> TextBlock::SetCaretBlinkVisible(bool value) noexcept {
    if (caretBlinkVisible_ == value) return {};
    caretBlinkVisible_ = value;
    return InvalidateRender();
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

Base::Result<TextPointer> TextPointer::GetNextInsertionPosition(
    LogicalDirection direction) const noexcept {
    return Aero::Detail::DocumentTextAccess::NextInsertion(*this, direction);
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

Base::Result<void> TextRange::CopyText(
    Base::String& output) const noexcept {
    if (!IsValid() || start_.Container() != end_.Container()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextRange is not valid");
    }
    Base::String flattened;
    Base::Result<void> copied = Documents::CopyText(
        *start_.Container(), flattened);
    if (!copied) return copied.GetStatus();
    if (end_.Offset() > flattened.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "TextRange exceeds the document text");
    }
    output.Clear();
    return output.TryAssign(flattened.View().Substr(
        start_.Offset(), Length()));
}

Base::Result<TextSelection> TextSelection::TryCreate(
    TextPointer anchor, TextPointer caret) noexcept {
    Base::Result<std::int32_t> compatible = anchor.CompareTo(caret);
    if (!compatible) return compatible.GetStatus();
    return TextSelection(anchor, caret);
}

Base::Result<TextRange> TextSelection::Range() const noexcept {
    if (!IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextSelection is not valid");
    }
    return TextRange::TryCreate(Start(), End());
}

Base::Result<void> TextSelection::CopyText(Base::String& output) const noexcept {
    Base::Result<TextRange> range = Range();
    return range ? range.Value().CopyText(output)
                 : Base::Result<void>(range.GetStatus());
}

Base::Result<TextPointer> GetPositionFromPoint(
    Controls::TextBlock& container,
    Presentation::Point point,
    bool snapToText) noexcept {
    return Aero::Detail::DocumentTextAccess::PositionFromPoint(
        container, point, snapToText);
}

Base::Result<Presentation::Rect> GetCharacterRect(
    const TextPointer& position) noexcept {
    return Aero::Detail::DocumentTextAccess::CharacterRect(position);
}

Base::Result<Presentation::Rect> GetCaretRect(
    const TextPointer& position) noexcept {
    return Aero::Detail::DocumentTextAccess::CaretRect(position);
}

Base::Result<void> GetTextRangeRectangles(
    const TextRange& range,
    Base::Vector<Presentation::Rect>& output) noexcept {
    return Aero::Detail::DocumentTextAccess::RangeRects(range, output);
}

Base::StringView Hyperlink::NavigateUri() const noexcept {
    return GetValueOr(NavigateUriProperty, Base::StringView{});
}

Presentation::ICommand* Hyperlink::Command() const noexcept {
    return GetValueOr(
        CommandProperty,
        Base::Ref<Presentation::ICommand>{}).Get();
}

Base::Ref<Base::Object> Hyperlink::CommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Base::Ref<Base::Object>{});
}

Presentation::UIElement* Hyperlink::CommandTarget() const noexcept {
    return GetValueOr(
        CommandTargetProperty,
        Base::Ref<Presentation::UIElement>{}).Get();
}

Base::Result<void> Hyperlink::SetNavigateUri(
    Base::StringView value) noexcept {
    return SetValue(NavigateUriProperty, value);
}

Base::Result<void> Hyperlink::SetCommand(
    Base::Ref<Presentation::ICommand> command) noexcept {
    return SetValue(CommandProperty, std::move(command));
}

Base::Result<void> Hyperlink::SetCommandParameter(
    Base::Ref<Base::Object> parameter) noexcept {
    return SetValue(CommandParameterProperty, std::move(parameter));
}

Base::Result<void> Hyperlink::SetCommandTarget(
    Base::Ref<Presentation::UIElement> target) noexcept {
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
    Presentation::UIElement& root) noexcept {
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
    const RequestNavigateEventArgs& args) noexcept {
    if (args.handled || !handler_ ||
        args.hyperlink == nullptr || args.uri.Empty()) {
        return;
    }
    if (handler_(args.uri, *args.hyperlink)) {
        args.handled = true;
    }
}

} // namespace Aero::Documents
