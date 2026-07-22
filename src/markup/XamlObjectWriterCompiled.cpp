// Compile the standard object writer through the activation-aware schema seam.
// This keeps the established writer implementation and transaction semantics
// while allowing LoadXamlWithActivation() to supply host construction services.
#include <Aero/Markup/XamlObjectWriter.hpp>

// Keep the activation-aware writer on the same member-provider dispatch path
// as the direct object writer implementation.
#define CreateObject CreateObjectActivated
#include "XamlObjectWriter.cpp"
#undef CreateObject
