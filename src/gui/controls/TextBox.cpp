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
    virtual void BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept = 0;
    virtual bool AllowsCopy() const noexcept = 0;
    virtual bool AllowsCut() const noexcept = 0;
};

class PlainTextDisplayPolicy : public TextDisplayPolicy {
public:
    void BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept override {
        model.Snapshot(output);
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

    bool SetMask(Base::StringView value) noexcept {
        ::Aero::Text::EditableTextModel validation;
        if (!validation.SetText(value) || validation.GraphemeCount() != 1U) {
            return false;
        }
        return static_cast<bool>(mask_.Assign(value));
    }

    Base::StringView GetMask() const noexcept { return mask_.View(); }

    void BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept override {
        output.Clear();
        const std::uint32_t count = model.GraphemeCount();
        if (count != 0U && mask_.SizeBytes() > UINT32_MAX / count) {
            return;
        }
        output.Reserve(mask_.SizeBytes() * count);
        Base::String source;
        model.Snapshot(source);
        for (std::uint32_t index = 0U; index < count; ++index) {
            const std::uint32_t begin =
                model.ByteOffsetForGrapheme(index);
            const std::uint32_t end =
                model.ByteOffsetForGrapheme(index + 1U);
            const Base::StringView cluster = source.View().Substr(
                begin, end - begin);
            const bool newline = !cluster.Empty() &&
                (cluster[0] == '\r' || cluster[0] == '\n');
            output.Append(
                newline ? cluster : mask_.View());
        }
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

namespace {
PlainTextDisplayPolicy s_plainDisplayPolicy;
PasswordTextDisplayPolicy s_defaultPasswordDisplayPolicy;
} // namespace

struct TextBoxBase::EditorState {
    ::Aero::Text::EditableTextModel model;
    ::Aero::Text::EditableTextModel* compositionModel = nullptr;
    TextDisplayPolicy* displayPolicy = &s_plainDisplayPolicy;
    String displayText;
    String compositionText;
    Base::Vector<std::uint64_t> glyphRuns;
    Base::Vector<TextHitRegion> hitRegions;
    Base::Vector<CaretStop> caretStops;
    Size textSize;
    std::uint32_t wrapColumns = UINT32_MAX;
    ScrollData scroll;
    ScrollViewer* scrollViewer = nullptr;
    Input::ITextInputMethodHost* inputMethodHost = nullptr;
    TextSelection compositionSelection;
    bool serviceOwnsGlyphRuns = false;
    bool compositionActive = false;
    bool showingPlaceholder = false;
    bool ownsDisplayPolicy = false;
    Ref<UIElement> view;
    DragSelectionState drag;

    ::Aero::Text::EditableTextModel* EnsureCompositionModel() noexcept {
        if (compositionModel == nullptr) {
            compositionModel = new (std::nothrow) ::Aero::Text::EditableTextModel();
        }
        return compositionModel;
    }

    ~EditorState() {
        delete compositionModel;
        compositionModel = nullptr;
        if (ownsDisplayPolicy) {
            delete displayPolicy;
            displayPolicy = nullptr;
        }
    }
};

Input::ITextInputMethodHost* TextBoxBase::GetInputMethodHost() const noexcept {
    return state_ != nullptr ? state_->inputMethodHost : nullptr;
}

bool TextBoxBase::GetIsComposing() const noexcept {
    return state_ != nullptr && state_->compositionActive;
}

StringView TextBoxBase::GetCompositionText() const noexcept {
    return state_ != nullptr ? state_->compositionText.View() : StringView{};
}

ScrollData TextBoxBase::GetData() const noexcept {
    return state_ != nullptr ? state_->scroll : ScrollData{};
}

TextBoxBase::TextBoxBase(TypeId runtimeType) noexcept
    : Control(runtimeType),
      state_(new (std::nothrow) EditorState()) {}

TextBoxBase::~TextBoxBase() {
    if (state_ != nullptr) {
        if (state_->inputMethodHost != nullptr) {
            static_cast<void>(
                state_->inputMethodHost->SetClient(nullptr));
            state_->inputMethodHost = nullptr;
        }
        state_->scrollViewer = nullptr;
        ReleaseGlyphRuns();
        delete state_;
        state_ = nullptr;
    }
}

const void*
TextBoxBase::GetActiveModel() const noexcept {
    if (state_ == nullptr) return nullptr;
    return (state_->compositionActive && state_->compositionModel != nullptr)
        ? state_->compositionModel
        : &state_->model;
}

TextSelection
TextBoxBase::GetSelection() const noexcept {
    return Model(GetActiveModel()).Selection();
}

std::uint32_t TextBoxBase::GetCaret() const noexcept {
    return Model(GetActiveModel()).Caret();
}

TextBox::TextBox() noexcept
    : TextBoxBase(StaticTypeId()) {
    if (state_ != nullptr) {
        state_->displayPolicy = &s_plainDisplayPolicy;
        state_->ownsDisplayPolicy = false;
    }
}

TextBox::~TextBox() = default;

Base::StringView TextBox::GetText() const noexcept {
    return GetValue(TextProperty);
}

void TextBox::SetText(
    Base::StringView value) noexcept {
    ::Aero::Text::EditableTextModel validation;
    if (!validation.SetText(value)) {
        return;
    }
    updatingTextProperty_ = true;
    SetValue(TextProperty, value);
    updatingTextProperty_ = false;
    static_cast<void>(SynchronizeModel());
}

void TextBox::OnTextPropertyChanged(
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    if (!updatingTextProperty_) {
        const bool synchronized = SynchronizeModel();
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
    if (value && state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    SetValue(IsReadOnlyProperty, value);
    state_->model.SetReadOnly(value);
}

std::uint32_t TextBox::GetMaxLength() const noexcept {
    return GetValue(MaxLengthProperty);
}

void TextBox::SetMaxLength(
    std::uint32_t value) noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    if (state_ != nullptr) {
        if (!state_->model.SetMaximumLength(EffectiveMaximumLength(value))) {
            return;
        }
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
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    state_->model.SetSelection(anchor, caret);
    EnsureCaretVisible();
    InvalidateEditor();
}

void TextBoxBase::SelectAll() noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    state_->model.SelectAll();
    EnsureCaretVisible();
    InvalidateEditor();
}

void TextBoxBase::Undo() noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    state_->model.Undo();
    CommitModelText();
}

void TextBoxBase::Redo() noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    state_->model.Redo();
    CommitModelText();
}

void TextBoxBase::AttachScrollViewer(
    ScrollViewer* viewer) noexcept {
    if (!VerifyAccess()) {
        return;
    }
    if (viewer == state_->scrollViewer) {
        return;
    }
    if (state_->scrollViewer != nullptr) {
        if (state_->scrollViewer->GetContentScrollInfo() == this) {
            state_->scrollViewer->SetContentScrollInfo(nullptr);
        }
        state_->scrollViewer->SetContent(nullptr);
    }
    state_->scrollViewer = viewer;
    if (viewer == nullptr) {
        return;
    }
    if (!state_->view) {
        state_->view = Base::MakeRef<Primitives::TextBoxView>(this);
    }
    viewer->SetContentScrollInfo(this);
    viewer->SetCanContentScroll(true);
    viewer->SetContent(state_->view.Get());
}

void TextBoxBase::InvalidateEditor() noexcept {
    InvalidateVisual();
    if (state_->view) {
        state_->view->InvalidateVisual();
    }
}

void TextBoxBase::SetInputMethodHost(
    Input::ITextInputMethodHost* host) noexcept {
    if (!VerifyAccess()) {
        return;
    }
    if (host == state_->inputMethodHost) {
        return;
    }
    if (state_->inputMethodHost != nullptr) {
        state_->inputMethodHost->SetClient(nullptr);
    }
    if (state_->compositionActive) {
        CancelComposition();
    }
    state_->inputMethodHost = nullptr;
    if (host == nullptr) {
        return;
    }
    host->SetClient(this);
    state_->inputMethodHost = host;
    (void)UpdateCandidateWindow();
}

void
TextBoxBase::BeginComposition() noexcept {
    if (state_->compositionActive) {
        return;
    }
    if (GetIsReadOnlyCore() || !GetIsEnabled()) {
        return;
    }
    ::Aero::Text::EditableTextModel* compModel =
        state_->EnsureCompositionModel();
    if (compModel == nullptr) {
        return;
    }
    Base::String snapshot;
    state_->model.Snapshot(snapshot);
    compModel->SetMaximumLength(UINT32_MAX);
    compModel->SetText(snapshot.View());
    state_->compositionSelection =
        state_->model.Selection();
    compModel->SetSelection(
        state_->compositionSelection.anchor,
        state_->compositionSelection.caret);
    state_->compositionText.Clear();
    state_->compositionActive = true;
    InvalidateMeasure();
    InvalidateVisual();
    UpdateCandidateWindow();
}

void TextBoxBase::UpdateComposition(
    Base::StringView text) noexcept {
    if (!state_->compositionActive) {
        BeginComposition();
        if (!state_->compositionActive) {
            return;
        }
    }
    ::Aero::Text::EditableTextModel* compModel =
        state_->EnsureCompositionModel();
    if (compModel == nullptr) {
        return;
    }
    Base::String filtered;
    SanitizeInput(text, filtered);
    ConstrainManualInput(
        filtered,
        &state_->model,
        state_->compositionSelection);
    Base::String snapshot;
    state_->model.Snapshot(snapshot);
    compModel->SetText(snapshot.View());
    compModel->SetSelection(
        state_->compositionSelection.anchor,
        state_->compositionSelection.caret);
    compModel->ReplaceSelection(filtered.View());
    static_cast<void>(state_->compositionText.Assign(filtered.View()));
    InvalidateMeasure();
    InvalidateVisual();
    UpdateCandidateWindow();
}

void TextBoxBase::CommitComposition(
    Base::StringView text) noexcept {
    if (!state_->compositionActive) {
        BeginComposition();
    }
    Base::String filtered;
    SanitizeInput(text, filtered);
    ConstrainManualInput(
        filtered,
        &state_->model,
        state_->compositionSelection);
    if (filtered.Empty() && !text.Empty()) {
        CancelComposition();
        return;
    }
    state_->model.SetSelection(
        state_->compositionSelection.anchor,
        state_->compositionSelection.caret);
    state_->model.ReplaceSelection(filtered.View());
    state_->compositionActive = false;
    state_->compositionText.Clear();
    CommitModelText();
    UpdateCandidateWindow();
}

void
TextBoxBase::CancelComposition() noexcept {
    if (!state_->compositionActive) {
        return;
    }
    state_->compositionActive = false;
    state_->compositionText.Clear();
    InvalidateMeasure();
    InvalidateVisual();
    EnsureCaretVisible();
}

void
TextBoxBase::CancelCompositionForFocusLoss() noexcept {
    if (!state_->compositionActive) {
        return;
    }
    if (state_->inputMethodHost != nullptr) {
        state_->inputMethodHost->CancelNativeComposition();
    }
    CancelComposition();
}

bool TextBox::SynchronizeModel() noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    state_->model.SetMaximumLength(UINT32_MAX);
    if (!state_->model.SetText(GetText())) {
        return false;
    }
    state_->model.SetReadOnly(GetIsReadOnly());
    InvalidateMeasure();
    InvalidateVisual();
    return true;
}

void TextBoxBase::CommitModelText() noexcept {
    InvalidateMeasure();
    EnsureCaretVisible();
    InvalidateEditor();
    if (state_->view) {
        state_->view->InvalidateMeasure();
    }
}

void TextBox::CommitModelText() noexcept {
    Base::String snapshot;
    if (state_ != nullptr) {
        state_->model.Snapshot(snapshot);
    }
    updatingTextProperty_ = true;
    SetCurrentValue(TextProperty, snapshot);
    updatingTextProperty_ = false;
    TextBoxBase::CommitModelText();
}

void TextBoxBase::SanitizeInput(
    Base::StringView input,
    Base::String& output) const noexcept {
    if (GetAcceptsReturnCore()) {
        output.Assign(input);
        return;
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
            output.AppendUnchecked(
                input.Substr(
                    start, index - start));
        }
        start = index + 1U;
    }
    if (start < input.SizeBytes()) {
        output.AppendUnchecked(
            input.Substr(start));
    }
}

