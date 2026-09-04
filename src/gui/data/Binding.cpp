#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/SolidColorBrush.hpp>
#include <Aero/Media/StreamGeometry.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>


#include "gui/data/BindingInternal.hpp"


namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Data;

BindingEngine::BindingEngine(
    Dispatcher& dispatcher,
    Meta::Registry* metadata) noexcept
    : dispatcher_(&dispatcher),
      metadata_(metadata),
      bindings_(),
      handleIndexMap_(),
      propertyChangedHandler_(this, &BindingEngine::OnPropertyChanged) {}

BindingEngine::~BindingEngine() noexcept {
    Shutdown();
}

Base::Result<void> BindingEngine::Initialize() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess();
    }
    if (initialized_) {
        return {};
    }
    // P3.2: ViewFrame drives DataBindHook() directly; no DataBind hook.
    initialized_ = true;
    return {};
}

void BindingEngine::Shutdown() noexcept {
    while (!bindings_.Empty()) {
        RemoveAt(bindings_.Size() - 1U);
    }
    deferredBindings_.Clear();
    pendingDeferredActivations_.Clear();
    flushing_ = false;
}

Base::Result<BindingHandle> BindingEngine::Attach(
    const BindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_) {
        return InvalidState("BindingEngine must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot attach while flushing");
    }
    Base::Result<void> valid = VerifyDescriptor(descriptor);
    if (!valid) {
        return valid.GetStatus();
    }
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Binding handle sequence is exhausted");
    }

    BindingRecord record;
    record.handle.value = nextHandle_++;
    record.handle.engine_ = this;
    record.descriptor = descriptor;
    record.descriptor.mode = ResolveBindingMode(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.mode);
    record.descriptor.updateSourceTrigger = ResolveUpdateSourceTrigger(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.updateSourceTrigger);
    const std::uint32_t newIndex = bindings_.Size();
    Base::Result<void> appended = bindings_.PushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    static_cast<void>(handleIndexMap_.Insert(bindings_.Back().handle.value, newIndex));
    descriptor.source->AddValueChangedHandler(
        descriptor.sourceProperty, propertyChangedHandler_);
    descriptor.target->AddValueChangedHandler(
        descriptor.targetProperty, propertyChangedHandler_);
    Base::Result<void> lostFocus =
        SubscribeLostFocus(bindings_.Back());
    if (!lostFocus) {
        RemoveAt(bindings_.Size() - 1U);
        return lostFocus.GetStatus();
    }
    return bindings_.Back().handle;
}

