#include "ModuleSet.hpp"

#include "BuiltinModules.hpp"
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/markup/MarkupRuntime.hpp"
#include "gui/markup/MarkupWriterRuntime.hpp"

#include <utility>

namespace Aero {

Base::Result<void> ModuleSet::ResolveOrder(
    Base::Vector<std::uint32_t>& order) const noexcept {
    order.Clear();
    Base::Result<void> reserved =
        order.Reserve(modules_.Size());
    if (!reserved) return reserved.GetStatus();
    Base::Vector<std::uint32_t> indegrees;
    Base::Result<void> sized =
        indegrees.Resize(modules_.Size(), 0U);
    if (!sized) return sized.GetStatus();
    Base::Vector<bool> emitted;
    sized = emitted.Resize(modules_.Size(), false);
    if (!sized) return sized.GetStatus();

    for (std::uint32_t index = 0U;
         index < modules_.Size();
         ++index) {
        for (const Module::Dependency& dependency :
             modules_[index].dependencies) {
            bool found = false;
            for (const Module& candidate : modules_) {
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

    while (order.Size() < modules_.Size()) {
        std::uint32_t selected = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < modules_.Size();
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
            modules_[selected].name.View();
        for (std::uint32_t index = 0U;
             index < modules_.Size();
             ++index) {
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

ModuleSet::ModuleSet() noexcept = default;

ModuleSet::~ModuleSet() noexcept = default;

Base::Result<void> ModuleSet::Add(
    const ModuleRegistration& registration) noexcept {
    if (frozen_) {
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
    for (const Module& module : modules_) {
        if (module.name.View() == registration.name) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Aero module is already present in the catalog");
        }
    }

    Module module;
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
        for (const Module::Dependency& existing :
             module.dependencies) {
            if (existing.name.View() == dependency.name) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Aero module dependency is duplicated");
            }
        }
        Module::Dependency stored;
        Base::Result<void> dependencyName =
            stored.name.Assign(dependency.name);
        if (!dependencyName) return dependencyName.GetStatus();
        stored.minimumSchemaVersion =
            dependency.minimumSchemaVersion;
        Base::Result<void> appended =
            module.dependencies.PushBack(std::move(stored));
        if (!appended) return appended.GetStatus();
    }
    Base::Result<void> scopeCapacity = module.resourceScopes.Reserve(
        registration.resourceScopes.Size());
    if (!scopeCapacity) return scopeCapacity.GetStatus();
    for (const Markup::ResourceScopeRegistration& scope :
         registration.resourceScopes) {
        if (scope.type == Meta::InvalidTypeId ||
            scope.addResource == nullptr ||
            scope.resolve == nullptr ||
            scope.abiVersion != XamlFacetAbiVersion) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Aero module resource-scope registration is invalid");
        }
        for (const Markup::ResourceScopeRegistration& existing :
             module.resourceScopes) {
            if (existing.type == scope.type) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Aero module resource scope is duplicated");
            }
        }
        Base::Result<void> appended =
            module.resourceScopes.PushBack(scope);
        if (!appended) return appended.GetStatus();
    }
    return modules_.PushBack(std::move(module));
}

Base::Result<void> ModuleSet::RegisterMetadata(
    ::Aero::Meta::Registry& domain) const noexcept {
    Base::Result<void> builtIns =
        RegisterBuiltInUiModules(domain);
    if (!builtIns) return builtIns.GetStatus();
    Base::Vector<std::uint32_t> order;
    Base::Result<void> resolved =
        ResolveOrder(order);
    if (!resolved) return resolved.GetStatus();
    for (std::uint32_t index : order) {
        const Module& module = modules_[index];
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
    return RegisterBuiltInMarkupModule(domain);
}

Base::Result<void> ModuleSet::RegisterResourceScopes(
    ::Aero::Markup::Schema& schema) const noexcept {
    Base::Vector<std::uint32_t> order;
    Base::Result<void> resolved = ResolveOrder(order);
    if (!resolved) return resolved.GetStatus();
    for (std::uint32_t index : order) {
        const Module& module = modules_[index];
        for (const Markup::ResourceScopeRegistration& scope :
             module.resourceScopes) {
            Base::Result<void> registered =
                Markup::SchemaPrivate::AddResourceScope(schema, {
                    scope.type,
                    scope.inherited,
                    scope.addResource,
                    scope.resolve,
                    scope.context});
            if (!registered) return registered.GetStatus();
        }
    }
    return {};
}

Base::Result<void> ModuleSet::Freeze() noexcept {
    Base::Vector<std::uint32_t> order;
    Base::Result<void> resolved =
        ResolveOrder(order);
    if (!resolved) return resolved.GetStatus();
    frozen_ = true;
    return {};
}

bool ModuleSet::IsFrozen() const noexcept {
    return frozen_;
}

std::uint32_t ModuleSet::ModuleCount() const noexcept {
    return modules_.Size();
}

} // namespace Aero
