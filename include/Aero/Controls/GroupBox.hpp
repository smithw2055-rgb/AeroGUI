#pragma once

#include <Aero/Controls/HeaderedContentControl.hpp>

namespace Aero::Controls {

class AERO_GUI_API GroupBox
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        GroupBox,
        HeaderedContentControl)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    GroupBox() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~GroupBox() override = default;
};

} // namespace Aero::Controls
