#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Input/Cursor.hpp>
#include "gui/input/InputState.hpp"

namespace Aero::Input::DeviceState {

// Tracks the most recent input context so the static Mouse/Keyboard device
// classes can answer queries without a caller-supplied element. The View's
// input dispatch updates these on every pointer/keyboard event.
InputRouter* ActiveRouter() noexcept;
void SetActiveRouter(InputRouter* router) noexcept;

Base::Point LastPointerPosition() noexcept;
void SetLastPointerPosition(const Base::Point& position) noexcept;

std::uint32_t LastModifiers() noexcept;
void SetLastModifiers(std::uint32_t modifiers) noexcept;

Base::Ref<Cursor> OverrideCursor() noexcept;
void SetOverrideCursor(const Base::Ref<Cursor>& cursor) noexcept;
void ClearOverrideCursor() noexcept;

} // namespace Aero::Input::DeviceState