void TextBoxBase::ConstrainManualInput(
    Base::String& input,
    const void* target,
    TextSelection selection) const noexcept {
    const auto& model = Model(target);
    const std::uint32_t maximum =
        GetMaxLengthCore();
    if (maximum == 0U || input.Empty()) {
        return;
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
    if (!inserted.SetText(input.View())) {
        return;
    }
    if (inserted.GraphemeCount() <= available) {
        return;
    }
    const std::uint32_t end =
        inserted.ByteOffsetForGrapheme(available);
    Base::String truncated;
    if (!truncated.Assign(
            input.View().Substr(0U, end))) {
        return;
    }
    input = std::move(truncated);
}

void TextBoxBase::ReplaceSelection(
    Base::StringView text) noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    Base::String filtered;
    SanitizeInput(text, filtered);
    ConstrainManualInput(
        filtered,
        &state_->model,
        state_->model.Selection());
    if (filtered.Empty() &&
        state_->model.Selection().GetIsEmpty()) {
        return;
    }
    state_->model.ReplaceSelection(
        filtered.View());
    CommitModelText();
}

void TextBoxBase::DeleteBackward() noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    state_->model.DeleteBackward();
    CommitModelText();
}

void TextBoxBase::DeleteForward() noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    state_->model.DeleteForward();
    CommitModelText();
}

