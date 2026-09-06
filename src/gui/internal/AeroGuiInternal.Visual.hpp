// Included from AeroGuiInternal.hpp inside class AeroGuiInternal.
// Visual / render hot fields.

    static VisualHandle Handle(const ::Aero::Media::Visual& visual) noexcept {
        return {
            AERO_GET_FIELD(visual, Visual_handleIndex),
            AERO_GET_FIELD(visual, Visual_handleGeneration)
        };
    }
    static void SetHandle(
        ::Aero::Media::Visual& visual, VisualHandle handle) noexcept {
        AERO_GET_FIELD(visual, Visual_handleIndex) = handle.index;
        AERO_GET_FIELD(visual, Visual_handleGeneration) = handle.generation;
    }
    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(
        ::Aero::Media::Visual& visual) noexcept {
        return AERO_CALL_METHOD0(visual, Visual_AcquireLifetime);
    }
    static Base::RenderNodeId& NodeId(::Aero::Media::Visual& visual) noexcept {
        return AERO_GET_FIELD(visual, Visual_renderNodeId);
    }
    struct PackedFlagRef {
        std::uint8_t* bits = nullptr;
        std::uint8_t mask = 0U;
        PackedFlagRef& operator=(bool value) noexcept {
            if (value) {
                *bits = static_cast<std::uint8_t>(*bits | mask);
            } else {
                *bits = static_cast<std::uint8_t>(
                    *bits & static_cast<std::uint8_t>(~mask));
            }
            return *this;
        }
        operator bool() const noexcept {
            return (*bits & mask) != 0U;
        }
    };
    static PackedFlagRef RenderAttached(::Aero::Media::Visual& visual) noexcept {
        return {&AERO_GET_FIELD(visual, Visual_visualFlags), 1U << 0U};
    }
    static PackedFlagRef RenderValid(::Aero::Media::Visual& visual) noexcept {
        return {&AERO_GET_FIELD(visual, Visual_visualFlags), 1U << 1U};
    }
    static PackedFlagRef RenderQueued(::Aero::Media::Visual& visual) noexcept {
        return {&AERO_GET_FIELD(visual, Visual_visualFlags), 1U << 2U};
    }
    static PackedFlagRef Rendering(::Aero::Media::Visual& visual) noexcept {
        return {&AERO_GET_FIELD(visual, Visual_visualFlags), 1U << 3U};
    }
    static std::uint8_t& RenderDirtyFlags(::Aero::Media::Visual& visual) noexcept {
        return AERO_GET_FIELD(visual, Visual_renderDirtyFlags);
    }
    static std::uint64_t& RenderRevision(::Aero::Media::Visual& visual) noexcept {
        return AERO_GET_FIELD(visual, Visual_renderRevision);
    }
    static ::Aero::Media::Visual* RenderParent(
        const ::Aero::Media::Visual& visual) noexcept {
        return AERO_GET_FIELD(visual, Visual_visualParent);
    }
    class RenderChildRange {
    public:
        class Iterator {
        public:
            Iterator(const ::Aero::Media::Visual* owner, std::uint32_t index) noexcept
                : owner_(owner), index_(index) {}
            ::Aero::Media::Visual* operator*() const noexcept {
                return owner_ != nullptr
                    ? AERO_CALL_METHOD(*owner_, Visual_GetVisualChild, index_)
                    : nullptr;
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
            : owner_(&visual), count_(AERO_CALL_METHOD0(visual, Visual_GetVisualChildrenCount)) {}
        Iterator begin() const noexcept { return Iterator(owner_, 0U); }
        Iterator end() const noexcept { return Iterator(owner_, count_); }
        std::uint32_t Size() const noexcept { return count_; }
        bool Empty() const noexcept { return count_ == 0U; }
        ::Aero::Media::Visual* operator[](std::uint32_t index) const noexcept {
            return owner_ != nullptr
                ? AERO_CALL_METHOD(*owner_, Visual_GetVisualChild, index)
                : nullptr;
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
    static ElementTree* VisualTree(const ::Aero::Media::Visual& visual) noexcept {
        return AERO_GET_FIELD(visual, Visual_tree);
    }
    static ElementTree* VisualTree(const ::Aero::Media::Visual* visual) noexcept {
        return visual != nullptr ? AERO_GET_FIELD(*visual, Visual_tree) : nullptr;
    }
    static void* RenderRuntime(const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = AERO_GET_FIELD(visual, Visual_tree);
        return tree != nullptr &&
            AERO_GET_FIELD(visual, Visual_renderNodeId) != Base::InvalidRenderNodeId
            ? static_cast<void*>(tree->RenderTree())
            : nullptr;
    }
    static void Render(
        ::Aero::Media::Visual& visual,
        ::Aero::Media::DrawingContext& context) noexcept {
        FrameworkElement* element =
            ::Aero::TryCast<FrameworkElement>(&visual);
        if (element != nullptr) {
            AERO_CALL_METHOD(*element, FE_OnRender, context);
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
