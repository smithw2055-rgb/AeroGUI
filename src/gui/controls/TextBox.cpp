#include <Aero/Controls/TextBlock.hpp>
#include "gui/core/Describe.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls/TextBox.hpp>
#include <Aero/ClassHandler.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Base/String.hpp>
#include "gui/core/ValueConversion.hpp"
#include "ControlPropertyValidators.hpp"
#include "gui/text/EditableText.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp"
#include <Aero/TextProperties.hpp>
#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include "gui/media/BrushRendering.hpp"
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Controls {
namespace TextBoxSupport {

inline constexpr double DefaultAdvance = 8.0;
inline constexpr double DefaultLineHeight = 18.0;
inline constexpr double CaretWidth = 1.0;
inline constexpr double ScrollLine = 16.0;

inline std::uint32_t EffectiveMaximumLength(
    std::uint32_t value) noexcept {
    return value == 0U ? UINT32_MAX : value;
}

inline double ClampOffset(
    double value,
    double extent,
    double viewport) noexcept {
    const double maximum =
        std::max(0.0, extent - viewport);
    return std::clamp(value, 0.0, maximum);
}

inline double ClampOffset(
    double value,
    double extent,
    double viewport,
    bool enabled) noexcept {
    if (!enabled) return 0.0;
    return ClampOffset(value, extent, viewport);
}

inline Point ToLocalPoint(
    const UIElement& element,
    Point point) noexcept {
    if (ElementTree* tree = ::Aero::VisualTree(element)) {
        if (Media::Visual* root = tree->Root()) {
            if (InputRouter* router = ElementTree::InputOf(element)) {
                Base::Result<Input::HitTestResult> local =
                    router->RootToLocal(*root, const_cast<UIElement&>(element), point);
                if (local) return local.Value().position;
            }
        }
    }
    const UIElement* current = &element;
    while (current != nullptr) {
        const Rect slot = current->GetLayoutSlot();
        point.x -= slot.x;
        point.y -= slot.y;
        current = current->LayoutParent();
    }
    return point;
}

inline Rect ToRootRect(
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

struct DragSelectionState {
    std::uint32_t pointerId = 0U;
    std::uint32_t dragAnchor = 0U;
    bool isDragging = false;
};

} // namespace TextBoxSupport

using TextBoxSupport::DefaultAdvance;
using TextBoxSupport::DefaultLineHeight;
using TextBoxSupport::CaretWidth;
using TextBoxSupport::ScrollLine;
using TextBoxSupport::EffectiveMaximumLength;
using TextBoxSupport::ClampOffset;
using TextBoxSupport::ToLocalPoint;
using TextBoxSupport::ToRootRect;
using TextBoxSupport::DragSelectionState;

class TextDisplayPolicy {
public:
    virtual ~TextDisplayPolicy() = default;
    virtual Base::Result<void> BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept = 0;
    virtual bool AllowsCopy() const noexcept = 0;
    virtual bool AllowsCut() const noexcept = 0;
};

class PlainTextDisplayPolicy : public TextDisplayPolicy {
public:
    Base::Result<void> BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
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
        ::Aero::Text::EditableTextModel validation;
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
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept override {
        output.Clear();
        const std::uint32_t count = model.GraphemeCount();
        if (count != 0U && mask_.SizeBytes() > UINT32_MAX / count) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Password display text exceeds capacity");
        }
        output.Reserve(mask_.SizeBytes() * count);
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
            output.Append(
                newline ? cluster : mask_.View());
        }
        return {};
    }

    bool AllowsCopy() const noexcept override { return false; }
    bool AllowsCut() const noexcept override { return false; }

private:
    Base::String mask_;
};

inline ::Aero::Text::EditableTextModel& Model(
    void* value) noexcept {
    return *static_cast<::Aero::Text::EditableTextModel*>(value);
}

inline const ::Aero::Text::EditableTextModel& Model(
    const void* value) noexcept {
    return *static_cast<const ::Aero::Text::EditableTextModel*>(value);
}

inline TextDisplayPolicy* DisplayPolicy(
    void* value) noexcept {
    return static_cast<TextDisplayPolicy*>(value);
}

inline PasswordTextDisplayPolicy* PasswordPolicy(
    void* value) noexcept {
    return static_cast<PasswordTextDisplayPolicy*>(value);
}

inline TextBlockLayout* LayoutService(
    const ::Aero::Media::Visual& visual) noexcept {
    return static_cast<TextBlockLayout*>(ElementTree::TextLayoutOf(visual));
}

using namespace Primitives;
using namespace ::Aero::Render;

Base::Ref<Media::Brush>
TextBoxBase::GetSelectionBrush() const noexcept {
    return GetValue(SelectionBrushProperty);
}

void TextBoxBase::SetSelectionBrush(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(SelectionBrushProperty, std::move(value));
}

double TextBoxBase::GetSelectionOpacity() const noexcept {
    return GetValue(SelectionOpacityProperty);
}

