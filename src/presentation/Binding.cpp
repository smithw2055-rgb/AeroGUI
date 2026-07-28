#include <Aero/Presentation/Binding.hpp>

#include <utility>

namespace Aero::Presentation {

using namespace Aero::Core;
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

} // namespace

BindingManager::BindingManager(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher),
      bindings_(),
      propertyChangedHandler_(this, &BindingManager::OnPropertyChanged) {}

BindingManager::~BindingManager() noexcept {
    Shutdown();
}

Base::Result<void> BindingManager::Initialize() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess();
    }
    if (hook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> registered =
        dispatcher_->RegisterFrameHook(
            DispatcherFramePhase::DataBind,
            &BindingManager::DataBindHook,
            this);
    if (!registered) {
        return registered.GetStatus();
    }
    hook_ = registered.Value();
    return {};
}

void BindingManager::Shutdown() noexcept {
    if (hook_.IsValid()) {
        (void)dispatcher_->RemoveFrameHook(hook_);
        hook_ = {};
    }
    while (!bindings_.Empty()) {
        RemoveAt(bindings_.Size() - 1U);
    }
    flushing_ = false;
}

Base::Result<BindingHandle> BindingManager::Attach(
    const BindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid()) {
        return InvalidState("BindingManager must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingManager cannot attach while flushing");
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
    record.descriptor = descriptor;
    Base::Result<void> appended = bindings_.TryPushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    Base::Result<void> sourceSubscription =
        descriptor.source->TryAddValueChangedHandler(
            descriptor.sourceProperty, propertyChangedHandler_);
    if (!sourceSubscription) {
        RemoveAt(bindings_.Size() - 1U);
        return sourceSubscription.GetStatus();
    }
    Base::Result<void> targetSubscription =
        descriptor.target->TryAddValueChangedHandler(
            descriptor.targetProperty, propertyChangedHandler_);
    if (!targetSubscription) {
        (void)descriptor.source->RemoveValueChangedHandler(
            descriptor.sourceProperty, propertyChangedHandler_);
        RemoveAt(bindings_.Size() - 1U);
        return targetSubscription.GetStatus();
    }
    return bindings_.Back().handle;
}

