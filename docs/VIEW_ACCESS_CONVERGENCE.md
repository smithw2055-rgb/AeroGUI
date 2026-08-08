# View access convergence — completed

> Historical milestone record. See `SOURCE_ARCHITECTURE.md` for the current
> `ViewState`/`ViewRenderer` ownership model.

This migration is complete. `Runtime::Detail::ViewAccess` has been removed.

The final ownership boundary is direct and domain-based:

```text
XamlReader(Gui&) -> Gui XamlRuntime -> unmounted XamlDocument
ReloadCoordinator -> Gui XamlRuntime + narrow View mount/reload operations
DesktopHost -> Gui + View + App::Detail::RenderContext
View -> content, resources, layout, input, animation and frame state
```

There is no generic private gateway and no service-locator-style access to View
engines. XAML loading does not require a View; View-affine Binding and
DynamicResource services are injected when deferred document effects are
mounted.

The installed View surface therefore reads as a host/presentation object rather
than a loader facade: content, viewport, update, input and renderer.
