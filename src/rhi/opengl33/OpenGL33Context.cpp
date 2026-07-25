#include <Aero/Rhi/OpenGL33.hpp>

namespace Aero::Rhi {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported, message);
}

template<class T>
T Resolve(
    GlProcAddressResolver resolver,
    void* userData,
    const char* name) noexcept {
    return reinterpret_cast<T>(resolver(userData, name));
}

std::uint32_t PositiveValue(GlInt value) noexcept {
    return value > 0 ? static_cast<std::uint32_t>(value) : 0U;
}

} // namespace

Base::Result<GlFunctionTable> LoadGlFunctionTable(
    GlProcAddressResolver resolver,
    void* userData) noexcept {
    if (resolver == nullptr) {
        return InvalidArgument(
            "OpenGL function resolution requires a host resolver");
    }

    GlFunctionTable functions;
#define AERO_GL_LOAD(field, type, name) \
    functions.field = Resolve<GlFunctionTable::type>( \
        resolver, userData, name)

    AERO_GL_LOAD(getString, GetStringProc, "glGetString");
    AERO_GL_LOAD(getStringi, GetStringiProc, "glGetStringi");
    AERO_GL_LOAD(getIntegerv, GetIntegervProc, "glGetIntegerv");
    AERO_GL_LOAD(getBooleanv, GetBooleanvProc, "glGetBooleanv");
    AERO_GL_LOAD(getError, GetErrorProc, "glGetError");
    AERO_GL_LOAD(isEnabled, IsEnabledProc, "glIsEnabled");
    AERO_GL_LOAD(enable, EnableProc, "glEnable");
    AERO_GL_LOAD(disable, DisableProc, "glDisable");
    AERO_GL_LOAD(viewport, ViewportProc, "glViewport");
    AERO_GL_LOAD(scissor, ScissorProc, "glScissor");
    AERO_GL_LOAD(
        blendEquationSeparate,
        BlendEquationSeparateProc,
        "glBlendEquationSeparate");
    AERO_GL_LOAD(
        blendFuncSeparate,
        BlendFuncSeparateProc,
        "glBlendFuncSeparate");
    AERO_GL_LOAD(colorMask, ColorMaskProc, "glColorMask");
    AERO_GL_LOAD(depthFunc, DepthFuncProc, "glDepthFunc");
    AERO_GL_LOAD(depthMask, DepthMaskProc, "glDepthMask");
    AERO_GL_LOAD(cullFace, CullFaceProc, "glCullFace");
    AERO_GL_LOAD(frontFace, FrontFaceProc, "glFrontFace");
    AERO_GL_LOAD(polygonMode, PolygonModeProc, "glPolygonMode");
    AERO_GL_LOAD(
        stencilFuncSeparate,
        StencilFuncSeparateProc,
        "glStencilFuncSeparate");
    AERO_GL_LOAD(
        stencilOpSeparate,
        StencilOpSeparateProc,
        "glStencilOpSeparate");
    AERO_GL_LOAD(
        stencilMaskSeparate,
        StencilMaskSeparateProc,
        "glStencilMaskSeparate");
    AERO_GL_LOAD(pixelStorei, PixelStoreiProc, "glPixelStorei");
    AERO_GL_LOAD(activeTexture, ActiveTextureProc, "glActiveTexture");

    AERO_GL_LOAD(genBuffers, GenObjectsProc, "glGenBuffers");
    AERO_GL_LOAD(deleteBuffers, DeleteObjectsProc, "glDeleteBuffers");
    AERO_GL_LOAD(bindBuffer, BindObjectProc, "glBindBuffer");
    AERO_GL_LOAD(bufferData, BufferDataProc, "glBufferData");
    AERO_GL_LOAD(bufferSubData, BufferSubDataProc, "glBufferSubData");
    AERO_GL_LOAD(
        bindBufferRange, BindBufferRangeProc, "glBindBufferRange");
    AERO_GL_LOAD(
        bindBufferBase, BindBufferBaseProc, "glBindBufferBase");
    AERO_GL_LOAD(
        genVertexArrays, GenObjectsProc, "glGenVertexArrays");
    AERO_GL_LOAD(
        deleteVertexArrays, DeleteObjectsProc, "glDeleteVertexArrays");
    AERO_GL_LOAD(
        bindVertexArray, BindVertexArrayProc, "glBindVertexArray");
    AERO_GL_LOAD(
        enableVertexAttribArray,
        EnableVertexAttribArrayProc,
        "glEnableVertexAttribArray");
    AERO_GL_LOAD(
        disableVertexAttribArray,
        DisableVertexAttribArrayProc,
        "glDisableVertexAttribArray");
    AERO_GL_LOAD(
        vertexAttribPointer,
        VertexAttribPointerProc,
        "glVertexAttribPointer");
    AERO_GL_LOAD(
        vertexAttribDivisor,
        VertexAttribDivisorProc,
        "glVertexAttribDivisor");

    AERO_GL_LOAD(genTextures, GenObjectsProc, "glGenTextures");
    AERO_GL_LOAD(deleteTextures, DeleteObjectsProc, "glDeleteTextures");
    AERO_GL_LOAD(bindTexture, BindObjectProc, "glBindTexture");
    AERO_GL_LOAD(texImage2D, TexImage2DProc, "glTexImage2D");
    AERO_GL_LOAD(texSubImage2D, TexSubImage2DProc, "glTexSubImage2D");
    AERO_GL_LOAD(texImage3D, TexImage3DProc, "glTexImage3D");
    AERO_GL_LOAD(texSubImage3D, TexSubImage3DProc, "glTexSubImage3D");
    AERO_GL_LOAD(
        texImage2DMultisample,
        TexImage2DMultisampleProc,
        "glTexImage2DMultisample");
    AERO_GL_LOAD(texParameteri, TexParameteriProc, "glTexParameteri");
    AERO_GL_LOAD(
        generateMipmap, GenerateMipmapProc, "glGenerateMipmap");
    AERO_GL_LOAD(genSamplers, GenObjectsProc, "glGenSamplers");
    AERO_GL_LOAD(
        deleteSamplers, DeleteObjectsProc, "glDeleteSamplers");
    AERO_GL_LOAD(bindSampler, BindSamplerProc, "glBindSampler");
    AERO_GL_LOAD(
        samplerParameteri, SamplerParameteriProc, "glSamplerParameteri");
    AERO_GL_LOAD(
        samplerParameterf, SamplerParameterfProc, "glSamplerParameterf");

    AERO_GL_LOAD(createShader, CreateShaderProc, "glCreateShader");
    AERO_GL_LOAD(shaderSource, ShaderSourceProc, "glShaderSource");
    AERO_GL_LOAD(compileShader, CompileShaderProc, "glCompileShader");
    AERO_GL_LOAD(getShaderiv, GetShaderivProc, "glGetShaderiv");
    AERO_GL_LOAD(
        getShaderInfoLog, GetShaderInfoLogProc, "glGetShaderInfoLog");
    AERO_GL_LOAD(deleteShader, DeleteShaderProc, "glDeleteShader");
    AERO_GL_LOAD(createProgram, CreateProgramProc, "glCreateProgram");
    AERO_GL_LOAD(attachShader, AttachShaderProc, "glAttachShader");
    AERO_GL_LOAD(
        bindAttribLocation,
        BindAttribLocationProc,
        "glBindAttribLocation");
    AERO_GL_LOAD(linkProgram, LinkProgramProc, "glLinkProgram");
    AERO_GL_LOAD(getProgramiv, GetProgramivProc, "glGetProgramiv");
    AERO_GL_LOAD(
        getProgramInfoLog,
        GetProgramInfoLogProc,
        "glGetProgramInfoLog");
    AERO_GL_LOAD(detachShader, DetachShaderProc, "glDetachShader");
    AERO_GL_LOAD(deleteProgram, DeleteProgramProc, "glDeleteProgram");
    AERO_GL_LOAD(useProgram, UseProgramProc, "glUseProgram");
    AERO_GL_LOAD(
        getUniformLocation,
        GetUniformLocationProc,
        "glGetUniformLocation");
    AERO_GL_LOAD(uniform1i, Uniform1iProc, "glUniform1i");
    AERO_GL_LOAD(uniform1f, Uniform1fProc, "glUniform1f");
    AERO_GL_LOAD(uniform2f, Uniform2fProc, "glUniform2f");
    AERO_GL_LOAD(uniform4f, Uniform4fProc, "glUniform4f");
    AERO_GL_LOAD(
        uniformMatrix4fv,
        UniformMatrix4fvProc,
        "glUniformMatrix4fv");
    AERO_GL_LOAD(
        getUniformBlockIndex,
        GetUniformBlockIndexProc,
        "glGetUniformBlockIndex");
    AERO_GL_LOAD(
        uniformBlockBinding,
        UniformBlockBindingProc,
        "glUniformBlockBinding");

    AERO_GL_LOAD(
        genFramebuffers, GenObjectsProc, "glGenFramebuffers");
    AERO_GL_LOAD(
        deleteFramebuffers, DeleteObjectsProc, "glDeleteFramebuffers");
    AERO_GL_LOAD(
        bindFramebuffer, BindObjectProc, "glBindFramebuffer");
    AERO_GL_LOAD(
        framebufferTexture2D,
        FramebufferTexture2DProc,
        "glFramebufferTexture2D");
    AERO_GL_LOAD(
        checkFramebufferStatus,
        CheckFramebufferStatusProc,
        "glCheckFramebufferStatus");
    AERO_GL_LOAD(
        blitFramebuffer, BlitFramebufferProc, "glBlitFramebuffer");
    AERO_GL_LOAD(clearColor, ClearColorProc, "glClearColor");
    AERO_GL_LOAD(clearDepth, ClearDepthProc, "glClearDepth");
    AERO_GL_LOAD(clearStencil, ClearStencilProc, "glClearStencil");
    AERO_GL_LOAD(clear, ClearProc, "glClear");
    AERO_GL_LOAD(clearBufferfv, ClearBufferfvProc, "glClearBufferfv");
    AERO_GL_LOAD(clearBufferiv, ClearBufferivProc, "glClearBufferiv");
    AERO_GL_LOAD(clearBufferfi, ClearBufferfiProc, "glClearBufferfi");
    AERO_GL_LOAD(drawBuffers, DrawBuffersProc, "glDrawBuffers");
    AERO_GL_LOAD(readBuffer, ReadBufferProc, "glReadBuffer");
    AERO_GL_LOAD(drawArrays, DrawArraysProc, "glDrawArrays");
    AERO_GL_LOAD(drawElements, DrawElementsProc, "glDrawElements");
    AERO_GL_LOAD(
        drawArraysInstanced,
        DrawArraysInstancedProc,
        "glDrawArraysInstanced");
    AERO_GL_LOAD(
        drawElementsInstanced,
        DrawElementsInstancedProc,
        "glDrawElementsInstanced");
    AERO_GL_LOAD(
        drawElementsBaseVertex,
        DrawElementsBaseVertexProc,
        "glDrawElementsBaseVertex");
    AERO_GL_LOAD(
        drawElementsInstancedBaseVertex,
        DrawElementsInstancedBaseVertexProc,
        "glDrawElementsInstancedBaseVertex");
    AERO_GL_LOAD(fenceSync, FenceSyncProc, "glFenceSync");
    AERO_GL_LOAD(deleteSync, DeleteSyncProc, "glDeleteSync");
    AERO_GL_LOAD(
        clientWaitSync, ClientWaitSyncProc, "glClientWaitSync");
    AERO_GL_LOAD(flush, FlushProc, "glFlush");
    AERO_GL_LOAD(readPixels, ReadPixelsProc, "glReadPixels");

#undef AERO_GL_LOAD

    Base::Result<void> validation = ValidateGlFunctionTable(functions);
    if (!validation) {
        return validation.GetStatus();
    }
    return functions;
}

