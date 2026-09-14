#pragma once

#include <Aero/Base/Config.hpp>

#include <cstdint>

namespace Aero::Media {

class Visual;

class AERO_GUI_API VisualTreeHelper {
public:
    static Visual* GetParent(const Visual& visual) noexcept;
    static std::uint32_t GetChildrenCount(const Visual& visual) noexcept;
    static Visual* GetChild(const Visual& visual, std::uint32_t index) noexcept;
};

} // namespace Aero::Media
