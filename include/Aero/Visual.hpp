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
class AeroGuiInternal;

} // namespace Aero

namespace Aero::Media {

class VisualTreeHelper;

class AERO_GUI_API Visual : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(Visual, ::Aero::DependencyObject)
public:

    explicit Visual(::Aero::Meta::TypeId runtimeType) noexcept;
    ~Visual() override;

    Visual* GetVisualParent() const noexcept { return visualParent_; }
    ::Aero::DependencyObject* GetLogicalParent() const noexcept { return logicalParent_; }
    bool GetIsLoaded() const noexcept { return loaded_; }

    bool IsAncestorOf(const Visual& descendant) const noexcept;
    Base::Transform2D TransformToVisual(const Visual& visual) const noexcept;
    Base::Point PointToScreen(Base::Point point) const noexcept;
    Base::Point PointFromScreen(Base::Point point) const noexcept;

#if defined(AERO_GUI_IMPLEMENTATION)
    ::Aero::ElementTree* GetTree() const noexcept { return tree_; }
#endif

protected:
    virtual std::uint32_t GetVisualChildrenCount() const noexcept { return 0U; }
    virtual Visual* GetVisualChild(std::uint32_t) const noexcept { return nullptr; }

    void AddVisualChild(Visual* child) noexcept;
    void RemoveVisualChild(Visual* child) noexcept;

    virtual void OnVisualParentChanged(Visual* oldParent) noexcept {
        static_cast<void>(oldParent);
    }
    virtual void OnVisualChildrenChanged(
        Visual* visualAdded,
        Visual* visualRemoved) noexcept {
        static_cast<void>(visualAdded);
        static_cast<void>(visualRemoved);
    }

private:
    friend class ::Aero::LogicalTreeHelper;
    friend class ::Aero::ElementTree;
    friend class VisualTreeHelper;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
    Result<Ref<Base::Object>> AcquireLifetime() noexcept;

    ::Aero::ElementTree* tree_ = nullptr;
    ::Aero::DependencyObject* logicalParent_ = nullptr;
    Visual* visualParent_ = nullptr;
    Ref<Base::Object> lifetime_;
    Base::RenderNodeId renderNodeId_ =
        Base::InvalidRenderNodeId;
    std::uint64_t renderRevision_ = 0U;
    std::uint32_t handleIndex_ = UINT32_MAX;
    std::uint32_t handleGeneration_ = 0U;
    std::uint8_t renderDirtyFlags_ = 0x07U;
    bool renderAttached_ = false;
    bool renderValid_ = false;
    bool renderQueued_ = false;
    bool rendering_ = false;
    bool loaded_ = false;
};

} // namespace Aero::Media
