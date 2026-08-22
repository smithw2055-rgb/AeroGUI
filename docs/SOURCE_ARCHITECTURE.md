# AeroGUI source ownership

This document maps implementation responsibilities inside `src`. It does not
define another SDK layer; installed product contracts are under `include/Aero`,
`include/AeroRender`, `include/AeroApp`, and `include/AeroAudio`.

## Directory owners

```text
src/base/       allocation, strings, object lifetime, streams and C ABI
src/gui/        WPF semantic kernel, Gui/View composition and ViewRenderer
src/gui/core/        root Aero: elements, DependencyProperty, Freezable, events, Dispatcher
src/gui/controls/    controls implementations + the Measure/Arrange layout engine (no templates/styles)
src/gui/templates/   ControlTemplate, DataTemplate and the template engine
src/gui/styles/      Style and ResourceDictionary
src/gui/data/        Aero::Data Binding engine
src/gui/markup/      XAML schema, parser, writer, compiled documents and cache
src/gui/input/       platform-neutral input services
src/gui/text/        shaping, glyph atlas, editing and font adapters
src/gui/media/       brushes, images, transforms, effects and animation (incl. the EventTrigger/StoryboardActions/TimerTrigger engine)
src/gui/interactivity/  Aero::Interactivity: Blend behaviors, trigger actions and the interactivity engine
src/gui/triggers/    Aero::Triggers: core WPF style triggers (Trigger/DataTrigger/MultiTrigger/MultiDataTrigger and Condition)
src/gui/meta/        Aero::Meta / Aero::Module type, value, metadata and modules
src/gui/diagnostics/ opt-in inspection and rendering diagnostics
src/render/     immutable-frame encoding, GPU resources and native backends
src/app/        Application, Window, DesktopHost and desktop presentation
src/audio/      optional audio product
```

The `src/gui` tree intentionally mirrors the installed WPF-semantic namespaces
(`Aero`, `Aero::Controls`, `Aero::Data`, `Aero::Markup`, `Aero::Media`,
`Aero::Triggers`, `Aero::Meta`) so that a WPF developer can locate the
implementation of a public type by its namespace. Private implementation
access headers use the `*State.hpp` / `*Access.hpp` spelling (for example
`ElementState.hpp`, `MetadataState.hpp`) rather than the retired `*Runtime.hpp`
pattern.

App-owned XAML behavior is supplied to the Gui schema through copied module
descriptors (`Markup::ResourceScopeRegistration`). This keeps callbacks close
to the owning product while preserving the one-way `AeroApp -> AeroGui`
binary dependency.

The retired `src/integration`, `src/runtime`, `src/providers`, root
`src/platform`, and domain `private`/`detail` directories must not return.
Files use responsibility names; `*Internal*` and `*Private*` filenames are
forbidden. Helpers needed by one translation unit stay in an anonymous
namespace.

## Namespace policy

`Detail` and `Runtime` namespaces are retired everywhere, in installed headers
and in source. Symbol classes that previously lived behind `Detail` now belong
directly to their owning product namespace (`Aero::Base`, `Aero::Meta`,
`Aero::Media::Animation::Model`, and so on). Implementation state that only
one translation unit needs stays in an anonymous namespace (for example the
object lifetime/control-block machinery in `src/base/Object.cpp`); opaque
handles (`void*`) keep such state out of the installed headers. The
`cmake/CheckArchitecture.cmake` gate fails any installed header or source file
that still declares a `Detail` or `Runtime` namespace.

## Trigger and behavior headers

The old `include/Aero/Triggers/` bucket mixed three WPF concepts with three
different namespaces. It is now split into a WPF-faithful three-way layout so a
WPF developer can find a type by its .NET namespace:

```text
include/Aero/Triggers/          Aero::*            core WPF style triggers
    TriggerBase.hpp  Trigger.hpp  DataTrigger.hpp
    MultiTrigger.hpp  MultiDataTrigger.hpp  Conditions.hpp (Aero::Condition)
    Triggers.hpp                      umbrella aggregator

include/Aero/Interactivity/      Aero::Interactivity   System.Windows.Interactivity (Blend)
    Behavior.hpp  BlendBehaviors.hpp  TriggerAction.hpp
    ChangePropertyAction.hpp  SetFocusAction.hpp
    RemoveElementAction.hpp  LaunchUriOrFileAction.hpp
    InteractionTriggers.hpp  (PropertyChangedTrigger, KeyTrigger,
                              InvokeCommandAction, SelectAction,
                              SelectAllAction, PlaySoundAction)
    Conditions.hpp       (ComparisonCondition, ConditionalExpression,
                          ConditionBehavior, the Blend condition primitives)

include/Aero/Media/Animation/    Aero::Media::Animation   System.Windows.Media.Animation
    EventTrigger.hpp  StoryboardActions.hpp (BeginStoryboard, etc.)
    StoryboardCompletedTrigger.hpp  TimerTrigger.hpp  MediaActions.hpp
```

