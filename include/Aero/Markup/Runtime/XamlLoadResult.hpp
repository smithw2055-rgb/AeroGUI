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

#include <utility>

namespace Aero::Markup {

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
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Core::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle property;
    void* rollbackContext = nullptr;
    std::uint64_t rollbackToken = 0U;
    void (*rollback)(void* context, std::uint64_t token) noexcept = nullptr;

    void Rollback() noexcept {
        if (effectiveValues != nullptr && target != nullptr) {
            static_cast<void>(effectiveValues->ClearLocalExpression(
                *target, property));
        }
        if (rollback != nullptr) {
            rollback(rollbackContext, rollbackToken);
        }
        effectiveValues = nullptr;
        target = nullptr;
        rollbackContext = nullptr;
        rollbackToken = 0U;
        rollback = nullptr;
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

    void Clear() noexcept {
        effects.Rollback();
        root.Reset();
        names.Clear();
        resources.Clear();
        visualContent.ReleaseContent();
        visualContent.Clear();
        canonicalUri = {};
        dependencies.Clear();
    }
};

} // namespace Aero::Markup
