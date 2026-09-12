#pragma once

#include <Aero/Controls/HeaderedContentControl.hpp>

namespace Aero::Controls {

class AERO_GUI_API TabItem
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        TabItem,
        HeaderedContentControl)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    TabItem() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~TabItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;
    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);
};

} // namespace Aero::Controls
