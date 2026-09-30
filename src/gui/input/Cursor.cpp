#include <Aero/Input/Cursor.hpp>
#include "gui/core/Describe.hpp"

namespace Aero::Input {

namespace {

constexpr Base::StringView CursorTypeName(CursorType type) noexcept {
    switch (type) {
    case CursorType::None: return Base::StringView("None");
    case CursorType::No: return Base::StringView("No");
    case CursorType::Arrow: return Base::StringView("Arrow");
    case CursorType::AppStarting: return Base::StringView("AppStarting");
    case CursorType::Cross: return Base::StringView("Cross");
    case CursorType::Help: return Base::StringView("Help");
    case CursorType::IBeam: return Base::StringView("IBeam");
    case CursorType::SizeAll: return Base::StringView("SizeAll");
    case CursorType::SizeNESW: return Base::StringView("SizeNESW");
    case CursorType::SizeNS: return Base::StringView("SizeNS");
    case CursorType::SizeNWSE: return Base::StringView("SizeNWSE");
    case CursorType::SizeWE: return Base::StringView("SizeWE");
    case CursorType::UpArrow: return Base::StringView("UpArrow");
    case CursorType::Wait: return Base::StringView("Wait");
    case CursorType::Hand: return Base::StringView("Hand");
    case CursorType::Pen: return Base::StringView("Pen");
    case CursorType::ScrollNS: return Base::StringView("ScrollNS");
    case CursorType::ScrollWE: return Base::StringView("ScrollWE");
    case CursorType::ScrollAll: return Base::StringView("ScrollAll");
    case CursorType::ScrollN: return Base::StringView("ScrollN");
    case CursorType::ScrollS: return Base::StringView("ScrollS");
    case CursorType::ScrollW: return Base::StringView("ScrollW");
    case CursorType::ScrollE: return Base::StringView("ScrollE");
    case CursorType::ScrollNW: return Base::StringView("ScrollNW");
    case CursorType::ScrollNE: return Base::StringView("ScrollNE");
    case CursorType::ScrollSW: return Base::StringView("ScrollSW");
    case CursorType::ScrollSE: return Base::StringView("ScrollSE");
    case CursorType::ArrowCD: return Base::StringView("ArrowCD");
    case CursorType::Custom: return Base::StringView("Custom");
    case CursorType::Count: return Base::StringView("Arrow");
    }
    return Base::StringView("Arrow");
}

} // namespace

Cursor::Cursor(CursorType type) noexcept
    : type_(type) {}

Cursor::Cursor(const String& filename) noexcept
    : type_(CursorType::Custom),
      filename_(filename) {}

String Cursor::ToString() const {
    if (type_ == CursorType::Custom) {
        return filename_;
    }
    String result;
    result.Assign(CursorTypeName(type_));
    return result;
}

StringView Cursor::Name() const noexcept {
    if (type_ == CursorType::Custom) {
        return filename_.View();
    }
    return CursorTypeName(type_);
}

Base::Ref<Cursor> Cursor::Create(CursorType type) noexcept {
    return Base::MakeRef<Cursor>(type).Value();
}

Base::Ref<Cursor> Cursor::Create(const String& filename) noexcept {
    return Base::MakeRef<Cursor>(filename).Value();
}

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

// Metadata registration for the types implemented in this file.
namespace Aero::Input {

AERO_DESCRIBE(Cursor) {
    using namespace Aero::Meta;
    Register<Cursor>(context);
}

} // namespace Aero::Input

