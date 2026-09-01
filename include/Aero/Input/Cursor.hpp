#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>
#include <cstdint>

namespace Aero::Input {

// Specifies the built-in cursor types. Mirrors the reference AeroUI
// Aero::CursorType enumeration.
enum class CursorType : std::int32_t {
    // No cursor should be displayed.
    None = 0,
    // A "no" cursor (operation not allowed).
    No,
    // A standard arrow cursor.
    Arrow,
    // A standard arrow with a small hourglass.
    AppStarting,
    // A crosshair cursor.
    Cross,
    // A help cursor (arrow + question mark).
    Help,
    // A text I-Beam cursor.
    IBeam,
    // Four joined arrows pointing north, south, east, west.
    SizeAll,
    // Two-headed northeast/southwest sizing cursor.
    SizeNESW,
    // Two-headed north/south sizing cursor.
    SizeNS,
    // Two-headed northwest/southeast sizing cursor.
    SizeNWSE,
    // Two-headed west/east sizing cursor.
    SizeWE,
    // A vertical arrow cursor.
    UpArrow,
    // An hourglass (wait) cursor.
    Wait,
    // A hand cursor.
    Hand,
    // A pen cursor.
    Pen,
    // Scrolling cursor with arrows north/south.
    ScrollNS,
    // Scrolling cursor with arrows west/east.
    ScrollWE,
    // Scrolling cursor with arrows north/south/east/west.
    ScrollAll,
    // Scrolling cursor, arrow pointing north.
    ScrollN,
    // Scrolling cursor, arrow pointing south.
    ScrollS,
    // Scrolling cursor, arrow pointing west.
    ScrollW,
    // Scrolling cursor, arrow pointing east.
    ScrollE,
    // Scrolling cursor, arrows north/west.
    ScrollNW,
    // Scrolling cursor, arrows north/east.
    ScrollNE,
    // Scrolling cursor, arrows south/west.
    ScrollSW,
    // Scrolling cursor, arrows south/east.
    ScrollSE,
    // Arrow with a compact disk.
    ArrowCD,
    // A cursor loaded from a custom .cur/.ani file.
    Custom,
    Count
};

// Represents the image used for the mouse pointer. Built-in cursors are
// identified by a CursorType; custom cursors carry a file name. OS pointer
// wiring is not performed by this type.
class AERO_GUI_API Cursor : public Base::Object {
    AERO_DECLARE_TYPE(Cursor, Base::Object)
public:
    explicit Cursor(CursorType type) noexcept;
    explicit Cursor(const String& filename) noexcept;
    ~Cursor() override = default;

    // Returns the cursor kind.
    CursorType Type() const noexcept { return type_; }

    // For custom cursors returns the file name; empty for built-in cursors.
    StringView Filename() const noexcept { return filename_.View(); }

    // Returns the standard cursor name (for example "Hand") or the custom
    // file path for custom cursors.
    String ToString() const;

    // Returns the standard WPF-style name for built-in cursors.
    StringView Name() const noexcept;

    static Base::Ref<Cursor> Create(CursorType type) noexcept;
    static Base::Ref<Cursor> Create(const String& filename) noexcept;

private:
    CursorType type_ = CursorType::Arrow;
    String filename_;
};

} // namespace Aero::Input

AERO_DECLARE_TYPE_ENUM(Aero::Input::CursorType)
