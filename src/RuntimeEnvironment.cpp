#include <Aero/RuntimeEnvironment.hpp>

namespace Aero {

RuntimeEnvironment::RuntimeEnvironment(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      schema_(allocator_),
      documents_(allocator_) {}

Base::Result<void> RuntimeEnvironment::AddModule(
    const ModuleRegistration& registration) noexcept {
    if (initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Runtime environment modules are frozen");
    }
    return modules_.TryAdd(registration);
}

Base::Result<void> RuntimeEnvironment::Initialize() noexcept {
    if (initialized_) return {};
    Base::Result<void> prepared = schema_.Prepare(modules_);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> finalized = schema_.Finalize(
        modules_, SchemaBundleServices{allocator_});
    if (!finalized) return finalized.GetStatus();
    Base::Result<void> frozen = modules_.Freeze();
    if (!frozen) return frozen.GetStatus();
    initialized_ = true;
    return {};
}

bool RuntimeEnvironment::IsInitialized() const noexcept {
    return initialized_ && schema_.IsFrozen();
}

RuntimeView::RuntimeView(
    RuntimeEnvironment& environment,
    Base::IAllocator* allocator) noexcept
    : environment_(&environment),
      host_(environment.Schema(), environment.Documents(), allocator) {}

Base::Result<void> RuntimeView::Initialize(
    const RuntimeHostOptions& options) noexcept {
    if (environment_ == nullptr || !environment_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Runtime environment must be initialized before creating a view");
    }
    return host_.Initialize(options);
}

} // namespace Aero
