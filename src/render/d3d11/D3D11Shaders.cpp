#include "D3D11Shaders.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <new>


#include "AeroD3D11RenderFramePixelShader.hpp"
#include "AeroD3D11RenderFrameVertexShader.hpp"
#include "AeroD3D11RenderFrameImagePixelShader.hpp"
#include "AeroD3D11RenderFrameImageVertexShader.hpp"
#include "AeroD3D11RenderFrameMaskPixelShader.hpp"
#include "AeroD3D11RenderFrameMaskVertexShader.hpp"
#include "AeroD3D11RenderFrameEffectPixelShader.hpp"
#include "AeroD3D11RenderFrameEffectVertexShader.hpp"
#include "AeroD3D11RenderFrameMeshPixelShader.hpp"
#include "AeroD3D11RenderFrameMeshVertexShader.hpp"
#include "AeroD3D11RenderFrameGlyphPixelShader.hpp"
#include "AeroD3D11RenderFrameGlyphVertexShader.hpp"

namespace Aero::Render {
namespace {


Graphics::NativeShaderProgram Shader(
    Graphics::ShaderStage stage,
    const std::uint8_t* bytecode,
    std::uint32_t bytecodeSize,
    std::uint64_t stableId,
    Base::StringView entryPoint) noexcept {
    Graphics::NativeShaderProgram descriptor;
    descriptor.stage = stage;
    descriptor.language = Graphics::ShaderLanguage::Dxbc;
    descriptor.bytecode = bytecode;
    descriptor.bytecodeSize = bytecodeSize;
    descriptor.entryPoint = entryPoint;
    descriptor.stableId = stableId;
    return descriptor;
}

} // namespace

BackendShaderCatalog MakeD3D11BackendShaderCatalog() noexcept {
    BackendShaderCatalog shaders;
    shaders.rectangleVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameVertexShader)),
        UINT64_C(0xD3111001), Base::StringView("vs_main"));
    shaders.rectangleFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFramePixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFramePixelShader)),
        UINT64_C(0xD3111002), Base::StringView("ps_main"));
    shaders.imageVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameImageVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameImageVertexShader)),
        UINT64_C(0xD3111011), Base::StringView("vs_main"));
    shaders.imageFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameImagePixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameImagePixelShader)),
        UINT64_C(0xD3111012), Base::StringView("ps_main"));
    shaders.maskVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameMaskVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMaskVertexShader)),
        UINT64_C(0xD3111013), Base::StringView("vs_main"));
    shaders.maskFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameMaskPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMaskPixelShader)),
        UINT64_C(0xD3111014), Base::StringView("ps_main"));
    shaders.effectVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameEffectVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameEffectVertexShader)),
        UINT64_C(0xD3111015), Base::StringView("vs_main"));
    shaders.effectFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameEffectPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameEffectPixelShader)),
        UINT64_C(0xD3111016), Base::StringView("ps_main"));
    shaders.meshVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameMeshVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMeshVertexShader)),
        UINT64_C(0xD3111021), Base::StringView("vs_main"));
    shaders.meshFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameMeshPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMeshPixelShader)),
        UINT64_C(0xD3111022), Base::StringView("ps_main"));
    shaders.glyphVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameGlyphVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameGlyphVertexShader)),
        UINT64_C(0xD3111031), Base::StringView("vs_main"));
    shaders.glyphFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameGlyphPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameGlyphPixelShader)),
        UINT64_C(0xD3111032), Base::StringView("ps_main"));
    shaders.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
    return shaders;
}

