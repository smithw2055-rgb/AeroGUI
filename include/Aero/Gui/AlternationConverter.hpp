#pragma once

#include <Aero/Gui/DependencyObject.hpp>

#include <utility>

namespace Aero::Controls {

class AERO_API AlternationConverter : public Base::Object {
    AERO_DECLARE_TYPE(AlternationConverter, Base::Object)
public:
    AlternationConverter() noexcept
        : values_(&Base::GetDefaultAllocator()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<Base::Object>> GetValues() const noexcept {
        return values_.AsSpan();
    }
    Base::Result<void> AddValue(Base::Ref<Base::Object> value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AlternationConverter values cannot be null");
        }
        return values_.PushBack(std::move(value));
    }
    void ClearValues() noexcept { values_.Clear(); }

private:
    Base::Vector<Base::Ref<Base::Object>> values_;
};

} // namespace Aero::Controls
