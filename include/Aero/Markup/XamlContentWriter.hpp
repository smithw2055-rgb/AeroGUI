#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Markup/XamlLoadResult.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

// Translates visual ContentFacet assignments into a declarative content plan.
// It does not own ObjectTree, LayoutManager, RenderManager, root mounting, or
// any long-lived runtime state. Presentation consumes the resulting plan.
class AERO_API XamlContentWriter final {
public:
    explicit XamlContentWriter(XamlVisualContentPlan& plan) noexcept
        : plan_(&plan) {}

    XamlContentWriter(const XamlContentWriter&) = delete;
    XamlContentWriter& operator=(const XamlContentWriter&) = delete;

    Base::Result<void> Register(XamlSchemaContext& schema) noexcept;
    Base::Result<void> Discard() noexcept;

    std::uint32_t StagedContentCount() const noexcept {
        return plan_ != nullptr ? plan_->EdgeCount() : 0U;
    }

private:
    XamlVisualContentPlan* plan_ = nullptr;
    XamlSchemaContext* schema_ = nullptr;

    Base::Result<Presentation::Visual*> ResolveVisual(
        Base::Object& object, Core::TypeId type) const noexcept;
    Base::Result<Presentation::UIElement*> ResolveUIElement(
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

} // namespace Aero::Markup
