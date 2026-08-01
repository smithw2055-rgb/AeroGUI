#include "render/opengl33/OpenGL33State.hpp"

#include <algorithm>

namespace Aero::Graphics {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

bool Equal(
    const GlRectangleState& left,
    const GlRectangleState& right) noexcept {
    return left.x == right.x &&
        left.y == right.y &&
        left.width == right.width &&
        left.height == right.height;
}

bool Equal(
    const GlBlendState& left,
    const GlBlendState& right) noexcept {
    return left.enabled == right.enabled &&
        left.colorEquation == right.colorEquation &&
        left.alphaEquation == right.alphaEquation &&
        left.sourceColor == right.sourceColor &&
        left.destinationColor == right.destinationColor &&
        left.sourceAlpha == right.sourceAlpha &&
        left.destinationAlpha == right.destinationAlpha &&
        left.writeRed == right.writeRed &&
        left.writeGreen == right.writeGreen &&
        left.writeBlue == right.writeBlue &&
        left.writeAlpha == right.writeAlpha;
}

bool Equal(
    const GlDepthState& left,
    const GlDepthState& right) noexcept {
    return left.enabled == right.enabled &&
        left.function == right.function &&
        left.writeEnabled == right.writeEnabled;
}

bool Equal(
    const GlRasterState& left,
    const GlRasterState& right) noexcept {
    return left.cullEnabled == right.cullEnabled &&
        left.cullFace == right.cullFace &&
        left.frontFace == right.frontFace &&
        left.polygonMode == right.polygonMode;
}

bool Equal(
    const GlStencilFaceState& left,
    const GlStencilFaceState& right) noexcept {
    return left.function == right.function &&
        left.reference == right.reference &&
        left.readMask == right.readMask &&
        left.stencilFail == right.stencilFail &&
        left.depthFail == right.depthFail &&
        left.pass == right.pass &&
        left.writeMask == right.writeMask;
}

bool Equal(
    const GlStencilState& left,
    const GlStencilState& right) noexcept {
    return left.enabled == right.enabled &&
        Equal(left.front, right.front) &&
        Equal(left.back, right.back);
}

bool Equal(
    const GlPixelUnpackState& left,
    const GlPixelUnpackState& right) noexcept {
    return left.alignment == right.alignment &&
        left.rowLength == right.rowLength &&
        left.skipRows == right.skipRows &&
        left.skipPixels == right.skipPixels;
}

bool IsValidRectangle(const GlRectangleState& value) noexcept {
    return value.width >= 0 && value.height >= 0;
}

bool IsValidUnpackAlignment(GlInt alignment) noexcept {
    return alignment == 1 ||
        alignment == 2 ||
        alignment == 4 ||
        alignment == 8;
}

GlUInt ToUInt(GlInt value) noexcept {
    return static_cast<GlUInt>(value);
}

bool ToBool(GlBoolean value) noexcept {
    return value != GlConstant::False;
}

} // namespace

Base::Result<void> GlStateCache::Initialize(
    const GlFunctionTable& functions,
    const GlCapabilities& capabilities) noexcept {
    if (active_) {
        return InvalidState(
            "OpenGL state cache cannot be initialized during an active scope");
    }
    Base::Result<void> validation =
        ValidateGlFunctionTable(functions);
    if (!validation) {
        return validation.GetStatus();
    }
    if (!capabilities.coreProfile ||
        capabilities.majorVersion < 3U ||
        (capabilities.majorVersion == 3U &&
         capabilities.minorVersion < 3U) ||
        capabilities.contextGeneration == 0U ||
        capabilities.limits.maxCombinedTextureUnits == 0U) {
        return InvalidArgument(
            "OpenGL state cache requires validated 3.3 Core capabilities");
    }

    functions_ = functions;
    capabilities_ = capabilities;
    generation_ = capabilities.contextGeneration;
    current_ = {};
    snapshot_ = {};
    known_ = {};
    initialized_ = true;
    return {};
}

