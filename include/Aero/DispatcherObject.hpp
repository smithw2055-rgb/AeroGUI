#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Threading {

class Dispatcher;

class AERO_GUI_API DispatcherObject : public Base::Object {
    AERO_DECLARE_TYPE(DispatcherObject, Base::Object)
public:
    bool CheckAccess() const noexcept;
    Result<void> VerifyAccess() const noexcept;
    Dispatcher& GetDispatcher() const noexcept;

protected:
    explicit DispatcherObject(Dispatcher& dispatcher) noexcept;
    ~DispatcherObject() override = default;

private:
    Dispatcher* dispatcher_ = nullptr;
};

} // namespace Aero::Threading

namespace Aero {
using ::Aero::Threading::DispatcherObject;
}
