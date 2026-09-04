#include "render/DisplayList.hpp"
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include "gui/text/EditableText.hpp"
#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"
#include "TextBlockLayout.hpp"


#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
#include "TextBoxPolicy.inl"

using namespace ::Aero::Render;

namespace {

constexpr double DefaultAdvance = 8.0;
constexpr double DefaultLineHeight = 18.0;
constexpr double CaretWidth = 1.0;
constexpr double ScrollLine = 16.0;

std::uint32_t EffectiveMaximumLength(
    std::uint32_t value) noexcept {
    return value == 0U ? UINT32_MAX : value;
}

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
        const Rect slot = current->GetLayoutSlot();
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
        const Rect slot = current->GetLayoutSlot();
        rect.x += slot.x;
        rect.y += slot.y;
        current = current->LayoutParent();
    }
    return rect;
}

} // namespace

::Aero::Text::EditableTextModel& Model(
    void* value) noexcept {
    return *static_cast<::Aero::Text::EditableTextModel*>(value);
}

const ::Aero::Text::EditableTextModel& Model(
    const void* value) noexcept {
    return *static_cast<const ::Aero::Text::EditableTextModel*>(value);
}

::Aero::Controls::TextDisplayPolicy* DisplayPolicy(
    void* value) noexcept {
    return static_cast<::Aero::Controls::TextDisplayPolicy*>(value);
}

::Aero::Controls::PasswordTextDisplayPolicy* PasswordPolicy(
    void* value) noexcept {
    return static_cast<::Aero::Controls::PasswordTextDisplayPolicy*>(value);
}

::Aero::Controls::TextBlockLayout* LayoutService(
    const ::Aero::Media::Visual& visual) noexcept {
    return AeroGuiInternal::TypedTextLayoutRuntime<::Aero::Controls::TextBlockLayout>(visual);
}

Base::Ref<Media::Brush>
TextBoxBase::GetSelectionBrush() const noexcept {
    return GetValueOr(
        SelectionBrushProperty,
        Base::Ref<Media::Brush>{});
}

void TextBoxBase::SetSelectionBrush(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(SelectionBrushProperty, std::move(value));
}

double TextBoxBase::GetSelectionOpacity() const noexcept {
    return GetValueOr(SelectionOpacityProperty, 0.25);
}

void TextBoxBase::SetSelectionOpacity(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    SetValue(SelectionOpacityProperty, value);
}

Base::Ref<Media::Brush>
TextBoxBase::GetCaretBrush() const noexcept {
    return GetValueOr(
        CaretBrushProperty,
        Base::Ref<Media::Brush>{});
}

void TextBoxBase::SetCaretBrush(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(CaretBrushProperty, std::move(value));
}

TextBox::TextBox() noexcept
    : TextBoxBase(StaticTypeId()),
      model_(new (std::nothrow) ::Aero::Text::EditableTextModel()),
      compositionModel_(new (std::nothrow) ::Aero::Text::EditableTextModel()),
      displayPolicy_(nullptr),
      plainPolicy_(new (std::nothrow) PlainTextDisplayPolicy()),
      textChangedHandler_(
          this,
          &TextBox::OnTextPropertyChanged) {
    displayPolicy_ = plainPolicy_;
    static_cast<void>(AddValueChangedHandlerChecked(
        TextProperty, textChangedHandler_));
}

TextBox::~TextBox() {
    static_cast<void>(RemoveValueChangedHandler(
        TextProperty, textChangedHandler_));
    if (inputMethodHost_ != nullptr) {
        static_cast<void>(
            inputMethodHost_->
                SetClient(nullptr));
        inputMethodHost_ = nullptr;
    }
    if (scrollViewer_ != nullptr &&
        scrollViewer_->GetContentScrollInfo() == this) {
        static_cast<void>(
            scrollViewer_->SetContentScrollInfo(nullptr));
    }
    ReleaseGlyphRuns();
    delete static_cast<::Aero::Text::EditableTextModel*>(model_);
    model_ = nullptr;
    delete static_cast<::Aero::Text::EditableTextModel*>(compositionModel_);
    compositionModel_ = nullptr;
    delete static_cast<::Aero::Controls::PlainTextDisplayPolicy*>(plainPolicy_);
    plainPolicy_ = nullptr;
    displayPolicy_ = nullptr;
}

#include "PasswordBox.inl"

const void*
TextBox::GetActiveModel() const noexcept {
    return compositionActive_
        ? compositionModel_
        : model_;
}

TextSelection
TextBox::GetSelection() const noexcept {
    return Model(GetActiveModel()).Selection();
}