Base::Result<void> GlStateCache::Begin(
    GlContextGeneration generation,
    GlEmbeddingMode mode) noexcept {
    if (!initialized_) {
        return InvalidState(
            "OpenGL state cache has not been initialized");
    }
    if (active_) {
        return InvalidState(
            "OpenGL state cache scope is already active");
    }
    if (generation == 0U || generation != generation_) {
        return InvalidState(
            "OpenGL context generation changed without cache invalidation");
    }

    mode_ = mode;
    active_ = true;
    if (mode_ == GlEmbeddingMode::PreserveAndRestore) {
        Base::Result<void> capture = Capture();
        if (!capture) {
            active_ = false;
            return capture.GetStatus();
        }
    } else {
        known_ = {};
    }
    return {};
}

Base::Result<void> GlStateCache::End() noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (mode_ == GlEmbeddingMode::PreserveAndRestore) {
        Restore();
    } else {
        known_ = {};
    }
    active_ = false;
    return {};
}

void GlStateCache::Invalidate(
    GlContextGeneration generation) noexcept {
    generation_ = generation;
    active_ = false;
    known_ = {};
    current_ = {};
    snapshot_ = {};
    ++statistics_.invalidations;
}

Base::Result<void> GlStateCache::VerifyActive() const noexcept {
    if (!initialized_ || !active_) {
        return InvalidState(
            "OpenGL state operation requires an active cache scope");
    }
    return {};
}

