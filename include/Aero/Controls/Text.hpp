#pragma once

#include <Aero/Integration/Platform.hpp>
#include <Aero/Controls/Primitives.hpp>

namespace Aero::Controls {

using ::Aero::Meta::TypeId;

class PasswordBox;

namespace Primitives {

class AERO_API TextBoxBase : public Control {
    AERO_DECLARE_TYPE(TextBoxBase, Control)
protected:
    explicit TextBoxBase(TypeId runtimeType) noexcept
        : Control(runtimeType) {}
    ~TextBoxBase() override = default;

public:
    // WPF BaseTextBox owns the selection/caret appearance.  Keep these
    // values as brushes so authored XAML can use SolidColorBrush, gradients,
    // and dynamic resources instead of a control-specific Color facade.
    Base::Ref<Media::Brush> GetSelectionBrush() const noexcept;
    virtual void SetSelectionBrush(
        Base::Ref<Media::Brush> value) noexcept;
    double GetSelectionOpacity() const noexcept;
    virtual void SetSelectionOpacity(
        double value) noexcept;
    Base::Ref<Media::Brush> GetCaretBrush() const noexcept;
    virtual void SetCaretBrush(
        Base::Ref<Media::Brush> value) noexcept;

    inline static constexpr Members::Property<Base::Ref<Media::Brush>> SelectionBrushProperty{"SelectionBrush"};
    inline static constexpr Members::Property<double> SelectionOpacityProperty{"SelectionOpacity"};
    inline static constexpr Members::Property<Base::Ref<Media::Brush>> CaretBrushProperty{"CaretBrush"};
};

} // namespace Primitives

class AERO_API TextBox
    : public Primitives::TextBoxBase,
      private IScrollInfo,
      private Integration::ITextCompositionClient {
    AERO_DECLARE_TYPE(TextBox, Primitives::TextBoxBase)
public:
    struct Impl;

    TextBox() noexcept;
    ~TextBox() override;

    Base::StringView GetText() const noexcept;
    void SetText(
        Base::StringView value) noexcept;
    bool GetIsReadOnly() const noexcept;
    void SetIsReadOnly(
        bool value) noexcept;
    std::uint32_t GetMaxLength() const noexcept;
    void SetMaxLength(
        std::uint32_t value) noexcept;
    bool GetAcceptsReturn() const noexcept;
    void SetAcceptsReturn(
        bool value) noexcept;
    TextWrapping GetTextWrapping() const noexcept;
    void SetTextWrapping(
        TextWrapping value) noexcept;
    Base::StringView GetPlaceholder() const noexcept;
    void SetPlaceholder(
        Base::StringView value) noexcept;
    Base::Ref<Media::Brush> GetPlaceholderForeground() const noexcept;
    void SetPlaceholderForeground(
        Base::Ref<Media::Brush> value) noexcept;
    double GetFontSize() const noexcept;
    void SetFontSize(
        double value) noexcept;
    Base::Ref<Media::FontFamily> GetFontFamily() const noexcept;
    void SetFontFamily(Base::Ref<Media::FontFamily> value) noexcept;
    Base::Result<void> SetFontFamily(Base::StringView value) noexcept;
    FontWeight GetFontWeight() const noexcept;
    void SetFontWeight(
        FontWeight value) noexcept;
    FontStyle GetFontStyle() const noexcept;
    void SetFontStyle(
        FontStyle value) noexcept;
    TextAlignment GetTextAlignment() const noexcept;
    void SetTextAlignment(
        TextAlignment value) noexcept;
    std::uint32_t GetMaxLines() const noexcept;
    void SetMaxLines(
        std::uint32_t value) noexcept;
    std::uint32_t GetMinLines() const noexcept;
    void SetMinLines(
        std::uint32_t value) noexcept;
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> TextChangedEvent{"TextChanged"};
    UIElement::Event<RoutedEventArgs>
        TextChanged() noexcept {
        return GetEvent(TextChangedEvent);
    }

    TextSelection GetSelection() const noexcept;
    std::uint32_t GetCaret() const noexcept;
    void SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Base::Result<void> SelectAll() noexcept;
    Base::Result<void> Undo() noexcept;
    Base::Result<void> Redo() noexcept;

    Base::Result<void> AttachScrollViewer(
        ScrollViewer* viewer) noexcept;
    void SetInputMethodHost(
        Integration::ITextInputMethodHost* host) noexcept;
    Integration::ITextInputMethodHost*
    GetInputMethodHost() const noexcept {
        return inputMethodHost_;
    }
    bool GetIsComposing() const noexcept {
        return compositionActive_;
    }
    Base::StringView GetCompositionText() const noexcept {
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

    Rect GetCaretRectangle() const noexcept;
    std::uint32_t HitTestText(
        Point position) const noexcept;

    ScrollData GetData() const noexcept override {
        return scroll_;
    }
    void SetViewport(
        Size viewport) noexcept override;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;

    inline static constexpr Members::Property<Base::String> TextProperty{"Text"};
    inline static constexpr Members::Property<bool> IsReadOnlyProperty{"IsReadOnly"};
    inline static constexpr Members::Property<std::uint32_t> MaxLengthProperty{"MaxLength"};
    inline static constexpr Members::Property<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr Members::Property<bool> AcceptsReturnProperty{"AcceptsReturn"};
    inline static constexpr Members::Property<TextWrapping> TextWrappingProperty{"TextWrapping"};
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> PlaceholderForegroundProperty{"PlaceholderForeground"};
    inline static constexpr Members::Property<double> FontSizeProperty{"FontSize"};
    inline static constexpr auto FontFamilyProperty = FrameworkElement::FontFamilyProperty;
    inline static constexpr Members::Property<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr Members::Property<FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr Members::Property<TextAlignment> TextAlignmentProperty{"TextAlignment"};
    inline static constexpr Members::Property<std::uint32_t> MaxLinesProperty{"MaxLines"};
    inline static constexpr Members::Property<std::uint32_t> MinLinesProperty{"MinLines"};
    inline static constexpr auto ForegroundProperty = Control::ForegroundProperty;

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        DrawingContext& context) noexcept override;

private:
    friend struct Impl;
    friend class PasswordBox;
    friend struct ::Aero::Controls::Control::Impl;

    struct CaretStop {
        double x = 0.0;
        double y = 0.0;
        double height = 0.0;
        std::uint32_t line = 0U;
    };

    void* model_ = nullptr;
    void* compositionModel_ = nullptr;
    void* displayPolicy_ = nullptr;
    void* plainPolicy_ = nullptr;
    Base::String displayText_;
    Base::String compositionText_;
    Base::Vector<std::uint64_t> glyphRuns_;
    Base::Vector<CaretStop> caretStops_;
    Size textSize_;
    std::uint32_t wrapColumns_ = UINT32_MAX;
    ScrollData scroll_;
    ScrollViewer* scrollViewer_ = nullptr;
    Integration::ITextInputMethodHost*
        inputMethodHost_ = nullptr;
    TextSelection compositionSelection_;
    bool serviceOwnsGlyphRuns_ = false;
    bool updatingTextProperty_ = false;
    bool compositionActive_ = false;
    bool showingPlaceholder_ = false;
    UIElement* coordinateOwner_ = nullptr;
    PasswordBox* passwordOwner_ = nullptr;
    DependencyPropertyChangedEventHandler
        textChangedHandler_;

    Base::Result<void> SynchronizeModel() noexcept;
    Base::Result<void> CommitModelText() noexcept;
    Base::Result<void> ReplaceSelection(
        Base::StringView text) noexcept;
    Base::Result<void> DeleteBackward() noexcept;
    Base::Result<void> DeleteForward() noexcept;
    Base::Result<void> CopySelection(
        Integration::IClipboard& clipboard) const noexcept;
    Base::Result<void> CutSelection(
        Integration::IClipboard& clipboard) noexcept;
    Base::Result<void> Paste(
        Integration::IClipboard& clipboard) noexcept;
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
    Base::Result<void> ConstrainManualInput(
        Base::String& input,
        const void* target,
        TextSelection selection) const noexcept;
    void ReleaseGlyphRuns() noexcept;
    double GetLineHeight() const noexcept;
    const void*
    GetActiveModel() const noexcept;
    Base::Result<void>
    UpdateCandidateWindow() noexcept;
    Base::Result<void>
    CancelCompositionForFocusLoss() noexcept;
    Base::Result<void> RenderEditor(
        DrawingContext& context,
        Size viewport,
        bool drawCaret) noexcept;
    void OnTextPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API PasswordBox : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(PasswordBox, Primitives::TextBoxBase)
public:
    struct Impl;

    PasswordBox() noexcept;
    ~PasswordBox() override;

    Base::StringView GetPassword() const noexcept {
        return password_.View();
    }
    void SetPassword(
        Base::StringView value) noexcept;
    Base::StringView GetPasswordChar() const noexcept;
    void SetPasswordChar(
        Base::StringView value) noexcept;
    std::uint32_t GetMaxLength() const noexcept;
    void SetMaxLength(
        std::uint32_t value) noexcept;
    void SetSelectionBrush(
        Base::Ref<Media::Brush> value) noexcept override;
    void SetSelectionOpacity(
        double value) noexcept override;
    void SetCaretBrush(
        Base::Ref<Media::Brush> value) noexcept override;
    TextSelection GetSelection() const noexcept;
    std::uint32_t GetCaret() const noexcept;
    void SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Base::Result<void> SelectAll() noexcept;
    void SetInputMethodHost(
        Integration::ITextInputMethodHost* host) noexcept;
    Integration::ITextInputMethodHost*
    GetInputMethodHost() const noexcept;
    bool GetIsComposing() const noexcept;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> PasswordChangedEvent{"PasswordChanged"};
    UIElement::Event<RoutedEventArgs>
        PasswordChanged() noexcept {
        return GetEvent(PasswordChangedEvent);
    }
    inline static constexpr Members::Property<Base::String> PasswordCharProperty{"PasswordChar"};
    inline static constexpr Members::Property<std::uint32_t> MaxLengthProperty{"MaxLength"};
    inline static constexpr Members::Property<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr auto ForegroundProperty = Control::ForegroundProperty;

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        DrawingContext& context) noexcept override;

private:
    friend class TextBox;
    friend struct ::Aero::Controls::TextBox::Impl;
    friend struct ::Aero::Controls::Control::Impl;
    Base::String password_;
    void* validation_ = nullptr;
    void* passwordPolicy_ = nullptr;
    TextBox editor_;
    bool synchronizingEditor_ = false;

    Base::Result<void>
        SynchronizeEditorFromPassword() noexcept;
    Base::Result<void>
        SynchronizePasswordFromEditor() noexcept;
};


} // namespace Aero::Controls
