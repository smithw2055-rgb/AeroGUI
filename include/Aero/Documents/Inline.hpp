#pragma once

#include <Aero/Documents/TextElement.hpp>

namespace Aero::Documents {

class AERO_GUI_API Inline : public TextElement {
    AERO_DECLARE_TYPE(Inline, TextElement)
public:
    ~Inline() override = default;

protected:
    explicit Inline(Meta::TypeId runtimeType) noexcept
        : TextElement(runtimeType) {}
};

} // namespace Aero::Documents