Base::Result<void> GlStateCache::Capture() noexcept {
    if (capabilities_.limits.maxCombinedTextureUnits == 0U) {
        return InvalidState(
            "OpenGL state capture has no texture-unit capability");
    }

    GlInt value = 0;
    functions_.getIntegerv(GlConstant::CurrentProgram, &value);
    snapshot_.program = ToUInt(value);
    functions_.getIntegerv(GlConstant::VertexArrayBinding, &value);
    snapshot_.vertexArray = ToUInt(value);
    functions_.getIntegerv(GlConstant::ArrayBufferBinding, &value);
    snapshot_.arrayBuffer = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::ElementArrayBufferBinding, &value);
    snapshot_.elementArrayBuffer = ToUInt(value);
    functions_.getIntegerv(GlConstant::UniformBufferBinding, &value);
    snapshot_.uniformBuffer = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::DrawFramebufferBinding, &value);
    snapshot_.drawFramebuffer = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::ReadFramebufferBinding, &value);
    snapshot_.readFramebuffer = ToUInt(value);

    GlInt rectangle[4]{};
    functions_.getIntegerv(GlConstant::Viewport, rectangle);
    snapshot_.viewport = {
        rectangle[0], rectangle[1], rectangle[2], rectangle[3]};
    functions_.getIntegerv(GlConstant::ScissorBox, rectangle);
    snapshot_.scissor = {
        rectangle[0], rectangle[1], rectangle[2], rectangle[3]};
    snapshot_.scissorEnabled =
        ToBool(functions_.isEnabled(GlConstant::ScissorTest));

    snapshot_.blend.enabled =
        ToBool(functions_.isEnabled(GlConstant::Blend));
    functions_.getIntegerv(GlConstant::BlendEquationRgb, &value);
    snapshot_.blend.colorEquation = ToUInt(value);
    functions_.getIntegerv(GlConstant::BlendEquationAlpha, &value);
    snapshot_.blend.alphaEquation = ToUInt(value);
    functions_.getIntegerv(GlConstant::BlendSrcRgb, &value);
    snapshot_.blend.sourceColor = ToUInt(value);
    functions_.getIntegerv(GlConstant::BlendDstRgb, &value);
    snapshot_.blend.destinationColor = ToUInt(value);
    functions_.getIntegerv(GlConstant::BlendSrcAlpha, &value);
    snapshot_.blend.sourceAlpha = ToUInt(value);
    functions_.getIntegerv(GlConstant::BlendDstAlpha, &value);
    snapshot_.blend.destinationAlpha = ToUInt(value);
    GlBoolean colorMask[4]{};
    functions_.getBooleanv(GlConstant::ColorWritemask, colorMask);
    snapshot_.blend.writeRed = ToBool(colorMask[0]);
    snapshot_.blend.writeGreen = ToBool(colorMask[1]);
    snapshot_.blend.writeBlue = ToBool(colorMask[2]);
    snapshot_.blend.writeAlpha = ToBool(colorMask[3]);

    snapshot_.depth.enabled =
        ToBool(functions_.isEnabled(GlConstant::DepthTest));
    functions_.getIntegerv(GlConstant::DepthFunc, &value);
    snapshot_.depth.function = ToUInt(value);
    GlBoolean booleanValue = GlConstant::False;
    functions_.getBooleanv(
        GlConstant::DepthWritemask, &booleanValue);
    snapshot_.depth.writeEnabled = ToBool(booleanValue);

    snapshot_.raster.cullEnabled =
        ToBool(functions_.isEnabled(GlConstant::CullFace));
    functions_.getIntegerv(GlConstant::CullFaceMode, &value);
    snapshot_.raster.cullFace = ToUInt(value);
    functions_.getIntegerv(GlConstant::FrontFace, &value);
    snapshot_.raster.frontFace = ToUInt(value);
    GlInt polygonModes[2]{};
    functions_.getIntegerv(GlConstant::PolygonMode, polygonModes);
    snapshot_.raster.polygonMode = ToUInt(polygonModes[0]);

    snapshot_.stencil.enabled =
        ToBool(functions_.isEnabled(GlConstant::StencilTest));
    functions_.getIntegerv(GlConstant::StencilFunc, &value);
    snapshot_.stencil.front.function = ToUInt(value);
    functions_.getIntegerv(GlConstant::StencilRef, &value);
    snapshot_.stencil.front.reference = value;
    functions_.getIntegerv(GlConstant::StencilValueMask, &value);
    snapshot_.stencil.front.readMask = ToUInt(value);
    functions_.getIntegerv(GlConstant::StencilFail, &value);
    snapshot_.stencil.front.stencilFail = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::StencilPassDepthFail, &value);
    snapshot_.stencil.front.depthFail = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::StencilPassDepthPass, &value);
    snapshot_.stencil.front.pass = ToUInt(value);
    functions_.getIntegerv(GlConstant::StencilWritemask, &value);
    snapshot_.stencil.front.writeMask = ToUInt(value);

    functions_.getIntegerv(GlConstant::StencilBackFunc, &value);
    snapshot_.stencil.back.function = ToUInt(value);
    functions_.getIntegerv(GlConstant::StencilBackRef, &value);
    snapshot_.stencil.back.reference = value;
    functions_.getIntegerv(
        GlConstant::StencilBackValueMask, &value);
    snapshot_.stencil.back.readMask = ToUInt(value);
    functions_.getIntegerv(GlConstant::StencilBackFail, &value);
    snapshot_.stencil.back.stencilFail = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::StencilBackPassDepthFail, &value);
    snapshot_.stencil.back.depthFail = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::StencilBackPassDepthPass, &value);
    snapshot_.stencil.back.pass = ToUInt(value);
    functions_.getIntegerv(
        GlConstant::StencilBackWritemask, &value);
    snapshot_.stencil.back.writeMask = ToUInt(value);

    functions_.getIntegerv(GlConstant::ActiveTexture, &value);
    const GlInt textureUnit =
        value - static_cast<GlInt>(GlConstant::Texture0);
    snapshot_.activeTextureUnit =
        textureUnit >= 0 ? static_cast<std::uint32_t>(textureUnit) : 0U;
    const std::uint32_t textureUnitCount = std::min(
        capabilities_.limits.maxCombinedTextureUnits,
        MaxCachedGlTextureUnits);
    for (std::uint32_t unit = 0U;
         unit < textureUnitCount;
         ++unit) {
        functions_.activeTexture(
            GlConstant::Texture0 + unit);
        functions_.getIntegerv(
            GlConstant::TextureBinding2D, &value);
        snapshot_.textureUnits[unit].texture2D = ToUInt(value);
        functions_.getIntegerv(
            GlConstant::TextureBinding2DArray, &value);
        snapshot_.textureUnits[unit].texture2DArray = ToUInt(value);
        functions_.getIntegerv(
            GlConstant::SamplerBinding, &value);
        snapshot_.textureUnits[unit].sampler = ToUInt(value);
    }
    functions_.activeTexture(
        GlConstant::Texture0 + snapshot_.activeTextureUnit);

    functions_.getIntegerv(GlConstant::UnpackAlignment, &value);
    snapshot_.pixelUnpack.alignment = value;
    functions_.getIntegerv(GlConstant::UnpackRowLength, &value);
    snapshot_.pixelUnpack.rowLength = value;
    functions_.getIntegerv(GlConstant::UnpackSkipRows, &value);
    snapshot_.pixelUnpack.skipRows = value;
    functions_.getIntegerv(GlConstant::UnpackSkipPixels, &value);
    snapshot_.pixelUnpack.skipPixels = value;

    current_ = snapshot_;
    known_ = {};
    known_.program = true;
    known_.vertexArray = true;
    known_.arrayBuffer = true;
    known_.elementArrayBuffer = true;
    known_.uniformBuffer = true;
    known_.drawFramebuffer = true;
    known_.readFramebuffer = true;
    known_.viewport = true;
    known_.scissor = true;
    known_.blend = true;
    known_.depth = true;
    known_.raster = true;
    known_.stencil = true;
    known_.activeTextureUnit = true;
    known_.pixelUnpack = true;
    for (std::uint32_t unit = 0U;
         unit < textureUnitCount;
         ++unit) {
        known_.textureUnits[unit] = true;
    }
    ++statistics_.captures;
    return {};
}

