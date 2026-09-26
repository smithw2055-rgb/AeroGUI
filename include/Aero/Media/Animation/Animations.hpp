#pragma once

// AnimationTimeline plus the from/to and key-frame templates. XAML names
// such as DoubleAnimation are these specializations, not a class per value.
#include <Aero/Media/Animation/Timeline.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/Animation/KeyFrames.hpp>
#include <Aero/Media/Animation/EasingFunctions.hpp>

#include <cmath>
#include <type_traits>
#include <utility>
#include <cstdint>

namespace Aero::Media::Animation {

class AERO_GUI_API AnimationTimeline : public Timeline {
    AERO_DECLARE_TYPE(AnimationTimeline, Timeline)
protected:
    explicit AnimationTimeline(Meta::TypeId runtimeType) noexcept
        : Timeline(runtimeType) {}
};

template<class T>
struct FromToName;

#define AERO_FROM_TO_NAME(Tag, CppType) \
    template<> struct FromToName<CppType> { \
        static constexpr Base::StringView BaseName() noexcept { \
            return #Tag "AnimationBase"; \
        } \
        static constexpr Base::StringView ConcreteName() noexcept { \
            return #Tag "Animation"; \
        } \
    };

AERO_FROM_TO_NAME(Double, double)
AERO_FROM_TO_NAME(Color, Base::Color)
AERO_FROM_TO_NAME(Point, Base::Point)
AERO_FROM_TO_NAME(Rect, Base::Rect)
AERO_FROM_TO_NAME(Thickness, Base::Thickness)
AERO_FROM_TO_NAME(Int16, std::int16_t)
AERO_FROM_TO_NAME(Int32, std::int32_t)
AERO_FROM_TO_NAME(Int64, std::int64_t)
AERO_FROM_TO_NAME(Size, Base::Size)
AERO_FROM_TO_NAME(Matrix, Base::Transform2D)
#undef AERO_FROM_TO_NAME

template<class TKeyFrame>
struct KeyedAnimationName;

#define AERO_KEYED_ANIMATION_NAME(Frame, NameLiteral) \
    template<> struct KeyedAnimationName<Frame> { \
        static constexpr Base::StringView Name() noexcept { \
            return NameLiteral; \
        } \
    };

AERO_KEYED_ANIMATION_NAME(DoubleKeyFrame, "DoubleAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(PointKeyFrame, "PointAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(ColorKeyFrame, "ColorAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(ThicknessKeyFrame, "ThicknessAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(ObjectKeyFrame, "ObjectAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(BooleanKeyFrame, "BooleanAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(Int16KeyFrame, "Int16AnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(Int32KeyFrame, "Int32AnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(Int64KeyFrame, "Int64AnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(SizeKeyFrame, "SizeAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(MatrixKeyFrame, "MatrixAnimationUsingKeyFrames")
AERO_KEYED_ANIMATION_NAME(StringKeyFrame, "StringAnimationUsingKeyFrames")
#undef AERO_KEYED_ANIMATION_NAME

template<class T>
inline bool AnimationValueAccepted(const T& value) noexcept {
    if constexpr (std::is_same_v<T, double>) {
        return std::isfinite(value);
    } else if constexpr (std::is_same_v<T, Base::Color>) {
        return Base::IsFiniteColor(value);
    } else if constexpr (std::is_same_v<T, Base::Point>) {
        return std::isfinite(value.x) && std::isfinite(value.y);
    } else if constexpr (std::is_same_v<T, Base::Rect>) {
        return Base::IsFiniteRect(value);
    } else if constexpr (std::is_same_v<T, Base::Thickness>) {
        return std::isfinite(value.left) && std::isfinite(value.top) &&
            std::isfinite(value.right) && std::isfinite(value.bottom);
    } else if constexpr (std::is_same_v<T, Base::Size>) {
        return std::isfinite(value.width) && std::isfinite(value.height);
    } else if constexpr (std::is_same_v<T, Base::Transform2D>) {
        return Base::IsFiniteTransform(value);
    } else {
        static_cast<void>(value);
        return true;
    }
}

// Color, Point, Rect and Thickness historically stored the value without
// flipping the presence flag. ResolveFrom keeps that behavior.
template<class T>
inline constexpr bool AnimationRecordsPresence =
    !std::is_same_v<T, Base::Color> &&
    !std::is_same_v<T, Base::Point> &&
    !std::is_same_v<T, Base::Rect> &&
    !std::is_same_v<T, Base::Thickness>;

template<class T>
class AnimationBase : public AnimationTimeline {
public:
    using BaseType = AnimationTimeline;
    static constexpr Base::StringView StaticMetadataNamespace() noexcept {
        return Meta::AeroNamespaceUri();
    }
    static constexpr Base::StringView StaticMetadataName() noexcept {
        return FromToName<T>::BaseName();
    }
    static constexpr Meta::TypeId StaticTypeId() noexcept {
        return Meta::MakeTypeId(
            StaticMetadataNamespace(), StaticMetadataName());
    }

    T GetFrom() const noexcept { return from_; }
    T GetTo() const noexcept { return to_; }
    bool GetHasFrom() const noexcept { return hasFrom_; }
    bool GetHasTo() const noexcept { return hasTo_; }
    T ResolveFrom(T defaultOriginValue) const noexcept {
        return hasFrom_ ? from_ : defaultOriginValue;
    }
    T ResolveTo(T defaultDestinationValue) const noexcept {
        return hasTo_ ? to_ : defaultDestinationValue;
    }
    void SetFrom(T value) noexcept {
        if (!WritePreamble() || !AnimationValueAccepted(value)) return;
        from_ = value;
        if constexpr (AnimationRecordsPresence<T>) hasFrom_ = true;
        WritePostscript();
    }
    void SetTo(T value) noexcept {
        if (!WritePreamble() || !AnimationValueAccepted(value)) return;
        to_ = value;
        if constexpr (AnimationRecordsPresence<T>) hasTo_ = true;
        WritePostscript();
    }
    T GetCurrentValue(
        T defaultOriginValue,
        T defaultDestinationValue,
        double progress) const noexcept {
        if constexpr (std::is_same_v<T, double>) {
            const double from = ResolveFrom(defaultOriginValue);
            const double to = ResolveTo(defaultDestinationValue);
            return from + (to - from) * progress;
        } else {
            static_cast<void>(defaultOriginValue);
            static_cast<void>(defaultDestinationValue);
            static_cast<void>(progress);
            return T{};
        }
    }

protected:
    explicit AnimationBase(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    T from_{};
    T to_{};
    bool hasFrom_ = false;
    bool hasTo_ = false;
};

template<class T, bool Enabled = std::is_same_v<T, double>>
struct AnimationRatioStorage {};

template<class T>
struct AnimationRatioStorage<T, true> {
    double accelerationRatio_ = 0.0;
    double decelerationRatio_ = 0.0;
};

template<class T>
class Animation
    : public AnimationBase<T>,
      private AnimationRatioStorage<T> {
public:
    using BaseType = AnimationBase<T>;
    static constexpr Base::StringView StaticMetadataNamespace() noexcept {
        return Meta::AeroNamespaceUri();
    }
    static constexpr Base::StringView StaticMetadataName() noexcept {
        return FromToName<T>::ConcreteName();
    }
    static constexpr Meta::TypeId StaticTypeId() noexcept {
        return Meta::MakeTypeId(
            StaticMetadataNamespace(), StaticMetadataName());
    }

    Animation() noexcept : AnimationBase<T>(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept {
        if (!this->WritePreamble() || easing_.Get() == value.Get()) return;
        easing_ = std::move(value);
        this->WritePostscript();
    }
    double GetAccelerationRatio() const noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return this->accelerationRatio_;
        } else {
            return 0.0;
        }
    }
    double GetDecelerationRatio() const noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return this->decelerationRatio_;
        } else {
            return 0.0;
        }
    }
    void SetAccelerationRatio(double value) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            if (!this->WritePreamble()) return;
            if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
                value + this->decelerationRatio_ > 1.0) {
                return;
            }
            this->accelerationRatio_ = value;
            this->WritePostscript();
        } else {
            static_cast<void>(value);
        }
    }
    void SetDecelerationRatio(double value) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            if (!this->WritePreamble()) return;
            if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
                this->accelerationRatio_ + value > 1.0) {
                return;
            }
            this->decelerationRatio_ = value;
            this->WritePostscript();
        } else {
            static_cast<void>(value);
        }
    }

