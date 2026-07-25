#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Rhi {

constexpr std::uint32_t GlFunctionTableAbiVersion = 1U;
constexpr std::uint32_t GlContextContractAbiVersion = 1U;

using GlBoolean = std::uint8_t;
using GlBitfield = std::uint32_t;
using GlEnum = std::uint32_t;
using GlInt = std::int32_t;
using GlSize = std::int32_t;
using GlUInt = std::uint32_t;
using GlIntPtr = std::intptr_t;
using GlSizePtr = std::intptr_t;
using GlFloat = float;
using GlChar = char;
using GlSync = void*;
using GlThreadToken = std::uintptr_t;
using GlContextGeneration = std::uint64_t;

#if defined(_WIN32)
#define AERO_GL_CALL __stdcall
#else
#define AERO_GL_CALL
#endif

using GlProcAddress = void (AERO_GL_CALL*)();
using GlProcAddressResolver =
    GlProcAddress (*)(void* userData, const char* name) noexcept;
using GlContextIsCurrent =
    bool (*)(void* userData, const void* contextHandle) noexcept;
using GlCurrentThreadToken =
    GlThreadToken (*)(void* userData) noexcept;

namespace GlConstant {

constexpr GlBoolean False = 0U;
constexpr GlBoolean True = 1U;

constexpr GlEnum Version = 0x1F02U;
constexpr GlEnum Vendor = 0x1F00U;
constexpr GlEnum Renderer = 0x1F01U;
constexpr GlEnum Extensions = 0x1F03U;
constexpr GlEnum MajorVersion = 0x821BU;
constexpr GlEnum MinorVersion = 0x821CU;
constexpr GlEnum NumExtensions = 0x821DU;
constexpr GlEnum ContextFlags = 0x821EU;
constexpr GlEnum ContextProfileMask = 0x9126U;
constexpr GlInt ContextCoreProfileBit = 0x00000001;
constexpr GlInt ContextCompatibilityProfileBit = 0x00000002;
constexpr GlInt ContextFlagDebugBit = 0x00000002;

constexpr GlEnum MaxTextureSize = 0x0D33U;
constexpr GlEnum MaxCombinedTextureImageUnits = 0x8B4DU;
constexpr GlEnum MaxVertexAttribs = 0x8869U;
constexpr GlEnum MaxUniformBlockSize = 0x8A30U;
constexpr GlEnum UniformBufferOffsetAlignment = 0x8A34U;
constexpr GlEnum MaxSamples = 0x8D57U;
constexpr GlEnum MaxColorAttachments = 0x8CDFU;

constexpr GlEnum CurrentProgram = 0x8B8DU;
constexpr GlEnum VertexArrayBinding = 0x85B5U;
constexpr GlEnum ArrayBuffer = 0x8892U;
constexpr GlEnum ArrayBufferBinding = 0x8894U;
constexpr GlEnum ElementArrayBuffer = 0x8893U;
constexpr GlEnum ElementArrayBufferBinding = 0x8895U;
constexpr GlEnum UniformBuffer = 0x8A11U;
constexpr GlEnum UniformBufferBinding = 0x8A28U;
constexpr GlEnum DrawFramebuffer = 0x8CA9U;
constexpr GlEnum DrawFramebufferBinding = 0x8CA6U;
constexpr GlEnum Framebuffer = 0x8D40U;
constexpr GlEnum FramebufferComplete = 0x8CD5U;
constexpr GlEnum ColorAttachment0 = 0x8CE0U;

constexpr GlEnum Viewport = 0x0BA2U;
constexpr GlEnum ScissorTest = 0x0C11U;
constexpr GlEnum ScissorBox = 0x0C10U;
constexpr GlEnum Blend = 0x0BE2U;
constexpr GlEnum BlendEquationRgb = 0x8009U;
constexpr GlEnum BlendEquationAlpha = 0x883DU;
constexpr GlEnum BlendSrcRgb = 0x80C9U;
constexpr GlEnum BlendDstRgb = 0x80C8U;
constexpr GlEnum BlendSrcAlpha = 0x80CBU;
constexpr GlEnum BlendDstAlpha = 0x80CAU;
constexpr GlEnum ColorWritemask = 0x0C23U;
constexpr GlEnum DepthTest = 0x0B71U;
constexpr GlEnum DepthFunc = 0x0B74U;
constexpr GlEnum DepthWritemask = 0x0B72U;
constexpr GlEnum StencilTest = 0x0B90U;
constexpr GlEnum StencilFunc = 0x0B92U;
constexpr GlEnum StencilValueMask = 0x0B93U;
constexpr GlEnum StencilRef = 0x0B97U;
constexpr GlEnum StencilFail = 0x0B94U;
constexpr GlEnum StencilPassDepthFail = 0x0B95U;
constexpr GlEnum StencilPassDepthPass = 0x0B96U;
constexpr GlEnum StencilWritemask = 0x0B98U;
constexpr GlEnum StencilBackFunc = 0x8800U;
constexpr GlEnum StencilBackFail = 0x8801U;
constexpr GlEnum StencilBackPassDepthFail = 0x8802U;
constexpr GlEnum StencilBackPassDepthPass = 0x8803U;
constexpr GlEnum StencilBackRef = 0x8CA3U;
constexpr GlEnum StencilBackValueMask = 0x8CA4U;
constexpr GlEnum StencilBackWritemask = 0x8CA5U;
constexpr GlEnum Front = 0x0404U;
constexpr GlEnum Back = 0x0405U;

constexpr GlEnum ActiveTexture = 0x84E0U;
constexpr GlEnum Texture0 = 0x84C0U;
constexpr GlEnum Texture2D = 0x0DE1U;
constexpr GlEnum TextureBinding2D = 0x8069U;
constexpr GlEnum SamplerBinding = 0x8919U;
constexpr GlEnum UnpackAlignment = 0x0CF5U;
constexpr GlEnum UnpackRowLength = 0x0CF2U;
constexpr GlEnum UnpackSkipRows = 0x0CF3U;
constexpr GlEnum UnpackSkipPixels = 0x0CF4U;

} // namespace GlConstant