void GlStateCache::Restore() noexcept {
    functions_.useProgram(snapshot_.program);
    MarkCall();
    functions_.bindVertexArray(snapshot_.vertexArray);
    MarkCall();
    functions_.bindBuffer(
        GlConstant::ArrayBuffer, snapshot_.arrayBuffer);
    MarkCall();
    functions_.bindBuffer(
        GlConstant::ElementArrayBuffer,
        snapshot_.elementArrayBuffer);
    MarkCall();
    functions_.bindBuffer(
        GlConstant::UniformBuffer, snapshot_.uniformBuffer);
    MarkCall();
    functions_.bindFramebuffer(
        GlConstant::DrawFramebuffer, snapshot_.drawFramebuffer);
    MarkCall();
    functions_.bindFramebuffer(
        GlConstant::ReadFramebuffer, snapshot_.readFramebuffer);
    MarkCall();
    functions_.viewport(
        snapshot_.viewport.x,
        snapshot_.viewport.y,
        snapshot_.viewport.width,
        snapshot_.viewport.height);
    MarkCall();
    if (snapshot_.scissorEnabled) {
        functions_.enable(GlConstant::ScissorTest);
    } else {
        functions_.disable(GlConstant::ScissorTest);
    }
    MarkCall();
    functions_.scissor(
        snapshot_.scissor.x,
        snapshot_.scissor.y,
        snapshot_.scissor.width,
        snapshot_.scissor.height);
    MarkCall();

    if (snapshot_.blend.enabled) {
        functions_.enable(GlConstant::Blend);
    } else {
        functions_.disable(GlConstant::Blend);
    }
    MarkCall();
    functions_.blendEquationSeparate(
        snapshot_.blend.colorEquation,
        snapshot_.blend.alphaEquation);
    MarkCall();
    functions_.blendFuncSeparate(
        snapshot_.blend.sourceColor,
        snapshot_.blend.destinationColor,
        snapshot_.blend.sourceAlpha,
        snapshot_.blend.destinationAlpha);
    MarkCall();
    functions_.colorMask(
        snapshot_.blend.writeRed ? GlConstant::True : GlConstant::False,
        snapshot_.blend.writeGreen ? GlConstant::True : GlConstant::False,
        snapshot_.blend.writeBlue ? GlConstant::True : GlConstant::False,
        snapshot_.blend.writeAlpha ? GlConstant::True : GlConstant::False);
    MarkCall();

    if (snapshot_.depth.enabled) {
        functions_.enable(GlConstant::DepthTest);
    } else {
        functions_.disable(GlConstant::DepthTest);
    }
    MarkCall();
    functions_.depthFunc(snapshot_.depth.function);
    MarkCall();
    functions_.depthMask(
        snapshot_.depth.writeEnabled
            ? GlConstant::True
            : GlConstant::False);
    MarkCall();

    if (snapshot_.raster.cullEnabled) {
        functions_.enable(GlConstant::CullFace);
    } else {
        functions_.disable(GlConstant::CullFace);
    }
    MarkCall();
    functions_.cullFace(snapshot_.raster.cullFace);
    MarkCall();
    functions_.frontFace(snapshot_.raster.frontFace);
    MarkCall();
    functions_.polygonMode(
        GlConstant::FrontAndBack,
        snapshot_.raster.polygonMode);
    MarkCall();

    if (snapshot_.stencil.enabled) {
        functions_.enable(GlConstant::StencilTest);
    } else {
        functions_.disable(GlConstant::StencilTest);
    }
    MarkCall();
    functions_.stencilFuncSeparate(
        GlConstant::Front,
        snapshot_.stencil.front.function,
        snapshot_.stencil.front.reference,
        snapshot_.stencil.front.readMask);
    MarkCall();
    functions_.stencilOpSeparate(
        GlConstant::Front,
        snapshot_.stencil.front.stencilFail,
        snapshot_.stencil.front.depthFail,
        snapshot_.stencil.front.pass);
    MarkCall();
    functions_.stencilMaskSeparate(
        GlConstant::Front,
        snapshot_.stencil.front.writeMask);
    MarkCall();
    functions_.stencilFuncSeparate(
        GlConstant::Back,
        snapshot_.stencil.back.function,
        snapshot_.stencil.back.reference,
        snapshot_.stencil.back.readMask);
    MarkCall();
    functions_.stencilOpSeparate(
        GlConstant::Back,
        snapshot_.stencil.back.stencilFail,
        snapshot_.stencil.back.depthFail,
        snapshot_.stencil.back.pass);
    MarkCall();
    functions_.stencilMaskSeparate(
        GlConstant::Back,
        snapshot_.stencil.back.writeMask);
    MarkCall();

    const std::uint32_t textureUnitCount = std::min(
        capabilities_.limits.maxCombinedTextureUnits,
        MaxCachedGlTextureUnits);
    for (std::uint32_t unit = 0U;
         unit < textureUnitCount;
         ++unit) {
        functions_.activeTexture(GlConstant::Texture0 + unit);
        MarkCall();
        functions_.bindTexture(
            GlConstant::Texture2D,
            snapshot_.textureUnits[unit].texture2D);
        MarkCall();
        functions_.bindTexture(
            GlConstant::Texture2DArray,
            snapshot_.textureUnits[unit].texture2DArray);
        MarkCall();
        functions_.bindSampler(
            unit, snapshot_.textureUnits[unit].sampler);
        MarkCall();
    }
    functions_.activeTexture(
        GlConstant::Texture0 + snapshot_.activeTextureUnit);
    MarkCall();

    functions_.pixelStorei(
        GlConstant::UnpackAlignment,
        snapshot_.pixelUnpack.alignment);
    MarkCall();
    functions_.pixelStorei(
        GlConstant::UnpackRowLength,
        snapshot_.pixelUnpack.rowLength);
    MarkCall();
    functions_.pixelStorei(
        GlConstant::UnpackSkipRows,
        snapshot_.pixelUnpack.skipRows);
    MarkCall();
    functions_.pixelStorei(
        GlConstant::UnpackSkipPixels,
        snapshot_.pixelUnpack.skipPixels);
    MarkCall();

    current_ = snapshot_;
    ++statistics_.restores;
}

