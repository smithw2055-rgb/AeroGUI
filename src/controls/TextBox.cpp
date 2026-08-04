#include "../render/DisplayList.hpp"
#include <Aero/Controls/Text.hpp>
#include "../text/EditableText.hpp"
#include "../render/RenderPrivate.hpp"
#include "../media/MediaPrivate.hpp"

#include "TextBlockLayout.hpp"

#include "gui/GuiPrivate.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls::Detail {

class TextDisplayPolicy {
public:
    virtual ~TextDisplayPolicy() = default;
    virtual Base::Result<void> BuildDisplayText(
        const ::Aero::Text::Detail::EditableTextModel& model,
        Base::String& output) noexcept = 0;
    virtual bool AllowsCopy() const noexcept = 0;
    virtual bool AllowsCut() const noexcept = 0;
};

class PlainTextDisplayPolicy : public TextDisplayPolicy {
public:
    Base::Result<void> BuildDisplayText(
        const ::Aero::Text::Detail::EditableTextModel& model,
        Base::String& output) noexcept override {
        return model.Snapshot(output);
    }
    bool AllowsCopy() const noexcept override { return true; }
    bool AllowsCut() const noexcept override { return true; }
};

class PasswordTextDisplayPolicy : public TextDisplayPolicy {
public:
    explicit PasswordTextDisplayPolicy(
        Base::IAllocator* allocator = nullptr) noexcept
        : mask_(allocator) {
        static_cast<void>(mask_.Assign(
            Base::StringView(u8"\u2022")));
    }

    Base::Result<void> SetMask(Base::StringView value) noexcept {
        ::Aero::Text::Detail::EditableTextModel validation;
        Base::Result<void> assigned = validation.SetText(value);
        if (!assigned || validation.GraphemeCount() != 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Password mask must be one grapheme cluster");
        }
        return mask_.Assign(value);
    }

    Base::StringView GetMask() const noexcept { return mask_.View(); }

    Base::Result<void> BuildDisplayText(
        const ::Aero::Text::Detail::EditableTextModel& model,
        Base::String& output) noexcept override {
        output.Clear();
        const std::uint32_t count = model.GraphemeCount();
        if (count != 0U && mask_.SizeBytes() > UINT32_MAX / count) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Password display text exceeds capacity");
        }
        Base::Result<void> reserved =
            output.Reserve(mask_.SizeBytes() * count);
        if (!reserved) return reserved;
        Base::String source;
        Base::Result<void> snapshot = model.Snapshot(source);
        if (!snapshot) return snapshot;
        for (std::uint32_t index = 0U; index < count; ++index) {
            Base::Result<std::uint32_t> begin =
                model.ByteOffsetForGrapheme(index);
            if (!begin) return begin.GetStatus();
            Base::Result<std::uint32_t> end =
                model.ByteOffsetForGrapheme(index + 1U);
            if (!end) return end.GetStatus();
            const Base::StringView cluster = source.View().Substr(
                begin.Value(), end.Value() - begin.Value());
            const bool newline = !cluster.Empty() &&
                (cluster[0] == '\r' || cluster[0] == '\n');
            Base::Result<void> appended = output.Append(
                newline ? cluster : mask_.View());
            if (!appended) return appended;
        }
        return {};
    }

    bool AllowsCopy() const noexcept override { return false; }
    bool AllowsCut() const noexcept override { return false; }

private:
    Base::String mask_;
};

} // namespace Aero::Controls::Detail

namespace Aero::Controls {

using namespace Primitives;
using namespace ::Aero::Render;
} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using ::Aero::Controls::Detail::TextDisplayPolicy;
using ::Aero::Controls::Detail::PlainTextDisplayPolicy;
using ::Aero::Controls::Detail::PasswordTextDisplayPolicy;
using ::Aero::Controls::Detail::TextLayoutRequest;
using ::Aero::Controls::Detail::TextLayoutResult;
} // namespace Aero::Controls::Detail

