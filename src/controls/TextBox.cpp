#include <Aero/Controls/TextBox.hpp>

#include <Aero/Core/ObjectServices.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Aero::Controls {
namespace {

constexpr double DefaultAdvance = 8.0;
constexpr double DefaultLineHeight = 18.0;
constexpr double CaretWidth = 1.0;
constexpr double ScrollLine = 16.0;

double ClampOffset(
    double value,
    double extent,
    double viewport) noexcept {
    const double maximum =
        std::max(0.0, extent - viewport);
    return std::min(std::max(0.0, value), maximum);
}

Point ToLocalPoint(
    const UIElement& element,
    Point point) noexcept {
    const UIElement* current = &element;
    while (current != nullptr) {
        const Rect slot = current->LayoutSlot();
        point.x -= slot.x;
        point.y -= slot.y;
        current = current->LayoutParent();
    }
    return point;
}

Rect ToRootRect(
    const UIElement& element,
    Rect rect) noexcept {
    const UIElement* current = &element;
    while (current != nullptr) {
        const Rect slot = current->LayoutSlot();
        rect.x += slot.x;
        rect.y += slot.y;
        current = current->LayoutParent();
    }
    return rect;
}

} // namespace

Base::Result<void>
PlainTextDisplayPolicy::BuildDisplayText(
    const Text::EditableTextModel& model,
    Base::String& output) noexcept {
    return model.Snapshot(output);
}

PasswordTextDisplayPolicy::PasswordTextDisplayPolicy(
    Base::IAllocator* allocator) noexcept
    : mask_(allocator) {
    static_cast<void>(mask_.TryAssign(
        Base::StringView(u8"\u2022")));
}

Base::Result<void> PasswordTextDisplayPolicy::SetMask(
    Base::StringView value) noexcept {
    Text::EditableTextModel validation;
    Base::Result<void> assigned =
        validation.SetText(value);
    if (!assigned ||
        validation.GraphemeCount() != 1U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Password mask must be one grapheme cluster");
    }
    return mask_.TryAssign(value);
}

Base::Result<void>
PasswordTextDisplayPolicy::BuildDisplayText(
    const Text::EditableTextModel& model,
    Base::String& output) noexcept {
    output.Clear();
    const std::uint32_t count =
        model.GraphemeCount();
    if (count != 0U &&
        mask_.SizeBytes() >
            UINT32_MAX / count) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Password display text exceeds capacity");
    }
    Base::Result<void> reserved =
        output.TryReserve(
            mask_.SizeBytes() * count);
    if (!reserved) {
        return reserved;
    }
    Base::String source;
    Base::Result<void> snapshot =
        model.Snapshot(source);
    if (!snapshot) {
        return snapshot;
    }
    for (std::uint32_t index = 0U;
         index < count; ++index) {
        Base::Result<std::uint32_t> begin =
            model.ByteOffsetForGrapheme(index);
        if (!begin) {
            return begin.GetStatus();
        }
        Base::Result<std::uint32_t> end =
            model.ByteOffsetForGrapheme(
                index + 1U);
        if (!end) {
            return end.GetStatus();
        }
        const Base::StringView cluster =
            source.View().Substr(
                begin.Value(),
                end.Value() - begin.Value());
        const bool newline =
            !cluster.Empty() &&
            (cluster[0] == '\r' ||
             cluster[0] == '\n');
        Base::Result<void> appended =
            output.TryAppend(
                newline
                ? cluster
                : mask_.View());
        if (!appended) {
            return appended;
        }
    }
    return {};
}

TextBox::TextBox() noexcept
    : FrameworkElement(StaticTypeId()),
      layoutService_(
          GetCurrentTextBlockLayoutService()),
      displayPolicy_(&plainPolicy_) {}

TextBox::~TextBox() {
    if (inputMethodHost_ != nullptr) {
        static_cast<void>(
            inputMethodHost_->
                SetClient(nullptr));
        inputMethodHost_ = nullptr;
    }
    if (scrollViewer_ != nullptr &&
        scrollViewer_->ContentScrollInfo() == this) {
        static_cast<void>(
            scrollViewer_->SetContentScrollInfo(nullptr));
    }
    ReleaseGlyphRuns();
}

const Text::EditableTextModel&
TextBox::ActiveModel() const noexcept {
    return compositionActive_
        ? compositionModel_
        : model_;
}

Text::TextSelection
TextBox::Selection() const noexcept {
    return ActiveModel().Selection();
}

std::uint32_t TextBox::Caret() const noexcept {
    return ActiveModel().Caret();
}

Base::StringView TextBox::Text() const noexcept {
    return GetValueOr(TextProperty, Base::StringView());
}

Base::Result<void> TextBox::SetText(
    Base::StringView value) noexcept {
    Text::EditableTextModel validation;
    Base::Result<void> checked =
        validation.SetMaximumLength(
            MaximumLength());
    if (checked) {
        checked = validation.SetText(value);
    }
    if (!checked) {
        return checked;
    }
    updatingTextProperty_ = true;
    Base::Result<void> changed =
        SetValue(TextProperty, value);
    updatingTextProperty_ = false;
    if (!changed) {
        return changed;
    }
    return SynchronizeModel();
}

bool TextBox::IsReadOnly() const noexcept {
    return GetValueOr(IsReadOnlyProperty, false);
}

Base::Result<void> TextBox::SetReadOnly(
    bool value) noexcept {
    if (value && compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> changed =
        SetValue(IsReadOnlyProperty, value);
    if (!changed) {
        return changed;
    }
    return model_.SetReadOnly(value);
}

std::uint32_t TextBox::MaximumLength() const noexcept {
    return GetValueOr(MaximumLengthProperty, UINT32_MAX);
}

Base::Result<void> TextBox::SetMaximumLength(
    std::uint32_t value) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> limited =
        model_.SetMaximumLength(value);
    if (!limited) {
        return limited;
    }
    return SetValue(MaximumLengthProperty, value);
}