Base::Result<void> GlStateCache::UseProgram(
    GlUInt program) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.program && current_.program == program) {
        MarkRedundant();
        return {};
    }
    functions_.useProgram(program);
    MarkCall();
    current_.program = program;
    known_.program = true;
    return {};
}

Base::Result<void> GlStateCache::BindVertexArray(
    GlUInt vertexArray) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.vertexArray &&
        current_.vertexArray == vertexArray) {
        MarkRedundant();
        return {};
    }
    functions_.bindVertexArray(vertexArray);
    MarkCall();
    current_.vertexArray = vertexArray;
    known_.vertexArray = true;
    known_.elementArrayBuffer = false;
    return {};
}

Base::Result<void> GlStateCache::BindArrayBuffer(
    GlUInt buffer) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.arrayBuffer &&
        current_.arrayBuffer == buffer) {
        MarkRedundant();
        return {};
    }
    functions_.bindBuffer(GlConstant::ArrayBuffer, buffer);
    MarkCall();
    current_.arrayBuffer = buffer;
    known_.arrayBuffer = true;
    return {};
}

Base::Result<void> GlStateCache::BindElementArrayBuffer(
    GlUInt buffer) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.elementArrayBuffer &&
        current_.elementArrayBuffer == buffer) {
        MarkRedundant();
        return {};
    }
    functions_.bindBuffer(
        GlConstant::ElementArrayBuffer, buffer);
    MarkCall();
    current_.elementArrayBuffer = buffer;
    known_.elementArrayBuffer = true;
    return {};
}