enum class GlEmbeddingMode : std::uint8_t {
    HostReset = 0U,
    PreserveAndRestore
};

struct GlLimits final {
    std::uint32_t maxTextureSize = 0U;
    std::uint32_t maxCombinedTextureUnits = 0U;
    std::uint32_t maxVertexAttributes = 0U;
    std::uint32_t maxUniformBlockSize = 0U;
    std::uint32_t uniformBufferOffsetAlignment = 0U;
    std::uint32_t maxSamples = 0U;
    std::uint32_t maxColorAttachments = 0U;
};

struct GlCapabilities final {
    std::uint32_t majorVersion = 0U;
    std::uint32_t minorVersion = 0U;
    std::uint32_t contextFlags = 0U;
    std::uint32_t profileMask = 0U;
    GlContextGeneration contextGeneration = 0U;
    bool coreProfile = false;
    bool debugContext = false;
    bool supportsSamplerObjects = false;
    bool supportsSyncObjects = false;
    bool supportsInstancing = false;
    GlLimits limits;
};

struct GlContextContract final {
    std::uint32_t structSize =
        static_cast<std::uint32_t>(sizeof(GlContextContract));
    std::uint32_t abiVersion = GlContextContractAbiVersion;
    void* userData = nullptr;
    const void* contextHandle = nullptr;
    GlProcAddressResolver resolve = nullptr;
    GlContextIsCurrent isCurrent = nullptr;
    GlCurrentThreadToken currentThreadToken = nullptr;
    GlThreadToken owningThreadToken = 0U;
    GlContextGeneration generation = 0U;
    GlEmbeddingMode embeddingMode = GlEmbeddingMode::HostReset;
};