void TextBoxBase::SelectedText(
    Base::String& output) const noexcept {
    const TextSelection selection =
        state_->model.Selection();
    if (selection.GetIsEmpty()) {
        output.Clear();
        return;
    }
    const std::uint32_t begin =
        state_->model.ByteOffsetForGrapheme(
            selection.GetStart());
    const std::uint32_t end =
        state_->model.ByteOffsetForGrapheme(
            selection.GetEnd());
    Base::String snapshot;
    state_->model.Snapshot(snapshot);
    output.Assign(
        snapshot.View().Substr(
            begin,
            end - begin));
}

void TextBoxBase::CopySelection(
    Input::IClipboard& clipboard) const noexcept {
    if (!state_->displayPolicy->AllowsCopy()) {
        return;
    }
    Base::String selected;
    SelectedText(selected);
    if (selected.Empty()) {
        return;
    }
    clipboard.WriteText(
        selected.View());
}

void TextBoxBase::CutSelection(
    Input::IClipboard& clipboard) noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    if (!state_->displayPolicy->AllowsCut()) {
        return;
    }
    if (GetIsReadOnlyCore()) {
        return;
    }
    const TextSelection selection =
        state_->model.Selection();
    if (selection.GetIsEmpty()) {
        return;
    }
    CopySelection(clipboard);
    state_->model.ReplaceSelection({});
    CommitModelText();
}

void TextBoxBase::Paste(
    Input::IClipboard& clipboard) noexcept {
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    if (GetIsReadOnlyCore()) {
        return;
    }
    Base::String text;
    if (!clipboard.ReadText(text)) {
        return;
    }
    ReplaceSelection(text.View());
}

void TextBoxBase::OnMouseDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !GetIsEnabled()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    const std::uint32_t caret = HitTestText(local);
    SetSelection(caret, caret);
    Focus();
    if (CapturePointer(args.GetPointerId())) {
        state_->drag.pointerId = args.GetPointerId();
        state_->drag.dragAnchor = caret;
        state_->drag.isDragging = true;
    }
    args.SetHandled(true);
}

void TextBoxBase::OnMouseMove(MouseEventArgs& args) {
    if (!state_->drag.isDragging || state_->drag.pointerId != args.GetPointerId()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    static_cast<void>(SetSelection(state_->drag.dragAnchor, HitTestText(local)));
    args.SetHandled(true);
}

void TextBoxBase::OnMouseUp(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !state_->drag.isDragging || state_->drag.pointerId != args.GetPointerId()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    static_cast<void>(SetSelection(state_->drag.dragAnchor, HitTestText(local)));
    state_->drag.isDragging = false;
    static_cast<void>(ReleasePointer(args.GetPointerId()));
    args.SetHandled(true);
}

void TextBoxBase::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) {
        return;
    }
    const bool shift = HasKeyboardModifier(args.GetModifiers(), KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(args.GetModifiers(), KeyboardModifiers::Control);
    bool handled = true;
    if (control && args.GetKey() == KeyboardKeyA) {
        SelectAll();
    } else if (control && args.GetKey() == KeyboardKeyC) {
        Input::IClipboard* clipboard = ElementTree::ClipboardOf(*this);
        if (clipboard != nullptr) {
            CopySelection(*clipboard);
        }
    } else if (control && args.GetKey() == KeyboardKeyX) {
        Input::IClipboard* clipboard = ElementTree::ClipboardOf(*this);
        if (clipboard != nullptr) {
            CutSelection(*clipboard);
        }
    } else if (control && args.GetKey() == KeyboardKeyV) {
        Input::IClipboard* clipboard = ElementTree::ClipboardOf(*this);
        if (clipboard != nullptr) {
            Paste(*clipboard);
        }
    } else if (control && args.GetKey() == KeyboardKeyZ) {
        if (shift) {
            Redo();
        } else {
            Undo();
        }
    } else if (control && args.GetKey() == KeyboardKeyY) {
        Redo();
    } else if (args.GetKey() == KeyboardKeyLeft) {
        MoveCaretHorizontal(-1.0, shift);
    } else if (args.GetKey() == KeyboardKeyRight) {
        MoveCaretHorizontal(1.0, shift);
    } else if (args.GetKey() == KeyboardKeyHome) {
        MoveCaretLineBoundary(false, shift);
    } else if (args.GetKey() == KeyboardKeyEnd) {
        MoveCaretLineBoundary(true, shift);
    } else if (args.GetKey() == KeyboardKeyBackspace) {
        DeleteBackward();
    } else if (args.GetKey() == KeyboardKeyDelete) {
        DeleteForward();
    } else if (args.GetKey() == KeyboardKeyEnter && GetAcceptsReturnCore()) {
        ReplaceSelection(Base::StringView("\n"));
    } else {
        handled = false;
    }
    if (handled) {
        args.SetHandled(true);
    }
}

