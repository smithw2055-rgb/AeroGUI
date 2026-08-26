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
    class RenderChildRange {
    public:
        class Iterator {
        public:
            Iterator(const ::Aero::Media::Visual* owner, std::uint32_t index) noexcept
                : owner_(owner), index_(index) {}
            ::Aero::Media::Visual* operator*() const noexcept {
                return owner_ != nullptr ? owner_->GetVisualChild(index_) : nullptr;
            }
            Iterator& operator++() noexcept {
                ++index_;
                return *this;
            }
            bool operator!=(const Iterator& other) const noexcept {
                return owner_ != other.owner_ || index_ != other.index_;
            }
        private:
            const ::Aero::Media::Visual* owner_ = nullptr;
            std::uint32_t index_ = 0U;
        };

        explicit RenderChildRange(const ::Aero::Media::Visual& visual) noexcept
            : owner_(&visual), count_(visual.GetVisualChildrenCount()) {}
        Iterator begin() const noexcept { return Iterator(owner_, 0U); }
        Iterator end() const noexcept { return Iterator(owner_, count_); }
        std::uint32_t Size() const noexcept { return count_; }
        bool Empty() const noexcept { return count_ == 0U; }
        ::Aero::Media::Visual* operator[](std::uint32_t index) const noexcept {
            return owner_ != nullptr ? owner_->GetVisualChild(index) : nullptr;
        }
    private:
        const ::Aero::Media::Visual* owner_ = nullptr;
        std::uint32_t count_ = 0U;
    };
    static RenderChildRange RenderChildren(
        const ::Aero::Media::Visual& visual) noexcept {
        return RenderChildRange(visual);
    }
    static ::Aero::Media::Visual* AsVisual(::Aero::DependencyObject* object) noexcept {
        return ::Aero::TryCast<::Aero::Media::Visual>(object);
    }
    static const ::Aero::Media::Visual* AsVisual(
        const ::Aero::DependencyObject* object) noexcept {
        return ::Aero::TryCast<::Aero::Media::Visual>(object);
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
        FrameworkElement* element =
            ::Aero::TryCast<FrameworkElement>(&visual);
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
