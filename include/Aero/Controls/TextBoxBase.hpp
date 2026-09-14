#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Media/Brushes.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls::Primitives {
using ::Aero::Meta::TypeId;
class AERO_GUI_API TextBoxBase : public Control {
    AERO_DECLARE_TYPE(TextBoxBase, Control)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

protected:
    struct DragSelectionState {
        std::uint32_t pointerId = 0U;
        std::uint32_t dragAnchor = 0U;
        bool isDragging = false;
    };
    DragSelectionState drag_;

    explicit TextBoxBase(TypeId runtimeType) noexcept
        : Control(runtimeType) {}
    ~TextBoxBase() override = default;

public:
    // WPF BaseTextBox owns the selection/caret appearance.  Keep these
    // values as brushes so authored XAML can use SolidColorBrush, gradients,
    // and dynamic resources instead of a control-specific Color facade.
    Ref<Media::Brush> GetSelectionBrush() const noexcept;
    virtual void SetSelectionBrush(Ref<Media::Brush> value) noexcept;
    double GetSelectionOpacity() const noexcept;
    virtual void SetSelectionOpacity(double value) noexcept;
    Ref<Media::Brush> GetCaretBrush() const noexcept;
    virtual void SetCaretBrush(Ref<Media::Brush> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<Media::Brush>, SelectionBrush);
    AERO_DEPENDENCY_PROPERTY(double, SelectionOpacity);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Brush>, CaretBrush);
};
} // namespace Aero::Controls::Primitives