struct GlFunctionTable final {
    using GetStringProc = const std::uint8_t* (AERO_GL_CALL*)(GlEnum);
    using GetStringiProc =
        const std::uint8_t* (AERO_GL_CALL*)(GlEnum, GlUInt);
    using GetIntegervProc = void (AERO_GL_CALL*)(GlEnum, GlInt*);
    using GetBooleanvProc = void (AERO_GL_CALL*)(GlEnum, GlBoolean*);
    using GetErrorProc = GlEnum (AERO_GL_CALL*)();
    using IsEnabledProc = GlBoolean (AERO_GL_CALL*)(GlEnum);
    using EnableProc = void (AERO_GL_CALL*)(GlEnum);
    using DisableProc = void (AERO_GL_CALL*)(GlEnum);
    using ViewportProc =
        void (AERO_GL_CALL*)(GlInt, GlInt, GlSize, GlSize);
    using ScissorProc =
        void (AERO_GL_CALL*)(GlInt, GlInt, GlSize, GlSize);
    using BlendEquationSeparateProc =
        void (AERO_GL_CALL*)(GlEnum, GlEnum);
    using BlendFuncSeparateProc =
        void (AERO_GL_CALL*)(GlEnum, GlEnum, GlEnum, GlEnum);
    using ColorMaskProc =
        void (AERO_GL_CALL*)(GlBoolean, GlBoolean, GlBoolean, GlBoolean);
    using DepthFuncProc = void (AERO_GL_CALL*)(GlEnum);
    using DepthMaskProc = void (AERO_GL_CALL*)(GlBoolean);
    using StencilFuncSeparateProc =
        void (AERO_GL_CALL*)(GlEnum, GlEnum, GlInt, GlUInt);
    using StencilOpSeparateProc =
        void (AERO_GL_CALL*)(GlEnum, GlEnum, GlEnum, GlEnum);
    using StencilMaskSeparateProc =
        void (AERO_GL_CALL*)(GlEnum, GlUInt);
    using PixelStoreiProc = void (AERO_GL_CALL*)(GlEnum, GlInt);
    using ActiveTextureProc = void (AERO_GL_CALL*)(GlEnum);

    using GenObjectsProc = void (AERO_GL_CALL*)(GlSize, GlUInt*);
    using DeleteObjectsProc = void (AERO_GL_CALL*)(GlSize, const GlUInt*);
    using BindObjectProc = void (AERO_GL_CALL*)(GlEnum, GlUInt);
    using BindVertexArrayProc = void (AERO_GL_CALL*)(GlUInt);
    using BufferDataProc =
        void (AERO_GL_CALL*)(GlEnum, GlSizePtr, const void*, GlEnum);
    using BufferSubDataProc =
        void (AERO_GL_CALL*)(GlEnum, GlIntPtr, GlSizePtr, const void*);
    using BindBufferRangeProc =
        void (AERO_GL_CALL*)(GlEnum, GlUInt, GlUInt, GlIntPtr, GlSizePtr);
    using BindBufferBaseProc =
        void (AERO_GL_CALL*)(GlEnum, GlUInt, GlUInt);
    using EnableVertexAttribArrayProc = void (AERO_GL_CALL*)(GlUInt);
    using DisableVertexAttribArrayProc = void (AERO_GL_CALL*)(GlUInt);
    using VertexAttribPointerProc =
        void (AERO_GL_CALL*)(
            GlUInt, GlInt, GlEnum, GlBoolean, GlSize, const void*);
    using VertexAttribDivisorProc =
        void (AERO_GL_CALL*)(GlUInt, GlUInt);