protected:
    explicit Animation(Meta::TypeId runtimeType) noexcept
        : AnimationBase<T>(runtimeType) {}

private:
    Ref<EasingFunctionBase> easing_;
};

template<class TKeyFrame>
class AnimationUsingKeyFrames : public AnimationTimeline {
public:
    using BaseType = AnimationTimeline;
    static constexpr Base::StringView StaticMetadataNamespace() noexcept {
        return Meta::AeroNamespaceUri();
    }
    static constexpr Base::StringView StaticMetadataName() noexcept {
        return KeyedAnimationName<TKeyFrame>::Name();
    }
    static constexpr Meta::TypeId StaticTypeId() noexcept {
        return Meta::MakeTypeId(
            StaticMetadataNamespace(), StaticMetadataName());
    }

    AnimationUsingKeyFrames() noexcept
        : AnimationTimeline(StaticTypeId()) {}

    void AddKeyFrame(Ref<TKeyFrame> value) noexcept {
        Result<void> writable = WritePreamble();
        if (!writable) { AERO_ASSERT(false); return; }
        if (!value) { AERO_ASSERT(false); return; }
        keyFrames_.PushBack(std::move(value));
        WritePostscript();
    }
    void ClearKeyFrames() noexcept {
        if (!WritePreamble() || keyFrames_.Empty()) return;
        keyFrames_.Clear();
        WritePostscript();
    }
    Span<const Ref<TKeyFrame>> GetKeyFrames() const noexcept {
        return {keyFrames_.Data(), keyFrames_.Size()};
    }

protected:
    explicit AnimationUsingKeyFrames(Meta::TypeId runtimeType) noexcept
        : AnimationTimeline(runtimeType) {}

private:
    Base::Vector<Ref<TKeyFrame>> keyFrames_;
};

