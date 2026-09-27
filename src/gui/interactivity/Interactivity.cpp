#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/FrameworkElement.hpp>
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/EventTrigger.hpp>
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
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Interactivity {

Behavior::~Behavior() {
    Detach();
}

Base::Result<void> Behavior::Attach(FrameworkElement& object) noexcept {
    if (associatedObject_ == &object) return {};
    if (associatedObject_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Behavior is already attached to another object");
    }
    associatedObject_ = &object;
    OnAttached();
    return {};
}

void Behavior::Detach() noexcept {
    if (associatedObject_ == nullptr) return;
    OnDetaching();
    associatedObject_ = nullptr;
}

void Behavior::AddAuthoredBinding(
    Meta::DependencyPropertyHandle property,
    Base::Ref<Aero::Data::Binding> binding) noexcept {
    if (!property.IsValid() || !binding) { AERO_ASSERT(false); return; }
    for (AuthoredBinding& existing : authoredBindings_) {
        if (existing.property == property) {
            existing.binding = std::move(binding);
            return;
        }
    }
    authoredBindings_.PushBack(
        {property, std::move(binding)});
}

void Behavior::CopyAuthoredBindingsTo(
    Behavior& destination) const noexcept {
    for (const AuthoredBinding& binding : authoredBindings_) {
        destination.AddAuthoredBinding(
            binding.property, binding.binding);
    }
}

Base::Result<Base::Ref<Behavior>> Behavior::ClonePrototype(
    const Behavior& prototype,
    Meta::Registry& metadata) noexcept {
    Base::Result<Base::Ref<Base::Object>> created =
        metadata.CreateObject(prototype.RuntimeType());
    if (!created) return created.GetStatus();
    if (!created.Value() ||
        !metadata.Types().IsDerivedFrom(
            created.Value()->RuntimeType(),
            Behavior::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Behavior factory returned an incompatible object");
    }
    Base::Ref<Behavior> clone =
        Base::Ref<Behavior>::FromBorrowed(
            *static_cast<Behavior*>(created.Value().Get()));
    for (const Meta::DependencyProperty& property :
         DependencyObjectAccess::PropertyRegistry((prototype)).Properties()) {
        if (property.MetadataFor(prototype.RuntimeType()) == nullptr ||
            property.MetadataFor(clone->RuntimeType()) == nullptr) {
            continue;
        }
        Meta::PropertyValue local =
            prototype.ReadLocalValue(property.Handle());
        if (local.IsUnset()) continue;
        clone->SetValue(property.Handle(), local);
    }
    prototype.CopyAuthoredBindingsTo(*clone);
    return clone;
}

void StyleBehaviorCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    items_.PushBack(std::move(value));
}

void StyleTriggerCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    items_.PushBack(std::move(value));
}

void StyleInteraction::OnBehaviorsChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!DependencyObjectAccess::PropertyRegistry((object)).Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        (element).ClearStyleBehaviorPrototypes());
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleBehaviorCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& behavior :
         static_cast<StyleBehaviorCollection&>(*value.AsObject()).GetItems()) {
        (element).AddStyleBehaviorPrototype( behavior);
    }
}

void StyleInteraction::OnTriggersChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!DependencyObjectAccess::PropertyRegistry((object)).Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        (element).ClearStyleTriggerPrototypes());
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleTriggerCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& trigger :
         static_cast<StyleTriggerCollection&>(*value.AsObject()).GetItems()) {
        (element).AddStyleTriggerPrototype( trigger);
    }
}

} // namespace Aero::Interactivity

// Metadata registration for the types implemented in this file.
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

void AddPropertyChangedTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<TriggerAction> retained =
        Base::Ref<TriggerAction>::TryFromBorrowed(
            static_cast<TriggerAction&>(*value));
    if (!retained) return;
    static_cast<PropertyChangedTrigger&>(owner)
        .AddAction(std::move(retained));
}

void ClearPropertyChangedTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<PropertyChangedTrigger&>(owner)
        .ClearActions();
}

void AddKeyTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<TriggerAction> retained =
        Base::Ref<TriggerAction>::TryFromBorrowed(
            static_cast<TriggerAction&>(*value));
    if (!retained) return;
    static_cast<KeyTrigger&>(owner)
        .AddAction(std::move(retained));
}

void ClearKeyTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<KeyTrigger&>(owner).ClearActions();
}

void AddInteractionBehavior(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    if (!value) return;
    if (owner.RuntimeType() == DataTrigger::StaticTypeId()) {
        static_cast<DataTrigger&>(owner).AddBehavior(value);
    } else if (owner.RuntimeType() ==
               StoryboardCompletedTrigger::StaticTypeId()) {
        static_cast<StoryboardCompletedTrigger&>(owner)
            .AddConditionBehavior(value);
    } else if (owner.RuntimeType() ==
               EventTrigger::StaticTypeId()) {
        static_cast<EventTrigger&>(owner)
            .AddConditionBehavior(value);
    } else {
        static_cast<void>((static_cast<FrameworkElement&>(owner)).AddAuthoredBehavior( value));
    }
}

