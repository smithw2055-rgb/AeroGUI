#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>

namespace Aero::Interactivity {

// Marker object used by the interactivity metadata layer. It remains a
// concrete public type so behavior XAML can register the same owner type as
// the original Gallery model.
class AERO_GUI_API Interaction : public Base::Object {
    AERO_DECLARE_TYPE(Interaction, Base::Object)
private:
    Interaction() noexcept = default;
};

} // namespace Aero::Interactivity
