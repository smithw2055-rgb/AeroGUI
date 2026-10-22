#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/DependencyObject.hpp>

#include <cstdint>

namespace Aero {

class ElementTree;
class LogicalTreeHelper;
namespace Render { class RenderTree; }

} // namespace Aero

namespace Aero::Media {

class VisualTreeHelper;
class Visual;

struct PointHitTestResult {
    Visual* visualHit = nullptr;
    Base::Point pointHit{};

    constexpr PointHitTestResult() noexcept = default;
    constexpr PointHitTestResult(Visual* visual, Base::Point point) noexcept
        : visualHit(visual), pointHit(point) {}

    constexpr explicit operator bool() const noexcept { return visualHit != nullptr; }
    constexpr bool HasHit() const noexcept { return visualHit != nullptr; }
    Visual* GetVisualHit() const noexcept { return visualHit; }
    Base::Point GetPointHit() const noexcept { return pointHit; }
};

struct PointHitTestParameters {
    Base::Point hitPoint{};

    constexpr explicit PointHitTestParameters(Base::Point point) noexcept
        : hitPoint(point) {}
    constexpr Base::Point HitPoint() const noexcept { return hitPoint; }
};

class AERO_GUI_API Visual : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(Visual, ::Aero::DependencyObject)

public:
    struct FlagRef {
        std::uint8_t* bits = nullptr;
        std::uint8_t mask = 0U;
        FlagRef& operator=(bool value) noexcept {
            if (value) {
                *bits = static_cast<std::uint8_t>(*bits | mask);
            } else {
                *bits = static_cast<std::uint8_t>(*bits & static_cast<std::uint8_t>(~mask));
            }
            return *this;
        }
        operator bool() const noexcept {
            return bits != nullptr && (*bits & mask) != 0U;
        }
    };

    class RenderChildRange {
    public:
        class Iterator {
        public:
            Iterator(const Visual* owner, std::uint32_t index) noexcept
                : owner_(owner), index_(index) {}
            Visual* operator*() const noexcept {
                return owner_ != nullptr ? owner_->GetVisualChild(index_) : nullptr;
            }
            Iterator& operator++() noexcept { ++index_; return *this; }
            bool operator!=(const Iterator& other) const noexcept {
                return owner_ != other.owner_ || index_ != other.index_;
            }

        private:
            const Visual* owner_ = nullptr;
            std::uint32_t index_ = 0U;
        };
        explicit RenderChildRange(const Visual& visual) noexcept
            : owner_(&visual), count_(visual.GetVisualChildrenCount()) {}
        Iterator begin() const noexcept { return Iterator(owner_, 0U); }
        Iterator end() const noexcept { return Iterator(owner_, count_); }
        std::uint32_t Size() const noexcept { return count_; }
        bool Empty() const noexcept { return count_ == 0U; }
        Visual* operator[](std::uint32_t index) const noexcept {
            return owner_ != nullptr ? owner_->GetVisualChild(index) : nullptr;
        }

    private:
        const Visual* owner_ = nullptr;
        std::uint32_t count_ = 0U;
    };

    explicit Visual(::Aero::Meta::TypeId runtimeType) noexcept;
    ~Visual() override;

    bool IsAncestorOf(const Visual& descendant) const noexcept;
    Base::Transform2D TransformToVisual(const Visual& visual) const noexcept;
    bool TryTransformToVisual(const Visual& visual, Base::ProjectiveTransform2D& output) const noexcept;
    Base::Transform2D TransformToRoot() const noexcept;
    Base::Transform2D TransformToAncestor(const Visual* ancestor = nullptr) const noexcept;
    Base::Point PointToScreen(Base::Point point) const noexcept;
    bool TryPointToScreen(Base::Point point, Base::Point& screen) const noexcept;
    Base::Point PointFromScreen(Base::Point point) const noexcept;
    bool TryPointFromScreen(Base::Point point, Base::Point& local) const noexcept;
    Result<Ref<Base::Object>> AcquireLifetime() noexcept;
    void InvalidateRenderDrawing() noexcept;
    void InvalidateRenderState() noexcept;
    RenderChildRange RenderChildren() const noexcept { return RenderChildRange(*this); }

    bool GetIsLoaded() const noexcept { return LoadedFlag(); }
    bool IsRenderAttached() const noexcept { return (visualFlags_ & kFlagRenderAttached) != 0U; }

    Visual* GetVisualParent() const noexcept { return visualParent_; }
    ::Aero::DependencyObject* GetLogicalParent() const noexcept { return logicalParent_; }



