#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDescriptors.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Markup/Resources/XamlNamesResources.hpp>
#include <Aero/Presentation/VisualTreeMount.hpp>

#include <atomic>
#include <utility>

namespace Aero::Markup {

class AERO_API XamlEffectLifetime final : public Base::Object {
public:
    XamlEffectLifetime() noexcept = default;
    ~XamlEffectLifetime() noexcept override = default;

    bool IsActive() const noexcept {
        return active_.load(std::memory_order_acquire);
    }
    void Invalidate() noexcept {
        active_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> active_{true};
};

enum class XamlEffectCommitMode : std::uint8_t {
    Immediate = 0U,
    Deferred
};

using XamlEffectCommitCallback = Base::Result<std::uint64_t> (*)(
    void* context) noexcept;
using XamlEffectRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using XamlEffectCleanupCallback = void (*)(void* context) noexcept;

struct XamlVisualContentEdge final {
    Base::Ref<Base::Object> parentOwner;
    Base::Ref<Base::Object> childOwner;
    Core::ContentClearCallback clearContent = nullptr;
    void* contentContext = nullptr;
};

// Markup-owned declaration result for visual content. The plan intentionally
// stores only content ownership and Presentation mount edges; Presentation owns
// the actual attach/detach sequence through VisualTreeMount.
struct XamlVisualContentPlan final {
    Base::Vector<XamlVisualContentEdge> contentEdges;
    Base::Vector<Presentation::VisualTreeMountEdge> mountEdges;
    Base::Vector<Presentation::Visual*> nodes;

    Base::Result<void> TryReserve(
        std::uint32_t contentEdgeCount,
        std::uint32_t mountEdgeCount,
        std::uint32_t nodeCount) noexcept;
    Base::Result<void> TryAddNode(
        Presentation::Visual& node) noexcept;
    void ReleaseContent() noexcept;
    void Clear() noexcept;
    std::uint32_t EdgeCount() const noexcept {
        return mountEdges.Size();
    }
    std::uint32_t NodeCount() const noexcept {
        return nodes.Size();
    }
};


struct XamlCommittedEffect final {
    Base::Ref<XamlEffectLifetime> lifetime;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Core::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle property;
    Core::PropertyExpression pendingExpression;
    void* context = nullptr;
    std::uint64_t token = 0U;
    XamlEffectCommitCallback commit = nullptr;
    XamlEffectRollbackCallback rollback = nullptr;
    XamlEffectCleanupCallback cleanup = nullptr;
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

class XamlCommittedEffectPlan final {
public:
    XamlCommittedEffectPlan() noexcept = default;
    ~XamlCommittedEffectPlan() noexcept { Rollback(); }

    XamlCommittedEffectPlan(
        XamlCommittedEffectPlan&& other) noexcept
        : effects_(std::move(other.effects_)) {}
    XamlCommittedEffectPlan& operator=(
        XamlCommittedEffectPlan&& other) noexcept {
        if (this == &other) return *this;
        Rollback();
        effects_ = std::move(other.effects_);
        return *this;
    }

    XamlCommittedEffectPlan(const XamlCommittedEffectPlan&) = delete;
    XamlCommittedEffectPlan& operator=(
        const XamlCommittedEffectPlan&) = delete;

    Base::Vector<XamlCommittedEffect>& Items() noexcept { return effects_; }
    const Base::Vector<XamlCommittedEffect>& Items() const noexcept {
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
    Base::Vector<XamlCommittedEffect> effects_;
};

// Ownership returned by a successful XAML load. The object writer remains a
// short-lived loading session; mounted runtimes keep names, resources, and the
// visual content plan here instead of reaching back into Markup services.
struct XamlLoadResult final {
    Base::Ref<Base::Object> root;
    NameScope names;
    ResourceDictionary resources;
    XamlVisualContentPlan visualContent;
    XamlCommittedEffectPlan effects;
    Base::ResourceUri canonicalUri;
    Base::Vector<Base::ResourceUri> dependencies;
    Base::Ref<XamlEffectLifetime> runtimeLifetime;

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
    }
};

} // namespace Aero::Markup
