#pragma once

#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Render/Renderer.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdint>

namespace Aero::Tests {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;

struct SharedRenderPlanOptions final {
    RenderImageId image = InvalidRenderImageId;
    RenderMeshId mesh = InvalidRenderMeshId;
    RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId;
};

inline constexpr std::uint64_t SharedRenderPlanHash =
    UINT64_C(0x5AB6629124AFA95F);
inline constexpr std::uint64_t SharedFullRenderPlanHash =
    UINT64_C(0x56E28A80F01A4A3E);

inline SharedRenderPlanOptions
SharedFullRenderPlanOptions() noexcept {
    SharedRenderPlanOptions options;
    options.image = 1U;
    options.mesh = 1U;
    options.glyphRun = 1U;
    return options;
}

class SharedRenderPlanElement final : public FrameworkElement {
public:
    SharedRenderPlanElement(
        TypeId type,
        SharedRenderPlanOptions options) noexcept
        : FrameworkElement(type),
          options_(options) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        return Size{
            std::min(64.0, available.width),
            std::min(64.0, available.height)};
    }

    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        return finalSize;
    }

    Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override {
        Result<void> result = builder.PushClip(
            {0.0, 0.0, RenderSize().width, RenderSize().height});
        if (!result) return result;
        result = builder.FillRect(
            {0.0, 0.0, RenderSize().width, RenderSize().height},
            {0.0F, 0.0F, 1.0F, 1.0F});
        if (!result) return result;
        result = builder.PushOpacity(0.5);
        if (!result) return result;
        result = builder.FillRect(
            {16.0, 16.0, 32.0, 32.0},
            {1.0F, 0.0F, 0.0F, 1.0F});
        if (!result) return result;
        result = builder.PopOpacity();
        if (!result) return result;
        result = builder.FillRoundedRect(
            {4.0, 4.0, 12.0, 12.0},
            {0.0F, 1.0F, 0.0F, 1.0F},
            3.0);
        if (!result) return result;
        if (options_.image != InvalidRenderImageId) {
            result = builder.DrawImage(
                options_.image,
                {8.0, 36.0, 16.0, 16.0},
                {0.0, 0.0, 1.0, 1.0});
            if (!result) return result;
        }
        if (options_.mesh != InvalidRenderMeshId) {
            result = builder.DrawMesh(options_.mesh);
            if (!result) return result;
        }
        if (options_.glyphRun != InvalidRenderGlyphRunId) {
            result = builder.DrawGlyphRun(
                options_.glyphRun,
                {1.0F, 1.0F, 1.0F, 1.0F});
            if (!result) return result;
        }
        return builder.PopClip();
    }

private:
    SharedRenderPlanOptions options_;
};

inline bool BuildSharedRenderPlan(
    RenderPlan& output,
    SharedRenderPlanOptions options = {}) noexcept {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore behaviors(types);
    MetadataRegistrationTypes registration(types, behaviors);
    DependencyPropertyRegistry properties(types, behaviors);
    Dispatcher dispatcher;
    ObjectServicesScope services(dispatcher, properties);
    const StringView nameSpace("urn:aero-render-conformance");
    const TypeId objectType =
        MakeTypeId(nameSpace, StringView("Object"));
    const TypeId elementType =
        MakeTypeId(nameSpace, StringView("SharedElement"));
    if (!registration.TryRegisterType(TypeRegistration::Object(
            nameSpace,
            StringView("Object"),
            InvalidTypeId,
            TypeFlags::None,
            nullptr)) ||
        !registration.TryRegisterType(TypeRegistration::Object(
            nameSpace,
            StringView("SharedElement"),
            objectType,
            TypeFlags::None,
            nullptr)) ||
        !types.Freeze() ||
        !properties.Freeze()) {
        return false;
    }

    EffectiveValueEngine values(dispatcher, properties);
    ObjectTree tree(dispatcher, values);
    LayoutManager layout(dispatcher);
    NullRenderBackend verifier;
    RenderManager renderer(dispatcher, verifier);
    SharedRenderPlanElement root(elementType, options);
    if (!values.Initialize() ||
        !tree.Initialize() ||
        !layout.Initialize() ||
        !renderer.Initialize() ||
        !tree.SetRoot(&root) ||
        !layout.SetRoot(&root, {64.0, 64.0}) ||
        !renderer.SetRoot(&root) ||
        !dispatcher.RunFramePhase(
            DispatcherFramePhase::Layout) ||
        !dispatcher.RunFramePhase(
            DispatcherFramePhase::RenderCommit)) {
        return false;
    }
    output = renderer.CurrentPlan();
    const bool isRectanglePlan =
        options.image == InvalidRenderImageId &&
        options.mesh == InvalidRenderMeshId &&
        options.glyphRun == InvalidRenderGlyphRunId;
    const bool isFullPlan =
        options.image == 1U &&
        options.mesh == 1U &&
        options.glyphRun == 1U;
    const bool valid =
        output.Nodes().Size() == 1U &&
        output.Commands().Size() != 0U &&
        (!isRectanglePlan ||
         output.StableHash() == SharedRenderPlanHash) &&
        (!isFullPlan ||
         output.StableHash() == SharedFullRenderPlanHash);
    static_cast<void>(renderer.SetRoot(nullptr));
    static_cast<void>(layout.SetRoot(nullptr, {0.0, 0.0}));
    static_cast<void>(tree.SetRoot(nullptr));
    static_cast<void>(values.DetachObject(root));
    return valid;
}

