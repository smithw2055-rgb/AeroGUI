#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>
#include <utility>

namespace Aero::Data {

class AERO_GUI_API BindingBase : public Base::Object {
    AERO_DECLARE_TYPE(BindingBase, Base::Object)
public:
    ~BindingBase() override = default;

    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    StringView GetStringFormat() const noexcept { return stringFormat_.View(); }
    void SetStringFormat(StringView value) noexcept { (void)stringFormat_.Assign(value); }
    const Value& GetFallbackValue() const noexcept { return fallbackValue_; }
    void SetFallbackValue(Value value) noexcept { fallbackValue_ = std::move(value); }
    const Value& GetTargetNullValue() const noexcept { return targetNullValue_; }
    void SetTargetNullValue(Value value) noexcept { targetNullValue_ = std::move(value); }

protected:
    explicit BindingBase(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
    String stringFormat_;
    Value fallbackValue_;
    Value targetNullValue_;
};
} // namespace Aero::Data
