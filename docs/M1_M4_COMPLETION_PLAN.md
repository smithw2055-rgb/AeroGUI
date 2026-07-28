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

Implementation status: **portable implementation complete; CI validated**.

Deliverables:

- `Aero::View` composition root;
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

Implementation status: **portable safety primitives and conformance coverage complete; direct ObjectTree/LayoutManager/RenderManager migration remains incremental**.

Delivered on this branch:

- compensation-based `MutationJournal` with reverse-order rollback;
- weak-reference `SafeDeferredWorkQueue` with cancellation, expiry and failure statistics;
- strong-reference `EventRouteLifetimeSnapshot` for dispatch-time lifetime protection;
- destroyed-object, rollback and route-lifetime regression tests.

Remaining native runtime migration:

- replace legacy raw-pointer lifecycle/layout/render queues with the safe queue or generation-handle storage;
- route all existing tree mutation code through prepared capacity plus rollback journals;
- add allocator-injected failure points to each existing mutation stage.

Acceptance target:

- any failed mutation leaves public state identical to pre-call state;
- no deferred queue stores an unprotected object pointer;
- ASan/UBSan and injected-OOM suites pass.

### Slice C — Runtime services and object-state slimming

Implementation status: **shared mount transaction and sidecar foundation complete; legacy callers still need migration**.

Delivered on this branch:

- `MountTransactionService` for logical, visual, layout and render attachment/rollback;
- generation-keyed `RuntimeObjectStateStore` sidecars;
- platform-neutral `HostTextServices` bundle;
- split TextEditor controller/layout/render state records;
- mount/detach and stale-sidecar tests.

Remaining migration:

- move XAML, templates and item generation to the shared service;
- move eligible queue flags/revisions from public elements into sidecars;
- refactor ControlGallery to use only View and shared mount services.

Acceptance target:

- one attachment transaction path;
- no duplicate logical/visual/layout/render ownership sequencing in controls;
- ControlGallery uses View rather than manual service construction.

### Slice D — Unicode text completion

Implementation status: **provider-neutral UTF-8 analysis baseline complete; generated full Unicode tables and final shaping integration remain**.

Delivered on this branch:

- strict UTF-8 scalar decoding;
- Latin, Arabic, Hebrew, Han, Hiragana and Katakana itemization;
- paragraph direction, embedding levels and visual scalar ordering;
- logical/visual scalar and cluster mapping;
- deterministic line-break opportunities for spaces, mandatory breaks, hyphens and CJK;
- mixed Arabic/Latin/CJK and combining-mark tests.

Remaining production integration:

- replace compact classifiers with generated Unicode data files;
- wire analysis into TextLayout shaping, wrapping, trimming, caret and selection geometry;
- add locked-font glyph/cluster/line and mixed-direction pixel gates.

Acceptance target:

- locked fonts and structural glyph/cluster/line snapshots;
- TextBlock and TextBox mixed-direction pixel gates;
- deterministic results across supported desktop backends.

### Slice E — GLES 3.0, EGL and Android

Implementation status: **portable lifecycle/GLES 3.0 contract complete; native EGL/GLES adapter and Android sample pending platform toolchain**.

Delivered on this branch:

- `Gles30Contract` rejecting ES 3.1-only assumptions;
- `AndroidLifecycleHost` pause/resume/surface-loss/restore state machine;
- generation, resize and lifecycle conformance tests.

Native deliverables still required:

- EGL owned/borrowed display, context and surface adapter;
- GLES 3.0 RHI command execution and GLSL ES 300 shader package;
- Android NativeActivity or game-engine host sample and device/emulator CI.

### Slice F — Vulkan strategic backend

Implementation status: **portable resource-state, fence-retirement and lifecycle contract complete; native Vulkan implementation pending SDK-backed target**.

Delivered on this branch:

- owned/borrowed backend lifecycle model;
- portable resource state validation;
- monotonic submit/completion fences;
- fence-safe retirement queue and conformance tests.

Native deliverables still required:

- Vulkan instance/device/queue/swapchain adapters;
- descriptors, pipelines, command buffers and resource barriers;
- Linux/Android surface integration, validation-layer and pixel fixtures.

### Slice G — D3D12 and Metal strategic backends

Implementation status: **portable descriptor/drawable/background lifecycle contracts complete; native API implementations pending**.

Delivered on this branch:

- D3D12 descriptor heap capacity and reset contract;
- Metal drawable availability, command-buffer and background/foreground state model;
- owned/borrowed lifetime tests.

Native deliverables still required:

- D3D12 device/queue, resource states, heaps, command lists, fences and device-loss recovery;
- Metal device/queue, pipeline, resource and drawable integration;
- offline DXIL/metallib packages and platform CI.

### Slice H — WebAssembly and WebGL 2

Implementation status: **WebGL2-only browser lifecycle contract complete; Emscripten/browser adapter and browser automation pending**.

Delivered on this branch:

- explicit rejection of WebGL 1 fallback;
- devicePixelRatio/canvas pixel sizing;
- requestAnimationFrame serial scheduling;
- context loss/restore and resource-generation rebuild signaling;
- conformance tests.

Native/Web deliverables still required:

- WebGL2 RHI function adapter and Emscripten host;
- browser event/input/IME/clipboard integration;
- repeated `WEBGL_lose_context` recovery in two browser engines.

### Slice I — Production rendering features

Implementation status: **backend-neutral production model complete; renderer lowering and GPU implementations remain**.

Delivered on this branch:

- immutable `FrozenRenderPacket` for cross-thread queue consumption;
- ordered offscreen/effect/mask pass graph validation;
- geometry tessellation provider boundary;
- fence-aware cache budget manager with deterministic LRU eviction;
- deterministic scalar animation clock;
- render packet, graph, cache and animation tests.

Remaining renderer work:

- lower pass graph to D3D11/OpenGL and future backends;
- implement blur, drop shadow, masks and offscreen target pooling;
- integrate geometry/image/glyph caches and animation provider with effective values.

### Slice J — Accessibility, inspector and release gates

Implementation status: **portable accessibility/inspection/gate models complete; platform adapters and mandatory release CI expansion remain**.

Delivered on this branch:

- accessibility tree, roles, actions and versioned platform adapter callbacks;
- logical/visual/render inspector snapshot;
- locked performance budget evaluator;
- fuzz target harness;
- production capability manifest validation;
- long-running stability counter;
- accessibility, inspector, performance, fuzz, capability and stability tests.

Remaining release work:

- Win32 UIA, Apple Accessibility, Android accessibility and Web ARIA adapters;
- inspector UI and effective-value/binding/cache detail views;
- sanitizer, ABI, dependency and long-running jobs made mandatory;
- release-level ControlGallery fixtures for every native backend.

## Current branch acceptance

The `agent/m1-m4-runtime-foundation` branch instantiates and executes portable Slice A-J paths through `AeroModuleRuntimeTests`. The CI matrix covers:

- Windows Debug static plus D3D11/WARP gates;
- Windows Release shared;
- Linux GCC Debug static plus quality and public-header gates;
- Linux GCC Release shared;
- Linux Clang Debug static.

Passing these jobs proves C++17 portability and portable contract behavior. It does **not** by itself prove native Vulkan, D3D12, Metal, GLES or WebGL execution; those remain separate SDK-backed acceptance targets as listed above.

## Definition of M1-M4 complete

M1-M4 is complete only when Slices A-J are merged and their native/platform acceptance gates run in required CI profiles. Missing private console SDK backends may remain in restricted repositories, but the public versioned adapter contract and conformance harness must be complete.
