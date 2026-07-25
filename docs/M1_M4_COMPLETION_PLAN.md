# M1-M4 Completion Program

Status: active implementation program

This document converts the broad M1-M4 roadmap into mergeable, test-gated slices. A milestone is complete only when every listed acceptance gate is automated. Feature presence without failure, lifetime, performance and recovery coverage does not count as completion.

## Current baseline

The `codex/m3-interactive-controls-text-opengl` baseline already contains most M1-M3.5 vertical slices:

- C++17 foundation, intrusive ownership, metadata, DependencyProperty and Dispatcher;
- runtime and compiled XAML, logical/visual trees, layout and immutable RenderPlan;
- Binding/DataContext, Style/Template, commands and routed input;
- interactive controls, TextBox, IME, scrolling, ItemsControl and recycling virtualization;
- provider-neutral text, FreeType/HarfBuzz, D3D11/WARP and OpenGL 3.3 through WGL/GLX;
- a real ControlGallery and cross-backend conformance fixtures.

The remaining work is primarily production closure, lifecycle unification, platform breadth and quality gates.

## Slice ordering

### Slice A — Runtime composition and host-driven render queue

Deliverables:

- `Markup::RuntimeHost` composition root;
- deterministic initialization and reverse-order shutdown;
- one frame-phase entry point;
- automatic input/control/TextBox interaction wiring;
- bounded `QueuedRenderBackend` with no hidden worker thread;
- lifecycle and queue tests.

Acceptance:

- static/shared Debug/Release on MSVC, GCC and Clang;
- public-header C++17 self-containment;
- mount, frame, unmount and queue tests under CTest.

### Slice B — Atomic tree mutation and safe deferred work

Deliverables:

- two-phase ObjectTree root/attach transactions;
- rollback-safe handle, inheritance and lifecycle publication;
- generation-handle layout/render queues;
- event-route lifetime snapshots;
- detach-before-flush and destroy-during-dispatch tests;
- allocation-failure tests for every mutation stage.

Acceptance:

- any failed mutation leaves public state identical to pre-call state;
- no deferred queue stores an unprotected object pointer;
- ASan/UBSan and injected-OOM suites pass.

### Slice C — Runtime services and object-state slimming

Deliverables:

- internal mount service shared by XAML, templates and item generation;
- ItemContainerGenerator limited to mapping, realization and recycling;
- TextBox editor controller/layout/render-state split;
- platform-neutral clipboard/IME contracts separated from Win32 adapters;
- private sidecars for layout/render/input state where source compatibility permits.

Acceptance:

- one attachment transaction path;
- no duplicate logical/visual/layout/render ownership sequencing in controls;
- ControlGallery uses RuntimeHost rather than manual service construction.

### Slice D — Unicode text completion

Deliverables:

- Unicode line-break opportunities;
- paragraph bidi resolution and visual run order;
- script/language itemization;
- logical/visual cluster mapping;
- bidi caret navigation, hit testing and selection geometry;
- mixed Arabic/Latin/CJK wrapping and trimming fixtures.

Acceptance:

- locked fonts and structural glyph/cluster/line snapshots;
- TextBlock and TextBox mixed-direction pixel gates;
- deterministic results across supported desktop backends.

### Slice E — GLES 3.0, EGL and Android

Deliverables:

- EGL owned/borrowed display, context and surface adapter;
- GLES 3.0 RHI backend with GLSL ES 300 package;
- Android lifecycle host sample;
- pause/resume, surface loss, resize and DPI tests.

Acceptance:

- no GLES 3.1 assumptions;
- context recreation rebuilds renderer-owned resources;
- Android sample survives repeated background/foreground cycles.

### Slice F — Vulkan strategic backend

Deliverables:

- owned and borrowed instance/device/queue contracts;
- resource states, descriptor/pipeline cache and fence retirement;
- Linux and Android surfaces;
- RenderPlan conformance shared with D3D11/OpenGL/GLES.

Acceptance:

- validation-layer clean;
- repeated device/surface recreation;
- fixed pixel and plan-hash fixtures.

### Slice G — D3D12 and Metal strategic backends

Deliverables:

- D3D12 descriptor heaps, resource states, fences and device loss;
- Metal command-buffer integration, drawable loss and mobile backgrounding;
- host-owned device/queue modes;
- offline shader packages.

Acceptance:

- debug/validation clean;
- shared RenderPlan fixtures;
- owned/borrowed lifetime tests.

### Slice H — WebAssembly and WebGL 2

Deliverables:

- browser host and requestAnimationFrame scheduler;
- WebGL 2-only RHI path;
- devicePixelRatio and Canvas resize;
- context loss/restore and full atlas/resource rebuild;
- WASM sample and two-browser automation.

Acceptance:

- no WebGL 1 fallback;
- repeated `WEBGL_lose_context` recovery;
- browser console errors fail CI.

### Slice I — Production rendering features

Deliverables:

- offscreen passes, masks and effects;
- geometry/path cache and tessellation provider boundary;
- image cache and budget manager;
- render-proxy snapshots consumed through the M4 queue;
- animation clock and property animation provider.

Acceptance:

- immutable cross-thread packets;
- fence-safe resource retirement;
- budget, loss and long-running stress tests.

### Slice J — Accessibility, inspector and release gates

Deliverables:

- accessibility tree and platform adapters;
- logical/visual/render tree inspector;
- effective-value, binding, layout, batching and cache diagnostics;
- fuzz, sanitizer, ABI and dependency audits;
- locked performance budgets and long-running stability tests.

Acceptance:

- all required jobs are mandatory rather than allowed failures;
- M1-M4 roadmap documents and capability manifests match executable behavior;
- ControlGallery is a release-level conformance application.

## Definition of M1-M4 complete

M1-M4 is complete only when Slices A-J are merged and their acceptance gates run in required CI profiles. Missing private console SDK backends may remain in restricted repositories, but the public versioned adapter contract and conformance harness must be complete.
