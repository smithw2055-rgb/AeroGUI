#include <Aero/Rhi/OpenGL33Backend.hpp>
#include <Aero/Rhi/WglSurface.hpp>
#include <Aero/Render/OpenGL33RendererBackend.hpp>

#include "../render/SharedRenderPlanFixture.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Rhi;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

struct HiddenWindow final {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    const wchar_t* className =
        L"AeroWglHiddenConformanceWindow";

    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM word,
        LPARAM value) noexcept {
        return DefWindowProcW(window, message, word, value);
    }

    bool Initialize() noexcept {
        instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc = &WindowProcedure;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        if (RegisterClassW(&windowClass) == 0U &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        window = CreateWindowExW(
            0U,
            className,
            L"Aero WGL conformance",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            96,
            96,
            nullptr,
            nullptr,
            instance,
            nullptr);
        return window != nullptr;
    }

    ~HiddenWindow() noexcept {
        if (window != nullptr) {
            static_cast<void>(DestroyWindow(window));
        }
        if (instance != nullptr) {
            static_cast<void>(
                UnregisterClassW(className, instance));
        }
    }
};

PipelineDescriptor MakePipeline() noexcept {
    static const std::uint8_t VertexSource[] =
        "#version 330 core\n"
        "const vec2 p[3]=vec2[3](vec2(-0.8,-0.8),"
        "vec2(0.8,-0.8),vec2(0.0,0.8));\n"
        "void main(){gl_Position=vec4(p[gl_VertexID],0,1);}\n";
    static const std::uint8_t FragmentSource[] =
        "#version 330 core\n"
        "out vec4 color;\n"
        "void main(){color=vec4(0.2,0.6,0.9,1.0);}\n";

    PipelineDescriptor descriptor;
    descriptor.vertexShader.stage = ShaderStage::Vertex;
    descriptor.vertexShader.language = ShaderLanguage::Glsl330;
    descriptor.vertexShader.bytecode = VertexSource;
    descriptor.vertexShader.bytecodeSize =
        static_cast<std::uint32_t>(
            sizeof(VertexSource) - 1U);
    descriptor.vertexShader.entryPoint = StringView("main");
    descriptor.vertexShader.stableId = 15001U;
    descriptor.fragmentShader.stage = ShaderStage::Fragment;
    descriptor.fragmentShader.language = ShaderLanguage::Glsl330;
    descriptor.fragmentShader.bytecode = FragmentSource;
    descriptor.fragmentShader.bytecodeSize =
        static_cast<std::uint32_t>(
            sizeof(FragmentSource) - 1U);
    descriptor.fragmentShader.entryPoint = StringView("main");
    descriptor.fragmentShader.stableId = 15002U;
    return descriptor;
}

NativeSurfaceDescriptor MakeOwnedDescriptor(
    HWND window,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::WglWindow;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Fifo;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.colorFormat =
        GraphicsTextureFormat::Bgra8Unorm;
    descriptor.wgl.window =
        reinterpret_cast<std::uintptr_t>(window);
    descriptor.stableId = 1500U;
    return descriptor;
}

bool TestBorrowedContext(
    WglSurfaceBackend& owned,
    HWND window) noexcept {
    WglSurfaceBackend borrowed;
    NativeSurfaceDescriptor descriptor =
        MakeOwnedDescriptor(window, 64U, 64U);
    descriptor.ownership = SurfaceOwnership::Borrowed;
    descriptor.wgl.deviceContext =
        owned.NativeDeviceContext();
    descriptor.wgl.renderContext =
        owned.NativeRenderContext();
    CHECK(borrowed.CreateSurface(descriptor));
    CHECK(!borrowed.OwnsContext());
    Result<GlContextContract> contract =
        borrowed.ContextContract();
    CHECK(contract);
    CHECK(contract.Value().embeddingMode ==
        GlEmbeddingMode::PreserveAndRestore);
    CHECK(ValidateGlContextContract(contract.Value()));
    Result<GlFunctionTable> functions =
        borrowed.LoadFunctions();
    CHECK(functions);
    CHECK(Aero::Tests::RunBorrowedOpenGLStateConformance(
        functions.Value(),
        contract.Value(),
        Aero::Render::MakeOpenGL33RendererShaderSet()));
    borrowed.DestroySurface();
    CHECK(owned.MakeCurrent());
    return true;
}

