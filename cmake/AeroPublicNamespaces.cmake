# Canonical namespace manifest for installed Aero headers.
#
# The list is intentionally expressed as semantic namespace prefixes rather
# than as a generated inventory of every type. A nested namespace is allowed
# only when it belongs to one of these prefixes.
set(AERO_PUBLIC_NAMESPACE_PREFIXES
    Aero
    Aero::App
    Aero::Audio
    Aero::Base
    Aero::Collections
    Aero::Controls
    Aero::Controls::Primitives
    Aero::Data
    Aero::Diagnostics
    Aero::Documents
    Aero::Events
    Aero::Input
    Aero::Platform
    Aero::Render
    Aero::Markup
    Aero::Media
    Aero::Media::Animation
    Aero::Meta
    Aero::Shapes
    Aero::Text
    Aero::Threading)

# Base detail templates are a temporary ABI implementation seam. They may be
# forward-declared by public classes, but are not authoring namespaces and
# must not gain new user-facing types.
set(AERO_PUBLIC_NAMESPACE_DETAIL_PREFIXES
    Aero::Base::Detail)

set(AERO_PUBLIC_NAMESPACE_DETAIL_HEADERS
    include/Aero/Meta.hpp)

set(AERO_PUBLIC_NAMESPACE_BASE_DETAIL_HEADERS
    include/Aero/Base/Delegate.hpp
    include/Aero/Base/HashSet.hpp
    include/Aero/Base/MetadataId.hpp
    include/Aero/Base/Object.hpp
    include/Aero/Base/Ref.hpp
    include/Aero/Base/Vector.hpp
    include/Aero/Meta.hpp
    include/Aero/Value.hpp)

# Product-layer implementation namespaces are never allowed in installed
# headers. Source-private Detail namespaces are valid implementation choices,
# but must not leak through the SDK surface.
set(AERO_PUBLIC_NAMESPACE_FORBIDDEN_PATTERNS
    "namespace[ \t]+Internal([ \t:{]|$)"
    "namespace[ \t]+Aero::Internal([ \t:{]|$)"
    "Aero::Internal::"
    "::Internal::"
    "namespace[ \t]+impl([ \t:{]|$)"
    "namespace[ \t]+Aero::impl([ \t:{]|$)"
    "namespace[ \t]+Aero::Gui::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero([ \t:{]|$)"
    "namespace[ \t]+Aero::Runtime::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Text([ \t:{]|$)"
    "Aero::Gui::Detail::"
    "Aero::"
    "Aero::Runtime::Detail::"
    "Aero::Text::"
    "::impl::"
    "namespace[ \t]+Aero::Render([ \t:{]|$)"
    "Aero::Render::"
    "namespace[ \t]+Aero::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Controls([ \t:{]|$)"
    "namespace[ \t]+Aero::Media([ \t:{]|$)"
    "namespace[ \t]+Aero::Markup([ \t:{]|$)"
    "namespace[ \t]+Aero::App([ \t:{]|$)"
    "Aero::Controls::"
    "Aero::Media::"
    "Aero::Markup::"
    "Aero::App::"
    "(^|[^A-Za-z0-9_])(ElementPrivate|ControlPrivate|TemplatePrivate|StylePrivate|TransformPrivate|BrushPrivate|EffectPrivate|AnimationPrivate|DesktopPrivate|ViewData|RenderTree|RenderResources|RenderFunctions|RenderDeviceFactory|AdoptRenderDevice|ITextBlockLayoutService|TextBlockLayoutServiceScope|TextBlockRenderService|D3D11TextBlockRenderService|IGlyphRunResourceRegistry|DisplayListBuilder|RoutedHandlerStorage|RoutedHandlerTraits|RuntimeManagersFwd|ThemeStyleRegistry)([^A-Za-z0-9_]|$)")