using DoubleAnimationBase = AnimationBase<double>;
using DoubleAnimation = Animation<double>;
using ColorAnimationBase = AnimationBase<Base::Color>;
using ColorAnimation = Animation<Base::Color>;
using PointAnimationBase = AnimationBase<Base::Point>;
using PointAnimation = Animation<Base::Point>;
using RectAnimationBase = AnimationBase<Base::Rect>;
using RectAnimation = Animation<Base::Rect>;
using ThicknessAnimationBase = AnimationBase<Base::Thickness>;
using ThicknessAnimation = Animation<Base::Thickness>;
using Int16AnimationBase = AnimationBase<std::int16_t>;
using Int16Animation = Animation<std::int16_t>;
using Int32AnimationBase = AnimationBase<std::int32_t>;
using Int32Animation = Animation<std::int32_t>;
using Int64AnimationBase = AnimationBase<std::int64_t>;
using Int64Animation = Animation<std::int64_t>;
using SizeAnimationBase = AnimationBase<Base::Size>;
using SizeAnimation = Animation<Base::Size>;
using MatrixAnimationBase = AnimationBase<Base::Transform2D>;
using MatrixAnimation = Animation<Base::Transform2D>;

using DoubleAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<DoubleKeyFrame>;
using PointAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<PointKeyFrame>;
using ColorAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<ColorKeyFrame>;
using ThicknessAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<ThicknessKeyFrame>;
using ObjectAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<ObjectKeyFrame>;
using BooleanAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<BooleanKeyFrame>;
using Int16AnimationUsingKeyFrames =
    AnimationUsingKeyFrames<Int16KeyFrame>;
using Int32AnimationUsingKeyFrames =
    AnimationUsingKeyFrames<Int32KeyFrame>;
using Int64AnimationUsingKeyFrames =
    AnimationUsingKeyFrames<Int64KeyFrame>;
using SizeAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<SizeKeyFrame>;
using MatrixAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<MatrixKeyFrame>;
using StringAnimationUsingKeyFrames =
    AnimationUsingKeyFrames<StringKeyFrame>;

} // namespace Aero::Media::Animation
