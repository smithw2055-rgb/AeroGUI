#pragma once

// Items implementation detail — not part of the public Controls umbrella.
// BoxedItemValue adapts scalar ItemsSource values for the items pipeline.
// NOTE: renamed from ItemsDetail.hpp (responsibility name; no behavior change).

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include "gui/core/ValueConversion.hpp"
#include <utility>

namespace Aero::Controls {

class BoxedItemValue : public Base::Object {
    AERO_DECLARE_TYPE(BoxedItemValue, Base::Object)
public:
    explicit BoxedItemValue(Meta::Value value) noexcept
        : value_(std::move(value)) {}

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    const Meta::Value& Value() const noexcept {
        return value_;
    }

private:
    Meta::Value value_;
};

inline bool FormatDisplayString(
    const Meta::Value& value,
    Meta::Registry* metadata,
    Base::String& outText) noexcept {
    switch (value.Kind()) {
    case Meta::ValueKind::String:
    case Meta::ValueKind::Boolean:
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double: {
        Base::Result<void> formatted = Base::ValueConversion::FormatScalarValue(outText, value);
        return static_cast<bool>(formatted);
    }
    case Meta::ValueKind::Object:
        if (!value.IsNullObject()) {
            auto* obj = value.AsObject().Get();
            if (obj != nullptr) {
                if (obj->RuntimeType() == BoxedItemValue::StaticTypeId()) {
                    const Meta::Value& inner = static_cast<const BoxedItemValue&>(*obj).Value();
                    return FormatDisplayString(inner, metadata, outText);
                }
                if (metadata != nullptr) {
                    static constexpr Base::StringView candidateProps[] = {
                        Base::StringView("Name"),
                        Base::StringView("Value"),
                        Base::StringView("Text"),
                        Base::StringView("Content"),
                        Base::StringView("Header")
                    };
                    for (const Base::StringView propName : candidateProps) {
                        const Meta::PropertyInfo* prop =
                            metadata->Types().FindProperty(obj->RuntimeType(), propName, true);
                        if (prop != nullptr) {
                            Base::Result<Meta::Value> displayed = metadata->GetProperty(*obj, prop->Id());
                            if (displayed) {
                                if (displayed.Value().Kind() == Meta::ValueKind::String) {
                                    (void)outText.Assign(displayed.Value().AsString());
                                    return true;
                                }
                                Base::Result<void> formatted =
                                    Base::ValueConversion::FormatScalarValue(outText, displayed.Value());
                                if (formatted) {
                                    return true;
                                }
                            }
                        }
                    }
                    const auto* typeInfo = metadata->Types().FindType(obj->RuntimeType());
                    if (typeInfo != nullptr) {
                        (void)outText.Assign(typeInfo->Name());
                        return true;
                    }
                }
            }
        }
        return false;
    default:
        return false;
    }
}

} // namespace Aero::Controls
