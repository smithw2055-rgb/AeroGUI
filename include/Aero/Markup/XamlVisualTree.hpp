#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Core/EffectiveValueEngine.hpp>
#include <Aero/Core/ObjectTree.hpp>
#include <Aero/Core/Rendering.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

using XamlAsVisualCallback = Core::Visual* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsContentPresenterCallback = Core::ContentPresenter* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsContentOwnerCallback = Core::UIElement* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsStackPanelCallback = Core::StackPanel* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsCollectionOwnerCallback = Core::UIElement* (*)(
    Base::Object& object, void* context) noexcept;
using XamlConfigureCollectionChildCallback = Base::Result<void> (*)(
    Base::Object& parentObject,
    Core::UIElement& parent,
    Core::UIElement& child,
    void* context) noexcept;

struct XamlVisualTreeTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsVisualCallback asVisual = nullptr;
    void* context = nullptr;
};

struct XamlContentPresenterRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsContentPresenterCallback asPresenter = nullptr;
    void* context = nullptr;
    // Generic single-content controls (for example Border) keep their child
    // alive through XamlVisualTreeHost until Unmount(). ContentPresenter uses
    // its stronger control-owned reference through asPresenter instead.
    XamlAsContentOwnerCallback asContentOwner = nullptr;
};

struct XamlCollectionContentRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    Core::MemberId member = Core::InvalidMemberId;
    XamlAsStackPanelCallback asStackPanel = nullptr;
    void* context = nullptr;
    // Non-StackPanel collection containers need no additional child owner:
    // XamlVisualTreeHost keeps staged child references until Unmount().
    XamlAsCollectionOwnerCallback asCollectionOwner = nullptr;
    XamlConfigureCollectionChildCallback configureChild = nullptr;
};

// Commits XAML ContentPresenter relationships as one UI-tree transaction.
// Content member writes only stage edges while the object writer is running;
// Mount() performs the tree/layout/render mutations after the XAML document
// was built successfully. Call Unmount() before releasing the XAML root.
class AERO_API XamlVisualTreeHost final {
public:
    XamlVisualTreeHost(
        Core::ObjectTree& tree,
        Core::LayoutManager& layout,
        Core::EffectiveValueEngine& values,
        Core::RenderManager* renderer = nullptr) noexcept;
    ~XamlVisualTreeHost() noexcept;

    XamlVisualTreeHost(const XamlVisualTreeHost&) = delete;
    XamlVisualTreeHost& operator=(const XamlVisualTreeHost&) = delete;

    Base::Result<void> TryRegisterType(
        const XamlVisualTreeTypeRegistration& registration) noexcept;
    Base::Result<void> TryRegisterContentPresenter(
        const XamlContentPresenterRegistration& registration) noexcept;
    Base::Result<void> TryRegisterCollectionContent(
        const XamlCollectionContentRegistration& registration) noexcept;
    Base::Result<void> Register(
        XamlSchemaContext& schema) noexcept;

    Base::Result<void> Mount(
        Base::Object& root,
        Core::TypeId rootType,
        Core::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;
    Base::Result<void> DiscardStaged() noexcept;
    bool IsMounted() const noexcept { return mounted_; }
    std::uint32_t StagedContentCount() const noexcept {
        return edges_.Size();
    }

private:
    struct Edge final {
        Base::Ref<Base::Object> parentOwner;
        Base::Ref<Base::Object> childOwner;
        Core::UIElement* parent = nullptr;
        Core::UIElement* child = nullptr;
        Core::ContentPresenter* presenter = nullptr;
        Core::StackPanel* stackPanel = nullptr;
        XamlConfigureCollectionChildCallback configureCollectionChild = nullptr;
        void* collectionContext = nullptr;
        bool logicalAttached = false;
        bool visualAttached = false;
        bool layoutAttached = false;
        bool renderAttached = false;
    };

    Core::ObjectTree* tree_ = nullptr;
    Core::LayoutManager* layout_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Core::RenderManager* renderer_ = nullptr;
    XamlSchemaContext* schema_ = nullptr;
    Base::Vector<XamlVisualTreeTypeRegistration> types_;
    Base::Vector<XamlContentPresenterRegistration> presenters_;
    Base::Vector<XamlCollectionContentRegistration> collections_;
    Base::Vector<Edge> edges_;
    Base::Vector<Core::Visual*> nodes_;
    Core::Visual* rootNode_ = nullptr;
    Core::UIElement* rootLayout_ = nullptr;
    Core::FrameworkElement* rootRender_ = nullptr;
    bool mounted_ = false;

    const XamlVisualTreeTypeRegistration* FindType(
        Core::TypeId type) const noexcept;
    const XamlContentPresenterRegistration* FindPresenter(
        Core::TypeId type) const noexcept;
    const XamlCollectionContentRegistration* FindCollection(
        Core::TypeId type, Core::MemberId member) const noexcept;
    Base::Result<Core::Visual*> ResolveVisual(
        Base::Object& object, Core::TypeId type) const noexcept;
    Base::Result<Core::UIElement*> ResolveUIElement(
        Base::Object& object, Core::TypeId type) const noexcept;
    Core::FrameworkElement* ResolveFrameworkElement(
        Base::Object& object, Core::TypeId type) const noexcept;
    Base::Result<void> StageContent(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services) noexcept;
    Base::Result<void> AddNode(
        Core::Visual& node) noexcept;
    Base::Result<void> AttachEdge(Edge& edge) noexcept;
    void DetachEdge(Edge& edge) noexcept;

    static Base::Result<void> SetContentMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

// Load through this helper whenever the schema contains XamlVisualTreeHost
// adapters. It clears any stale staged edges if parsing or object construction
// fails, leaving the host ready for the next document.
AERO_API Base::Result<Base::Ref<Base::Object>>
LoadXamlVisualTreeWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

} // namespace Aero::Markup
