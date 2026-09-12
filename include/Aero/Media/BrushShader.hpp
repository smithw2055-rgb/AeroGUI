#pragma once

#include <Aero/DependencyProperty.hpp>
#include <Aero/Media/Brush.hpp>

namespace Aero::Media {

class AERO_GUI_API BrushShader : public DependencyObject {
    AERO_DECLARE_TYPE(BrushShader, DependencyObject)
public:
    BrushShader() noexcept : BrushShader(StaticTypeId()) {}
    ~BrushShader() override = default;
protected:
    explicit BrushShader(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
};
} // namespace Aero::Media
