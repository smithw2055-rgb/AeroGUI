#include <Aero/Rhi/GlxSurface.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>

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
        static_cast<std::uint32_t>(sizeof(VertexSource) - 1U);
    descriptor.vertexShader.entryPoint = StringView("main");
    descriptor.vertexShader.stableId = 16001U;
    descriptor.fragmentShader.stage = ShaderStage::Fragment;
    descriptor.fragmentShader.language = ShaderLanguage::Glsl330;
    descriptor.fragmentShader.bytecode = FragmentSource;
    descriptor.fragmentShader.bytecodeSize =
        static_cast<std::uint32_t>(sizeof(FragmentSource) - 1U);
    descriptor.fragmentShader.entryPoint = StringView("main");
    descriptor.fragmentShader.stableId = 16002U;
    return descriptor;
}

NativeSurfaceDescriptor MakeOwnedDescriptor(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::GlxWindow;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Fifo;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.glx.screen = -1;
    descriptor.stableId = 1600U;
    return descriptor;
}

bool TestBorrowedContext(GlxSurfaceBackend& owned) noexcept {
    GlxSurfaceBackend borrowed;
    NativeSurfaceDescriptor descriptor =
        MakeOwnedDescriptor(64U, 64U);
    descriptor.ownership = SurfaceOwnership::Borrowed;
    descriptor.glx.display = owned.NativeDisplay();
    descriptor.glx.drawable = owned.NativeDrawable();
    descriptor.glx.context = owned.NativeContext();
    CHECK(borrowed.CreateSurface(descriptor));
    CHECK(!borrowed.OwnsDisplay());
    CHECK(!borrowed.OwnsDrawable());
    CHECK(!borrowed.OwnsContext());
    Result<GlContextContract> contract = borrowed.ContextContract();
    CHECK(contract);
    CHECK(contract.Value().embeddingMode ==
        GlEmbeddingMode::PreserveAndRestore);
    CHECK(ValidateGlContextContract(contract.Value()));
    CHECK(borrowed.LoadFunctions());
    borrowed.DestroySurface();
    CHECK(owned.MakeCurrent());
    return true;
}

bool TestOwnedContextAndRendering(
    SurfaceSession& session,
    GlxSurfaceBackend& surface) noexcept {
    Result<GlFunctionTable> functions = surface.LoadFunctions();
    CHECK(functions);
    Result<GlContextContract> contract = surface.ContextContract();
    CHECK(contract);
    Result<GlCapabilities> capabilities =
        QueryGlCapabilities(functions.Value(), contract.Value());
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
        external.contextGeneration = contract.Value().generation;
        external.stableId = frame.Value().target.stableId;
        external.texture.width = frame.Value().target.width;
        external.texture.height = frame.Value().target.height;
        external.texture.format = frame.Value().target.colorFormat;
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
        pass.colorAttachments[0U].load = LoadOperation::Clear;
        pass.colorAttachments[0U].clearColor =
            {0.05F, 0.1F, 0.15F, 1.0F};

        CommandEncoder encoder;
        CHECK(encoder.BeginRenderPass(pass));
        CHECK(encoder.BindPipeline(pipeline.Value()));
        CHECK(encoder.Draw(3U));
        CHECK(encoder.EndRenderPass());
        Result<CommandList> commands = encoder.Finish();
        CHECK(commands);
        Result<FenceValue> fence = device.Submit(commands.Value());
        CHECK(fence);
        CHECK(backend.WaitForFence(fence.Value()));
        CHECK(session.Present(frame.Value(), fence.Value()));

        CHECK(device.DestroyResource(target.Value(), fence.Value()));
        CHECK(device.DestroyResource(pipeline.Value(), fence.Value()));
        CHECK(device.CollectGarbage());
    }
    backend.Shutdown();
    return true;
}

} // namespace

int main() {
    GlxSurfaceBackend surface;
    SurfaceSession session(surface);
    NativeSurfaceDescriptor descriptor =
        MakeOwnedDescriptor(64U, 64U);
    Result<void> initialized = session.Initialize(descriptor);
    if (!initialized &&
        initialized.GetStatus().code == ErrorCode::Unsupported) {
        std::fprintf(stderr, "GLX 3.3 Core unavailable: %s\n",
            initialized.GetStatus().message);
        return 77;
    }
    if (!initialized) {
        std::fprintf(stderr, "GLX initialization failed: %s\n",
            initialized.GetStatus().message);
        return 1;
    }
    if (!surface.OwnsDisplay() ||
        !surface.OwnsDrawable() ||
        !surface.OwnsContext()) {
        return 1;
    }

    const GlContextGeneration firstGeneration =
        surface.ContextGeneration();
    if (!TestBorrowedContext(surface) ||
        !TestOwnedContextAndRendering(session, surface)) {
        return 1;
    }

    if (!session.Resize(80U, 48U) ||
        !session.NotifyContextLost()) {
        return 1;
    }
    descriptor.width = 80U;
    descriptor.height = 48U;
    if (!session.Restore(descriptor) ||
        surface.ContextGeneration() != firstGeneration + 1U) {
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
        restoredFunctions.Value(), restoredContract.Value());
    if (!restoredBackend.Initialize()) {
        return 1;
    }
    restoredBackend.Shutdown();

    std::printf("GLX surface tests passed\n");
    return 0;
}
