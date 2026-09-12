#pragma once

#include <Aero/RoutedCommand.hpp>

namespace Aero::Input {

class AERO_GUI_API InputBinding : public Base::Object {
    AERO_DECLARE_TYPE(InputBinding, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Ref<RoutedCommand> GetCommand() const noexcept { return command_; }
    void SetCommand(Ref<RoutedCommand> value) noexcept {
        command_ = std::move(value);
    }
    virtual Result<void> Finalize() noexcept { return {}; }

protected:
    explicit InputBinding(Meta::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

    Ref<RoutedCommand> command_;

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
};

} // namespace Aero::Input
