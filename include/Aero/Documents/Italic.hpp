#pragma once

#include <Aero/Documents/Span.hpp>

namespace Aero::Documents {

class AERO_GUI_API Italic : public Span {
    AERO_DECLARE_TYPE(Italic, Span)
public:
    Italic() noexcept : Span(StaticTypeId()) {}
    ~Italic() override = default;
};

} // namespace Aero::Documents
