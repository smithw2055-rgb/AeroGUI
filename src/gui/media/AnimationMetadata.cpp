// Ordered metadata installer. Describe bodies live next to each type.

#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
using namespace ::Aero::Threading;
using namespace ::Aero::Input;
using namespace ::Aero::Media;
using namespace ::Aero::Data;
using namespace ::Aero::Interactivity;
    using namespace Interactivity;
    using Media::Animation::BeginStoryboard;
    using Media::Animation::BooleanAnimationUsingKeyFrames;
    using Media::Animation::BooleanKeyFrame;
    using Media::Animation::ColorAnimationUsingKeyFrames;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimationUsingKeyFrames;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::EventTrigger;
    using Media::Animation::Int16AnimationUsingKeyFrames;
    using Media::Animation::Int16KeyFrame;
    using Media::Animation::Int32AnimationUsingKeyFrames;
    using Media::Animation::Int32KeyFrame;
    using Media::Animation::Int64AnimationUsingKeyFrames;
    using Media::Animation::Int64KeyFrame;
    using Media::Animation::MatrixAnimationUsingKeyFrames;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::ObjectAnimationUsingKeyFrames;
    using Media::Animation::ObjectKeyFrame;
    using Media::Animation::PointAnimationUsingKeyFrames;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::SizeAnimationUsingKeyFrames;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::Storyboard;
    using Media::Animation::StoryboardCompletedTrigger;
    using Media::Animation::StringAnimationUsingKeyFrames;
    using Media::Animation::StringKeyFrame;
    using Media::Animation::ThicknessAnimationUsingKeyFrames;
    using Media::Animation::ThicknessKeyFrame;
    using Media::Animation::Timeline;
    using Media::Animation::TimelineGroup;
    using Media::Effect;
    using Media::FontFamily;
    using Media::Geometry;
    using Media::GeometryGroup;
    using Media::PathFigure;
    using Media::PathGeometry;
    using Media::PathSegment;
    using Media::StreamGeometry;
