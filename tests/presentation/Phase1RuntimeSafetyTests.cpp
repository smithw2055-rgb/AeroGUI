#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Markup/RuntimeHost.hpp>
#include <Aero/Presentation/MountService.hpp>

#include "TestAllocatorScope.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Core;
using namespace Aero::Markup;
using namespace Aero::Presentation;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class FailingAllocator final : public IAllocator {
public:
    void* Allocate(const AllocationRequest& request) noexcept override {
        if (remaining_ == 0U) {
            remaining_ = UINT32_MAX;
            return nullptr;
        }
        if (remaining_ != UINT32_MAX) --remaining_;
        void* memory = upstream_.Allocate(request);
        if (memory != nullptr) ++active_;
        return memory;
    }

    void Deallocate(
        void* memory,
        std::size_t size,
        std::size_t alignment,
        MemoryTag tag) noexcept override {
        if (memory != nullptr) {
            if (active_ == 0U) std::abort();
            --active_;
        }
        upstream_.Deallocate(memory, size, alignment, tag);
    }

    void FailAfter(std::uint32_t successfulAllocations) noexcept {
        remaining_ = successfulAllocations;
    }
    void DisableFailures() noexcept { remaining_ = UINT32_MAX; }
    std::uint32_t Active() const noexcept { return active_; }

private:
    MallocAllocator upstream_;
    std::uint32_t remaining_ = UINT32_MAX;
    std::uint32_t active_ = 0U;
};

class SwitchableBackend final : public IRenderBackend {
public:
    Result<void> Submit(const RenderPlan& plan) noexcept override {
        if (failNext_) {
            failNext_ = false;
            return Status::Failure(
                ErrorCode::InternalError,
                "Injected render submission failure");
        }
        lastVersion_ = plan.Version();
        lastHash_ = plan.StableHash();
        return {};
    }

    void FailNext() noexcept { failNext_ = true; }
    std::uint64_t LastVersion() const noexcept { return lastVersion_; }
    std::uint64_t LastHash() const noexcept { return lastHash_; }

private:
    bool failNext_ = false;
    std::uint64_t lastVersion_ = 0U;
    std::uint64_t lastHash_ = 0U;
};

bool TestDualParentMountAndPresentationReorder() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());

    Result<Ref<StackPanel>> rootMade = MakeRef<StackPanel>();
    Result<Ref<StackPanel>> hostMade = MakeRef<StackPanel>();
    Result<Ref<Border>> childMade = MakeRef<Border>();
    CHECK(rootMade && hostMade && childMade);
    Ref<StackPanel> root = std::move(rootMade).Value();
    Ref<StackPanel> host = std::move(hostMade).Value();
    Ref<Border> child = std::move(childMade).Value();

    CHECK(runtime.Mount(Ref<Object>(root), {640.0, 480.0}));
    MountService mounts(*runtime.Tree(), runtime.Layout(), runtime.Renderer());

    Result<MountEdgeState> hostMount = mounts.Attach(*root, *host);
    CHECK(hostMount);
    Result<MountEdgeState> childMount =
        mounts.Attach(*root, *host, *child);
    CHECK(childMount);
    CHECK(child->LogicalParent() == root.Get());
    CHECK(child->VisualParent() == host.Get());
    CHECK(child->OwningTree() == runtime.Tree());

    CHECK(mounts.DetachPresentation(childMount.Value()));
    CHECK(child->LogicalParent() == root.Get());
    CHECK(child->VisualParent() == nullptr);
    CHECK(mounts.AttachPresentation(childMount.Value(), *host));
    CHECK(child->VisualParent() == host.Get());

    CHECK(mounts.Detach(childMount.Value()));
    CHECK(mounts.Detach(hostMount.Value()));
    CHECK(runtime.EffectiveValues()->DetachObject(*child));
    CHECK(runtime.EffectiveValues()->DetachObject(*host));
    CHECK(runtime.Unmount());
    return true;
}

bool TestDetachBeforeDeferredLayoutAndRenderFlush() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());

    Result<Ref<StackPanel>> rootMade = MakeRef<StackPanel>();
    Result<Ref<Border>> childMade = MakeRef<Border>();
    CHECK(rootMade && childMade);
    Ref<StackPanel> root = std::move(rootMade).Value();
    Ref<Border> child = std::move(childMade).Value();
    WeakRef<Border> weak(child);

    CHECK(runtime.Mount(Ref<Object>(root), {320.0, 200.0}));
    MountService mounts(*runtime.Tree(), runtime.Layout(), runtime.Renderer());
    Result<MountEdgeState> state = mounts.Attach(*root, *child);
    CHECK(state);
    CHECK(child->SetWidth(120.0));
    CHECK(child->SetBackground({0.2F, 0.3F, 0.4F, 1.0F}));
    CHECK(mounts.Detach(state.Value()));
    CHECK(runtime.EffectiveValues()->DetachObject(*child));
    child.Reset();

    CHECK(!weak.Expired());
    Result<RuntimeFrameResult> frame = runtime.RunFrame();
    CHECK(frame);
    CHECK(weak.Expired());
    CHECK(runtime.Unmount());
    return true;
}

