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

using XamlAsTreeNodeCallback = Core::TreeNode* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsLayoutElementCallback = Core::LayoutElement* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsRenderElementCallback = Core::RenderElement* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsContentPresenterCallback = Core::ContentPresenter* (*)(
    Base::Object& object, void* context) noexcept;
using XamlAsStackPanelCallback = Core::StackPanel* (*)(
    Base::Object& object, void* context) noexcept;

struct XamlVisualTreeTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsTreeNodeCallback asTreeNode = nullptr;
    XamlAsLayoutElementCallback asLayoutElement = nullptr;
    XamlAsRenderElementCallback asRenderElement = nullptr;
    void* context = nullptr;
};

struct XamlContentPresenterRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsContentPresenterCallback asPresenter = nullptr;
    void* context = nullptr;
};

struct XamlCollectionContentRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    Core::MemberId member = Core::InvalidMemberId;
    XamlAsStackPanelCallback asStackPanel = nullptr;
    void* context = nullptr;
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
        Core::RenderManager* renderer = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlVisualTreeHost() noexcept;

    XamlVisualTreeHost(const XamlVisualTreeHost&) = delete;
    XamlVisualTreeHost& operator=(const XamlVisualTreeHost&) = delete;

    AERO_NODISCARD Base::Result<void> TryRegisterType(
        const XamlVisualTreeTypeRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<void> TryRegisterContentPresenter(
        const XamlContentPresenterRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<void> TryRegisterCollectionContent(
        const XamlCollectionContentRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<void> Register(
        XamlSchemaContext& schema) noexcept;

    AERO_NODISCARD Base::Result<void> Mount(
        Base::Object& root,
        Core::TypeId rootType,
        Core::Size availableSize) noexcept;
    AERO_NODISCARD Base::Result<void> Unmount() noexcept;
    AERO_NODISCARD Base::Result<void> DiscardStaged() noexcept;
    AERO_NODISCARD bool IsMounted() const noexcept { return mounted_; }
    AERO_NODISCARD std::uint32_t StagedContentCount() const noexcept {
        return edges_.Size();
    }

private:
    struct Edge final {
        Core::LayoutElement* parent = nullptr;
        Core::LayoutElement* child = nullptr;
        Core::ContentPresenter* presenter = nullptr;
        Core::StackPanel* stackPanel = nullptr;
        bool logicalAttached = false;
        bool visualAttached = false;
        bool layoutAttached = false;
        bool renderAttached = false;
    };

    Core::ObjectTree* tree_ = nullptr;
    Core::LayoutManager* layout_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Core::RenderManager* renderer_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    XamlSchemaContext* schema_ = nullptr;
    Base::Vector<XamlVisualTreeTypeRegistration> types_;
    Base::Vector<XamlContentPresenterRegistration> presenters_;
    Base::Vector<XamlCollectionContentRegistration> collections_;
    Base::Vector<Edge> edges_;
    Base::Vector<Core::TreeNode*> nodes_;
    Core::TreeNode* rootNode_ = nullptr;
    Core::LayoutElement* rootLayout_ = nullptr;
    Core::RenderElement* rootRender_ = nullptr;
    bool mounted_ = false;

    AERO_NODISCARD const XamlVisualTreeTypeRegistration* FindType(
        Core::TypeId type) const noexcept;
    AERO_NODISCARD const XamlContentPresenterRegistration* FindPresenter(
        Core::TypeId type) const noexcept;
    AERO_NODISCARD const XamlCollectionContentRegistration* FindCollection(
        Core::TypeId type, Core::MemberId member) const noexcept;
    AERO_NODISCARD Base::Result<Core::TreeNode*> ResolveTreeNode(
        Base::Object& object, Core::TypeId type) const noexcept;
    AERO_NODISCARD Base::Result<Core::LayoutElement*> ResolveLayoutElement(
        Base::Object& object, Core::TypeId type) const noexcept;
    AERO_NODISCARD Core::RenderElement* ResolveRenderElement(
        Base::Object& object, Core::TypeId type) const noexcept;
    AERO_NODISCARD Base::Result<void> StageContent(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services) noexcept;
    AERO_NODISCARD Base::Result<void> AddNode(
        Core::TreeNode& node) noexcept;
    AERO_NODISCARD Base::Result<void> AttachEdge(Edge& edge) noexcept;
    void DetachEdge(Edge& edge) noexcept;

    static AERO_NODISCARD Base::Result<void> SetContentMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

// Load through this helper whenever the schema contains XamlVisualTreeHost
// adapters. It clears any stale staged edges if parsing or object construction
// fails, leaving the host ready for the next document.
AERO_NODISCARD AERO_API Base::Result<Base::Ref<Base::Object>>
LoadXamlVisualTreeWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

} // namespace Aero::Markup
