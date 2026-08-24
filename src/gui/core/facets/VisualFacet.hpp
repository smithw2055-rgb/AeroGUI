#pragma once

#include "gui/core/Facet.hpp"

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Visual.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdint>

namespace Aero {
class ElementTree;
class EventRouter;
namespace Controls {
class Panel;
class Decorator;
} // namespace Controls
} // namespace Aero

namespace Aero::Core {

// Visual Tree and hierarchy traversal facet
class VisualFacet : public Facet {
public:
    static constexpr FacetType StaticType = FacetType::Visual;

    explicit VisualFacet(::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}

    ::Aero::Media::Visual& Owner() const noexcept { return *owner_; }

    ElementTree* Tree() const noexcept {
        return owner_->tree_;
    }

    ::Aero::Media::Visual* LogicalParent() const noexcept {
        return owner_->logicalParent_;
    }

    ::Aero::Media::Visual* VisualParent() const noexcept {
        return owner_->visualParent_;
    }

    Base::Span<::Aero::Media::Visual* const> LogicalChildren() const noexcept {
        return { owner_->logicalChildren_.Data(), owner_->logicalChildren_.Size() };
    }

    Base::Span<::Aero::Media::Visual* const> VisualChildren() const noexcept {
        return { owner_->visualChildren_.Data(), owner_->visualChildren_.Size() };
    }

    bool IsLoaded() const noexcept {
        return owner_->loaded_;
    }

    VisualHandle Handle() const noexcept {
        return { owner_->handleIndex_, owner_->handleGeneration_ };
    }

    void SetHandle(VisualHandle handle) noexcept {
        owner_->handleIndex_ = handle.index;
        owner_->handleGeneration_ = handle.generation;
    }

    UIElement* AsUIElement() noexcept {
        return owner_->AsUIElement();
    }

    const UIElement* AsUIElement() const noexcept {
        return owner_->AsUIElement();
    }

    FrameworkElement* AsFrameworkElement() noexcept {
        return owner_->AsFrameworkElement();
    }

    const FrameworkElement* AsFrameworkElement() const noexcept {
        return owner_->AsFrameworkElement();
    }

    Base::Result<Base::Ref<Base::Object>> AcquireLifetime() noexcept {
        return owner_->AcquireLifetime();
    }

    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.AcquireLifetime();
    }

    static VisualHandle Handle(const ::Aero::Media::Visual& visual) noexcept {
        return { visual.handleIndex_, visual.handleGeneration_ };
    }

    static std::uint32_t PanelChildCount(
        const Aero::Controls::Panel& panel) noexcept;

    static Base::Ref<Base::Object> PanelChildAt(
        const Aero::Controls::Panel& panel,
        std::uint32_t index) noexcept;

    static Base::Result<void> PanelAddChild(
        Aero::Controls::Panel& panel,
        const Base::Ref<Base::Object>& owner,
        Aero::UIElement& child) noexcept;

    static Base::Result<bool> PanelRemoveChild(
        Aero::Controls::Panel& panel,
        Aero::UIElement& child) noexcept;

    static void PanelClearChildren(
        Aero::Controls::Panel& panel) noexcept;

    static const Base::Ref<Base::Object>& DecoratorOwnedChild(
        const Aero::Controls::Decorator& decorator) noexcept;

    static Base::Result<void> DecoratorSetOwnedChild(
        Aero::Controls::Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        Aero::UIElement& child) noexcept;

    static void Attach(
        ContentElement& element,
        DependencyObject* logicalParent,
        UIElement* contentHost,
        EventRouter* eventRouter) noexcept;

    static void Detach(ContentElement& element) noexcept;

    static DependencyObject* Parent(
        const ContentElement& element) noexcept;

    static UIElement* ContentHost(
        const ContentElement& element) noexcept;

    static std::uint32_t LogicalChildrenCount(
        const FrameworkContentElement& element) noexcept;

    static DependencyObject* LogicalChild(
        const FrameworkContentElement& element,
        std::uint32_t index) noexcept;

private:
    ::Aero::Media::Visual* owner_ = nullptr;
};

template<>
struct FacetTrait<VisualFacet> {
    static constexpr std::uint32_t Id = static_cast<std::uint32_t>(FacetId::Visual);
    static constexpr FacetType Type = FacetType::Visual;
};

} // namespace Aero::Core
