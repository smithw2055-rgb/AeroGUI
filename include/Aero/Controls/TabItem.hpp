#pragma once

#include <Aero/Controls/HeaderedContentControl.hpp>

namespace Aero::Controls {

class AERO_GUI_API TabItem
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        TabItem,
        HeaderedContentControl)
public:
    TabItem() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~TabItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(
        bool value) noexcept;
    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
};

} // namespace Aero::Controls
