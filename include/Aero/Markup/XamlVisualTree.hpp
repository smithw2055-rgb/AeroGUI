#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/MountService.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

using XamlAsVisualCallback = Presentation::Visual* (*)(
    Base::Object& object, void* context) noexcept;
using XamlSetSingleContentCallback = Base::Result<void> (*)(
    Base::Object& parentObject,
    const Base::Ref<Base::Object>& childObject,
    Presentation::UIElement& child,
    void* context) noexcept;
using XamlClearContentCallback = Base::Result<void> (*)(
    Base::Object& parentObject,
    void* context) noexcept;
using XamlAddCollectionChildCallback = Base::Result<void> (*)(
    Base::Object& parentObject,
    const Base::Ref<Base::Object>& childObject,
    Presentation::UIElement& child,
    void* context) noexcept;
using XamlClearCollectionCallback = Base::Result<void> (*)(
    Base::Object& parentObject,
    void* context) noexcept;
using XamlConfigureCollectionChildCallback = Base::Result<void> (*)(
    Base::Object& parentObject,
    Presentation::UIElement& parent,
    Presentation::UIElement& child,
    void* context) noexcept;

struct XamlVisualTreeTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsVisualCallback asVisual = nullptr;
    void* context = nullptr;
};

struct XamlSingleContentRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlSetSingleContentCallback setContent = nullptr;
    XamlClearContentCallback clearContent = nullptr;
    void* context = nullptr;
};

struct XamlCollectionContentRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    Core::MemberId member = Core::InvalidMemberId;
    XamlAddCollectionChildCallback addChild = nullptr;
    XamlClearCollectionCallback clearChildren = nullptr;
    XamlConfigureCollectionChildCallback configureChild = nullptr;
    void* context = nullptr;
};

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

    Base::Result<void> TryRegisterType(
        const XamlVisualTreeTypeRegistration& registration) noexcept;
    Base::Result<void> TryRegisterSingleContent(
        const XamlSingleContentRegistration& registration) noexcept;
    Base::Result<void> TryRegisterCollectionContent(
        const XamlCollectionContentRegistration& registration) noexcept;
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
        XamlClearContentCallback clearSingle = nullptr;
        XamlClearCollectionCallback clearCollection = nullptr;
        XamlConfigureCollectionChildCallback configureCollectionChild = nullptr;
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
    Base::Vector<XamlVisualTreeTypeRegistration> types_;
    Base::Vector<XamlSingleContentRegistration> singles_;
    Base::Vector<XamlCollectionContentRegistration> collections_;
    Base::Vector<Edge> edges_;
    Base::Vector<Presentation::Visual*> nodes_;
    Presentation::Visual* rootNode_ = nullptr;
    Presentation::UIElement* rootLayout_ = nullptr;
    Presentation::FrameworkElement* rootRender_ = nullptr;
    bool mounted_ = false;

    const XamlVisualTreeTypeRegistration* FindType(Core::TypeId type) const noexcept;
    const XamlSingleContentRegistration* FindSingle(Core::TypeId type) const noexcept;
    const XamlCollectionContentRegistration* FindCollection(
        Core::TypeId type, Core::MemberId member) const noexcept;
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
    void DetachEdge(Edge& edge) noexcept;
    void ReleaseStagedContent() noexcept;

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
