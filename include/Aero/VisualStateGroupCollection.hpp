#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/VisualStateGroup.hpp>

namespace Aero {

class AERO_GUI_API VisualStateGroupCollection : public Base::Object {
    AERO_DECLARE_TYPE(VisualStateGroupCollection, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Span<const Ref<VisualStateGroup>> GetItems() const noexcept {
        return {items_.Data(), items_.Size()};
    }
    void Add(Ref<VisualStateGroup> value) noexcept {
        Base::Result<void> pushed = items_.PushBack(std::move(value));
        if (!pushed) { AERO_ASSERT(false); return; }
    }
    void Clear() noexcept { items_.Clear(); }

private:
    Base::Vector<Ref<VisualStateGroup>> items_;
};

} // namespace Aero
