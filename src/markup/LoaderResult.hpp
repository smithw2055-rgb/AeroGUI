#pragma once

// Private transaction result consumed by Loader, UiDocument, and ViewRuntime.

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include "core/property/EffectiveValueEngine.hpp"
#include "Loader.hpp"
#include <Aero/Markup/Resources.hpp>
#include "../ui/VisualTreeMount.hpp"

#include <utility>

namespace Aero::Markup {

using EffectCommitCallback = Base::Result<std::uint64_t> (*)(
    void* context) noexcept;
using EffectRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using EffectCleanupCallback = void (*)(void* context) noexcept;

struct VisualContentEdge final {
    Base::Ref<Base::Object> parentOwner;
    Base::Ref<Base::Object> childOwner;
    Core::MetadataRuntime* runtime = nullptr;
    Core::MemberId member = Core::InvalidMemberId;
    bool property = false;
};

// Markup-owned declaration result for visual content. The plan intentionally
// stores only content ownership and UI mount edges; the UI runtime owns
// the actual attach/detach sequence through VisualTreeMount.
struct VisualContentPlan final {
    Base::Vector<VisualContentEdge> contentEdges;
    Base::Vector<Aero::Detail::VisualTreeMountEdge> mountEdges;
    Base::Vector<Aero::Visual*> nodes;

    Base::Result<void> TryReserve(
        std::uint32_t contentEdgeCount,
        std::uint32_t mountEdgeCount,
        std::uint32_t nodeCount) noexcept;
    Base::Result<void> TryAddNode(
        Aero::Visual& node) noexcept;
    void ReleaseContent() noexcept;
    void Clear() noexcept;
    std::uint32_t EdgeCount() const noexcept {
        return mountEdges.Size();
    }
    std::uint32_t NodeCount() const noexcept {
        return nodes.Size();
    }
};


struct CommittedEffect final {
    Base::Ref<EffectLifetime> lifetime;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Core::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle property;
    Core::PropertyExpression pendingExpression;
    void* context = nullptr;
    std::uint64_t token = 0U;
    EffectCommitCallback commit = nullptr;
    EffectRollbackCallback rollback = nullptr;
    EffectCleanupCallback cleanup = nullptr;
    bool committed = false;

    Base::Result<void> Commit() noexcept {
        if (committed) return {};
        if (lifetime && !lifetime->IsActive()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML effect runtime is no longer active");
        }
        if (pendingExpression.IsValid()) {
            if (effectiveValues == nullptr || target == nullptr ||
                !property.IsValid()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Deferred XAML property expression is invalid");
            }
            Base::Result<void> installed =
                effectiveValues->SetLocalExpression(
                    *target, property, pendingExpression);
            if (!installed) return installed.GetStatus();
            pendingExpression = {};
            committed = true;
            return {};
        }
        if (commit != nullptr) {
            Base::Result<std::uint64_t> result = commit(context);
            if (!result) return result.GetStatus();
            token = result.Value();
            committed = true;
            return {};
        }
        committed = true;
        return {};
    }

    void Rollback() noexcept {
        const bool runtimeActive = !lifetime || lifetime->IsActive();
        if (committed && runtimeActive) {
            if (effectiveValues != nullptr && target != nullptr) {
                static_cast<void>(effectiveValues->ClearLocalExpression(
                    *target, property));
            }
            if (rollback != nullptr) {
                rollback(context, token);
            }
        } else if (!committed && pendingExpression.cleanup != nullptr) {
            pendingExpression.cleanup(pendingExpression.context);
        }
        if (cleanup != nullptr) cleanup(context);
        lifetime.Reset();
        effectiveValues = nullptr;
        target = nullptr;
        pendingExpression = {};
        context = nullptr;
        token = 0U;
        commit = nullptr;
        rollback = nullptr;
        cleanup = nullptr;
        committed = false;
    }
};

class CommittedEffectPlan final {
public:
    CommittedEffectPlan() noexcept = default;
    ~CommittedEffectPlan() noexcept { Rollback(); }

    CommittedEffectPlan(
        CommittedEffectPlan&& other) noexcept
        : effects_(std::move(other.effects_)) {}
    CommittedEffectPlan& operator=(
        CommittedEffectPlan&& other) noexcept {
        if (this == &other) return *this;
        Rollback();
        effects_ = std::move(other.effects_);
        return *this;
    }

    CommittedEffectPlan(const CommittedEffectPlan&) = delete;
    CommittedEffectPlan& operator=(
        const CommittedEffectPlan&) = delete;

    Base::Vector<CommittedEffect>& Items() noexcept { return effects_; }
    const Base::Vector<CommittedEffect>& Items() const noexcept {
        return effects_;
    }
    Base::Result<void> Commit() noexcept {
        for (std::uint32_t index = 0U; index < effects_.Size(); ++index) {
            Base::Result<void> committed = effects_[index].Commit();
            if (committed) continue;
            for (std::uint32_t rollbackIndex = index;
                 rollbackIndex > 0U; --rollbackIndex) {
                effects_[rollbackIndex - 1U].Rollback();
            }
            return committed.GetStatus();
        }
        return {};
    }
    void Rollback() noexcept {
        for (std::uint32_t index = effects_.Size(); index > 0U; --index) {
            effects_[index - 1U].Rollback();
        }
        effects_.Clear();
    }
    std::uint32_t Size() const noexcept { return effects_.Size(); }

private:
    Base::Vector<CommittedEffect> effects_;
};

// Ownership returned by a successful XAML load. The object writer remains a
// short-lived loading session; mounted runtimes keep names, resources, and the
// visual content plan here instead of reaching back into Markup services.
struct LoaderResult final {
    Base::Ref<Base::Object> root;
    Aero::NameScope names;
    Aero::ResourceDictionary resources;
    VisualContentPlan visualContent;
    CommittedEffectPlan effects;
    Base::ResourceUri canonicalUri;
    Base::Vector<Base::ResourceUri> dependencies;
    Base::Ref<EffectLifetime> runtimeLifetime;
    bool hasDeferredStaticResources = false;

    void Clear() noexcept {
        effects.Rollback();
        root.Reset();
        names.Clear();
        resources.Clear();
        visualContent.ReleaseContent();
        visualContent.Clear();
        canonicalUri = {};
        dependencies.Clear();
        runtimeLifetime.Reset();
        hasDeferredStaticResources = false;
    }
};

} // namespace Aero::Markup
