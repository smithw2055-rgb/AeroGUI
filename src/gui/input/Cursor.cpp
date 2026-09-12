#include <Aero/Input/Cursor.hpp>

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

} // namespace Aero::Input