namespace {

void AddDoubleKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<DoubleKeyFrame> retained =
        Base::Ref<DoubleKeyFrame>::TryFromBorrowed(
            static_cast<DoubleKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<DoubleAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearDoubleKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<DoubleAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddPointKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<PointKeyFrame> retained =
        Base::Ref<PointKeyFrame>::TryFromBorrowed(
            static_cast<PointKeyFrame&>(*value));
    if (!retained) return;
    static_cast<PointAnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearPointKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<PointAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddThicknessKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ThicknessKeyFrame> retained =
        Base::Ref<ThicknessKeyFrame>::
            TryFromBorrowed(
                static_cast<
                    ThicknessKeyFrame&>(
                        *value));
    if (!retained) {
        return;
    }
    static_cast<ThicknessAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearThicknessKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ThicknessAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddColorKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ColorKeyFrame> retained =
        Base::Ref<ColorKeyFrame>::TryFromBorrowed(
            static_cast<ColorKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<ColorAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearColorKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ColorAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddObjectKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ObjectKeyFrame> retained =
        Base::Ref<ObjectKeyFrame>::TryFromBorrowed(
            static_cast<ObjectKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<ObjectAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearObjectKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ObjectAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddBooleanKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<BooleanKeyFrame> retained =
        Base::Ref<BooleanKeyFrame>::TryFromBorrowed(
            static_cast<BooleanKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<BooleanAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearBooleanKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<BooleanAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddInt16KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int16KeyFrame> retained =
        Base::Ref<Int16KeyFrame>::TryFromBorrowed(
            static_cast<Int16KeyFrame&>(*value));
    if (!retained) return;
    static_cast<Int16AnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearInt16KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int16AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddInt32KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int32KeyFrame> retained =
        Base::Ref<Int32KeyFrame>::TryFromBorrowed(
            static_cast<Int32KeyFrame&>(*value));
    if (!retained) return;
    static_cast<Int32AnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearInt32KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int32AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddInt64KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int64KeyFrame> retained =
        Base::Ref<Int64KeyFrame>::TryFromBorrowed(
            static_cast<Int64KeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<Int64AnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearInt64KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int64AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddSizeKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<SizeKeyFrame> retained =
        Base::Ref<SizeKeyFrame>::TryFromBorrowed(
            static_cast<SizeKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<SizeAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearSizeKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<SizeAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddMatrixKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<MatrixKeyFrame> retained =
        Base::Ref<MatrixKeyFrame>::TryFromBorrowed(
            static_cast<MatrixKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<MatrixAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearMatrixKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<MatrixAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddStringKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<StringKeyFrame> retained =
        Base::Ref<StringKeyFrame>::TryFromBorrowed(
            static_cast<StringKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<StringAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearStringKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<StringAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero {
using namespace ::Aero::MetadataSupport;

Base::Result<void> PopulateUiAnimation(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Media::Animation;
    using namespace Interactivity;
    // Prefer public Animation types over Model::* (AnimationEngine.hpp).
    using Media::Animation::ColorAnimation;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimation;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::MatrixAnimation;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::PointAnimation;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::RectAnimation;
    using Media::Animation::RepeatBehavior;
    using Media::Animation::SizeAnimation;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::ThicknessAnimation;
    using Media::Animation::ThicknessKeyFrame;
    // Prefer geometry typedefs over BrushRendering/AnimationModel conversion fns in Animation.
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;

    Register<Duration>(context)
        .ValueSemantics()
        .TextConverter<&Duration::TryParse>();

    Register<TimeSpan>(context)
        .ValueSemantics()
        .TextConverter<&TimeSpan::TryParse>();

    Register<RepeatBehavior>(context)
        .ValueSemantics()
        .TextConverter<&RepeatBehavior::TryParse>();

    Register<KeyTime>(context)
        .ValueSemantics()
        .TextConverter<&KeyTime::TryParse>();

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::Timeline>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::AnimationTimeline>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::TimelineGroup>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::ParallelTimeline>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::Storyboard>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::EasingFunctionBase>::Run(context);

#define AERO_EASE_FACTORY(name, kind) \
    +[]() noexcept -> Base::Result<Base::Ref<Base::Object>> { \
        auto created = Base::MakeRef<EasingFunctionBase>( \
            ::Aero::Meta::MakeTypeId(::Aero::Meta::AeroNamespaceUri(), #name), \
            EasingFunctionBase::Kind::kind); \
        if (!created) return created.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(created).Value()); \
    }
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "SineEase"),
        AERO_EASE_FACTORY(SineEase, Sine));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuadraticEase"),
        AERO_EASE_FACTORY(QuadraticEase, Quadratic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "CubicEase"),
        AERO_EASE_FACTORY(CubicEase, Cubic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuarticEase"),
        AERO_EASE_FACTORY(QuarticEase, Quartic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuinticEase"),
        AERO_EASE_FACTORY(QuinticEase, Quintic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "CircleEase"),
        AERO_EASE_FACTORY(CircleEase, Circle));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "ExponentialEase"),
        AERO_EASE_FACTORY(ExponentialEase, Exponential));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "PowerEase"),
        AERO_EASE_FACTORY(PowerEase, Power));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "BackEase"),
        AERO_EASE_FACTORY(BackEase, Back));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "BounceEase"),
        AERO_EASE_FACTORY(BounceEase, Bounce));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "ElasticEase"),
        AERO_EASE_FACTORY(ElasticEase, Elastic));
#undef AERO_EASE_FACTORY

#define AERO_FROM_TO_BASE(Name, CppType) \
    Register<Name##AnimationBase>(context, TypeFlags::Abstract) \
        .Property<CppType, &Name##AnimationBase::GetFrom, &Name##AnimationBase::SetFrom>("From") \
        .Property<CppType, &Name##AnimationBase::GetTo, &Name##AnimationBase::SetTo>("To");

#define AERO_FROM_TO_ANIMATION(Name, CppType) \
    AERO_FROM_TO_BASE(Name, CppType) \
    Register<Name##Animation>(context) \
        .Property<Base::Ref<EasingFunctionBase>, &Name##Animation::GetEasingFunction, &Name##Animation::SetEasingFunction>("EasingFunction", PropertyFlags::Structural) \
        .Factory();

    AERO_FROM_TO_BASE(Double, double)
    Register<DoubleAnimation>(context)
        .Property<double, &DoubleAnimation::GetAccelerationRatio, &DoubleAnimation::SetAccelerationRatio>("AccelerationRatio")
        .Property<double, &DoubleAnimation::GetDecelerationRatio, &DoubleAnimation::SetDecelerationRatio>("DecelerationRatio")
        .Property<Base::Ref<EasingFunctionBase>, &DoubleAnimation::GetEasingFunction, &DoubleAnimation::SetEasingFunction>("EasingFunction", PropertyFlags::Structural)
        .Factory();
    AERO_FROM_TO_ANIMATION(Color, Color)
    AERO_FROM_TO_ANIMATION(Point, Point)
    AERO_FROM_TO_ANIMATION(Rect, Rect)
    AERO_FROM_TO_ANIMATION(Thickness, Base::Thickness)
    AERO_FROM_TO_ANIMATION(Int16, std::int16_t)
    AERO_FROM_TO_ANIMATION(Int32, std::int32_t)
    AERO_FROM_TO_ANIMATION(Int64, std::int64_t)
    AERO_FROM_TO_ANIMATION(Size, Base::Size)
    AERO_FROM_TO_ANIMATION(Matrix, Base::Transform2D)
