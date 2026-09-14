#pragma once

// Per-class builtin metadata Fill functions for the core element spine.
// Each Fill owns one class registration and lives next to its implementation
// (Visual.cpp, ContentElement.cpp, ...); this header is the ordered dispatch
// contract consumed by BuiltinMetadata.cpp (via meta/Elements.inl).
// Registration order is frozen: Visual, ContentElement,
// FrameworkContentElement, UIElement, FrameworkElement.

#include <Aero/Base/Result.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Meta {

Base::Result<void> FillVisualMetadata(
    Registration& context) noexcept;
Base::Result<void> FillContentElementMetadata(
    Registration& context) noexcept;
Base::Result<void> FillFrameworkContentElementMetadata(
    Registration& context) noexcept;
Base::Result<void> FillUIElementMetadata(
    Registration& context) noexcept;
Base::Result<void> FillFrameworkElementMetadata(
    Registration& context) noexcept;

} // namespace Aero::Meta