    // WPF-friendly hit-test extension point. Default walks visual children;
    // custom Visuals override to participate without going through UIElement.
    virtual PointHitTestResult HitTestCore(PointHitTestParameters hitTestParameters) const noexcept {
        return HitTestCore(hitTestParameters.HitPoint());
    }
    virtual PointHitTestResult HitTestCore(Base::Point point) const noexcept {
        if (HitTestCore()) {
            return PointHitTestResult{const_cast<Visual*>(this), point};
        }
        return PointHitTestResult{};
    }
    virtual bool HitTestCore() const noexcept {
        return false;
    }

protected:
    virtual std::uint32_t GetVisualChildrenCount() const noexcept { return 0U; }
    virtual Visual* GetVisualChild(std::uint32_t) const noexcept { return nullptr; }
    void AddVisualChild(Visual* child) noexcept;
    void RemoveVisualChild(Visual* child) noexcept;
    virtual void OnVisualParentChanged(Visual* oldParent) noexcept { static_cast<void>(oldParent); }
    virtual void OnVisualChildrenChanged(Visual* visualAdded, Visual* visualRemoved) noexcept {
        static_cast<void>(visualAdded);
        static_cast<void>(visualRemoved);
    }

private:
    friend class ::Aero::LogicalTreeHelper;
    friend class ::Aero::ElementTree;
    friend class VisualTreeHelper;
    friend class ::Aero::Render::RenderTree;

    // Internal RenderTree / ElementTree state access
    FlagRef RenderAttached() noexcept { return {&visualFlags_, kFlagRenderAttached}; }
    FlagRef RenderValid() noexcept { return {&visualFlags_, kFlagRenderValid}; }
    FlagRef RenderQueued() noexcept { return {&visualFlags_, kFlagRenderQueued}; }
    FlagRef Rendering() noexcept { return {&visualFlags_, kFlagRendering}; }
    Base::RenderNodeId& NodeId() noexcept { return renderNodeId_; }
    std::uint8_t& RenderDirtyFlags() noexcept { return renderDirtyFlags_; }
    std::uint64_t& RenderRevision() noexcept { return renderRevision_; }

    static constexpr std::uint8_t kFlagRenderAttached = 1U << 0U;
    static constexpr std::uint8_t kFlagRenderValid = 1U << 1U;
    static constexpr std::uint8_t kFlagRenderQueued = 1U << 2U;
    static constexpr std::uint8_t kFlagRendering = 1U << 3U;
    static constexpr std::uint8_t kFlagLoaded = 1U << 4U;
    static constexpr std::uint8_t kFlagIsOverlay = 1U << 5U;
    bool LoadedFlag() const noexcept { return (visualFlags_ & kFlagLoaded) != 0U; }
    bool IsOverlayFlag() const noexcept { return (visualFlags_ & kFlagIsOverlay) != 0U; }

    void SetLoadedFlag(bool loaded) noexcept {
        if (loaded) {
            visualFlags_ = static_cast<std::uint8_t>(visualFlags_ | kFlagLoaded);
        } else { visualFlags_ = static_cast<std::uint8_t>(visualFlags_ & static_cast<std::uint8_t>(~kFlagLoaded)); }
    }

    void SetOverlayFlag(bool isOverlay) noexcept {
        if (isOverlay) {
            visualFlags_ = static_cast<std::uint8_t>(visualFlags_ | kFlagIsOverlay);
        } else {
            visualFlags_ = static_cast<std::uint8_t>(visualFlags_ & static_cast<std::uint8_t>(~kFlagIsOverlay));
        }
    }

    ::Aero::ElementTree* tree_ = nullptr;
    ::Aero::DependencyObject* logicalParent_ = nullptr;
    Visual* visualParent_ = nullptr;
    Visual* firstVisualChild_ = nullptr;
    Visual* nextVisualSibling_ = nullptr;
    Visual* prevVisualSibling_ = nullptr;
    Ref<Base::Object> lifetime_;
    Base::RenderNodeId renderNodeId_ = Base::InvalidRenderNodeId;
    std::uint64_t renderRevision_ = 0U;
    std::uint32_t handleIndex_ = UINT32_MAX;
    std::uint32_t handleGeneration_ = 0U;
    std::uint32_t drawingIndex_ = UINT32_MAX;
    std::uint8_t renderDirtyFlags_ = 0x07U;
    std::uint8_t visualFlags_ = 0U;
};

} // namespace Aero::Media

namespace Aero {
// WPF/Noesis-friendly alias: Visual is conceptually in Aero root even though
// the implementation lives in Aero::Media (mirrors System.Windows.Media.Visual).
using Visual = Media::Visual;
using PointHitTestResult = Media::PointHitTestResult;
using PointHitTestParameters = Media::PointHitTestParameters;
} // namespace Aero
