#pragma once

#include "gui/core/Facet.hpp"
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Visual.hpp>
#include <Aero/FrameworkElement.hpp>

namespace Aero::Media { class DrawingContext; }
namespace Aero::Controls { class Image; }

namespace Aero::Core {

// Render Facet: Visual drawing command emission, render tree linkage, and dirty tracking
class RenderFacet : public Facet {
public:
    static constexpr FacetType StaticType = FacetType::Render;

    explicit RenderFacet(::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}

    ::Aero::Media::Visual& Owner() const noexcept { return *owner_; }

    Base::RenderNodeId& NodeId() noexcept { return owner_->renderNodeId_; }
    const Base::RenderNodeId& NodeId() const noexcept { return owner_->renderNodeId_; }

    bool& RenderAttached() noexcept { return owner_->renderAttached_; }
    bool RenderAttached() const noexcept { return owner_->renderAttached_; }

    bool& RenderValid() noexcept { return owner_->renderValid_; }
    bool RenderValid() const noexcept { return owner_->renderValid_; }

    bool& RenderQueued() noexcept { return owner_->renderQueued_; }
    bool RenderQueued() const noexcept { return owner_->renderQueued_; }

    bool& Rendering() noexcept { return owner_->rendering_; }
    bool Rendering() const noexcept { return owner_->rendering_; }

    std::uint64_t& RenderRevision() noexcept { return owner_->renderRevision_; }
    std::uint64_t RenderRevision() const noexcept { return owner_->renderRevision_; }

    std::uint8_t& RenderDirtyFlags() noexcept { return owner_->renderDirtyFlags_; }
    std::uint8_t RenderDirtyFlags() const noexcept { return owner_->renderDirtyFlags_; }

    void Render(::Aero::Media::DrawingContext& context) noexcept {
        Render(*owner_, context);
    }

    Base::Result<void> InvalidateRenderDrawing() noexcept {
        return InvalidateRenderDrawing(*owner_);
    }
    Base::Result<void> InvalidateRenderState() noexcept {
        return InvalidateRenderState(*owner_);
    }

    static Base::RenderNodeId& NodeId(::Aero::Media::Visual& visual) noexcept {
        return visual.renderNodeId_;
    }
    static bool& RenderAttached(::Aero::Media::Visual& visual) noexcept {
        return visual.renderAttached_;
    }
    static bool& RenderValid(::Aero::Media::Visual& visual) noexcept {
        return visual.renderValid_;
    }
    static bool& RenderQueued(::Aero::Media::Visual& visual) noexcept {
        return visual.renderQueued_;
    }
    static bool& Rendering(::Aero::Media::Visual& visual) noexcept {
        return visual.rendering_;
    }
    static void* RenderRuntime(const ::Aero::Media::Visual& visual) noexcept;
    static ::Aero::Media::Visual* RenderParent(const ::Aero::Media::Visual& visual) noexcept {
        return visual.visualParent_;
    }
    static std::uint8_t& RenderDirtyFlags(::Aero::Media::Visual& visual) noexcept {
        return visual.renderDirtyFlags_;
    }
    static std::uint64_t& RenderRevision(::Aero::Media::Visual& visual) noexcept {
        return visual.renderRevision_;
    }
    static Base::Span<::Aero::Media::Visual* const> RenderChildren(const ::Aero::Media::Visual& visual) noexcept {
        return { visual.visualChildren_.Data(), visual.visualChildren_.Size() };
    }
    static Base::Result<void> InvalidateRenderDrawing(::Aero::Media::Visual& visual) noexcept;
    static Base::Result<void> InvalidateRenderState(::Aero::Media::Visual& visual) noexcept;
    static Base::Result<void> SetImageRuntimeData(
        Aero::Controls::Image& image,
        std::uint64_t renderImage,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight) noexcept;
    static void Render(::Aero::Media::Visual& visual, ::Aero::Media::DrawingContext& context) noexcept {
        FrameworkElement* element = visual.AsFrameworkElement();
        if (element != nullptr) {
            element->OnRender(context);
        }
    }

private:
    ::Aero::Media::Visual* owner_ = nullptr;
};

template<>
struct FacetTrait<RenderFacet> {
    static constexpr std::uint32_t Id = static_cast<std::uint32_t>(FacetId::Render);
    static constexpr FacetType Type = FacetType::Render;
};

} // namespace Aero::Core