Base::Result<BindingHandle> BindingEngine::Attach(
    const MetadataBindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_) {
        return InvalidState("BindingEngine must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot attach while flushing");
    }
    Base::Result<void> valid = VerifyDescriptor(descriptor);
    if (!valid) return valid.GetStatus();
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Binding handle sequence is exhausted");
    }

    BindingRecord record;
    record.handle.value = nextHandle_++;
    record.handle.engine_ = this;
    record.sourceKind = descriptor.source != nullptr
        ? (descriptor.bindsToSource
            ? BindingSourceKind::MetadataObject
            : BindingSourceKind::MetadataPath)
        : BindingSourceKind::DataContext;
    record.metadata = descriptor.metadata;
    record.metadataSource = descriptor.source;
    record.dataContextProperty = descriptor.dataContextProperty;
    record.dataContextOwner = descriptor.dataContextOwner != nullptr
        ? descriptor.dataContextOwner
        : descriptor.target;
    // A Binding authored on FrameworkElement.DataContext reads from the
    // inherited parent DataContext. Reading the target property itself would
    // create a self-reference and hide the inherited value while the
    // expression is unresolved.
    if (record.sourceKind == BindingSourceKind::DataContext &&
        descriptor.targetProperty == descriptor.dataContextProperty &&
        record.metadata->Types().IsDerivedFrom(
            descriptor.target->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        auto& targetElement =
            *static_cast<FrameworkElement*>(descriptor.target);
        ::Aero::Media::Visual* parent = ::Aero::TryCast<::Aero::Media::Visual>(targetElement.GetLogicalParent());
        if (parent == nullptr) parent = targetElement.GetVisualParent();
        if (parent != nullptr && ::Aero::TryCast<::Aero::FrameworkElement>(parent) != nullptr) {
            record.dataContextOwner = ::Aero::TryCast<::Aero::FrameworkElement>(parent);
        }
    }
    record.descriptor.target = descriptor.target;
    record.descriptor.targetProperty = descriptor.targetProperty;
    record.descriptor.mode = ResolveBindingMode(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.mode);
    record.descriptor.updateSourceTrigger = ResolveUpdateSourceTrigger(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.updateSourceTrigger);
    record.descriptor.convert = descriptor.convert;
    record.descriptor.convertBack = descriptor.convertBack;
    record.descriptor.converterResource = descriptor.converterResource;
    record.descriptor.converterParameter = descriptor.converterParameter;
    record.descriptor.validate = descriptor.validate;
    record.descriptor.validateBack = descriptor.validateBack;
    record.descriptor.conversionContext =
        descriptor.conversionContext;
    record.descriptor.fallbackValue = descriptor.fallbackValue;
    record.descriptor.targetNullValue = descriptor.targetNullValue;
    record.descriptor.diagnostic = descriptor.diagnostic;
    record.descriptor.diagnosticContext =
        descriptor.diagnosticContext;
    record.bindsToSource = descriptor.bindsToSource;
    Base::Result<void> assigned = record.path.Assign(descriptor.path);
    if (!assigned) {
        --nextHandle_;
        return assigned.GetStatus();
    }
    assigned = record.stringFormat.Assign(
        descriptor.stringFormat);
    if (!assigned) {
        --nextHandle_;
        return assigned.GetStatus();
    }

    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        BindingPathCompileError compileError;
        Base::Result<BindingPathPlan> compiled = BindingPathPlan::Compile(
            *record.metadata,
            record.metadataSource->RuntimeType(),
            record.path.View(),
            &compileError);
        if (!compiled) {
            --nextHandle_;
            return BindingPathFailure(
                record.metadata->Types(),
                record.path.View(),
                compileError,
                compiled.GetStatus());
        }
        record.pathPlan = std::move(compiled).Value();
        const DependencyProperty* targetProperty =
            descriptor.target->PropertyRegistry().Find(
                descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (descriptor.convert == nullptr &&
             !descriptor.converterResource &&
             !TargetAcceptsPathResult(
                 record.metadata->Types(),
                 targetProperty,
                 record.pathPlan.ResultType(),
                 record.pathPlan.HasDynamicResult()))) {
            --nextHandle_;
            return BindingTypeMismatch(
                record.metadata->Types(),
                record.path.View(),
                record.pathPlan.ResultType(),
                *descriptor.target,
                targetProperty);
        }
        const bool wantsWriteback =
            record.descriptor.mode == BindingMode::TwoWay ||
            record.descriptor.mode == BindingMode::OneWayToSource;
        if (wantsWriteback && !record.pathPlan.CanWrite()) {
            if (descriptor.mode != BindingMode::Default ||
                record.descriptor.mode == BindingMode::OneWayToSource) {
                --nextHandle_;
                return Base::Status::Failure(
                    Base::ErrorCode::ReadOnly,
                    "Binding source path is not writable");
            }
            record.descriptor.mode = BindingMode::OneWay;
        }
        if (wantsWriteback &&
            record.descriptor.mode != BindingMode::OneWay &&
            !record.pathPlan.HasDynamicResult() &&
            targetProperty->ValueType() != record.pathPlan.ResultType() &&
            descriptor.convertBack == nullptr &&
            !descriptor.converterResource &&
            !HasDefaultTargetConversion(
                targetProperty->ValueType(),
                record.pathPlan.ResultType()) &&
            !CanRoundTripObjectValue(
                record.metadata->Types(),
                record.pathPlan.ResultType(),
                targetProperty->ValueType())) {
            if (descriptor.mode != BindingMode::Default ||
                record.descriptor.mode == BindingMode::OneWayToSource) {
                --nextHandle_;
                return InvalidArgument(
                    "Binding requires ConvertBack for different source and target types");
            }
            record.descriptor.mode = BindingMode::OneWay;
        }
    } else if (record.sourceKind == BindingSourceKind::MetadataObject) {
        const DependencyProperty* targetProperty =
            descriptor.target->PropertyRegistry().Find(
                descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (!targetProperty->AcceptsAnyValue() &&
            !record.metadata->Types().IsAssignableFrom(
                targetProperty->ValueType(),
                record.metadataSource->RuntimeType()))) {
            --nextHandle_;
            return BindingTypeMismatch(
                record.metadata->Types(),
                Base::StringView("."),
                record.metadataSource->RuntimeType(),
                *descriptor.target,
                targetProperty);
        }
        if (record.descriptor.mode == BindingMode::TwoWay ||
            record.descriptor.mode == BindingMode::OneWayToSource) {
            if (descriptor.mode != BindingMode::Default ||
                record.descriptor.mode == BindingMode::OneWayToSource) {
                --nextHandle_;
                return Base::Status::Failure(
                    Base::ErrorCode::ReadOnly,
                    "Binding to a source object does not support writeback");
            }
            record.descriptor.mode = BindingMode::OneWay;
        }
    }

    const std::uint32_t newIndex = bindings_.Size();
    Base::Result<void> appended =
        bindings_.PushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    BindingRecord& stored = bindings_.Back();
    static_cast<void>(handleIndexMap_.Insert(stored.handle.value, newIndex));
    descriptor.target->AddValueChangedHandler(
        descriptor.targetProperty, propertyChangedHandler_);
    if (stored.sourceKind == BindingSourceKind::DataContext) {
        stored.dataContextOwner->AddValueChangedHandler(
            descriptor.dataContextProperty,
            propertyChangedHandler_);
    } else if (stored.sourceKind == BindingSourceKind::MetadataPath) {
        Base::Result<void> sourceSubscription =
            SubscribeMetadataSource(stored);
        if (!sourceSubscription) {
            RemoveAt(bindings_.Size() - 1U);
            return sourceSubscription.GetStatus();
        }
    }
    Base::Result<void> lostFocus = SubscribeLostFocus(stored);
    if (!lostFocus) {
        RemoveAt(bindings_.Size() - 1U);
        return lostFocus.GetStatus();
    }
    return stored.handle;
}

