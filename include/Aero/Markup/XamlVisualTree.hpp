#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/MountService.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

// Stages visual content declared through Core::ContentFacet and then mounts the
// resulting graph through Presentation::MountService. The host deliberately has
// no control-specific registration surface; custom controls participate by
// registering metadata factories and content accessors.
class AERO_API XamlVisualTreeHost final {
public:
    XamlVisualTreeHost(
        Presentation::ObjectTree& tree,
        Presentation::LayoutManager& layout,
        Core::EffectiveValueEngine& values,
        Presentation::RenderManager* renderer = nullptr) noexcept;
    ~XamlVisualTreeHost() noexcept;

    XamlVisualTreeHost(const XamlVisualTreeHost&) = delete;
    XamlVisualTreeHost& operator=(const XamlVisualTreeHost&) = delete;

    Base::Result<void> Register(XamlSchemaContext& schema) noexcept;

    Base::Result<void> Mount(
        Base::Object& root,
        Core::TypeId rootType,
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;
    Base::Result<void> DiscardStaged() noexcept;
    bool IsMounted() const noexcept { return mounted_; }
    std::uint32_t StagedContentCount() const noexcept { return edges_.Size(); }

private:
    struct Edge final {
        Base::Ref<Base::Object> parentOwner;
        Base::Ref<Base::Object> childOwner;
        Presentation::UIElement* parent = nullptr;
        Presentation::UIElement* child = nullptr;
        Core::ContentClearCallback clearContent = nullptr;
        void* contentContext = nullptr;
        Presentation::MountEdgeState mount;
    };

    Presentation::ObjectTree* tree_ = nullptr;
    Presentation::LayoutManager* layout_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Presentation::RenderManager* renderer_ = nullptr;
    Presentation::MountService mounts_;
    Presentation::MountRootState rootMount_;
    XamlSchemaContext* schema_ = nullptr;
    Base::Vector<Edge> edges_;
    Base::Vector<Presentation::Visual*> nodes_;
    Presentation::Visual* rootNode_ = nullptr;
    Presentation::UIElement* rootLayout_ = nullptr;
    Presentation::FrameworkElement* rootRender_ = nullptr;
    bool mounted_ = false;

    Base::Result<Presentation::Visual*> ResolveVisual(
        Base::Object& object, Core::TypeId type) const noexcept;
    Base::Result<Presentation::UIElement*> ResolveUIElement(
        Base::Object& object, Core::TypeId type) const noexcept;
    Presentation::FrameworkElement* ResolveFrameworkElement(
        Base::Object& object, Core::TypeId type) const noexcept;
    Base::Result<void> StageContent(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services) noexcept;
    Base::Result<void> AddNode(Presentation::Visual& node) noexcept;
    Base::Result<void> AttachEdge(Edge& edge) noexcept;
    void ReleaseStagedContent() noexcept;

    static bool HandlesContentMember(
        const XamlResolvedMember& member,
        void* context) noexcept;
    static Base::Result<void> SetContentMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

AERO_API Base::Result<Base::Ref<Base::Object>>
LoadXamlVisualTreeWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

AERO_API Base::Result<Base::Ref<Base::Object>>
LoadXamlVisualTreeWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

} // namespace Aero::Markup
