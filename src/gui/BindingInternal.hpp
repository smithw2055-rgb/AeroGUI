#pragma once

// Property-path parsing and access used by binding expressions.
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Meta/Registry.hpp>

#include <cstdint>

namespace Aero::Core {

enum class BindingPathSegmentKind : std::uint8_t {
    ObjectProperty = 0U,
    ValueField
};

struct BindingPathSegment final {
    BindingPathSegmentKind kind = BindingPathSegmentKind::ObjectProperty;
    MemberId member = InvalidMemberId;
    TypeId inputType = InvalidTypeId;
    TypeId outputType = InvalidTypeId;
    bool readable = false;
    bool writable = false;
    bool copyOnWrite = false;
};

struct BindingPathCompileError final {
    std::uint32_t segmentIndex = UINT32_MAX;
    TypeId inputType = InvalidTypeId;
    Base::String segment;
    Base::Status status;
};

// Immutable, schema-bound access plan used by Binding and compiled XAML.
// Compilation resolves every textual segment to stable descriptor IDs. Get/Set
// execute those IDs directly and never repeat member-name lookup.
class AERO_API BindingPathPlan final {
public:
    BindingPathPlan() noexcept = default;

    static Base::Result<BindingPathPlan> Compile(
        Meta::Registry& runtime,
        TypeId rootType,
        Base::StringView path,
        BindingPathCompileError* error = nullptr) noexcept;

    bool IsValid() const noexcept {
        return rootType_ != InvalidTypeId &&
            resultType_ != InvalidTypeId &&
            schemaHash_ != 0U &&
            !segments_.Empty();
    }
    TypeId RootType() const noexcept { return rootType_; }
    TypeId ResultType() const noexcept { return resultType_; }
    Base::HashCode SchemaHash() const noexcept { return schemaHash_; }
    bool CanRead() const noexcept { return canRead_; }
    bool CanWrite() const noexcept { return canWrite_; }
    Base::Span<const BindingPathSegment> Segments() const noexcept {
        return {segments_.Data(), segments_.Size()};
    }

    Base::Result<Value> Get(
        Meta::Registry& runtime,
        const Base::Object& root) const noexcept;
    Base::Result<Value> Get(
        Meta::Registry& runtime,
        const Value& root) const noexcept;
    Base::Result<void> Set(
        Meta::Registry& runtime,
        Base::Object& root,
        const Value& value) const noexcept;
    Base::Result<void> Set(
        Meta::Registry& runtime,
        Value& root,
        const Value& value) const noexcept;

private:
    const Meta::Registry* compiledDomain_ = nullptr;
    TypeId rootType_ = InvalidTypeId;
    TypeId resultType_ = InvalidTypeId;
    Base::HashCode schemaHash_ = 0U;
    Base::Vector<BindingPathSegment> segments_;
    bool canRead_ = false;
    bool canWrite_ = false;

    Base::Result<void> VerifyRuntime(
        Meta::Registry& runtime) const noexcept;
    Base::Result<Value> GetObject(
        Meta::Registry& runtime,
        const Base::Object& object,
        std::uint32_t segmentIndex) const noexcept;
    Base::Result<Value> GetValue(
        Meta::Registry& runtime,
        const Value& value,
        std::uint32_t segmentIndex) const noexcept;
    Base::Result<void> SetObject(
        Meta::Registry& runtime,
        Base::Object& object,
        std::uint32_t segmentIndex,
        const Value& value) const noexcept;
    Base::Result<bool> SetValue(
        Meta::Registry& runtime,
        Value& owner,
        std::uint32_t segmentIndex,
        const Value& value) const noexcept;
};

} // namespace Aero::Core

// Binding descriptors, expressions and the view binding service.

#include <Aero/Data.hpp>

namespace Aero::Data {

using ::Aero::Meta::Registry;
using Core::PropertyValue;
using Core::TypeId;

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
    Meta::Registry* metadata = nullptr;
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

} // namespace Aero::Data

#include "gui/ElementInternal.hpp"


namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Data;

class AERO_API BindingEngine final {
public:
    explicit BindingEngine(Dispatcher& dispatcher) noexcept;
    ~BindingEngine() noexcept;

    BindingEngine(const BindingEngine&) = delete;
    BindingEngine& operator=(const BindingEngine&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<BindingHandle> Attach(
        const BindingDescriptor& descriptor) noexcept;
    Base::Result<BindingHandle> Attach(
        const MetadataBindingDescriptor& descriptor) noexcept;
    // Deferred templates are cloned before their visual roots are mounted.
    // Queueing preserves the declaration until the target acquires its
    // Dispatcher; ViewStyling activates it while walking the
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
        Meta::Registry* metadata = nullptr;
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
        Meta::Registry* metadata = nullptr;
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