Base::Result<BindingHandle> BindingManager::Attach(
    const MetadataBindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid()) {
        return InvalidState("BindingManager must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingManager cannot attach while flushing");
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
    record.sourceKind = descriptor.source != nullptr
        ? BindingSourceKind::MetadataPath
        : BindingSourceKind::DataContext;
    record.metadata = descriptor.metadata;
    record.metadataSource = descriptor.source;
    record.dataContextProperty = descriptor.dataContextProperty;
    record.descriptor.target = descriptor.target;
    record.descriptor.targetProperty = descriptor.targetProperty;
    record.descriptor.mode = descriptor.mode;
    record.descriptor.updateSourceTrigger =
        descriptor.updateSourceTrigger;
    record.descriptor.convert = descriptor.convert;
    record.descriptor.convertBack = descriptor.convertBack;
    record.descriptor.validate = descriptor.validate;
    record.descriptor.validateBack = descriptor.validateBack;
    record.descriptor.conversionContext =
        descriptor.conversionContext;
    record.descriptor.fallbackValue = descriptor.fallbackValue;
    record.descriptor.targetNullValue = descriptor.targetNullValue;
    record.descriptor.diagnostic = descriptor.diagnostic;
    record.descriptor.diagnosticContext =
        descriptor.diagnosticContext;
    Base::Result<void> assigned = record.path.TryAssign(descriptor.path);
    if (!assigned) {
        --nextHandle_;
        return assigned.GetStatus();
    }

    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        Base::Result<BindingPathPlan> compiled = BindingPathPlan::Compile(
            *record.metadata,
            record.metadataSource->RuntimeType(),
            record.path.View());
        if (!compiled) {
            --nextHandle_;
            return compiled.GetStatus();
        }
        record.pathPlan = std::move(compiled).Value();
        const DependencyProperty* targetProperty =
            descriptor.target->PropertyRegistry().Find(
                descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (descriptor.convert == nullptr &&
            !record.metadata->Types().IsAssignableFrom(
                targetProperty->ValueType(),
                record.pathPlan.ResultType()))) {
            --nextHandle_;
            return InvalidArgument(
                "Binding path result type does not match the target property");
        }
        if ((descriptor.mode == BindingMode::TwoWay ||
             descriptor.mode == BindingMode::OneWayToSource) &&
            !record.pathPlan.CanWrite()) {
            --nextHandle_;
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Binding source path is not writable");
        }
        if ((descriptor.mode == BindingMode::TwoWay ||
             descriptor.mode == BindingMode::OneWayToSource) &&
            targetProperty->ValueType() != record.pathPlan.ResultType() &&
            descriptor.convertBack == nullptr) {
            --nextHandle_;
            return InvalidArgument(
                "Binding requires ConvertBack for different source and target types");
        }
    }

    Base::Result<void> appended =
        bindings_.TryPushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    BindingRecord& stored = bindings_.Back();
    Base::Result<void> targetSubscription =
        descriptor.target->TryAddValueChangedHandler(
            descriptor.targetProperty, propertyChangedHandler_);
    if (!targetSubscription) {
        RemoveAt(bindings_.Size() - 1U);
        return targetSubscription.GetStatus();
    }
    if (stored.sourceKind == BindingSourceKind::DataContext) {
        Base::Result<void> contextSubscription =
            descriptor.target->TryAddValueChangedHandler(
                descriptor.dataContextProperty,
                propertyChangedHandler_);
        if (!contextSubscription) {
            RemoveAt(bindings_.Size() - 1U);
            return contextSubscription.GetStatus();
        }
    } else {
        Base::Result<void> sourceSubscription =
            SubscribeMetadataSource(stored);
        if (!sourceSubscription) {
            RemoveAt(bindings_.Size() - 1U);
            return sourceSubscription.GetStatus();
        }
    }
    return stored.handle;
}

Base::Result<bool> BindingManager::Detach(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (flushing_) {
        return InvalidState("BindingManager cannot detach while flushing");
    }
    if (!handle.IsValid()) {
        return false;
    }
    for (std::uint32_t index = 0U; index < bindings_.Size(); ++index) {
        if (bindings_[index].handle.value == handle.value) {
            RemoveAt(index);
            return true;
        }
    }
    return false;
}

Base::Result<bool> BindingManager::UpdateSource(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid() || flushing_) {
        return InvalidState("BindingManager is not ready to update a source");
    }
    for (BindingRecord& record : bindings_) {
        if (record.handle.value != handle.value) {
            continue;
        }
        if (record.descriptor.mode != BindingMode::TwoWay &&
            record.descriptor.mode != BindingMode::OneWayToSource) {
            return false;
        }
        record.targetDirty = true;
        record.forceSourceUpdate = true;
        return true;
    }
    return false;
}

Base::Result<std::uint32_t> BindingManager::DetachObject(
    DependencyObject& object) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (flushing_) {
        return InvalidState("BindingManager cannot detach objects while flushing");
    }
    std::uint32_t detached = 0U;
    for (std::uint32_t index = 0U; index < bindings_.Size();) {
        const BindingRecord& record = bindings_[index];
        if (record.descriptor.source != &object &&
            record.metadataSource != &object &&
            record.descriptor.target != &object) {
            ++index;
            continue;
        }
        RemoveAt(index);
        ++detached;
    }
    return detached;
}

