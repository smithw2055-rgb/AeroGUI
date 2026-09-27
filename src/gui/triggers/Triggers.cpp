#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Value.hpp>

#include "gui/core/ValueConversion.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
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

namespace Aero {

// ===== TriggerBase =====

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept {
    if (value.Kind() != ValueKind::Object ||
        value.IsNullObject()) {
        return false;
    }
    if (value.Type() == Data::Binding::StaticTypeId()) {
        return true;
    }
    if (value.AsObject() &&
        value.AsObject()->RuntimeType() ==
            Data::Binding::StaticTypeId()) {
        return true;
    }
    if (value.Kind() == ValueKind::String) {
        const Base::StringView text = value.AsString();
        constexpr Base::StringView prefix("\x01DynamicResource:");
        if (text.SizeBytes() >= prefix.SizeBytes() &&
            text.Substr(0U, prefix.SizeBytes()) == prefix) {
            return true;
        }
    }
    return false;
}

void TriggerBase::AddEnterAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    enterActions_.PushBack(
        std::move(action));
}

void TriggerBase::AddExitAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    exitActions_.PushBack(
        std::move(action));
}

// ===== Trigger =====

void Trigger::SetProperty(
    DependencyPropertyHandle value) noexcept {
    if (!value.IsValid()) return;
    property_ = value;
}

void Trigger::SetValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    value_ = value;
}

void Trigger::AddSetter(
    const Setter& setter) noexcept {
    if (!setter.GetProperty().IsValid() ||
        setter.GetValue().IsUnset()) { AERO_ASSERT(false); return; }
    setterProperties_.PushBack(setter.GetProperty());
    setterValues_.PushBack(setter.GetValue());
}

void Trigger::SetPropertyName(
    Base::StringView value) noexcept {
    if (value.Empty()) return;
    Base::String candidate;
    if (!candidate.Assign(value)) return;
    propertyName_ = std::move(candidate);
}

void Trigger::SetSourceName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    sourceName_ = std::move(candidate);
}

void Trigger::SetAuthoredValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    authoredValue_ = value;
}

void Trigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(
        std::move(setter));
}

void Trigger::ClearAuthoredSetters() noexcept {
    authoredSetters_.Clear();
}

// ===== DataTrigger =====

void DataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(
        std::move(setter));
}

void DataTrigger::SetPropertyName(StringView value) noexcept {
    static_cast<void>(propertyName_.Assign(value));
}

void DataTrigger::SetSourceName(StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(value));
}

// ===== MultiTrigger =====

void MultiTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    conditions_.PushBack(std::move(condition));
}

void MultiTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(std::move(setter));
}

// ===== MultiDataTrigger =====

void MultiDataTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    conditions_.PushBack(
        std::move(condition));
}

void MultiDataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(
        std::move(setter));
}

// ===== Condition =====

void Condition::SetPropertyName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    propertyName_ = std::move(candidate);
}

void Condition::SetSourceName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    sourceName_ = std::move(candidate);
}

} // namespace Aero

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

void AddTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != Setter::StaticTypeId()) {
        return;
    }
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) {
        return;
    }
    static_cast<Trigger&>(owner).AddAuthoredSetter(
        std::move(retained));
}

void ClearTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Trigger&>(owner).ClearAuthoredSetters();
    return;
}

void AddTriggerEnterAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    static_cast<void>(
        static_cast<TriggerBase&>(owner).AddEnterAction(value));
}

void ClearTriggerEnterActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TriggerBase&>(owner)
        .ClearEnterActions();
    return;
}

void AddTriggerExitAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    static_cast<void>(
        static_cast<TriggerBase&>(owner).AddExitAction(value));
}

void ClearTriggerExitActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TriggerBase&>(owner)
        .ClearExitActions();
    return;
}

void AddDataTriggerContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    auto& trigger = static_cast<DataTrigger&>(owner);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        Base::Ref<Setter> setter = Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
        if (setter) trigger.AddAuthoredSetter(std::move(setter));
        return;
    }
    trigger.AddEnterAction(value);
}

void ClearDataTriggerContent(Base::Object& owner, void*) noexcept {
    auto& trigger = static_cast<DataTrigger&>(owner);
    trigger.ClearAuthoredSetters();
    trigger.ClearEnterActions();
}

void AddMultiDataCondition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Condition> retained =
        Base::Ref<Condition>::TryFromBorrowed(
            static_cast<Condition&>(*value));
    if (!retained) return;
    static_cast<MultiDataTrigger&>(owner).AddCondition(
        std::move(retained));
}

void ClearMultiDataConditions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiDataTrigger&>(owner)
        .ClearConditions();
    return;
}

void AddMultiDataSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) return;
    static_cast<MultiDataTrigger&>(owner).AddAuthoredSetter(
        std::move(retained));
}

void ClearMultiDataSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiDataTrigger&>(owner)
        .ClearAuthoredSetters();
    return;
}

void AddMultiTriggerCondition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Condition> retained =
        Base::Ref<Condition>::TryFromBorrowed(
            static_cast<Condition&>(*value));
    if (!retained) return;
    static_cast<MultiTrigger&>(owner).AddCondition(
        std::move(retained));
}

void ClearMultiTriggerConditions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiTrigger&>(owner).ClearConditions();
    return;
}

void AddMultiTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) return;
    static_cast<MultiTrigger&>(owner).AddAuthoredSetter(
        std::move(retained));
}

void ClearMultiTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiTrigger&>(owner).ClearAuthoredSetters();
    return;
}

} // namespace
} // namespace Aero::MetadataSupport

AERO_DESCRIBE(::Aero::SetterBase) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<SetterBase>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(::Aero::Setter) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<Setter>(context)
            .Property("TargetName", &Setter::GetTargetName, &Setter::SetTargetName)
            .Property("Property", &Setter::GetPropertyName, &Setter::SetPropertyName)
            .Property<Value, &Setter::GetAuthoredValue, &Setter::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(::Aero::EventSetter) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<EventSetter>(context)
            .Property("Handler", &EventSetter::GetHandlerName, &EventSetter::SetHandlerName)
            .Factory();
}

AERO_DESCRIBE(::Aero::TriggerBase) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<TriggerBase>(context, TypeFlags::Abstract)
            .Collection<Aero::Interactivity::TriggerAction>("EnterActions", &::Aero::MetadataSupport::AddTriggerEnterAction, &::Aero::MetadataSupport::ClearTriggerEnterActions)
            .Collection<Aero::Interactivity::TriggerAction>("ExitActions", &::Aero::MetadataSupport::AddTriggerExitAction, &::Aero::MetadataSupport::ClearTriggerExitActions);
}

AERO_DESCRIBE(::Aero::Trigger) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<Trigger>(context)
            .Property("Property", &Trigger::GetPropertyName, &Trigger::SetPropertyName)
            .Property("SourceName", &Trigger::GetSourceName, &Trigger::SetSourceName)
            .Property<Value, &Trigger::GetAuthoredValue, &Trigger::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Content<Setter>("Setters", ContentKind::Collection, &::Aero::MetadataSupport::AddTriggerSetter, &::Aero::MetadataSupport::ClearTriggerSetters)
            .Factory();
}

AERO_DESCRIBE(::Aero::DataTrigger) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<DataTrigger>(context)
            .Property<Base::Ref<Binding>, &DataTrigger::SetBinding>("Binding", PropertyFlags::Structural)
            .Property<Value, &DataTrigger::GetAuthoredValue, &DataTrigger::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Property("Comparison", &DataTrigger::GetComparison, &DataTrigger::SetComparison)
            .Content<Base::Object>("Setters", ContentKind::Collection, &::Aero::MetadataSupport::AddDataTriggerContent, &::Aero::MetadataSupport::ClearDataTriggerContent)
            .Factory();
}

AERO_DESCRIBE(::Aero::Condition) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<Condition>(context)
            .Property<Base::Ref<Binding>, &Condition::SetBinding>("Binding", PropertyFlags::Structural)
            .Property("Property", &Condition::GetPropertyName, &Condition::SetPropertyName)
            .Property("SourceName", &Condition::GetSourceName, &Condition::SetSourceName)
            .Property<Value, &Condition::GetAuthoredValue, &Condition::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(::Aero::MultiDataTrigger) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<MultiDataTrigger>(context)
            .Collection<Condition>("Conditions", &::Aero::MetadataSupport::AddMultiDataCondition, &::Aero::MetadataSupport::ClearMultiDataConditions)
            .Content<Setter>("Setters", ContentKind::Collection, &::Aero::MetadataSupport::AddMultiDataSetter, &::Aero::MetadataSupport::ClearMultiDataSetters)
            .Factory();
}

AERO_DESCRIBE(::Aero::MultiTrigger) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Data;
    Register<MultiTrigger>(context)
            .Collection<Condition>("Conditions", &::Aero::MetadataSupport::AddMultiTriggerCondition, &::Aero::MetadataSupport::ClearMultiTriggerConditions)
            .Content<Setter>("Setters", ContentKind::Collection, &::Aero::MetadataSupport::AddMultiTriggerSetter, &::Aero::MetadataSupport::ClearMultiTriggerSetters)
            .Factory();
}
