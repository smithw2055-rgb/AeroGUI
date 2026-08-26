#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/KeyFrameBase.hpp>
#include <cmath>
#include <type_traits>
#include <utility>

namespace Aero::Media::Animation {

template<typename T>
class KeyFrame : public KeyFrameBase {
public:
    T GetValue() const noexcept { return value_; }
    void SetValue(T value) noexcept {
        if constexpr (std::is_same<T, double>::value) {
            if (!std::isfinite(value)) return;
        } else if constexpr (std::is_same<T, Base::Point>::value) {
            if (!std::isfinite(value.x) || !std::isfinite(value.y)) return;
        } else if constexpr (std::is_same<T, Base::Color>::value) {
            if (!Base::IsFiniteColor(value)) return;
        } else if constexpr (std::is_same<T, Base::Thickness>::value) {
            if (!std::isfinite(value.left) || !std::isfinite(value.top) ||
                !std::isfinite(value.right) || !std::isfinite(value.bottom)) {
                return;
            }
        } else if constexpr (std::is_same<T, Base::Size>::value) {
            if (!std::isfinite(value.width) || !std::isfinite(value.height)) {
                return;
            }
        } else if constexpr (std::is_same<T, Meta::PropertyValue>::value) {
            if (value.IsUnset()) return;
        }
        if (!WritePreamble()) return;
        value_ = std::move(value);
        WritePostscript();
    }

protected:
    KeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrameBase(runtimeType, interpolation) {}

private:
    T value_{};
};

} // namespace Aero::Media::Animation