void TextBoxBase::SetSelectionOpacity(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    SetValue(SelectionOpacityProperty, value);
}

Base::Ref<Media::Brush>
TextBoxBase::GetCaretBrush() const noexcept {
    return GetValue(CaretBrushProperty);
}

void TextBoxBase::SetCaretBrush(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(CaretBrushProperty, std::move(value));
}

TextBoxBase::TextBoxBase(TypeId runtimeType) noexcept
    : Control(runtimeType),
      model_(new (std::nothrow) ::Aero::Text::EditableTextModel()),
      compositionModel_(new (std::nothrow) ::Aero::Text::EditableTextModel()),
      displayPolicy_(nullptr) {}

TextBoxBase::~TextBoxBase() {
    if (inputMethodHost_ != nullptr) {
        static_cast<void>(
            inputMethodHost_->
                SetClient(nullptr));
        inputMethodHost_ = nullptr;
    }
    scrollViewer_ = nullptr;
    ReleaseGlyphRuns();
    delete static_cast<::Aero::Text::EditableTextModel*>(model_);
    model_ = nullptr;
    delete static_cast<::Aero::Text::EditableTextModel*>(compositionModel_);
    compositionModel_ = nullptr;
    displayPolicy_ = nullptr;
}

const void*
TextBoxBase::GetActiveModel() const noexcept {
    return compositionActive_
        ? compositionModel_
        : model_;
}

TextSelection
TextBoxBase::GetSelection() const noexcept {
    return Model(GetActiveModel()).Selection();
}

std::uint32_t TextBoxBase::GetCaret() const noexcept {
    return Model(GetActiveModel()).Caret();
}

TextBox::TextBox() noexcept
    : TextBoxBase(StaticTypeId()),
      plainPolicy_(new (std::nothrow) PlainTextDisplayPolicy()) {
    displayPolicy_ = plainPolicy_;
}

TextBox::~TextBox() {
    delete static_cast<::Aero::Controls::PlainTextDisplayPolicy*>(plainPolicy_);
    plainPolicy_ = nullptr;
}

Base::StringView TextBox::GetText() const noexcept {
    return GetValue(TextProperty);
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
    return GetValue(IsReadOnlyProperty);
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
    return GetValue(MaxLengthProperty);
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
    return GetValue(AcceptsReturnProperty);
}

void TextBox::SetAcceptsReturn(
    bool value) noexcept {
    SetValue(AcceptsReturnProperty, value);
}

TextWrapping
TextBox::GetTextWrapping() const noexcept {
    return GetValue(TextWrappingProperty);
}

void TextBox::SetTextWrapping(
    TextWrapping value) noexcept {
    SetValue(TextWrappingProperty, value);
}

Base::StringView TextBox::GetPlaceholder() const noexcept {
    return GetValue(PlaceholderProperty);
}

void TextBox::SetPlaceholder(
    Base::StringView value) noexcept {
    SetValue(PlaceholderProperty, value);
}

Base::Ref<Media::Brush>
TextBox::GetPlaceholderForeground() const noexcept {
    return GetValue(PlaceholderForegroundProperty);
}

void TextBox::SetPlaceholderForeground(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(PlaceholderForegroundProperty, std::move(value));
}

double TextBox::GetFontSize() const noexcept {
    return GetValue(FontSizeProperty);
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

void TextBox::SetFontFamily(
    Base::StringView value) noexcept {
    FrameworkElement::SetFontFamily(value);
}

FontWeight TextBox::GetFontWeight() const noexcept {
    return GetValue(FontWeightProperty);
}

void TextBox::SetFontWeight(
    FontWeight value) noexcept {
    SetValue(FontWeightProperty, value);
}

FontStyle TextBox::GetFontStyle() const noexcept {
    return GetValue(FontStyleProperty);
}

void TextBox::SetFontStyle(
    FontStyle value) noexcept {
    SetValue(FontStyleProperty, value);
}

TextAlignment
TextBox::GetTextAlignment() const noexcept {
    return GetValue(TextAlignmentProperty);
}

void TextBox::SetTextAlignment(
    TextAlignment value) noexcept {
    SetValue(TextAlignmentProperty, value);
}

std::uint32_t TextBox::GetMaxLines() const noexcept {
    return GetValue(MaxLinesProperty);
}

void TextBox::SetMaxLines(
    std::uint32_t value) noexcept {
    SetValue(MaxLinesProperty, value);
}

std::uint32_t TextBox::GetMinLines() const noexcept {
    return GetValue(MinLinesProperty);
}

void TextBox::SetMinLines(
    std::uint32_t value) noexcept {
    SetValue(MinLinesProperty, value);
}

void TextBoxBase::SetSelection(
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
    EnsureCaretVisible();
    InvalidateEditor();
}

void TextBoxBase::SelectAll() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        AERO_ASSERT(cancelled);
        (void)cancelled;
    }
    Base::Result<void> selected =
        Model(model_).SelectAll();
    AERO_ASSERT(selected);
    (void)selected;
    EnsureCaretVisible();
    InvalidateEditor();
}