static Graphics::NativePipelineState MakeUiNativePipelineState(
    const BackendShaderCatalog& shaders,
    ::Aero::Render::UiPipelineKey key) noexcept {
    Graphics::NativePipelineState descriptor;
    descriptor.vertexLayout.bufferCount = 1U;
    descriptor.vertexLayout.attributeCount = 1U;
    descriptor.vertexLayout.buffers[0].stride = 8U;
    descriptor.vertexLayout.attributes[0].location = 0U;
    descriptor.vertexLayout.attributes[0].bufferSlot = 0U;
    descriptor.vertexLayout.attributes[0].format = Graphics::VertexFormat::Float2;
    descriptor.topology = Graphics::PrimitiveTopology::TriangleStrip;
    descriptor.blend.enabled = key.blend != ::Aero::Render::UiBlendMode::Opaque;
    descriptor.blend.color.source = Graphics::BlendFactor::SourceAlpha;
    descriptor.blend.color.destination = Graphics::BlendFactor::OneMinusSourceAlpha;
    descriptor.blend.alpha.source = Graphics::BlendFactor::One;
    descriptor.blend.alpha.destination = Graphics::BlendFactor::OneMinusSourceAlpha;
    descriptor.colorFormat = shaders.colorFormat;
    descriptor.raster.scissorEnabled = true;

    switch (key.shader) {
    case ::Aero::Render::UiShader::Image:
        descriptor.vertexShader = shaders.imageVertex;
        descriptor.fragmentShader = shaders.imageFragment;
        break;
    case ::Aero::Render::UiShader::Mask:
        descriptor.vertexShader = shaders.maskVertex;
        descriptor.fragmentShader = shaders.maskFragment;
        break;
    case ::Aero::Render::UiShader::Effect:
        descriptor.vertexShader = shaders.effectVertex;
        descriptor.fragmentShader = shaders.effectFragment;
        break;
    case ::Aero::Render::UiShader::Mesh:
        descriptor.vertexShader = shaders.meshVertex;
        descriptor.fragmentShader = shaders.meshFragment;
        descriptor.vertexLayout.buffers[0].stride = 28U;
        descriptor.vertexLayout.attributeCount = 3U;
        descriptor.vertexLayout.attributes[1] = {1U, 0U, Graphics::VertexFormat::Float4, 8U};
        descriptor.vertexLayout.attributes[2] = {2U, 0U, Graphics::VertexFormat::Float, 24U};
        descriptor.topology = Graphics::PrimitiveTopology::TriangleList;
        break;
    case ::Aero::Render::UiShader::Glyph:
        descriptor.vertexShader = shaders.glyphVertex;
        descriptor.fragmentShader = shaders.glyphFragment;
        descriptor.vertexLayout.buffers[0].stride = 16U;
        descriptor.vertexLayout.attributeCount = 2U;
        descriptor.vertexLayout.attributes[1] = {1U, 0U, Graphics::VertexFormat::Float2, 8U};
        descriptor.topology = Graphics::PrimitiveTopology::TriangleList;
        break;
    case ::Aero::Render::UiShader::Rectangle:
    default:
        descriptor.vertexShader = shaders.rectangleVertex;
        descriptor.fragmentShader = shaders.rectangleFragment;
        break;
    }

    switch (key.blend) {
    case ::Aero::Render::UiBlendMode::Multiply:
        descriptor.blend.color.source = Graphics::BlendFactor::DestinationColor;
        descriptor.blend.color.destination = Graphics::BlendFactor::Zero;
        break;
    case ::Aero::Render::UiBlendMode::Screen:
        descriptor.blend.color.source = Graphics::BlendFactor::One;
        descriptor.blend.color.destination = Graphics::BlendFactor::OneMinusSourceColor;
        break;
    case ::Aero::Render::UiBlendMode::Additive:
        descriptor.blend.color.source = Graphics::BlendFactor::SourceAlpha;
        descriptor.blend.color.destination = Graphics::BlendFactor::One;
        break;
    case ::Aero::Render::UiBlendMode::Mask:
        descriptor.blend.color.source = Graphics::BlendFactor::Zero;
        descriptor.blend.color.destination = Graphics::BlendFactor::SourceAlpha;
        descriptor.blend.alpha.source = Graphics::BlendFactor::Zero;
        descriptor.blend.alpha.destination = Graphics::BlendFactor::SourceAlpha;
        break;
    case ::Aero::Render::UiBlendMode::Opaque:
        descriptor.blend.enabled = false;
        break;
    case ::Aero::Render::UiBlendMode::Normal:
    default:
        break;
    }
    return descriptor;
}

Graphics::NativePipelineState MakeD3D11UiPipeline(
    ::Aero::Render::UiPipelineKey key) noexcept {
    return MakeUiNativePipelineState(MakeD3D11BackendShaderCatalog(), key);
}


} // namespace Aero::Render
