# Canonical namespace manifest for installed Aero headers.
#
# The list is intentionally expressed as semantic namespace prefixes rather
# than as a generated inventory of every type.  A nested namespace is allowed
# only when it belongs to one of these prefixes; implementation-only Detail
# namespaces are listed separately so the architecture check can keep their
# transitional ABI surface explicit.
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
    Aero::Integration
    Aero::Markup
    Aero::Media
    Aero::Media::Animation
    Aero::Meta
    Aero::Shapes
    Aero::Threading)

# Base detail templates are a temporary ABI implementation seam. They may be
# forward-declared by public classes, but are not authoring namespaces and
# must not gain new user-facing types.
set(AERO_PUBLIC_NAMESPACE_DETAIL_PREFIXES
    Aero::Base::Detail)

# The only product-layer header with an unqualified `namespace Detail` is the
# metadata authoring header.  Base implementation details are tracked by the
# separate Base list below; do not keep broad historical exceptions here.
set(AERO_PUBLIC_NAMESPACE_DETAIL_HEADERS
    include/Aero/Meta.hpp
    )

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
# headers.  Keep this list explicit so a new namespace cannot silently become
# part of the SDK through a friend declaration or a forwarding header.
set(AERO_PUBLIC_NAMESPACE_FORBIDDEN_PATTERNS
    "namespace[ \t]+Internal([ \t:{]|$)"
    "namespace[ \t]+Aero::Internal([ \t:{]|$)"
    "Aero::Internal::"
    "::Internal::"
    "namespace[ \t]+impl([ \t:{]|$)"
    "namespace[ \t]+Aero::impl([ \t:{]|$)"
    "namespace[ \t]+Aero::Gui::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::GuiPrivate::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Runtime::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Text::Detail([ \t:{]|$)"
    "Aero::Gui::Detail::"
    "Aero::GuiPrivate::Detail::"
    "Aero::Runtime::Detail::"
    "Aero::Text::Detail::"
    "::impl::"
    "namespace[ \t]+Aero::Render([ \t:{]|$)"
    "Aero::Render::"
    "namespace[ \t]+Aero::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Controls::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Media::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Markup::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::Integration::Detail([ \t:{]|$)"
    "namespace[ \t]+Aero::App::Detail([ \t:{]|$)"
    "Aero::Controls::Detail::"
    "Aero::Media::Detail::"
    "Aero::Markup::Detail::"
    "Aero::Integration::Detail::"
    "Aero::App::Detail::"
    "(^|[^A-Za-z0-9_])(ElementPrivate|ControlPrivate|TemplatePrivate|StylePrivate|TransformPrivate|BrushPrivate|EffectPrivate|AnimationPrivate|DesktopPrivate|ViewData|RenderTree|RenderResources|RenderFunctions|RenderDeviceFactory|AdoptRenderDevice|ITextBlockLayoutService|TextBlockLayoutServiceScope|TextBlockRenderService|D3D11TextBlockRenderService|IGlyphRunResourceRegistry|DisplayListBuilder|RoutedHandlerStorage|RoutedHandlerTraits|RuntimeManagersFwd|ThemeStyleRegistry)([^A-Za-z0-9_]|$)")
