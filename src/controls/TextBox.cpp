#include "../render/DisplayList.hpp"
#include <Aero/Controls/Text.hpp>
#include "../render/DrawingContextAccess.hpp"

#include "TextLayoutService.hpp"

#include "core/ObjectServices.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include "../ui/RuntimeManagers.hpp"
#include "RuntimeManagers.hpp"

namespace Aero::Controls {
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
    : TextBoxBase(StaticTypeId()),
      layoutService_(nullptr),
      displayPolicy_(&plainPolicy_),
      textChangedHandler_(
          this,
          &TextBox::OnTextPropertyChanged) {
    static_cast<void>(TryAddValueChangedHandler(
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
        scrollViewer_->ContentScrollInfo() == this) {
        static_cast<void>(
            scrollViewer_->SetContentScrollInfo(nullptr));
    }
    ReleaseGlyphRuns();
}

PasswordBox::PasswordBox() noexcept
    : TextBoxBase(StaticTypeId()) {
    editor_.displayPolicy_ =
        &passwordPolicy_;
    editor_.coordinateOwner_ = this;
    editor_.passwordOwner_ = this;
}

Base::Result<void> PasswordBox::SetPassword(
    Base::StringView value) noexcept {
    if (password_.View() == value) return {};
    Text::EditableTextModel next;
    Base::Result<void> limited =
        next.SetMaximumLength(
            EffectiveMaximumLength(
                MaximumLength()));
    if (limited) limited = next.SetText(value);
    if (!limited) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Password exceeds its UTF-8 or maximum-length contract");
    }
    Base::String nextPassword;
    Base::Result<void> copied =
        nextPassword.TryAssign(value);
    if (!copied) return copied.GetStatus();
    Base::Result<void> modelLimit =
        validation_.SetMaximumLength(
            EffectiveMaximumLength(
                MaximumLength()));
    if (modelLimit) {
        modelLimit = validation_.SetText(value);
    }
    if (!modelLimit) return modelLimit.GetStatus();
    password_ = std::move(nextPassword);
    synchronizingEditor_ = true;
    Base::Result<void> editor =
        editor_.SetText(password_.View());
    synchronizingEditor_ = false;
    if (!editor) {
        return editor.GetStatus();
    }
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    Base::Result<void> render =
        InvalidateRender();
    if (!render) return render.GetStatus();
    RoutedEventArgs args;
    Base::Result<void> raised =
        RaiseRoutedEvent(PasswordChangedEvent, &args);
    return !raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized
        ? raised
        : Base::Result<void>{};
}

Base::StringView PasswordBox::PasswordChar() const noexcept {
    return GetValueOr(
        PasswordCharProperty,
        Base::StringView(u8"\u2022"));
}

Base::Result<void> PasswordBox::SetPasswordChar(
    Base::StringView value) noexcept {
    PasswordTextDisplayPolicy validation;
    Base::Result<void> valid =
        validation.SetMask(value);
    if (!valid) return valid.GetStatus();
    Base::Result<void> changed =
        SetValue(PasswordCharProperty, value);
    if (!changed) return changed.GetStatus();
    Base::Result<void> mask =
        passwordPolicy_.SetMask(value);
    if (!mask) return mask.GetStatus();
    Base::Result<void> measure =
        editor_.InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    measure = InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    return InvalidateRender();
}

std::uint32_t PasswordBox::MaximumLength() const noexcept {
    return GetValueOr(
        MaximumLengthProperty,
        0U);
}

Base::Result<void> PasswordBox::SetMaximumLength(
    std::uint32_t value) noexcept {
    const std::uint32_t effective =
        EffectiveMaximumLength(value);
    if (validation_.GraphemeCount() > effective) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "PasswordBox maximum length is shorter than the current password");
    }
    Base::Result<void> stored =
        SetValue(MaximumLengthProperty, value);
    if (!stored) return stored.GetStatus();
    Base::Result<void> validation =
        validation_.SetMaximumLength(effective);
    if (!validation) {
        return validation.GetStatus();
    }
    return editor_.SetMaximumLength(value);
}

