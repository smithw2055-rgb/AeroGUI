#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Input/Cursor.hpp>

namespace Aero::Input {

// Defines a set of default cursors, mirroring AeroUI's Aero::Cursors static
// class. Each accessor returns a shared, immutable Cursor instance.
struct AERO_GUI_API Cursors {
    static Base::Ref<Cursor> AppStarting();
    static Base::Ref<Cursor> Arrow();
    static Base::Ref<Cursor> ArrowCD();
    static Base::Ref<Cursor> Cross();
    static Base::Ref<Cursor> Hand();
    static Base::Ref<Cursor> Help();
    static Base::Ref<Cursor> IBeam();
    static Base::Ref<Cursor> No();
    static Base::Ref<Cursor> None();
    static Base::Ref<Cursor> Pen();
    static Base::Ref<Cursor> ScrollAll();
    static Base::Ref<Cursor> ScrollE();
    static Base::Ref<Cursor> ScrollN();
    static Base::Ref<Cursor> ScrollNE();
    static Base::Ref<Cursor> ScrollNS();
    static Base::Ref<Cursor> ScrollNW();
    static Base::Ref<Cursor> ScrollS();
    static Base::Ref<Cursor> ScrollSE();
    static Base::Ref<Cursor> ScrollSW();
    static Base::Ref<Cursor> ScrollW();
    static Base::Ref<Cursor> ScrollWE();
    static Base::Ref<Cursor> SizeAll();
    static Base::Ref<Cursor> SizeNESW();
    static Base::Ref<Cursor> SizeNS();
    static Base::Ref<Cursor> SizeNWSE();
    static Base::Ref<Cursor> SizeWE();
    static Base::Ref<Cursor> UpArrow();
    static Base::Ref<Cursor> Wait();
};

} // namespace Aero::Input
