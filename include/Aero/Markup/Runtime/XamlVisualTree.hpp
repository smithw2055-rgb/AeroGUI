#pragma once

#include <Aero/Markup/Runtime/XamlContentWriter.hpp>

// Compatibility include retained for source trees that previously included
// XamlVisualTree.hpp. Visual content registration now uses XamlContentWriter;
// loading returns XamlLoadResult and Presentation owns mounting.
