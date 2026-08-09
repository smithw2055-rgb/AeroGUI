#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Media/Brushes.hpp>

namespace Aero::Controls::Primitives {
using ::Aero::Meta::TypeId;
class AERO_GUI_API TextBoxBase : public Control {
    AERO_DECLARE_TYPE(TextBoxBase, Control)
protected:
    explicit TextBoxBase(TypeId runtimeType) noexcept
        : Control(runtimeType) {}
    ~TextBoxBase() override = default;

public:
    // WPF BaseTextBox owns the selection/caret appearance.  Keep these
    // values as brushes so authored XAML can use SolidColorBrush, gradients,
    // and dynamic resources instead of a control-specific Color facade.
    Ref<Media::Brush> GetSelectionBrush() const noexcept;
    virtual void SetSelectionBrush(
        Ref<Media::Brush> value) noexcept;
    double GetSelectionOpacity() const noexcept;
    virtual void SetSelectionOpacity(
        double value) noexcept;
    Ref<Media::Brush> GetCaretBrush() const noexcept;
    virtual void SetCaretBrush(
        Ref<Media::Brush> value) noexcept;

    inline static constexpr DependencyProperty<Ref<Media::Brush>> SelectionBrushProperty{"SelectionBrush"};
    inline static constexpr DependencyProperty<double> SelectionOpacityProperty{"SelectionOpacity"};
    inline static constexpr DependencyProperty<Ref<Media::Brush>> CaretBrushProperty{"CaretBrush"};
};
} // namespace Aero::Controls::Primitives
