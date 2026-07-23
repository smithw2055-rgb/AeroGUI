#include <Aero/Core/Binding.hpp>

#include <utility>

namespace Aero::Core {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

} // namespace

BindingManager::BindingManager(
    Dispatcher& dispatcher,
    Base::IAllocator* allocator) noexcept
    : dispatcher_(&dispatcher),
      allocator_(allocator != nullptr ? allocator : &dispatcher.Allocator()),
      bindings_(allocator_),
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
    const std::uint32_t snapshotCount = bindings_.Size();
    std::uint32_t updated = 0U;
    for (std::uint32_t index = 0U; index < snapshotCount; ++index) {
        BindingRecord& record = bindings_[index];
        if (record.descriptor.mode == BindingMode::OneTime && record.applied) {
            continue;
        }
        if (record.applied && !record.sourceDirty && !record.targetDirty) {
            continue;
        }

        Base::Result<PropertyValue> source =
            record.descriptor.source->GetValue(record.descriptor.sourceProperty);
        if (!source) {
            lastError_ = source.GetStatus();
            flushing_ = false;
            return source.GetStatus();
        }
        Base::Result<PropertyValue> target =
            record.descriptor.target->GetValue(record.descriptor.targetProperty);
        if (!target) {
            lastError_ = target.GetStatus();
            flushing_ = false;
            return target.GetStatus();
        }

        const bool sourceChanged = !record.applied || record.sourceDirty;
        const bool targetChanged = record.descriptor.updateSourceTrigger ==
                UpdateSourceTrigger::Explicit
            ? record.forceSourceUpdate
            : (!record.applied || record.targetDirty);
        Base::Result<void> applied;
        switch (record.descriptor.mode) {
        case BindingMode::OneTime:
            if (!record.applied) {
                applied = record.descriptor.target->SetCurrentValue(
                    record.descriptor.targetProperty, source.Value());
                if (!applied) {
                    lastError_ = applied.GetStatus();
                    flushing_ = false;
                    return applied.GetStatus();
                }
                target = source.Value();
                ++updated;
            }
            break;
        case BindingMode::OneWay:
            if (sourceChanged) {
                applied = record.descriptor.target->SetCurrentValue(
                    record.descriptor.targetProperty, source.Value());
                if (!applied) {
                    lastError_ = applied.GetStatus();
                    flushing_ = false;
                    return applied.GetStatus();
                }
                target = source.Value();
                ++updated;
            }
            break;
        case BindingMode::OneWayToSource:
            if (targetChanged) {
                applied = record.descriptor.source->SetValue(
                    record.descriptor.sourceProperty, target.Value());
                if (!applied) {
                    lastError_ = applied.GetStatus();
                    flushing_ = false;
                    return applied.GetStatus();
                }
                source = target.Value();
                ++updated;
            }
            break;
        case BindingMode::TwoWay:
            if (sourceChanged) {
                applied = record.descriptor.target->SetCurrentValue(
                    record.descriptor.targetProperty, source.Value());
                if (!applied) {
                    lastError_ = applied.GetStatus();
                    flushing_ = false;
                    return applied.GetStatus();
                }
                target = source.Value();
                ++updated;
            } else if (targetChanged) {
                applied = record.descriptor.source->SetValue(
                    record.descriptor.sourceProperty, target.Value());
                if (!applied) {
                    lastError_ = applied.GetStatus();
                    flushing_ = false;
                    return applied.GetStatus();
                }
                source = target.Value();
                ++updated;
            }
            break;
        }
        record.lastSourceValue = source.Value();
        record.lastTargetValue = target.Value();
        record.applied = true;
        record.sourceDirty = false;
        record.targetDirty = false;
        record.forceSourceUpdate = false;
    }
    flushing_ = false;
    lastError_ = {};
    return updated;
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
        if (record.descriptor.source == &object &&
            record.descriptor.sourceProperty == args.property) {
            record.sourceDirty = true;
        }
        if (record.descriptor.target == &object &&
            record.descriptor.targetProperty == args.property) {
            record.targetDirty = true;
        }
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
    if (source.Value().Type() != target.Value().Type()) {
        return InvalidArgument("Binding source and target property types differ");
    }
    return {};
}

void BindingManager::RemoveAt(std::uint32_t index) noexcept {
    BindingRecord& removed = bindings_[index];
    (void)removed.descriptor.source->RemoveValueChangedHandler(
        removed.descriptor.sourceProperty, propertyChangedHandler_);
    (void)removed.descriptor.target->RemoveValueChangedHandler(
        removed.descriptor.targetProperty, propertyChangedHandler_);
    for (std::uint32_t current = index + 1U;
         current < bindings_.Size();
         ++current) {
        bindings_[current - 1U] = std::move(bindings_[current]);
    }
    bindings_.PopBack();
}

} // namespace Aero::Core
