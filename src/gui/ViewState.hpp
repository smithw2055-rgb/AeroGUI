#pragma once

namespace Aero {

// The complete definition is kept beside View's composition-root code in
// View.cpp. This source-only declaration gives other implementation units a
// stable vocabulary without exposing View state through the installed SDK.
struct ViewState;
class ViewRenderer;

} // namespace Aero