bool TextBox::AcceptsReturn() const noexcept {
    return GetValueOr(AcceptsReturnProperty, false);
}

Base::Result<void> TextBox::SetAcceptsReturn(
    bool value) noexcept {
    return SetValue(AcceptsReturnProperty, value);
}

Color TextBox::Foreground() const noexcept {
    return GetValueOr(
        ForegroundProperty,
        Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Color TextBox::SelectionBrush() const noexcept {
    return GetValueOr(
        SelectionBrushProperty,
        Color{0.18F, 0.48F, 0.95F, 0.45F});
}

Color TextBox::CaretBrush() const noexcept {
    return GetValueOr(
        CaretBrushProperty,
        Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Base::Result<void> TextBox::SetForeground(
    Color value) noexcept {
    return SetValue(ForegroundProperty, value);
}

Base::Result<void> TextBox::SetSelectionBrush(
    Color value) noexcept {
    return SetValue(SelectionBrushProperty, value);
}

Base::Result<void> TextBox::SetCaretBrush(
    Color value) noexcept {
    return SetValue(CaretBrushProperty, value);
}

Base::Result<void> TextBox::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> changed =
        model_.SetSelection(anchor, caret);
    if (!changed) {
        return changed;
    }
    Base::Result<void> visible =
        EnsureCaretVisible();
    if (!visible) {
        return visible;
    }
    return InvalidateRender();
}

Base::Result<void> TextBox::SelectAll() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> selected =
        model_.SelectAll();
    if (!selected) {
        return selected;
    }
    Base::Result<void> visible =
        EnsureCaretVisible();
    if (!visible) {
        return visible;
    }
    return InvalidateRender();
}

Base::Result<void> TextBox::Undo() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> undone = model_.Undo();
    if (!undone) {
        return undone;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::Redo() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> redone = model_.Redo();
    if (!redone) {
        return redone;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::SetLayoutService(
    ITextBlockLayoutService* service) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (service == layoutService_) {
        return {};
    }
    ReleaseGlyphRuns();
    layoutService_ = service;
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        return measure;
    }
    return InvalidateRender();
}

Base::Result<void> TextBox::SetDisplayPolicy(
    ITextDisplayPolicy* policy) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    ITextDisplayPolicy* next =
        policy != nullptr ? policy : &plainPolicy_;
    if (next == displayPolicy_) {
        return {};
    }
    displayPolicy_ = next;
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        return measure;
    }
    return InvalidateRender();
}

Base::Result<void> TextBox::AttachScrollViewer(
    ScrollViewer* viewer) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (viewer == scrollViewer_) {
        return {};
    }
    if (scrollViewer_ != nullptr &&
        scrollViewer_->ContentScrollInfo() == this) {
        Base::Result<void> detached =
            scrollViewer_->SetContentScrollInfo(nullptr);
        if (!detached) {
            return detached;
        }
    }
    scrollViewer_ = viewer;
    if (viewer == nullptr) {
        return {};
    }
    Base::Result<void> content =
        viewer->SetContentScrollInfo(this);
    if (!content) {
        scrollViewer_ = nullptr;
        return content;
    }
    return viewer->SetCanContentScroll(true);
}

Base::Result<void> TextBox::SetInputMethodHost(
    Platform::ITextInputMethodHost* host) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (host == inputMethodHost_) {
        return {};
    }
    if (inputMethodHost_ != nullptr) {
        Base::Result<void> detached =
            inputMethodHost_->
                SetClient(nullptr);
        if (!detached) {
            return detached;
        }
    }
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelComposition();
        if (!cancelled) {
            return cancelled;
        }
    }
    inputMethodHost_ = nullptr;
    if (host == nullptr) {
        return {};
    }
    Base::Result<void> attached =
        host->SetClient(this);
    if (!attached) {
        return attached;
    }
    inputMethodHost_ = host;
    return UpdateCandidateWindow();
}

Base::Result<void>
TextBox::BeginComposition() noexcept {
    if (compositionActive_) {
        return {};
    }
    if (IsReadOnly() || !IsEnabled()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "TextBox cannot begin composition while disabled or read-only");
    }
    Base::String snapshot;
    Base::Result<void> copied =
        model_.Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    Base::Result<void> limited =
        compositionModel_.SetMaximumLength(
            model_.MaximumLength());
    if (!limited) {
        return limited;
    }
    Base::Result<void> text =
        compositionModel_.SetText(
            snapshot.View());
    if (!text) {
        return text;
    }
    compositionSelection_ =
        model_.Selection();
    Base::Result<void> selected =
        compositionModel_.SetSelection(
            compositionSelection_.anchor,
            compositionSelection_.caret);
    if (!selected) {
        return selected;
    }
    compositionText_.Clear();
    compositionActive_ = true;
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        compositionActive_ = false;
        return measure;
    }
    Base::Result<void> render =
        InvalidateRender();
    if (!render) {
        compositionActive_ = false;
        return render;
    }
    return UpdateCandidateWindow();
}

Base::Result<void> TextBox::UpdateComposition(
    Base::StringView text) noexcept {
    if (!compositionActive_) {
        Base::Result<void> begun =
            BeginComposition();
        if (!begun) {
            return begun;
        }
    }
    Base::String filtered;
    Base::Result<void> sanitized =
        SanitizeInput(text, filtered);
    if (!sanitized) {
        return sanitized;
    }
    Base::String snapshot;
    Base::Result<void> copied =
        model_.Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    Base::Result<void> reset =
        compositionModel_.SetText(
            snapshot.View());
    if (!reset) {
        return reset;
    }
    reset = compositionModel_.SetSelection(
        compositionSelection_.anchor,
        compositionSelection_.caret);
    if (!reset) {
        return reset;
    }
    reset = compositionModel_.ReplaceSelection(
        filtered.View());
    if (!reset) {
        return reset;
    }
    Base::Result<void> stored =
        compositionText_.TryAssign(
            filtered.View());
    if (!stored) {
        return stored;
    }
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        return measure;
    }
    Base::Result<void> render =
        InvalidateRender();
    if (!render) {
        return render;
    }
    return UpdateCandidateWindow();
}

