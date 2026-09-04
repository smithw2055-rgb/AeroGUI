#pragma once

// Shared Binding evaluation helpers used by Binding.cpp / BindingEvaluation.cpp /
// BindingOperations.cpp (formerly anonymous helpers in BindingEvaluation.inl).

#include "gui/data/BindingEngine.hpp"
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/controls/State.hpp"

#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Data {

using Aero::Meta::TypeRegistry;
using Aero::Meta::TypeId;
using Aero::Meta::Registry;
using Aero::Meta::BindingPathCompileError;
using PropertyValue = ::Aero::PropertyValue;
using DependencyProperty = ::Aero::Meta::DependencyProperty;

Base::Status InvalidState(const char* message) noexcept;

Base::Status InvalidArgument(const char* message) noexcept;

Base::Status BindingTypeMismatch(
    const TypeRegistry& types,
    Base::StringView path,
    TypeId sourceType,
    const ::Aero::DependencyObject& target,
    const DependencyProperty* targetProperty) noexcept;

Base::Status BindingPathFailure(
    const TypeRegistry& types,
    Base::StringView path,
    const BindingPathCompileError& error,
    Base::Status status) noexcept;

bool IsNumericType(TypeId type) noexcept;

Base::Result<PropertyValue> ConvertNumericValue(
    const PropertyValue& value,
    TypeId targetType) noexcept;

bool HasDefaultTargetConversion(
    TypeId sourceType,
    TypeId targetType) noexcept;

bool TargetAcceptsPathResult(
    const TypeRegistry& types,
    const DependencyProperty* targetProperty,
    TypeId resultType,
    bool hasDynamicResult) noexcept;

::Aero::DependencyObject* BindingParent(
    ::Aero::DependencyObject& node) noexcept;

bool BindingOwnerSeesDataContextChange(
    ::Aero::DependencyObject* owner,
    ::Aero::DependencyObject& changed) noexcept;

Base::Result<PropertyValue> ReadDataContextValue(
    ::Aero::DependencyObject& node,
    Meta::DependencyPropertyHandle handle) noexcept;

Base::Result<PropertyValue> ConvertNullableBooleanValue(
    const PropertyValue& value,
    TypeId targetType) noexcept;

Base::Result<PropertyValue> ConvertThicknessValue(
    const PropertyValue& value) noexcept;

Base::Result<PropertyValue> ConvertLengthValue(
    const PropertyValue& value,
    TypeId targetType) noexcept;

Base::Result<PropertyValue> ConvertColorToBrush(
    const PropertyValue& value) noexcept;

bool CanRoundTripObjectValue(
    const TypeRegistry& types,
    TypeId sourceType,
    TypeId targetType) noexcept;

bool TryParseFixedPrecision(
    Base::StringView specifier,
    std::uint32_t& precision) noexcept;

bool IsZeroPaddingFormat(
    Base::StringView specifier) noexcept;

Base::Result<Base::String> FormatBindingString(
    const PropertyValue& value,
    Base::StringView format,
    const Registry* metadata = nullptr) noexcept;

Base::Result<PropertyValue> UnboxItemsValue(
    Base::Result<PropertyValue> value) noexcept;

} // namespace Aero::Data
