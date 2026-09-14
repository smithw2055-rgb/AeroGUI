#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

namespace Aero::Data {

class AERO_GUI_API IValueConverter : public Base::Object {
    AERO_DECLARE_TYPE(IValueConverter, Base::Object)
public:
    ~IValueConverter() override = default;

    virtual Result<Value> Convert(const Value& value, const Value& parameter) noexcept = 0;
    virtual Result<Value> ConvertBack(const Value& value, const Value& parameter) noexcept = 0;

protected:
    IValueConverter() noexcept = default;
};
} // namespace Aero::Data
