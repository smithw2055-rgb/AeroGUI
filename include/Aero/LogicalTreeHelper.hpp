#pragma once

#include <Aero/Base/Config.hpp>

#include <cstdint>

namespace Aero {

class DependencyObject;

class AERO_GUI_API LogicalTreeHelper {
public:
    static DependencyObject* GetParent(const DependencyObject& object) noexcept;
    static std::uint32_t GetChildrenCount(const DependencyObject& object) noexcept;
    static DependencyObject* GetChild(const DependencyObject& object, std::uint32_t index) noexcept;
};

} // namespace Aero
