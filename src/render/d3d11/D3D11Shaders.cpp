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


Graphics::ShaderDescriptor Shader(
    Graphics::ShaderStage stage,
    const std::uint8_t* bytecode,
    std::uint32_t bytecodeSize,
    std::uint64_t stableId,
    Base::StringView entryPoint) noexcept {
    Graphics::ShaderDescriptor descriptor;
    descriptor.stage = stage;
    descriptor.language = Graphics::ShaderLanguage::Dxbc;
    descriptor.bytecode = bytecode;
    descriptor.bytecodeSize = bytecodeSize;
    descriptor.entryPoint = entryPoint;
    descriptor.stableId = stableId;
    return descriptor;
}

} // namespace

FrameShaderSet MakeD3D11FrameShaderSet() noexcept {
    FrameShaderSet shaders;
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


} // namespace Aero::Render
