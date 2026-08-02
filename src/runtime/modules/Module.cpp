#include "ModuleSet.hpp"

#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include "BuiltinModules.hpp"
#include "gui/MetadataInternal.hpp"

#include <new>
#include <utility>

namespace Aero {

struct ModuleSet::Impl {
    struct Module {
        struct Dependency {
            Base::String name;
            std::uint32_t minimumSchemaVersion = 1U;
        };

        Base::String name;
        std::uint32_t schemaVersion = 1U;
        ModuleRegisterCallback registerModule = nullptr;
        ModuleRegisterContextCallback registerModuleWithContext = nullptr;
        void* context = nullptr;
        std::uint32_t abiVersion = ModuleAbiVersion;
        Base::Vector<Dependency> dependencies;
    };

    Base::Vector<Module> modules;
    bool frozen = false;

    Base::Result<void> ResolveOrder(
        Base::Vector<std::uint32_t>& order) const noexcept;
};

namespace {

Base::Status OutOfMemory() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory,
        "Aero module catalog allocation failed");
}

} // namespace

Base::Result<void> ModuleSet::Impl::ResolveOrder(
    Base::Vector<std::uint32_t>& order) const noexcept {
    order.Clear();
    Base::Result<void> reserved =
        order.Reserve(modules.Size());
    if (!reserved) return reserved.GetStatus();
    Base::Vector<std::uint32_t> indegrees;
    Base::Result<void> sized =
        indegrees.Resize(modules.Size(), 0U);
    if (!sized) return sized.GetStatus();
    Base::Vector<bool> emitted;
    sized = emitted.Resize(modules.Size(), false);
    if (!sized) return sized.GetStatus();

    for (std::uint32_t index = 0U;
         index < modules.Size();
         ++index) {
        for (const Module::Dependency& dependency :
             modules[index].dependencies) {
            bool found = false;
            for (const Module& candidate : modules) {
                if (candidate.name.View() !=
                    dependency.name.View()) continue;
                found = true;
                if (candidate.schemaVersion <
                    dependency.minimumSchemaVersion) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Aero module dependency version is not satisfied");
                }
                break;
            }
            if (!found) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Aero module dependency was not found");
            }
            ++indegrees[index];
        }
    }

    while (order.Size() < modules.Size()) {
        std::uint32_t selected = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < modules.Size();
             ++index) {
            if (!emitted[index] && indegrees[index] == 0U) {
                selected = index;
                break;
            }
        }
        if (selected == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Aero module dependency graph contains a cycle");
        }
        emitted[selected] = true;
        Base::Result<void> appended =
            order.PushBack(selected);
        if (!appended) return appended.GetStatus();
        const Base::StringView emittedName =
            modules[selected].name.View();
        for (std::uint32_t index = 0U;
             index < modules.Size();
             ++index) {
            if (emitted[index] || indegrees[index] == 0U) continue;
            for (const Module::Dependency& dependency :
                 modules[index].dependencies) {
                if (dependency.name.View() == emittedName) {
                    --indegrees[index];
                    break;
                }
            }
        }
    }
    return {};
}

ModuleSet::ModuleSet() noexcept
    : impl_(new (std::nothrow) Impl()) {}

ModuleSet::~ModuleSet() noexcept {
    delete impl_;
}

Base::Result<void> ModuleSet::Add(
    const ModuleRegistration& registration) noexcept {
    if (impl_ == nullptr) return OutOfMemory();
    if (impl_->frozen) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Aero module catalog is frozen");
    }
    if (registration.name.Empty() ||
        registration.schemaVersion == 0U ||
        (registration.registerModule == nullptr &&
         registration.registerModuleWithContext == nullptr) ||
        (registration.registerModule != nullptr &&
         registration.registerModuleWithContext != nullptr) ||
        registration.abiVersion != ModuleAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Aero module registration is incomplete");
    }
    for (const Impl::Module& module : impl_->modules) {
        if (module.name.View() == registration.name) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Aero module is already present in the catalog");
        }
    }

    Impl::Module module;
    Base::Result<void> named =
        module.name.Assign(registration.name);
    if (!named) return named.GetStatus();
    module.schemaVersion = registration.schemaVersion;
    module.registerModule = registration.registerModule;
    module.registerModuleWithContext =
        registration.registerModuleWithContext;
    module.context = registration.context;
    module.abiVersion = registration.abiVersion;
    Base::Result<void> reserved = module.dependencies.Reserve(
        registration.dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const ModuleDependency& dependency :
         registration.dependencies) {
        if (dependency.name.Empty() ||
            dependency.minimumSchemaVersion == 0U ||
            dependency.name == registration.name) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Aero module dependency is invalid");
        }
        for (const Impl::Module::Dependency& existing :
             module.dependencies) {
            if (existing.name.View() == dependency.name) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Aero module dependency is duplicated");
            }
        }
        Impl::Module::Dependency stored;
        Base::Result<void> dependencyName =
            stored.name.Assign(dependency.name);
        if (!dependencyName) return dependencyName.GetStatus();
        stored.minimumSchemaVersion =
            dependency.minimumSchemaVersion;
        Base::Result<void> appended =
            module.dependencies.PushBack(std::move(stored));
        if (!appended) return appended.GetStatus();
    }
    return impl_->modules.PushBack(std::move(module));
}

Base::Result<void> ModuleSet::RegisterMetadata(
    ::Aero::Meta::Registry& domain) const noexcept {
    if (impl_ == nullptr) return OutOfMemory();
    Base::Result<void> builtIns =
        RegisterBuiltInUiModules(domain);
    if (!builtIns) return builtIns.GetStatus();
    Base::Vector<std::uint32_t> order;
    Base::Result<void> resolved =
        impl_->ResolveOrder(order);
    if (!resolved) return resolved.GetStatus();
    for (std::uint32_t index : order) {
        const Impl::Module& module = impl_->modules[index];
        const Base::StringView name = module.name.View();
        Base::Result<void> registered =
            domain.RegisterModule({
                Meta::MakeMetadataModuleId(name),
                name,
                module.schemaVersion,
                module.registerModule,
                module.registerModuleWithContext,
                module.context});
        if (!registered) return registered.GetStatus();
    }
    return {};
}

Base::Result<void> ModuleSet::Freeze() noexcept {
    if (impl_ == nullptr) return OutOfMemory();
    Base::Vector<std::uint32_t> order;
    Base::Result<void> resolved =
        impl_->ResolveOrder(order);
    if (!resolved) return resolved.GetStatus();
    impl_->frozen = true;
    return {};
}

bool ModuleSet::IsFrozen() const noexcept {
    return impl_ != nullptr && impl_->frozen;
}

std::uint32_t ModuleSet::ModuleCount() const noexcept {
    return impl_ != nullptr ? impl_->modules.Size() : 0U;
}

} // namespace Aero