namespace Aero::Controls {
using namespace Primitives;
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

::Aero::Text::Detail::EditableTextModel& Model(
    void* value) noexcept {
    return *static_cast<::Aero::Text::Detail::EditableTextModel*>(value);
}

const ::Aero::Text::Detail::EditableTextModel& Model(
    const void* value) noexcept {
    return *static_cast<const ::Aero::Text::Detail::EditableTextModel*>(value);
}

::Aero::Controls::Detail::TextDisplayPolicy* DisplayPolicy(
    void* value) noexcept {
    return static_cast<::Aero::Controls::Detail::TextDisplayPolicy*>(value);
}

::Aero::Controls::Detail::PasswordTextDisplayPolicy* PasswordPolicy(
    void* value) noexcept {
    return static_cast<::Aero::Controls::Detail::PasswordTextDisplayPolicy*>(value);
}

::Aero::Controls::Detail::TextBlockLayout* LayoutService(
    const Visual& visual) noexcept {
    return static_cast<::Aero::Controls::Detail::TextBlockLayout*>(
        ::Aero::Visual::Impl::TextLayoutRuntime(visual));
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
      model_(new (std::nothrow) ::Aero::Text::Detail::EditableTextModel()),
      compositionModel_(new (std::nothrow) ::Aero::Text::Detail::EditableTextModel()),
      displayPolicy_(nullptr),
      plainPolicy_(new (std::nothrow) Detail::PlainTextDisplayPolicy()),
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
    delete static_cast<::Aero::Text::Detail::EditableTextModel*>(model_);
    model_ = nullptr;
    delete static_cast<::Aero::Text::Detail::EditableTextModel*>(compositionModel_);
    compositionModel_ = nullptr;
    delete static_cast<::Aero::Controls::Detail::PlainTextDisplayPolicy*>(plainPolicy_);
    plainPolicy_ = nullptr;
    displayPolicy_ = nullptr;
}

PasswordBox::PasswordBox() noexcept
    : TextBoxBase(StaticTypeId()),
      passwordPolicy_(new (std::nothrow) Detail::PasswordTextDisplayPolicy()),
      validation_(new (std::nothrow) ::Aero::Text::Detail::EditableTextModel()) {
    editor_.displayPolicy_ =
        passwordPolicy_;
    editor_.coordinateOwner_ = this;
    editor_.passwordOwner_ = this;
}

PasswordBox::~PasswordBox() {
    delete static_cast<::Aero::Text::Detail::EditableTextModel*>(validation_);
    validation_ = nullptr;
    delete static_cast<::Aero::Controls::Detail::PasswordTextDisplayPolicy*>(passwordPolicy_);
    passwordPolicy_ = nullptr;
}

void PasswordBox::SetPassword(
    Base::StringView value) noexcept {
    if (password_.View() == value) return;
    ::Aero::Text::Detail::EditableTextModel next;
    Base::Result<void> limited =
        next.SetMaximumLength(
            EffectiveMaximumLength(
                GetMaxLength()));
    if (limited) limited = next.SetText(value);
    if (!limited) {
        return;
    }
    Base::String nextPassword;
    Base::Result<void> copied =
        nextPassword.Assign(value);
    if (!copied) return;
    Base::Result<void> modelLimit =
        Model(validation_).SetMaximumLength(
            EffectiveMaximumLength(
                GetMaxLength()));
    if (modelLimit) {
        modelLimit = Model(validation_).SetText(value);
    }
    if (!modelLimit) return;
    synchronizingEditor_ = true;
    editor_.SetText(value);
    synchronizingEditor_ = false;
    password_ = std::move(nextPassword);
    (void)InvalidateMeasure();
    (void)InvalidateVisual();
    RoutedEventArgs args;
    RaiseEvent(PasswordChangedEvent, &args);
}

Base::StringView PasswordBox::GetPasswordChar() const noexcept {
    return GetValueOr(
        PasswordCharProperty,
        Base::StringView(u8"\u2022"));
}

void PasswordBox::SetPasswordChar(
    Base::StringView value) noexcept {
    Detail::PasswordTextDisplayPolicy validation;
    Base::Result<void> valid =
        validation.SetMask(value);
    if (!valid) return;
    if (passwordPolicy_ == nullptr) {
        return;
    }
    Base::Result<void> mask =
        PasswordPolicy(passwordPolicy_)->SetMask(value);
    if (!mask) return;
    SetValue(PasswordCharProperty, value);
    (void)editor_.InvalidateMeasure();
    (void)InvalidateMeasure();
    (void)InvalidateVisual();
}

std::uint32_t PasswordBox::GetMaxLength() const noexcept {
    return GetValueOr(
        MaxLengthProperty,
        0U);
}

void PasswordBox::SetMaxLength(
    std::uint32_t value) noexcept {
    const std::uint32_t effective =
        EffectiveMaximumLength(value);
    if (Model(validation_).GraphemeCount() > effective) {
        return;
    }
    Base::Result<void> validation =
        Model(validation_).SetMaximumLength(effective);
    if (!validation) return;
    editor_.SetMaxLength(value);
    SetValue(MaxLengthProperty, value);
}

void PasswordBox::SetSelectionBrush(
    Base::Ref<Media::Brush> value) noexcept {
    TextBoxBase::SetSelectionBrush(std::move(value));
    (void)editor_.SetSelectionBrush(GetSelectionBrush());
}

void PasswordBox::SetSelectionOpacity(
    double value) noexcept {
    TextBoxBase::SetSelectionOpacity(value);
    (void)editor_.SetSelectionOpacity(value);
}

void PasswordBox::SetCaretBrush(
    Base::Ref<Media::Brush> value) noexcept {
    TextBoxBase::SetCaretBrush(std::move(value));
    (void)editor_.SetCaretBrush(GetCaretBrush());
}

TextSelection
PasswordBox::GetSelection() const noexcept {
    return editor_.GetSelection();
}

std::uint32_t PasswordBox::GetCaret() const noexcept {
    return editor_.GetCaret();
}

void PasswordBox::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    (void)editor_.SetSelection(anchor, caret);
}

Base::Result<void> PasswordBox::SelectAll() noexcept {
    return editor_.SelectAll();
}