std::uint32_t TextBox::GetCaret() const noexcept {
    return Model(GetActiveModel()).Caret();
}

Base::StringView TextBox::GetText() const noexcept {
    return GetValueOr(TextProperty, Base::StringView());
}

void TextBox::SetText(
    Base::StringView value) noexcept {
    ::Aero::Text::EditableTextModel validation;
    Base::Result<void> checked =
        validation.SetText(value);
    if (!checked) {
        return;
    }
    updatingTextProperty_ = true;
    SetValue(TextProperty, value);
    updatingTextProperty_ = false;
    (void)SynchronizeModel();
}

void TextBox::OnTextPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    if (!updatingTextProperty_) {
        const Base::Result<void> synchronized =
            SynchronizeModel();
        AERO_ASSERT(synchronized);
        static_cast<void>(synchronized);
    }
    RoutedEventArgs changed;
    RaiseEvent(TextChangedEvent, &changed);
}

bool TextBox::GetIsReadOnly() const noexcept {
    return GetValueOr(IsReadOnlyProperty, false);
}

void TextBox::SetIsReadOnly(
    bool value) noexcept {
    if (value && compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    SetValue(IsReadOnlyProperty, value);
    (void)Model(model_).SetReadOnly(value);
}

std::uint32_t TextBox::GetMaxLength() const noexcept {
    return GetValueOr(MaxLengthProperty, 0U);
}

void TextBox::SetMaxLength(
    std::uint32_t value) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    if (model_ != nullptr) {
        Base::Result<void> limited =
            Model(model_).SetMaximumLength(EffectiveMaximumLength(value));
        if (!limited) return;
    }
    SetValue(MaxLengthProperty, value);
}

bool TextBox::GetAcceptsReturn() const noexcept {
    return GetValueOr(AcceptsReturnProperty, false);
}

void TextBox::SetAcceptsReturn(
    bool value) noexcept {
    SetValue(AcceptsReturnProperty, value);
}

TextWrapping
TextBox::GetTextWrapping() const noexcept {
    return GetValueOr(
        TextWrappingProperty,
        TextWrapping::NoWrap);
}

void TextBox::SetTextWrapping(
    TextWrapping value) noexcept {
    SetValue(TextWrappingProperty, value);
}

Base::StringView TextBox::GetPlaceholder() const noexcept {
    return GetValueOr(
        PlaceholderProperty,
        Base::StringView());
}

void TextBox::SetPlaceholder(
    Base::StringView value) noexcept {
    SetValue(PlaceholderProperty, value);
}

Base::Ref<Media::Brush>
TextBox::GetPlaceholderForeground() const noexcept {
    return GetValueOr(
        PlaceholderForegroundProperty,
        Base::Ref<Media::Brush>{});
}

void TextBox::SetPlaceholderForeground(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(PlaceholderForegroundProperty, std::move(value));
}

double TextBox::GetFontSize() const noexcept {
    return GetValueOr(FontSizeProperty, 15.0);
}

void TextBox::SetFontSize(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) return;
    SetValue(FontSizeProperty, value);
}

Base::Ref<Media::FontFamily> TextBox::GetFontFamily() const noexcept {
    return FrameworkElement::GetFontFamily();
}

void TextBox::SetFontFamily(
    Base::Ref<Media::FontFamily> value) noexcept {
    FrameworkElement::SetFontFamily(std::move(value));
}

Base::Result<void> TextBox::SetFontFamily(
    Base::StringView value) noexcept {
    return FrameworkElement::SetFontFamily(value);
}

FontWeight TextBox::GetFontWeight() const noexcept {
    return GetValueOr(
        FontWeightProperty,
        FontWeight::Normal);
}

void TextBox::SetFontWeight(
    FontWeight value) noexcept {
    SetValue(FontWeightProperty, value);
}

FontStyle TextBox::GetFontStyle() const noexcept {
    return GetValueOr(
        FontStyleProperty,
        FontStyle::Normal);
}

void TextBox::SetFontStyle(
    FontStyle value) noexcept {
    SetValue(FontStyleProperty, value);
}

TextAlignment
TextBox::GetTextAlignment() const noexcept {
    return GetValueOr(
        TextAlignmentProperty,
        TextAlignment::Left);
}

void TextBox::SetTextAlignment(
    TextAlignment value) noexcept {
    SetValue(TextAlignmentProperty, value);
}

std::uint32_t TextBox::GetMaxLines() const noexcept {
    return GetValueOr(MaxLinesProperty, 0U);
}

