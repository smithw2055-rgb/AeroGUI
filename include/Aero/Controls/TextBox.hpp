#pragma once

#include <Aero/InputInterop.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/TextFormatting.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>

namespace Aero::Controls {
class PasswordBox;
class TextBox;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::PropertyValue;
using ::Aero::Meta::TypeId;

namespace Primitives {

class TextBoxView;

class AERO_GUI_API TextBoxBase : public Control,
      public IScrollInfo,
      public Input::ITextCompositionClient {
    AERO_DECLARE_TYPE(TextBoxBase, Control)

public:
    Ref<Media::Brush> GetSelectionBrush() const noexcept;
    virtual void SetSelectionBrush(Ref<Media::Brush> value) noexcept;
    double GetSelectionOpacity() const noexcept;
    virtual void SetSelectionOpacity(double value) noexcept;
    Ref<Media::Brush> GetCaretBrush() const noexcept;
    virtual void SetCaretBrush(Ref<Media::Brush> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<Media::Brush>, SelectionBrush);
    AERO_DEPENDENCY_PROPERTY(double, SelectionOpacity);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Brush>, CaretBrush);

    void AttachTextLayout(void* service, bool invalidate = false) noexcept;
    virtual void SelectAll() noexcept;
    virtual void Undo() noexcept;
    virtual void Redo() noexcept;
    void AttachScrollViewer(ScrollViewer* viewer) noexcept;
    Result<void> BeginComposition() noexcept override;
    Result<void> UpdateComposition(StringView text) noexcept override;
    Result<void> CommitComposition(StringView text) noexcept override;
    Result<void> CancelComposition() noexcept override;
    std::uint32_t HitTestText(Point position) const noexcept;
    bool LineHorizontal(double direction) noexcept override;
    bool LineVertical(double direction) noexcept override;
    bool PageHorizontal(double direction) noexcept override;
    bool PageVertical(double direction) noexcept override;

    TextSelection GetSelection() const noexcept;
    virtual void SetSelection(std::uint32_t anchor, std::uint32_t caret) noexcept;
    std::uint32_t GetCaret() const noexcept;
    Input::ITextInputMethodHost* GetInputMethodHost() const noexcept { return inputMethodHost_; }
    virtual void SetInputMethodHost(Input::ITextInputMethodHost* host) noexcept;
    bool GetIsComposing() const noexcept { return compositionActive_; }
    StringView GetCompositionText() const noexcept { return compositionText_.View(); }
    Rect GetCaretRectangle() const noexcept;
    ScrollData GetData() const noexcept override { return scroll_; }
    void SetViewport(Size viewport) noexcept override;
    void SetHorizontalOffset(double value) noexcept override;
    void SetVerticalOffset(double value) noexcept override;

protected:
    struct DragSelectionState {
        std::uint32_t pointerId = 0U;
        std::uint32_t dragAnchor = 0U;
        bool isDragging = false;
    };

    struct CaretStop {
        double x = 0.0;
        double y = 0.0;
        double height = 0.0;
        std::uint32_t line = 0U;
    };

    explicit TextBoxBase(TypeId runtimeType) noexcept;
    ~TextBoxBase() override;

    void OnApplyTemplate() noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
    void OnMouseDown(MouseButtonEventArgs& args);
    void OnMouseMove(MouseEventArgs& args);
    void OnMouseUp(MouseButtonEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);
    void OnTextInput(TextCompositionEventArgs& args);
    void OnLostKeyboardFocus(KeyboardFocusChangedEventArgs& args);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

    virtual void CommitModelText() noexcept;
    virtual bool GetAcceptsReturnCore() const noexcept { return false; }
    virtual bool GetIsReadOnlyCore() const noexcept { return false; }
    virtual std::uint32_t GetMaxLengthCore() const noexcept { return 0U; }
    virtual TextWrapping GetTextWrappingCore() const noexcept { return TextWrapping::NoWrap; }
    virtual TextAlignment GetTextAlignmentCore() const noexcept { return TextAlignment::Left; }
    virtual std::uint32_t GetMaxLinesCore() const noexcept { return 0U; }
    virtual std::uint32_t GetMinLinesCore() const noexcept { return 1U; }
    virtual StringView GetPlaceholderCore() const noexcept { return {}; }
    virtual Ref<Media::Brush> GetPlaceholderForegroundCore() const noexcept { return {}; }
    virtual FontStyle GetFontStyleCore() const noexcept { return FontStyle::Normal; }