bool TestRenderSubmissionFailureIsAtomic() {
    SwitchableBackend backend;
    RuntimeHost runtime;
    RuntimeHostOptions options;
    options.renderBackend = &backend;
    CHECK(runtime.Initialize(options));

    Result<Ref<Border>> rootMade = MakeRef<Border>();
    CHECK(rootMade);
    Ref<Border> root = std::move(rootMade).Value();
    CHECK(runtime.Mount(Ref<Object>(root), {240.0, 160.0}));
    CHECK(root->SetBackground({0.1F, 0.2F, 0.3F, 1.0F}));
    CHECK(runtime.RunFrame());

    const std::uint64_t revision = root->RenderRevision();
    const std::uint64_t version = runtime.Renderer()->CurrentPlan().Version();
    const std::uint64_t hash = runtime.Renderer()->CurrentPlan().StableHash();
    CHECK(root->SetBackground({0.8F, 0.4F, 0.2F, 1.0F}));
    CHECK(!root->IsRenderValid());

    backend.FailNext();
    Result<std::uint32_t> failed = runtime.Renderer()->Commit();
    CHECK(!failed);
    CHECK(root->RenderRevision() == revision);
    CHECK(!root->IsRenderValid());
    CHECK(runtime.Renderer()->CurrentPlan().Version() == version);
    CHECK(runtime.Renderer()->CurrentPlan().StableHash() == hash);

    Result<std::uint32_t> recovered = runtime.Renderer()->Commit();
    CHECK(recovered);
    CHECK(root->RenderRevision() == revision + 1U);
    CHECK(root->IsRenderValid());
    CHECK(runtime.Renderer()->CurrentPlan().Version() == version + 1U);
    CHECK(backend.LastVersion() == version + 1U);
    CHECK(backend.LastHash() == runtime.Renderer()->CurrentPlan().StableHash());
    CHECK(runtime.Unmount());
    return true;
}

bool TestMountAllocationFailureRollbackMatrix() {
    bool observedFailure = false;
    bool observedSuccess = false;

    for (std::uint32_t failAfter = 0U; failAfter < 24U; ++failAfter) {
        FailingAllocator allocator;
        {
            Aero::Tests::ScopedDefaultAllocator allocatorScope(allocator);
            RuntimeHost runtime(&allocator);
            CHECK(runtime.Initialize());

            Result<Ref<StackPanel>> rootMade =
                MakeRefWithAllocator<StackPanel>(allocator);
            Result<Ref<Border>> childMade =
                MakeRefWithAllocator<Border>(allocator);
            CHECK(rootMade && childMade);
            Ref<StackPanel> root = std::move(rootMade).Value();
            Ref<Border> child = std::move(childMade).Value();
            CHECK(runtime.Mount(Ref<Object>(root), {300.0, 180.0}));

            MountService mounts(
                *runtime.Tree(), runtime.Layout(), runtime.Renderer());
            allocator.FailAfter(failAfter);
            Result<MountEdgeState> mounted = mounts.Attach(*root, *child);
            allocator.DisableFailures();

            if (!mounted) {
                observedFailure = true;
                CHECK(child->LogicalParent() == nullptr);
                CHECK(child->VisualParent() == nullptr);
                CHECK(child->OwningTree() == nullptr);
                CHECK(!child->Handle().IsValid());
            } else {
                observedSuccess = true;
                CHECK(mounts.Detach(mounted.Value()));
            }
            CHECK(runtime.EffectiveValues()->DetachObject(*child));
            child.Reset();
            CHECK(runtime.Unmount());
            root.Reset();
            runtime.Shutdown();
        }
        CHECK(allocator.Active() == 0U);
    }

    CHECK(observedFailure);
    CHECK(observedSuccess);
    return true;
}

} // namespace

int main() {
    if (!TestDualParentMountAndPresentationReorder()) return 1;
    if (!TestDetachBeforeDeferredLayoutAndRenderFlush()) return 1;
    if (!TestRenderSubmissionFailureIsAtomic()) return 1;
    if (!TestMountAllocationFailureRollbackMatrix()) return 1;
    std::puts("Aero Phase 1 runtime safety tests passed");
    return 0;
}
