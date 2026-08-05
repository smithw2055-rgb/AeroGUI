# WPF / NoesisGUI architecture review

Baseline: `codex/local-no-samples-tests` at `266fd86542e4c4d0e75ed0ceacb869d2dd69b5c8`.

## Product model

AeroGUI keeps three supported products and does not expose internal object
libraries as SDK concepts:

- `Aero::Gui` is the retained-mode WPF/XAML class library.
- `Aero::Integration` is the embeddable View, renderer and provider product.
- `Aero::App` is the optional WPF-style `Application` / `Window` desktop product.

`Application` and `Window` retain their WPF-facing names in namespace `Aero`,
but their object state, descriptors and default desktop lifetime are owned by
`Aero::App`. The Integration binary must not acquire those symbols merely to
create a `View`.

The schema order is now explicit:

1. Core value and metadata types.
2. GUI dependency-object types.
3. Controls.
4. Product and user modules, including `Aero.App` when the App product is used.
5. Markup metadata and object-writer facets.

This removes the previous `App -> Controls` base-type inversion and lets the
same platform-neutral `Gui` initialize without desktop application semantics.
The offline schema compiler explicitly opts into `Aero.App`, so App.xaml and
Window roots remain available to compiled XAML.

## Dependency-property value boundary

Expression runtimes no longer borrow a conversion helper from Binding. Binding
and MultiBinding now converge on one GUI-kernel operation after an explicit
converter has run; animation and trigger runtimes can use the same operation
without depending on the Data subsystem:

- exact and `AnyValue` assignments pass through;
- authored text uses the target type's metadata text codec;
- null object values are retagged to the declared target type;
- object covariance is checked against the frozen type registry;
- all other type changes fail deterministically.

Media-specific conversions do not belong in the property kernel. In particular,
`Color -> Brush` is no longer hard-coded in Binding. XAML text conversion belongs
to Brush metadata, while runtime model conversion belongs to an authored
`IValueConverter`. This follows WPF's separation between the property system,
XAML type conversion and binding conversion. The default StringFormat path also
checks for `StreamGeometry` before casting, removing the previous invalid cast
from a base `Geometry` object.

## Retained decisions

- C++17, no exceptions and no RTTI remain product constraints.
- Public control and property names follow WPF/XAML vocabulary.
- Facets remain implementation data used by metadata, XAML and compiled XAML;
  they are not a second authoring object hierarchy.
- `View` owns one runtime state graph and emits immutable render frames.
- Render scheduling remains host-owned; render devices do not create hidden
  worker threads or service locators.

## Next structural stages

The next high-value pass should interiorize the remaining private loader,
resource and namescope helpers declared on `View`. The installed `View` surface
should read as a host object: content, viewport, update, input and renderer.
`XamlReader` and source-private runtime accessors can use a single private
`ViewAccess` friend instead of a growing list of private methods.

After that, split the large consolidated implementation files by stable WPF
responsibility only when it improves ownership or compilation isolation. Do not
reintroduce `Runtime`, `Service`, `Manager`, `Contract`, `Catalog` or `Endpoint`
facades merely to create directories.