Base::Result<std::uint32_t> BindingManager::Flush() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!hook_.IsValid()) {
        return InvalidState("BindingManager is not initialized");
    }
    if (flushing_) {
        return InvalidState("BindingManager cannot flush recursively");
    }

    flushing_ = true;
    lastError_ = {};
    const std::uint32_t snapshotCount = bindings_.Size();
    std::uint32_t updated = 0U;
    for (std::uint32_t index = 0U; index < snapshotCount; ++index) {
        BindingRecord& record = bindings_[index];
        if (record.descriptor.mode == BindingMode::OneTime && record.applied) {
            continue;
        }
        const bool metadataPath =
            record.sourceKind != BindingSourceKind::DependencyProperty;
        if (record.applied && !record.sourceDirty &&
            !record.targetDirty && !metadataPath) {
            continue;
        }

        Base::Result<PropertyValue> source = ReadSource(record);
        bool usedFallback = false;
        if (!source) {
            const BindingDiagnosticStage stage =
                record.sourceKind == BindingSourceKind::DataContext &&
                source.GetStatus().code == Base::ErrorCode::NotFound
                    ? BindingDiagnosticStage::ResolveSource
                    : BindingDiagnosticStage::ReadSource;
            ReportDiagnostic(record, stage, source.GetStatus());
            if (record.descriptor.fallbackValue.IsUnset()) {
                record.applied = false;
                record.sourceDirty = true;
                continue;
            }
            source = record.descriptor.fallbackValue;
            usedFallback = true;
        }
        Base::Result<PropertyValue> target =
            record.descriptor.target->GetValue(record.descriptor.targetProperty);
        if (!target) {
            ReportDiagnostic(
                record,
                BindingDiagnosticStage::WriteTarget,
                target.GetStatus());
            continue;
        }

        const bool sourceChanged = !record.applied ||
            record.sourceDirty ||
            usedFallback ||
            (metadataPath &&
             source.Value() != record.lastSourceValue);
        const bool targetChanged = record.descriptor.updateSourceTrigger ==
                UpdateSourceTrigger::Explicit
            ? record.forceSourceUpdate
            : (!record.applied || record.targetDirty);
        Base::Result<void> applied;
        switch (record.descriptor.mode) {
        case BindingMode::OneTime:
            if (!record.applied) {
                Base::Result<PropertyValue> converted = usedFallback
                    ? Base::Result<PropertyValue>(source.Value())
                    : ConvertForTarget(record, source.Value());
                if (!converted) {
                    ReportDiagnostic(
                        record,
                        record.conversionFailureStage,
                        converted.GetStatus());
                    if (record.descriptor.fallbackValue.IsUnset() ||
                        usedFallback) {
                        applied = converted.GetStatus();
                        break;
                    }
                    converted = record.descriptor.fallbackValue;
                    usedFallback = true;
                }
                applied = record.descriptor.target->SetCurrentValue(
                    record.descriptor.targetProperty, converted.Value());
                if (!applied) {
                    ReportDiagnostic(
                        record,
                        BindingDiagnosticStage::WriteTarget,
                        applied.GetStatus());
                    break;
                }
                target = converted.Value();
                ++updated;
            }
            break;
        case BindingMode::OneWay:
            if (sourceChanged) {
                Base::Result<PropertyValue> converted = usedFallback
                    ? Base::Result<PropertyValue>(source.Value())
                    : ConvertForTarget(record, source.Value());
                if (!converted) {
                    ReportDiagnostic(
                        record,
                        record.conversionFailureStage,
                        converted.GetStatus());
                    if (record.descriptor.fallbackValue.IsUnset() ||
                        usedFallback) {
                        applied = converted.GetStatus();
                        break;
                    }
                    converted = record.descriptor.fallbackValue;
                    usedFallback = true;
                }
                applied = record.descriptor.target->SetCurrentValue(
                    record.descriptor.targetProperty, converted.Value());
                if (!applied) {
                    ReportDiagnostic(
                        record,
                        BindingDiagnosticStage::WriteTarget,
                        applied.GetStatus());
                    break;
                }
                target = converted.Value();
                ++updated;
            }
            break;
        case BindingMode::OneWayToSource:
            if (targetChanged) {
                Base::Result<PropertyValue> converted =
                    ConvertForSource(record, target.Value());
                if (!converted) {
                    ReportDiagnostic(
                        record,
                        record.conversionFailureStage,
                        converted.GetStatus());
                    applied = converted.GetStatus();
                    break;
                }
                applied = WriteSource(record, converted.Value());
                if (!applied) {
                    ReportDiagnostic(
                        record,
                        BindingDiagnosticStage::WriteSource,
                        applied.GetStatus());
                    break;
                }
                source = converted.Value();
                ++updated;
            }
            break;
        case BindingMode::TwoWay:
            if (sourceChanged) {
                Base::Result<PropertyValue> converted = usedFallback
                    ? Base::Result<PropertyValue>(source.Value())
                    : ConvertForTarget(record, source.Value());
                if (!converted) {
                    ReportDiagnostic(
                        record,
                        record.conversionFailureStage,
                        converted.GetStatus());
                    if (record.descriptor.fallbackValue.IsUnset() ||
                        usedFallback) {
                        applied = converted.GetStatus();
                        break;
                    }
                    converted = record.descriptor.fallbackValue;
                    usedFallback = true;
                }
                applied = record.descriptor.target->SetCurrentValue(
                    record.descriptor.targetProperty, converted.Value());
                if (!applied) {
                    ReportDiagnostic(
                        record,
                        BindingDiagnosticStage::WriteTarget,
                        applied.GetStatus());
                    break;
                }
                target = converted.Value();
                ++updated;
            } else if (targetChanged) {
                Base::Result<PropertyValue> converted =
                    ConvertForSource(record, target.Value());
                if (!converted) {
                    ReportDiagnostic(
                        record,
                        record.conversionFailureStage,
                        converted.GetStatus());
                    applied = converted.GetStatus();
                    break;
                }
                applied = WriteSource(record, converted.Value());
                if (!applied) {
                    ReportDiagnostic(
                        record,
                        BindingDiagnosticStage::WriteSource,
                        applied.GetStatus());
                    break;
                }
                source = converted.Value();
                ++updated;
            }
            break;
        }
        if (!applied && (sourceChanged || targetChanged)) {
            record.sourceDirty = true;
            continue;
        }
        record.lastSourceValue = source.Value();
        record.lastTargetValue = target.Value();
        record.applied = true;
        record.sourceDirty = false;
        record.targetDirty = false;
        record.forceSourceUpdate = false;
    }
    flushing_ = false;
    return updated;
}