Base::Result<void> TextBox::CommitComposition(
    Base::StringView text) noexcept {
    if (!compositionActive_) {
        Base::Result<void> begun =
            BeginComposition();
        if (!begun) {
            return begun;
        }
    }
    Base::String filtered;
    Base::Result<void> sanitized =
        SanitizeInput(text, filtered);
    if (!sanitized) {
        return sanitized;
    }
    if (filtered.Empty() && !text.Empty()) {
        return CancelComposition();
    }
    Base::Result<void> selected =
        model_.SetSelection(
            compositionSelection_.anchor,
            compositionSelection_.caret);
    if (!selected) {
        return selected;
    }
    Base::Result<void> replaced =
        model_.ReplaceSelection(
            filtered.View());
    if (!replaced) {
        return replaced;
    }
    compositionActive_ = false;
    compositionText_.Clear();
    Base::Result<void> committed =
        CommitModelText();
    if (!committed) {
        return committed;
    }
    return UpdateCandidateWindow();
}

Base::Result<void>
TextBox::CancelComposition() noexcept {
    if (!compositionActive_) {
        return {};
    }
    compositionActive_ = false;
    compositionText_.Clear();
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        return measure;
    }
    Base::Result<void> render =
        InvalidateRender();
    if (!render) {
        return render;
    }
    return EnsureCaretVisible();
}

Base::Result<void>
TextBox::CancelCompositionForFocusLoss() noexcept {
    if (!compositionActive_) {
        return {};
    }
    if (inputMethodHost_ != nullptr) {
        Base::Result<void> native =
            inputMethodHost_->
                CancelNativeComposition();
        if (!native) {
            return native;
        }
    }
    return CancelComposition();
}

Base::Result<void> TextBox::SynchronizeModel() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> maximum =
        model_.SetMaximumLength(
            MaximumLength());
    if (!maximum) {
        return maximum;
    }
    Base::Result<void> text =
        model_.SetText(Text());
    if (!text) {
        return text;
    }
    Base::Result<void> readOnly =
        model_.SetReadOnly(IsReadOnly());
    if (!readOnly) {
        return readOnly;
    }
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        return measure;
    }
    return InvalidateRender();
}

Base::Result<void> TextBox::CommitModelText() noexcept {
    Base::String snapshot;
    Base::Result<void> copied =
        model_.Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    updatingTextProperty_ = true;
    Base::Result<void> committed =
        SetCurrentValue(TextProperty, snapshot);
    updatingTextProperty_ = false;
    if (!committed) {
        return committed;
    }
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        return measure;
    }
    Base::Result<void> visible =
        EnsureCaretVisible();
    if (!visible) {
        return visible;
    }
    return InvalidateRender();
}

Base::Result<void> TextBox::SanitizeInput(
    Base::StringView input,
    Base::String& output) const noexcept {
    if (AcceptsReturn()) {
        return output.TryAssign(input);
    }
    output.Clear();
    std::uint32_t start = 0U;
    for (std::uint32_t index = 0U;
         index < input.SizeBytes(); ++index) {
        if (input[index] != '\r' &&
            input[index] != '\n') {
            continue;
        }
        if (index > start) {
            Base::Result<void> appended =
                output.TryAppendUnchecked(
                    input.Substr(
                        start, index - start));
            if (!appended) {
                return appended;
            }
        }
        start = index + 1U;
    }
    if (start < input.SizeBytes()) {
        return output.TryAppendUnchecked(
            input.Substr(start));
    }
    return {};
}