#undef AERO_FROM_TO_BASE
#undef AERO_FROM_TO_ANIMATION

#define AERO_KEY_FACTORY(Type, name, interpolation) \
    +[]() noexcept -> Base::Result<Base::Ref<Base::Object>> { \
        auto created = Base::MakeRef<Type>( \
            ::Aero::Meta::MakeTypeId(::Aero::Meta::AeroNamespaceUri(), #name), \
            KeyFrameBase::Interpolation::interpolation); \
        if (!created) return created.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(created).Value()); \
    }
#define AERO_KEYFRAME_ALIAS(Name, Kind) \
    SetObjectFactory(RegisterAlias<Name##KeyFrame>(context, #Kind #Name "KeyFrame"), \
        AERO_KEY_FACTORY(Name##KeyFrame, Kind##Name##KeyFrame, Kind));
#define AERO_KEYFRAME_ALIASES(Name) \
    AERO_KEYFRAME_ALIAS(Name, Linear) \
    AERO_KEYFRAME_ALIAS(Name, Discrete) \
    AERO_KEYFRAME_ALIAS(Name, Easing) \
    AERO_KEYFRAME_ALIAS(Name, Spline)
#define AERO_KEYFRAME_VALUE(Name, CppType) \
    Register<Name##KeyFrame>(context, TypeFlags::Abstract) \
        .Property<CppType, &Name##KeyFrame::GetValue, &Name##KeyFrame::SetValue>("Value");
#define AERO_KEYFRAME_COLLECTION(Name) \
    Register<Name##AnimationUsingKeyFrames>(context) \
        .Content<Name##KeyFrame>("KeyFrames", ContentKind::Collection, &Add##Name##KeyFrame, &Clear##Name##KeyFrames) \
        .Factory();
#define AERO_KEYFRAMES(Name, CppType) \
    AERO_KEYFRAME_VALUE(Name, CppType) \
    AERO_KEYFRAME_ALIASES(Name) \
    AERO_KEYFRAME_COLLECTION(Name)
#define AERO_DISCRETE_KEYFRAMES(Name, CppType) \
    AERO_KEYFRAME_VALUE(Name, CppType) \
    AERO_KEYFRAME_ALIAS(Name, Discrete) \
    AERO_KEYFRAME_COLLECTION(Name)
    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::KeyFrameBase>::Run(context);

    AERO_KEYFRAMES(Double, double)
    AERO_KEYFRAMES(Point, Point)
    AERO_KEYFRAMES(Thickness, Thickness)
    AERO_KEYFRAMES(Color, Base::Color)
    AERO_KEYFRAMES(Int16, std::int16_t)
    AERO_KEYFRAMES(Int32, std::int32_t)
    AERO_KEYFRAMES(Int64, std::int64_t)
    AERO_KEYFRAMES(Size, Base::Size)
    AERO_KEYFRAMES(Matrix, Base::Transform2D)

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::ObjectKeyFrame>::Run(context);
    AERO_KEYFRAME_ALIAS(Object, Discrete)
    AERO_KEYFRAME_COLLECTION(Object)

    AERO_DISCRETE_KEYFRAMES(Boolean, bool)
    AERO_DISCRETE_KEYFRAMES(String, Base::String)
#undef AERO_KEYFRAME_ALIAS
#undef AERO_KEYFRAME_ALIASES
#undef AERO_KEYFRAME_VALUE
#undef AERO_KEYFRAME_COLLECTION
#undef AERO_KEYFRAMES
#undef AERO_DISCRETE_KEYFRAMES

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::TriggerAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Input::KeyBinding>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Input::MouseBinding>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::ChangePropertyAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::SetFocusAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::LaunchUriOrFileAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::RemoveElementAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::ControllableStoryboardAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::BeginStoryboard>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::ControlStoryboardAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::PauseStoryboard>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::ResumeStoryboard>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::StopStoryboard>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::RemoveStoryboard>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::SeekStoryboard>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::EventTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::TimerTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::PropertyChangedTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::KeyTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::InvokeCommandAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::SelectAction>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Interactivity::SelectAllAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::PlaySoundAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::PlayMediaAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::PauseMediaAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::StopMediaAction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::ComparisonCondition>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::ConditionalExpression>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::ConditionBehavior>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Animation::StoryboardCompletedTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::Behavior>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::MouseDragElementBehavior>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::BackgroundEffectBehavior>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::StyleBehaviorCollection>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::StyleTriggerCollection>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::StyleInteraction>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Interactivity::Interaction>::Run(context);
    return {};
}

} // namespace Aero