Base::Result<void> GlStateCache::BindUniformBuffer(
    GlUInt buffer) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.uniformBuffer &&
        current_.uniformBuffer == buffer) {
        MarkRedundant();
        return {};
    }
    functions_.bindBuffer(GlConstant::UniformBuffer, buffer);
    MarkCall();
    current_.uniformBuffer = buffer;
    known_.uniformBuffer = true;
    return {};
}

Base::Result<void> GlStateCache::BindDrawFramebuffer(
    GlUInt framebuffer) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.drawFramebuffer &&
        current_.drawFramebuffer == framebuffer) {
        MarkRedundant();
        return {};
    }
    functions_.bindFramebuffer(
        GlConstant::DrawFramebuffer, framebuffer);
    MarkCall();
    current_.drawFramebuffer = framebuffer;
    known_.drawFramebuffer = true;
    return {};
}

Base::Result<void> GlStateCache::BindReadFramebuffer(
    GlUInt framebuffer) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.readFramebuffer &&
        current_.readFramebuffer == framebuffer) {
        MarkRedundant();
        return {};
    }
    functions_.bindFramebuffer(
        GlConstant::ReadFramebuffer, framebuffer);
    MarkCall();
    current_.readFramebuffer = framebuffer;
    known_.readFramebuffer = true;
    return {};
}

Base::Result<void> GlStateCache::SetViewport(
    const GlRectangleState& viewport) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (!IsValidRectangle(viewport)) {
        return InvalidArgument(
            "OpenGL viewport dimensions cannot be negative");
    }
    if (known_.viewport &&
        Equal(current_.viewport, viewport)) {
        MarkRedundant();
        return {};
    }
    functions_.viewport(
        viewport.x, viewport.y, viewport.width, viewport.height);
    MarkCall();
    current_.viewport = viewport;
    known_.viewport = true;
    return {};
}

Base::Result<void> GlStateCache::SetScissor(
    bool enabled,
    const GlRectangleState& scissor) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (!IsValidRectangle(scissor)) {
        return InvalidArgument(
            "OpenGL scissor dimensions cannot be negative");
    }
    if (known_.scissor &&
        current_.scissorEnabled == enabled &&
        Equal(current_.scissor, scissor)) {
        MarkRedundant();
        return {};
    }
    if (!known_.scissor ||
        current_.scissorEnabled != enabled) {
        if (enabled) {
            functions_.enable(GlConstant::ScissorTest);
        } else {
            functions_.disable(GlConstant::ScissorTest);
        }
        MarkCall();
    }
    if (!known_.scissor ||
        !Equal(current_.scissor, scissor)) {
        functions_.scissor(
            scissor.x, scissor.y, scissor.width, scissor.height);
        MarkCall();
    }
    current_.scissorEnabled = enabled;
    current_.scissor = scissor;
    known_.scissor = true;
    return {};
}

