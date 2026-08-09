#pragma once

#include <Aero/DependencyObject.hpp>

#include <utility>

namespace Aero::Controls {

class AERO_GUI_API AlternationConverter : public Base::Object {
    AERO_DECLARE_TYPE(AlternationConverter, Base::Object)
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

private:
    Base::Vector<Ref<Base::Object>> values_;
};

} // namespace Aero::Controls
