#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero {

using Meta::TypeId;

class AERO_GUI_API SetterBase : public Base::Object {
    AERO_DECLARE_TYPE(SetterBase, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }

protected:
    explicit SetterBase(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    ~SetterBase() override = default;

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
};

} // namespace Aero