Rules:

* Core style triggers (`Trigger`, `DataTrigger`, `MultiTrigger`,
  `MultiDataTrigger`) stay in the `Aero` namespace (no sub-namespace), matching
  WPF `System.Windows.TriggerBase` and friends.
* Blend interactivity (`Behavior`, `TriggerAction` and the concrete actions,
  `InteractionTriggers`) lives in `Aero::Interactivity`, matching
  `System.Windows.Interactivity`.
* Animation triggers (`EventTrigger`, `StoryboardActions`, `TimerTrigger`,
  `MediaActions`, `StoryboardCompletedTrigger`) live in `Aero::Media::Animation`,
  matching `System.Windows.Media.Animation`.

`TriggerAction` is an `Aero::Interactivity` type; the animation trigger headers
reference it through `using Aero::Interactivity::TriggerAction;`. The Blend
condition primitives (`ComparisonCondition`, `ConditionalExpression`,
`ConditionBehavior`) were relocated from `Aero::Media::Animation` to
`Aero::Interactivity` because they are authored through interactivity XAML and
are not part of the timeline model. `Triggers.hpp` re-exports all three groups.

## View composition

`ViewState` owns the view-affine object factory, effective values, animation,
element tree, layout, render tree, image cache, text pipeline, binding, events,
input, control behavior, templates, styles, and the one `ViewRenderer`.

The public `View` surface stays small and WPF/Noesis shaped. `Gui` and
`XamlReader` are the only trusted construction/loading peers. `DesktopHost`
uses public `View` methods and does not operate on `ViewState`.

No source-only object uses a heap-allocated `Impl`/`Access` Pimpl. Where a
large implementation type must remain out of a source header, the owner keeps
fixed aligned storage and constructs the data state in place. There is no
second ownership or forwarding object.

## Render source boundary

`src/render/` holds the backend-neutral render contracts (`DrawingContext`,
`RenderTree`, `RenderDevice`, `RenderTarget`, `FrameEncoder`, `TextRenderer`).
These compile directly into `AeroGui`; `AeroRender` is an installed *interface*
CMake target over those contracts, not a separate DLL or binary. The
`d3d11/`, `opengl33/`, and `platform/` subdirectories are the opt-in native
backend products (`AeroRenderD3D11`, `AeroRenderOpenGL33`) and their surface
adapters, and link into those backend products only.

## Rendering ownership

```text
View
  -> ViewRenderer : IRenderer
       -> immutable RenderFrame
       -> UiFrameEncoder
       -> direct buffer/texture uploads
       -> one RenderBatch per UI draw
       -> shared RenderDevice
```

Only `ViewRenderer` is a concrete renderer. `RenderDevice` has no renderer
pointer or token. `UiFrameEncoder` is a helper directly owned by
`ViewRenderer`, not a peer renderer or command-list product.

D3D11 and OpenGL device implementations execute `RenderBatch` directly and
own their native resource/pipeline caches. A batch is one draw, not a list of
begin-pass, bind, upload, draw, and end-pass commands. Native pipeline values
are selected from the UI shader/render-state key inside the backend.

`RenderTarget` identifies a drawable embedded or desktop target and delegates
only target acquisition, resize, loss, restore, and drawing. Desktop frame
state and presentation belong exclusively to `src/app/RenderContext.*` and
the concrete App D3D11/OpenGL contexts.

## Markup ownership

`Schema`, `GuiSchema`, `Loader`, `DocumentCache`, `DependencyGraph`, UI object
model facets, and template facets are ordinary source-only implementations.
They use direct or in-place state and do not publish implementation classes in
the SDK. `XamlDocument` is the ABI-heavy public value that deliberately keeps
an opaque state pointer.

## CMake ownership

Gui, Controls, Markup, View composition, and backend-neutral rendering sources
compile directly into `AeroGui`; App sources compile into `AeroApp`; Audio
sources compile into `AeroAudio`; native backend sources compile into their
matching render backend DLLs. `Aero::Render` is an installed interface target
over contracts exported by `AeroGui`, not another DLL. Object targets exist
only for header-consumer checks and are never installed products. Every `.cpp`
has one compile owner.

The permanent gate is `cmake/CheckArchitecture.cmake`. It checks final
invariants rather than migration stage names.
