#pragma once

#include <Aero/Documents/Inline.hpp>

namespace Aero::Documents {

class AERO_GUI_API LineBreak : public Inline {
    AERO_DECLARE_TYPE(LineBreak, Inline)
public:
    LineBreak() noexcept : Inline(StaticTypeId()) {}
    ~LineBreak() override = default;
};

} // namespace Aero::Documents