void TextBoxBase::Undo() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        AERO_ASSERT(cancelled);
        (void)cancelled;
    }
    Base::Result<void> undone = Model(model_).Undo();
    AERO_ASSERT(undone);
    (void)undone;
    Base::Result<void> committed = CommitModelText();
    AERO_ASSERT(committed);
    (void)committed;
}

void TextBoxBase::Redo() noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        AERO_ASSERT(cancelled);
        (void)cancelled;
    }
    Base::Result<void> redone = Model(model_).Redo();
    AERO_ASSERT(redone);
    (void)redone;
    Base::Result<void> committed = CommitModelText();
    AERO_ASSERT(committed);
    (void)committed;
}

Base::Result<void> TextBoxBase::AttachScrollViewer(
    ScrollViewer* viewer) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (viewer == scrollViewer_) {
        return {};
    }
    if (scrollViewer_ != nullptr) {
        if (scrollViewer_->GetContentScrollInfo() == this) {
            scrollViewer_->SetContentScrollInfo(nullptr);
        }
        scrollViewer_->SetContent(nullptr);
    }
    scrollViewer_ = viewer;
    if (viewer == nullptr) {
        return {};
    }
    if (!view_) {
        view_ = Base::MakeRef<Primitives::TextBoxView>(this).Value();
    }
    viewer->SetContentScrollInfo(this);
    viewer->SetCanContentScroll(true);
    viewer->SetContent(view_.Get());
    return {};
}

void TextBoxBase::InvalidateEditor() noexcept {
    InvalidateVisual();
    if (view_) {
        view_->InvalidateVisual();
    }
}