Base::Result<std::uint32_t>
BindingManager::InspectBindings(
    const DependencyObject& object,
    Base::Vector<BindingInspection>&
        output) const noexcept {
    output.Clear();
    for (const BindingRecord& record :
        bindings_) {
        Base::Object* source =
            record.sourceKind ==
                BindingSourceKind::
                    DependencyProperty
            ? static_cast<Base::Object*>(
                record.descriptor.source)
            : record.metadataSource;
        if (source != &object &&
            record.descriptor.target !=
                &object) {
            continue;
        }
        BindingInspection inspection;
        inspection.handle = record.handle;
        inspection.source = source;
        inspection.target =
            record.descriptor.target;
        inspection.sourceProperty =
            record.descriptor.
                sourceProperty;
        inspection.targetProperty =
            record.descriptor.
                targetProperty;
        inspection.mode =
            record.descriptor.mode;
        inspection.updateSourceTrigger =
            record.descriptor.
                updateSourceTrigger;
        inspection.usesDataContext =
            record.sourceKind ==
                BindingSourceKind::
                    DataContext;
        inspection.applied =
            record.applied;
        Base::Result<void> assigned =
            inspection.path.TryAssign(
                record.path.View());
        if (!assigned) {
            output.Clear();
            return assigned.GetStatus();
        }
        Base::Result<void> appended =
            output.TryPushBack(
                std::move(inspection));
        if (!appended) {
            output.Clear();
            return appended.GetStatus();
        }
    }
    return output.Size();
}

