#pragma once

#include <Aero/Documents/Inline.hpp>
#include <Aero/UIElement.hpp>

namespace Aero::Documents {

class AERO_GUI_API InlineUIContainer : public Inline {
    AERO_DECLARE_TYPE(InlineUIContainer, Inline)
public:
    InlineUIContainer() noexcept : Inline(StaticTypeId()) {}

    UIElement* GetChild() const noexcept { return child_.Get(); }
    void SetChild(Ref<UIElement> value) noexcept { child_ = std::move(value); }

    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, Child);

private:
    Ref<UIElement> child_;
};

} // namespace Aero::Documents