void ClearInteractionBehaviors(Base::Object& owner, void*) noexcept {
    if (owner.RuntimeType() == DataTrigger::StaticTypeId()) {
        static_cast<DataTrigger&>(owner).ClearBehaviors();
    } else if (owner.RuntimeType() ==
               StoryboardCompletedTrigger::StaticTypeId()) {
        static_cast<StoryboardCompletedTrigger&>(owner)
            .ClearConditionBehaviors();
    } else if (owner.RuntimeType() ==
               EventTrigger::StaticTypeId()) {
        static_cast<EventTrigger&>(owner)
            .ClearConditionBehaviors();
    } else {
        static_cast<void>((static_cast<FrameworkElement&>(owner)).ClearAuthoredBehaviors());
    }
}

void AddStyleBehaviorItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    static_cast<void>(
        static_cast<StyleBehaviorCollection&>(owner)
            .Add(value));
}

void ClearStyleBehaviorItems(Base::Object& owner, void*) noexcept {
    static_cast<StyleBehaviorCollection&>(owner).Clear();
}

void AddStyleTriggerItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    static_cast<void>(
        static_cast<StyleTriggerCollection&>(owner)
            .Add(value));
}

void ClearStyleTriggerItems(Base::Object& owner, void*) noexcept {
    static_cast<StyleTriggerCollection&>(owner).Clear();
}

void AddConditionalComparison(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    if (!value) return;
    Base::Ref<ComparisonCondition> retained =
        Base::Ref<ComparisonCondition>::TryFromBorrowed(
            static_cast<ComparisonCondition&>(*value));
    if (!retained) return;
    static_cast<ConditionalExpression&>(owner)
        .AddCondition(std::move(retained));
}

void ClearConditionalComparisons(Base::Object& owner, void*) noexcept {
    static_cast<ConditionalExpression&>(owner).ClearConditions(); return;
}

void SetConditionBehaviorExpression(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    static_cast<ConditionBehavior&>(owner).SetExpression(
        Base::Ref<ConditionalExpression>::FromBorrowed(
            static_cast<ConditionalExpression&>(*value)));
    return;
}

void ClearConditionBehaviorExpression(Base::Object& owner, void*) noexcept {
    static_cast<ConditionBehavior&>(owner).SetExpression({});
    return;
}

void AddInteractionTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    auto& dependencyObject =
        static_cast<DependencyObject&>(owner);
    const Meta::TypeRegistry& types =
        DependencyObjectAccess::PropertyRegistry((dependencyObject)).Types();
    if (types.IsDerivedFrom(
            owner.RuntimeType(), FrameworkElement::StaticTypeId())) {
        static_cast<void>(
            (static_cast<FrameworkElement&>(owner)).AddAuthoredTrigger( value));
    } else if (types.IsDerivedFrom(
                   owner.RuntimeType(),
                   FrameworkContentElement::StaticTypeId())) {
        static_cast<void>(
            (static_cast<FrameworkContentElement&>(owner)).AddAuthoredTrigger( value));
    }
}

void ClearInteractionTriggers(
    Base::Object& owner,
    void*) noexcept {
    auto& dependencyObject =
        static_cast<DependencyObject&>(owner);
    const Meta::TypeRegistry& types =
        DependencyObjectAccess::PropertyRegistry((dependencyObject)).Types();
    if (types.IsDerivedFrom(
            owner.RuntimeType(), FrameworkElement::StaticTypeId())) {
        static_cast<void>(
            (static_cast<FrameworkElement&>(owner)).ClearAuthoredTriggers());
    } else if (types.IsDerivedFrom(
                   owner.RuntimeType(),
                   FrameworkContentElement::StaticTypeId())) {
        static_cast<void>(
            (static_cast<FrameworkContentElement&>(owner)).ClearAuthoredTriggers());
    }
}

} // namespace
} // namespace Aero::MetadataSupport

