#include "GalleryRuntime.hpp"

#include <Aero/Platform/Win32Window.hpp>
#include <Aero/Rhi/D3D11Backend.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>
#include <Aero/Rhi/WglSurface.hpp>
#include <Aero/Render/D3D11RendererBackend.hpp>
#include <Aero/Render/D3D11TextBlockRenderService.hpp>
#include <Aero/Render/OpenGL33RendererBackend.hpp>
#include <Aero/Text/FreeTypeAdapter.hpp>
#include <Aero/Text/FontManager.hpp>

#include <cstdlib>
#include <filesystem>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Platform;
using namespace Rhi;
using namespace Render;

bool ResolveGalleryFontPath(
    std::string& output,
    const std::vector<std::string_view>& candidates) noexcept {
    for (std::string_view candidate : candidates) {
        if (candidate.empty()) continue;
        std::error_code error;
        if (std::filesystem::exists(candidate, error) &&
            !error) {
            output = candidate;
            return true;
        }
    }
    return false;
}

Result<void> LoadGalleryFontFace(
    Text::FontManager& fonts,
    Text::FontProviderId provider,
    const char* role,
    const char* family,
    const char* language,
    const std::vector<std::string_view>& candidates,
    Text::FontFace& out) noexcept {
    std::string path;
    if (!ResolveGalleryFontPath(path, candidates)) {
        return Status::Failure(
            ErrorCode::NotFound,
            role != nullptr
                ? role
                : "No suitable gallery font was found");
    }

    Text::Typeface typeface;
    Base::StringView familyView(
        family,
        static_cast<std::uint32_t>(std::strlen(family)));
    Result<void> configured =
        typeface.TrySetFamily(familyView);
    if (!configured) return configured.GetStatus();
    if (language != nullptr && language[0] != '\0') {
        Base::StringView languageView(
            language,
            static_cast<std::uint32_t>(std::strlen(language)));
        configured = typeface.TrySetLanguage(languageView);
        if (!configured) return configured.GetStatus();
    }

    Text::FontSource source;
    source.kind = Text::FontSourceKind::File;
    source.identifier = Base::StringView(
        path.data(),
        static_cast<std::uint32_t>(path.size()));

    Result<void> loaded =
        fonts.LoadFace(provider, source, typeface, out);
    if (!loaded) {
        return loaded.GetStatus();
    }
    return {};
}

Result<void> RefreshRuntimeFrame(
    GalleryRuntime& runtime) noexcept {
    WindowEvent refresh;
    refresh.type = WindowEventType::Exposed;
    Result<bool> frame = runtime.HandleWindowEvent(refresh);
    if (!frame) {
        return frame.GetStatus();
    }
    return {};
}

class OpenGL33GlyphRunResourceRegistry final
    : public IGlyphRunResourceRegistry {
public:
    explicit OpenGL33GlyphRunResourceRegistry(
        OpenGL33RenderPlanBackend& backend) noexcept
        : backend_(&backend) {}

    Base::Result<void> RegisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::ResourceHandle atlasTexture,
        Rhi::ResourceHandle sampler,
        Rhi::IndexType indexType) noexcept override {
        return backend_->RegisterGlyphRun(
            glyphRun, vertexBuffer, indexBuffer, indexCount,
            atlasTexture, sampler, indexType);
    }

    Base::Result<void> UnregisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept override {
        return backend_->UnregisterGlyphRun(glyphRun);
    }

private:
    OpenGL33RenderPlanBackend* backend_ = nullptr;
};

constexpr std::uint32_t GalleryWidth = 900U;
constexpr std::uint32_t GalleryHeight = 640U;

WindowDescriptor MakeWindowDescriptor(
    bool visible) noexcept {
    WindowDescriptor descriptor;
    descriptor.title = "AeroGUI ControlGallery";
    descriptor.width = GalleryWidth;
    descriptor.height = GalleryHeight;
    descriptor.visible = visible;
    descriptor.resizable = true;
    return descriptor;
}

