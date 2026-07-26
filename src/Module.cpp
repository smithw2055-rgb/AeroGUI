#include <Aero/Module.hpp>

#include <Aero/BuiltinModules.hpp>

#include <utility>

namespace Aero {

Base::Result<void> ModuleCatalog::TryAdd(
    const ModuleRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Aero module catalog is frozen");
    }
    if (registration.name.Empty() || registration.schemaVersion == 0U ||
        registration.registerModule == nullptr) {
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
    module.context = registration.context;
    return modules_.TryPushBack(std::move(module));
}

Base::Result<void> ModuleCatalog::RegisterMetadata(
    Core::MetadataDomain& domain) const noexcept {
    Base::Result<void> builtIns = RegisterBuiltInUiModules(domain);
    if (!builtIns) return builtIns.GetStatus();
    for (const Module& module : modules_) {
        const Base::StringView name = module.name.View();
        Base::Result<void> registered =
            domain.TryRegisterModule({
                Core::MakeMetadataModuleId(name),
                name,
                module.schemaVersion,
                module.registerModule,
                module.context});
        if (!registered) return registered.GetStatus();
    }
    return {};
}

Base::Result<void> ModuleCatalog::Freeze() noexcept {
    frozen_ = true;
    return {};
}

} // namespace Aero