void PasswordBox::SetInputMethodHost(
    Integration::ITextInputMethodHost* host) noexcept {
    (void)editor_.SetInputMethodHost(host);
}

Integration::ITextInputMethodHost*
PasswordBox::GetInputMethodHost() const noexcept {
    return editor_.GetInputMethodHost();
}

bool PasswordBox::GetIsComposing() const noexcept {
    return editor_.GetIsComposing();
}

Size PasswordBox::MeasureOverride(
    Size availableSize) noexcept {
    Size templateSize{};
    if (GetTemplateRoot() != nullptr) {
        templateSize = Control::MeasureOverride(availableSize);
    }
    editor_.SetUseLayoutRounding(GetUseLayoutRounding(), GetDpiScale());
    editor_.SetForeground(GetForeground());
    editor_.SetSelectionBrush(GetSelectionBrush());
    editor_.SetSelectionOpacity(GetSelectionOpacity());
    editor_.SetCaretBrush(GetCaretBrush());
    Base::Result<Size> measuredEditor =
        editor_.MeasureOverride(availableSize);
    if (!measuredEditor) {
        return Size{};
    }
    const Size editorSize = measuredEditor.Value();
    return Size{
        std::max(templateSize.width, editorSize.width),
        std::max(templateSize.height, editorSize.height)};
}

Size PasswordBox::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        Control::ArrangeOverride(finalSize);
    }
    editor_.SetViewport(finalSize);
    return finalSize;
}

void PasswordBox::OnRender(
    DrawingContext& context) noexcept {
    static_cast<void>(editor_.RenderEditor(
        context,
        GetRenderSize(),
        GetIsKeyboardFocused()));
}

Base::Result<void>
PasswordBox::SynchronizeEditorFromPassword()
    noexcept {
    if (synchronizingEditor_) {
        return {};
    }
    synchronizingEditor_ = true;
    editor_.SetMaxLength(GetMaxLength());
    editor_.SetText(password_.View());
    synchronizingEditor_ = false;
    return {};
}