NativeSurfaceDescriptor MakeD3D11Descriptor(
    D3D11GraphicsBackend& backend,
    const Win32Window& window) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::D3D11Window;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Immediate;
    descriptor.width = window.ClientWidth();
    descriptor.height = window.ClientHeight();
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    descriptor.sampleCount = 1U;
    descriptor.stableId = UINT64_C(0x43474433443131);
    descriptor.d3d11.window = window.NativeHandle().window;
    descriptor.d3d11.device = backend.NativeDevice();
    descriptor.d3d11.immediateContext =
        backend.NativeImmediateContext();
    return descriptor;
}

NativeSurfaceDescriptor MakeWglDescriptor(
    const Win32Window& window) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::WglWindow;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Immediate;
    descriptor.width = window.ClientWidth();
    descriptor.height = window.ClientHeight();
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    descriptor.sampleCount = 1U;
    descriptor.stableId = UINT64_C(0x434757474C3333);
    descriptor.wgl.window = window.NativeHandle().window;
    return descriptor;
}

bool RequestsClose(
    WindowEventType type) noexcept {
    return type == WindowEventType::CloseRequested ||
        type == WindowEventType::Closed;
}

Result<void> SubmitD3D11Frame(
    D3D11RenderPlanBackend& renderer,
    D3D11GraphicsBackend& backend,
    const GalleryRuntime& runtime) noexcept {
    Result<void> submitted = renderer.Submit(
        runtime.Plan());
    if (!submitted) {
        return submitted.GetStatus();
    }
    return backend.WaitForFence(
        renderer.LastSubmittedFence());
}

Result<void> RunD3D11WindowLoop(
    Win32Window& window,
    GalleryRuntime& runtime,
    D3D11SurfacePresenter& presenter,
    D3D11RenderPlanBackend& renderer,
    D3D11GraphicsBackend& backend) noexcept {
    while (window.IsOpen()) {
        WindowEvent event;
        Result<bool> received = window.WaitEvent(event);
        if (!received) {
            return received.GetStatus();
        }
        if (!received.Value() || RequestsClose(event.type)) {
            break;
        }

        Result<bool> runtimeFrame =
            runtime.HandleWindowEvent(event);
        if (!runtimeFrame) {
            return runtimeFrame.GetStatus();
        }
        if (event.type == WindowEventType::Resized ||
            event.type == WindowEventType::ScaleChanged) {
            if (event.width == 0U || event.height == 0U) {
                continue;
            }
            Result<void> resized = presenter.Resize(
                event.width, event.height);
            if (!resized) {
                return resized.GetStatus();
            }
        }
        if (runtimeFrame.Value()) {
            Result<void> rendered = SubmitD3D11Frame(
                renderer, backend, runtime);
            if (!rendered) {
                return rendered.GetStatus();
            }
        }
    }
    return {};
}

Result<void> SubmitOpenGlFrame(
    OpenGL33RenderPlanBackend& renderer,
    OpenGL33GraphicsBackend& backend,
    const GalleryRuntime& runtime) noexcept {
    Result<void> submitted = renderer.Submit(
        runtime.Plan());
    if (!submitted) {
        return submitted.GetStatus();
    }
    return backend.WaitForFence(
        renderer.LastSubmittedFence());
}

Result<void> RunOpenGlWindowLoop(
    Win32Window& window,
    GalleryRuntime& runtime,
    SurfaceSession& surface,
    OpenGL33RenderPlanBackend& renderer,
    OpenGL33GraphicsBackend& backend) noexcept {
    while (window.IsOpen()) {
        WindowEvent event;
        Result<bool> received = window.WaitEvent(event);
        if (!received) {
            return received.GetStatus();
        }
        if (!received.Value() || RequestsClose(event.type)) {
            break;
        }

        Result<bool> runtimeFrame =
            runtime.HandleWindowEvent(event);
        if (!runtimeFrame) {
            return runtimeFrame.GetStatus();
        }
        if (event.type == WindowEventType::Resized ||
            event.type == WindowEventType::ScaleChanged) {
            if (event.width == 0U || event.height == 0U) {
                continue;
            }
            Result<void> resized = surface.Resize(
                event.width, event.height);
            if (!resized) {
                return resized.GetStatus();
            }
        }
        if (runtimeFrame.Value()) {
            Result<void> rendered = SubmitOpenGlFrame(
                renderer, backend, runtime);
            if (!rendered) {
                return rendered.GetStatus();
            }
        }
    }
    return {};
}

