# Application and Window Product Model

## Status

This specification defines the WPF-facing Application and Window boundary for
the AeroGUI C++ SDK. It complements `PUBLIC_NAMESPACE_MODEL.md` and is the
source of truth for App-layer ownership and dependency direction.

## Product position

AeroGUI is a WPF/XAML-oriented C++ UI SDK implemented with metadata, behavioral
facets, a retained View runtime and native GPU rendering. Meta and Facet are
implementation mechanisms; they do not replace the familiar Application,
Window or control authoring model.

The canonical public types are:

```cpp
Aero::Application
Aero::Window
Aero::App::Launcher
Aero::App::LaunchOptions
Aero::Integration::WindowInterop
```

`Aero::App::Application` and `Aero::App::Window` are not public alternatives.
The legacy include paths under `Aero/App/` may forward to canonical headers,
but they must not define duplicate type aliases.

## Application semantics

`Aero::Application` is the process-level WPF/XAML application object. Its core
responsibilities are application resources, startup policy, current application
identity, the main-window association and shutdown requests.

Application remains constructible in a headless or embedded runtime. It does
not own a native window, graphics device, audio engine or render endpoint.
Optional platform services belong to `Aero::App::Services`, owned by Launcher.

## Window semantics

`Aero::Window` is a WPF-facing XAML content root and derives from
`Aero::Controls::ContentControl`. It owns Window-specific dependency properties
and lifecycle commands while inheriting normal FrameworkElement and Control
properties.

Window must not redeclare forwarding accessors for inherited properties such as
FontFamily. A property is declared on the semantic owner and inherited through
the normal metadata/type graph.

Native handles and hosted Views are integration concerns. They are available to
friend integration accessors, not normal application code.

## Peer boundary

Application and Window delegate default-host operations through narrow App peer
contracts:

```text
Aero::Application  -> App::Detail::IApplicationPeer
Aero::Window       -> App::Detail::IWindowPeer
```

The WPF-facing object headers contain only forward declarations and non-owning
peer pointers. They do not define native-window interfaces, host
implementations or View orchestration contracts.

Future peer expansion must correspond to observable Window or Application
semantics. It must not become a generic service locator.

## Metadata ownership and order

Metadata modules follow this dependency order:

```text
Aero.Core
  -> Aero.UI
    -> Aero.Controls
      -> Aero.App
        -> Aero.Markup
```

The App module owns Application and Window descriptors. Controls must not
include or register App types. Because Window derives from ContentControl,
Controls metadata must be registered before App metadata. Markup is registered
last because object-writer behavior consumes the complete built-in UI schema.

## Product surfaces

```text
Aero::Gui          retained WPF/XAML class library
Aero::App          default Application/Window lifetime
Aero::Integration  custom engine and native hosting
Aero::ModuleSdk    metadata authoring over normal GUI types
```

CMake targets express deployment boundaries. C++ namespaces express semantic
domains. XAML identity remains the namespace URI and local name, independent
from both.

## Next stages

### A1: lifecycle semantics

- Startup and Exit event contracts;
- explicit ShutdownMode behavior;
- stable Current lifetime and nested-host rejection;
- startup and shutdown diagnostics.

### A2: complete Window contract

- state, style, resize mode, owner and dialog semantics;
- activated, deactivated, closing and closed events;
- size and location synchronization through dependency properties;
- DPI changes without platform concerns leaking into Window.

### A3: multiple windows

- Window collection owned by the App runtime;
- main-window replacement;
- shutdown policy based on main, last or explicit close;
- one View and peer per top-level Window.

### A4: launcher decomposition

- keep Launcher as the public entry point;
- make ApplicationHost an implementation detail before stable ABI;
- separate event pumping, endpoint selection and document startup;
- keep engine-hosted View integration independent from App.

### A5: package closure

- make App model target dependencies explicit;
- ensure Gui is consumable without native-window backends;
- retain compile-only consumers for Gui, App and Integration;
- reject duplicate App type aliases in architecture checks.
