#pragma once

// KeyFrameBase, KeyFrame<T>, and the typed key-frame family.
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Media/Animation/Timeline.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/EasingFunctions.hpp>

#include <cstdint>
#include <cmath>
#include <type_traits>
#include <utility>

namespace Aero::Media::Animation {

class AERO_GUI_API KeyFrameBase : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(KeyFrameBase, ::Aero::Freezable)
public:
    enum class Interpolation : std::uint8_t {
        Linear = 0U,
        Discrete,
        Easing,
        Spline
    };

    KeyTime GetKeyTime() const noexcept { return GetValue(KeyTimeProperty); }
    void SetKeyTime(KeyTime value) noexcept;
    void SetKeyTime(StringView value) noexcept;
    AnimationTime GetKeyTimeMicroseconds() const noexcept { return GetKeyTime().GetTimeSpan().Microseconds(); }

    Ref<EasingFunctionBase> GetEasingFunction() const noexcept { return GetValue(EasingFunctionProperty); }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

    StringView GetKeySpline() const noexcept { return GetValue(KeySplineProperty); }
    void SetKeySpline(StringView value) noexcept;

    Interpolation GetInterpolation() const noexcept { return interpolation_; }
    double GetSplineControlPoint1X() const noexcept { return controlPoint1X_; }
    double GetSplineControlPoint1Y() const noexcept { return controlPoint1Y_; }
    double GetSplineControlPoint2X() const noexcept { return controlPoint2X_; }
    double GetSplineControlPoint2Y() const noexcept { return controlPoint2Y_; }

    AERO_DEPENDENCY_PROPERTY(KeyTime, KeyTime);
    AERO_DEPENDENCY_PROPERTY(Ref<EasingFunctionBase>, EasingFunction);
    AERO_DEPENDENCY_PROPERTY(String, KeySpline);

    static void OnKeySplineChanged(DependencyObject& object, const DependencyPropertyChangedEventArgs& args) noexcept;

protected:
    KeyFrameBase(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : Freezable(runtimeType), interpolation_(interpolation) {}

private:
    Interpolation interpolation_ = Interpolation::Linear;
    double controlPoint1X_ = 0.0;
    double controlPoint1Y_ = 0.0;
    double controlPoint2X_ = 1.0;
    double controlPoint2Y_ = 1.0;
};

template<typename T> class KeyFrame : public KeyFrameBase {
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
                !std::isfinite(value.right) || !std::isfinite(value.bottom)) { return; }
        } else if constexpr (std::is_same<T, Base::Size>::value) {
            if (!std::isfinite(value.width) || !std::isfinite(value.height)) { return; }
        } else if constexpr (std::is_same<T, Base::Transform2D>::value) {
            if (!Base::IsFiniteTransform(value)) return;
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

class AERO_GUI_API DoubleKeyFrame : public KeyFrame<double> {
    AERO_DECLARE_TYPE(DoubleKeyFrame, KeyFrameBase)
public:
    explicit DoubleKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<double>(runtimeType, interpolation) {}
};

class AERO_GUI_API PointKeyFrame : public KeyFrame<Base::Point> {
    AERO_DECLARE_TYPE(PointKeyFrame, KeyFrameBase)
public:
    explicit PointKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<Base::Point>(runtimeType, interpolation) {}
};

class AERO_GUI_API ColorKeyFrame : public KeyFrame<Base::Color> {
    AERO_DECLARE_TYPE(ColorKeyFrame, KeyFrameBase)
public:
    explicit ColorKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<Base::Color>(runtimeType, interpolation) {}
};

class AERO_GUI_API ThicknessKeyFrame : public KeyFrame<Base::Thickness> {
    AERO_DECLARE_TYPE(ThicknessKeyFrame, KeyFrameBase)
public:
    explicit ThicknessKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<Base::Thickness>(runtimeType, interpolation) {}
};

class AERO_GUI_API ObjectKeyFrame : public KeyFrame<Meta::PropertyValue> {
    AERO_DECLARE_TYPE(ObjectKeyFrame, KeyFrameBase)
public:
    explicit ObjectKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<Meta::PropertyValue>(runtimeType, interpolation) {}
};

class AERO_GUI_API BooleanKeyFrame : public KeyFrame<bool> {
    AERO_DECLARE_TYPE(BooleanKeyFrame, KeyFrameBase)
public:
    explicit BooleanKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<bool>(runtimeType, interpolation) {}
};

class AERO_GUI_API Int16KeyFrame : public KeyFrame<std::int16_t> {
    AERO_DECLARE_TYPE(Int16KeyFrame, KeyFrameBase)
public:
    explicit Int16KeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<std::int16_t>(runtimeType, interpolation) {}
};

class AERO_GUI_API Int32KeyFrame : public KeyFrame<std::int32_t> {
    AERO_DECLARE_TYPE(Int32KeyFrame, KeyFrameBase)
public:
    explicit Int32KeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<std::int32_t>(runtimeType, interpolation) {}
};

class AERO_GUI_API Int64KeyFrame : public KeyFrame<std::int64_t> {
    AERO_DECLARE_TYPE(Int64KeyFrame, KeyFrameBase)
public:
    explicit Int64KeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<std::int64_t>(runtimeType, interpolation) {}
};

class AERO_GUI_API SizeKeyFrame : public KeyFrame<Base::Size> {
    AERO_DECLARE_TYPE(SizeKeyFrame, KeyFrameBase)
public:
    explicit SizeKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<Base::Size>(runtimeType, interpolation) {}
};

class AERO_GUI_API MatrixKeyFrame : public KeyFrame<Base::Transform2D> {
    AERO_DECLARE_TYPE(MatrixKeyFrame, KeyFrameBase)
public:
    explicit MatrixKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<Base::Transform2D>(runtimeType, interpolation) {}
};

class AERO_GUI_API StringKeyFrame : public KeyFrame<Base::String> {
    AERO_DECLARE_TYPE(StringKeyFrame, KeyFrameBase)
public:
    explicit StringKeyFrame(Meta::TypeId runtimeType, Interpolation interpolation) noexcept
        : KeyFrame<Base::String>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation
