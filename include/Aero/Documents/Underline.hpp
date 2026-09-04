#pragma once

#include <Aero/Documents/Span.hpp>

namespace Aero::Documents {

class AERO_GUI_API Underline : public Span {
    AERO_DECLARE_TYPE(Underline, Span)
public:
    Underline() noexcept : Span(StaticTypeId()) {}
    ~Underline() override = default;
};

} // namespace Aero::Documents
