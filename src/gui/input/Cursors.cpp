#include <Aero/Input/Cursors.hpp>

namespace Aero::Input {

Base::Ref<Cursor> Cursors::AppStarting() {
    return Cursor::Create(CursorType::AppStarting);
}
Base::Ref<Cursor> Cursors::Arrow() {
    return Cursor::Create(CursorType::Arrow);
}
Base::Ref<Cursor> Cursors::ArrowCD() {
    return Cursor::Create(CursorType::ArrowCD);
}
Base::Ref<Cursor> Cursors::Cross() {
    return Cursor::Create(CursorType::Cross);
}
Base::Ref<Cursor> Cursors::Hand() {
    return Cursor::Create(CursorType::Hand);
}
Base::Ref<Cursor> Cursors::Help() {
    return Cursor::Create(CursorType::Help);
}
Base::Ref<Cursor> Cursors::IBeam() {
    return Cursor::Create(CursorType::IBeam);
}
Base::Ref<Cursor> Cursors::No() {
    return Cursor::Create(CursorType::No);
}
Base::Ref<Cursor> Cursors::None() {
    return Cursor::Create(CursorType::None);
}
Base::Ref<Cursor> Cursors::Pen() {
    return Cursor::Create(CursorType::Pen);
}
Base::Ref<Cursor> Cursors::ScrollAll() {
    return Cursor::Create(CursorType::ScrollAll);
}
Base::Ref<Cursor> Cursors::ScrollE() {
    return Cursor::Create(CursorType::ScrollE);
}
Base::Ref<Cursor> Cursors::ScrollN() {
    return Cursor::Create(CursorType::ScrollN);
}
Base::Ref<Cursor> Cursors::ScrollNE() {
    return Cursor::Create(CursorType::ScrollNE);
}
Base::Ref<Cursor> Cursors::ScrollNS() {
    return Cursor::Create(CursorType::ScrollNS);
}
Base::Ref<Cursor> Cursors::ScrollNW() {
    return Cursor::Create(CursorType::ScrollNW);
}
Base::Ref<Cursor> Cursors::ScrollS() {
    return Cursor::Create(CursorType::ScrollS);
}
Base::Ref<Cursor> Cursors::ScrollSE() {
    return Cursor::Create(CursorType::ScrollSE);
}
Base::Ref<Cursor> Cursors::ScrollSW() {
    return Cursor::Create(CursorType::ScrollSW);
}
Base::Ref<Cursor> Cursors::ScrollW() {
    return Cursor::Create(CursorType::ScrollW);
}
Base::Ref<Cursor> Cursors::ScrollWE() {
    return Cursor::Create(CursorType::ScrollWE);
}
Base::Ref<Cursor> Cursors::SizeAll() {
    return Cursor::Create(CursorType::SizeAll);
}
Base::Ref<Cursor> Cursors::SizeNESW() {
    return Cursor::Create(CursorType::SizeNESW);
}
Base::Ref<Cursor> Cursors::SizeNS() {
    return Cursor::Create(CursorType::SizeNS);
}
Base::Ref<Cursor> Cursors::SizeNWSE() {
    return Cursor::Create(CursorType::SizeNWSE);
}
Base::Ref<Cursor> Cursors::SizeWE() {
    return Cursor::Create(CursorType::SizeWE);
}
Base::Ref<Cursor> Cursors::UpArrow() {
    return Cursor::Create(CursorType::UpArrow);
}
Base::Ref<Cursor> Cursors::Wait() {
    return Cursor::Create(CursorType::Wait);
}

} // namespace Aero::Input
