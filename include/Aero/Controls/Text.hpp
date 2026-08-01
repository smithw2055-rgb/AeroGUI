#pragma once

#include <Aero/Integration/PlatformServices.hpp>
#include <Aero/Text/EditableText.hpp>
#include <Aero/Controls/Primitives.hpp>

namespace Aero::Detail { class ControlRuntimeAccess; }

namespace Aero::Controls {

namespace Detail {
class TextLayoutService;
class TextServicesAccess;
}

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

class PasswordBox;

namespace Primitives {

class AERO_API TextBoxBase : public Control {
    AERO_DECLARE_TYPE(TextBoxBase, Control)
protected:
    explicit TextBoxBase(TypeId runtimeType) noexcept
        : Control(runtimeType) {}
    ~TextBoxBase() override = default;
};

} // namespace Primitives

class AERO_API TextBox final
    : public Primitives::TextBoxBase,
      public IScrollInfo,
      public Integration::ITextCompositionClient {
    AERO_DECLARE_TYPE(TextBox, Primitives::TextBoxBase)
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
    Text::TextWrapping TextWrapping() const noexcept;
    Base::Result<void> SetTextWrapping(
        Text::TextWrapping value) noexcept;
    Base::StringView Placeholder() const noexcept;
    Base::Result<void> SetPlaceholder(
        Base::StringView value) noexcept;
    Color PlaceholderForeground() const noexcept;
    Base::Result<void> SetPlaceholderForeground(
        Color value) noexcept;
    double FontSize() const noexcept;
    Base::Result<void> SetFontSize(
        double value) noexcept;
    Base::StringView FontFamily() const noexcept;
    Base::Result<void> SetFontFamily(
        Base::StringView value) noexcept;
    FontWeight GetFontWeight() const noexcept;
    Base::Result<void> SetFontWeight(
        FontWeight value) noexcept;
    Text::FontStyle GetFontStyle() const noexcept;
    Base::Result<void> SetFontStyle(
        Text::FontStyle value) noexcept;
    Text::TextAlignment TextAlignment() const noexcept;
    Base::Result<void> SetTextAlignment(
        Text::TextAlignment value) noexcept;
    std::uint32_t MaxLines() const noexcept;
    Base::Result<void> SetMaxLines(
        std::uint32_t value) noexcept;
    std::uint32_t MinLines() const noexcept;
    Base::Result<void> SetMinLines(
        std::uint32_t value) noexcept;
    Color Foreground() const noexcept;
    Color SelectionBrush() const noexcept;
    double SelectionOpacity() const noexcept;
    Color CaretBrush() const noexcept;
    Base::Result<void> SetForeground(
        Color value) noexcept;
    Base::Result<void> SetSelectionBrush(
        Color value) noexcept;
    Base::Result<void> SetSelectionOpacity(
        double value) noexcept;
    Base::Result<void> SetCaretBrush(
        Color value) noexcept;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> TextChangedEvent{"TextChanged"};
    UIElement::Event<RoutedEventArgs>
        TextChanged() noexcept {
        return GetEvent(TextChangedEvent);
    }

    Text::TextSelection Selection() const noexcept;
    std::uint32_t Caret() const noexcept;
    Base::Result<void> SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Base::Result<void> SelectAll() noexcept;
    Base::Result<void> Undo() noexcept;
    Base::Result<void> Redo() noexcept;

    Base::Result<void> SetDisplayPolicy(
        ITextDisplayPolicy* policy) noexcept;
    ITextDisplayPolicy* DisplayPolicy() const noexcept {
        return displayPolicy_;
    }
    Base::Result<void> AttachScrollViewer(
        ScrollViewer* viewer) noexcept;
    Base::Result<void> SetInputMethodHost(
        Integration::ITextInputMethodHost* host) noexcept;
    Integration::ITextInputMethodHost*
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

    inline static constexpr Members::Property<Base::String> TextProperty{"Text"};
    inline static constexpr Members::Property<bool> IsReadOnlyProperty{"IsReadOnly"};
    inline static constexpr Members::Property<std::uint32_t> MaxLengthProperty{"MaxLength"};
    inline static constexpr Members::Property<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr auto MaximumLengthProperty = MaxLengthProperty;
    inline static constexpr Members::Property<bool> AcceptsReturnProperty{"AcceptsReturn"};
    inline static constexpr Members::Property<Text::TextWrapping> TextWrappingProperty{"TextWrapping"};
    inline static constexpr Members::Property<Aero::Media::Color> PlaceholderForegroundProperty{"PlaceholderForeground"};
    inline static constexpr Members::Property<double> FontSizeProperty{"FontSize"};
    inline static constexpr auto FontFamilyProperty = FrameworkElement::FontFamilyProperty;
    inline static constexpr Members::Property<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr Members::Property<Text::FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr Members::Property<Text::TextAlignment> TextAlignmentProperty{"TextAlignment"};
    inline static constexpr Members::Property<std::uint32_t> MaxLinesProperty{"MaxLines"};
    inline static constexpr Members::Property<std::uint32_t> MinLinesProperty{"MinLines"};
    inline static constexpr Members::Property<Aero::Media::Color> ForegroundProperty{"Foreground"};
    inline static constexpr Members::Property<Aero::Media::Color> SelectionBrushProperty{"SelectionBrush"};
    inline static constexpr Members::Property<double> SelectionOpacityProperty{"SelectionOpacity"};
    inline static constexpr Members::Property<Aero::Media::Color> CaretBrushProperty{"CaretBrush"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;
    Base::Result<void> OnRender(
        DrawingContext& context) noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    friend class PasswordBox;
    friend class Detail::TextServicesAccess;

    struct CaretStop final {
        double x = 0.0;
        double y = 0.0;
        double height = 0.0;
        std::uint32_t line = 0U;
    };

    Text::EditableTextModel model_;
    Text::EditableTextModel compositionModel_;
    Detail::TextLayoutService* layoutService_ = nullptr;
    ITextDisplayPolicy* displayPolicy_ = nullptr;
    PlainTextDisplayPolicy plainPolicy_;
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
    Text::TextSelection compositionSelection_;
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
        const Text::EditableTextModel& target,
        Text::TextSelection selection) const noexcept;
    void ReleaseGlyphRuns() noexcept;
    double LineHeight() const noexcept;
    const Text::EditableTextModel&
    ActiveModel() const noexcept;
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

class AERO_API PasswordBox final : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(PasswordBox, Primitives::TextBoxBase)
public:
    PasswordBox() noexcept;
    ~PasswordBox() override = default;

    Base::StringView Password() const noexcept {
        return password_.View();
    }
    Base::Result<void> SetPassword(
        Base::StringView value) noexcept;
    Base::StringView PasswordChar() const noexcept;
    Base::Result<void> SetPasswordChar(
        Base::StringView value) noexcept;
    std::uint32_t MaximumLength() const noexcept;
    Base::Result<void> SetMaximumLength(
        std::uint32_t value) noexcept;
    Color Foreground() const noexcept;
    Color SelectionBrush() const noexcept;
    double SelectionOpacity() const noexcept;
    Color CaretBrush() const noexcept;
    Base::Result<void> SetForeground(
        Color value) noexcept;
    Base::Result<void> SetSelectionBrush(
        Color value) noexcept;
    Base::Result<void> SetSelectionOpacity(
        double value) noexcept;
    Base::Result<void> SetCaretBrush(
        Color value) noexcept;
    Text::TextSelection Selection() const noexcept;
    std::uint32_t Caret() const noexcept;
    Base::Result<void> SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Base::Result<void> SelectAll() noexcept;
    Base::Result<void> SetInputMethodHost(
        Integration::ITextInputMethodHost* host) noexcept;
    Integration::ITextInputMethodHost*
    InputMethodHost() const noexcept;
    bool IsComposing() const noexcept;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> PasswordChangedEvent{"PasswordChanged"};
    UIElement::Event<RoutedEventArgs>
        PasswordChanged() noexcept {
        return GetEvent(PasswordChangedEvent);
    }
    inline static constexpr Members::Property<Base::String> PasswordCharProperty{"PasswordChar"};
    inline static constexpr Members::Property<std::uint32_t> MaxLengthProperty{"MaxLength"};
    inline static constexpr auto MaximumLengthProperty = MaxLengthProperty;
    inline static constexpr Members::Property<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr Members::Property<Aero::Media::Color> ForegroundProperty{"Foreground"};
    inline static constexpr Members::Property<Aero::Media::Color> SelectionBrushProperty{"SelectionBrush"};
    inline static constexpr Members::Property<double> SelectionOpacityProperty{"SelectionOpacity"};
    inline static constexpr Members::Property<Aero::Media::Color> CaretBrushProperty{"CaretBrush"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;
    Base::Result<void> OnRender(
        DrawingContext& context) noexcept override;

private:
    friend class TextBox;
    friend class Aero::Detail::ControlRuntimeAccess;
    friend class Detail::TextServicesAccess;
    Base::String password_;
    Text::EditableTextModel validation_;
    PasswordTextDisplayPolicy passwordPolicy_;
    TextBox editor_;
    bool synchronizingEditor_ = false;

    Base::Result<void>
        SynchronizeEditorFromPassword() noexcept;
    Base::Result<void>
        SynchronizePasswordFromEditor() noexcept;
};


} // namespace Aero::Controls