Result<void> RunOpenGlSession(
    SurfaceSession& surface,
    WglSurfaceBackend& native,
    GalleryRuntime& runtime,
    Win32Window* interactiveWindow) noexcept {
    Result<GlFunctionTable> functions =
        native.LoadFunctions();
    if (!functions) {
        return functions.GetStatus();
    }
    Result<GlContextContract> contract =
        native.ContextContract();
    if (!contract) {
        return contract.GetStatus();
    }

    OpenGL33GraphicsBackend backend(
        functions.Value(), contract.Value());
    Result<void> result = backend.Initialize();
    if (!result) {
        return result.GetStatus();
    }
    {
        RhiDevice device(backend);
        result = device.Initialize();
        if (result) {
            OpenGL33RenderPlanBackend renderer(
                device,
                backend,
                surface,
                contract.Value().generation);
            result = renderer.Initialize();
            if (result) {
#if defined(AERO_CONTROL_GALLERY_WITH_FREETYPE)
                Text::FreeTypeAdapter fontProvider;
                Text::FontManager fontManager;
                Text::FontFace latinFace;
                Text::FontFace cjkFace;
                bool hasCjkFace = false;
                result = fontProvider.Initialize();
                if (result) {
                    result = fontManager.Initialize();
                }
                if (result) {
                    result = fontManager.RegisterProvider(
                        {
                            &fontProvider,
                            &fontProvider,
                            &fontProvider});
                }
                if (result) {
                    std::vector<std::string_view> latinCandidates;
                    const char* envFont =
                        std::getenv("AERO_TEXT_TEST_FONT");
                    if (envFont != nullptr && envFont[0] != '\0') {
                        latinCandidates.emplace_back(envFont);
                    }
                    latinCandidates.emplace_back(
                        "C:\\Windows\\Fonts\\arial.ttf");
                    latinCandidates.emplace_back(
                        "C:\\Windows\\Fonts\\segoeui.ttf");
                    latinCandidates.emplace_back(
                        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
                    latinCandidates.emplace_back(
                        "/System/Library/Fonts/Helvetica.ttc");
                    result = LoadGalleryFontFace(
                        fontManager,
                        fontProvider.Identity().id,
                        "primary latin font",
                        "Arial",
                        nullptr,
                        latinCandidates,
                        latinFace);
                }
                if (result) {
                    std::vector<std::string_view> cjkCandidates;
                    const char* envCjk =
                        std::getenv("AERO_TEXT_TEST_CJK_FONT");
                    if (envCjk != nullptr && envCjk[0] != '\0') {
                        cjkCandidates.emplace_back(envCjk);
                    }
                    cjkCandidates.emplace_back(
                        "C:\\Windows\\Fonts\\msyh.ttc");
                    cjkCandidates.emplace_back(
                        "C:\\Windows\\Fonts\\msyhbd.ttc");
                    cjkCandidates.emplace_back(
                        "C:\\Windows\\Fonts\\simhei.ttf");
                    cjkCandidates.emplace_back(
                        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc");
                    cjkCandidates.emplace_back(
                        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
                    Result<void> cjkLoad =
                        LoadGalleryFontFace(
                            fontManager,
                            fontProvider.Identity().id,
                            "cjk fallback font",
                            "Microsoft YaHei",
                            "zh-CN",
                            cjkCandidates,
                            cjkFace);
                    if (cjkLoad) {
                        hasCjkFace = true;
                    }
                }

                OpenGL33GlyphRunResourceRegistry registry(renderer);
                TextBlockRenderService textService(
                    fontManager,
                    device,
                    registry);

                if (result) {
                    TextBlockRenderServiceConfig config;
                    config.face = latinFace;
                    if (hasCjkFace) {
                        config.fallbackFaces = {
                            &cjkFace,
                            1U};
                    }
                    config.pixelSize = 20.0F;
                    config.atlas.pageWidth = 256U;
                    config.atlas.pageHeight = 256U;
                    config.atlas.maxPages = 2U;
                    result = textService.Initialize(config);
                }
                if (result) {
                    result = runtime.SetTextLayoutService(
                        textService, true);
                }
                if (result) {
                    result = RefreshRuntimeFrame(runtime);
                }
#endif
                if (result) {
                    result = SubmitOpenGlFrame(
                        renderer, backend, runtime);
                }
            }
            if (result && interactiveWindow != nullptr) {
                result = RunOpenGlWindowLoop(
                    *interactiveWindow,
                    runtime,
                    surface,
                    renderer,
                    backend);
            }
            renderer.Shutdown();
            if (result) {
                Result<std::uint32_t> collected =
                    device.CollectGarbage();
                if (!collected) {
                    result = collected.GetStatus();
                }
            }
        }
    }
    backend.Shutdown();
    return result;
}

} // namespace

Base::Result<void> RunControlGalleryD3D11(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    Win32Window window;
    Result<void> status = window.Create(
        MakeWindowDescriptor(interactive));
    if (!status) {
        return status.GetStatus();
    }

    D3D11BackendOptions options;
    options.deviceMode = interactive
        ? D3D11DeviceMode::Hardware
        : D3D11DeviceMode::Warp;
    options.allowWarpFallback = true;
    D3D11GraphicsBackend backend(options);
    status = backend.Initialize();
    if (!status) {
        return status.GetStatus();
    }
    {
        RhiDevice device(backend);
        status = device.Initialize();
        if (!status) {
            backend.Shutdown();
            return status.GetStatus();
        }
        D3D11SwapChainSurface native(backend);
        SurfaceSession surface(native);
        NativeSurfaceDescriptor descriptor =
            MakeD3D11Descriptor(backend, window);
        status = surface.Initialize(descriptor);
        if (!status) {
            backend.Shutdown();
            return status.GetStatus();
        }
        D3D11SurfacePresenter presenter(
            device, backend, surface);
        status = presenter.Initialize();
        D3D11RenderPlanBackend renderer(
            device, presenter);
        if (status) {
            status = renderer.Initialize();
        }
        if (status) {
#if defined(AERO_CONTROL_GALLERY_WITH_FREETYPE)
            Text::FreeTypeAdapter fontProvider;
            Text::FontManager fontManager;
            Text::FontFace latinFace;
            Text::FontFace cjkFace;
            bool hasCjkFace = false;
            D3D11GlyphRunResourceRegistry registry(renderer);
            TextBlockRenderService textService(
                fontManager,
                device,
                registry);
            status = fontProvider.Initialize();
            if (status) {
                status = fontManager.Initialize();
            }
            if (status) {
                status = fontManager.RegisterProvider(
                    {&fontProvider,
                     &fontProvider,
                     &fontProvider});
            }
            if (status) {
                std::vector<std::string_view> latinCandidates;
                const char* envFont =
                    std::getenv("AERO_TEXT_TEST_FONT");
                if (envFont != nullptr && envFont[0] != '\0') {
                    latinCandidates.emplace_back(envFont);
                }
                latinCandidates.emplace_back(
                    "C:\\Windows\\Fonts\\arial.ttf");
                latinCandidates.emplace_back(
                    "C:\\Windows\\Fonts\\segoeui.ttf");
                latinCandidates.emplace_back(
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
                latinCandidates.emplace_back(
                    "/System/Library/Fonts/Helvetica.ttc");
                status = LoadGalleryFontFace(
                    fontManager,
                    fontProvider.Identity().id,
                    "primary latin font",
                    "Arial",
                    nullptr,
                    latinCandidates,
                    latinFace);
            }
            if (status) {
                std::vector<std::string_view> cjkCandidates;
                const char* envCjk =
                    std::getenv("AERO_TEXT_TEST_CJK_FONT");
                if (envCjk != nullptr && envCjk[0] != '\0') {
                    cjkCandidates.emplace_back(envCjk);
                }
                cjkCandidates.emplace_back(
                    "C:\\Windows\\Fonts\\msyh.ttc");
                cjkCandidates.emplace_back(
                    "C:\\Windows\\Fonts\\msyhbd.ttc");
                cjkCandidates.emplace_back(
                    "C:\\Windows\\Fonts\\simhei.ttf");
                cjkCandidates.emplace_back(
                    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc");
                cjkCandidates.emplace_back(
                    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
                Result<void> cjkLoad = LoadGalleryFontFace(
                    fontManager,
                    fontProvider.Identity().id,
                    "cjk fallback font",
                    "Microsoft YaHei",
                    "zh-CN",
                    cjkCandidates,
                    cjkFace);
                if (cjkLoad) {
                    hasCjkFace = true;
                }
            }
            if (status) {
                TextBlockRenderServiceConfig config;
                config.face = latinFace;
                if (hasCjkFace) {
                    config.fallbackFaces = {&cjkFace, 1U};
                }
                config.pixelSize = 20.0F;
                config.atlas.pageWidth = 256U;
                config.atlas.pageHeight = 256U;
                config.atlas.maxPages = 2U;
                status = textService.Initialize(config);
            }
            if (status) {
                status = runtime.SetTextLayoutService(
                    textService, true);
            }
            if (status) {
                status = RefreshRuntimeFrame(runtime);
            }
#endif
            if (status) {
                status = SubmitD3D11Frame(
                    renderer, backend, runtime);
            }
            if (status && simulateContextLoss) {
                renderer.Shutdown();
                presenter.Shutdown();
                status = surface.NotifyContextLost();
                if (status) {
                    status = surface.Restore(descriptor);
                }
                if (status) {
                    status = presenter.Initialize();
                }
                if (status) {
                    status = renderer.Initialize();
                }
                if (status) {
#if defined(AERO_CONTROL_GALLERY_WITH_FREETYPE)
                    // Text layout service must be rebound after context restoration.
                    Result<void> textRecoverResult =
                        textService.RecoverDeviceResources(
                            device, registry);
                    if (textRecoverResult) {
                        status = runtime.SetTextLayoutService(
                            textService, true);
                    } else {
                        status = textRecoverResult.GetStatus();
                    }
                    if (status) {
                        status = RefreshRuntimeFrame(runtime);
                    }
#endif
                }
                if (status) {
                    status = SubmitD3D11Frame(
                        renderer, backend, runtime);
                }
            }
        }
        if (status && interactive) {
            status = RunD3D11WindowLoop(
                window,
                runtime,
                presenter,
                renderer,
                backend);
        }
        renderer.Shutdown();
        presenter.Shutdown();
        surface.Shutdown();
    }
    backend.Shutdown();
    return status;
}

Base::Result<void> RunControlGalleryWgl(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    Win32Window window;
    Result<void> status = window.Create(
        MakeWindowDescriptor(interactive));
    if (!status) {
        return status.GetStatus();
    }

    WglSurfaceBackend native;
    SurfaceSession surface(native);
    NativeSurfaceDescriptor descriptor =
        MakeWglDescriptor(window);
    status = surface.Initialize(descriptor);
    if (!status) {
        return status.GetStatus();
    }

    if (simulateContextLoss) {
        status = RunOpenGlSession(
            surface, native, runtime, nullptr);
        if (status) {
            status = surface.NotifyContextLost();
        }
        if (status) {
            status = surface.Restore(descriptor);
        }
        if (status) {
            status = RunOpenGlSession(
                surface,
                native,
                runtime,
                interactive ? &window : nullptr);
        }
    } else {
        status = RunOpenGlSession(
            surface,
            native,
            runtime,
            interactive ? &window : nullptr);
    }
    surface.Shutdown();
    return status;
}

} // namespace Aero::Samples::ControlGallery