void TextBoxBase::SetInputMethodHost(
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
TextBoxBase::BeginComposition() noexcept {
    if (compositionActive_) {
        return {};
    }
    if (GetIsReadOnlyCore() || !GetIsEnabled()) {
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
    InvalidateMeasure();
    InvalidateVisual();
    UpdateCandidateWindow();
    return {};
}

Base::Result<void> TextBoxBase::UpdateComposition(
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
    InvalidateMeasure();
    InvalidateVisual();
    UpdateCandidateWindow();
    return {};
}

Base::Result<void> TextBoxBase::CommitComposition(
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
    UpdateCandidateWindow();
    return {};
}

Base::Result<void>
TextBoxBase::CancelComposition() noexcept {
    if (!compositionActive_) {
        return {};
    }
    compositionActive_ = false;
    compositionText_.Clear();
    InvalidateMeasure();
    InvalidateVisual();
    EnsureCaretVisible();
    return {};
}

Base::Result<void>
TextBoxBase::CancelCompositionForFocusLoss() noexcept {
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
    InvalidateMeasure();
    InvalidateVisual();
    return {};
}

Base::Result<void> TextBoxBase::CommitModelText() noexcept {
    InvalidateMeasure();
    EnsureCaretVisible();
    InvalidateEditor();
    if (view_) {
        view_->InvalidateMeasure();
    }
    return {};
}

Base::Result<void> TextBox::CommitModelText() noexcept {
    Base::String snapshot;
    if (model_ != nullptr) {
        Base::Result<void> copied =
            Model(model_).Snapshot(snapshot);
        if (!copied) {
            return copied;
        }
    }
    updatingTextProperty_ = true;
    SetCurrentValue(TextProperty, snapshot);
    updatingTextProperty_ = false;
    return TextBoxBase::CommitModelText();
}

Base::Result<void> TextBoxBase::SanitizeInput(
    Base::StringView input,
    Base::String& output) const noexcept {
    if (GetAcceptsReturnCore()) {
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

Base::Result<void> TextBoxBase::ConstrainManualInput(
    Base::String& input,
    const void* target,
    TextSelection selection) const noexcept {
    const auto& model = Model(target);
    const std::uint32_t maximum =
        GetMaxLengthCore();
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

Base::Result<void> TextBoxBase::ReplaceSelection(
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

Base::Result<void> TextBoxBase::DeleteBackward() noexcept {
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

Base::Result<void> TextBoxBase::DeleteForward() noexcept {
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

Base::Result<void> TextBoxBase::SelectedText(
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

Base::Result<void> TextBoxBase::CopySelection(
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

Base::Result<void> TextBoxBase::CutSelection(
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
    if (GetIsReadOnlyCore()) {
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

Base::Result<void> TextBoxBase::Paste(
    Input::IClipboard& clipboard) noexcept {
    if (compositionActive_) {
        Base::Result<void> cancelled =
            CancelCompositionForFocusLoss();
        if (!cancelled) {
            return cancelled;
        }
    }
    if (GetIsReadOnlyCore()) {
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

void TextBoxBase::OnMouseDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !GetIsEnabled()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    const std::uint32_t caret = HitTestText(local);
    static_cast<void>(SetSelection(caret, caret));
    static_cast<void>(Focus());
    Base::Result<void> captured = CapturePointer(args.GetPointerId());
    if (captured) {
        drag_.pointerId = args.GetPointerId();
        drag_.dragAnchor = caret;
        drag_.isDragging = true;
    }
    args.SetHandled(true);
}

void TextBoxBase::OnMouseMove(MouseEventArgs& args) {
    if (!drag_.isDragging || drag_.pointerId != args.GetPointerId()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    static_cast<void>(SetSelection(drag_.dragAnchor, HitTestText(local)));
    args.SetHandled(true);
}

void TextBoxBase::OnMouseUp(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !drag_.isDragging || drag_.pointerId != args.GetPointerId()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    static_cast<void>(SetSelection(drag_.dragAnchor, HitTestText(local)));
    drag_.isDragging = false;
    static_cast<void>(ReleasePointer(args.GetPointerId()));
    args.SetHandled(true);
}

void TextBoxBase::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) {
        return;
    }
    const bool shift = HasKeyboardModifier(args.GetModifiers(), KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(args.GetModifiers(), KeyboardModifiers::Control);
    Base::Result<void> result;
    bool handled = true;
    if (control && args.GetKey() == KeyboardKeyA) {
        SelectAll();
        result = {};
    } else if (control && args.GetKey() == KeyboardKeyC) {
        Input::IClipboard* clipboard = ElementTree::ClipboardOf(*this);
        if (clipboard != nullptr) {
            result = CopySelection(*clipboard);
        }
    } else if (control && args.GetKey() == KeyboardKeyX) {
        Input::IClipboard* clipboard = ElementTree::ClipboardOf(*this);
        if (clipboard != nullptr) {
            result = CutSelection(*clipboard);
        }
    } else if (control && args.GetKey() == KeyboardKeyV) {
        Input::IClipboard* clipboard = ElementTree::ClipboardOf(*this);
        if (clipboard != nullptr) {
            result = Paste(*clipboard);
        }
    } else if (control && args.GetKey() == KeyboardKeyZ) {
        if (shift) {
            Redo();
        } else {
            Undo();
        }
        result = {};
    } else if (control && args.GetKey() == KeyboardKeyY) {
        Redo();
        result = {};
    } else if (args.GetKey() == KeyboardKeyLeft) {
        result = MoveCaretHorizontal(-1.0, shift);
    } else if (args.GetKey() == KeyboardKeyRight) {
        result = MoveCaretHorizontal(1.0, shift);
    } else if (args.GetKey() == KeyboardKeyHome) {
        result = MoveCaretLineBoundary(false, shift);
    } else if (args.GetKey() == KeyboardKeyEnd) {
        result = MoveCaretLineBoundary(true, shift);
    } else if (args.GetKey() == KeyboardKeyBackspace) {
        result = DeleteBackward();
    } else if (args.GetKey() == KeyboardKeyDelete) {
        result = DeleteForward();
    } else if (args.GetKey() == KeyboardKeyEnter && GetAcceptsReturnCore()) {
        result = ReplaceSelection(Base::StringView("\n"));
    } else {
        handled = false;
    }
    if (handled && result) {
        args.SetHandled(true);
    }
}

void TextBoxBase::OnTextInput(TextCompositionEventArgs& args) {
    if (!GetIsEnabled() || GetIsReadOnlyCore()) {
        return;
    }
    if (compositionActive_) {
        Base::Result<void> cancelled = CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    Base::Result<void> inserted = ReplaceSelection(args.GetText());
    if (inserted) {
        args.SetHandled(true);
    }
}

void TextBoxBase::OnLostKeyboardFocus(KeyboardFocusChangedEventArgs&) {
    static_cast<void>(CancelCompositionForFocusLoss());
    if (!drag_.isDragging) {
        return;
    }
    drag_.isDragging = false;
    static_cast<void>(ReleasePointer(drag_.pointerId));
}

void TextBoxBase::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Control::OnPropertyChanged(args);
    if (args.GetProperty() == TextBoxBase::SelectionBrushProperty ||
        args.GetProperty() == TextBoxBase::SelectionOpacityProperty ||
        args.GetProperty() == TextBoxBase::CaretBrushProperty) {
        InvalidateEditor();
    } else if (args.GetProperty() == UIElement::IsEnabledProperty && !args.GetNewValue().AsBoolean()) {
        static_cast<void>(CancelCompositionForFocusLoss());
    } else if (args.GetProperty() == UIElement::IsKeyboardFocusedProperty) {
        InvalidateEditor();
    }
}

void TextBox::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    TextBoxBase::OnPropertyChanged(args);
    if (args.GetProperty() == TextBox::TextProperty) {
        OnTextPropertyChanged(args);
    } else if (args.GetProperty() == TextBox::IsReadOnlyProperty) {
        if (args.GetNewValue().AsBoolean()) {
            static_cast<void>(CancelCompositionForFocusLoss());
        }
        static_cast<void>(Model(model_).SetReadOnly(args.GetNewValue().AsBoolean()));
    } else if (args.GetProperty() == TextBox::MaxLengthProperty) {
        static_cast<void>(CancelCompositionForFocusLoss());
    }
}

PropertyValue TextBox::CoerceValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& baseValue) noexcept {
    if (property != TextBox::TextProperty.Handle()) {
        return baseValue;
    }
    if (baseValue.Kind() != Meta::ValueKind::String) {
        return baseValue;
    }
    ::Aero::Text::EditableTextModel validation;
    if (!validation.SetText(baseValue.AsString())) {
        return PropertyValue::Unset();
    }
    return baseValue;
}

// ---- Section: Selection/caret (merged from TextBoxSelection.cpp) ----

using namespace Primitives;
using namespace ::Aero::Render;

Base::Result<void> TextBoxBase::MoveCaretHorizontal(
    double direction,
    bool extend) noexcept {
    const TextSelection old =
        Model(GetActiveModel()).Selection();
    std::uint32_t next = old.caret;
    if (!extend && !old.GetIsEmpty()) {
        next = direction < 0.0
            ? old.GetStart() : old.GetEnd();
    } else if (direction < 0.0) {
        if (next != 0U) {
            --next;
        }
    } else if (next < Model(GetActiveModel()).GraphemeCount()) {
        ++next;
    }
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

Base::Result<void>
TextBoxBase::MoveCaretLineBoundary(
    bool end,
    bool extend) noexcept {
    const TextSelection old =
        Model(GetActiveModel()).Selection();
    std::uint32_t lineIndex = 0U;
    for (std::uint32_t line = 0U;
         line < Model(GetActiveModel()).LineCount(); ++line) {
        Base::Result<TextRange> range =
            Model(GetActiveModel()).LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t nextStart =
            line + 1U < Model(GetActiveModel()).LineCount()
            ? range.Value().GetEnd() + 1U
            : Model(GetActiveModel()).GraphemeCount() + 1U;
        if (old.caret < nextStart) {
            lineIndex = line;
            break;
        }
    }
    Base::Result<TextRange> range =
        Model(GetActiveModel()).LineRange(lineIndex);
    if (!range) {
        return range.GetStatus();
    }
    const std::uint32_t next = end
        ? range.Value().GetEnd()
        : range.Value().start;
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

double TextBoxBase::GetLineHeight() const noexcept {
    if (!caretStops_.Empty() &&
        caretStops_[0].height > 0.0) {
        return caretStops_[0].height;
    }
    return GetFontSize() * 1.6 /
        std::max(1.0, GetDpiScale());
}

Rect TextBoxBase::GetCaretRectangle() const noexcept {
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

std::uint32_t TextBoxBase::HitTestText(
    Point position) const noexcept {
    if (caretStops_.Empty()) {
        return 0U;
    }
    double padLeft = GetPadding().left;
    double padTop = GetPadding().top;
    if (scrollViewer_ != nullptr) {
        const Rect svSlot = scrollViewer_->GetLayoutSlot();
        padLeft = svSlot.x + scrollViewer_->GetPadding().left;
        padTop = svSlot.y + scrollViewer_->GetPadding().top;
    }
    const double x =
        position.x -
            padLeft +
            scroll_.horizontalOffset;
    const double y =
        position.y -
            padTop +
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
TextBoxBase::RebuildCaretStops() noexcept {
    caretStops_.Clear();
    const auto& active = Model(GetActiveModel());
    const std::uint32_t graphemes =
        active.GraphemeCount();
    caretStops_.Reserve(
            graphemes + 1U);
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

    caretStops_.Resize(
            graphemes + 1U);
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

Size TextBoxBase::MeasureOverride(
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
        !GetPlaceholderCore().Empty();
    if (showingPlaceholder_) {
        display = displayText_.Assign(
            GetPlaceholderCore());
        if (!display) {
            return Size{};
        }
    }
    const Thickness padding = GetPadding();
    const Size contentAvailable =
        Deflate(availableSize, padding);
    if (GetTextWrappingCore() !=
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
        TextLayoutRequest request;
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
                GetFontStyleCore() !=
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
        request.wrapping = GetTextWrappingCore();
        request.alignment = GetTextAlignmentCore();
        request.direction = GetFlowDirection() == FlowDirection::RightToLeft
            ? Text::TextDirection::RightToLeft
            : Text::TextDirection::LeftToRight;
        TextLayoutResult result;
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
        EnsureCaretVisible();
    }
    UpdateCandidateWindow();
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
        GetMaxLinesCore();
    const std::uint32_t minimumLines =
        GetMinLinesCore();
    if (maximumLines != 0U) {
        const double lineBoxHeight =
            std::max(
                GetLineHeight(),
                GetFontSize() * 1.6);
        const double maximumHeight =
            lineBoxHeight *
            static_cast<double>(
                maximumLines);
        if (GetTextWrappingCore() !=
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
    const Size textInflated = Inflate(desired, padding);
    Size templateSize{};
    if (GetTemplateRoot() != nullptr) {
        templateSize = Control::MeasureOverride(availableSize);
    }
    return Size{
        std::max(templateSize.width, textInflated.width),
        std::max(templateSize.height, textInflated.height)};
}

Size TextBoxBase::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        Control::ArrangeOverride(finalSize);
    }
    const Size contentViewport =
        Deflate(finalSize, GetPadding());
    SetViewport(contentViewport);
    if (GetIsKeyboardFocused() ||
        compositionActive_) {
        EnsureCaretVisible();
    }
    UpdateCandidateWindow();
    return finalSize;
}

void TextBoxBase::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild(Base::StringView("PART_ContentHost"));
    if (part != nullptr && DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            part->RuntimeType(), ScrollViewer::StaticTypeId())) {
        static_cast<void>(AttachScrollViewer(static_cast<ScrollViewer*>(part)));
    } else {
        static_cast<void>(AttachScrollViewer(nullptr));
    }
}

void TextBoxBase::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    if (GetTemplateRoot() == nullptr) {
        auto& builder = Aero::Render::DrawingBridge::Builder(context);
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
            ::Aero::Media::SampleBrush(GetBorderBrush());
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
                ::Aero::Media::SampleBrush(GetBackground()),
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
    }
    if (GetTemplateRoot() == nullptr || scrollViewer_ == nullptr) {
        static_cast<void>(RenderEditor(
            context,
            GetRenderSize(),
            GetIsKeyboardFocused(),
            false));
    }
}

Base::Result<void>
TextBoxBase::RenderEditor(
    ::Aero::Media::DrawingContext& context,
    Size viewport,
    bool drawCaret,
    bool isContentHost) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    Thickness padding = isContentHost ? Thickness{} : GetPadding();
    if (!isContentHost && scrollViewer_ != nullptr) {
        const Rect svSlot = scrollViewer_->GetLayoutSlot();
        padding.left += svSlot.x;
        padding.top += svSlot.y;
    }
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
        builder.PushTransform(Transform2D{
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
                ::Aero::Media::SampleBrush(
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
                    ? ::Aero::Media::SampleBrush(
                        GetPlaceholderForegroundCore(),
                        0.5,
                        Color{
                            123.0F / 255.0F,
                            128.0F / 255.0F,
                            133.0F / 255.0F,
                            1.0F})
                    : ::Aero::Media::SampleBrush(
                        GetForeground(),
                        0.5,
                        Color{
                            0.0F, 0.0F, 0.0F, 1.0F}));
        if (!drawn) {
            return drawn;
        }
    }
    if (drawCaret) {
        Rect caret = GetCaretRectangle();
        caret.x += scroll_.horizontalOffset;
        caret.y += scroll_.verticalOffset;
        Base::Result<void> drawn =
            builder.FillRect(
                caret,
                ::Aero::Media::SampleBrush(
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

void TextBoxBase::ReleaseGlyphRuns() noexcept {
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

void TextBoxBase::SetViewport(
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
        InvalidateEditor();
    }
}

void TextBoxBase::SetHorizontalOffset(
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
    InvalidateEditor();
}

void TextBoxBase::SetVerticalOffset(
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
    InvalidateEditor();
}

Base::Result<bool> TextBoxBase::LineHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * ScrollLine);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBoxBase::LineVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * GetLineHeight());
    return old != scroll_.verticalOffset;
}

Base::Result<bool> TextBoxBase::PageHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * scroll_.viewportWidth);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBoxBase::PageVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * scroll_.viewportHeight);
    return old != scroll_.verticalOffset;
}

void TextBoxBase::EnsureCaretVisible() noexcept {
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
}

void TextBoxBase::UpdateCandidateWindow() noexcept {
    if (inputMethodHost_ == nullptr ||
        !compositionActive_) {
        return;
    }
    Input::ImeCandidateWindow candidate;
    Rect caret = GetCaretRectangle();
    caret.x += GetPadding().left;
    caret.y += GetPadding().top;
    UIElement& owner = *this;
    candidate.caret =
        ToRootRect(owner, caret);
    candidate.dpiScale = GetDpiScale();
    inputMethodHost_->SetCandidateWindow(candidate);
}

namespace Primitives {

TextBoxView::TextBoxView() noexcept
    : FrameworkElement(StaticTypeId()) {}

TextBoxView::TextBoxView(TextBoxBase* owner) noexcept
    : FrameworkElement(StaticTypeId()), owner_(owner) {}

Size TextBoxView::MeasureOverride(Size) noexcept {
    if (owner_ == nullptr) return Size{};
    return Size{
        owner_->textSize_.width,
        std::max(owner_->textSize_.height, owner_->GetLineHeight())};
}

Size TextBoxView::ArrangeOverride(Size finalSize) noexcept {
    return finalSize;
}

void TextBoxView::OnRender(::Aero::Media::DrawingContext& context) noexcept {
    if (owner_ == nullptr) return;
    static_cast<void>(owner_->RenderEditor(context, GetRenderSize(), owner_->GetIsKeyboardFocused(), true));
}

AERO_DESCRIBE(TextBoxView) {
    Register<TextBoxView>(context);
}

AERO_DESCRIBE(TextBoxBase) {
    using namespace Aero::Meta;
    using namespace Aero::Media;
    const auto makeBrush = [](Base::Color color) noexcept {
        Base::Result<Base::Ref<Brush>> made = MakeSolidColorBrush(color);
        return made ? std::move(made).Value() : Base::Ref<Brush>{};
    };
    const Base::Ref<Brush> black = makeBrush({0.0F, 0.0F, 0.0F, 1.0F});
    const Base::Ref<Brush> selection = makeBrush({46.0F / 255.0F, 174.0F / 255.0F, 235.0F / 255.0F, 1.0F});

    Register<TextBoxBase>(context, TypeFlags::Abstract)
        .Override(UIElement::FocusableProperty, true, FrameworkPropertyMetadataOptions::None)
        .Override(UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Property(TextBoxBase::SelectionBrushProperty, selection, AffectsRender)
        .Property(TextBoxBase::SelectionOpacityProperty, 0.25, AffectsRender, &ValidateNormalizedDouble)
        .Property(TextBoxBase::CaretBrushProperty, black, AffectsRender)
        .TemplatePart("PART_ContentHost", TypeOf<ScrollViewer>());
    AERO_ON(TextBoxBase, &TextBoxBase::OnMouseDown, UIElement::MouseDownEvent);
    AERO_ON(TextBoxBase, &TextBoxBase::OnMouseMove, UIElement::MouseMoveEvent);
    AERO_ON(TextBoxBase, &TextBoxBase::OnMouseUp, UIElement::MouseUpEvent);
    AERO_ON(TextBoxBase, &TextBoxBase::OnKeyDown, UIElement::KeyDownEvent);
    AERO_ON(TextBoxBase, &TextBoxBase::OnTextInput, UIElement::TextInputEvent);
    AERO_ON(TextBoxBase, &TextBoxBase::OnLostKeyboardFocus, UIElement::LostKeyboardFocusEvent);
}

} // namespace Primitives

AERO_DESCRIBE(TextBox) {
    using namespace Aero::Meta;
    using namespace Aero::Media;
    const auto makeBrush = [](Base::Color color) noexcept {
        Base::Result<Base::Ref<Brush>> made = MakeSolidColorBrush(color);
        return made ? std::move(made).Value() : Base::Ref<Brush>{};
    };
    const Base::Ref<Brush> placeholder = makeBrush({123.0F / 255.0F, 128.0F / 255.0F, 133.0F / 255.0F, 1.0F});

    Register<TextBox>(context)
        .Event(TextBox::TextChangedEvent)
        .Property(TextBox::TextProperty, FrameworkPropertyMetadata(Base::String{}, AffectsMeasure | AffectsRender | BindsTwoWayByDefault).UpdateSource(UpdateSourceTrigger::LostFocus))
        .Property(TextBox::IsReadOnlyProperty, false, AffectsRender)
        .Property(TextBox::MaxLengthProperty, std::uint32_t{0})
        .Property(TextBox::AcceptsReturnProperty, false, AffectsMeasure | AffectsRender)
        .Property(TextBox::TextWrappingProperty, TextWrapping::NoWrap, AffectsMeasure | AffectsRender)
        .Property(TextBox::PlaceholderProperty, Base::String{}, AffectsMeasure)
        .Property(TextBox::PlaceholderForegroundProperty, placeholder, AffectsRender)
        .Property(TextBox::FontSizeProperty, 15.0, AffectsMeasure, &ValidatePositiveFiniteDouble)
        .Property(TextBox::FontWeightProperty, FontWeight::Normal, AffectsMeasure)
        .Property(TextBox::FontStyleProperty, FontStyle::Normal, AffectsMeasure)
        .Property(TextBox::TextAlignmentProperty, TextAlignment::Left, AffectsMeasure)
        .Property(TextBox::MaxLinesProperty, std::uint32_t{0}, AffectsMeasure | AffectsRender)
        .Property(TextBox::MinLinesProperty, std::uint32_t{1}, AffectsMeasure | AffectsRender, &Base::Validate::Positive<std::uint32_t>)
        .Factory();
}

PasswordBox::PasswordBox() noexcept
    : TextBoxBase(StaticTypeId()),
      passwordPolicy_(new (std::nothrow) PasswordTextDisplayPolicy()) {
    displayPolicy_ = passwordPolicy_;
    if (passwordPolicy_ != nullptr) {
        static_cast<void>(PasswordPolicy(passwordPolicy_)->SetMask(GetPasswordChar()));
    }
    SetValue(TextProperties::PasswordLengthProperty, std::uint32_t{0});
}

PasswordBox::~PasswordBox() {
    delete static_cast<::Aero::Controls::PasswordTextDisplayPolicy*>(passwordPolicy_);
    passwordPolicy_ = nullptr;
}

void PasswordBox::SetPassword(
    Base::StringView value) noexcept {
    if (password_.View() == value) return;
    ::Aero::Text::EditableTextModel next;
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
    if (model_ != nullptr) {
        static_cast<void>(Model(model_).SetText(value));
        static_cast<void>(Model(model_).SetSelection(0U, 0U));
    }
    password_ = std::move(nextPassword);
    SetValue(TextProperties::PasswordLengthProperty, static_cast<std::uint32_t>(password_.SizeBytes()));
    InvalidateMeasure();
    InvalidateVisual();
    InvalidateEditor();
    if (view_) {
        view_->InvalidateMeasure();
    }
    RoutedEventArgs args;
    RaiseEvent(PasswordChangedEvent, &args);
}

Base::StringView PasswordBox::GetPasswordChar() const noexcept {
    return GetValue(PasswordCharProperty);
}

void PasswordBox::SetPasswordChar(
    Base::StringView value) noexcept {
    PasswordTextDisplayPolicy validation;
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
    InvalidateMeasure();
    InvalidateVisual();
    InvalidateEditor();
    if (view_) {
        view_->InvalidateMeasure();
    }
}

std::uint32_t PasswordBox::GetMaxLength() const noexcept {
    return GetValue(MaxLengthProperty);
}

void PasswordBox::SetMaxLength(
    std::uint32_t value) noexcept {
    const std::uint32_t effective =
        EffectiveMaximumLength(value);
    if (Model(model_).GraphemeCount() > effective) {
        return;
    }
    SetValue(MaxLengthProperty, value);
}

Base::Result<void> PasswordBox::CommitModelText() noexcept {
    Base::String snapshot;
    if (model_ != nullptr) {
        Base::Result<void> copied =
            Model(model_).Snapshot(snapshot);
        if (!copied) {
            return copied;
        }
    }
    const bool changed = (password_.View() != snapshot.View());
    if (changed) {
        password_ = std::move(snapshot);
        SetValue(TextProperties::PasswordLengthProperty, static_cast<std::uint32_t>(password_.SizeBytes()));
        RoutedEventArgs args;
        RaiseEvent(PasswordChangedEvent, &args);
    }
    return TextBoxBase::CommitModelText();
}

bool PasswordBox::ValidateValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& value) const noexcept {
    if (property == PasswordCharProperty.Handle()) {
        if (value.Kind() != Meta::ValueKind::String) {
            return false;
        }
        ::Aero::Text::EditableTextModel validation;
        Base::Result<void> text =
            validation.SetText(value.AsString());
        return text &&
            validation.GraphemeCount() == 1U;
    }
    return TextBoxBase::ValidateValueCore(property, value);
}

void PasswordBox::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    TextBoxBase::OnPropertyChanged(args);
    if (args.GetProperty() == PasswordBox::PasswordCharProperty) {
        if (passwordPolicy_ != nullptr) {
            static_cast<void>(PasswordPolicy(passwordPolicy_)->SetMask(GetPasswordChar()));
        }
        InvalidateMeasure();
        InvalidateVisual();
        InvalidateEditor();
        if (view_) {
            view_->InvalidateMeasure();
        }
    } else if (args.GetProperty() == PasswordBox::MaxLengthProperty) {
        static_cast<void>(CancelCompositionForFocusLoss());
    }
}

AERO_DESCRIBE(PasswordBox) {
    using namespace Aero::Meta;
    Base::String defaultPasswordChar;
    (void)defaultPasswordChar.Assign(Base::StringView(u8"\u2022"));

    Register<PasswordBox>(context)
        .Event(PasswordBox::PasswordChangedEvent)
        .Property<Base::String, &PasswordBox::GetPassword, &PasswordBox::SetPassword>("Password", PropertyFlags::None)
        .Property(PasswordBox::PasswordCharProperty, std::move(defaultPasswordChar), AffectsMeasure)
        .Property(PasswordBox::MaxLengthProperty, std::uint32_t{0}, AffectsMeasure)
        .Property(PasswordBox::PlaceholderProperty, Base::String{}, AffectsMeasure | AffectsRender)
        .Factory();
}

} // namespace Aero::Controls

namespace Aero {

void Controls::TextBlock::AttachTextLayout(
    void*,
    bool invalidate) noexcept {
    if (invalidate) InvalidateMeasure();
}

void Controls::Primitives::TextBoxBase::AttachTextLayout(
    void*,
    bool invalidate) noexcept {
    if (invalidate) InvalidateMeasure();
}

} // namespace Aero