bool TestOwnedContextAndRendering(
    SurfaceSession& session,
    WglSurfaceBackend& surface) noexcept {
    Result<GlFunctionTable> functions =
        surface.LoadFunctions();
    CHECK(functions);
    Result<GlContextContract> contract =
        surface.ContextContract();
    CHECK(contract);
    Result<GlCapabilities> capabilities =
        QueryGlCapabilities(
            functions.Value(), contract.Value());
    CHECK(capabilities);
    CHECK(capabilities.Value().coreProfile);
    CHECK(capabilities.Value().majorVersion >= 3U);

    OpenGL33GraphicsBackend backend(
        functions.Value(), contract.Value());
    CHECK(backend.Initialize());
    {
        RhiDevice device(backend);
        CHECK(device.Initialize());
        Result<ResourceHandle> pipeline =
            device.CreatePipeline(MakePipeline());
        CHECK(pipeline);

        Result<SurfaceFrame> frame = session.AcquireFrame();
        CHECK(frame);
        CHECK(frame.Value().target.defaultFramebuffer);

        OpenGL33ExternalRenderTargetDescriptor external;
        external.defaultFramebuffer = true;
        external.contextGeneration =
            contract.Value().generation;
        external.stableId = frame.Value().target.stableId;
        external.texture.width = frame.Value().target.width;
        external.texture.height = frame.Value().target.height;
        external.texture.format =
            frame.Value().target.colorFormat;
        external.texture.usage =
            TextureUsageBit(TextureUsage::RenderTarget);
        Result<ResourceHandle> target =
            ImportOpenGL33ExternalRenderTarget(
                device, backend, external);
        CHECK(target);

        RenderPassDescriptor pass;
        pass.renderArea = {
            0.0F,
            0.0F,
            static_cast<float>(frame.Value().target.width),
            static_cast<float>(frame.Value().target.height)};
        pass.colorAttachmentCount = 1U;
        pass.colorAttachments[0U].target = target.Value();
        pass.colorAttachments[0U].load =
            LoadOperation::Clear;
        pass.colorAttachments[0U].clearColor =
            {0.05F, 0.1F, 0.15F, 1.0F};

        CommandEncoder encoder;
        CHECK(encoder.BeginRenderPass(pass));
        CHECK(encoder.BindPipeline(pipeline.Value()));
        CHECK(encoder.Draw(3U));
        CHECK(encoder.EndRenderPass());
        Result<CommandList> commands = encoder.Finish();
        CHECK(commands);
        Result<FenceValue> fence =
            device.Submit(commands.Value());
        CHECK(fence);
        CHECK(backend.WaitForFence(fence.Value()));
        CHECK(session.Present(frame.Value(), fence.Value()));

        CHECK(device.DestroyResource(
            target.Value(), fence.Value()));
        CHECK(device.DestroyResource(
            pipeline.Value(), fence.Value()));
        CHECK(device.CollectGarbage());

        CHECK(Aero::Tests::RunSharedRenderPlanConformance(
            device,
            backend,
            Aero::Render::MakeOpenGL33RendererShaderSet()));
        Aero::Presentation::RenderPlan plan;
        CHECK(Aero::Tests::BuildSharedRenderPlan(plan));
        Aero::Render::OpenGL33RenderPlanBackend surfaceRenderer(
            device,
            backend,
            session,
            contract.Value().generation);
        CHECK(surfaceRenderer.Initialize());
        CHECK(surfaceRenderer.Submit(plan));
        CHECK(surfaceRenderer.LastSubmitStatistics().
            renderPassCount == 1U);
        CHECK(surfaceRenderer.LastSubmitStatistics().
            drawCallCount == 3U);
        CHECK(backend.WaitForFence(
            surfaceRenderer.LastSubmittedFence()));
        surfaceRenderer.Shutdown();
        CHECK(device.CollectGarbage());
    }
    backend.Shutdown();
    return true;
}

} // namespace

int main() {
    HiddenWindow window;
    if (!window.Initialize()) {
        std::fprintf(stderr,
            "Failed to create hidden WGL test window\n");
        return 1;
    }

    WglSurfaceBackend surface;
    SurfaceSession session(surface);
    NativeSurfaceDescriptor descriptor =
        MakeOwnedDescriptor(window.window, 64U, 64U);
    Result<void> initialized = session.Initialize(descriptor);
    if (!initialized &&
        initialized.GetStatus().code == ErrorCode::Unsupported) {
        std::fprintf(stderr,
            "WGL 3.3 Core unavailable: %s\n",
            initialized.GetStatus().message);
        return 77;
    }
    if (!initialized) {
        std::fprintf(stderr,
            "WGL initialization failed: %s\n",
            initialized.GetStatus().message);
        return 1;
    }

    const GlContextGeneration firstGeneration =
        surface.ContextGeneration();
    if (!TestBorrowedContext(surface, window.window) ||
        !TestOwnedContextAndRendering(session, surface)) {
        return 1;
    }

    if (!session.Resize(80U, 48U)) {
        return 1;
    }
    if (!session.NotifyContextLost()) {
        return 1;
    }
    descriptor.width = 80U;
    descriptor.height = 48U;
    if (!session.Restore(descriptor)) {
        return 1;
    }
    if (surface.ContextGeneration() !=
        firstGeneration + 1U) {
        return 1;
    }
    Result<GlFunctionTable> restoredFunctions =
        surface.LoadFunctions();
    Result<GlContextContract> restoredContract =
        surface.ContextContract();
    if (!restoredFunctions || !restoredContract) {
        return 1;
    }
    OpenGL33GraphicsBackend restoredBackend(
        restoredFunctions.Value(),
        restoredContract.Value());
    if (!restoredBackend.Initialize()) {
        return 1;
    }
    restoredBackend.Shutdown();

    std::printf("WGL surface tests passed\n");
    return 0;
}
