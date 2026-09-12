#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstdint>

namespace Aero {
class BindingEngine;
class DependencyObject;
}

namespace Aero::Data {

struct BindingHandle {
    std::uint64_t value = 0U;

    constexpr BindingHandle() noexcept = default;
    explicit constexpr BindingHandle(std::uint64_t token) noexcept
        : value(token) {}

    constexpr bool IsValid() const noexcept {
        return value != 0U && engine_ != nullptr;
    }

private:
    friend class ::Aero::BindingEngine;
    friend class BindingExpression;
    friend class MultiBindingExpression;
    void* engine_ = nullptr;
};

enum class BindingStatus : std::uint8_t {
    Unattached = 0U,
    Inactive,
    Active,
    UpdateTargetError,
    UpdateSourceError
};

// Weak facade over a BindingEngine record. Holds only BindingHandle; never a
// record pointer (records live in a moving Vector).
class AERO_GUI_API BindingExpression {
public:
    BindingExpression() noexcept = default;
    explicit BindingExpression(BindingHandle handle) noexcept
        : handle_(handle) {}

    bool IsValid() const noexcept;
    BindingHandle Handle() const noexcept { return handle_; }
    BindingStatus Status() const noexcept;
    Base::Status UpdateSource() noexcept;
    Base::Status UpdateTarget() noexcept;

private:
    friend class MultiBindingExpression;
    static BindingEngine* EngineOf(const BindingHandle& handle) noexcept;
    BindingHandle handle_{};
};

} // namespace Aero::Data
