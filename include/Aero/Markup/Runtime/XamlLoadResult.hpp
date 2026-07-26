#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDescriptors.hpp>
#include <Aero/Markup/Resources/XamlNamesResources.hpp>
#include <Aero/Presentation/VisualTreeMount.hpp>

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

// Ownership returned by a successful XAML load. The object writer remains a
// short-lived loading session; mounted runtimes keep names, resources, and the
// visual content plan here instead of reaching back into Markup services.
struct XamlLoadResult final {
    Base::Ref<Base::Object> root;
    NameScope names;
    ResourceDictionary resources;
    XamlVisualContentPlan visualContent;
    Base::ResourceUri canonicalUri;
    Base::Vector<Base::ResourceUri> dependencies;

    void Clear() noexcept {
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
