#include <Aero/Module.hpp>

#include <Aero/BuiltinModules.hpp>
#include <utility>

namespace Aero {

Base::Result<void> ModuleCatalog::Add(
    const ModuleRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Aero module catalog is frozen");
    }
    if (registration.name.Empty() || registration.schemaVersion == 0U ||
        (registration.registerModule == nullptr &&
         registration.registerModuleWithContext == nullptr) ||
        (registration.registerModule != nullptr &&
         registration.registerModuleWithContext != nullptr) ||
        registration.abiVersion != ModuleAbiVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Aero module registration is incomplete");
    }
    for (const Module& module : modules_) {
        if (module.name.View() == registration.name) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Aero module is already present in the catalog");
        }
    }

    Module module;
    Base::Result<void> named =
        module.name.TryAssign(registration.name);
    if (!named) return named.GetStatus();
    module.schemaVersion = registration.schemaVersion;
    module.registerModule = registration.registerModule;
    module.registerModuleWithContext =
        registration.registerModuleWithContext;
    module.context = registration.context;
    module.abiVersion = registration.abiVersion;
    Base::Result<void> reserved = module.dependencies.TryReserve(
        registration.dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const ModuleDependency& dependency : registration.dependencies) {
        if (dependency.name.Empty() ||
            dependency.minimumSchemaVersion == 0U ||
            dependency.name == registration.name) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Aero module dependency is invalid");
        }
        for (const Module::Dependency& existing : module.dependencies) {
            if (existing.name.View() == dependency.name) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Aero module dependency is duplicated");
            }
        }
        Module::Dependency stored;
        Base::Result<void> dependencyName =
            stored.name.TryAssign(dependency.name);
        if (!dependencyName) return dependencyName.GetStatus();
        stored.minimumSchemaVersion = dependency.minimumSchemaVersion;
        Base::Result<void> appended = module.dependencies.TryPushBack(
            std::move(stored));
        if (!appended) return appended.GetStatus();
    }
    return modules_.TryPushBack(std::move(module));
}

Base::Result<void> ModuleCatalog::ResolveOrder(
    Base::Vector<std::uint32_t>& order) const noexcept {
    order.Clear();
    Base::Result<void> reserved = order.TryReserve(modules_.Size());
    if (!reserved) return reserved.GetStatus();
    Base::Vector<std::uint32_t> indegrees;
    Base::Result<void> sized = indegrees.TryResize(modules_.Size(), 0U);
    if (!sized) return sized.GetStatus();
    Base::Vector<bool> emitted;
    sized = emitted.TryResize(modules_.Size(), false);
    if (!sized) return sized.GetStatus();

    for (std::uint32_t index = 0U; index < modules_.Size(); ++index) {
        for (const Module::Dependency& dependency :
             modules_[index].dependencies) {
            bool found = false;
            for (const Module& candidate : modules_) {
                if (candidate.name.View() != dependency.name.View()) continue;
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

    while (order.Size() < modules_.Size()) {
        std::uint32_t selected = UINT32_MAX;
        for (std::uint32_t index = 0U; index < modules_.Size(); ++index) {
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
        Base::Result<void> appended = order.TryPushBack(selected);
        if (!appended) return appended.GetStatus();
        const Base::StringView emittedName = modules_[selected].name.View();
        for (std::uint32_t index = 0U; index < modules_.Size(); ++index) {
            if (emitted[index] || indegrees[index] == 0U) continue;
            for (const Module::Dependency& dependency :
                 modules_[index].dependencies) {
                if (dependency.name.View() == emittedName) {
                    --indegrees[index];
                    break;
                }
            }
        }
    }
    return {};
}

Base::Result<void> ModuleCatalog::RegisterMetadata(
    Core::MetadataDomain& domain) const noexcept {
    Base::Result<void> builtIns = RegisterBuiltInUiModules(domain);
    if (!builtIns) return builtIns.GetStatus();
    Base::Vector<std::uint32_t> order;
    Base::Result<void> resolved = ResolveOrder(order);
    if (!resolved) return resolved.GetStatus();
    for (std::uint32_t index : order) {
        const Module& module = modules_[index];
        const Base::StringView name = module.name.View();
        Base::Result<void> registered =
            domain.TryRegisterModule({
                Core::MakeMetadataModuleId(name),
                name,
                module.schemaVersion,
                module.registerModule,
                module.registerModuleWithContext,
                module.context});
        if (!registered) return registered.GetStatus();
    }
    return {};
}

Base::Result<void> ModuleCatalog::Freeze() noexcept {
    Base::Vector<std::uint32_t> order;
    Base::Result<void> resolved = ResolveOrder(order);
    if (!resolved) return resolved.GetStatus();
    frozen_ = true;
    return {};
}

} // namespace Aero
