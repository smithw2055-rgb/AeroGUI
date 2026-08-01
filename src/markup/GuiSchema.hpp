#pragma once

#include "runtime/modules/ModuleSet.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Core {
class MetaRegistry;
}

namespace Aero::Markup {
class Schema;
}

namespace Aero {

struct GuiSchemaOptions final {
    Base::IAllocator* allocator = nullptr;
};

class GuiSchema final {
public:
    explicit GuiSchema(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~GuiSchema() noexcept;

    GuiSchema(const GuiSchema&) = delete;
    GuiSchema& operator=(const GuiSchema&) = delete;

    Base::Result<void> Prepare(
        const ModuleSet& modules) noexcept;
    Base::Result<void> Finalize(
        const GuiSchemaOptions& inputs) noexcept;
    bool IsPrepared() const noexcept;
    bool IsFrozen() const noexcept;
    Core::MetaRegistry& Metadata() noexcept;
    const Core::MetaRegistry& Metadata() const noexcept;
    Markup::Schema& Schema() noexcept;
    const Markup::Schema& Schema() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
