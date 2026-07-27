#pragma once

#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/TextBlockLayoutService.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Platform/Ime.hpp>
#include <Aero/Text/EditableText.hpp>
#include <Aero/Type.hpp>

namespace Aero::Controls {

class AERO_API ITextDisplayPolicy {
public:
    virtual ~ITextDisplayPolicy() = default;

    virtual Base::Result<void> BuildDisplayText(
        const Text::EditableTextModel& model,
        Base::String& output) noexcept = 0;
    virtual bool AllowsCopy() const noexcept = 0;
    virtual bool AllowsCut() const noexcept = 0;
};

class AERO_API PlainTextDisplayPolicy final
    : public ITextDisplayPolicy {
public:
    Base::Result<void> BuildDisplayText(
        const Text::EditableTextModel& model,
        Base::String& output) noexcept override;
    bool AllowsCopy() const noexcept override {
        return true;
    }
    bool AllowsCut() const noexcept override {
        return true;
    }
};

class AERO_API PasswordTextDisplayPolicy final
    : public ITextDisplayPolicy {
public:
    explicit PasswordTextDisplayPolicy(
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<void> SetMask(
        Base::StringView value) noexcept;
    Base::StringView Mask() const noexcept {
        return mask_.View();
    }
    Base::Result<void> BuildDisplayText(
        const Text::EditableTextModel& model,
        Base::String& output) noexcept override;
    bool AllowsCopy() const noexcept override {
        return false;
    }
    bool AllowsCut() const noexcept override {
        return false;
    }

private:
    Base::String mask_;
};

class TextBoxInteractionManager;

class AERO_API TextBox final
    : public FrameworkElement,
      public IScrollInfo,
      public Platform::ITextCompositionClient {
    AERO_DECLARE_TYPE(TextBox, FrameworkElement)
public:
    TextBox() noexcept;
    ~TextBox() override;

    Base::StringView Text() const noexcept;
    Base::Result<void> SetText(
        Base::StringView value) noexcept;
    bool IsReadOnly() const noexcept;
    Base::Result<void> SetReadOnly(
        bool value) noexcept;
    std::uint32_t MaximumLength() const noexcept;
    Base::Result<void> SetMaximumLength(
        std::uint32_t value) noexcept;
    bool AcceptsReturn() const noexcept;
    Base::Result<void> SetAcceptsReturn(
        bool value) noexcept;
    Color Foreground() const noexcept;
    Color SelectionBrush() const noexcept;
    Color CaretBrush() const noexcept;
    Base::Result<void> SetForeground(
        Color value) noexcept;
    Base::Result<void> SetSelectionBrush(
        Color value) noexcept;
    Base::Result<void> SetCaretBrush(
        Color value) noexcept;

    Text::TextSelection Selection() const noexcept;
    std::uint32_t Caret() const noexcept;
    Base::Result<void> SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Base::Result<void> SelectAll() noexcept;
    Base::Result<void> Undo() noexcept;
    Base::Result<void> Redo() noexcept;

    Base::Result<void> SetLayoutService(
        ITextBlockLayoutService* service) noexcept;
    ITextBlockLayoutService* LayoutService() const noexcept {
        return layoutService_;
    }
    Base::Result<void> SetDisplayPolicy(
        ITextDisplayPolicy* policy) noexcept;
    ITextDisplayPolicy* DisplayPolicy() const noexcept {
        return displayPolicy_;
    }
    Base::Result<void> AttachScrollViewer(
        ScrollViewer* viewer) noexcept;
    Base::Result<void> SetInputMethodHost(
        Platform::ITextInputMethodHost* host) noexcept;
    Platform::ITextInputMethodHost*
    InputMethodHost() const noexcept {
        return inputMethodHost_;
    }
    bool IsComposing() const noexcept {
        return compositionActive_;
    }
    Base::StringView CompositionText() const noexcept {
        return compositionText_.View();
    }

    Base::Result<void>
    BeginComposition() noexcept override;
    Base::Result<void> UpdateComposition(
        Base::StringView text) noexcept override;
    Base::Result<void> CommitComposition(
        Base::StringView text) noexcept override;
    Base::Result<void>
    CancelComposition() noexcept override;

    Rect CaretRectangle() const noexcept;
    std::uint32_t HitTestText(
        Point position) const noexcept;

    ScrollData Data() const noexcept override {
        return scroll_;
    }
    Base::Result<bool> SetViewport(
        Size viewport) noexcept override;
    Base::Result<bool> SetHorizontalOffset(
        double value) noexcept override;
    Base::Result<bool> SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;
    inline static constexpr auto TextProperty =
        Members::Property<Base::String>{"Text"};
    inline static constexpr auto IsReadOnlyProperty =
        Members::Property<bool>{"IsReadOnly"};
    inline static constexpr auto MaximumLengthProperty =
        Members::Property<std::uint32_t>{"MaximumLength"};
    inline static constexpr auto AcceptsReturnProperty =
        Members::Property<bool>{"AcceptsReturn"};
    inline static constexpr auto ForegroundProperty =
        Members::Property<Color>{"Foreground"};
    inline static constexpr auto SelectionBrushProperty =
        Members::Property<Color>{"SelectionBrush"};
    inline static constexpr auto CaretBrushProperty =
        Members::Property<Color>{"CaretBrush"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;
    Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;

private:
    friend class TextBoxInteractionManager;

    struct CaretStop final {
        double x = 0.0;
        double y = 0.0;
        double height = 0.0;
        std::uint32_t line = 0U;
    };

    Text::EditableTextModel model_;
    Text::EditableTextModel compositionModel_;
    ITextBlockLayoutService* layoutService_ = nullptr;
    ITextDisplayPolicy* displayPolicy_ = nullptr;
    PlainTextDisplayPolicy plainPolicy_;
    Base::String displayText_;
    Base::String compositionText_;
    Base::Vector<RenderGlyphRunId> glyphRuns_;
    Base::Vector<CaretStop> caretStops_;
    Size textSize_;
    ScrollData scroll_;
    ScrollViewer* scrollViewer_ = nullptr;
    Platform::ITextInputMethodHost*
        inputMethodHost_ = nullptr;
    Text::TextSelection compositionSelection_;
    bool serviceOwnsGlyphRuns_ = false;
    bool updatingTextProperty_ = false;
    bool compositionActive_ = false;

    Base::Result<void> SynchronizeModel() noexcept;
    Base::Result<void> CommitModelText() noexcept;
    Base::Result<void> ReplaceSelection(
        Base::StringView text) noexcept;
    Base::Result<void> DeleteBackward() noexcept;
    Base::Result<void> DeleteForward() noexcept;
    Base::Result<void> CopySelection(
        Platform::IClipboard& clipboard) const noexcept;
    Base::Result<void> CutSelection(
        Platform::IClipboard& clipboard) noexcept;
    Base::Result<void> Paste(
        Platform::IClipboard& clipboard) noexcept;
    Base::Result<void> SelectedText(
        Base::String& output) const noexcept;
    Base::Result<void> MoveCaretHorizontal(
        double direction,
        bool extend) noexcept;
    Base::Result<void> MoveCaretLineBoundary(
        bool end,
        bool extend) noexcept;
    Base::Result<void> EnsureCaretVisible() noexcept;
    Base::Result<void> RebuildCaretStops() noexcept;
    Base::Result<void> SanitizeInput(
        Base::StringView input,
        Base::String& output) const noexcept;
    void ReleaseGlyphRuns() noexcept;
    double LineHeight() const noexcept;
    const Text::EditableTextModel&
    ActiveModel() const noexcept;
    Base::Result<void>
    UpdateCandidateWindow() noexcept;
    Base::Result<void>
    CancelCompositionForFocusLoss() noexcept;
};

class AERO_API TextBoxInteractionManager final {
public:
    TextBoxInteractionManager(
        ObjectTree& tree,
        RoutedEventManager& events,
        PointerInputManager& pointer,
        FocusManager& focus,
        Platform::IClipboard& clipboard) noexcept;
    ~TextBoxInteractionManager() noexcept;

    Base::Result<void> Attach(
        TextBox& textBox) noexcept;
    Base::Result<bool> Detach(
        TextBox& textBox) noexcept;

private:
    struct Record final {
        VisualHandle handle;
        std::uint32_t pointerId = 0U;
        std::uint32_t anchor = 0U;
        bool dragging = false;
    };

    ObjectTree* tree_ = nullptr;
    [[maybe_unused]] RoutedEventManager* events_ = nullptr;
    PointerInputManager* pointer_ = nullptr;
    FocusManager* focus_ = nullptr;
    Platform::IClipboard* clipboard_ = nullptr;
    Base::Vector<Record> records_;
    MouseButtonEventHandler mouseDownHandler_;
    MouseEventHandler mouseMoveHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    TextCompositionEventHandler textInputHandler_;
    KeyboardFocusChangedEventHandler focusChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    PointerCaptureChangedHandler captureChangedHandler_;
    bool captureSubscribed_ = false;

    std::uint32_t Find(
        const TextBox& textBox) const noexcept;
    TextBox* Resolve(
        std::uint32_t index) noexcept;
    void RemoveAt(
        std::uint32_t index) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        const MouseButtonEventArgs& args) noexcept;
    void OnMouseMove(
        Base::Object* sender,
        const MouseEventArgs& args) noexcept;
    void OnMouseUp(
        Base::Object* sender,
        const MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        const KeyEventArgs& args) noexcept;
    void OnTextInput(
        Base::Object* sender,
        const TextCompositionEventArgs& args) noexcept;
    void OnFocusChanged(
        Base::Object* sender,
        const KeyboardFocusChangedEventArgs& args) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnCaptureChanged(
        std::uint32_t pointerId,
        UIElement* target,
        bool captured) noexcept;
};

} // namespace Aero::Controls
