#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/DependencyObject.hpp>

#include <cstdint>

namespace Aero {

class FrameworkElement;
class ElementTree;
class UIElement;

} // namespace Aero

namespace Aero::Media {

class Visual;

class AERO_GUI_API VisualTreeHelper {
public:
    static Visual* GetParent(const Visual& visual) noexcept;
    static std::uint32_t GetChildrenCount(const Visual& visual) noexcept;
    static Visual* GetChild(const Visual& visual, std::uint32_t index) noexcept;
};

} // namespace Aero::Media

namespace Aero {

class AERO_GUI_API LogicalTreeHelper {
public:
    static DependencyObject* GetParent(const DependencyObject& object) noexcept;
    static std::uint32_t GetChildrenCount(const DependencyObject& object) noexcept;
    static DependencyObject* GetChild(const DependencyObject& object, std::uint32_t index) noexcept;
    static Media::Visual* GetParent(const Media::Visual& visual) noexcept;
    static std::uint32_t GetChildrenCount(const Media::Visual& visual) noexcept;
    static Media::Visual* GetChild(const Media::Visual& visual, std::uint32_t index) noexcept;
};

} // namespace Aero

namespace Aero {
class AeroGuiInternal;
}

namespace Aero::Media {

class AERO_GUI_API Visual : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(Visual, ::Aero::DependencyObject)
public:

    explicit Visual(::Aero::Meta::TypeId runtimeType) noexcept;
    ~Visual() override;

    Visual* GetVisualParent() const noexcept { return visualParent_; }
    Visual* GetLogicalParent() const noexcept { return logicalParent_; }
    bool GetIsLoaded() const noexcept { return loaded_; }

    ::Aero::ElementTree* GetTree() const noexcept { return tree_; }

    virtual Base::Span<Visual* const> GetVisualChildren() const noexcept {
        return { visualChildren_.Data(), visualChildren_.Size() };
    }
    Base::Span<Visual* const> GetLogicalChildren() const noexcept {
        return { logicalChildren_.Data(), logicalChildren_.Size() };
    }

    virtual ::Aero::UIElement* AsUIElement() noexcept { return nullptr; }
    virtual const ::Aero::UIElement* AsUIElement() const noexcept { return nullptr; }
    virtual ::Aero::FrameworkElement* AsFrameworkElement() noexcept { return nullptr; }
    virtual const ::Aero::FrameworkElement* AsFrameworkElement() const noexcept { return nullptr; }

protected:
    virtual std::uint32_t GetVisualChildrenCount() const noexcept { return visualChildren_.Size(); }
    virtual Visual* GetVisualChild(std::uint32_t index) const noexcept { return index < visualChildren_.Size() ? visualChildren_[index] : nullptr; }

    virtual void OnVisualParentChanged(Visual* oldParent) noexcept {
        static_cast<void>(oldParent);
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
    Visual* logicalParent_ = nullptr;
    Visual* visualParent_ = nullptr;
    Base::Vector<Visual*> logicalChildren_;
    Base::Vector<Visual*> visualChildren_;
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