void TextBox::SetMaxLines(
    std::uint32_t value) noexcept {
    SetValue(MaxLinesProperty, value);
}

std::uint32_t TextBox::GetMinLines() const noexcept {
    return GetValueOr(MinLinesProperty, 1U);
}

void TextBox::SetMinLines(
    std::uint32_t value) noexcept {
    SetValue(MinLinesProperty, value);
}

void TextBox::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    Base::Result<void> changed =
        Model(model_).SetSelection(anchor, caret);
    if (!changed) return;
    (void)EnsureCaretVisible();
    (void)InvalidateVisual();
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
        Model(model_).SelectAll();
    if (!selected) {
        return selected;
    }
    Base::Result<void> visible =
        EnsureCaretVisible();
    if (!visible) {
        return visible;
    }
    return InvalidateVisual();
}

Base::Result<void> TextBox::Undo() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    Base::Result<void> undone = Model(model_).Undo();
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
    Base::Result<void> redone = Model(model_).Redo();
    if (!redone) {
        return redone;
    }
    return CommitModelText();
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
        scrollViewer_->GetContentScrollInfo() == this) {
        scrollViewer_->SetContentScrollInfo(nullptr);
    }
    scrollViewer_ = viewer;
    if (viewer == nullptr) {
        return {};
    }
    viewer->SetContentScrollInfo(this);
    viewer->SetCanContentScroll(true);
    return {};
}

void TextBox::SetInputMethodHost(
    Input::ITextInputMethodHost* host) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return;
    }
    if (host == inputMethodHost_) {
        return;
    }
    if (inputMethodHost_ != nullptr) {
        inputMethodHost_->SetClient(nullptr);
    }
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelComposition();
        if (!cancelled) {
            return;
        }
    }
    inputMethodHost_ = nullptr;
    if (host == nullptr) {
        return;
    }
    host->SetClient(this);
    inputMethodHost_ = host;
    (void)UpdateCandidateWindow();
}