AERO_DESCRIBE(::Aero::Interactivity::TriggerAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<TriggerAction>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(::Aero::Interactivity::ChangePropertyAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<ChangePropertyAction>(context)
            .Property("TargetName", &ChangePropertyAction::GetTargetName, &ChangePropertyAction::SetTargetName)
            .Property<Base::Ref<Data::Binding>, &ChangePropertyAction::GetTargetObject, &ChangePropertyAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Property("PropertyName", &ChangePropertyAction::GetPropertyName, &ChangePropertyAction::SetPropertyName)
            .Property<Meta::PropertyValue, &ChangePropertyAction::GetValue, &ChangePropertyAction::SetValue>("Value", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::SetFocusAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<SetFocusAction>(context)
            .Property("TargetName", &SetFocusAction::GetTargetName, &SetFocusAction::SetTargetName)
            .Property<Base::Ref<Data::Binding>, &SetFocusAction::GetTargetObject, &SetFocusAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Property("Engage", &SetFocusAction::GetEngage, &SetFocusAction::SetEngage)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::LaunchUriOrFileAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<LaunchUriOrFileAction>(context)
            .Property("Path", &LaunchUriOrFileAction::GetPath, &LaunchUriOrFileAction::SetPath)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::RemoveElementAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<RemoveElementAction>(context)
            .Property<Base::Ref<Data::Binding>, &RemoveElementAction::GetTargetObject, &RemoveElementAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::PropertyChangedTrigger) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<PropertyChangedTrigger>(context)
            .Property<Base::Ref<Data::Binding>, &PropertyChangedTrigger::GetBinding, &PropertyChangedTrigger::SetBinding>("Binding")
            .Content<TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddPropertyChangedTriggerAction, &::Aero::MetadataSupport::ClearPropertyChangedTriggerActions)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::KeyTrigger) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<KeyTrigger>(context)
            .Property("Key", &KeyTrigger::GetKey, &KeyTrigger::SetKey)
            .Property("ActiveOnFocus", &KeyTrigger::GetActiveOnFocus, &KeyTrigger::SetActiveOnFocus)
            .Content<TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddKeyTriggerAction, &::Aero::MetadataSupport::ClearKeyTriggerActions)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::InvokeCommandAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<InvokeCommandAction>(context)
            .Property("Command", &InvokeCommandAction::GetCommand, &InvokeCommandAction::SetCommand)
            .Property<Meta::PropertyValue, &InvokeCommandAction::GetCommandParameter, &InvokeCommandAction::SetCommandParameter>("CommandParameter", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::SelectAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<SelectAction>(context)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::SelectAllAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<SelectAllAction>(context)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::PlaySoundAction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<PlaySoundAction>(context)
            .Property("Source", &PlaySoundAction::GetSource, &PlaySoundAction::SetSource)
            .Property("Volume", &PlaySoundAction::GetVolume, &PlaySoundAction::SetVolume)
            .Property(PlaySoundAction::IsEnabledProperty, true)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::ComparisonCondition) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<ComparisonCondition>(context)
            .Property<Base::Ref<Data::Binding>, &ComparisonCondition::GetLeftOperand, &ComparisonCondition::SetLeftOperand>("LeftOperand")
            .Property<Meta::PropertyValue, &ComparisonCondition::GetRightOperand, &ComparisonCondition::SetRightOperand>("RightOperand", PropertyFlags::AnyValue)
            .Property("Operator", &ComparisonCondition::GetComparisonOperator, &ComparisonCondition::SetComparisonOperator)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::ConditionalExpression) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<ConditionalExpression>(context)
            .Property("ForwardChaining", &ConditionalExpression::GetChaining, &ConditionalExpression::SetChaining)
            .Content<ComparisonCondition>("Conditions", ContentKind::Collection, &::Aero::MetadataSupport::AddConditionalComparison, &::Aero::MetadataSupport::ClearConditionalComparisons)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::ConditionBehavior) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<ConditionBehavior>(context)
            .Content<ConditionalExpression>("Expression", ContentKind::Single, &::Aero::MetadataSupport::SetConditionBehaviorExpression, &::Aero::MetadataSupport::ClearConditionBehaviorExpression)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::Behavior) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<Behavior>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(::Aero::Interactivity::StyleBehaviorCollection) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<StyleBehaviorCollection>(context)
            .Content<Base::Object>("Items", ContentKind::Collection, &::Aero::MetadataSupport::AddStyleBehaviorItem, &::Aero::MetadataSupport::ClearStyleBehaviorItems)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::StyleTriggerCollection) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<StyleTriggerCollection>(context)
            .Content<Base::Object>("Items", ContentKind::Collection, &::Aero::MetadataSupport::AddStyleTriggerItem, &::Aero::MetadataSupport::ClearStyleTriggerItems)
            .Factory();
}

AERO_DESCRIBE(::Aero::Interactivity::StyleInteraction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<StyleInteraction>(context, TypeFlags::Abstract)
            .Property(StyleInteraction::BehaviorsProperty, FrameworkPropertyMetadata(Base::Ref<StyleBehaviorCollection>{}) .Changed(&StyleInteraction::OnBehaviorsChanged))
            .Property(StyleInteraction::TriggersProperty, FrameworkPropertyMetadata(Base::Ref<StyleTriggerCollection>{}) .Changed(&StyleInteraction::OnTriggersChanged));
}

AERO_DESCRIBE(::Aero::Interactivity::Interaction) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media::Animation;
    using namespace Interactivity;
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
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<Interaction>(context, TypeFlags::Abstract)
            .Collection<Base::Object>("Triggers", &::Aero::MetadataSupport::AddInteractionTrigger, &::Aero::MetadataSupport::ClearInteractionTriggers, PropertyFlags::Attached | PropertyFlags::Structural)
            .Collection<Base::Object>("Behaviors", &::Aero::MetadataSupport::AddInteractionBehavior, &::Aero::MetadataSupport::ClearInteractionBehaviors, PropertyFlags::Attached | PropertyFlags::Structural);
}