Color PasswordBox::Foreground() const noexcept {
    return GetValueOr(
        ForegroundProperty,
        Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Base::Result<void> PasswordBox::SetForeground(
    Color value) noexcept {
    Base::Result<void> changed =
        SetValue(ForegroundProperty, value);
    if (!changed) return changed.GetStatus();
    return editor_.SetForeground(value);
}

Color PasswordBox::SelectionBrush() const noexcept {
    return GetValueOr(
        SelectionBrushProperty,
        Color{0.18F, 0.48F, 0.95F, 0.45F});
}

double PasswordBox::SelectionOpacity() const noexcept {
    return GetValueOr(SelectionOpacityProperty, 0.25);
}

Color PasswordBox::CaretBrush() const noexcept {
    return GetValueOr(
        CaretBrushProperty,
        Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Base::Result<void> PasswordBox::SetSelectionBrush(
    Color value) noexcept {
    Base::Result<void> changed =
        SetValue(SelectionBrushProperty, value);
    if (!changed) return changed.GetStatus();
    return editor_.SetSelectionBrush(value);
}

Base::Result<void> PasswordBox::SetSelectionOpacity(
    double value) noexcept {
    Base::Result<void> changed =
        SetValue(SelectionOpacityProperty, value);
    if (!changed) return changed.GetStatus();
    return editor_.SetSelectionOpacity(value);
}

Base::Result<void> PasswordBox::SetCaretBrush(
    Color value) noexcept {
    Base::Result<void> changed =
        SetValue(CaretBrushProperty, value);
    if (!changed) return changed.GetStatus();
    return editor_.SetCaretBrush(value);
}

Text::TextSelection
PasswordBox::Selection() const noexcept {
    return editor_.Selection();
}

std::uint32_t PasswordBox::Caret() const noexcept {
    return editor_.Caret();
}

Base::Result<void> PasswordBox::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    return editor_.SetSelection(anchor, caret);
}

Base::Result<void> PasswordBox::SelectAll() noexcept {
    return editor_.SelectAll();
}

Base::Result<void> PasswordBox::SetInputMethodHost(
    Platform::ITextInputMethodHost* host) noexcept {
    return editor_.SetInputMethodHost(host);
}

Platform::ITextInputMethodHost*
PasswordBox::InputMethodHost() const noexcept {
    return editor_.InputMethodHost();
}

bool PasswordBox::IsComposing() const noexcept {
    return editor_.IsComposing();
}

Base::Result<Size> PasswordBox::MeasureOverride(
    Size availableSize) noexcept {
    Size templateSize{};
    if (TemplateChild() != nullptr) {
        Base::Result<Size> measuredTemplate =
            Control::MeasureOverride(availableSize);
        if (!measuredTemplate) {
            return measuredTemplate.GetStatus();
        }
        templateSize = measuredTemplate.Value();
    }
    Base::Result<void> dpi =
        editor_.SetLayoutRounding(
            UseLayoutRounding(),
            DpiScale());
    if (!dpi) return dpi.GetStatus();
    Base::Result<void> foreground =
        editor_.SetForeground(Foreground());
    if (foreground) {
        foreground =
            editor_.SetSelectionBrush(
                SelectionBrush());
    }
    if (foreground) {
        foreground = editor_.SetSelectionOpacity(
            SelectionOpacity());
    }
    if (foreground) {
        foreground =
            editor_.SetCaretBrush(
                CaretBrush());
    }
    if (!foreground) {
        return foreground.GetStatus();
    }
    Base::Result<Size> measuredEditor =
        editor_.MeasureOverride(availableSize);
    if (!measuredEditor) {
        return measuredEditor.GetStatus();
    }
    const Size editorSize = measuredEditor.Value();
    return Size{
        std::max(templateSize.width, editorSize.width),
        std::max(templateSize.height, editorSize.height)};
}

Base::Result<Size> PasswordBox::ArrangeOverride(
    Size finalSize) noexcept {
    if (TemplateChild() != nullptr) {
        Base::Result<Size> arrangedTemplate =
            Control::ArrangeOverride(finalSize);
        if (!arrangedTemplate) {
            return arrangedTemplate.GetStatus();
        }
    }
    Base::Result<bool> viewport =
        editor_.SetViewport(finalSize);
    if (!viewport) {
        return viewport.GetStatus();
    }
    return finalSize;
}

Base::Result<void> PasswordBox::OnRender(
    DrawingContext& context) noexcept {
    return editor_.RenderEditor(
        context,
        RenderSize(),
        IsKeyboardFocused());
}

Base::Result<void>
PasswordBox::SynchronizeEditorFromPassword()
    noexcept {
    if (synchronizingEditor_) {
        return {};
    }
    synchronizingEditor_ = true;
    Base::Result<void> maximum =
        editor_.SetMaximumLength(
            MaximumLength());
    if (maximum) {
        maximum =
            editor_.SetText(password_.View());
    }
    synchronizingEditor_ = false;
    return maximum;
}

Base::Result<void>
PasswordBox::SynchronizePasswordFromEditor()
    noexcept {
    if (synchronizingEditor_ ||
        password_.View() == editor_.Text()) {
        return {};
    }
    Base::String next;
    Base::Result<void> copied =
        next.TryAssign(editor_.Text());
    if (!copied) return copied.GetStatus();
    Base::Result<void> model =
        validation_.SetMaximumLength(
            EffectiveMaximumLength(
                MaximumLength()));
    if (model) {
        model = validation_.SetText(
            next.View());
    }
    if (!model) return model.GetStatus();
    password_ = std::move(next);
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    Base::Result<void> render =
        InvalidateRender();
    if (!render) return render.GetStatus();
    RoutedEventArgs args;
    Base::Result<void> raised =
        RaiseRoutedEvent(
            PasswordChangedEvent, &args);
    return !raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized
        ? raised
        : Base::Result<void>{};
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
        validation.SetText(value);
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
    const Base::Result<void> raised =
        RaiseRoutedEvent(
            TextChangedEvent, &changed);
    if (!raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized) {
        static_cast<void>(raised);
    }
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
    return GetValueOr(MaximumLengthProperty, 0U);
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
    return SetValue(MaximumLengthProperty, value);
}

bool TextBox::AcceptsReturn() const noexcept {
    return GetValueOr(AcceptsReturnProperty, false);
}

Base::Result<void> TextBox::SetAcceptsReturn(
    bool value) noexcept {
    return SetValue(AcceptsReturnProperty, value);
}

Text::TextWrapping
TextBox::TextWrapping() const noexcept {
    return GetValueOr(
        TextWrappingProperty,
        Text::TextWrapping::NoWrap);
}

Base::Result<void> TextBox::SetTextWrapping(
    Text::TextWrapping value) noexcept {
    return SetValue(TextWrappingProperty, value);
}

Base::StringView TextBox::Placeholder() const noexcept {
    return GetValueOr(
        PlaceholderProperty,
        Base::StringView());
}

Base::Result<void> TextBox::SetPlaceholder(
    Base::StringView value) noexcept {
    return SetValue(PlaceholderProperty, value);
}

Color TextBox::PlaceholderForeground() const noexcept {
    return GetValueOr(
        PlaceholderForegroundProperty,
        Color{
            123.0F / 255.0F,
            128.0F / 255.0F,
            133.0F / 255.0F,
            1.0F});
}

Base::Result<void> TextBox::SetPlaceholderForeground(
    Color value) noexcept {
    return SetValue(
        PlaceholderForegroundProperty,
        value);
}

double TextBox::FontSize() const noexcept {
    return GetValueOr(FontSizeProperty, 15.0);
}

Base::Result<void> TextBox::SetFontSize(
    double value) noexcept {
    return SetValue(FontSizeProperty, value);
}

Base::StringView TextBox::FontFamily() const noexcept {
    return FrameworkElement::FontFamily();
}

Base::Result<void> TextBox::SetFontFamily(
    Base::StringView value) noexcept {
    return SetValue(FontFamilyProperty, value);
}

FontWeight TextBox::GetFontWeight() const noexcept {
    return GetValueOr(
        FontWeightProperty,
        FontWeight::Normal);
}

Base::Result<void> TextBox::SetFontWeight(
    FontWeight value) noexcept {
    return SetValue(FontWeightProperty, value);
}

Text::FontStyle TextBox::GetFontStyle() const noexcept {
    return GetValueOr(
        FontStyleProperty,
        Text::FontStyle::Normal);
}

Base::Result<void> TextBox::SetFontStyle(
    Text::FontStyle value) noexcept {
    return SetValue(FontStyleProperty, value);
}

Text::TextAlignment
TextBox::TextAlignment() const noexcept {
    return GetValueOr(
        TextAlignmentProperty,
        Text::TextAlignment::Start);
}

Base::Result<void> TextBox::SetTextAlignment(
    Text::TextAlignment value) noexcept {
    return SetValue(TextAlignmentProperty, value);
}

std::uint32_t TextBox::MaxLines() const noexcept {
    return GetValueOr(MaxLinesProperty, 0U);
}

Base::Result<void> TextBox::SetMaxLines(
    std::uint32_t value) noexcept {
    return SetValue(MaxLinesProperty, value);
}

std::uint32_t TextBox::MinLines() const noexcept {
    return GetValueOr(MinLinesProperty, 1U);
}

Base::Result<void> TextBox::SetMinLines(
    std::uint32_t value) noexcept {
    return SetValue(MinLinesProperty, value);
}

Color TextBox::Foreground() const noexcept {
    return GetValueOr(
        ForegroundProperty,
        Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Color TextBox::SelectionBrush() const noexcept {
    return GetValueOr(
        SelectionBrushProperty,
        Color{
            46.0F / 255.0F,
            174.0F / 255.0F,
            235.0F / 255.0F,
            1.0F});
}

double TextBox::SelectionOpacity() const noexcept {
    return GetValueOr(
        SelectionOpacityProperty,
        0.25);
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

Base::Result<void> TextBox::SetSelectionOpacity(
    double value) noexcept {
    return SetValue(SelectionOpacityProperty, value);
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
            UINT32_MAX);
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
            UINT32_MAX);
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

Base::Result<void> TextBox::ConstrainManualInput(
    Base::String& input,
    const Text::EditableTextModel& target,
    Text::TextSelection selection) const noexcept {
    const std::uint32_t maximum =
        MaximumLength();
    if (maximum == 0U || input.Empty()) {
        return {};
    }
    const std::uint32_t retained =
        target.GraphemeCount() -
        std::min(
            target.GraphemeCount(),
            selection.Length());
    const std::uint32_t available =
        retained >= maximum
        ? 0U
        : maximum - retained;
    Text::EditableTextModel inserted;
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
        truncated.TryAssign(
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
            model_.Selection());
    if (!constrained) {
        return constrained;
    }
    if (filtered.Empty() &&
        model_.Selection().Empty()) {
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
    return FontSize() * 1.6 /
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
        position.x -
            Padding().left +
            scroll_.horizontalOffset;
    const double y =
        position.y -
            Padding().top +
            scroll_.verticalOffset;
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
    std::uint32_t visualLines = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<Text::TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t length =
            range.Value().length;
        const std::uint32_t wrapped =
            wrapColumns_ == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (length + wrapColumns_ - 1U) /
                      wrapColumns_);
        visualLines += wrapped;
    }
    visualLines = std::max(1U, visualLines);
    const double lineHeight =
        textSize_.height > 0.0
        ? textSize_.height /
            static_cast<double>(visualLines)
        : FontSize() * 1.6 /
            std::max(1.0, DpiScale());
    const double advance =
        wrapColumns_ != UINT32_MAX
        ? DefaultAdvance *
              FontSize() / 16.0 /
              std::max(1.0, DpiScale())
        : maximumLineLength != 0U &&
            textSize_.width > 0.0
        ? textSize_.width /
            static_cast<double>(
                maximumLineLength)
        : DefaultAdvance *
              FontSize() / 16.0 /
            std::max(1.0, DpiScale());

    Base::Result<void> initial =
        caretStops_.TryResize(
            graphemes + 1U);
    if (!initial) {
        return initial;
    }
    std::uint32_t visualLineBase = 0U;
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
            const std::uint32_t offset =
                index - start;
            std::uint32_t visualLineOffset = 0U;
            std::uint32_t column = offset;
            if (wrapColumns_ != UINT32_MAX) {
                visualLineOffset =
                    offset / wrapColumns_;
                column = offset % wrapColumns_;
                if (offset == range.Value().length &&
                    offset != 0U &&
                    column == 0U) {
                    --visualLineOffset;
                    column = wrapColumns_;
                }
            }
            caretStops_[index] = {
                static_cast<double>(column) *
                    advance,
                static_cast<double>(
                    visualLineBase +
                    visualLineOffset) *
                    lineHeight,
                lineHeight,
                visualLineBase +
                    visualLineOffset};
        }
        const std::uint32_t wrappedLines =
            wrapColumns_ == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (range.Value().length +
                   wrapColumns_ - 1U) /
                      wrapColumns_);
        visualLineBase += wrappedLines;
        if (line + 1U < lines &&
            end < graphemes) {
            caretStops_[end + 1U] = {
                0.0,
                static_cast<double>(
                    visualLineBase) *
                    lineHeight,
                lineHeight,
                visualLineBase};
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
    wrapColumns_ = UINT32_MAX;
    showingPlaceholder_ =
        displayText_.Empty() &&
        !Placeholder().Empty();
    if (showingPlaceholder_) {
        display = displayText_.TryAssign(
            Placeholder());
        if (!display) {
            return display.GetStatus();
        }
    }
    const Thickness padding = Padding();
    const Size contentAvailable =
        Deflate(availableSize, padding);
    if (TextWrapping() !=
            Text::TextWrapping::NoWrap &&
        contentAvailable.width > 0.0) {
        const double fallbackAdvance =
            DefaultAdvance *
            FontSize() / 16.0 /
            std::max(1.0, DpiScale());
        const double columns =
            std::floor(
                contentAvailable.width /
                fallbackAdvance);
        wrapColumns_ = static_cast<std::uint32_t>(
            std::min(
                static_cast<double>(UINT32_MAX),
                std::max(1.0, columns)));
    }
    if (layoutService_ != nullptr &&
        !displayText_.Empty()) {
        Detail::TextLayoutRequest request;
        request.text = displayText_.View();
        request.availableSize =
            contentAvailable;
        request.dpiScale = DpiScale();
        request.pixelSize =
            static_cast<float>(FontSize());
        request.lineHeight =
            static_cast<float>(
                FontSize() * 1.6);
        Base::StringView family =
            FontFamily();
        const bool defaultFamily =
            family.Empty() ||
            family == Base::StringView(
                "Segoe UI");
        if (defaultFamily) {
            const bool bold =
                GetFontWeight() ==
                    FontWeight::Bold ||
                GetFontWeight() ==
                    FontWeight::SemiBold;
            const bool italic =
                GetFontStyle() !=
                    Text::FontStyle::Normal;
            if (bold && italic) {
                family = Base::StringView(
                    "Segoe UI Bold Italic");
            } else if (bold) {
                family = Base::StringView(
                    "Segoe UI Bold");
            } else if (italic) {
                family = Base::StringView(
                    "Segoe UI Italic");
            }
        }
        request.fontFamily = family;
        request.wrapping = TextWrapping();
        request.alignment = TextAlignment();
        Detail::TextLayoutResult result;
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
        std::uint32_t visualLineCount = 0U;
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
            visualLineCount +=
                wrapColumns_ == UINT32_MAX
                ? 1U
                : std::max(
                      1U,
                      (range.Value().length +
                       wrapColumns_ - 1U) /
                          wrapColumns_);
        }
        const std::uint32_t visibleColumns =
            wrapColumns_ == UINT32_MAX
            ? maximumLine
            : std::min(
                  maximumLine,
                  wrapColumns_);
        textSize_ = {
            static_cast<double>(visibleColumns) *
                DefaultAdvance *
                FontSize() / 16.0 /
                std::max(1.0, DpiScale()),
            static_cast<double>(
                std::max(1U, visualLineCount)) *
                FontSize() * 1.6 /
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
    if (IsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return visible.GetStatus();
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    const double minimumWidth =
        DefaultAdvance *
        FontSize() / 16.0 /
        std::max(1.0, DpiScale());
    Size desired{
        std::min(
            std::max(minimumWidth, textSize_.width),
            contentAvailable.width),
        std::min(
            std::max(LineHeight(), textSize_.height),
            contentAvailable.height)};
    const std::uint32_t maximumLines =
        MaxLines();
    const std::uint32_t minimumLines =
        MinLines();
    if (maximumLines != 0U) {
        const double lineBoxHeight =
            std::max(
                LineHeight(),
                FontSize() * 1.6);
        const double maximumHeight =
            lineBoxHeight *
            static_cast<double>(
                maximumLines);
        if (TextWrapping() !=
                Text::TextWrapping::NoWrap &&
            !displayText_.Empty()) {
            desired.height = std::min(
                contentAvailable.height,
                maximumHeight);
        } else {
            desired.height = std::min(
                desired.height,
                maximumHeight);
        }
    }
    desired.height = std::max(
        desired.height,
        std::min(
            contentAvailable.height,
            std::max(
                LineHeight(),
                FontSize() * 1.6) *
                static_cast<double>(
                    minimumLines)));
    return Inflate(desired, padding);
}

Base::Result<Size> TextBox::ArrangeOverride(
    Size finalSize) noexcept {
    const Size contentViewport =
        Deflate(finalSize, Padding());
    Base::Result<bool> updated =
        SetViewport(contentViewport);
    if (!updated) {
        return updated.GetStatus();
    }
    if (IsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return visible.GetStatus();
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    return finalSize;
}

Base::Result<void> TextBox::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Detail::DrawingContextAccess::Builder(context);
    const Rect bounds{
        0.0, 0.0,
        RenderSize().width,
        RenderSize().height};
    const Thickness border =
        BorderThickness();
    const double borderThickness = std::max(
        std::max(border.left, border.right),
        std::max(border.top, border.bottom));
    Color borderBrush =
        BorderBrush();
    if (IsKeyboardFocused() && IsEnabled()) {
        borderBrush = Color{
            11.0F / 255.0F,
            128.0F / 255.0F,
            193.0F / 255.0F,
            1.0F};
    } else if (IsMouseOver() && IsEnabled()) {
        borderBrush = Color{
            93.0F / 255.0F,
            100.0F / 255.0F,
            105.0F / 255.0F,
            1.0F};
    }
    Base::Result<void> chrome =
        builder.FillRoundedRect(
            bounds,
            Background(),
            1.75);
    if (!chrome) {
        return chrome;
    }
    if (borderThickness > 0.0 &&
        borderBrush.alpha > 0.0F) {
        chrome = builder.StrokeRect(
            bounds,
            borderBrush,
            IsKeyboardFocused()
                ? std::max(1.0, borderThickness)
                : borderThickness);
        if (!chrome) {
            return chrome;
        }
    }
    return RenderEditor(
        context,
        RenderSize(),
        IsKeyboardFocused());
}

Base::Result<void>
TextBox::RenderEditor(
    DrawingContext& context,
    Size viewport,
    bool drawCaret) noexcept {
    auto& builder = Aero::Detail::DrawingContextAccess::Builder(context);
    const Thickness padding = Padding();
    const Rect contentBounds{
        padding.left,
        padding.top,
        std::max(
            0.0,
            viewport.width -
                padding.left -
                padding.right),
        std::max(
            0.0,
            viewport.height -
                padding.top -
                padding.bottom)};
    Base::Result<void> clip =
        builder.PushClip(contentBounds);
    if (!clip) {
        return clip;
    }
    Base::Result<void> transform =
        builder.PushTransform({
            1.0, 0.0, 0.0, 1.0,
            padding.left -
                scroll_.horizontalOffset,
            padding.top -
                scroll_.verticalOffset});
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
            Color selectionColor =
                SelectionBrush();
            selectionColor.alpha *=
                static_cast<float>(
                    SelectionOpacity());
            Base::Result<void> filled =
                builder.FillRect(
                    {first.x, first.y,
                     width, first.height},
                    selectionColor);
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
                glyph,
                showingPlaceholder_
                ? PlaceholderForeground()
                : Foreground());
        if (!drawn) {
            return drawn;
        }
    }
    if (drawCaret &&
        !showingPlaceholder_) {
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
    Rect caret = CaretRectangle();
    caret.x += Padding().left;
    caret.y += Padding().top;
    UIElement& owner =
        coordinateOwner_ != nullptr
        ? *coordinateOwner_
        : static_cast<UIElement&>(*this);
    candidate.caret =
        ToRootRect(owner, caret);
    candidate.dpiScale = DpiScale();
    return inputMethodHost_->
        SetCandidateWindow(candidate);
}

} // namespace Aero::Controls

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Controls;

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
        UIElement* owner =
            ResolveOwner(records_.Size() - 1U);
        if (owner == nullptr) {
            records_.PopBack();
        } else if (records_[
                       records_.Size() - 1U].
                       password) {
            static_cast<void>(Detach(
                *static_cast<PasswordBox*>(
                    owner)));
        } else {
            static_cast<void>(Detach(
                *static_cast<TextBox*>(owner)));
        }
    }
}

std::uint32_t TextBoxInteractionManager::Find(
    const UIElement& owner) const noexcept {
    for (std::uint32_t index = 0U;
         index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(
                records_[index].handle) ==
            &owner) {
            return index;
        }
    }
    return UINT32_MAX;
}