void TextBoxBase::OnTextInput(TextCompositionEventArgs& args) {
    if (!GetIsEnabled() || GetIsReadOnlyCore()) {
        return;
    }
    if (state_->compositionActive) {
        CancelCompositionForFocusLoss();
    }
    ReplaceSelection(args.GetText());
    args.SetHandled(true);
}

void TextBoxBase::OnLostKeyboardFocus(KeyboardFocusChangedEventArgs&) {
    static_cast<void>(CancelCompositionForFocusLoss());
    if (!state_->drag.isDragging) {
        return;
    }
    state_->drag.isDragging = false;
    static_cast<void>(ReleasePointer(state_->drag.pointerId));
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
        state_->model.SetReadOnly(args.GetNewValue().AsBoolean());
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

void TextBoxBase::MoveCaretHorizontal(
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
}

void
TextBoxBase::MoveCaretLineBoundary(
    bool end,
    bool extend) noexcept {
    const TextSelection old =
        Model(GetActiveModel()).Selection();
    std::uint32_t lineIndex = 0U;
    for (std::uint32_t line = 0U;
         line < Model(GetActiveModel()).LineCount(); ++line) {
        const TextRange range =
            Model(GetActiveModel()).LineRange(line);
        const std::uint32_t nextStart =
            line + 1U < Model(GetActiveModel()).LineCount()
            ? range.GetEnd() + 1U
            : Model(GetActiveModel()).GraphemeCount() + 1U;
        if (old.caret < nextStart) {
            lineIndex = line;
            break;
        }
    }
    const TextRange range =
        Model(GetActiveModel()).LineRange(lineIndex);
    const std::uint32_t next = end
        ? range.GetEnd()
        : range.start;
    SetSelection(extend ? old.anchor : next, next);
}

double TextBoxBase::GetLineHeight() const noexcept {
    if (!state_->caretStops.Empty() &&
        state_->caretStops[0].height > 0.0) {
        return state_->caretStops[0].height;
    }
    return GetFontSize() * 1.6 /
        std::max(1.0, GetDpiScale());
}

Rect TextBoxBase::GetCaretRectangle() const noexcept {
    if (state_->caretStops.Empty()) {
        return {
            -state_->scroll.horizontalOffset,
            -state_->scroll.verticalOffset,
            CaretWidth / std::max(1.0, GetDpiScale()),
            GetLineHeight()};
    }
    const std::uint32_t index =
        std::min(
            Model(GetActiveModel()).Caret(),
            state_->caretStops.Size() - 1U);
    const CaretStop& stop =
        state_->caretStops[index];
    return {
        stop.x - state_->scroll.horizontalOffset,
        stop.y - state_->scroll.verticalOffset,
        CaretWidth / std::max(1.0, GetDpiScale()),
        stop.height};
}

std::uint32_t TextBoxBase::HitTestText(
    Point position) const noexcept {
    if (state_->caretStops.Empty()) {
        return 0U;
    }
    double padLeft = GetPadding().left;
    double padTop = GetPadding().top;
    if (state_->scrollViewer != nullptr) {
        const Rect svSlot = state_->scrollViewer->GetLayoutSlot();
        padLeft = svSlot.x + state_->scrollViewer->GetPadding().left;
        padTop = svSlot.y + state_->scrollViewer->GetPadding().top;
    }
    const double x =
        position.x -
            padLeft +
            state_->scroll.horizontalOffset;
    const double y =
        position.y -
            padTop +
            state_->scroll.verticalOffset;
    std::uint32_t best = 0U;
    double bestDistance =
        std::numeric_limits<double>::infinity();
    for (std::uint32_t index = 0U;
         index < state_->caretStops.Size(); ++index) {
        const CaretStop& stop =
            state_->caretStops[index];
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

void
TextBoxBase::RebuildCaretStops() noexcept {
    state_->caretStops.Clear();
    const auto& active = Model(GetActiveModel());
    const std::uint32_t graphemes =
        active.GraphemeCount();
    state_->caretStops.Reserve(
            graphemes + 1U);
    const std::uint32_t lines =
        std::max(1U, active.LineCount());
    if (!state_->showingPlaceholder &&
        !state_->glyphRuns.Empty() &&
        !state_->hitRegions.Empty() &&
        graphemes > 0U) {
        ::Aero::Text::EditableTextModel displayModel;
        const bool isPlain = state_->displayPolicy == &s_plainDisplayPolicy;
        const bool validDisplayModel = isPlain
            ? (state_->displayText.SizeBytes() == active.ByteOffsetForGrapheme(graphemes))
            : (displayModel.SetText(state_->displayText.View()) &&
               displayModel.GraphemeCount() == graphemes);
        if (validDisplayModel) {
            const auto byteOffsetFor = [&](std::uint32_t g) noexcept -> std::uint32_t {
                return isPlain
                    ? active.ByteOffsetForGrapheme(g)
                    : displayModel.ByteOffsetForGrapheme(g);
            };
            double defaultLineHeight =
                GetFontSize() * 1.6 / std::max(1.0, GetDpiScale());
            for (const TextHitRegion& region : state_->hitRegions) {
                if (region.height > 0.0F) {
                    defaultLineHeight = static_cast<double>(region.height);
                    break;
                }
            }
            state_->caretStops.Resize(graphemes + 1U);
            std::uint32_t currentVisualLine = 0U;
            double currentLineY = 0.0;
            double currentLineHeight = defaultLineHeight;
            std::uint32_t regionCursor = 0U;
            const std::uint32_t regionCount = state_->hitRegions.Size();

            for (std::uint32_t line = 0U; line < lines; ++line) {
                const TextRange range = active.LineRange(line);
                const std::uint32_t start = range.start;
                const std::uint32_t end = range.GetEnd();
                bool firstRegionInLogicalLine = true;
                double lastRightX = 0.0;

                for (std::uint32_t index = start; index < end; ++index) {
                    const std::uint32_t byteStart = byteOffsetFor(index);
                    const std::uint32_t byteEnd = byteOffsetFor(index + 1U);
                    while (regionCursor < regionCount) {
                        const TextHitRegion& cand = state_->hitRegions[regionCursor];
                        if (cand.textLength > 0U &&
                            cand.textOffset + cand.textLength > byteStart) {
                            break;
                        }
                        ++regionCursor;
                    }

                    bool foundRegion = false;
                    double leftX = lastRightX;
                    double rightX = lastRightX;
                    double regY = currentLineY;
                    double regH = currentLineHeight;

                    auto accumulateRegion = [&](const TextHitRegion& reg) noexcept {
                        if (reg.textLength == 0U ||
                            reg.textOffset >= byteEnd ||
                            reg.textOffset + reg.textLength <= byteStart) {
                            return false;
                        }
                        const double startFrac = byteStart > reg.textOffset
                            ? static_cast<double>(byteStart - reg.textOffset) /
                                  static_cast<double>(reg.textLength)
                            : 0.0;
                        const double endFrac = byteEnd < reg.textOffset + reg.textLength
                            ? static_cast<double>(byteEnd - reg.textOffset) /
                                  static_cast<double>(reg.textLength)
                            : 1.0;
                        const double rLeft =
                            static_cast<double>(reg.x) +
                            static_cast<double>(reg.width) * startFrac;
                        const double rRight =
                            static_cast<double>(reg.x) +
                            static_cast<double>(reg.width) * endFrac;
                        if (!foundRegion) {
                            foundRegion = true;
                            leftX = rLeft;
                            rightX = rRight;
                            regY = static_cast<double>(reg.y);
                            if (reg.height > 0.0F) {
                                regH = static_cast<double>(reg.height);
                            }
                        } else if (std::fabs(static_cast<double>(reg.y) - regY) < 0.5) {
                            leftX = std::min(leftX, rLeft);
                            rightX = std::max(rightX, rRight);
                            if (reg.height > 0.0F) {
                                regH = std::max(regH, static_cast<double>(reg.height));
                            }
                        }
                        return true;
                    };

                    for (std::uint32_t r = regionCursor; r < regionCount; ++r) {
                        const TextHitRegion& reg = state_->hitRegions[r];
                        if (reg.textOffset >= byteEnd && foundRegion) {
                            break;
                        }
                        accumulateRegion(reg);
                    }
                    if (!foundRegion) {
                        for (std::uint32_t r = 0U; r < regionCursor; ++r) {
                            accumulateRegion(state_->hitRegions[r]);
                        }
                    }

                    if (foundRegion) {
                        if (firstRegionInLogicalLine) {
                            firstRegionInLogicalLine = false;
                            currentLineY = regY;
                            currentLineHeight = regH;
                        } else if (std::fabs(regY - currentLineY) >= 0.5) {
                            ++currentVisualLine;
                            currentLineY = regY;
                            currentLineHeight = regH;
                        } else {
                            currentLineHeight = std::max(currentLineHeight, regH);
                        }
                        lastRightX = rightX;
                    }
                    state_->caretStops[index] = {
                        leftX,
                        currentLineY,
                        currentLineHeight,
                        currentVisualLine};
                }

                state_->caretStops[end] = {
                    lastRightX,
                    currentLineY,
                    currentLineHeight,
                    currentVisualLine};

                if (line + 1U < lines) {
                    ++currentVisualLine;
                    currentLineY += currentLineHeight;
                    if (end < graphemes) {
                        state_->caretStops[end + 1U] = {
                            0.0,
                            currentLineY,
                            currentLineHeight,
                            currentVisualLine};
                    }
                }
            }
            return;
        }
    }
    std::uint32_t maximumLineLength = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        const TextRange range =
            active.LineRange(line);
        maximumLineLength =
            std::max(
                maximumLineLength,
                range.length);
    }
    std::uint32_t visualLines = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        const TextRange range =
            active.LineRange(line);
        const std::uint32_t length =
            range.length;
        const std::uint32_t wrapped =
            state_->wrapColumns == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (length + state_->wrapColumns - 1U) /
                      state_->wrapColumns);
        visualLines += wrapped;
    }
    visualLines = std::max(1U, visualLines);
    const double lineHeight =
        state_->textSize.height > 0.0
        ? state_->textSize.height /
            static_cast<double>(visualLines)
        : GetFontSize() * 1.6 /
            std::max(1.0, GetDpiScale());
    const double advance =
        state_->wrapColumns != UINT32_MAX
        ? DefaultAdvance *
              GetFontSize() / 16.0 /
              std::max(1.0, GetDpiScale())
        : maximumLineLength != 0U &&
            state_->textSize.width > 0.0
        ? state_->textSize.width /
            static_cast<double>(
                maximumLineLength)
        : DefaultAdvance *
              GetFontSize() / 16.0 /
            std::max(1.0, GetDpiScale());

    state_->caretStops.Resize(
            graphemes + 1U);
    std::uint32_t visualLineBase = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        const TextRange range =
            active.LineRange(line);
        const std::uint32_t start =
            range.start;
        const std::uint32_t end =
            range.GetEnd();
        for (std::uint32_t index = start;
             index <= end; ++index) {
            const std::uint32_t offset =
                index - start;
            std::uint32_t visualLineOffset = 0U;
            std::uint32_t column = offset;
            if (state_->wrapColumns != UINT32_MAX) {
                visualLineOffset =
                    offset / state_->wrapColumns;
                column = offset % state_->wrapColumns;
                if (offset == range.length &&
                    offset != 0U &&
                    column == 0U) {
                    --visualLineOffset;
                    column = state_->wrapColumns;
                }
            }
            state_->caretStops[index] = {
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
            state_->wrapColumns == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (range.length +
                   state_->wrapColumns - 1U) /
                      state_->wrapColumns);
        visualLineBase += wrappedLines;
        if (line + 1U < lines &&
            end < graphemes) {
            state_->caretStops[end + 1U] = {
                0.0,
                static_cast<double>(
                    visualLineBase) *
                    lineHeight,
                lineHeight,
                visualLineBase};
        }
    }
}