Base::Result<void> ValidateGlFunctionTable(
    const GlFunctionTable& functions) noexcept {
    if (functions.structSize < sizeof(GlFunctionTable) ||
        functions.abiVersion != GlFunctionTableAbiVersion) {
        return InvalidArgument(
            "OpenGL function table ABI or structure size is incompatible");
    }

#define AERO_GL_REQUIRE(field) \
    if (functions.field == nullptr) { \
        return InvalidArgument( \
            "OpenGL 3.3 function table is missing a required entry point"); \
    }

    AERO_GL_REQUIRE(getString);
    AERO_GL_REQUIRE(getStringi);
    AERO_GL_REQUIRE(getIntegerv);
    AERO_GL_REQUIRE(getBooleanv);
    AERO_GL_REQUIRE(getError);
    AERO_GL_REQUIRE(isEnabled);
    AERO_GL_REQUIRE(enable);
    AERO_GL_REQUIRE(disable);
    AERO_GL_REQUIRE(viewport);
    AERO_GL_REQUIRE(scissor);
    AERO_GL_REQUIRE(blendEquationSeparate);
    AERO_GL_REQUIRE(blendFuncSeparate);
    AERO_GL_REQUIRE(colorMask);
    AERO_GL_REQUIRE(depthFunc);
    AERO_GL_REQUIRE(depthMask);
    AERO_GL_REQUIRE(cullFace);
    AERO_GL_REQUIRE(frontFace);
    AERO_GL_REQUIRE(polygonMode);
    AERO_GL_REQUIRE(stencilFuncSeparate);
    AERO_GL_REQUIRE(stencilOpSeparate);
    AERO_GL_REQUIRE(stencilMaskSeparate);
    AERO_GL_REQUIRE(pixelStorei);
    AERO_GL_REQUIRE(activeTexture);
    AERO_GL_REQUIRE(genBuffers);
    AERO_GL_REQUIRE(deleteBuffers);
    AERO_GL_REQUIRE(bindBuffer);
    AERO_GL_REQUIRE(bufferData);
    AERO_GL_REQUIRE(bufferSubData);
    AERO_GL_REQUIRE(bindBufferRange);
    AERO_GL_REQUIRE(bindBufferBase);
    AERO_GL_REQUIRE(genVertexArrays);
    AERO_GL_REQUIRE(deleteVertexArrays);
    AERO_GL_REQUIRE(bindVertexArray);
    AERO_GL_REQUIRE(enableVertexAttribArray);
    AERO_GL_REQUIRE(disableVertexAttribArray);
    AERO_GL_REQUIRE(vertexAttribPointer);
    AERO_GL_REQUIRE(vertexAttribDivisor);
    AERO_GL_REQUIRE(genTextures);
    AERO_GL_REQUIRE(deleteTextures);
    AERO_GL_REQUIRE(bindTexture);
    AERO_GL_REQUIRE(texImage2D);
    AERO_GL_REQUIRE(texSubImage2D);
    AERO_GL_REQUIRE(texImage3D);
    AERO_GL_REQUIRE(texSubImage3D);
    AERO_GL_REQUIRE(texImage2DMultisample);
    AERO_GL_REQUIRE(texParameteri);
    AERO_GL_REQUIRE(generateMipmap);
    AERO_GL_REQUIRE(genSamplers);
    AERO_GL_REQUIRE(deleteSamplers);
    AERO_GL_REQUIRE(bindSampler);
    AERO_GL_REQUIRE(samplerParameteri);
    AERO_GL_REQUIRE(samplerParameterf);
    AERO_GL_REQUIRE(createShader);
    AERO_GL_REQUIRE(shaderSource);
    AERO_GL_REQUIRE(compileShader);
    AERO_GL_REQUIRE(getShaderiv);
    AERO_GL_REQUIRE(getShaderInfoLog);
    AERO_GL_REQUIRE(deleteShader);
    AERO_GL_REQUIRE(createProgram);
    AERO_GL_REQUIRE(attachShader);
    AERO_GL_REQUIRE(bindAttribLocation);
    AERO_GL_REQUIRE(linkProgram);
    AERO_GL_REQUIRE(getProgramiv);
    AERO_GL_REQUIRE(getProgramInfoLog);
    AERO_GL_REQUIRE(detachShader);
    AERO_GL_REQUIRE(deleteProgram);
    AERO_GL_REQUIRE(useProgram);
    AERO_GL_REQUIRE(getUniformLocation);
    AERO_GL_REQUIRE(uniform1i);
    AERO_GL_REQUIRE(uniform1f);
    AERO_GL_REQUIRE(uniform2f);
    AERO_GL_REQUIRE(uniform4f);
    AERO_GL_REQUIRE(uniformMatrix4fv);
    AERO_GL_REQUIRE(getUniformBlockIndex);
    AERO_GL_REQUIRE(uniformBlockBinding);
    AERO_GL_REQUIRE(genFramebuffers);
    AERO_GL_REQUIRE(deleteFramebuffers);
    AERO_GL_REQUIRE(bindFramebuffer);
    AERO_GL_REQUIRE(framebufferTexture2D);
    AERO_GL_REQUIRE(checkFramebufferStatus);
    AERO_GL_REQUIRE(blitFramebuffer);
    AERO_GL_REQUIRE(clearColor);
    AERO_GL_REQUIRE(clearDepth);
    AERO_GL_REQUIRE(clearStencil);
    AERO_GL_REQUIRE(clear);
    AERO_GL_REQUIRE(clearBufferfv);
    AERO_GL_REQUIRE(clearBufferiv);
    AERO_GL_REQUIRE(clearBufferfi);
    AERO_GL_REQUIRE(drawBuffers);
    AERO_GL_REQUIRE(readBuffer);
    AERO_GL_REQUIRE(drawArrays);
    AERO_GL_REQUIRE(drawElements);
    AERO_GL_REQUIRE(drawArraysInstanced);
    AERO_GL_REQUIRE(drawElementsInstanced);
    AERO_GL_REQUIRE(drawElementsBaseVertex);
    AERO_GL_REQUIRE(drawElementsInstancedBaseVertex);
    AERO_GL_REQUIRE(fenceSync);
    AERO_GL_REQUIRE(deleteSync);
    AERO_GL_REQUIRE(clientWaitSync);
    AERO_GL_REQUIRE(flush);
    AERO_GL_REQUIRE(readPixels);

#undef AERO_GL_REQUIRE

    return {};
}

