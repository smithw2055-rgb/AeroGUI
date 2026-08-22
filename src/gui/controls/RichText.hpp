#pragma once

namespace Aero {

class DependencyObject;

// Private rich-text formatting helper shared by Controls.cpp (TextBlock layout)
// and RichText.cpp (rich-text token parsing and change handling).
void ApplyRichText(DependencyObject& object) noexcept;

} // namespace Aero