void BindingManager::DataBindHook(void* context) noexcept {
    BindingManager* manager = static_cast<BindingManager*>(context);
    if (manager != nullptr) {
        (void)manager->Flush();
    }
}

void BindingManager::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind == BindingSourceKind::DependencyProperty &&
            record.descriptor.source == &object &&
            record.descriptor.sourceProperty == args.property) {
            record.sourceDirty = true;
        }
        if (record.sourceKind == BindingSourceKind::DataContext &&
            record.descriptor.target == &object &&
            record.dataContextProperty == args.property) {
            ReleaseMetadataSource(record);
            record.metadataSource = nullptr;
            record.pathPlan = {};
            record.sourceDirty = true;
            record.applied = false;
        }
        if (record.descriptor.target == &object &&
            record.descriptor.targetProperty == args.property) {
            record.targetDirty = true;
        }
    }
}

void BindingManager::OnMetadataPropertyChanged(
    Base::Object& object,
    MemberId property) noexcept {
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind ==
                BindingSourceKind::DependencyProperty ||
            record.metadataSource != &object ||
            !record.pathPlan.IsValid() ||
            record.pathPlan.Segments().Empty()) {
            continue;
        }
        if (property == InvalidMemberId ||
            record.pathPlan.Segments()[0].member == property) {
            record.sourceDirty = true;
        }
    }
}

void BindingManager::MetadataPropertyChanged(
    Base::Object& object,
    MemberId property,
    void* context) noexcept {
    BindingManager* manager = static_cast<BindingManager*>(context);
    if (manager != nullptr) {
        manager->OnMetadataPropertyChanged(object, property);
    }
}

Base::Result<void> BindingManager::VerifyDescriptor(
    const BindingDescriptor& descriptor) const noexcept {
    if (descriptor.source == nullptr || descriptor.target == nullptr ||
        !descriptor.sourceProperty.IsValid() ||
        !descriptor.targetProperty.IsValid()) {
        return InvalidArgument("Binding descriptor is incomplete");
    }
    if (&descriptor.source->GetDispatcher() != dispatcher_ ||
        &descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument("Binding source and target must use this Dispatcher");
    }
    Base::Result<PropertyValue> source =
        descriptor.source->GetValue(descriptor.sourceProperty);
    if (!source) {
        return source.GetStatus();
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) {
        return target.GetStatus();
    }
    if (source.Value().Type() != target.Value().Type() &&
        descriptor.convert == nullptr) {
        return InvalidArgument("Binding source and target property types differ");
    }
    if ((descriptor.mode == BindingMode::TwoWay ||
         descriptor.mode == BindingMode::OneWayToSource) &&
        source.Value().Type() != target.Value().Type() &&
        descriptor.convertBack == nullptr) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    return {};
}

Base::Result<void> BindingManager::VerifyDescriptor(
    const MetadataBindingDescriptor& descriptor) const noexcept {
    if (descriptor.metadata == nullptr ||
        !descriptor.metadata->IsFrozen() ||
        descriptor.target == nullptr ||
        !descriptor.targetProperty.IsValid() ||
        descriptor.path.Empty() ||
        (descriptor.source == nullptr &&
         !descriptor.dataContextProperty.IsValid())) {
        return InvalidArgument("Metadata binding descriptor is incomplete");
    }
    if (&descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument(
            "Binding target must use this Dispatcher");
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) return target.GetStatus();
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    if (descriptor.source == nullptr) {
        Base::Result<PropertyValue> dataContext =
            descriptor.target->GetValue(
                descriptor.dataContextProperty);
        if (!dataContext) return dataContext.GetStatus();
        if (dataContext.Value().Kind() != ValueKind::Object) {
            return InvalidArgument(
                "Binding DataContext property must contain an object");
        }
    } else if (descriptor.source->RuntimeType() == InvalidTypeId ||
        descriptor.metadata->Types().FindType(
            descriptor.source->RuntimeType()) == nullptr) {
        return InvalidArgument(
            "Binding metadata source has no registered runtime type");
    }
    return {};
}