Size TextBoxBase::MeasureOverride(
    Size availableSize) noexcept {
    state_->displayPolicy->BuildDisplayText(
        Model(GetActiveModel()), state_->displayText);
    ReleaseGlyphRuns();
    state_->textSize = {};
    state_->wrapColumns = UINT32_MAX;
    const bool hasTemplatePlaceholder =
        GetTemplateRoot() != nullptr &&
        GetTemplateChild(Base::StringView("Placeholder")) != nullptr;
    state_->showingPlaceholder =
        !hasTemplatePlaceholder &&
        state_->displayText.Empty() &&
        !GetPlaceholderCore().Empty();
    if (state_->showingPlaceholder) {
        if (!state_->displayText.Assign(GetPlaceholderCore())) {
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
        state_->wrapColumns = static_cast<std::uint32_t>(
            std::min(
                static_cast<double>(UINT32_MAX),
                std::max(1.0, columns)));
    }
    auto* layoutService = LayoutService(*this);
    if (layoutService != nullptr &&
        !state_->displayText.Empty()) {
        TextLayoutRequest request;
        request.text = state_->displayText.View();
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
        request.fontWeight = GetFontWeight();
        request.fontStyle = GetFontStyleCore();
        request.wrapping = GetTextWrappingCore();
        request.includeHitRegions = !state_->showingPlaceholder;
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
        state_->glyphRuns =
            std::move(result.glyphRuns);
        state_->serviceOwnsGlyphRuns =
            !state_->glyphRuns.Empty();
        state_->hitRegions =
            std::move(result.hitRegions);
        state_->textSize = result.desiredSize;
    } else {
        std::uint32_t maximumLine = 0U;
        std::uint32_t visualLineCount = 0U;
        const auto& active = Model(GetActiveModel());
        for (std::uint32_t line = 0U;
             line < active.LineCount();
             ++line) {
            const TextRange range =
                active.LineRange(line);
            maximumLine = std::max(
                maximumLine,
                range.length);
            visualLineCount +=
                state_->wrapColumns == UINT32_MAX
                ? 1U
                : std::max(
                      1U,
                      (range.length +
                       state_->wrapColumns - 1U) /
                          state_->wrapColumns);
        }
        const std::uint32_t visibleColumns =
            state_->wrapColumns == UINT32_MAX
            ? maximumLine
            : std::min(
                  maximumLine,
                  state_->wrapColumns);
        state_->textSize = {
            static_cast<double>(visibleColumns) *
                DefaultAdvance *
                GetFontSize() / 16.0 /
                std::max(1.0, GetDpiScale()),
            static_cast<double>(
                std::max(1U, visualLineCount)) *
                GetFontSize() * 1.6 /
                std::max(1.0, GetDpiScale())};
    }
    RebuildCaretStops();
    state_->scroll.extentWidth = state_->textSize.width;
    state_->scroll.extentHeight =
        std::max(static_cast<double>(state_->textSize.height), GetLineHeight());
    state_->scroll.horizontalOffset = ClampOffset(
        state_->scroll.horizontalOffset,
        state_->scroll.extentWidth,
        state_->scroll.viewportWidth);
    state_->scroll.verticalOffset = ClampOffset(
        state_->scroll.verticalOffset,
        state_->scroll.extentHeight,
        state_->scroll.viewportHeight);
    if (GetIsKeyboardFocused() ||
        state_->compositionActive) {
        EnsureCaretVisible();
    }
    UpdateCandidateWindow();
    const double minimumWidth =
        DefaultAdvance *
        GetFontSize() / 16.0 /
        std::max(1.0, GetDpiScale());
    Size desired{
        std::min(
            std::max(static_cast<float>(minimumWidth), state_->textSize.width),
            contentAvailable.width),
        std::min(
            std::max(static_cast<float>(GetLineHeight()), state_->textSize.height),
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
            !state_->displayText.Empty()) {
            desired.height = std::min(
                contentAvailable.height,
                static_cast<float>(maximumHeight));
        } else {
            desired.height = std::min(
                desired.height,
                static_cast<float>(maximumHeight));
        }
    }
    desired.height = std::max(
        desired.height,
        std::min(
            contentAvailable.height,
            static_cast<float>(std::max(
                GetLineHeight(),
                GetFontSize() * 1.6) *
                static_cast<double>(
                    minimumLines))));
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
        state_->compositionActive) {
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
    InvalidateMeasure();
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
        builder.FillRoundedRect(
            bounds,
            ::Aero::Media::SampleBrush(GetBackground()),
            1.75);
        if (borderThickness > 0.0 &&
            borderBrush.alpha > 0.0F) {
            builder.StrokeRect(
                bounds,
                borderBrush,
                GetIsKeyboardFocused()
                    ? std::max(1.0, borderThickness)
                    : borderThickness);
        }
    }
    if (GetTemplateRoot() == nullptr || state_->scrollViewer == nullptr) {
        static_cast<void>(RenderEditor(
            context,
            GetRenderSize(),
            GetIsKeyboardFocused(),
            false));
    }
}

