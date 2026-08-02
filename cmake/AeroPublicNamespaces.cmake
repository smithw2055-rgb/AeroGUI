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

# Transitional Base detail references are limited to these declaration owners.
# A new public header must either remove the implementation friend or be
# reviewed as an explicit ABI exception.
set(AERO_PUBLIC_NAMESPACE_DETAIL_HEADERS
    include/Aero/Animation.hpp
    include/Aero/Application.hpp
    include/Aero/ContentElement.hpp
    include/Aero/Controls/Common.hpp
    include/Aero/Controls/Core.hpp
    include/Aero/Controls/Items.hpp
    include/Aero/Controls/Panels.hpp
    include/Aero/Controls/Primitives.hpp
    include/Aero/Controls/Text.hpp
    include/Aero/Documents.hpp
    include/Aero/DrawingContext.hpp
    include/Aero/FrameworkElement.hpp
    include/Aero/Integration/RenderDevice.hpp
    include/Aero/Markup.hpp
    include/Aero/Media/Brushes.hpp
    include/Aero/Media/Effects.hpp
    include/Aero/Media/Transforms.hpp
    include/Aero/Meta.hpp
    include/Aero/Shapes.hpp
    include/Aero/Style.hpp
    include/Aero/Styling.hpp
    include/Aero/UIElement.hpp
    include/Aero/View.hpp
    include/Aero/Visual.hpp
    include/Aero/Window.hpp)

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
    "Aero::App::Detail::")