Base::Result<void>
TextBox::BeginComposition() noexcept {
    if (compositionActive_) {
        return {};
    }
    if (GetIsReadOnly() || !GetIsEnabled()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "TextBox cannot begin composition while disabled or read-only");
    }
    Base::String snapshot;
    Base::Result<void> copied =
        Model(model_).Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    Base::Result<void> limited =
        Model(compositionModel_).SetMaximumLength(
            UINT32_MAX);
    if (!limited) {
        return limited;
    }
    Base::Result<void> text =
        Model(compositionModel_).SetText(
            snapshot.View());
    if (!text) {
        return text;
    }
    compositionSelection_ =
        Model(model_).Selection();
    Base::Result<void> selected =
        Model(compositionModel_).SetSelection(
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
        InvalidateVisual();
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
    Base::Result<void> constrained =
        ConstrainManualInput(
            filtered,
            model_,
            compositionSelection_);
    if (!constrained) {
        return constrained;
    }
    Base::String snapshot;
    Base::Result<void> copied =
        Model(model_).Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    Base::Result<void> reset =
        Model(compositionModel_).SetText(
            snapshot.View());
    if (!reset) {
        return reset;
    }
    reset = Model(compositionModel_).SetSelection(
        compositionSelection_.anchor,
        compositionSelection_.caret);
    if (!reset) {
        return reset;
    }
    reset = Model(compositionModel_).ReplaceSelection(
        filtered.View());
    if (!reset) {
        return reset;
    }
    Base::Result<void> stored =
        compositionText_.Assign(
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
        InvalidateVisual();
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
    Base::Result<void> constrained =
        ConstrainManualInput(
            filtered,
            model_,
            compositionSelection_);
    if (!constrained) {
        return constrained;
    }
    if (filtered.Empty() && !text.Empty()) {
        return CancelComposition();
    }
    Base::Result<void> selected =
        Model(model_).SetSelection(
            compositionSelection_.anchor,
            compositionSelection_.caret);
    if (!selected) {
        return selected;
    }
    Base::Result<void> replaced =
        Model(model_).ReplaceSelection(
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
        InvalidateVisual();
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
        Model(model_).SetMaximumLength(
            UINT32_MAX);
    if (!maximum) {
        return maximum;
    }
    Base::Result<void> text =
        Model(model_).SetText(GetText());
    if (!text) {
        return text;
    }
    Base::Result<void> readOnly =
        Model(model_).SetReadOnly(GetIsReadOnly());
    if (!readOnly) {
        return readOnly;
    }
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) {
        return measure;
    }
    return InvalidateVisual();
}

Base::Result<void> TextBox::CommitModelText() noexcept {
    Base::String snapshot;
    Base::Result<void> copied =
        Model(model_).Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    updatingTextProperty_ = true;
    SetCurrentValue(TextProperty, snapshot);
    updatingTextProperty_ = false;
    if (passwordOwner_ != nullptr) {
        Base::Result<void> password =
            passwordOwner_->
                SynchronizePasswordFromEditor();
        if (!password) {
            return password.GetStatus();
        }
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
    return InvalidateVisual();
}

Base::Result<void> TextBox::SanitizeInput(
    Base::StringView input,
    Base::String& output) const noexcept {
    if (GetAcceptsReturn()) {
        return output.Assign(input);
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
                output.AppendUnchecked(
                    input.Substr(
                        start, index - start));
            if (!appended) {
                return appended;
            }
        }
        start = index + 1U;
    }
    if (start < input.SizeBytes()) {
        return output.AppendUnchecked(
            input.Substr(start));
    }
    return {};
}

Base::Result<void> TextBox::ConstrainManualInput(
    Base::String& input,
    const void* target,
    TextSelection selection) const noexcept {
    const auto& model = Model(target);
    const std::uint32_t maximum =
        GetMaxLength();
    if (maximum == 0U || input.Empty()) {
        return {};
    }
    const std::uint32_t retained =
        model.GraphemeCount() -
        std::min(
            model.GraphemeCount(),
            selection.GetLength());
    const std::uint32_t available =
        retained >= maximum
        ? 0U
        : maximum - retained;
    ::Aero::Text::EditableTextModel inserted;
    Base::Result<void> parsed =
        inserted.SetText(input.View());
    if (!parsed) {
        return parsed;
    }
    if (inserted.GraphemeCount() <= available) {
        return {};
    }
    Base::Result<std::uint32_t> end =
        inserted.ByteOffsetForGrapheme(available);
    if (!end) {
        return end.GetStatus();
    }
    Base::String truncated;
    Base::Result<void> copied =
        truncated.Assign(
            input.View().Substr(0U, end.Value()));
    if (!copied) {
        return copied;
    }
    input = std::move(truncated);
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
    Base::Result<void> constrained =
        ConstrainManualInput(
            filtered,
            model_,
            Model(model_).Selection());
    if (!constrained) {
        return constrained;
    }
    if (filtered.Empty() &&
        Model(model_).Selection().GetIsEmpty()) {
        return {};
    }
    Base::Result<void> replaced =
        Model(model_).ReplaceSelection(
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
        Model(model_).DeleteBackward();
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
        Model(model_).DeleteForward();
    if (!removed) {
        return removed;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::SelectedText(
    Base::String& output) const noexcept {
    const TextSelection selection =
        Model(model_).Selection();
    if (selection.GetIsEmpty()) {
        output.Clear();
        return {};
    }
    Base::Result<std::uint32_t> begin =
        Model(model_).ByteOffsetForGrapheme(
            selection.GetStart());
    if (!begin) {
        return begin.GetStatus();
    }
    Base::Result<std::uint32_t> end =
        Model(model_).ByteOffsetForGrapheme(
            selection.GetEnd());
    if (!end) {
        return end.GetStatus();
    }
    Base::String snapshot;
    Base::Result<void> copied =
        Model(model_).Snapshot(snapshot);
    if (!copied) {
        return copied;
    }
    return output.Assign(
        snapshot.View().Substr(
            begin.Value(),
            end.Value() - begin.Value()));
}

Base::Result<void> TextBox::CopySelection(
    Input::IClipboard& clipboard) const noexcept {
    if (!DisplayPolicy(displayPolicy_)->AllowsCopy()) {
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
    Input::IClipboard& clipboard) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    if (!DisplayPolicy(displayPolicy_)->AllowsCut()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Text display policy blocks clipboard cut");
    }
    if (GetIsReadOnly()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "TextBox is read-only");
    }
    const TextSelection selection =
        Model(model_).Selection();
    if (selection.GetIsEmpty()) {
        return {};
    }
    Base::Result<void> copied =
        CopySelection(clipboard);
    if (!copied) {
        return copied;
    }
    Base::Result<void> removed =
        Model(model_).ReplaceSelection({});
    if (!removed) {
        return removed;
    }
    return CommitModelText();
}

Base::Result<void> TextBox::Paste(
    Input::IClipboard& clipboard) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    if (GetIsReadOnly()) {
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

#include "TextBoxSelection.inl"

} // namespace Aero::Controls

#include "TextBoxBehavior.inl"