Base::Result<void> TextBox::ReplaceSelection(
    Base::StringView text) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::String filtered;
    Base::Result<void> sanitized =
        SanitizeInput(text, filtered);
    if (!sanitized) {
        return sanitized;
    }
    if (filtered.Empty()) {
        return {};
    }
    Base::Result<void> replaced =
        model_.ReplaceSelection(
            filtered.View());
    if (!replaced) {
        return replaced;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::DeleteBackward() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> removed =
        model_.DeleteBackward();
    if (!removed) {
        return removed;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::DeleteForward() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> removed =
        model_.DeleteForward();
    if (!removed) {
        return removed;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::SelectedText(
    Base::String& output) const noexcept {
    const Text::TextSelection selection =
        model_.Selection();
    if (selection.Empty()) {
        output.Clear();
        return {};
    }
    Base::Result<std::uint32_t> begin =
        model_.ByteOffsetForGrapheme(
            selection.Start());
    if (!begin) {
        return begin.GetStatus();
    }
    Base::Result<std::uint32_t> end =
        model_.ByteOffsetForGrapheme(
            selection.End());
    if (!end) {
        return end.GetStatus();
    }
    Base::String snapshot;
    Base::Result<void> copied =
        model_.Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    return output.TryAssign(
        snapshot.View().Substr(
            begin.Value(),
            end.Value() - begin.Value()));
}

Base::Result<void> TextBox::CopySelection(
    Platform::IClipboard& clipboard) const noexcept {
    if (!displayPolicy_->AllowsCopy()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Text display policy blocks clipboard copy");
    }
    Base::String selected;
    Base::Result<void> copied =
        SelectedText(selected);
    if (!copied) {
        return copied;
    }
    if (selected.Empty()) {
        return {};
    }
    return clipboard.WriteText(
        selected.View());
}

Base::Result<void> TextBox::CutSelection(
    Platform::IClipboard& clipboard) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    if (!displayPolicy_->AllowsCut()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Text display policy blocks clipboard cut");
    }
    if (IsReadOnly()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "TextBox is read-only");
    }
    const Text::TextSelection selection =
        model_.Selection();
    if (selection.Empty()) {
        return {};
    }
    Base::Result<void> copied =
        CopySelection(clipboard);
    if (!copied) {
        return copied;
    }
    Base::Result<void> removed =
        model_.ReplaceSelection({});
    if (!removed) {
        return removed;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::Paste(
    Platform::IClipboard& clipboard) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    if (IsReadOnly()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "TextBox is read-only");
    }
    Base::String text;
    Base::Result<void> read =
        clipboard.ReadText(text);
    if (!read) {
        return read;
    }
    return ReplaceSelection(text.View());
}

Base::Result<void> TextBox::MoveCaretHorizontal(
    double direction,
    bool extend) noexcept {
    const Text::TextSelection old =
        model_.Selection();
    std::uint32_t next = old.caret;
    if (!extend && !old.Empty()) {
        next = direction < 0.0
            ? old.Start() : old.End();
    } else if (direction < 0.0) {
        if (next != 0U) {
            --next;
        }
    } else if (next < model_.GraphemeCount()) {
        ++next;
    }
    return SetSelection(
        extend ? old.anchor : next,
        next);
}

Base::Result<void>
TextBox::MoveCaretLineBoundary(
    bool end,
    bool extend) noexcept {
    const Text::TextSelection old =
        model_.Selection();
    std::uint32_t lineIndex = 0U;
    for (std::uint32_t line = 0U;
         line < model_.LineCount(); ++line) {
        Base::Result<Text::TextRange> range =
            model_.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t nextStart =
            line + 1U < model_.LineCount()
            ? range.Value().End() + 1U
            : model_.GraphemeCount() + 1U;
        if (old.caret < nextStart) {
            lineIndex = line;
            break;
        }
    }
    Base::Result<Text::TextRange> range =
        model_.LineRange(lineIndex);
    if (!range) {
        return range.GetStatus();
    }
    const std::uint32_t next = end
        ? range.Value().End()
        : range.Value().start;
    return SetSelection(
        extend ? old.anchor : next,
        next);
}

double TextBox::LineHeight() const noexcept {
    if (!caretStops_.Empty() &&
        caretStops_[0].height > 0.0) {
        return caretStops_[0].height;
    }
    return DefaultLineHeight /
        std::max(1.0, DpiScale());
}

Rect TextBox::CaretRectangle() const noexcept {
    if (caretStops_.Empty()) {
        return {
            -scroll_.horizontalOffset,
            -scroll_.verticalOffset,
            CaretWidth / std::max(1.0, DpiScale()),
            LineHeight()};
    }
    const std::uint32_t index =
        std::min(
            ActiveModel().Caret(),
            caretStops_.Size() - 1U);
    const CaretStop& stop =
        caretStops_[index];
    return {
        stop.x - scroll_.horizontalOffset,
        stop.y - scroll_.verticalOffset,
        CaretWidth / std::max(1.0, DpiScale()),
        stop.height};
}

std::uint32_t TextBox::HitTestText(
    Point position) const noexcept {
    if (caretStops_.Empty()) {
        return 0U;
    }
    const double x =
        position.x + scroll_.horizontalOffset;
    const double y =
        position.y + scroll_.verticalOffset;
    std::uint32_t best = 0U;
    double bestDistance =
        std::numeric_limits<double>::infinity();
    for (std::uint32_t index = 0U;
         index < caretStops_.Size(); ++index) {
        const CaretStop& stop =
            caretStops_[index];
        const double vertical =
            y < stop.y
            ? stop.y - y
            : (y > stop.y + stop.height
                ? y - (stop.y + stop.height)
                : 0.0);
        const double distance =
            vertical * 10000.0 +
            std::abs(x - stop.x);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

Base::Result<void>
TextBox::RebuildCaretStops() noexcept {
    caretStops_.Clear();
    const Text::EditableTextModel&
        active = ActiveModel();
    const std::uint32_t graphemes =
        active.GraphemeCount();
    Base::Result<void> capacity =
        caretStops_.TryReserve(
            graphemes + 1U);
    if (!capacity) {
        return capacity;
    }
    const std::uint32_t lines =
        std::max(1U, active.LineCount());
    std::uint32_t maximumLineLength = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<Text::TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        maximumLineLength =
            std::max(
                maximumLineLength,
                range.Value().length);
    }
    const double lineHeight =
        textSize_.height > 0.0
        ? textSize_.height /
            static_cast<double>(lines)
        : DefaultLineHeight /
            std::max(1.0, DpiScale());
    const double advance =
        maximumLineLength != 0U &&
            textSize_.width > 0.0
        ? textSize_.width /
            static_cast<double>(
                maximumLineLength)
        : DefaultAdvance /
            std::max(1.0, DpiScale());

    Base::Result<void> initial =
        caretStops_.TryResize(
            graphemes + 1U);
    if (!initial) {
        return initial;
    }
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<Text::TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t start =
            range.Value().start;
        const std::uint32_t end =
            range.Value().End();
        for (std::uint32_t index = start;
             index <= end; ++index) {
            caretStops_[index] = {
                static_cast<double>(
                    index - start) * advance,
                static_cast<double>(line) *
                    lineHeight,
                lineHeight,
                line};
        }
        if (line + 1U < lines &&
            end < graphemes) {
            caretStops_[end + 1U] = {
                0.0,
                static_cast<double>(line + 1U) *
                    lineHeight,
                lineHeight,
                line + 1U};
        }
    }
    return {};
}

Base::Result<Size> TextBox::MeasureOverride(
    Size availableSize) noexcept {
    Base::Result<void> display =
        displayPolicy_->BuildDisplayText(
            ActiveModel(), displayText_);
    if (!display) {
        return display.GetStatus();
    }
    ReleaseGlyphRuns();
    textSize_ = {};
    if (layoutService_ != nullptr &&
        !displayText_.Empty()) {
        TextBlockLayoutRequest request;
        request.text = displayText_.View();
        request.availableSize = availableSize;
        request.dpiScale = DpiScale();
        TextBlockLayoutResult result;
        Base::Result<void> prepared =
            layoutService_->ShapeAndPrepare(
                request, result);
        if (!prepared) {
            return prepared.GetStatus();
        }
        for (RenderGlyphRunId glyph :
             result.glyphRuns) {
            if (glyph ==
                InvalidRenderGlyphRunId) {
                for (RenderGlyphRunId release :
                     result.glyphRuns) {
                    if (release !=
                        InvalidRenderGlyphRunId) {
                        layoutService_->
                            ReleaseGlyphRun(release);
                    }
                }
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "TextBox layout returned an invalid glyph run");
            }
        }
        glyphRuns_ =
            std::move(result.glyphRuns);
        serviceOwnsGlyphRuns_ =
            !glyphRuns_.Empty();
        textSize_ = result.desiredSize;
    } else {
        std::uint32_t maximumLine = 0U;
        const Text::EditableTextModel&
            active = ActiveModel();
        for (std::uint32_t line = 0U;
             line < active.LineCount();
             ++line) {
            Base::Result<Text::TextRange> range =
                active.LineRange(line);
            if (!range) {
                return range.GetStatus();
            }
            maximumLine = std::max(
                maximumLine,
                range.Value().length);
        }
        textSize_ = {
            static_cast<double>(maximumLine) *
                DefaultAdvance /
                std::max(1.0, DpiScale()),
            static_cast<double>(
                active.LineCount()) *
                DefaultLineHeight /
                std::max(1.0, DpiScale())};
    }
    Base::Result<void> stops =
        RebuildCaretStops();
    if (!stops) {
        return stops.GetStatus();
    }
    scroll_.extentWidth = textSize_.width;
    scroll_.extentHeight =
        std::max(textSize_.height, LineHeight());
    scroll_.horizontalOffset = ClampOffset(
        scroll_.horizontalOffset,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    scroll_.verticalOffset = ClampOffset(
        scroll_.verticalOffset,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    Base::Result<void> visible =
        EnsureCaretVisible();
    if (!visible) {
        return visible.GetStatus();
    }
    static_cast<void>(
        UpdateCandidateWindow());
    const double minimumWidth =
        DefaultAdvance /
        std::max(1.0, DpiScale());
    return Size{
        std::min(
            std::max(minimumWidth, textSize_.width),
            availableSize.width),
        std::min(
            std::max(LineHeight(), textSize_.height),
            availableSize.height)};
}

Base::Result<Size> TextBox::ArrangeOverride(
    Size finalSize) noexcept {
    Base::Result<bool> viewport =
        SetViewport(finalSize);
    if (!viewport) {
        return viewport.GetStatus();
    }
    Base::Result<void> visible =
        EnsureCaretVisible();
    if (!visible) {
        return visible.GetStatus();
    }
    static_cast<void>(
        UpdateCandidateWindow());
    return finalSize;
}

Base::Result<void> TextBox::BuildDisplayList(
    DisplayListBuilder& builder) noexcept {
    Base::Result<void> clip =
        builder.PushClip({
            0.0, 0.0,
            RenderSize().width,
            RenderSize().height});
    if (!clip) {
        return clip;
    }
    Base::Result<void> transform =
        builder.PushTransform({
            1.0, 0.0, 0.0, 1.0,
            -scroll_.horizontalOffset,
            -scroll_.verticalOffset});
    if (!transform) {
        return transform;
    }
    const Text::TextSelection selection =
        ActiveModel().Selection();
    if (!selection.Empty() &&
        !caretStops_.Empty()) {
        const std::uint32_t begin =
            selection.Start();
        const std::uint32_t end =
            selection.End();
        std::uint32_t index = begin;
        while (index < end) {
            const CaretStop& first =
                caretStops_[index];
            std::uint32_t lineEnd =
                index + 1U;
            while (lineEnd < end &&
                lineEnd < caretStops_.Size() &&
                caretStops_[lineEnd].line ==
                    first.line) {
                ++lineEnd;
            }
            const CaretStop& last =
                caretStops_[
                    std::min(
                        lineEnd,
                        caretStops_.Size() - 1U)];
            const double width =
                last.line == first.line
                ? std::max(
                    0.0,
                    last.x - first.x)
                : std::max(
                    DefaultAdvance,
                    textSize_.width - first.x);
            Base::Result<void> filled =
                builder.FillRect(
                    {first.x, first.y,
                     width, first.height},
                    SelectionBrush());
            if (!filled) {
                return filled;
            }
            index = lineEnd;
        }
    }
    for (RenderGlyphRunId glyph :
         glyphRuns_) {
        Base::Result<void> drawn =
            builder.DrawGlyphRun(
                glyph, Foreground());
        if (!drawn) {
            return drawn;
        }
    }
    if (IsKeyboardFocused()) {
        Rect caret = CaretRectangle();
        caret.x += scroll_.horizontalOffset;
        caret.y += scroll_.verticalOffset;
        Base::Result<void> drawn =
            builder.FillRect(
                caret, CaretBrush());
        if (!drawn) {
            return drawn;
        }
    }
    Base::Result<void> popTransform =
        builder.PopTransform();
    if (!popTransform) {
        return popTransform;
    }
    return builder.PopClip();
}

void TextBox::ReleaseGlyphRuns() noexcept {
    if (serviceOwnsGlyphRuns_ &&
        layoutService_ != nullptr) {
        for (RenderGlyphRunId glyph :
             glyphRuns_) {
            layoutService_->ReleaseGlyphRun(glyph);
        }
    }
    glyphRuns_.Clear();
    serviceOwnsGlyphRuns_ = false;
}

Base::Result<bool> TextBox::SetViewport(
    Size viewport) noexcept {
    if (!IsValidLayoutSize(viewport)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBox viewport is invalid");
    }
    const ScrollData old = scroll_;
    scroll_.viewportWidth = viewport.width;
    scroll_.viewportHeight = viewport.height;
    scroll_.horizontalOffset = ClampOffset(
        scroll_.horizontalOffset,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    scroll_.verticalOffset = ClampOffset(
        scroll_.verticalOffset,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    const bool changed =
        old.viewportWidth !=
            scroll_.viewportWidth ||
        old.viewportHeight !=
            scroll_.viewportHeight ||
        old.horizontalOffset !=
            scroll_.horizontalOffset ||
        old.verticalOffset !=
            scroll_.verticalOffset;
    if (changed) {
        Base::Result<void> invalidated =
            InvalidateRender();
        if (!invalidated) {
            return invalidated.GetStatus();
        }
    }
    return changed;
}

Base::Result<bool>
TextBox::SetHorizontalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBox horizontal offset is invalid");
    }
    const double next = ClampOffset(
        value,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    if (next == scroll_.horizontalOffset) {
        return false;
    }
    scroll_.horizontalOffset = next;
    Base::Result<void> invalidated =
        InvalidateRender();
    return invalidated
        ? Base::Result<bool>(true)
        : Base::Result<bool>(
            invalidated.GetStatus());
}

Base::Result<bool>
TextBox::SetVerticalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBox vertical offset is invalid");
    }
    const double next = ClampOffset(
        value,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    if (next == scroll_.verticalOffset) {
        return false;
    }
    scroll_.verticalOffset = next;
    Base::Result<void> invalidated =
        InvalidateRender();
    return invalidated
        ? Base::Result<bool>(true)
        : Base::Result<bool>(
            invalidated.GetStatus());
}

Base::Result<bool> TextBox::LineHorizontal(
    double direction) noexcept {
    return SetHorizontalOffset(
        scroll_.horizontalOffset +
        direction * ScrollLine);
}

Base::Result<bool> TextBox::LineVertical(
    double direction) noexcept {
    return SetVerticalOffset(
        scroll_.verticalOffset +
        direction * LineHeight());
}

Base::Result<bool> TextBox::PageHorizontal(
    double direction) noexcept {
    return SetHorizontalOffset(
        scroll_.horizontalOffset +
        direction * scroll_.viewportWidth);
}

Base::Result<bool> TextBox::PageVertical(
    double direction) noexcept {
    return SetVerticalOffset(
        scroll_.verticalOffset +
        direction * scroll_.viewportHeight);
}

Base::Result<void>
TextBox::EnsureCaretVisible() noexcept {
    const Rect caret = CaretRectangle();
    double horizontal =
        scroll_.horizontalOffset;
    double vertical =
        scroll_.verticalOffset;
    const double contentX =
        caret.x + scroll_.horizontalOffset;
    const double contentY =
        caret.y + scroll_.verticalOffset;
    if (contentX < horizontal) {
        horizontal = contentX;
    } else if (contentX + caret.width >
        horizontal + scroll_.viewportWidth) {
        horizontal = contentX + caret.width -
            scroll_.viewportWidth;
    }
    if (contentY < vertical) {
        vertical = contentY;
    } else if (contentY + caret.height >
        vertical + scroll_.viewportHeight) {
        vertical = contentY + caret.height -
            scroll_.viewportHeight;
    }
    Base::Result<bool> movedX =
        SetHorizontalOffset(horizontal);
    if (!movedX) {
        return movedX.GetStatus();
    }
    Base::Result<bool> movedY =
        SetVerticalOffset(vertical);
    if (!movedY) {
        return movedY.GetStatus();
    }
    return {};
}

Base::Result<void>
TextBox::UpdateCandidateWindow() noexcept {
    if (inputMethodHost_ == nullptr ||
        !compositionActive_) {
        return {};
    }
    Platform::ImeCandidateWindow candidate;
    candidate.caret =
        ToRootRect(*this, CaretRectangle());
    candidate.dpiScale = DpiScale();
    return inputMethodHost_->
        SetCandidateWindow(candidate);
}

TextBoxInteractionManager::
TextBoxInteractionManager(
    ObjectTree& tree,
    RoutedEventManager& events,
    PointerInputManager& pointer,
    FocusManager& focus,
    Platform::IClipboard& clipboard) noexcept
    : tree_(&tree),
      events_(&events),
      pointer_(&pointer),
      focus_(&focus),
      clipboard_(&clipboard),
      mouseDownHandler_(
          this,
          &TextBoxInteractionManager::
              OnMouseDown),
      mouseMoveHandler_(
          this,
          &TextBoxInteractionManager::
              OnMouseMove),
      mouseUpHandler_(
          this,
          &TextBoxInteractionManager::
              OnMouseUp),
      keyDownHandler_(
          this,
          &TextBoxInteractionManager::
              OnKeyDown),
      textInputHandler_(
          this,
          &TextBoxInteractionManager::
              OnTextInput),
      focusChangedHandler_(
          this,
          &TextBoxInteractionManager::
              OnFocusChanged),
      propertyChangedHandler_(
          this,
          &TextBoxInteractionManager::
              OnPropertyChanged),
      captureChangedHandler_(
          this,
          &TextBoxInteractionManager::
              OnCaptureChanged) {}

TextBoxInteractionManager::
~TextBoxInteractionManager() noexcept {
    while (!records_.Empty()) {
        TextBox* textBox =
            Resolve(records_.Size() - 1U);
        if (textBox == nullptr) {
            records_.PopBack();
        } else {
            static_cast<void>(Detach(*textBox));
        }
    }
}

std::uint32_t TextBoxInteractionManager::Find(
    const TextBox& textBox) const noexcept {
    for (std::uint32_t index = 0U;
         index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(
                records_[index].handle) ==
            &textBox) {
            return index;
        }
    }
    return UINT32_MAX;
}

TextBox* TextBoxInteractionManager::Resolve(
    std::uint32_t index) noexcept {
    if (index >= records_.Size()) {
        return nullptr;
    }
    Visual* visual =
        tree_->ResolveHandle(
            records_[index].handle);
    if (visual == nullptr ||
        visual->RuntimeType() !=
            TextBox::StaticTypeId()) {
        return nullptr;
    }
    return static_cast<TextBox*>(visual);
}

void TextBoxInteractionManager::RemoveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != records_.Size()) {
        records_[index] = std::move(
            records_[records_.Size() - 1U]);
    }
    records_.PopBack();
}

