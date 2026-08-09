#pragma once

#include <Aero/Data/Binding.hpp>

#include <utility>

namespace Aero::Controls {

class AERO_GUI_API AlternationConverter : public Data::IValueConverter {
    AERO_DECLARE_TYPE(AlternationConverter, Data::IValueConverter)
public:
    AlternationConverter() noexcept
        : values_(&Base::GetDefaultAllocator()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Span<const Ref<Base::Object>> GetValues() const noexcept {
        return values_.AsSpan();
    }
    Result<void> AddValue(Ref<Base::Object> value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AlternationConverter values cannot be null");
        }
        return values_.PushBack(std::move(value));
    }
    void ClearValues() noexcept { values_.Clear(); }

    Result<Value> Convert(
        const Value& value,
        const Value& parameter) noexcept override;
    Result<Value> ConvertBack(
        const Value& value,
        const Value& parameter) noexcept override;

private:
    Base::Vector<Ref<Base::Object>> values_;
};

} // namespace Aero::Controls
