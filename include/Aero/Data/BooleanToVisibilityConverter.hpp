#pragma once

#include <Aero/Data/IValueConverter.hpp>

namespace Aero::Data {

class AERO_GUI_API BooleanToVisibilityConverter final
    : public IValueConverter {
    AERO_DECLARE_TYPE(BooleanToVisibilityConverter, IValueConverter)
public:
    BooleanToVisibilityConverter() noexcept = default;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Result<Value> Convert(
        const Value& value,
        const Value& parameter) noexcept override;
    Result<Value> ConvertBack(
        const Value& value,
        const Value& parameter) noexcept override;
};
} // namespace Aero::Data