UIElement*
TextBoxInteractionManager::ResolveOwner(
    std::uint32_t index) noexcept {
    if (index >= records_.Size()) {
        return nullptr;
    }
    Visual* visual =
        tree_->ResolveHandle(
            records_[index].handle);
    if (visual == nullptr) {
        return nullptr;
    }
    const TypeId expected =
        records_[index].password
        ? PasswordBox::StaticTypeId()
        : TextBox::StaticTypeId();
    if (visual->RuntimeType() != expected) {
        return nullptr;
    }
    return static_cast<UIElement*>(visual);
}

TextBox*
TextBoxInteractionManager::ResolveEditor(
    std::uint32_t index) noexcept {
    UIElement* owner = ResolveOwner(index);
    if (owner == nullptr) return nullptr;
    return records_[index].password
        ? &static_cast<PasswordBox*>(
              owner)->editor_
        : static_cast<TextBox*>(owner);
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

Base::Result<void>
TextBoxInteractionManager::Attach(
    PasswordBox& passwordBox) noexcept {
    if (Find(passwordBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "PasswordBox is already attached");
    }
    if (!passwordBox.IsLoaded() ||
        passwordBox.OwningTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "PasswordBox must be loaded in the interaction tree");
    }
    Base::Result<void> synced =
        passwordBox.passwordPolicy_.SetMask(
            passwordBox.PasswordChar());
    if (synced) {
        synced =
            passwordBox.
                SynchronizeEditorFromPassword();
    }
    if (synced) {
        synced =
            passwordBox.editor_.SetForeground(
                passwordBox.Foreground());
    }
    if (synced) {
        synced =
            passwordBox.editor_.
                SetSelectionBrush(
                    passwordBox.SelectionBrush());
    }
    if (synced) {
        synced = passwordBox.editor_.SetSelectionOpacity(
            passwordBox.SelectionOpacity());
    }
    if (synced) {
        synced =
            passwordBox.editor_.SetCaretBrush(
                passwordBox.CaretBrush());
    }
    if (!synced) return synced.GetStatus();

    Record record;
    record.handle = passwordBox.Handle();
    record.password = true;
    Base::Result<void> appended =
        records_.TryPushBack(record);
    if (!appended) return appended.GetStatus();
    if (!captureSubscribed_) {
        Base::Result<void> capture =
            pointer_->TryAddCaptureChanged(
                captureChangedHandler_);
        if (!capture) {
            records_.PopBack();
            return capture.GetStatus();
        }
        captureSubscribed_ = true;
    }

    Base::Result<void> result =
        passwordBox.TryAddHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (result) {
        result = passwordBox.TryAddHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (result) {
        result = passwordBox.TryAddHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (result) {
        result = passwordBox.TryAddHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    }
    if (result) {
        result = passwordBox.TryAddHandler(
            UIElement::TextInputEvent,
            textInputHandler_);
    }
    if (result) {
        result = passwordBox.TryAddHandler(
            UIElement::LostKeyboardFocusEvent,
            focusChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                TryAddValueChangedHandler(
                    PasswordBox::
                        PasswordCharProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                TryAddValueChangedHandler(
                    PasswordBox::
                        MaximumLengthProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                TryAddValueChangedHandler(
                    PasswordBox::
                        ForegroundProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                TryAddValueChangedHandler(
                    PasswordBox::
                        SelectionBrushProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                TryAddValueChangedHandler(
                    PasswordBox::
                        CaretBrushProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                TryAddValueChangedHandler(
                    UIElement::
                        IsEnabledProperty,
                    propertyChangedHandler_);
    }
    if (!result) {
        const Base::Status failure =
            result.GetStatus();
        static_cast<void>(
            Detach(passwordBox));
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

Base::Result<bool>
TextBoxInteractionManager::Detach(
    PasswordBox& passwordBox) noexcept {
    const std::uint32_t index =
        Find(passwordBox);
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
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::TextInputEvent,
            textInputHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::
                LostKeyboardFocusEvent,
            focusChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    PasswordCharProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    MaximumLengthProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    ForegroundProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    SelectionBrushProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    CaretBrushProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                UIElement::IsEnabledProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            SetInputMethodHost(nullptr));
    RemoveAt(index);
    if (records_.Empty() &&
        captureSubscribed_) {
        static_cast<void>(
            pointer_->
                RemoveCaptureChanged(
                    captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return true;
}

void TextBoxInteractionManager::OnMouseDown(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    if (args.changedButton !=
            MouseButton::Left ||
        !owner.IsEnabled()) {
        return;
    }
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (editor == nullptr) {
        return;
    }
    const Point local =
        ToLocalPoint(
            owner, args.position);
    const std::uint32_t caret =
        editor->HitTestText(local);
    static_cast<void>(
        editor->SetSelection(caret, caret));
    static_cast<void>(
        focus_->SetFocus(&owner));
    Base::Result<void> captured =
        pointer_->CapturePointer(
            args.pointerId, owner);
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
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    if (index == UINT32_MAX ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.pointerId) {
        return;
    }
    TextBox* editor =
        ResolveEditor(index);
    if (editor == nullptr) return;
    const Point local =
        ToLocalPoint(
            owner, args.position);
    static_cast<void>(
        editor->SetSelection(
            records_[index].anchor,
            editor->HitTestText(local)));
    args.handled = true;
}

void TextBoxInteractionManager::OnMouseUp(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    if (index == UINT32_MAX ||
        args.changedButton !=
            MouseButton::Left ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.pointerId) {
        return;
    }
    TextBox* editor =
        ResolveEditor(index);
    if (editor == nullptr) return;
    const Point local =
        ToLocalPoint(
            owner, args.position);
    static_cast<void>(
        editor->SetSelection(
            records_[index].anchor,
            editor->HitTestText(local)));
    records_[index].dragging = false;
    static_cast<void>(
        pointer_->ReleasePointer(
            args.pointerId));
    args.handled = true;
}

void TextBoxInteractionManager::OnKeyDown(
    Base::Object* sender,
    const KeyEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (!owner.IsEnabled() ||
        editor == nullptr) {
        return;
    }
    const bool password =
        records_[index].password;
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
        result = editor->SelectAll();
    } else if (control &&
        args.key == KeyboardKeyC) {
        result = password
            ? Base::Result<void>{}
            : editor->CopySelection(
                  *clipboard_);
    } else if (control &&
        args.key == KeyboardKeyX) {
        result = password
            ? editor->ReplaceSelection(
                  Base::StringView{})
            : editor->CutSelection(
                  *clipboard_);
    } else if (control &&
        args.key == KeyboardKeyV) {
        result = editor->Paste(
            *clipboard_);
    } else if (control &&
        args.key == KeyboardKeyZ) {
        result = shift
            ? editor->Redo()
            : editor->Undo();
    } else if (control &&
        args.key == KeyboardKeyY) {
        result = editor->Redo();
    } else if (args.key ==
        KeyboardKeyLeft) {
        result =
            editor->MoveCaretHorizontal(
                -1.0, shift);
    } else if (args.key ==
        KeyboardKeyRight) {
        result =
            editor->MoveCaretHorizontal(
                1.0, shift);
    } else if (args.key ==
        KeyboardKeyHome) {
        result =
            editor->MoveCaretLineBoundary(
                false, shift);
    } else if (args.key ==
        KeyboardKeyEnd) {
        result =
            editor->MoveCaretLineBoundary(
                true, shift);
    } else if (args.key ==
        KeyboardKeyBackspace) {
        result =
            editor->DeleteBackward();
    } else if (args.key ==
        KeyboardKeyDelete) {
        result =
            editor->DeleteForward();
    } else if (args.key ==
            KeyboardKeyEnter &&
        editor->AcceptsReturn()) {
        result = editor->ReplaceSelection(
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
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (!owner.IsEnabled() ||
        editor == nullptr ||
        editor->IsReadOnly()) {
        return;
    }
    if (editor->IsComposing()) {
        Base::Result<void> cancelled =
            editor->
                CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    Base::Result<void> inserted =
        editor->ReplaceSelection(args.text);
    if (inserted) {
        args.handled = true;
    }
}

void TextBoxInteractionManager::OnFocusChanged(
    Base::Object* sender,
    const KeyboardFocusChangedEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    if (args.newFocus == &owner) {
        return;
    }
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (editor == nullptr) return;
    static_cast<void>(
        editor->
            CancelCompositionForFocusLoss());
    if (!records_[index].dragging) {
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
    if (object.RuntimeType() ==
        PasswordBox::StaticTypeId()) {
        auto& passwordBox =
            static_cast<PasswordBox&>(object);
        if (args.property ==
                PasswordBox::
                    PasswordCharProperty) {
            static_cast<void>(
                passwordBox.passwordPolicy_.
                    SetMask(
                        passwordBox.
                            PasswordChar()));
            static_cast<void>(
                passwordBox.editor_.
                    InvalidateMeasure());
            static_cast<void>(
                passwordBox.editor_.
                    InvalidateRender());
        } else if (args.property ==
                PasswordBox::
                    MaximumLengthProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    CancelCompositionForFocusLoss());
            static_cast<void>(
                passwordBox.validation_.
                    SetMaximumLength(
                        EffectiveMaximumLength(
                            passwordBox.
                                MaximumLength())));
            static_cast<void>(
                passwordBox.editor_.
                    SetMaximumLength(
                        passwordBox.
                            MaximumLength()));
        } else if (args.property ==
                PasswordBox::
                    ForegroundProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetForeground(
                        passwordBox.
                            Foreground()));
        } else if (args.property ==
                PasswordBox::
                    SelectionBrushProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetSelectionBrush(
                        passwordBox.
                            SelectionBrush()));
        } else if (args.property ==
                PasswordBox::
                    SelectionOpacityProperty) {
            static_cast<void>(
                passwordBox.editor_.SetSelectionOpacity(
                    passwordBox.SelectionOpacity()));
        } else if (args.property ==
                PasswordBox::
                    CaretBrushProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetCaretBrush(
                        passwordBox.
                            CaretBrush()));
        } else if (args.property ==
                       UIElement::
                           IsEnabledProperty &&
                   !args.newValue.
                       AsBoolean()) {
            static_cast<void>(
                passwordBox.editor_.
                    CancelCompositionForFocusLoss());
        }
        return;
    }
    auto& textBox =
        static_cast<TextBox&>(object);
    if (args.property ==
            TextBox::TextProperty) {
        if (!textBox.updatingTextProperty_) {
            static_cast<void>(
                textBox.SynchronizeModel());
        }
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
        UIElement* owner =
            ResolveOwner(index);
        if (owner != target ||
            records_[index].pointerId !=
                pointerId) {
            continue;
        }
        records_[index].dragging = false;
        return;
    }
}

} // namespace Aero::Detail
