// Compile the standard object writer through the activation-aware schema seam.
// This keeps the established writer implementation and transaction semantics
// while allowing LoadXamlWithActivation() to supply host construction services.
#include <Aero/Markup/XamlObjectWriter.hpp>

#define CreateObject CreateObjectActivated
#include "XamlObjectWriter.cpp"
#undef CreateObject
