// Included from AeroGuiInternal.hpp inside class AeroGuiInternal.
// Visual / render hot fields.

    static VisualHandle Handle(const ::Aero::Media::Visual& visual) noexcept {
        return {visual.handleIndex_, visual.handleGeneration_};
    }
    static void SetHandle(
        ::Aero::Media::Visual& visual, VisualHandle handle) noexcept {
        visual.handleIndex_ = handle.index;
        visual.handleGeneration_ = handle.generation;
    }
    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.AcquireLifetime();
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
    static std::uint8_t& RenderDirtyFlags(::Aero::Media::Visual& visual) noexcept {
        return visual.renderDirtyFlags_;
    }
    static std::uint64_t& RenderRevision(::Aero::Media::Visual& visual) noexcept {
        return visual.renderRevision_;
    }
    static ::Aero::Media::Visual* RenderParent(
        const ::Aero::Media::Visual& visual) noexcept {
        return visual.visualParent_;
    }
    static Base::Span<::Aero::Media::Visual* const> RenderChildren(
        const ::Aero::Media::Visual& visual) noexcept {
        return {visual.visualChildren_.Data(), visual.visualChildren_.Size()};
    }
    static void* RenderRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return visual.tree_ != nullptr &&
            visual.renderNodeId_ != Base::InvalidRenderNodeId
            ? static_cast<void*>(visual.tree_->Renderer())
            : nullptr;
    }
    static void Render(
        ::Aero::Media::Visual& visual,
        ::Aero::Media::DrawingContext& context) noexcept {
        FrameworkElement* element = visual.AsFrameworkElement();
        if (element != nullptr) {
            element->OnRender(context);
        }
    }
    static Base::Result<void> InvalidateRenderDrawing(
        ::Aero::Media::Visual& visual) noexcept;
    static Base::Result<void> InvalidateRenderState(
        ::Aero::Media::Visual& visual) noexcept;
    static Base::Result<void> SetImageRuntimeData(
        Controls::Image& image,
        std::uint64_t renderImage,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight) noexcept;
