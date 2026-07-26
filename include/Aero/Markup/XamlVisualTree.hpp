#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/VisualTreeMount.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlLoadResult.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

// Stages visual content declared through Core::ContentFacet. Presentation owns
// the actual logical, visual, layout, render, resize, and detach sequence via
// VisualTreeMount; custom controls participate by registering metadata facets.
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
    Base::Result<void> Resize(
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;
    Base::Result<void> DiscardStaged() noexcept;
    bool IsMounted() const noexcept { return mount_.IsMounted(); }
    std::uint32_t StagedContentCount() const noexcept {
        return stagedContent_.EdgeCount();
    }
    XamlVisualContentPlan TakeStagedContent() noexcept;

private:
    Core::EffectiveValueEngine* values_ = nullptr;
    Presentation::VisualTreeMount mount_;
    XamlSchemaContext* schema_ = nullptr;
    XamlVisualContentPlan stagedContent_;

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

AERO_API Base::Result<XamlLoadResult>
LoadXamlVisualTreeDocumentWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

AERO_API Base::Result<XamlLoadResult>
LoadXamlVisualTreeDocumentWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

} // namespace Aero::Markup
