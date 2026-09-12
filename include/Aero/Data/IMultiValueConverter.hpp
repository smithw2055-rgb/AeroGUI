#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

namespace Aero::Data {

class AERO_GUI_API IMultiValueConverter : public Base::Object {
    AERO_DECLARE_TYPE(IMultiValueConverter, Base::Object)
public:
    ~IMultiValueConverter() override = default;

    virtual Result<Value> Convert(
        Span<const Value> values,
        Meta::TypeId targetType,
        const Value& parameter) noexcept = 0;

protected:
    IMultiValueConverter() noexcept = default;
};
} // namespace Aero::Data
