#pragma once

#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>

namespace Aero::Markup {

inline const ::Aero::Meta::DependencyProperty* ResolveXamlDependencyProperty(
    const ::Aero::Meta::DependencyPropertyRegistry& properties,
    ::Aero::Meta::TypeId targetType,
    ::Aero::Base::StringView name) noexcept {
    const ::Aero::Meta::DependencyProperty* property = properties.Find(targetType, name);
    if (property != nullptr) return property;

    if (name == ::Aero::Base::StringView("IsFocused") &&
        properties.Types().IsDerivedFrom(targetType, ::Aero::UIElement::StaticTypeId())) {
        return properties.Find(::Aero::UIElement::IsKeyboardFocusedProperty.Handle());
    }

    if (name == ::Aero::Base::StringView("ContextMenu")) {
        property = properties.Find(
            ::Aero::Controls::ContextMenuService::ContextMenuProperty.Handle());
        if (property != nullptr) return property;
    }

    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == '.') separator = index;
    }
    if (separator == UINT32_MAX || separator == 0U || separator + 1U >= name.SizeBytes()) {
        return nullptr;
    }

    ::Aero::Base::StringView ownerName = name.Substr(0U, separator);
    for (std::uint32_t index = 0U; index < ownerName.SizeBytes(); ++index) {
        if (ownerName[index] == ':') {
            ownerName = ownerName.Substr(index + 1U, ownerName.SizeBytes() - index - 1U);
            break;
        }
    }

    const ::Aero::Meta::TypeInfo* owner = properties.Types().FindType(
        ::Aero::Meta::AeroNamespaceUri(), ownerName);
    if (owner == nullptr && ownerName == ::Aero::Base::StringView("Text")) {
        owner = properties.Types().FindType(
            ::Aero::Meta::AeroNamespaceUri(), "TextProperties");
    }
    const ::Aero::Base::StringView member = name.Substr(
        separator + 1U, name.SizeBytes() - separator - 1U);
    if (owner != nullptr) {
        property = properties.Find(owner->Id(), member);
        if (property != nullptr) return property;
    }

    return properties.Find(targetType, member);
}

inline ::Aero::Base::Result<::Aero::Meta::Value> CoerceXamlValueForProperty(
    ::Aero::Meta::TypeId targetPropertyType,
    const ::Aero::Meta::Value& value) noexcept {
    if (value.Type() == targetPropertyType) {
        return value;
    }

    if ((targetPropertyType == ::Aero::Length::StaticTypeId() ||
         targetPropertyType == ::Aero::Meta::TypeOf<::Aero::Length>() ||
         targetPropertyType == ::Aero::Meta::TypeOf<::Aero::GridLength>()) &&
        (value.Kind() == ::Aero::Meta::ValueKind::SignedInteger ||
         value.Kind() == ::Aero::Meta::ValueKind::UnsignedInteger ||
         value.Kind() == ::Aero::Meta::ValueKind::Double)) {
        double numericVal = 0.0;
        if (value.Kind() == ::Aero::Meta::ValueKind::Double) {
            auto num = ::Aero::Meta::ValueCodec<double>::Decode(value);
            if (num) numericVal = num.Value();
        } else if (value.Kind() == ::Aero::Meta::ValueKind::SignedInteger) {
            auto num = ::Aero::Meta::ValueCodec<std::int64_t>::Decode(value);
            if (num) numericVal = static_cast<double>(num.Value());
        } else if (value.Kind() == ::Aero::Meta::ValueKind::UnsignedInteger) {
            auto num = ::Aero::Meta::ValueCodec<std::uint64_t>::Decode(value);
            if (num) numericVal = static_cast<double>(num.Value());
        }
        if (targetPropertyType == ::Aero::Meta::TypeOf<::Aero::GridLength>()) {
            return ::Aero::Meta::ValueCodec<::Aero::GridLength>::Encode(::Aero::GridLength::Pixel(numericVal));
        }
        return ::Aero::Meta::ValueCodec<::Aero::Length>::Encode(::Aero::Length::Pixels(numericVal));
    }

    if (targetPropertyType == ::Aero::Media::Brush::StaticTypeId() &&
        value.Type() == ::Aero::Meta::TypeOf<::Aero::Base::Color>()) {
        auto color = ::Aero::Meta::ValueCodec<::Aero::Base::Color>::Decode(value);
        if (!color) return color.GetStatus();
        auto brush = ::Aero::Media::MakeSolidColorBrush(color.Value());
        if (!brush) {
            return ::Aero::Base::Status::Failure(::Aero::Base::ErrorCode::OutOfMemory, "Failed to create brush");
        }
        return ::Aero::Meta::Value::FromObject(
            ::Aero::Media::Brush::StaticTypeId(),
            ::Aero::Base::Ref<::Aero::Base::Object>(std::move(brush)));
    }

    if (targetPropertyType == ::Aero::Meta::TypeOf<::Aero::Nullable<bool>>()) {
        if (value.IsNullObject()) {
            return ::Aero::Meta::ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{});
        }
        if (value.Type() == ::Aero::Meta::TypeOf<bool>() ||
            value.Kind() == ::Aero::Meta::ValueKind::Boolean) {
            auto boolVal = ::Aero::Meta::ValueCodec<bool>::Decode(value);
            if (!boolVal) return boolVal.GetStatus();
            return ::Aero::Meta::ValueCodec<::Aero::Nullable<bool>>::Encode(
                ::Aero::Nullable<bool>(boolVal.Value()));
        }
    }

    if (value.IsNullObject() && value.Type() != targetPropertyType) {
        return ::Aero::Meta::Value::NullObject(targetPropertyType);
    }

    return value;
}

inline ::Aero::Base::Result<::Aero::Meta::Value> CoerceXamlValueForProperty(
    const ::Aero::Meta::DependencyProperty& property,
    const ::Aero::Meta::Value& value,
    const ::Aero::Meta::TypeRegistry& types) noexcept {
    if (property.ValueType() == ::Aero::Meta::TypeOf<::Aero::Base::Color>() &&
        value.Kind() == ::Aero::Meta::ValueKind::Object &&
        types.IsDerivedFrom(value.Type(), ::Aero::Media::Brush::StaticTypeId())) {
        ::Aero::Base::Result<::Aero::Base::Ref<::Aero::Media::Brush>> brush =
            ::Aero::Meta::ValueCodec<::Aero::Base::Ref<::Aero::Media::Brush>>::Decode(value);
        if (!brush) return brush.GetStatus();
        if (brush.Value() == nullptr ||
            brush->RuntimeType() != ::Aero::Media::SolidColorBrush::StaticTypeId()) {
            return ::Aero::Base::Status::Failure(
                ::Aero::Base::ErrorCode::InvalidArgument,
                "Color-backed text property requires SolidColorBrush");
        }
        return ::Aero::Meta::ValueCodec<::Aero::Base::Color>::Encode(
            static_cast<const ::Aero::Media::SolidColorBrush&>(*brush.Value()).GetColor());
    }

    return CoerceXamlValueForProperty(property.ValueType(), value);
}

} // namespace Aero::Markup