void
TextBoxBase::RenderEditor(
    ::Aero::Media::DrawingContext& context,
    Size viewport,
    bool drawCaret,
    bool isContentHost) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    Thickness padding = isContentHost ? Thickness{} : GetPadding();
    if (!isContentHost && state_->scrollViewer != nullptr) {
        const Rect svSlot = state_->scrollViewer->GetLayoutSlot();
        padding.left += svSlot.x;
        padding.top += svSlot.y;
    }
    const Rect contentBounds{
        padding.left,
        padding.top,
        std::max(
            0.0f,
            viewport.width -
                padding.left -
                padding.right),
        std::max(
            0.0f,
            viewport.height -
                padding.top -
                padding.bottom)};
    builder.PushClip(contentBounds);
    builder.PushTransform(Transform2D{
        1.0, 0.0, 0.0, 1.0,
        padding.left -
            state_->scroll.horizontalOffset,
        padding.top -
            state_->scroll.verticalOffset});
    const TextSelection selection =
        Model(GetActiveModel()).Selection();
    if (!selection.GetIsEmpty() &&
        !state_->caretStops.Empty()) {
        const std::uint32_t begin =
            selection.GetStart();
        const std::uint32_t end =
            selection.GetEnd();
        std::uint32_t index = begin;
        while (index < end) {
            const CaretStop& first =
                state_->caretStops[index];
            std::uint32_t lineEnd =
                index + 1U;
            while (lineEnd < end &&
                lineEnd < state_->caretStops.Size() &&
                state_->caretStops[lineEnd].line ==
                    first.line) {
                ++lineEnd;
            }
            const CaretStop& last =
                state_->caretStops[
                    std::min(
                        lineEnd,
                        state_->caretStops.Size() - 1U)];
            const double width =
                last.line == first.line
                ? std::max(
                    0.0,
                    last.x - first.x)
                : std::max(
                    DefaultAdvance,
                    state_->textSize.width - first.x);
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
            builder.FillRect(
                {first.x, first.y,
                 width, first.height},
                selectionColor);
            index = lineEnd;
        }
    }
    for (RenderGlyphRunId glyph :
         state_->glyphRuns) {
            builder.DrawGlyphRun(
                glyph,
                state_->showingPlaceholder
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
    }
    if (drawCaret) {
        Rect caret = GetCaretRectangle();
        caret.x += state_->scroll.horizontalOffset;
        caret.y += state_->scroll.verticalOffset;
        builder.FillRect(
            caret,
            ::Aero::Media::SampleBrush(
                GetCaretBrush(),
                0.5,
                Color{
                    0.0F, 0.0F, 0.0F, 1.0F}));
    }
    builder.PopTransform();
    builder.PopClip();
}