Base::Result<void> GlStateCache::SetBlendState(
    const GlBlendState& state) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.blend && Equal(current_.blend, state)) {
        MarkRedundant();
        return {};
    }
    if (!known_.blend ||
        current_.blend.enabled != state.enabled) {
        if (state.enabled) {
            functions_.enable(GlConstant::Blend);
        } else {
            functions_.disable(GlConstant::Blend);
        }
        MarkCall();
    }
    if (!known_.blend ||
        current_.blend.colorEquation != state.colorEquation ||
        current_.blend.alphaEquation != state.alphaEquation) {
        functions_.blendEquationSeparate(
            state.colorEquation, state.alphaEquation);
        MarkCall();
    }
    if (!known_.blend ||
        current_.blend.sourceColor != state.sourceColor ||
        current_.blend.destinationColor != state.destinationColor ||
        current_.blend.sourceAlpha != state.sourceAlpha ||
        current_.blend.destinationAlpha != state.destinationAlpha) {
        functions_.blendFuncSeparate(
            state.sourceColor,
            state.destinationColor,
            state.sourceAlpha,
            state.destinationAlpha);
        MarkCall();
    }
    if (!known_.blend ||
        current_.blend.writeRed != state.writeRed ||
        current_.blend.writeGreen != state.writeGreen ||
        current_.blend.writeBlue != state.writeBlue ||
        current_.blend.writeAlpha != state.writeAlpha) {
        functions_.colorMask(
            state.writeRed ? GlConstant::True : GlConstant::False,
            state.writeGreen ? GlConstant::True : GlConstant::False,
            state.writeBlue ? GlConstant::True : GlConstant::False,
            state.writeAlpha ? GlConstant::True : GlConstant::False);
        MarkCall();
    }
    current_.blend = state;
    known_.blend = true;
    return {};
}

Base::Result<void> GlStateCache::SetDepthState(
    const GlDepthState& state) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.depth && Equal(current_.depth, state)) {
        MarkRedundant();
        return {};
    }
    if (!known_.depth ||
        current_.depth.enabled != state.enabled) {
        if (state.enabled) {
            functions_.enable(GlConstant::DepthTest);
        } else {
            functions_.disable(GlConstant::DepthTest);
        }
        MarkCall();
    }
    if (!known_.depth ||
        current_.depth.function != state.function) {
        functions_.depthFunc(state.function);
        MarkCall();
    }
    if (!known_.depth ||
        current_.depth.writeEnabled != state.writeEnabled) {
        functions_.depthMask(
            state.writeEnabled
                ? GlConstant::True
                : GlConstant::False);
        MarkCall();
    }
    current_.depth = state;
    known_.depth = true;
    return {};
}

Base::Result<void> GlStateCache::SetStencilState(
    const GlStencilState& state) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.stencil && Equal(current_.stencil, state)) {
        MarkRedundant();
        return {};
    }
    if (!known_.stencil ||
        current_.stencil.enabled != state.enabled) {
        if (state.enabled) {
            functions_.enable(GlConstant::StencilTest);
        } else {
            functions_.disable(GlConstant::StencilTest);
        }
        MarkCall();
    }
    if (!known_.stencil ||
        !Equal(current_.stencil.front, state.front)) {
        functions_.stencilFuncSeparate(
            GlConstant::Front,
            state.front.function,
            state.front.reference,
            state.front.readMask);
        MarkCall();
        functions_.stencilOpSeparate(
            GlConstant::Front,
            state.front.stencilFail,
            state.front.depthFail,
            state.front.pass);
        MarkCall();
        functions_.stencilMaskSeparate(
            GlConstant::Front, state.front.writeMask);
        MarkCall();
    }
    if (!known_.stencil ||
        !Equal(current_.stencil.back, state.back)) {
        functions_.stencilFuncSeparate(
            GlConstant::Back,
            state.back.function,
            state.back.reference,
            state.back.readMask);
        MarkCall();
        functions_.stencilOpSeparate(
            GlConstant::Back,
            state.back.stencilFail,
            state.back.depthFail,
            state.back.pass);
        MarkCall();
        functions_.stencilMaskSeparate(
            GlConstant::Back, state.back.writeMask);
        MarkCall();
    }
    current_.stencil = state;
    known_.stencil = true;
    return {};
}

