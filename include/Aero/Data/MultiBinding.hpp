#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>

namespace Aero::Data {

class AERO_GUI_API MultiBinding final : public BindingBase {
    AERO_DECLARE_TYPE(MultiBinding, BindingBase)
public:
    MultiBinding() noexcept
        : BindingBase(StaticTypeId()),
          bindings_(&Base::GetDefaultAllocator()) {}

    Ref<IMultiValueConverter> GetConverter() const noexcept {
        return converter_;
    }
    void SetConverter(Ref<IMultiValueConverter> value) noexcept {
        converter_ = std::move(value);
    }
    const Value& GetConverterParameter() const noexcept {
        return converterParameter_;
    }
    void SetConverterParameter(Value value) noexcept {
        converterParameter_ = std::move(value);
    }
    Span<const Ref<Binding>> GetBindings() const noexcept {
        return bindings_.AsSpan();
    }
    Result<void> AddBinding(Ref<Binding> value) noexcept {
        return value
            ? bindings_.PushBack(std::move(value))
            : Result<void>(Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "MultiBinding child Binding is null"));
    }
    void ClearBindings() noexcept { bindings_.Clear(); }

private:
    Base::Vector<Ref<Binding>> bindings_;
    Ref<IMultiValueConverter> converter_;
    Value converterParameter_;
};
} // namespace Aero::Data