void TextBoxBase::ReleaseGlyphRuns() noexcept {
    auto* layoutService = LayoutService(*this);
    if (state_->serviceOwnsGlyphRuns && layoutService != nullptr) {
        for (RenderGlyphRunId glyph :
             state_->glyphRuns) {
            layoutService->ReleaseGlyphRun(glyph);
        }
    }
    state_->glyphRuns.Clear();
    state_->hitRegions.Clear();
    state_->serviceOwnsGlyphRuns = false;
}

void TextBoxBase::SetViewport(
    Size viewport) noexcept {
    if (!IsValidLayoutSize(viewport)) {
        return;
    }
    const ScrollData old = state_->scroll;
    state_->scroll.viewportWidth = viewport.width;
    state_->scroll.viewportHeight = viewport.height;
    state_->scroll.horizontalOffset = ClampOffset(
        state_->scroll.horizontalOffset,
        state_->scroll.extentWidth,
        state_->scroll.viewportWidth);
    state_->scroll.verticalOffset = ClampOffset(
        state_->scroll.verticalOffset,
        state_->scroll.extentHeight,
        state_->scroll.viewportHeight);
    const bool changed =
        old.viewportWidth !=
            state_->scroll.viewportWidth ||
        old.viewportHeight !=
            state_->scroll.viewportHeight ||
        old.horizontalOffset !=
            state_->scroll.horizontalOffset ||
        old.verticalOffset !=
            state_->scroll.verticalOffset;
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
        state_->scroll.extentWidth,
        state_->scroll.viewportWidth);
    if (next == state_->scroll.horizontalOffset) {
        return;
    }
    state_->scroll.horizontalOffset = next;
    InvalidateEditor();
}

void TextBoxBase::SetVerticalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = ClampOffset(
        value,
        state_->scroll.extentHeight,
        state_->scroll.viewportHeight);
    if (next == state_->scroll.verticalOffset) {
        return;
    }
    state_->scroll.verticalOffset = next;
    InvalidateEditor();
}

bool TextBoxBase::LineHorizontal(
    double direction) noexcept {
    const double old = state_->scroll.horizontalOffset;
    SetHorizontalOffset(old + direction * ScrollLine);
    return old != state_->scroll.horizontalOffset;
}

bool TextBoxBase::LineVertical(
    double direction) noexcept {
    const double old = state_->scroll.verticalOffset;
    SetVerticalOffset(old + direction * GetLineHeight());
    return old != state_->scroll.verticalOffset;
}

bool TextBoxBase::PageHorizontal(
    double direction) noexcept {
    const double old = state_->scroll.horizontalOffset;
    SetHorizontalOffset(old + direction * state_->scroll.viewportWidth);
    return old != state_->scroll.horizontalOffset;
}

bool TextBoxBase::PageVertical(
    double direction) noexcept {
    const double old = state_->scroll.verticalOffset;
    SetVerticalOffset(old + direction * state_->scroll.viewportHeight);
    return old != state_->scroll.verticalOffset;
}

void TextBoxBase::EnsureCaretVisible() noexcept {
    const Rect caret = GetCaretRectangle();
    double horizontal =
        state_->scroll.horizontalOffset;
    double vertical =
        state_->scroll.verticalOffset;
    const double contentX =
        caret.x + state_->scroll.horizontalOffset;
    const double contentY =
        caret.y + state_->scroll.verticalOffset;
    if (contentX < horizontal) {
        horizontal = contentX;
    } else if (contentX + caret.width >
        horizontal + state_->scroll.viewportWidth) {
        horizontal = contentX + caret.width -
            state_->scroll.viewportWidth;
    }
    if (contentY < vertical) {
        vertical = contentY;
    } else if (contentY + caret.height >
        vertical + state_->scroll.viewportHeight) {
        vertical = contentY + caret.height -
            state_->scroll.viewportHeight;
    }
    SetHorizontalOffset(horizontal);
    SetVerticalOffset(vertical);
}