Base::Result<void> BindingEngine::QueueDeferred(
    const MetadataBindingDescriptor& descriptor) noexcept {
    if (descriptor.metadata == nullptr ||
        descriptor.target == nullptr ||
        !descriptor.targetProperty.IsValid() ||
        (descriptor.path.Empty() && !descriptor.bindsToSource)) {
        return InvalidArgument(
            "Deferred Binding descriptor is invalid");
    }
    if (descriptor.source == nullptr &&
        !descriptor.dataContextProperty.IsValid()) {
        return InvalidArgument(
            "Deferred DataContext Binding requires a DataContext property");
    }
    DeferredBindingRecord record;
    record.metadata = descriptor.metadata;
    record.source = descriptor.source;
    record.target = descriptor.target;
    record.targetProperty = descriptor.targetProperty;
    record.dataContextProperty =
        descriptor.dataContextProperty;
    record.dataContextOwner = descriptor.dataContextOwner;
    record.mode = descriptor.mode;
    record.updateSourceTrigger =
        descriptor.updateSourceTrigger;
    record.bindsToSource = descriptor.bindsToSource;
    record.convert = descriptor.convert;
    record.convertBack = descriptor.convertBack;
    record.converterResource = descriptor.converterResource;
    record.converterParameter = descriptor.converterParameter;
    record.validate = descriptor.validate;
    record.validateBack = descriptor.validateBack;
    record.conversionContext =
        descriptor.conversionContext;
    record.fallbackValue = descriptor.fallbackValue;
    record.targetNullValue =
        descriptor.targetNullValue;
    record.diagnostic = descriptor.diagnostic;
    record.diagnosticContext =
        descriptor.diagnosticContext;
    Base::Result<void> assigned =
        record.path.Assign(descriptor.path);
    if (!assigned) return assigned.GetStatus();
    assigned = record.stringFormat.Assign(
        descriptor.stringFormat);
    if (!assigned) return assigned.GetStatus();
    return deferredBindings_.PushBack(
        std::move(record));
}