Base::Result<void>
TextBoxInteractionManager::Attach(
    TextBox& textBox) noexcept {
    if (Find(textBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "TextBox is already attached");
    }
    if (!textBox.IsLoaded() ||
        textBox.OwningTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TextBox must be loaded in the interaction tree");
    }
    Base::Result<void> synced =
        textBox.SynchronizeModel();
    if (!synced) {
        return synced;
    }
    Record record;
    record.handle = textBox.Handle();
    Base::Result<void> appended =
        records_.TryPushBack(record);
    if (!appended) {
        return appended;
    }
    if (!captureSubscribed_) {
        Base::Result<void> capture =
            pointer_->TryAddCaptureChanged(
                captureChangedHandler_);
        if (!capture) {
            records_.PopBack();
            return capture;
        }
        captureSubscribed_ = true;
    }
    Base::Result<void> result =
        textBox.TryAddHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (result) {
        result = textBox.TryAddHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (result) {
        result = textBox.TryAddHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (result) {
        result = textBox.TryAddHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    }
    if (result) {
        result = textBox.TryAddHandler(
            UIElement::TextInputEvent,
            textInputHandler_);
    }
    if (result) {
        result = textBox.TryAddHandler(
            UIElement::LostKeyboardFocusEvent,
            focusChangedHandler_);
    }
    if (result) {
        result =
            textBox.TryAddValueChangedHandler(
                TextBox::TextProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.TryAddValueChangedHandler(
                TextBox::IsReadOnlyProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.TryAddValueChangedHandler(
                TextBox::MaximumLengthProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.TryAddValueChangedHandler(
                UIElement::IsEnabledProperty,
                propertyChangedHandler_);
    }
    if (!result) {
        const Base::Status failure =
            result.GetStatus();
        static_cast<void>(Detach(textBox));
        return failure;
    }
    return {};
}

Base::Result<bool>
TextBoxInteractionManager::Detach(
    TextBox& textBox) noexcept {
    const std::uint32_t index = Find(textBox);
    if (index == UINT32_MAX) {
        return false;
    }
    Record& record = records_[index];
    if (record.dragging) {
        Base::Result<bool> released =
            pointer_->ReleasePointer(
                record.pointerId);
        if (!released) {
            return released.GetStatus();
        }
    }
    static_cast<void>(textBox.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::MouseMoveEvent,
        mouseMoveHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::MouseUpEvent,
        mouseUpHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::TextInputEvent,
        textInputHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::LostKeyboardFocusEvent,
        focusChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            TextBox::TextProperty,
            propertyChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            TextBox::IsReadOnlyProperty,
            propertyChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            TextBox::MaximumLengthProperty,
            propertyChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            UIElement::IsEnabledProperty,
            propertyChangedHandler_));
    RemoveAt(index);
    if (records_.Empty() &&
        captureSubscribed_) {
        static_cast<void>(
            pointer_->RemoveCaptureChanged(
                captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return true;
}

void TextBoxInteractionManager::OnMouseDown(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& textBox =
        *static_cast<TextBox*>(sender);
    if (args.changedButton !=
            MouseButton::Left ||
        !textBox.IsEnabled()) {
        return;
    }
    const std::uint32_t index =
        Find(textBox);
    if (index == UINT32_MAX) {
        return;
    }
    const Point local =
        ToLocalPoint(
            textBox, args.position);
    const std::uint32_t caret =
        textBox.HitTestText(local);
    static_cast<void>(
        textBox.SetSelection(caret, caret));
    static_cast<void>(
        focus_->SetFocus(&textBox));
    Base::Result<void> captured =
        pointer_->CapturePointer(
            args.pointerId, textBox);
    if (captured) {
        records_[index].pointerId =
            args.pointerId;
        records_[index].anchor = caret;
        records_[index].dragging = true;
    }
    args.handled = true;
}

void TextBoxInteractionManager::OnMouseMove(
    Base::Object* sender,
    const MouseEventArgs& args) noexcept {
    auto& textBox =
        *static_cast<TextBox*>(sender);
    const std::uint32_t index =
        Find(textBox);
    if (index == UINT32_MAX ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.pointerId) {
        return;
    }
    const Point local =
        ToLocalPoint(
            textBox, args.position);
    static_cast<void>(
        textBox.SetSelection(
            records_[index].anchor,
            textBox.HitTestText(local)));
    args.handled = true;
}

void TextBoxInteractionManager::OnMouseUp(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& textBox =
        *static_cast<TextBox*>(sender);
    const std::uint32_t index =
        Find(textBox);
    if (index == UINT32_MAX ||
        args.changedButton !=
            MouseButton::Left ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.pointerId) {
        return;
    }
    const Point local =
        ToLocalPoint(
            textBox, args.position);
    static_cast<void>(
        textBox.SetSelection(
            records_[index].anchor,
            textBox.HitTestText(local)));
    records_[index].dragging = false;
    static_cast<void>(
        pointer_->ReleasePointer(
            args.pointerId));
    args.handled = true;
}

void TextBoxInteractionManager::OnKeyDown(
    Base::Object* sender,
    const KeyEventArgs& args) noexcept {
    auto& textBox =
        *static_cast<TextBox*>(sender);
    if (!textBox.IsEnabled()) {
        return;
    }
    const bool shift =
        HasKeyboardModifier(
            args.modifiers,
            KeyboardModifiers::Shift);
    const bool control =
        HasKeyboardModifier(
            args.modifiers,
            KeyboardModifiers::Control);
    Base::Result<void> result;
    bool handled = true;
    if (control &&
        args.key == KeyboardKeyA) {
        result = textBox.SelectAll();
    } else if (control &&
        args.key == KeyboardKeyC) {
        result = textBox.CopySelection(
            *clipboard_);
    } else if (control &&
        args.key == KeyboardKeyX) {
        result = textBox.CutSelection(
            *clipboard_);
    } else if (control &&
        args.key == KeyboardKeyV) {
        result = textBox.Paste(
            *clipboard_);
    } else if (control &&
        args.key == KeyboardKeyZ) {
        result = shift
            ? textBox.Redo()
            : textBox.Undo();
    } else if (control &&
        args.key == KeyboardKeyY) {
        result = textBox.Redo();
    } else if (args.key ==
        KeyboardKeyLeft) {
        result =
            textBox.MoveCaretHorizontal(
                -1.0, shift);
    } else if (args.key ==
        KeyboardKeyRight) {
        result =
            textBox.MoveCaretHorizontal(
                1.0, shift);
    } else if (args.key ==
        KeyboardKeyHome) {
        result =
            textBox.MoveCaretLineBoundary(
                false, shift);
    } else if (args.key ==
        KeyboardKeyEnd) {
        result =
            textBox.MoveCaretLineBoundary(
                true, shift);
    } else if (args.key ==
        KeyboardKeyBackspace) {
        result =
            textBox.DeleteBackward();
    } else if (args.key ==
        KeyboardKeyDelete) {
        result =
            textBox.DeleteForward();
    } else if (args.key ==
            KeyboardKeyEnter &&
        textBox.AcceptsReturn()) {
        result = textBox.ReplaceSelection(
            Base::StringView("\n"));
    } else {
        handled = false;
    }
    if (handled && result) {
        args.handled = true;
    }
}

void TextBoxInteractionManager::OnTextInput(
    Base::Object* sender,
    const TextCompositionEventArgs& args) noexcept {
    auto& textBox =
        *static_cast<TextBox*>(sender);
    if (!textBox.IsEnabled() ||
        textBox.IsReadOnly()) {
        return;
    }
    if (textBox.IsComposing()) {
        Base::Result<void> cancelled =
            textBox.
                CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    Base::Result<void> inserted =
        textBox.ReplaceSelection(args.text);
    if (inserted) {
        args.handled = true;
    }
}

void TextBoxInteractionManager::OnFocusChanged(
    Base::Object* sender,
    const KeyboardFocusChangedEventArgs& args) noexcept {
    auto& textBox =
        *static_cast<TextBox*>(sender);
    if (args.newFocus == &textBox) {
        return;
    }
    static_cast<void>(
        textBox.
            CancelCompositionForFocusLoss());
    const std::uint32_t index =
        Find(textBox);
    if (index == UINT32_MAX ||
        !records_[index].dragging) {
        return;
    }
    records_[index].dragging = false;
    static_cast<void>(
        pointer_->ReleasePointer(
            records_[index].pointerId));
}

void TextBoxInteractionManager::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    auto& textBox =
        static_cast<TextBox&>(object);
    if (args.property ==
            TextBox::TextProperty &&
        !textBox.updatingTextProperty_) {
        static_cast<void>(
            textBox.SynchronizeModel());
    } else if (args.property ==
            TextBox::IsReadOnlyProperty) {
        if (args.newValue.AsBoolean()) {
            static_cast<void>(
                textBox.
                    CancelCompositionForFocusLoss());
        }
        static_cast<void>(
            textBox.model_.SetReadOnly(
                args.newValue.AsBoolean()));
    } else if (args.property ==
            TextBox::MaximumLengthProperty) {
        static_cast<void>(
            textBox.
                CancelCompositionForFocusLoss());
        static_cast<void>(
            textBox.model_.SetMaximumLength(
                static_cast<std::uint32_t>(
                    args.newValue.
                        AsUnsignedInteger())));
    } else if (args.property ==
                   UIElement::IsEnabledProperty &&
               !args.newValue.AsBoolean()) {
        static_cast<void>(
            textBox.
                CancelCompositionForFocusLoss());
    }
}

void TextBoxInteractionManager::OnCaptureChanged(
    std::uint32_t pointerId,
    UIElement* target,
    bool captured) noexcept {
    if (captured || target == nullptr) {
        return;
    }
    for (std::uint32_t index = 0U;
         index < records_.Size(); ++index) {
        TextBox* textBox = Resolve(index);
        if (textBox != target ||
            records_[index].pointerId !=
                pointerId) {
            continue;
        }
        records_[index].dragging = false;
        return;
    }
}

} // namespace Aero::Controls
