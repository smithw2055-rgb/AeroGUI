# Runtime Access Ownership Model

## Purpose

WPF/XAML authoring headers expose controls, dependency properties, routed
commands, styles, templates and value types. Runtime coordination managers are
implementation services owned by the view composition and must not own public
namespace declarations or require manager-specific friendship in authoring
classes.

## Ownership

Presentation runtime managers are nested under
`Aero::Detail::PresentationRuntimeAccess`. Controls runtime managers are nested
under `Aero::Detail::ControlRuntimeAccess`.

Private source code retains `Aero::Presentation::XManager` and
`Aero::Controls::XManager` aliases so implementation call sites stay readable.
The aliases are incomplete in installed headers; complete declarations remain
under `src/presentation/RuntimeManagers.hpp` and
`src/controls/RuntimeManagers.hpp`.

Authoring classes grant friendship to one access owner per domain, rather than
to every manager independently. Nested manager classes inherit the enclosing
friend access without expanding the public friend list.

## Deliberate exceptions

`RenderManager` remains in the private render boundary until render/RHI ABI
work is completed. `VisualStateManager` remains public because it maps to WPF
semantics. `FontManager` remains public pending the text-provider SDK review.

The existing TextElement, Run, Span, LineBreak and button-derived Hyperlink are
unchanged. The Documents model remains a separate future stage.