Base::Result<void> BindingManager::ResolveMetadataSource(
    BindingRecord& record) noexcept {
    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        return record.metadataSource != nullptr &&
            record.pathPlan.IsValid()
            ? Base::Result<void>()
            : Base::Result<void>(InvalidState(
                "Binding metadata source is not resolved"));
    }
    Base::Result<PropertyValue> dataContext =
        record.descriptor.target->GetValue(
            record.dataContextProperty);
    if (!dataContext) return dataContext.GetStatus();
    if (dataContext.Value().Kind() != ValueKind::Object) {
        return InvalidArgument(
            "Binding DataContext property is not an object");
    }
    if (dataContext.Value().IsNullObject()) {
        ReleaseMetadataSource(record);
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target has no DataContext object");
    }

    Base::Object* source = dataContext.Value().AsObject().Get();
    if (source == record.metadataSource &&
        record.pathPlan.IsValid()) {
        return {};
    }
    ReleaseMetadataSource(record);
    record.metadataSource = nullptr;
    record.pathPlan = {};

    Base::Result<BindingPathPlan> compiled =
        BindingPathPlan::Compile(
            *record.metadata,
            source->RuntimeType(),
            record.path.View());
    if (!compiled) return compiled.GetStatus();
    const DependencyProperty* targetProperty =
        record.descriptor.target->PropertyRegistry().Find(
            record.descriptor.targetProperty);
    if (targetProperty == nullptr ||
        (record.descriptor.convert == nullptr &&
        !record.metadata->Types().IsAssignableFrom(
            targetProperty->ValueType(),
            compiled.Value().ResultType()))) {
        return InvalidArgument(
            "Binding path result type does not match the target property");
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        !compiled.Value().CanWrite()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding source path is not writable");
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        targetProperty->ValueType() != compiled.Value().ResultType() &&
        record.descriptor.convertBack == nullptr) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    record.metadataSource = source;
    record.pathPlan = std::move(compiled).Value();
    Base::Result<void> subscribed =
        SubscribeMetadataSource(record);
    if (!subscribed) {
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return subscribed.GetStatus();
    }
    record.sourceDirty = true;
    record.applied = false;
    return {};
}

Base::Result<PropertyValue> BindingManager::ReadSource(
    BindingRecord& record) noexcept {
    if (record.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        return record.descriptor.source->GetValue(
            record.descriptor.sourceProperty);
    }
    Base::Result<void> resolved = ResolveMetadataSource(record);
    if (!resolved) return resolved.GetStatus();
    return record.pathPlan.Get(
        *record.metadata, *record.metadataSource);
}

Base::Result<void> BindingManager::WriteSource(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    if (record.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        return record.descriptor.source->SetValue(
            record.descriptor.sourceProperty, value);
    }
    Base::Result<void> resolved = ResolveMetadataSource(record);
    if (!resolved) return resolved;
    return record.pathPlan.Set(
        *record.metadata, *record.metadataSource, value);
}

Base::Result<PropertyValue> BindingManager::ConvertForTarget(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    record.conversionFailureStage =
        BindingDiagnosticStage::Convert;
    const DependencyProperty* targetProperty =
        record.descriptor.target->PropertyRegistry().Find(
            record.descriptor.targetProperty);
    if (targetProperty == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target property was not found");
    }
    PropertyValue converted = value;
    if (converted.IsNullObject() &&
        !record.descriptor.targetNullValue.IsUnset()) {
        converted = record.descriptor.targetNullValue;
    } else if (record.descriptor.convert != nullptr) {
        Base::Result<PropertyValue> result =
            record.descriptor.convert(
                converted,
                targetProperty->ValueType(),
                record.descriptor.conversionContext);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    }
    if (converted.Type() != targetProperty->ValueType()) {
        return InvalidArgument(
            "Binding converter returned a value with the wrong target type");
    }
    if (record.descriptor.validate != nullptr) {
        record.conversionFailureStage =
            BindingDiagnosticStage::Validate;
        Base::Result<void> valid = record.descriptor.validate(
            converted, record.descriptor.conversionContext);
        if (!valid) return valid.GetStatus();
    }
    return converted;
}

