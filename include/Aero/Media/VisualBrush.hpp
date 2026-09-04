#pragma once

#include <Aero/Media/TileBrush.hpp>

namespace Aero::Media {

class AERO_GUI_API VisualBrush : public TileBrush {
    AERO_DECLARE_TYPE(VisualBrush, TileBrush)
public:
    VisualBrush() noexcept
        : TileBrush(StaticTypeId()) {}
    ~VisualBrush() override = default;

    Ref<Base::Object> GetVisual() const noexcept {
        return GetValue(VisualProperty);
    }
    void SetVisual(Ref<Base::Object> value) noexcept {
        SetValue(VisualProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<Ref<Base::Object>> VisualProperty{"Visual"};
};
} // namespace Aero::Media
