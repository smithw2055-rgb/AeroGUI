#pragma once

#include <Aero/Documents/Inline.hpp>

namespace Aero::Documents {

class AERO_GUI_API Run : public Inline {
    AERO_DECLARE_TYPE(Run, Inline)
public:
    Run() noexcept : Inline(StaticTypeId()) {}
    ~Run() override = default;

    StringView GetText() const noexcept {
        return GetValueOr(TextProperty, StringView{});
    }
    StringView GetContent() const noexcept { return GetText(); }
    void SetText(StringView value) noexcept {
        SetValue(TextProperty, value);
    }
    void SetContent(StringView value) noexcept {
        SetText(value);
    }

    inline static constexpr DependencyProperty<String> TextProperty{"Text"};
};

} // namespace Aero::Documents