    using TexImage2DProc =
        void (AERO_GL_CALL*)(
            GlEnum, GlInt, GlInt, GlSize, GlSize, GlInt,
            GlEnum, GlEnum, const void*);
    using TexSubImage2DProc =
        void (AERO_GL_CALL*)(
            GlEnum, GlInt, GlInt, GlInt, GlSize, GlSize,
            GlEnum, GlEnum, const void*);
    using TexImage3DProc =
        void (AERO_GL_CALL*)(
            GlEnum, GlInt, GlInt, GlSize, GlSize, GlSize, GlInt,
            GlEnum, GlEnum, const void*);
    using TexSubImage3DProc =
        void (AERO_GL_CALL*)(
            GlEnum, GlInt, GlInt, GlInt, GlInt,
            GlSize, GlSize, GlSize, GlEnum, GlEnum, const void*);
    using TexImage2DMultisampleProc =
        void (AERO_GL_CALL*)(
            GlEnum, GlSize, GlEnum, GlSize, GlSize, GlBoolean);
    using TexParameteriProc =
        void (AERO_GL_CALL*)(GlEnum, GlEnum, GlInt);
    using GenerateMipmapProc = void (AERO_GL_CALL*)(GlEnum);
    using BindSamplerProc = void (AERO_GL_CALL*)(GlUInt, GlUInt);
    using SamplerParameteriProc =
        void (AERO_GL_CALL*)(GlUInt, GlEnum, GlInt);
    using SamplerParameterfProc =
        void (AERO_GL_CALL*)(GlUInt, GlEnum, GlFloat);

    using CreateShaderProc = GlUInt (AERO_GL_CALL*)(GlEnum);
    using ShaderSourceProc =
        void (AERO_GL_CALL*)(
            GlUInt, GlSize, const GlChar* const*, const GlInt*);
    using CompileShaderProc = void (AERO_GL_CALL*)(GlUInt);
    using GetShaderivProc =
        void (AERO_GL_CALL*)(GlUInt, GlEnum, GlInt*);
    using GetShaderInfoLogProc =
        void (AERO_GL_CALL*)(GlUInt, GlSize, GlSize*, GlChar*);
    using DeleteShaderProc = void (AERO_GL_CALL*)(GlUInt);
    using CreateProgramProc = GlUInt (AERO_GL_CALL*)();
    using AttachShaderProc = void (AERO_GL_CALL*)(GlUInt, GlUInt);
    using BindAttribLocationProc =
        void (AERO_GL_CALL*)(GlUInt, GlUInt, const GlChar*);
    using LinkProgramProc = void (AERO_GL_CALL*)(GlUInt);
    using GetProgramivProc =
        void (AERO_GL_CALL*)(GlUInt, GlEnum, GlInt*);
    using GetProgramInfoLogProc =
        void (AERO_GL_CALL*)(GlUInt, GlSize, GlSize*, GlChar*);
    using DetachShaderProc = void (AERO_GL_CALL*)(GlUInt, GlUInt);
    using DeleteProgramProc = void (AERO_GL_CALL*)(GlUInt);
    using UseProgramProc = void (AERO_GL_CALL*)(GlUInt);
    using GetUniformLocationProc =
        GlInt (AERO_GL_CALL*)(GlUInt, const GlChar*);
    using Uniform1iProc = void (AERO_GL_CALL*)(GlInt, GlInt);
    using Uniform1fProc = void (AERO_GL_CALL*)(GlInt, GlFloat);
    using Uniform2fProc =
        void (AERO_GL_CALL*)(GlInt, GlFloat, GlFloat);
    using Uniform4fProc =
        void (AERO_GL_CALL*)(
            GlInt, GlFloat, GlFloat, GlFloat, GlFloat);
    using UniformMatrix4fvProc =
        void (AERO_GL_CALL*)(GlInt, GlSize, GlBoolean, const GlFloat*);
    using GetUniformBlockIndexProc =
        GlUInt (AERO_GL_CALL*)(GlUInt, const GlChar*);
    using UniformBlockBindingProc =
        void (AERO_GL_CALL*)(GlUInt, GlUInt, GlUInt);

