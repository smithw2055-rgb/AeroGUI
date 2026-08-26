#pragma once

#include <Aero/Media/Brush.hpp>
#include <Aero/Media/Images.hpp>

namespace Aero::Media {

class AERO_GUI_API VisualBrush : public Brush {
    AERO_DECLARE_TYPE(VisualBrush, Brush)
public:
    VisualBrush() noexcept
        : Brush(StaticTypeId()) {}
    ~VisualBrush() override = default;

    Ref<Base::Object> GetVisual() const noexcept {
        return GetValueOr(
            VisualProperty, Ref<Base::Object>{});
    }
    void SetVisual(
        Ref<Base::Object> value) noexcept {
        SetValue(VisualProperty, std::move(value));
    }
    Stretch GetStretch() const noexcept {
        return GetValueOr(StretchProperty, Stretch::Fill);
    }
    void SetStretch(Stretch value) noexcept {
        SetValue(StretchProperty, value);
    }
    Rect GetViewbox() const noexcept {
        return GetValueOr(
            ViewboxProperty, Rect{0.0, 0.0, 1.0, 1.0});
    }
    void SetViewbox(Rect value) noexcept {
        SetValue(ViewboxProperty, value);
    }
    VerticalAlignment GetAlignmentY() const noexcept {
        return GetValueOr(
            AlignmentYProperty, VerticalAlignment::Center);
    }
    void SetAlignmentY(
        VerticalAlignment value) noexcept {
        SetValue(AlignmentYProperty, value);
    }

    inline static constexpr DependencyProperty<Ref<Base::Object>> VisualProperty{"Visual"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};
    inline static constexpr DependencyProperty<Rect> ViewboxProperty{"Viewbox"};
    inline static constexpr DependencyProperty<VerticalAlignment> AlignmentYProperty{"AlignmentY"};
};
} // namespace Aero::Media