Base::Result<std::uint32_t>
BindingEngine::ActivateDeferred(
    DependencyObject& target) noexcept {
    std::uint32_t activated = 0U;
    for (std::uint32_t index = 0U;
         index < deferredBindings_.Size();) {
        DeferredBindingRecord& record =
            deferredBindings_[index];
        if (record.target != &target) {
            ++index;
            continue;
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = record.metadata;
        descriptor.source = record.source;
        descriptor.target = record.target;
        descriptor.targetProperty =
            record.targetProperty;
        descriptor.dataContextProperty =
            record.dataContextProperty;
        descriptor.dataContextOwner = record.dataContextOwner;
        descriptor.path = record.path.View();
        descriptor.stringFormat =
            record.stringFormat.View();
        descriptor.bindsToSource = record.bindsToSource;
        descriptor.mode = record.mode;
        descriptor.updateSourceTrigger =
            record.updateSourceTrigger;
        descriptor.convert = record.convert;
        descriptor.convertBack = record.convertBack;
        descriptor.converterResource = record.converterResource;
        descriptor.converterParameter = record.converterParameter;
        descriptor.validate = record.validate;
        descriptor.validateBack =
            record.validateBack;
        descriptor.conversionContext =
            record.conversionContext;
        descriptor.fallbackValue =
            record.fallbackValue;
        descriptor.targetNullValue =
            record.targetNullValue;
        descriptor.diagnostic = record.diagnostic;
        descriptor.diagnosticContext =
            record.diagnosticContext;
        Base::Result<BindingHandle> attached =
            Attach(descriptor);
        for (std::uint32_t move = index + 1U;
             move < deferredBindings_.Size();
             ++move) {
            deferredBindings_[move - 1U] =
                std::move(deferredBindings_[move]);
        }
        (void)deferredBindings_.Resize(
            deferredBindings_.Size() - 1U);
        if (!attached) {
            RecordError(attached.GetStatus());
            continue;
        }
        ++activated;
    }
    return activated;
}

Base::Result<void> BindingEngine::ActivateDeferredWhenReady(
    DependencyObject& target) noexcept {
    if (!flushing_) {
        Base::Result<std::uint32_t> activated =
            ActivateDeferred(target);
        return activated
            ? Base::Result<void>{}
            : Base::Result<void>(activated.GetStatus());
    }
    for (DependencyObject* pending : pendingDeferredActivations_) {
        if (pending == &target) return {};
    }
    return pendingDeferredActivations_.PushBack(&target);
}

Base::Result<std::uint32_t>
BindingEngine::ActivatePendingDeferred() noexcept {
    if (flushing_) {
        return InvalidState(
            "Deferred Binding activation cannot run while flushing");
    }
    Base::Vector<DependencyObject*> pending =
        std::move(pendingDeferredActivations_);
    pendingDeferredActivations_.Clear();
    std::uint32_t activated = 0U;
    for (DependencyObject* target : pending) {
        if (target == nullptr) continue;
        Base::Result<std::uint32_t> current =
            ActivateDeferred(*target);
        if (!current) {
            RecordError(current.GetStatus());
            continue;
        }
        activated += current.Value();
    }
    return activated;
}

Base::Result<bool> BindingEngine::Detach(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot detach while flushing");
    }
    if (handle.value == 0U) {
        return false;
    }
    const std::uint32_t* found = handleIndexMap_.Find(handle.value);
    if (found != nullptr && *found < bindings_.Size() && bindings_[*found].handle.value == handle.value) {
        RemoveAt(*found);
        return true;
    }
    return false;
}

Base::Result<bool> BindingEngine::UpdateSource(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_ || flushing_) {
        return InvalidState("BindingEngine is not ready to update a source");
    }
    BindingRecord* record = FindRecord(handle);
    if (record == nullptr) {
        return false;
    }
    if (record->descriptor.mode != BindingMode::TwoWay &&
        record->descriptor.mode != BindingMode::OneWayToSource) {
        return false;
    }
    record->targetDirty = true;
    record->forceSourceUpdate = true;
    return true;
}

Base::Result<bool> BindingEngine::UpdateTarget(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_ || flushing_) {
        return InvalidState("BindingEngine is not ready to update a target");
    }
    BindingRecord* record = FindRecord(handle);
    if (record == nullptr) return false;
    record->sourceDirty = true;
    Base::Result<std::uint32_t> flushed = Flush();
    if (!flushed) return flushed.GetStatus();
    return true;
}

BindingHandle BindingEngine::FindBinding(
    DependencyObject& target,
    DependencyPropertyHandle property) const noexcept {
    for (const BindingRecord& record : bindings_) {
        if (record.descriptor.target == &target &&
            record.descriptor.targetProperty == property) {
            return record.handle;
        }
    }
    return {};
}

