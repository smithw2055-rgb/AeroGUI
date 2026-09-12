#include "gui/markup/MarkupExtensionHost.hpp"

// Markup-extension implementations. Kept as one TU because the historic
// .inl files share helper symbols (constant conversion, dynamic-resource
// lookup, CaptureControlTemplateChildName) that were written for a single
// amalgamated translation unit.
#include "gui/markup/BindingExtension.inl"
#include "gui/markup/DynamicResourceExtension.inl"
#include "gui/markup/StaticResourceExtension.inl"
#include "gui/markup/LocExtension.inl"
#include "gui/markup/TemplateBindingExtension.inl"
#include "gui/markup/TypeExtension.inl"
#include "gui/markup/StaticExtension.inl"