    using FramebufferTexture2DProc =
        void (AERO_GL_CALL*)(GlEnum, GlEnum, GlEnum, GlUInt, GlInt);
    using CheckFramebufferStatusProc = GlEnum (AERO_GL_CALL*)(GlEnum);
    using BlitFramebufferProc =
        void (AERO_GL_CALL*)(
            GlInt, GlInt, GlInt, GlInt, GlInt, GlInt, GlInt, GlInt,
            GlBitfield, GlEnum);
    using ClearColorProc =
        void (AERO_GL_CALL*)(GlFloat, GlFloat, GlFloat, GlFloat);
    using ClearDepthProc = void (AERO_GL_CALL*)(double);
    using ClearStencilProc = void (AERO_GL_CALL*)(GlInt);
    using ClearProc = void (AERO_GL_CALL*)(GlBitfield);
    using DrawBuffersProc =
        void (AERO_GL_CALL*)(GlSize, const GlEnum*);
    using ReadBufferProc = void (AERO_GL_CALL*)(GlEnum);
    using DrawArraysProc =
        void (AERO_GL_CALL*)(GlEnum, GlInt, GlSize);
    using DrawElementsProc =
        void (AERO_GL_CALL*)(GlEnum, GlSize, GlEnum, const void*);
    using DrawArraysInstancedProc =
        void (AERO_GL_CALL*)(GlEnum, GlInt, GlSize, GlSize);
    using DrawElementsInstancedProc =
        void (AERO_GL_CALL*)(
            GlEnum, GlSize, GlEnum, const void*, GlSize);
    using FenceSyncProc = GlSync (AERO_GL_CALL*)(GlEnum, GlBitfield);
    using DeleteSyncProc = void (AERO_GL_CALL*)(GlSync);
    using ClientWaitSyncProc =
        GlEnum (AERO_GL_CALL*)(GlSync, GlBitfield, std::uint64_t);
    using FlushProc = void (AERO_GL_CALL*)();
    using ReadPixelsProc =
        void (AERO_GL_CALL*)(
            GlInt, GlInt, GlSize, GlSize, GlEnum, GlEnum, void*);

    std::uint32_t structSize =
        static_cast<std::uint32_t>(sizeof(GlFunctionTable));
    std::uint32_t abiVersion = GlFunctionTableAbiVersion;

    GetStringProc getString = nullptr;
    GetStringiProc getStringi = nullptr;
    GetIntegervProc getIntegerv = nullptr;
    GetBooleanvProc getBooleanv = nullptr;
    GetErrorProc getError = nullptr;
    IsEnabledProc isEnabled = nullptr;
    EnableProc enable = nullptr;
    DisableProc disable = nullptr;
    ViewportProc viewport = nullptr;
    ScissorProc scissor = nullptr;
    BlendEquationSeparateProc blendEquationSeparate = nullptr;
    BlendFuncSeparateProc blendFuncSeparate = nullptr;
    ColorMaskProc colorMask = nullptr;
    DepthFuncProc depthFunc = nullptr;
    DepthMaskProc depthMask = nullptr;
    StencilFuncSeparateProc stencilFuncSeparate = nullptr;
    StencilOpSeparateProc stencilOpSeparate = nullptr;
    StencilMaskSeparateProc stencilMaskSeparate = nullptr;
    PixelStoreiProc pixelStorei = nullptr;
    ActiveTextureProc activeTexture = nullptr;

    GenObjectsProc genBuffers = nullptr;
    DeleteObjectsProc deleteBuffers = nullptr;
    BindObjectProc bindBuffer = nullptr;
    BufferDataProc bufferData = nullptr;
    BufferSubDataProc bufferSubData = nullptr;
    BindBufferRangeProc bindBufferRange = nullptr;
    BindBufferBaseProc bindBufferBase = nullptr;
    GenObjectsProc genVertexArrays = nullptr;
    DeleteObjectsProc deleteVertexArrays = nullptr;
    BindVertexArrayProc bindVertexArray = nullptr;
    EnableVertexAttribArrayProc enableVertexAttribArray = nullptr;
    DisableVertexAttribArrayProc disableVertexAttribArray = nullptr;
    VertexAttribPointerProc vertexAttribPointer = nullptr;
    VertexAttribDivisorProc vertexAttribDivisor = nullptr;

