#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/BindingPath.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Dispatcher.hpp>

#include <cstdint>

namespace Aero::Presentation {

using namespace Aero::Core;

enum class BindingMode : std::uint8_t {
    OneTime = 0U,
    OneWay,
    TwoWay,
    OneWayToSource
};

enum class BindingRelativeSource : std::uint8_t {
    None = 0U,
    Self,
    TemplatedParent,
    Ancestor
};

class AERO_API BindingSpec final
    : public Base::Object {
    AERO_DECLARE_TYPE(BindingSpec, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView Path() const noexcept {
        return path_.View();
    }
    Base::StringView ElementName() const noexcept {
        return elementName_.View();
    }
    Base::StringView StringFormat() const noexcept {
        return stringFormat_.View();
    }
    BindingRelativeSource RelativeSource() const noexcept {
        return relativeSource_;
    }
    Base::StringView AncestorType() const noexcept {
        return ancestorType_.View();
    }
    BindingMode Mode() const noexcept {
        return mode_;
    }
    UpdateSourceTrigger UpdateTrigger() const noexcept {
        return updateSourceTrigger_;
    }
    Base::Result<void> Configure(
        Base::StringView path,
        Base::StringView elementName,
        BindingMode mode,
        UpdateSourceTrigger updateSourceTrigger,
        Base::StringView stringFormat = {},
        BindingRelativeSource relativeSource =
            BindingRelativeSource::None,
        Base::StringView ancestorType = {}) noexcept {
        Base::Result<void> assigned =
            path_.TryAssign(path);
        if (!assigned) return assigned.GetStatus();
        assigned = elementName_.TryAssign(
            elementName);
        if (!assigned) return assigned.GetStatus();
        assigned = stringFormat_.TryAssign(
            stringFormat);
        if (!assigned) return assigned.GetStatus();
        assigned = ancestorType_.TryAssign(ancestorType);
        if (!assigned) return assigned.GetStatus();
        mode_ = mode;
        relativeSource_ = relativeSource;
        updateSourceTrigger_ =
            updateSourceTrigger;
        return {};
    }

private:
    Base::String path_;
    Base::String elementName_;
    Base::String stringFormat_;
    Base::String ancestorType_;
    BindingMode mode_ = BindingMode::OneWay;
    BindingRelativeSource relativeSource_ =
        BindingRelativeSource::None;
    UpdateSourceTrigger updateSourceTrigger_ =
        UpdateSourceTrigger::PropertyChanged;
};

struct BindingHandle final {
    std::uint64_t value = 0U;

    constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

enum class BindingDiagnosticStage : std::uint8_t {
    ResolveSource = 0U,
    ReadSource,
    Convert,
    Validate,
    WriteTarget,
    ConvertBack,
    ValidateBack,
    WriteSource
};

struct BindingDiagnostic final {
    BindingHandle handle;
    BindingDiagnosticStage stage =
        BindingDiagnosticStage::ResolveSource;
    Base::Status status;
};

using BindingConvertCallback = Base::Result<PropertyValue> (*)(
    const PropertyValue& value,
    TypeId targetType,
    void* context) noexcept;
using BindingValidateCallback = Base::Result<void> (*)(
    const PropertyValue& value,
    void* context) noexcept;
using BindingDiagnosticCallback = void (*)(
    const BindingDiagnostic& diagnostic,
    void* context) noexcept;

struct BindingDescriptor final {
    DependencyObject* source = nullptr;
    DependencyPropertyHandle sourceProperty;
    DependencyObject* target = nullptr;
    DependencyPropertyHandle targetProperty;
    BindingMode mode = BindingMode::OneWay;
    UpdateSourceTrigger updateSourceTrigger = UpdateSourceTrigger::PropertyChanged;
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

struct BindingInspection final {
    BindingHandle handle;
    Base::Object* source = nullptr;
    DependencyObject* target = nullptr;
    DependencyPropertyHandle sourceProperty;
    DependencyPropertyHandle targetProperty;
    BindingMode mode = BindingMode::OneWay;
    UpdateSourceTrigger updateSourceTrigger =
        UpdateSourceTrigger::PropertyChanged;
    Base::String path;
    bool usesDataContext = false;
    bool applied = false;
};

// Metadata-path binding source. A non-null source selects an explicit source
// (including ElementName); otherwise the source is resolved from target's
// DataContext property. Text is compiled once per concrete source type and the
// resulting immutable BindingPathPlan is reused until the source changes.
struct MetadataBindingDescriptor final {
    MetadataRuntime* metadata = nullptr;
    Base::Object* source = nullptr;
    DependencyObject* target = nullptr;
    DependencyPropertyHandle targetProperty;
    DependencyPropertyHandle dataContextProperty;
    Base::StringView path;
    Base::StringView stringFormat;
    // WPF permits an ElementName/Source binding with no Path; in that form the
    // source object itself is assigned to the target property.
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

// A host-owned BindingExpression scheduler. Direct dependency-property and
// immutable metadata-path sources converge in DispatcherFramePhase::DataBind.
// DataContext changes re-resolve the source, notifications mark expressions
// dirty, and TwoWay conflicts in one phase are resolved source-to-target.
// Binding records are non-owning: a host must Detach() the binding or call
// Shutdown() before a non-DataContext source or target object is destroyed.
class AERO_API BindingManager final {
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
    // Dispatcher; RuntimePresentationServices activates it while walking the
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

} // namespace Aero::Presentation
