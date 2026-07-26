#include <Aero/RuntimeServices.hpp>

namespace Aero {
namespace {

Base::Status ServicesInvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

} // namespace

RuntimeObjectStateStore::RuntimeObjectStateStore(
    Base::IAllocator* allocator) noexcept
    : states_(allocator) {}

RuntimeObjectState* RuntimeObjectStateStore::Find(
    Presentation::VisualHandle handle) noexcept {
    if (!handle.IsValid()) return nullptr;
    for (RuntimeObjectState& state : states_) {
        if (state.handle.index == handle.index &&
            state.handle.generation == handle.generation) {
            return &state;
        }
    }
    return nullptr;
}

const RuntimeObjectState* RuntimeObjectStateStore::Find(
    Presentation::VisualHandle handle) const noexcept {
    if (!handle.IsValid()) return nullptr;
    for (const RuntimeObjectState& state : states_) {
        if (state.handle.index == handle.index &&
            state.handle.generation == handle.generation) {
            return &state;
        }
    }
    return nullptr;
}

Base::Result<RuntimeObjectState*>
RuntimeObjectStateStore::Ensure(
    Presentation::VisualHandle handle) noexcept {
    if (!handle.IsValid()) {
        return ServicesInvalidArgument(
            "Runtime sidecar requires a valid generation handle");
    }
    RuntimeObjectState* existing = Find(handle);
    if (existing != nullptr) return existing;
    Base::Result<RuntimeObjectState*> appended =
        states_.TryEmplaceBack();
    if (!appended) return appended.GetStatus();
    appended.Value()->handle = handle;
    return appended.Value();
}

bool RuntimeObjectStateStore::Remove(
    Presentation::VisualHandle handle) noexcept {
    for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
        RuntimeObjectState& state = states_[index];
        if (state.handle.index != handle.index ||
            state.handle.generation != handle.generation) {
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < states_.Size(); ++next) {
            states_[next - 1U] = states_[next];
        }
        states_.PopBack();
        return true;
    }
    return false;
}

std::uint32_t RuntimeObjectStateStore::Prune(
    const Presentation::ObjectTree& tree) noexcept {
    std::uint32_t removed = 0U;
    for (std::uint32_t index = 0U; index < states_.Size();) {
        if (tree.ResolveHandle(states_[index].handle) != nullptr) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < states_.Size(); ++next) {
            states_[next - 1U] = states_[next];
        }
        states_.PopBack();
        ++removed;
    }
    return removed;
}

} // namespace Aero