    GenObjectsProc genTextures = nullptr;
    DeleteObjectsProc deleteTextures = nullptr;
    BindObjectProc bindTexture = nullptr;
    TexImage2DProc texImage2D = nullptr;
    TexSubImage2DProc texSubImage2D = nullptr;
    TexImage3DProc texImage3D = nullptr;
    TexSubImage3DProc texSubImage3D = nullptr;
    TexImage2DMultisampleProc texImage2DMultisample = nullptr;
    TexParameteriProc texParameteri = nullptr;
    GenerateMipmapProc generateMipmap = nullptr;
    GenObjectsProc genSamplers = nullptr;
    DeleteObjectsProc deleteSamplers = nullptr;
    BindSamplerProc bindSampler = nullptr;
    SamplerParameteriProc samplerParameteri = nullptr;
    SamplerParameterfProc samplerParameterf = nullptr;

    CreateShaderProc createShader = nullptr;
    ShaderSourceProc shaderSource = nullptr;
    CompileShaderProc compileShader = nullptr;
    GetShaderivProc getShaderiv = nullptr;
    GetShaderInfoLogProc getShaderInfoLog = nullptr;
    DeleteShaderProc deleteShader = nullptr;
    CreateProgramProc createProgram = nullptr;
    AttachShaderProc attachShader = nullptr;
    BindAttribLocationProc bindAttribLocation = nullptr;
    LinkProgramProc linkProgram = nullptr;
    GetProgramivProc getProgramiv = nullptr;
    GetProgramInfoLogProc getProgramInfoLog = nullptr;
    DetachShaderProc detachShader = nullptr;
    DeleteProgramProc deleteProgram = nullptr;
    UseProgramProc useProgram = nullptr;
    GetUniformLocationProc getUniformLocation = nullptr;
    Uniform1iProc uniform1i = nullptr;
    Uniform1fProc uniform1f = nullptr;
    Uniform2fProc uniform2f = nullptr;
    Uniform4fProc uniform4f = nullptr;
    UniformMatrix4fvProc uniformMatrix4fv = nullptr;
    GetUniformBlockIndexProc getUniformBlockIndex = nullptr;
    UniformBlockBindingProc uniformBlockBinding = nullptr;

    GenObjectsProc genFramebuffers = nullptr;
    DeleteObjectsProc deleteFramebuffers = nullptr;
    BindObjectProc bindFramebuffer = nullptr;
    FramebufferTexture2DProc framebufferTexture2D = nullptr;
    CheckFramebufferStatusProc checkFramebufferStatus = nullptr;
    BlitFramebufferProc blitFramebuffer = nullptr;
    ClearColorProc clearColor = nullptr;
    ClearDepthProc clearDepth = nullptr;
    ClearStencilProc clearStencil = nullptr;
    ClearProc clear = nullptr;
    DrawBuffersProc drawBuffers = nullptr;
    ReadBufferProc readBuffer = nullptr;
    DrawArraysProc drawArrays = nullptr;
    DrawElementsProc drawElements = nullptr;
    DrawArraysInstancedProc drawArraysInstanced = nullptr;
    DrawElementsInstancedProc drawElementsInstanced = nullptr;
    FenceSyncProc fenceSync = nullptr;
    DeleteSyncProc deleteSync = nullptr;
    ClientWaitSyncProc clientWaitSync = nullptr;
    FlushProc flush = nullptr;
    ReadPixelsProc readPixels = nullptr;
};

AERO_API Base::Result<GlFunctionTable> LoadGlFunctionTable(
    GlProcAddressResolver resolver,
    void* userData) noexcept;

AERO_API Base::Result<void> ValidateGlFunctionTable(
    const GlFunctionTable& functions) noexcept;

AERO_API Base::Result<void> ValidateGlContextContract(
    const GlContextContract& contract) noexcept;

AERO_API Base::Result<GlCapabilities> QueryGlCapabilities(
    const GlFunctionTable& functions,
    const GlContextContract& contract) noexcept;

#undef AERO_GL_CALL

} // namespace Aero::Rhi