Base::Result<void> ValidateGlContextContract(
    const GlContextContract& contract) noexcept {
    if (contract.structSize < sizeof(GlContextContract) ||
        contract.abiVersion != GlContextContractAbiVersion) {
        return InvalidArgument(
            "OpenGL context contract ABI or structure size is incompatible");
    }
    if (contract.contextHandle == nullptr ||
        contract.resolve == nullptr ||
        contract.isCurrent == nullptr ||
        contract.currentThreadToken == nullptr ||
        contract.owningThreadToken == 0U ||
        contract.generation == 0U) {
        return InvalidArgument(
            "OpenGL context contract is incomplete");
    }
    if (contract.currentThreadToken(contract.userData) !=
        contract.owningThreadToken) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "OpenGL context is being used from a non-owning thread");
    }
    if (!contract.isCurrent(
            contract.userData, contract.contextHandle)) {
        return InvalidState(
            "OpenGL context is not current on the owning thread");
    }
    return {};
}

Base::Result<GlCapabilities> QueryGlCapabilities(
    const GlFunctionTable& functions,
    const GlContextContract& contract) noexcept {
    Base::Result<void> tableValidation =
        ValidateGlFunctionTable(functions);
    if (!tableValidation) {
        return tableValidation.GetStatus();
    }
    Base::Result<void> contextValidation =
        ValidateGlContextContract(contract);
    if (!contextValidation) {
        return contextValidation.GetStatus();
    }

    GlInt major = 0;
    GlInt minor = 0;
    GlInt profile = 0;
    GlInt flags = 0;
    functions.getIntegerv(GlConstant::MajorVersion, &major);
    functions.getIntegerv(GlConstant::MinorVersion, &minor);
    functions.getIntegerv(GlConstant::ContextProfileMask, &profile);
    functions.getIntegerv(GlConstant::ContextFlags, &flags);

    if (major < 3 || (major == 3 && minor < 3)) {
        return Unsupported(
            "Aero OpenGL backend requires OpenGL 3.3 or newer");
    }
    if ((profile & GlConstant::ContextCoreProfileBit) == 0 ||
        (profile & GlConstant::ContextCompatibilityProfileBit) != 0) {
        return Unsupported(
            "Aero OpenGL backend requires an OpenGL Core Profile context");
    }

    GlInt maxTextureSize = 0;
    GlInt maxArrayTextureLayers = 0;
    GlInt maxTextureUnits = 0;
    GlInt maxVertexAttributes = 0;
    GlInt maxUniformBlockSize = 0;
    GlInt maxUniformBufferBindings = 0;
    GlInt uniformAlignment = 0;
    GlInt maxSamples = 0;
    GlInt maxColorAttachments = 0;
    functions.getIntegerv(
        GlConstant::MaxTextureSize, &maxTextureSize);
    functions.getIntegerv(
        GlConstant::MaxArrayTextureLayers,
        &maxArrayTextureLayers);
    functions.getIntegerv(
        GlConstant::MaxCombinedTextureImageUnits, &maxTextureUnits);
    functions.getIntegerv(
        GlConstant::MaxVertexAttribs, &maxVertexAttributes);
    functions.getIntegerv(
        GlConstant::MaxUniformBlockSize, &maxUniformBlockSize);
    functions.getIntegerv(
        GlConstant::MaxUniformBufferBindings,
        &maxUniformBufferBindings);
    functions.getIntegerv(
        GlConstant::UniformBufferOffsetAlignment, &uniformAlignment);
    functions.getIntegerv(GlConstant::MaxSamples, &maxSamples);
    functions.getIntegerv(
        GlConstant::MaxColorAttachments, &maxColorAttachments);

    GlCapabilities capabilities;
    capabilities.majorVersion = PositiveValue(major);
    capabilities.minorVersion = PositiveValue(minor);
    capabilities.contextFlags =
        static_cast<std::uint32_t>(flags);
    capabilities.profileMask =
        static_cast<std::uint32_t>(profile);
    capabilities.contextGeneration = contract.generation;
    capabilities.coreProfile = true;
    capabilities.debugContext =
        (flags & GlConstant::ContextFlagDebugBit) != 0;
    capabilities.supportsSamplerObjects = true;
    capabilities.supportsSyncObjects = true;
    capabilities.supportsInstancing = true;
    capabilities.limits.maxTextureSize =
        PositiveValue(maxTextureSize);
    capabilities.limits.maxArrayTextureLayers =
        PositiveValue(maxArrayTextureLayers);
    capabilities.limits.maxCombinedTextureUnits =
        PositiveValue(maxTextureUnits);
    capabilities.limits.maxVertexAttributes =
        PositiveValue(maxVertexAttributes);
    capabilities.limits.maxUniformBlockSize =
        PositiveValue(maxUniformBlockSize);
    capabilities.limits.maxUniformBufferBindings =
        PositiveValue(maxUniformBufferBindings);
    capabilities.limits.uniformBufferOffsetAlignment =
        PositiveValue(uniformAlignment);
    capabilities.limits.maxSamples = PositiveValue(maxSamples);
    capabilities.limits.maxColorAttachments =
        PositiveValue(maxColorAttachments);

    const GlLimits& limits = capabilities.limits;
    if (limits.maxTextureSize == 0U ||
        limits.maxArrayTextureLayers == 0U ||
        limits.maxCombinedTextureUnits == 0U ||
        limits.maxVertexAttributes == 0U ||
        limits.maxUniformBlockSize == 0U ||
        limits.maxUniformBufferBindings == 0U ||
        limits.uniformBufferOffsetAlignment == 0U ||
        limits.maxColorAttachments == 0U) {
        return InvalidState(
            "OpenGL context reported incomplete Core Profile limits");
    }
    return capabilities;
}

} // namespace Aero::Rhi
