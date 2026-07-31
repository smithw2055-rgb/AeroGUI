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

namespace Aero::Data {

using namespace Aero::Core;

enum class BindingMode : std::uint8_t {
    OneTime = 0U,
    OneWay,
    TwoWay,
    OneWayToSource
};

enum class RelativeSourceMode : std::uint8_t {
    None = 0U,
    Self,
    TemplatedParent,
    Ancestor
};

class AERO_API Binding final
    : public Base::Object {
    AERO_DECLARE_TYPE(Binding, Base::Object)
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
    RelativeSourceMode RelativeSource() const noexcept {
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
        RelativeSourceMode relativeSource =
            RelativeSourceMode::None,
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
    RelativeSourceMode relativeSource_ =
        RelativeSourceMode::None;
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

} // namespace Aero::Data