Base::Result<PropertyValue> BindingManager::ConvertForSource(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    record.conversionFailureStage =
        BindingDiagnosticStage::ConvertBack;
    TypeId sourceType = InvalidTypeId;
    if (record.sourceKind == BindingSourceKind::DependencyProperty) {
        const DependencyProperty* sourceProperty =
            record.descriptor.source->PropertyRegistry().Find(
                record.descriptor.sourceProperty);
        if (sourceProperty != nullptr) {
            sourceType = sourceProperty->ValueType();
        }
    } else {
        Base::Result<void> resolved = ResolveMetadataSource(record);
        if (!resolved) return resolved.GetStatus();
        sourceType = record.pathPlan.ResultType();
    }
    if (sourceType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding source type was not found");
    }
    PropertyValue converted = value;
    if (record.descriptor.convertBack != nullptr) {
        Base::Result<PropertyValue> result =
            record.descriptor.convertBack(
                converted,
                sourceType,
                record.descriptor.conversionContext);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    }
    if (converted.Type() != sourceType) {
        return InvalidArgument(
            "Binding ConvertBack returned a value with the wrong source type");
    }
    if (record.descriptor.validateBack != nullptr) {
        record.conversionFailureStage =
            BindingDiagnosticStage::ValidateBack;
        Base::Result<void> valid = record.descriptor.validateBack(
            converted, record.descriptor.conversionContext);
        if (!valid) return valid.GetStatus();
    }
    return converted;
}

void BindingManager::ReportDiagnostic(
    BindingRecord& record,
    BindingDiagnosticStage stage,
    Base::Status status) noexcept {
    lastError_ = status;
    if (record.descriptor.diagnostic != nullptr) {
        record.descriptor.diagnostic(
            {record.handle, stage, status},
            record.descriptor.diagnosticContext);
    }
}

Base::Result<void> BindingManager::SubscribeMetadataSource(
    BindingRecord& record) noexcept {
    if (record.metadata == nullptr ||
        record.metadataSource == nullptr) {
        return InvalidArgument(
            "Binding metadata source subscription is incomplete");
    }
    Base::Result<std::uint64_t> subscribed =
        record.metadata->SubscribePropertyChanged(
        *record.metadataSource,
        &BindingManager::MetadataPropertyChanged,
        this);
    if (!subscribed) return subscribed.GetStatus();
    record.notificationSubscription = subscribed.Value();
    return {};
}

void BindingManager::ReleaseMetadataSource(
    BindingRecord& record) noexcept {
    if (record.notificationSubscription != 0U &&
        record.metadata != nullptr &&
        record.metadataSource != nullptr) {
        (void)record.metadata->UnsubscribePropertyChanged(
            *record.metadataSource,
            record.notificationSubscription);
    }
    record.notificationSubscription = 0U;
}

void BindingManager::RemoveAt(std::uint32_t index) noexcept {
    BindingRecord& removed = bindings_[index];
    if (removed.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        (void)removed.descriptor.source->RemoveValueChangedHandler(
            removed.descriptor.sourceProperty,
            propertyChangedHandler_);
    } else {
        ReleaseMetadataSource(removed);
        if (removed.sourceKind ==
            BindingSourceKind::DataContext) {
            (void)removed.descriptor.target->RemoveValueChangedHandler(
                removed.dataContextProperty,
                propertyChangedHandler_);
        }
    }
    (void)removed.descriptor.target->RemoveValueChangedHandler(
        removed.descriptor.targetProperty, propertyChangedHandler_);
    for (std::uint32_t current = index + 1U;
         current < bindings_.Size();
         ++current) {
        bindings_[current - 1U] = std::move(bindings_[current]);
    }
    bindings_.PopBack();
}

} // namespace Aero::Presentation
