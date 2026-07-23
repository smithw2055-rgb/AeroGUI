#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/MetadataRuntime.hpp>

namespace Aero::Core {

// Compatibility member facade backed exclusively by the sealed metadata
// runtime. Registration records are never inspected by this class.
class AERO_API MemberAccessor final {
public:
    explicit MemberAccessor(MetadataRuntime& runtime) noexcept
        : runtime_(&runtime) {}

    Base::Result<void> Freeze() noexcept;

    Base::Result<Value> GetProperty(
        const Base::Object& object,
        const PropertyInfo& property) const noexcept;
    Base::Result<void> SetProperty(
        Base::Object& object,
        const PropertyInfo& property,
        const Value& value) const noexcept;
    Base::Result<Value> InvokeMethod(
        Base::Object& object,
        const MethodInfo& method,
        Base::Span<const Value> arguments) const noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    bool UsesRuntime() const noexcept { return runtime_ != nullptr; }
    TypeRegistry& Types() const noexcept {
        return runtime_->Domain().Types();
    }
    MetadataRuntime& Runtime() const noexcept { return *runtime_; }

private:
    MetadataRuntime* runtime_ = nullptr;
    bool frozen_ = false;
};

} // namespace Aero::Core
