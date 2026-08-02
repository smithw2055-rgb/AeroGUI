#include <Aero/View.hpp>

#include <Aero/Integration/Providers/XamlProvider.hpp>
#include <Aero/Integration/ViewOptions.hpp>
#include "runtime/GuiData.hpp"

#include <new>
#include <utility>


namespace Aero {


Gui::Gui(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    Base::Result<Base::Ref<Impl>> made =
        Base::MakeRefWithAllocator<Impl>(
            *allocator_, *allocator_);
    if (!made) {
        Base::ReportOutOfMemory(
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::Object);
    }
    impl_ = Base::Ref<Base::Object>(std::move(made).Value());
}

Gui::~Gui() noexcept {
    impl_.Reset();
}

Base::Result<void> Gui::AddModule(
    const ModuleRegistration& registration) noexcept {
    Impl& state = static_cast<Impl&>(*impl_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui modules are frozen");
    }
    return state.modules.Add(registration);
}

Base::Result<void> Gui::Initialize() noexcept {
    Impl& state = static_cast<Impl&>(*impl_);
    if (state.initialized) return {};
    Base::Result<void> prepared = state.schema.Prepare(state.modules);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> finalized = state.schema.Finalize(
        GuiSchemaOptions{state.allocator});
    if (!finalized) return finalized.GetStatus();
    Base::Result<void> frozen = state.modules.Freeze();
    if (!frozen) return frozen.GetStatus();
    state.initialized = true;
    return {};
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    Base::IAllocator* allocator) noexcept {
    return CreateView(Integration::ViewOptions{}, allocator);
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    const Integration::ViewOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before creating a view");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : *allocator_;
    Base::Result<Base::Ref<View>> made =
        Base::MakeRefWithAllocator<View>(
            selected,
            View::ConstructionToken{},
            *this,
            &selected);
    if (!made) return made.GetStatus();
    Base::Result<void> initialized =
        made.Value()->Initialize(options);
    if (!initialized) return initialized.GetStatus();
    return std::move(made).Value();
}

bool Gui::IsInitialized() const noexcept {
    const Impl& state = static_cast<const Impl&>(*impl_);
    return state.initialized && state.schema.IsFrozen();
}

} // namespace Aero