inline bool ValidateSharedRenderPlanPixels(
    Base::Span<const std::uint8_t> pixels,
    std::uint32_t rowPitch) noexcept {
    if (rowPitch < 64U * 4U ||
        pixels.Size() < rowPitch * 64U) {
        return false;
    }
    const auto pixel = [&](std::uint32_t x, std::uint32_t y)
        noexcept -> const std::uint8_t* {
        return pixels.Data() + y * rowPitch + x * 4U;
    };
    const std::uint8_t* background = pixel(2U, 2U);
    const std::uint8_t* rounded = pixel(10U, 10U);
    const std::uint8_t* blended = pixel(32U, 32U);
    return
        background[0U] == 255U &&
        background[1U] == 0U &&
        background[2U] == 0U &&
        background[3U] == 255U &&
        rounded[0U] == 0U &&
        rounded[1U] == 255U &&
        rounded[2U] == 0U &&
        rounded[3U] == 255U &&
        blended[0U] >= 126U && blended[0U] <= 129U &&
        blended[1U] == 0U &&
        blended[2U] >= 126U && blended[2U] <= 129U &&
        blended[3U] == 255U;
}

template <typename GraphicsBackend>
bool RunSharedRenderPlanConformance(
    Rhi::RhiDevice& device,
    GraphicsBackend& backend,
    const Render::RendererShaderSet& shaders) noexcept {
    const SharedRenderPlanOptions options =
        SharedFullRenderPlanOptions();
    const auto fail = [](const char* message) noexcept {
        std::fprintf(
            stderr,
            "Shared RenderPlan conformance failed: %s\n",
            message);
        return false;
    };
    RenderPlan plan;
    if (!BuildSharedRenderPlan(plan, options)) {
        return fail("plan construction");
    }

    Rhi::TextureResourceDescriptor textureDescriptor;
    textureDescriptor.width = 64U;
    textureDescriptor.height = 64U;
    textureDescriptor.format =
        Rhi::GraphicsTextureFormat::Bgra8Unorm;
    textureDescriptor.usage =
        Rhi::TextureUsageBit(Rhi::TextureUsage::RenderTarget) |
        Rhi::TextureUsageBit(Rhi::TextureUsage::CopySource);
    Result<Rhi::ResourceHandle> target =
        device.CreateRenderTarget(textureDescriptor);
    if (!target) {
        return fail("render target creation");
    }

    Rhi::TextureResourceDescriptor imageDescriptor;
    imageDescriptor.width = 2U;
    imageDescriptor.height = 2U;
    imageDescriptor.format =
        Rhi::GraphicsTextureFormat::Rgba8Unorm;
    imageDescriptor.usage =
        Rhi::TextureUsageBit(Rhi::TextureUsage::Sampled) |
        Rhi::TextureUsageBit(Rhi::TextureUsage::CopyDestination);
    Result<Rhi::ResourceHandle> image =
        device.CreateTexture(imageDescriptor);
    Rhi::SamplerDescriptor samplerDescriptor;
    samplerDescriptor.minFilter = Rhi::FilterMode::Nearest;
    samplerDescriptor.magFilter = Rhi::FilterMode::Nearest;
    samplerDescriptor.mipFilter = Rhi::FilterMode::Nearest;
    Result<Rhi::ResourceHandle> sampler =
        device.CreateSampler(samplerDescriptor);

    struct MeshVertex final {
        float x;
        float y;
        float red;
        float green;
        float blue;
        float alpha;
    };
    static constexpr MeshVertex MeshVertices[] = {
        {40.0F, 4.0F, 1.0F, 1.0F, 0.0F, 1.0F},
        {56.0F, 4.0F, 1.0F, 1.0F, 0.0F, 1.0F},
        {48.0F, 20.0F, 1.0F, 1.0F, 0.0F, 1.0F}};
    static constexpr std::uint16_t MeshIndices[] = {
        0U, 1U, 2U};
    Rhi::BufferDescriptor meshVertexDescriptor;
    meshVertexDescriptor.sizeBytes = sizeof(MeshVertices);
    meshVertexDescriptor.usage = Rhi::BufferUsage::Vertex;
    Result<Rhi::ResourceHandle> meshVertex =
        device.CreateBuffer(meshVertexDescriptor);
    Rhi::BufferDescriptor meshIndexDescriptor;
    meshIndexDescriptor.sizeBytes = sizeof(MeshIndices);
    meshIndexDescriptor.usage = Rhi::BufferUsage::Index;
    Result<Rhi::ResourceHandle> meshIndex =
        device.CreateBuffer(meshIndexDescriptor);

    struct GlyphVertex final {
        float x;
        float y;
        float u;
        float v;
    };
    static constexpr GlyphVertex GlyphVertices[] = {
        {32.0F, 36.0F, 0.5F, 0.5F},
        {48.0F, 36.0F, 0.5F, 0.5F},
        {40.0F, 52.0F, 0.5F, 0.5F}};
    static constexpr std::uint16_t GlyphIndices[] = {
        0U, 1U, 2U};
    Rhi::BufferDescriptor glyphVertexDescriptor;
    glyphVertexDescriptor.sizeBytes = sizeof(GlyphVertices);
    glyphVertexDescriptor.usage = Rhi::BufferUsage::Vertex;
    Result<Rhi::ResourceHandle> glyphVertex =
        device.CreateBuffer(glyphVertexDescriptor);
    Rhi::BufferDescriptor glyphIndexDescriptor;
    glyphIndexDescriptor.sizeBytes = sizeof(GlyphIndices);
    glyphIndexDescriptor.usage = Rhi::BufferUsage::Index;
    Result<Rhi::ResourceHandle> glyphIndex =
        device.CreateBuffer(glyphIndexDescriptor);
    Rhi::TextureResourceDescriptor atlasDescriptor;
    atlasDescriptor.width = 1U;
    atlasDescriptor.height = 1U;
    atlasDescriptor.format =
        Rhi::GraphicsTextureFormat::R8Unorm;
    atlasDescriptor.usage =
        Rhi::TextureUsageBit(Rhi::TextureUsage::Sampled) |
        Rhi::TextureUsageBit(Rhi::TextureUsage::CopyDestination);
    Result<Rhi::ResourceHandle> atlas =
        device.CreateTexture(atlasDescriptor);
    if (!image || !sampler || !meshVertex || !meshIndex ||
        !glyphVertex || !glyphIndex || !atlas) {
        return fail("sample resource creation");
    }

    static constexpr std::uint8_t ImagePixels[] = {
        255U, 0U, 0U, 255U,
        0U, 255U, 0U, 255U,
        0U, 0U, 255U, 255U,
        255U, 255U, 255U, 255U};
    static constexpr std::uint8_t AtlasPixels[] = {128U};
    Rhi::CommandEncoder uploadEncoder;
    Result<void> uploaded = uploadEncoder.UploadTexture(
        image.Value(),
        {0U, 0U, 2U, 2U, 0U, 0U, 8U},
        Span<const std::uint8_t>(
            ImagePixels, sizeof(ImagePixels)));
    if (uploaded) {
        const auto* bytes =
            reinterpret_cast<const std::uint8_t*>(MeshVertices);
        uploaded = uploadEncoder.UploadBuffer(
            meshVertex.Value(),
            0U,
            {bytes, sizeof(MeshVertices)});
    }
    if (uploaded) {
        const auto* bytes =
            reinterpret_cast<const std::uint8_t*>(MeshIndices);
        uploaded = uploadEncoder.UploadBuffer(
            meshIndex.Value(),
            0U,
            {bytes, sizeof(MeshIndices)});
    }
    if (uploaded) {
        const auto* bytes =
            reinterpret_cast<const std::uint8_t*>(GlyphVertices);
        uploaded = uploadEncoder.UploadBuffer(
            glyphVertex.Value(),
            0U,
            {bytes, sizeof(GlyphVertices)});
    }
    if (uploaded) {
        const auto* bytes =
            reinterpret_cast<const std::uint8_t*>(GlyphIndices);
        uploaded = uploadEncoder.UploadBuffer(
            glyphIndex.Value(),
            0U,
            {bytes, sizeof(GlyphIndices)});
    }
    if (uploaded) {
        uploaded = uploadEncoder.UploadTexture(
            atlas.Value(),
            {0U, 0U, 1U, 1U, 0U, 0U, 1U},
            Span<const std::uint8_t>(
                AtlasPixels, sizeof(AtlasPixels)));
    }
    Result<Rhi::CommandList> uploadCommands =
        uploaded
        ? uploadEncoder.Finish()
        : Result<Rhi::CommandList>(uploaded.GetStatus());
    Result<Rhi::FenceValue> uploadFence =
        uploadCommands
        ? device.Submit(uploadCommands.Value())
        : Result<Rhi::FenceValue>(
            uploadCommands.GetStatus());
    if (!uploadFence ||
        !backend.WaitForFence(uploadFence.Value())) {
        return fail("sample resource upload");
    }

    Render::Renderer renderer(device, shaders);
    if (!renderer.Initialize() ||
        !renderer.RegisterImage(
            options.image, image.Value(), sampler.Value()) ||
        !renderer.RegisterMesh(
            options.mesh,
            meshVertex.Value(),
            meshIndex.Value(),
            3U,
            Rhi::IndexType::UInt16) ||
        !renderer.RegisterGlyphRun(
            options.glyphRun,
            glyphVertex.Value(),
            glyphIndex.Value(),
            3U,
            atlas.Value(),
            sampler.Value(),
            Rhi::IndexType::UInt16)) {
        return fail("renderer initialization or resource registration");
    }
    Result<Rhi::CommandList> commands =
        renderer.Record(plan, {target.Value(), 64U, 64U});
    if (!commands) {
        return fail("RenderPlan recording");
    }
    Result<Rhi::FenceValue> fence =
        device.Submit(commands.Value());
    if (!fence || !backend.WaitForFence(fence.Value())) {
        return fail("RenderPlan submission");
    }

    std::uint8_t pixels[64U * 64U * 4U]{};
    if (!backend.ReadbackTexture(
            target.Value(),
            Span<std::uint8_t>(pixels, sizeof(pixels)),
            64U * 4U) ||
        !ValidateSharedRenderPlanPixels(
            Span<const std::uint8_t>(pixels, sizeof(pixels)),
            64U * 4U)) {
        return fail("rectangle pixel validation");
    }
    const auto pixel = [&](std::uint32_t x, std::uint32_t y)
        noexcept -> const std::uint8_t* {
        return pixels + ((y * 64U + x) * 4U);
    };
    const std::uint8_t* imagePixel = pixel(12U, 40U);
    const std::uint8_t* meshPixel = pixel(48U, 8U);
    const std::uint8_t* glyphPixel = pixel(40U, 40U);
    if (imagePixel[0U] != 0U ||
        imagePixel[1U] != 0U ||
        imagePixel[2U] != 255U ||
        imagePixel[3U] != 255U ||
        meshPixel[0U] != 0U ||
        meshPixel[1U] != 255U ||
        meshPixel[2U] != 255U ||
        meshPixel[3U] != 255U ||
        glyphPixel[0U] < 190U ||
        glyphPixel[0U] > 193U ||
        glyphPixel[1U] < 126U ||
        glyphPixel[1U] > 129U ||
        glyphPixel[2U] < 190U ||
        glyphPixel[2U] > 193U ||
        glyphPixel[3U] != 255U) {
        std::fprintf(
            stderr,
            "Shared pixels image=%u,%u,%u,%u mesh=%u,%u,%u,%u glyph=%u,%u,%u,%u\n",
            imagePixel[0U], imagePixel[1U],
            imagePixel[2U], imagePixel[3U],
            meshPixel[0U], meshPixel[1U],
            meshPixel[2U], meshPixel[3U],
            glyphPixel[0U], glyphPixel[1U],
            glyphPixel[2U], glyphPixel[3U]);
        return fail("image, mesh, or glyph pixel validation");
    }

    renderer.Shutdown();
    const Rhi::FenceValue retireFence = fence.Value();
    return
        device.DestroyResource(target.Value(), retireFence) &&
        device.DestroyResource(image.Value(), retireFence) &&
        device.DestroyResource(sampler.Value(), retireFence) &&
        device.DestroyResource(meshVertex.Value(), retireFence) &&
        device.DestroyResource(meshIndex.Value(), retireFence) &&
        device.DestroyResource(glyphVertex.Value(), retireFence) &&
        device.DestroyResource(glyphIndex.Value(), retireFence) &&
        device.DestroyResource(atlas.Value(), retireFence) &&
        device.CollectGarbage();
}

