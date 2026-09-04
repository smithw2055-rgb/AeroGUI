#pragma once

#include <Aero/Media/Brush.hpp>

namespace Aero::Media {

class AERO_GUI_API SolidColorBrush : public Brush {
    AERO_DECLARE_TYPE(SolidColorBrush, Brush)
public:
    SolidColorBrush() noexcept
        : Brush(StaticTypeId()) {}
    explicit SolidColorBrush(Color color) noexcept
        : Brush(StaticTypeId()), initialColor_(color) {}
    ~SolidColorBrush() override = default;

    Color GetColor() const noexcept;
    void SetColor(Color value) noexcept;

    inline static constexpr DependencyProperty<Color> ColorProperty{"Color"};

private:
    Color initialColor_{};
};

AERO_GUI_API Result<Ref<Brush>>
MakeSolidColorBrush(Color color) noexcept;
} // namespace Aero::Media
