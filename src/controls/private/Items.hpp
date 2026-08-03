#pragma once

#include <Aero/Controls/Items.hpp>

namespace Aero::Controls::Detail {

// Internal adapter for scalar ItemsSource values. It is deliberately kept out
// of the public controls surface; callers use AddBoxedItem helpers instead.
class BoxedItemValue : public Base::Object {
    AERO_DECLARE_TYPE(BoxedItemValue, Base::Object)
public:
    explicit BoxedItemValue(Meta::Value value) noexcept
        : value_(std::move(value)) {}

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    const Meta::Value& Value() const noexcept {
        return value_;
    }

private:
    Meta::Value value_;
};

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail {
using ItemContainerGeneratorImpl =
    ::Aero::Controls::ItemContainerGenerator::Impl;
}

namespace Aero::Controls::Detail {
using ::Aero::Controls::Detail::BoxedItemValue;
}