    void ReplaceSelection(StringView text) noexcept;
    void DeleteBackward() noexcept;
    void DeleteForward() noexcept;
    void CopySelection(Input::IClipboard& clipboard) const noexcept;
    void CutSelection(Input::IClipboard& clipboard) noexcept;
    void Paste(Input::IClipboard& clipboard) noexcept;
    void SelectedText(String& output) const noexcept;
    void MoveCaretHorizontal(double direction, bool extend) noexcept;
    void MoveCaretLineBoundary(bool end, bool extend) noexcept;
    void EnsureCaretVisible() noexcept;
    void RebuildCaretStops() noexcept;
    void SanitizeInput(StringView input, String& output) const noexcept;
    void ConstrainManualInput(String& input, const void* target, TextSelection selection) const noexcept;
    void ReleaseGlyphRuns() noexcept;
    void UpdateCandidateWindow() noexcept;
    void CancelCompositionForFocusLoss() noexcept;
    void RenderEditor(::Aero::Media::DrawingContext& context, Size viewport, bool drawCaret, bool isContentHost = false) noexcept;
    void InvalidateEditor() noexcept;

    double GetLineHeight() const noexcept;
    const void* GetActiveModel() const noexcept;

    void* model_ = nullptr;
    void* compositionModel_ = nullptr;
    void* displayPolicy_ = nullptr;
    String displayText_;
    String compositionText_;
    Base::Vector<std::uint64_t> glyphRuns_;
    Base::Vector<CaretStop> caretStops_;
    Size textSize_;
    std::uint32_t wrapColumns_ = UINT32_MAX;
    ScrollData scroll_;
    ScrollViewer* scrollViewer_ = nullptr;
    Input::ITextInputMethodHost* inputMethodHost_ = nullptr;
    TextSelection compositionSelection_;
    bool serviceOwnsGlyphRuns_ = false;
    bool compositionActive_ = false;
    bool showingPlaceholder_ = false;
    Ref<UIElement> view_;
    DragSelectionState drag_;

    friend class TextBoxView;
};

class AERO_GUI_API TextBoxView : public FrameworkElement {
    AERO_DECLARE_TYPE(TextBoxView, FrameworkElement)

public:
    TextBoxView() noexcept;
    explicit TextBoxView(TextBoxBase* owner) noexcept;
    ~TextBoxView() override = default;

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    TextBoxBase* owner_ = nullptr;
};

} // namespace Primitives

class AERO_GUI_API TextBox : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(TextBox, Primitives::TextBoxBase)

public:
    TextBox() noexcept;
    ~TextBox() override;

    StringView GetText() const noexcept;
    void SetText(StringView value) noexcept;
    bool GetIsReadOnly() const noexcept;
    void SetIsReadOnly(bool value) noexcept;
    std::uint32_t GetMaxLength() const noexcept;
    void SetMaxLength(std::uint32_t value) noexcept;
    bool GetAcceptsReturn() const noexcept;
    void SetAcceptsReturn(bool value) noexcept;
    TextWrapping GetTextWrapping() const noexcept;
    void SetTextWrapping(TextWrapping value) noexcept;
    StringView GetPlaceholder() const noexcept;
    void SetPlaceholder(StringView value) noexcept;
    Ref<Media::Brush> GetPlaceholderForeground() const noexcept;
    void SetPlaceholderForeground(Ref<Media::Brush> value) noexcept;
    double GetFontSize() const noexcept;
    void SetFontSize(double value) noexcept;
    Ref<Media::FontFamily> GetFontFamily() const noexcept;
    void SetFontFamily(Ref<Media::FontFamily> value) noexcept;
    void SetFontFamily(StringView value) noexcept;
    FontWeight GetFontWeight() const noexcept;
    void SetFontWeight(FontWeight value) noexcept;
    FontStyle GetFontStyle() const noexcept;
    void SetFontStyle(FontStyle value) noexcept;
    TextAlignment GetTextAlignment() const noexcept;
    void SetTextAlignment(TextAlignment value) noexcept;
    std::uint32_t GetMaxLines() const noexcept;
    void SetMaxLines(std::uint32_t value) noexcept;
    std::uint32_t GetMinLines() const noexcept;
    void SetMinLines(std::uint32_t value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> TextChangedEvent{"TextChanged"};
    UIElement::Event<RoutedEventArgs> TextChanged() noexcept { return GetEvent(TextChangedEvent); }

    AERO_DEPENDENCY_PROPERTY(String, Text);
    AERO_DEPENDENCY_PROPERTY(bool, IsReadOnly);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, MaxLength);
    AERO_DEPENDENCY_PROPERTY(String, Placeholder);
    AERO_DEPENDENCY_PROPERTY(bool, AcceptsReturn);
    AERO_DEPENDENCY_PROPERTY(TextWrapping, TextWrapping);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, PlaceholderForeground);
    AERO_DEPENDENCY_PROPERTY(double, FontSize);
    inline static constexpr auto FontFamilyProperty = FrameworkElement::FontFamilyProperty;
    AERO_DEPENDENCY_PROPERTY(FontWeight, FontWeight);
    AERO_DEPENDENCY_PROPERTY(FontStyle, FontStyle);
    AERO_DEPENDENCY_PROPERTY(TextAlignment, TextAlignment);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, MaxLines);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, MinLines);
    inline static constexpr auto ForegroundProperty = Control::ForegroundProperty;

