#pragma once

#include "runtime/RuntimeFwd.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/tree/ObjectTree.hpp"

#include <Aero/Data.hpp>

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Data;

class AERO_API UiRuntimeAccess::BindingManager final {
public:
    explicit BindingManager(Dispatcher& dispatcher) noexcept;
    ~BindingManager() noexcept;

    BindingManager(const BindingManager&) = delete;
    BindingManager& operator=(const BindingManager&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<BindingHandle> Attach(
        const BindingDescriptor& descriptor) noexcept;
    Base::Result<BindingHandle> Attach(
        const MetadataBindingDescriptor& descriptor) noexcept;
    // Deferred templates are cloned before their visual roots are mounted.
    // Queueing preserves the declaration until the target acquires its
    // Dispatcher; RuntimeUiServices activates it while walking the
    // newly mounted instance.
    Base::Result<void> QueueDeferred(
        const MetadataBindingDescriptor& descriptor) noexcept;
    Base::Result<std::uint32_t> ActivateDeferred(
        DependencyObject& target) noexcept;
    template<
        class TSourceOwner,
        class TValue,
        class TTargetOwner>
    Base::Result<BindingHandle> Attach(
        DependencyObject& source,
        const Core::DependencyPropertyRef<
            TSourceOwner, TValue>& sourceProperty,
        DependencyObject& target,
        const Core::DependencyPropertyRef<
            TTargetOwner, TValue>& targetProperty,
        BindingMode mode = BindingMode::OneWay) noexcept {
        BindingDescriptor descriptor;
        descriptor.source = &source;
        descriptor.sourceProperty =
            sourceProperty.Handle();
        descriptor.target = &target;
        descriptor.targetProperty =
            targetProperty.Handle();
        descriptor.mode = mode;
        return Attach(descriptor);
    }
    template<
        class TSourceOwner,
        class TValue,
        class TTargetOwner>
    Base::Result<BindingHandle> Attach(
        DependencyObject& source,
        const Core::ReadOnlyPropertyRef<
            TSourceOwner, TValue>& sourceProperty,
        DependencyObject& target,
        const Core::DependencyPropertyRef<
            TTargetOwner, TValue>& targetProperty,
        BindingMode mode = BindingMode::OneWay) noexcept {
        BindingDescriptor descriptor;
        descriptor.source = &source;
        descriptor.sourceProperty =
            sourceProperty.Handle();
        descriptor.target = &target;
        descriptor.targetProperty =
            targetProperty.Handle();
        descriptor.mode = mode;
        return Attach(descriptor);
    }
    Base::Result<bool> Detach(BindingHandle handle) noexcept;
    Base::Result<bool> UpdateSource(BindingHandle handle) noexcept;

    // Removes every binding whose source or target is object. Tree/object
    // ownership code uses this before destroying a DependencyObject.
    Base::Result<std::uint32_t> DetachObject(
        DependencyObject& object) noexcept;

    // Flush is also exposed for deterministic headless tests. Normal hosts run
    // it through the DataBind frame phase registered by Initialize().
    Base::Result<std::uint32_t> Flush() noexcept;
    Base::Result<std::uint32_t>
    InspectBindings(
        const DependencyObject& object,
        Base::Vector<BindingInspection>&
            output) const noexcept;

    bool IsInitialized() const noexcept {
        return hook_.IsValid();
    }
    bool IsFlushing() const noexcept {
        return flushing_;
    }
    std::uint32_t BindingCount() const noexcept {
        return bindings_.Size();
    }
    Base::Status LastError() const noexcept {
        return lastError_;
    }

private:
    enum class BindingSourceKind : std::uint8_t {
        DependencyProperty = 0U,
        MetadataPath,
        MetadataObject,
        DataContext
    };

    struct BindingRecord final {
        BindingHandle handle;
        BindingDescriptor descriptor;
        BindingSourceKind sourceKind =
            BindingSourceKind::DependencyProperty;
        MetadataRuntime* metadata = nullptr;
        Base::Object* metadataSource = nullptr;
        DependencyPropertyHandle dataContextProperty;
        Base::String path;
        Base::String stringFormat;
        bool bindsToSource = false;
        BindingPathPlan pathPlan;
        std::uint64_t notificationSubscription = 0U;
        PropertyValue lastSourceValue;
        PropertyValue lastTargetValue;
        BindingDiagnosticStage conversionFailureStage =
            BindingDiagnosticStage::Convert;
        bool applied = false;
        bool sourceDirty = true;
        bool targetDirty = true;
        bool forceSourceUpdate = false;
    };

    struct DeferredBindingRecord final {
        MetadataRuntime* metadata = nullptr;
        Base::Object* source = nullptr;
        DependencyObject* target = nullptr;
        DependencyPropertyHandle targetProperty;
        DependencyPropertyHandle dataContextProperty;
        Base::String path;
        Base::String stringFormat;
        bool bindsToSource = false;
        BindingMode mode = BindingMode::OneWay;
        UpdateSourceTrigger updateSourceTrigger =
            UpdateSourceTrigger::PropertyChanged;
        BindingConvertCallback convert = nullptr;
        BindingConvertCallback convertBack = nullptr;
        BindingValidateCallback validate = nullptr;
        BindingValidateCallback validateBack = nullptr;
        void* conversionContext = nullptr;
        PropertyValue fallbackValue;
        PropertyValue targetNullValue;
        BindingDiagnosticCallback diagnostic = nullptr;
        void* diagnosticContext = nullptr;
    };

    Dispatcher* dispatcher_ = nullptr;
    Base::Vector<BindingRecord> bindings_;
    Base::Vector<DeferredBindingRecord> deferredBindings_;
    DispatcherFrameHookHandle hook_;
    std::uint64_t nextHandle_ = 1U;
    bool flushing_ = false;
    Base::Status lastError_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;

    static void DataBindHook(void* context) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnMetadataPropertyChanged(
        Base::Object& object,
        MemberId property) noexcept;
    static void MetadataPropertyChanged(
        Base::Object& object,
        MemberId property,
        void* context) noexcept;
    Base::Result<void> VerifyDescriptor(
        const BindingDescriptor& descriptor) const noexcept;
    Base::Result<void> VerifyDescriptor(
        const MetadataBindingDescriptor& descriptor) const noexcept;
    Base::Result<void> ResolveMetadataSource(
        BindingRecord& record) noexcept;
    Base::Result<PropertyValue> ReadSource(
        BindingRecord& record) noexcept;
    Base::Result<void> WriteSource(
        BindingRecord& record,
        const PropertyValue& value) noexcept;
    Base::Result<PropertyValue> ConvertForTarget(
        BindingRecord& record,
        const PropertyValue& value) noexcept;
    Base::Result<PropertyValue> ConvertForSource(
        BindingRecord& record,
        const PropertyValue& value) noexcept;
    void ReportDiagnostic(
        BindingRecord& record,
        BindingDiagnosticStage stage,
        Base::Status status) noexcept;
    Base::Result<void> SubscribeMetadataSource(
        BindingRecord& record) noexcept;
    void ReleaseMetadataSource(BindingRecord& record) noexcept;
    void RemoveAt(std::uint32_t index) noexcept;
};

} // namespace Aero::Detail