Data::BindingStatus BindingEngine::QueryStatus(
    BindingHandle handle) const noexcept {
    const BindingRecord* record = FindRecord(handle);
    if (record == nullptr) return Data::BindingStatus::Unattached;
    if (!record->lastStatus.IsOk()) {
        switch (record->conversionFailureStage) {
        case BindingDiagnosticStage::ConvertBack:
        case BindingDiagnosticStage::ValidateBack:
        case BindingDiagnosticStage::WriteSource:
            return Data::BindingStatus::UpdateSourceError;
        default:
            return Data::BindingStatus::UpdateTargetError;
        }
    }
    return record->applied
        ? Data::BindingStatus::Active
        : Data::BindingStatus::Inactive;
}

bool BindingEngine::Contains(BindingHandle handle) const noexcept {
    return FindRecord(handle) != nullptr;
}

UpdateSourceTrigger BindingEngine::ResolveUpdateSourceTrigger(
    DependencyObject& target,
    DependencyPropertyHandle property,
    UpdateSourceTrigger requested) noexcept {
    if (requested != UpdateSourceTrigger::Default) {
        return requested;
    }
    const DependencyProperty* info =
        target.PropertyRegistry().Find(property);
    if (info == nullptr) {
        return UpdateSourceTrigger::PropertyChanged;
    }
    const PropertyMetadata* metadata =
        info->MetadataFor(target.RuntimeType());
    if (metadata == nullptr ||
        metadata->defaultUpdateSourceTrigger ==
            UpdateSourceTrigger::Default) {
        return UpdateSourceTrigger::PropertyChanged;
    }
    return metadata->defaultUpdateSourceTrigger;
}

BindingMode BindingEngine::ResolveBindingMode(
    DependencyObject& target,
    DependencyPropertyHandle property,
    BindingMode requested) noexcept {
    if (requested != BindingMode::Default) {
        return requested;
    }
    const DependencyProperty* info =
        target.PropertyRegistry().Find(property);
    if (info == nullptr) {
        return BindingMode::OneWay;
    }
    const PropertyMetadata* metadata =
        info->MetadataFor(target.RuntimeType());
    if (metadata != nullptr &&
        HasFlag(
            metadata->flags,
            PropertyMetadataFlags::BindsTwoWayByDefault)) {
        return BindingMode::TwoWay;
    }
    return BindingMode::OneWay;
}

void BindingEngine::RegisterMultiBinding(
    DependencyObject& target,
    DependencyPropertyHandle property,
    Base::Span<const Data::BindingHandle> handles) noexcept {
    MultiBindingGroup group;
    group.target = &target;
    group.targetProperty = property;
    for (std::uint32_t index = 0U; index < handles.Size(); ++index) {
        if (!group.handles.PushBack(handles[index])) return;
    }
    static_cast<void>(multiBindings_.PushBack(std::move(group)));
}

Data::MultiBindingExpression BindingEngine::FindMultiBinding(
    DependencyObject& target,
    DependencyPropertyHandle property) const noexcept {
    Data::MultiBindingExpression expression;
    for (const MultiBindingGroup& group : multiBindings_) {
        if (group.target != &target ||
            group.targetProperty != property) {
            continue;
        }
        for (std::uint32_t index = 0U; index < group.handles.Size(); ++index) {
            if (!expression.handles_.PushBack(group.handles[index])) {
                return {};
            }
        }
        return expression;
    }
    return {};
}

BindingEngine::BindingRecord* BindingEngine::FindRecord(
    BindingHandle handle) noexcept {
    if (!handle.IsValid() || handle.engine_ != this) return nullptr;
    const std::uint32_t* found = handleIndexMap_.Find(handle.value);
    if (found != nullptr && *found < bindings_.Size() && bindings_[*found].handle.value == handle.value) {
        return &bindings_[*found];
    }
    return nullptr;
}

const BindingEngine::BindingRecord* BindingEngine::FindRecord(
    BindingHandle handle) const noexcept {
    if (!handle.IsValid() || handle.engine_ != this) return nullptr;
    const std::uint32_t* found = handleIndexMap_.Find(handle.value);
    if (found != nullptr && *found < bindings_.Size() && bindings_[*found].handle.value == handle.value) {
        return &bindings_[*found];
    }
    return nullptr;
}

