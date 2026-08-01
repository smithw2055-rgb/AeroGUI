#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyObject.hpp>

#include <cstdint>

namespace Aero::Detail {
class ElementPrivate;
}

namespace Aero {

class FrameworkElement;
class ElementTree;
class UIElement;
class Visual;

class AERO_API VisualTreeHelper final {
public:
    static Visual* GetParent(const Visual& visual) noexcept;
    static std::uint32_t GetChildrenCount(const Visual& visual) noexcept;
    static Visual* GetChild(const Visual& visual, std::uint32_t index) noexcept;
};

class AERO_API LogicalTreeHelper final {
public:
    static DependencyObject* GetParent(const DependencyObject& object) noexcept;
    static std::uint32_t GetChildrenCount(const DependencyObject& object) noexcept;
    static DependencyObject* GetChild(const DependencyObject& object, std::uint32_t index) noexcept;
    static Visual* GetParent(const Visual& visual) noexcept;
    static std::uint32_t GetChildrenCount(const Visual& visual) noexcept;
    static Visual* GetChild(const Visual& visual, std::uint32_t index) noexcept;
};

class AERO_API Visual : public DependencyObject {
    AERO_DECLARE_TYPE(Visual, DependencyObject)
public:
    explicit Visual(Core::TypeId runtimeType) noexcept;
    ~Visual() override;

    Visual* GetVisualParent() const noexcept { return visualParent_; }
    Visual* GetLogicalParent() const noexcept { return logicalParent_; }
    bool GetIsLoaded() const noexcept { return loaded_; }

    virtual UIElement* AsUIElement() noexcept { return nullptr; }
    virtual const UIElement* AsUIElement() const noexcept { return nullptr; }
    virtual FrameworkElement* AsFrameworkElement() noexcept { return nullptr; }
    virtual const FrameworkElement* AsFrameworkElement() const noexcept { return nullptr; }

protected:
    virtual std::uint32_t GetVisualChildrenCountCore() const noexcept { return visualChildren_.Size(); }
    virtual Visual* GetVisualChildCore(std::uint32_t index) const noexcept { return index < visualChildren_.Size() ? visualChildren_[index] : nullptr; }

private:
    friend class LogicalTreeHelper;
    friend class ElementTree;
    friend class VisualTreeHelper;
    friend class Aero::Detail::ElementPrivate;

    Base::Result<Base::Ref<Base::Object>> AcquireLifetime() noexcept;

    ElementTree* tree_ = nullptr;
    Visual* logicalParent_ = nullptr;
    Visual* visualParent_ = nullptr;
    Base::Vector<Visual*> logicalChildren_;
    Base::Vector<Visual*> visualChildren_;
    Base::Ref<Base::Object> lifetime_;
    std::uint32_t handleIndex_ = UINT32_MAX;
    std::uint32_t handleGeneration_ = 0U;
    bool loaded_ = false;
};

} // namespace Aero