protected:
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    PropertyValue CoerceValueCore(DependencyPropertyHandle property, const PropertyValue& baseValue) noexcept override;

    void CommitModelText() noexcept override;
    bool GetAcceptsReturnCore() const noexcept override { return GetAcceptsReturn(); }
    bool GetIsReadOnlyCore() const noexcept override { return GetIsReadOnly(); }
    std::uint32_t GetMaxLengthCore() const noexcept override { return GetMaxLength(); }
    TextWrapping GetTextWrappingCore() const noexcept override { return GetTextWrapping(); }
    TextAlignment GetTextAlignmentCore() const noexcept override { return GetTextAlignment(); }
    std::uint32_t GetMaxLinesCore() const noexcept override { return GetMaxLines(); }
    std::uint32_t GetMinLinesCore() const noexcept override { return GetMinLines(); }
    StringView GetPlaceholderCore() const noexcept override { return GetPlaceholder(); }
    Ref<Media::Brush> GetPlaceholderForegroundCore() const noexcept override { return GetPlaceholderForeground(); }
    FontStyle GetFontStyleCore() const noexcept override { return GetFontStyle(); }

private:
    void* plainPolicy_ = nullptr;
    bool updatingTextProperty_ = false;

    Result<void> SynchronizeModel() noexcept;
    void OnTextPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept;
};

class AERO_GUI_API PasswordBox : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(PasswordBox, Primitives::TextBoxBase)

public:
    PasswordBox() noexcept;
    ~PasswordBox() override;

    StringView GetPassword() const noexcept { return password_.View(); }
    void SetPassword(StringView value) noexcept;
    StringView GetPasswordChar() const noexcept;
    void SetPasswordChar(StringView value) noexcept;
    std::uint32_t GetMaxLength() const noexcept;
    void SetMaxLength(std::uint32_t value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> PasswordChangedEvent{"PasswordChanged"};
    UIElement::Event<RoutedEventArgs> PasswordChanged() noexcept { return GetEvent(PasswordChangedEvent); }
    AERO_DEPENDENCY_PROPERTY(String, PasswordChar);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, MaxLength);
    AERO_DEPENDENCY_PROPERTY(String, Placeholder);
    StringView GetPlaceholder() const noexcept { return GetValue(PlaceholderProperty); }
    void SetPlaceholder(StringView value) noexcept { SetValue(PlaceholderProperty, value); }
    inline static constexpr auto ForegroundProperty = Control::ForegroundProperty;

protected:
    bool ValidateValueCore(Meta::DependencyPropertyHandle property, const PropertyValue& value) const noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

    void CommitModelText() noexcept override;
    std::uint32_t GetMaxLengthCore() const noexcept override { return GetMaxLength(); }
    StringView GetPlaceholderCore() const noexcept override { return GetPlaceholder(); }

private:
    String password_;
    void* passwordPolicy_ = nullptr;
};

} // namespace Aero::Controls