Base::Result<void> BindingEngine::SubscribeLostFocus(
    BindingRecord& record) noexcept {
    if (record.descriptor.updateSourceTrigger !=
            UpdateSourceTrigger::LostFocus ||
        record.descriptor.target == nullptr) {
        return {};
    }
    UIElement* element = TryCast<UIElement>(record.descriptor.target);
    if (element == nullptr) return {};
    if (record.descriptor.targetProperty ==
        UIElement::IsKeyboardFocusedProperty.Handle()) {
        record.lostFocusSubscribed = true;
        return {};
    }
    element->AddValueChangedHandler(
        UIElement::IsKeyboardFocusedProperty.Handle(),
        propertyChangedHandler_);
    record.lostFocusSubscribed = true;
    return {};
}

void BindingEngine::UnsubscribeLostFocus(BindingRecord& record) noexcept {
    if (!record.lostFocusSubscribed || record.descriptor.target == nullptr) {
        return;
    }
    UIElement* element = TryCast<UIElement>(record.descriptor.target);
    if (element == nullptr) return;
    if (record.descriptor.targetProperty ==
        UIElement::IsKeyboardFocusedProperty.Handle()) {
        record.lostFocusSubscribed = false;
        return;
    }
    (void)element->RemoveValueChangedHandler(
        UIElement::IsKeyboardFocusedProperty.Handle(),
        propertyChangedHandler_);
    record.lostFocusSubscribed = false;
}

Base::Result<std::uint32_t> BindingEngine::DetachObject(
    DependencyObject& object) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot detach objects while flushing");
    }
    std::uint32_t detached = 0U;
    for (std::uint32_t index = 0U; index < bindings_.Size();) {
        const BindingRecord& record = bindings_[index];
        if (record.descriptor.source != &object &&
            record.metadataSource != &object &&
            record.descriptor.target != &object &&
            record.dataContextOwner != &object) {
            ++index;
            continue;
        }
        RemoveAt(index);
        ++detached;
    }
    for (std::uint32_t index = 0U;
         index < deferredBindings_.Size();) {
        const DeferredBindingRecord& record =
            deferredBindings_[index];
        if (record.source != &object &&
            record.target != &object &&
            record.dataContextOwner != &object) {
            ++index;
            continue;
        }
        for (std::uint32_t move = index + 1U;
             move < deferredBindings_.Size();
             ++move) {
            deferredBindings_[move - 1U] =
                std::move(deferredBindings_[move]);
        }
        (void)deferredBindings_.Resize(
            deferredBindings_.Size() - 1U);
        ++detached;
    }
    for (std::uint32_t index = 0U;
         index < pendingDeferredActivations_.Size();) {
        if (pendingDeferredActivations_[index] != &object) {
            ++index;
            continue;
        }
        for (std::uint32_t move = index + 1U;
             move < pendingDeferredActivations_.Size(); ++move) {
            pendingDeferredActivations_[move - 1U] =
                pendingDeferredActivations_[move];
        }
        pendingDeferredActivations_.PopBack();
    }
    return detached;
}



} // namespace Aero

namespace Aero::Data {

Base::Result<Value> BooleanToVisibilityConverter::Convert(
    const Value& value,
    const Value& parameter) noexcept {
    (void)parameter;
    Base::Result<bool> converted =
        Meta::ValueCodec<bool>::Decode(value);
    if (!converted) return converted.GetStatus();
    return Meta::ValueCodec<Aero::Visibility>::Encode(
        converted.Value()
            ? Aero::Visibility::Visible
            : Aero::Visibility::Collapsed);
}

Base::Result<Value> BooleanToVisibilityConverter::ConvertBack(
    const Value& value,
    const Value& parameter) noexcept {
    (void)parameter;
    Base::Result<Aero::Visibility> converted =
        Meta::ValueCodec<Aero::Visibility>::Decode(value);
    if (!converted) return converted.GetStatus();
    return Meta::ValueCodec<bool>::Encode(
        converted.Value() == Aero::Visibility::Visible);
}

Base::Ref<RelativeSource> RelativeSource::ForSelf() noexcept {
    Base::Result<Base::Ref<RelativeSource>> source = Base::MakeRef<RelativeSource>(RelativeSourceMode::Self);
    return source ? std::move(source).Value() : Base::Ref<RelativeSource>{};
}

Base::Ref<RelativeSource> RelativeSource::ForTemplatedParent() noexcept {
    Base::Result<Base::Ref<RelativeSource>> source = Base::MakeRef<RelativeSource>(RelativeSourceMode::TemplatedParent);
    return source ? std::move(source).Value() : Base::Ref<RelativeSource>{};
}

} // namespace Aero::Data