Base::Result<void>
PasswordBox::SynchronizePasswordFromEditor()
    noexcept {
    if (synchronizingEditor_ ||
        password_.View() == editor_.GetText()) {
        return {};
    }
    Base::String next;
    Base::Result<void> copied =
        next.Assign(editor_.GetText());
    if (!copied) return copied.GetStatus();
    Base::Result<void> model =
        Model(validation_).SetMaximumLength(
            EffectiveMaximumLength(
                GetMaxLength()));
    if (model) {
        model = Model(validation_).SetText(
            next.View());
    }
    if (!model) return model.GetStatus();
    password_ = std::move(next);
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    Base::Result<void> render =
        InvalidateVisual();
    if (!render) return render.GetStatus();
    RoutedEventArgs args;
    RaiseEvent(PasswordChangedEvent, &args);
    return {};
}

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
    ::Aero::Text::Detail::EditableTextModel validation;
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
        TextAlignment::Start);
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
    Integration::ITextInputMethodHost* host) noexcept {
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
    ::Aero::Text::Detail::EditableTextModel inserted;
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
    Integration::IClipboard& clipboard) const noexcept {
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
    Integration::IClipboard& clipboard) noexcept {
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
    Integration::IClipboard& clipboard) noexcept {
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

Base::Result<void> TextBox::MoveCaretHorizontal(
    double direction,
    bool extend) noexcept {
    const TextSelection old =
        Model(model_).Selection();
    std::uint32_t next = old.caret;
    if (!extend && !old.GetIsEmpty()) {
        next = direction < 0.0
            ? old.GetStart() : old.GetEnd();
    } else if (direction < 0.0) {
        if (next != 0U) {
            --next;
        }
    } else if (next < Model(model_).GraphemeCount()) {
        ++next;
    }
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

Base::Result<void>
TextBox::MoveCaretLineBoundary(
    bool end,
    bool extend) noexcept {
    const TextSelection old =
        Model(model_).Selection();
    std::uint32_t lineIndex = 0U;
    for (std::uint32_t line = 0U;
         line < Model(model_).LineCount(); ++line) {
        Base::Result<TextRange> range =
            Model(model_).LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t nextStart =
            line + 1U < Model(model_).LineCount()
            ? range.Value().GetEnd() + 1U
            : Model(model_).GraphemeCount() + 1U;
        if (old.caret < nextStart) {
            lineIndex = line;
            break;
        }
    }
    Base::Result<TextRange> range =
        Model(model_).LineRange(lineIndex);
    if (!range) {
        return range.GetStatus();
    }
    const std::uint32_t next = end
        ? range.Value().GetEnd()
        : range.Value().start;
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

double TextBox::GetLineHeight() const noexcept {
    if (!caretStops_.Empty() &&
        caretStops_[0].height > 0.0) {
        return caretStops_[0].height;
    }
    return GetFontSize() * 1.6 /
        std::max(1.0, GetDpiScale());
}

Rect TextBox::GetCaretRectangle() const noexcept {
    if (caretStops_.Empty()) {
        return {
            -scroll_.horizontalOffset,
            -scroll_.verticalOffset,
            CaretWidth / std::max(1.0, GetDpiScale()),
            GetLineHeight()};
    }
    const std::uint32_t index =
        std::min(
            Model(GetActiveModel()).Caret(),
            caretStops_.Size() - 1U);
    const CaretStop& stop =
        caretStops_[index];
    return {
        stop.x - scroll_.horizontalOffset,
        stop.y - scroll_.verticalOffset,
        CaretWidth / std::max(1.0, GetDpiScale()),
        stop.height};
}

std::uint32_t TextBox::HitTestText(
    Point position) const noexcept {
    if (caretStops_.Empty()) {
        return 0U;
    }
    const double x =
        position.x -
            GetPadding().left +
            scroll_.horizontalOffset;
    const double y =
        position.y -
            GetPadding().top +
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
    const auto& active = Model(GetActiveModel());
    const std::uint32_t graphemes =
        active.GraphemeCount();
    Base::Result<void> capacity =
        caretStops_.Reserve(
            graphemes + 1U);
    if (!capacity) {
        return capacity;
    }
    const std::uint32_t lines =
        std::max(1U, active.LineCount());
    std::uint32_t maximumLineLength = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
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
        Base::Result<TextRange> range =
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
        : GetFontSize() * 1.6 /
            std::max(1.0, GetDpiScale());
    const double advance =
        wrapColumns_ != UINT32_MAX
        ? DefaultAdvance *
              GetFontSize() / 16.0 /
              std::max(1.0, GetDpiScale())
        : maximumLineLength != 0U &&
            textSize_.width > 0.0
        ? textSize_.width /
            static_cast<double>(
                maximumLineLength)
        : DefaultAdvance *
              GetFontSize() / 16.0 /
            std::max(1.0, GetDpiScale());

    Base::Result<void> initial =
        caretStops_.Resize(
            graphemes + 1U);
    if (!initial) {
        return initial;
    }
    std::uint32_t visualLineBase = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t start =
            range.Value().start;
        const std::uint32_t end =
            range.Value().GetEnd();
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

Size TextBox::MeasureOverride(
    Size availableSize) noexcept {
    Base::Result<void> display =
        DisplayPolicy(displayPolicy_)->BuildDisplayText(
            Model(GetActiveModel()), displayText_);
    if (!display) {
        return Size{};
    }
    ReleaseGlyphRuns();
    textSize_ = {};
    wrapColumns_ = UINT32_MAX;
    showingPlaceholder_ =
        displayText_.Empty() &&
        !GetPlaceholder().Empty();
    if (showingPlaceholder_) {
        display = displayText_.Assign(
            GetPlaceholder());
        if (!display) {
            return Size{};
        }
    }
    const Thickness padding = GetPadding();
    const Size contentAvailable =
        Deflate(availableSize, padding);
    if (GetTextWrapping() !=
            TextWrapping::NoWrap &&
        contentAvailable.width > 0.0) {
        const double fallbackAdvance =
            DefaultAdvance *
            GetFontSize() / 16.0 /
            std::max(1.0, GetDpiScale());
        const double columns =
            std::floor(
                contentAvailable.width /
                fallbackAdvance);
        wrapColumns_ = static_cast<std::uint32_t>(
            std::min(
                static_cast<double>(UINT32_MAX),
                std::max(1.0, columns)));
    }
    auto* layoutService = LayoutService(*this);
    if (layoutService != nullptr &&
        !displayText_.Empty()) {
        Detail::TextLayoutRequest request;
        request.text = displayText_.View();
        request.availableSize =
            contentAvailable;
        request.dpiScale = GetDpiScale();
        request.pixelSize =
            static_cast<float>(GetFontSize());
        request.lineHeight =
            static_cast<float>(
                GetFontSize() * 1.6);
        const Base::Ref<Media::FontFamily> configuredFamily =
            GetFontFamily();
        Base::StringView family = configuredFamily
            ? configuredFamily->GetSource()
            : Base::StringView{};
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
                    FontStyle::Normal;
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
        request.wrapping = GetTextWrapping();
        request.alignment = GetTextAlignment();
        request.direction = GetFlowDirection() == FlowDirection::RightToLeft
            ? Text::TextDirection::RightToLeft
            : Text::TextDirection::LeftToRight;
        Detail::TextLayoutResult result;
        Base::Result<void> prepared =
            layoutService->ShapeAndPrepare(
                request, result);
        if (!prepared) {
            return Size{};
        }
        for (RenderGlyphRunId glyph :
             result.glyphRuns) {
            if (glyph ==
                InvalidRenderGlyphRunId) {
                for (RenderGlyphRunId release :
                     result.glyphRuns) {
                    if (release !=
                        InvalidRenderGlyphRunId) {
                        layoutService->
                            ReleaseGlyphRun(release);
                    }
                }
                return Size{};
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
        const auto& active = Model(GetActiveModel());
        for (std::uint32_t line = 0U;
             line < active.LineCount();
             ++line) {
            Base::Result<TextRange> range =
                active.LineRange(line);
            if (!range) {
                return Size{};
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
                GetFontSize() / 16.0 /
                std::max(1.0, GetDpiScale()),
            static_cast<double>(
                std::max(1U, visualLineCount)) *
                GetFontSize() * 1.6 /
                std::max(1.0, GetDpiScale())};
    }
    Base::Result<void> stops =
        RebuildCaretStops();
    if (!stops) {
        return Size{};
    }
    scroll_.extentWidth = textSize_.width;
    scroll_.extentHeight =
        std::max(textSize_.height, GetLineHeight());
    scroll_.horizontalOffset = ClampOffset(
        scroll_.horizontalOffset,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    scroll_.verticalOffset = ClampOffset(
        scroll_.verticalOffset,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    if (GetIsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return Size{};
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    const double minimumWidth =
        DefaultAdvance *
        GetFontSize() / 16.0 /
        std::max(1.0, GetDpiScale());
    Size desired{
        std::min(
            std::max(minimumWidth, textSize_.width),
            contentAvailable.width),
        std::min(
            std::max(GetLineHeight(), textSize_.height),
            contentAvailable.height)};
    const std::uint32_t maximumLines =
        GetMaxLines();
    const std::uint32_t minimumLines =
        GetMinLines();
    if (maximumLines != 0U) {
        const double lineBoxHeight =
            std::max(
                GetLineHeight(),
                GetFontSize() * 1.6);
        const double maximumHeight =
            lineBoxHeight *
            static_cast<double>(
                maximumLines);
        if (GetTextWrapping() !=
                TextWrapping::NoWrap &&
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
                GetLineHeight(),
                GetFontSize() * 1.6) *
                static_cast<double>(
                    minimumLines)));
    return Inflate(desired, padding);
}

Size TextBox::ArrangeOverride(
    Size finalSize) noexcept {
    const Size contentViewport =
        Deflate(finalSize, GetPadding());
    SetViewport(contentViewport);
    if (GetIsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return finalSize;
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    return finalSize;
}

void TextBox::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
    const Rect bounds{
        0.0, 0.0,
        GetRenderSize().width,
        GetRenderSize().height};
    const Thickness border =
        GetBorderThickness();
    const double borderThickness = std::max(
        std::max(border.left, border.right),
        std::max(border.top, border.bottom));
    Color borderBrush =
        ::Aero::Media::Detail::SampleBrush(GetBorderBrush());
    if (GetIsKeyboardFocused() && GetIsEnabled()) {
        borderBrush = Color{
            11.0F / 255.0F,
            128.0F / 255.0F,
            193.0F / 255.0F,
            1.0F};
    } else if (GetIsMouseOver() && GetIsEnabled()) {
        borderBrush = Color{
            93.0F / 255.0F,
            100.0F / 255.0F,
            105.0F / 255.0F,
            1.0F};
    }
    Base::Result<void> chrome =
        builder.FillRoundedRect(
            bounds,
            ::Aero::Media::Detail::SampleBrush(GetBackground()),
            1.75);
    if (!chrome) {
        return;
    }
    if (borderThickness > 0.0 &&
        borderBrush.alpha > 0.0F) {
        chrome = builder.StrokeRect(
            bounds,
            borderBrush,
            GetIsKeyboardFocused()
                ? std::max(1.0, borderThickness)
                : borderThickness);
        if (!chrome) {
            return;
        }
    }
    static_cast<void>(RenderEditor(
        context,
        GetRenderSize(),
        GetIsKeyboardFocused()));
}

Base::Result<void>
TextBox::RenderEditor(
    DrawingContext& context,
    Size viewport,
    bool drawCaret) noexcept {
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
    const Thickness padding = GetPadding();
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
    const TextSelection selection =
        Model(GetActiveModel()).Selection();
    if (!selection.GetIsEmpty() &&
        !caretStops_.Empty()) {
        const std::uint32_t begin =
            selection.GetStart();
        const std::uint32_t end =
            selection.GetEnd();
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
                ::Aero::Media::Detail::SampleBrush(
                    GetSelectionBrush(),
                    0.5,
                    Color{
                        46.0F / 255.0F,
                        174.0F / 255.0F,
                        235.0F / 255.0F,
                        1.0F});
            selectionColor.alpha *=
                static_cast<float>(
                    GetSelectionOpacity());
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
                    ? ::Aero::Media::Detail::SampleBrush(
                        GetPlaceholderForeground(),
                        0.5,
                        Color{
                            123.0F / 255.0F,
                            128.0F / 255.0F,
                            133.0F / 255.0F,
                            1.0F})
                    : ::Aero::Media::Detail::SampleBrush(
                        GetForeground(),
                        0.5,
                        Color{
                            0.0F, 0.0F, 0.0F, 1.0F}));
        if (!drawn) {
            return drawn;
        }
    }
    if (drawCaret &&
        !showingPlaceholder_) {
        Rect caret = GetCaretRectangle();
        caret.x += scroll_.horizontalOffset;
        caret.y += scroll_.verticalOffset;
        Base::Result<void> drawn =
            builder.FillRect(
                caret,
                ::Aero::Media::Detail::SampleBrush(
                    GetCaretBrush(),
                    0.5,
                    Color{
                        0.0F, 0.0F, 0.0F, 1.0F}));
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
    auto* layoutService = LayoutService(*this);
    if (serviceOwnsGlyphRuns_ && layoutService != nullptr) {
        for (RenderGlyphRunId glyph :
             glyphRuns_) {
            layoutService->ReleaseGlyphRun(glyph);
        }
    }
    glyphRuns_.Clear();
    serviceOwnsGlyphRuns_ = false;
}

void TextBox::SetViewport(
    Size viewport) noexcept {
    if (!IsValidLayoutSize(viewport)) {
        return;
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
        (void)InvalidateVisual();
    }
}

void TextBox::SetHorizontalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = ClampOffset(
        value,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    if (next == scroll_.horizontalOffset) {
        return;
    }
    scroll_.horizontalOffset = next;
    (void)InvalidateVisual();
}

void TextBox::SetVerticalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = ClampOffset(
        value,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    if (next == scroll_.verticalOffset) {
        return;
    }
    scroll_.verticalOffset = next;
    (void)InvalidateVisual();
}

Base::Result<bool> TextBox::LineHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * ScrollLine);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBox::LineVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * GetLineHeight());
    return old != scroll_.verticalOffset;
}

Base::Result<bool> TextBox::PageHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * scroll_.viewportWidth);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBox::PageVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * scroll_.viewportHeight);
    return old != scroll_.verticalOffset;
}

Base::Result<void>
TextBox::EnsureCaretVisible() noexcept {
    const Rect caret = GetCaretRectangle();
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
    SetHorizontalOffset(horizontal);
    SetVerticalOffset(vertical);
    return {};
}

Base::Result<void>
TextBox::UpdateCandidateWindow() noexcept {
    if (inputMethodHost_ == nullptr ||
        !compositionActive_) {
        return {};
    }
    Integration::ImeCandidateWindow candidate;
    Rect caret = GetCaretRectangle();
    caret.x += GetPadding().left;
    caret.y += GetPadding().top;
    UIElement& owner =
        coordinateOwner_ != nullptr
        ? *coordinateOwner_
        : static_cast<UIElement&>(*this);
    candidate.caret =
        ToRootRect(owner, caret);
    candidate.dpiScale = GetDpiScale();
    inputMethodHost_->SetCandidateWindow(candidate);
    return {};
}

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

TextBox::Impl::
Impl(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input,
    Integration::IClipboard& clipboard) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      clipboard_(&clipboard),
      mouseDownHandler_(
          this,
          &TextBox::Impl::
              OnMouseDown),
      mouseMoveHandler_(
          this,
          &TextBox::Impl::
              OnMouseMove),
      mouseUpHandler_(
          this,
          &TextBox::Impl::
              OnMouseUp),
      keyDownHandler_(
          this,
          &TextBox::Impl::
              OnKeyDown),
      textInputHandler_(
          this,
          &TextBox::Impl::
              OnTextInput),
      focusChangedHandler_(
          this,
          &TextBox::Impl::
              OnFocusChanged),
      propertyChangedHandler_(
          this,
          &TextBox::Impl::
              OnPropertyChanged),
      captureChangedHandler_(
          this,
          &TextBox::Impl::
              OnCaptureChanged) {}

TextBox::Impl::
~Impl() noexcept {
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

std::uint32_t TextBox::Impl::Find(
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
TextBox::Impl::ResolveOwner(
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
TextBox::Impl::ResolveEditor(
    std::uint32_t index) noexcept {
    UIElement* owner = ResolveOwner(index);
    if (owner == nullptr) return nullptr;
    return records_[index].password
        ? &static_cast<PasswordBox*>(
              owner)->editor_
        : static_cast<TextBox*>(owner);
}

void TextBox::Impl::RemoveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != records_.Size()) {
        records_[index] = std::move(
            records_[records_.Size() - 1U]);
    }
    records_.PopBack();
}

Base::Result<void>
TextBox::Impl::Attach(
    TextBox& textBox) noexcept {
    if (Find(textBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "TextBox is already attached");
    }
    if (!textBox.GetIsLoaded() ||
        Aero::GuiPrivate::Detail::ElementPrivate::Tree(textBox) != tree_) {
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
    record.handle = Aero::GuiPrivate::Detail::ElementPrivate::Handle(textBox);
    Base::Result<void> appended =
        records_.PushBack(record);
    if (!appended) {
        return appended;
    }
    if (!captureSubscribed_) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
        captureSubscribed_ = true;
    }
    Base::Result<void> result =
        textBox.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::TextInputEvent,
            textInputHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::LostKeyboardFocusEvent,
            focusChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
                TextBox::TextProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
                TextBox::IsReadOnlyProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
                TextBox::MaxLengthProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
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
TextBox::Impl::Attach(
    PasswordBox& passwordBox) noexcept {
    if (Find(passwordBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "PasswordBox is already attached");
    }
    if (!passwordBox.GetIsLoaded() ||
        Aero::GuiPrivate::Detail::ElementPrivate::Tree(passwordBox) != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "PasswordBox must be loaded in the interaction tree");
    }
    Base::Result<void> synced =
        PasswordPolicy(passwordBox.passwordPolicy_)->SetMask(
            passwordBox.GetPasswordChar());
    if (synced) {
        synced =
            passwordBox.
                SynchronizeEditorFromPassword();
    }
    if (synced) {
        passwordBox.editor_.SetForeground(passwordBox.GetForeground());
        passwordBox.editor_.SetSelectionBrush(passwordBox.GetSelectionBrush());
        passwordBox.editor_.SetSelectionOpacity(passwordBox.GetSelectionOpacity());
        passwordBox.editor_.SetCaretBrush(passwordBox.GetCaretBrush());
    }
    if (!synced) return synced.GetStatus();

    Record record;
    record.handle = Aero::GuiPrivate::Detail::ElementPrivate::Handle(passwordBox);
    record.password = true;
    Base::Result<void> appended =
        records_.PushBack(record);
    if (!appended) return appended.GetStatus();
    if (!captureSubscribed_) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
        captureSubscribed_ = true;
    }

    Base::Result<void> result =
        passwordBox.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::TextInputEvent,
            textInputHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::LostKeyboardFocusEvent,
            focusChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        PasswordCharProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        MaxLengthProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        ForegroundProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        SelectionBrushProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        CaretBrushProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
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
TextBox::Impl::Detach(
    TextBox& textBox) noexcept {
    const std::uint32_t index = Find(textBox);
    if (index == UINT32_MAX) {
        return false;
    }
    Record& record = records_[index];
    if (record.dragging) {
        Base::Result<bool> released =
            input_->ReleasePointer(
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
            TextBox::MaxLengthProperty,
            propertyChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            UIElement::IsEnabledProperty,
            propertyChangedHandler_));
    RemoveAt(index);
    if (records_.Empty() &&
        captureSubscribed_) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return true;
}

Base::Result<bool>
TextBox::Impl::Detach(
    PasswordBox& passwordBox) noexcept {
    const std::uint32_t index =
        Find(passwordBox);
    if (index == UINT32_MAX) {
        return false;
    }
    Record& record = records_[index];
    if (record.dragging) {
        Base::Result<bool> released =
            input_->ReleasePointer(
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
                    MaxLengthProperty,
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
    passwordBox.SetInputMethodHost(nullptr);
    RemoveAt(index);
    if (records_.Empty() &&
        captureSubscribed_) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                    captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return true;
}

void TextBox::Impl::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    if (args.GetChangedButton() !=
            MouseButton::Left ||
        !owner.GetIsEnabled()) {
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
            owner, args.GetPosition());
    const std::uint32_t caret =
        editor->HitTestText(local);
    static_cast<void>(
        editor->SetSelection(caret, caret));
    static_cast<void>(
        input_->SetFocus(&owner));
    Base::Result<void> captured =
        input_->CapturePointer(
            args.GetPointerId(), owner);
    if (captured) {
        records_[index].pointerId =
            args.GetPointerId();
        records_[index].anchor = caret;
        records_[index].dragging = true;
    }
    args.SetHandled(true);
}

void TextBox::Impl::OnMouseMove(
    Base::Object* sender,
    MouseEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    if (index == UINT32_MAX ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    TextBox* editor =
        ResolveEditor(index);
    if (editor == nullptr) return;
    const Point local =
        ToLocalPoint(
            owner, args.GetPosition());
    static_cast<void>(
        editor->SetSelection(
            records_[index].anchor,
            editor->HitTestText(local)));
    args.SetHandled(true);
}

void TextBox::Impl::OnMouseUp(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    if (index == UINT32_MAX ||
        args.GetChangedButton() !=
            MouseButton::Left ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    TextBox* editor =
        ResolveEditor(index);
    if (editor == nullptr) return;
    const Point local =
        ToLocalPoint(
            owner, args.GetPosition());
    static_cast<void>(
        editor->SetSelection(
            records_[index].anchor,
            editor->HitTestText(local)));
    records_[index].dragging = false;
    static_cast<void>(
        input_->ReleasePointer(
            args.GetPointerId()));
    args.SetHandled(true);
}

void TextBox::Impl::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (!owner.GetIsEnabled() ||
        editor == nullptr) {
        return;
    }
    const bool password =
        records_[index].password;
    const bool shift =
        HasKeyboardModifier(
            args.GetModifiers(),
            KeyboardModifiers::Shift);
    const bool control =
        HasKeyboardModifier(
            args.GetModifiers(),
            KeyboardModifiers::Control);
    Base::Result<void> result;
    bool handled = true;
    if (control &&
        args.GetKey() == KeyboardKeyA) {
        result = editor->SelectAll();
    } else if (control &&
        args.GetKey() == KeyboardKeyC) {
        result = password
            ? Base::Result<void>{}
            : editor->CopySelection(
                  *clipboard_);
    } else if (control &&
        args.GetKey() == KeyboardKeyX) {
        result = password
            ? editor->ReplaceSelection(
                  Base::StringView{})
            : editor->CutSelection(
                  *clipboard_);
    } else if (control &&
        args.GetKey() == KeyboardKeyV) {
        result = editor->Paste(
            *clipboard_);
    } else if (control &&
        args.GetKey() == KeyboardKeyZ) {
        result = shift
            ? editor->Redo()
            : editor->Undo();
    } else if (control &&
        args.GetKey() == KeyboardKeyY) {
        result = editor->Redo();
    } else if (args.GetKey() ==
        KeyboardKeyLeft) {
        result =
            editor->MoveCaretHorizontal(
                -1.0, shift);
    } else if (args.GetKey() ==
        KeyboardKeyRight) {
        result =
            editor->MoveCaretHorizontal(
                1.0, shift);
    } else if (args.GetKey() ==
        KeyboardKeyHome) {
        result =
            editor->MoveCaretLineBoundary(
                false, shift);
    } else if (args.GetKey() ==
        KeyboardKeyEnd) {
        result =
            editor->MoveCaretLineBoundary(
                true, shift);
    } else if (args.GetKey() ==
        KeyboardKeyBackspace) {
        result =
            editor->DeleteBackward();
    } else if (args.GetKey() ==
        KeyboardKeyDelete) {
        result =
            editor->DeleteForward();
    } else if (args.GetKey() ==
            KeyboardKeyEnter &&
        editor->GetAcceptsReturn()) {
        result = editor->ReplaceSelection(
            Base::StringView("\n"));
    } else {
        handled = false;
    }
    if (handled && result) {
        args.SetHandled(true);
    }
}

void TextBox::Impl::OnTextInput(
    Base::Object* sender,
    TextCompositionEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (!owner.GetIsEnabled() ||
        editor == nullptr ||
        editor->GetIsReadOnly()) {
        return;
    }
    if (editor->GetIsComposing()) {
        Base::Result<void> cancelled =
            editor->
                CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    Base::Result<void> inserted =
        editor->ReplaceSelection(args.GetText());
    if (inserted) {
        args.SetHandled(true);
    }
}

void TextBox::Impl::OnFocusChanged(
    Base::Object* sender,
    KeyboardFocusChangedEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    if (args.GetNewFocus() == &owner) {
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
        input_->ReleasePointer(
            records_[index].pointerId));
}

void TextBox::Impl::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (object.RuntimeType() ==
        PasswordBox::StaticTypeId()) {
        auto& passwordBox =
            static_cast<PasswordBox&>(object);
        if (args.GetProperty() ==
                PasswordBox::
                    PasswordCharProperty) {
            static_cast<void>(
                PasswordPolicy(passwordBox.passwordPolicy_)->
                    SetMask(
                        passwordBox.
                            GetPasswordChar()));
            static_cast<void>(
                passwordBox.editor_.
                    InvalidateMeasure());
            static_cast<void>(
                passwordBox.editor_.
                    InvalidateVisual());
        } else if (args.GetProperty() ==
                PasswordBox::
                    MaxLengthProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    CancelCompositionForFocusLoss());
            static_cast<void>(
                Model(passwordBox.validation_).
                    SetMaximumLength(
                        EffectiveMaximumLength(
                            passwordBox.
                                GetMaxLength())));
            static_cast<void>(
                passwordBox.editor_.
                        SetMaxLength(
                        passwordBox.
                            GetMaxLength()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    ForegroundProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetForeground(
                        passwordBox.
                            GetForeground()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    SelectionBrushProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetSelectionBrush(
                        passwordBox.
                            GetSelectionBrush()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    SelectionOpacityProperty) {
            static_cast<void>(
                passwordBox.editor_.SetSelectionOpacity(
                    passwordBox.GetSelectionOpacity()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    CaretBrushProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetCaretBrush(
                        passwordBox.
                            GetCaretBrush()));
        } else if (args.GetProperty() ==
                       UIElement::
                           IsEnabledProperty &&
                   !args.GetNewValue().
                       AsBoolean()) {
            static_cast<void>(
                passwordBox.editor_.
                    CancelCompositionForFocusLoss());
        }
        return;
    }
    auto& textBox =
        static_cast<TextBox&>(object);
    if (args.GetProperty() ==
            TextBox::TextProperty) {
        if (!textBox.updatingTextProperty_) {
            static_cast<void>(
                textBox.SynchronizeModel());
        }
    } else if (args.GetProperty() ==
            TextBox::IsReadOnlyProperty) {
        if (args.GetNewValue().AsBoolean()) {
            static_cast<void>(
                textBox.
                    CancelCompositionForFocusLoss());
        }
        static_cast<void>(
                Model(textBox.model_).SetReadOnly(
                args.GetNewValue().AsBoolean()));
    } else if (args.GetProperty() ==
            TextBox::MaxLengthProperty) {
        static_cast<void>(
            textBox.
                CancelCompositionForFocusLoss());
    } else if (args.GetProperty() ==
                   UIElement::IsEnabledProperty &&
               !args.GetNewValue().AsBoolean()) {
        static_cast<void>(
            textBox.
                CancelCompositionForFocusLoss());
    }
}

void TextBox::Impl::OnCaptureChanged(
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

} // namespace Aero::Controls