inline bool RunBorrowedOpenGLStateConformance(
    const Rhi::GlFunctionTable& functions,
    const Rhi::GlContextContract& contract,
    const Render::RendererShaderSet& shaders) noexcept {
    functions.viewport(7, 8, 31, 32);
    functions.enable(Rhi::GlConstant::ScissorTest);
    functions.scissor(3, 4, 20, 21);
    functions.activeTexture(Rhi::GlConstant::Texture0 + 3U);
    functions.pixelStorei(Rhi::GlConstant::UnpackAlignment, 2);

    Rhi::GlInt expectedViewport[4]{};
    Rhi::GlInt expectedScissor[4]{};
    Rhi::GlInt expectedActiveTexture = 0;
    Rhi::GlInt expectedUnpackAlignment = 0;
    functions.getIntegerv(
        Rhi::GlConstant::Viewport, expectedViewport);
    functions.getIntegerv(
        Rhi::GlConstant::ScissorBox, expectedScissor);
    functions.getIntegerv(
        Rhi::GlConstant::ActiveTexture,
        &expectedActiveTexture);
    functions.getIntegerv(
        Rhi::GlConstant::UnpackAlignment,
        &expectedUnpackAlignment);
    const Rhi::GlBoolean expectedScissorEnabled =
        functions.isEnabled(Rhi::GlConstant::ScissorTest);

    Rhi::OpenGL33BackendOptions options;
    options.embeddingMode =
        Rhi::GlEmbeddingMode::PreserveAndRestore;
    options.checkErrors = true;
    Rhi::OpenGL33GraphicsBackend backend(
        functions, contract, options);
    if (!backend.Initialize()) {
        return false;
    }
    {
        Rhi::RhiDevice device(backend);
        if (!device.Initialize() ||
            !RunSharedRenderPlanConformance(
                device, backend, shaders) ||
            device.LiveResourceCount() != 0U) {
            return false;
        }
    }
    backend.Shutdown();

    Rhi::GlInt actualViewport[4]{};
    Rhi::GlInt actualScissor[4]{};
    Rhi::GlInt actualActiveTexture = 0;
    Rhi::GlInt actualUnpackAlignment = 0;
    functions.getIntegerv(
        Rhi::GlConstant::Viewport, actualViewport);
    functions.getIntegerv(
        Rhi::GlConstant::ScissorBox, actualScissor);
    functions.getIntegerv(
        Rhi::GlConstant::ActiveTexture,
        &actualActiveTexture);
    functions.getIntegerv(
        Rhi::GlConstant::UnpackAlignment,
        &actualUnpackAlignment);
    const Rhi::GlBoolean actualScissorEnabled =
        functions.isEnabled(Rhi::GlConstant::ScissorTest);
    for (std::uint32_t index = 0U; index < 4U; ++index) {
        if (actualViewport[index] != expectedViewport[index] ||
            actualScissor[index] != expectedScissor[index]) {
            return false;
        }
    }
    return actualActiveTexture == expectedActiveTexture &&
        actualUnpackAlignment == expectedUnpackAlignment &&
        actualScissorEnabled == expectedScissorEnabled;
}

inline bool PixelsWithinTolerance(
    Base::Span<const std::uint8_t> left,
    Base::Span<const std::uint8_t> right,
    std::uint8_t tolerance,
    std::uint32_t& maximumDelta) noexcept {
    if (left.Size() != right.Size()) {
        return false;
    }
    maximumDelta = 0U;
    for (std::uint32_t index = 0U;
         index < left.Size();
         ++index) {
        const std::uint32_t leftValue = left[index];
        const std::uint32_t rightValue = right[index];
        const std::uint32_t delta = leftValue > rightValue
            ? leftValue - rightValue
            : rightValue - leftValue;
        maximumDelta = std::max(maximumDelta, delta);
        if (delta > tolerance) {
            return false;
        }
    }
    return true;
}

} // namespace Aero::Tests
