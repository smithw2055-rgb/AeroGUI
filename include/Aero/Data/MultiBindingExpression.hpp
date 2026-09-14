#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Data/BindingExpression.hpp>

namespace Aero::Data {

// MultiBinding is a handle group: child records write MultiBindingProxy, then
// a DP-to-DP record writes the target. Not a single BindingRecord.
class AERO_GUI_API MultiBindingExpression {
public:
    MultiBindingExpression() noexcept = default;

    bool IsValid() const noexcept;
    BindingStatus Status() const noexcept;
    Base::Status UpdateSource() noexcept;
    Base::Status UpdateTarget() noexcept;
    std::uint32_t HandleCount() const noexcept { return handles_.Size(); }

private:
    friend class BindingOperations;
    friend class ::Aero::BindingEngine;
    Base::Vector<BindingHandle> handles_;
};

} // namespace Aero::Data
