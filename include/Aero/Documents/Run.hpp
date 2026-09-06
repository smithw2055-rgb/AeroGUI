#pragma once

#include <Aero/Documents/Inline.hpp>

namespace Aero::Documents {

class AERO_GUI_API Run : public Inline {
    AERO_DECLARE_TYPE(Run, Inline)
public:
    Run() noexcept : Inline(StaticTypeId()) {}
    ~Run() override = default;

    StringView GetText() const noexcept {
        return GetValue(TextProperty);
    }
    StringView GetContent() const noexcept { return GetText(); }
    void SetText(StringView value) noexcept {
        SetValue(TextProperty, value);
    }
    void SetContent(StringView value) noexcept {
        SetText(value);
    }

    AERO_DEPENDENCY_PROPERTY(String, Text);
};

} // namespace Aero::Documents