Base::Result<void> GlStateCache::SetRasterState(
    const GlRasterState& state) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (known_.raster && Equal(current_.raster, state)) {
        MarkRedundant();
        return {};
    }
    if (!known_.raster ||
        current_.raster.cullEnabled != state.cullEnabled) {
        if (state.cullEnabled) {
            functions_.enable(GlConstant::CullFace);
        } else {
            functions_.disable(GlConstant::CullFace);
        }
        MarkCall();
    }
    if (!known_.raster ||
        current_.raster.cullFace != state.cullFace) {
        functions_.cullFace(state.cullFace);
        MarkCall();
    }
    if (!known_.raster ||
        current_.raster.frontFace != state.frontFace) {
        functions_.frontFace(state.frontFace);
        MarkCall();
    }
    if (!known_.raster ||
        current_.raster.polygonMode != state.polygonMode) {
        functions_.polygonMode(
            GlConstant::FrontAndBack,
            state.polygonMode);
        MarkCall();
    }
    current_.raster = state;
    known_.raster = true;
    return {};
}

Base::Result<void> GlStateCache::BindTextureSampler(
    std::uint32_t unit,
    GlEnum target,
    GlUInt texture,
    GlUInt sampler) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    const std::uint32_t textureUnitCount = std::min(
        capabilities_.limits.maxCombinedTextureUnits,
        MaxCachedGlTextureUnits);
    if (unit >= textureUnitCount) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "OpenGL texture unit exceeds the cache capability");
    }
    if (target != GlConstant::Texture2D &&
        target != GlConstant::Texture2DArray) {
        return InvalidArgument(
            "OpenGL state cache supports only 2D and 2D-array textures");
    }

    bool changed = false;
    if (!known_.activeTextureUnit ||
        current_.activeTextureUnit != unit) {
        functions_.activeTexture(GlConstant::Texture0 + unit);
        MarkCall();
        current_.activeTextureUnit = unit;
        known_.activeTextureUnit = true;
        changed = true;
    }
    GlUInt& currentTexture =
        target == GlConstant::Texture2D
        ? current_.textureUnits[unit].texture2D
        : current_.textureUnits[unit].texture2DArray;
    if (!known_.textureUnits[unit] ||
        currentTexture != texture) {
        functions_.bindTexture(target, texture);
        MarkCall();
        currentTexture = texture;
        changed = true;
    }
    if (!known_.textureUnits[unit] ||
        current_.textureUnits[unit].sampler != sampler) {
        functions_.bindSampler(unit, sampler);
        MarkCall();
        current_.textureUnits[unit].sampler = sampler;
        changed = true;
    }
    known_.textureUnits[unit] = true;
    if (!changed) {
        MarkRedundant();
    }
    return {};
}

Base::Result<void> GlStateCache::SetPixelUnpack(
    const GlPixelUnpackState& state) noexcept {
    Base::Result<void> ready = VerifyActive();
    if (!ready) {
        return ready.GetStatus();
    }
    if (!IsValidUnpackAlignment(state.alignment) ||
        state.rowLength < 0 ||
        state.skipRows < 0 ||
        state.skipPixels < 0) {
        return InvalidArgument(
            "OpenGL pixel-unpack state is invalid");
    }
    if (known_.pixelUnpack &&
        Equal(current_.pixelUnpack, state)) {
        MarkRedundant();
        return {};
    }
    if (!known_.pixelUnpack ||
        current_.pixelUnpack.alignment != state.alignment) {
        functions_.pixelStorei(
            GlConstant::UnpackAlignment, state.alignment);
        MarkCall();
    }
    if (!known_.pixelUnpack ||
        current_.pixelUnpack.rowLength != state.rowLength) {
        functions_.pixelStorei(
            GlConstant::UnpackRowLength, state.rowLength);
        MarkCall();
    }
    if (!known_.pixelUnpack ||
        current_.pixelUnpack.skipRows != state.skipRows) {
        functions_.pixelStorei(
            GlConstant::UnpackSkipRows, state.skipRows);
        MarkCall();
    }
    if (!known_.pixelUnpack ||
        current_.pixelUnpack.skipPixels != state.skipPixels) {
        functions_.pixelStorei(
            GlConstant::UnpackSkipPixels, state.skipPixels);
        MarkCall();
    }
    current_.pixelUnpack = state;
    known_.pixelUnpack = true;
    return {};
}

} // namespace Aero::Graphics