void TextBoxBase::UpdateCandidateWindow() noexcept {
    if (state_->inputMethodHost == nullptr ||
        !state_->compositionActive) {
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
    state_->inputMethodHost->SetCandidateWindow(candidate);
}

namespace Primitives {

TextBoxView::TextBoxView() noexcept
    : FrameworkElement(StaticTypeId()) {}

TextBoxView::TextBoxView(TextBoxBase* owner) noexcept
    : FrameworkElement(StaticTypeId()), owner_(owner) {}

Size TextBoxView::MeasureOverride(Size) noexcept {
    if (owner_ == nullptr || owner_->state_ == nullptr) return Size{};
    return Size{
        owner_->state_->textSize.width,
        std::max(owner_->state_->textSize.height, static_cast<float>(owner_->GetLineHeight()))};
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
    const Base::Ref<Brush> black = MakeSolidColorBrush({0.0F, 0.0F, 0.0F, 1.0F});
    const Base::Ref<Brush> selection = MakeSolidColorBrush({46.0F / 255.0F, 174.0F / 255.0F, 235.0F / 255.0F, 1.0F});

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
    const Base::Ref<Brush> placeholder = MakeSolidColorBrush({123.0F / 255.0F, 128.0F / 255.0F, 133.0F / 255.0F, 1.0F});

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
    : TextBoxBase(StaticTypeId()) {
    if (state_ != nullptr) {
        state_->displayPolicy = &s_defaultPasswordDisplayPolicy;
        state_->ownsDisplayPolicy = false;
    }
    SetValue(TextProperties::PasswordLengthProperty, std::uint32_t{0});
}

PasswordBox::~PasswordBox() = default;

void PasswordBox::SetPassword(
    Base::StringView value) noexcept {
    if (password_.View() == value) return;
    ::Aero::Text::EditableTextModel next;
    next.SetMaximumLength(
        EffectiveMaximumLength(
            GetMaxLength()));
    if (!next.SetText(value)) {
        return;
    }
    Base::String nextPassword;
    if (!nextPassword.Assign(value)) return;
    if (state_ != nullptr) {
        state_->model.SetText(value);
        state_->model.SetSelection(0U, 0U);
    }
    password_ = std::move(nextPassword);
    SetValue(TextProperties::PasswordLengthProperty, static_cast<std::uint32_t>(password_.SizeBytes()));
    InvalidateMeasure();
    InvalidateVisual();
    InvalidateEditor();
    if (state_->view) {
        state_->view->InvalidateMeasure();
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
    if (!validation.SetMask(value)) return;
    if (state_ == nullptr) {
        return;
    }
    if (value == Base::StringView(u8"\u2022")) {
        if (state_->ownsDisplayPolicy) {
            delete state_->displayPolicy;
            state_->ownsDisplayPolicy = false;
        }
        state_->displayPolicy = &s_defaultPasswordDisplayPolicy;
    } else {
        if (!state_->ownsDisplayPolicy) {
            auto* policy = new (std::nothrow) PasswordTextDisplayPolicy();
            if (policy == nullptr) return;
            state_->displayPolicy = policy;
            state_->ownsDisplayPolicy = true;
        }
        if (!PasswordPolicy(state_->displayPolicy)->SetMask(value)) return;
    }
    SetValue(PasswordCharProperty, value);
    InvalidateMeasure();
    InvalidateVisual();
    InvalidateEditor();
    if (state_->view) {
        state_->view->InvalidateMeasure();
    }
}

std::uint32_t PasswordBox::GetMaxLength() const noexcept {
    return GetValue(MaxLengthProperty);
}

void PasswordBox::SetMaxLength(
    std::uint32_t value) noexcept {
    const std::uint32_t effective =
        EffectiveMaximumLength(value);
    if (state_->model.GraphemeCount() > effective) {
        return;
    }
    SetValue(MaxLengthProperty, value);
}

void PasswordBox::CommitModelText() noexcept {
    Base::String snapshot;
    if (state_ != nullptr) {
        state_->model.Snapshot(snapshot);
    }
    const bool changed = (password_.View() != snapshot.View());
    if (changed) {
        password_ = std::move(snapshot);
        SetValue(TextProperties::PasswordLengthProperty, static_cast<std::uint32_t>(password_.SizeBytes()));
        RoutedEventArgs args;
        RaiseEvent(PasswordChangedEvent, &args);
    }
    TextBoxBase::CommitModelText();
}

bool PasswordBox::ValidateValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& value) const noexcept {
    if (property == PasswordCharProperty.Handle()) {
        if (value.Kind() != Meta::ValueKind::String) {
            return false;
        }
        ::Aero::Text::EditableTextModel validation;
        return validation.SetText(value.AsString()) &&
            validation.GraphemeCount() == 1U;
    }
    return TextBoxBase::ValidateValueCore(property, value);
}

void PasswordBox::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    TextBoxBase::OnPropertyChanged(args);
    if (args.GetProperty() == PasswordBox::PasswordCharProperty) {
        if (state_ != nullptr) {
            const Base::StringView mask = GetPasswordChar();
            if (mask == Base::StringView(u8"\u2022")) {
                if (state_->ownsDisplayPolicy) {
                    delete state_->displayPolicy;
                    state_->ownsDisplayPolicy = false;
                }
                state_->displayPolicy = &s_defaultPasswordDisplayPolicy;
            } else {
                if (!state_->ownsDisplayPolicy) {
                    auto* policy = new (std::nothrow) PasswordTextDisplayPolicy();
                    if (policy != nullptr) {
                        state_->displayPolicy = policy;
                        state_->ownsDisplayPolicy = true;
                    }
                }
                if (state_->ownsDisplayPolicy && state_->displayPolicy != nullptr) {
                    static_cast<void>(PasswordPolicy(state_->displayPolicy)->SetMask(mask));
                }
            }
        }
        InvalidateMeasure();
        InvalidateVisual();
        InvalidateEditor();
        if (state_ != nullptr && state_->view) {
            state_->view->InvalidateMeasure();
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
        .AddOwner(PasswordBox::PlaceholderProperty, TextBox::PlaceholderProperty, Base::String{}, AffectsMeasure | AffectsRender)
        .Factory();
}

} // namespace Aero::Controls

namespace Aero {

void Controls::TextBlock::AttachTextLayout(
    void*,
    bool invalidate) noexcept {
    if (invalidate) InvalidateDocumentText();
}

void Controls::Primitives::TextBoxBase::AttachTextLayout(
    void*,
    bool invalidate) noexcept {
    if (invalidate) InvalidateMeasure();
}

} // namespace Aero
