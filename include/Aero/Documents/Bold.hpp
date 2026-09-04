#pragma once

#include <Aero/Documents/Span.hpp>

namespace Aero::Documents {

class AERO_GUI_API Bold : public Span {
    AERO_DECLARE_TYPE(Bold, Span)
public:
    Bold() noexcept : Span(StaticTypeId()) {}
    ~Bold() override = default;
};

} // namespace Aero::Documents
