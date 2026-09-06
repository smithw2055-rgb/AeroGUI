#pragma once

// Items implementation detail — not part of the public Controls umbrella.
// BoxedItemValue adapts scalar ItemsSource values for the items pipeline.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <utility>

namespace Aero::Controls {

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

} // namespace Aero::Controls
