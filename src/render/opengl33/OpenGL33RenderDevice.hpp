#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <AeroRender/Texture.hpp>
#include <AeroRender/OpenGL33.hpp>

#include "render/common/StateCache.hpp"

#include <cstddef>
#include <cstdint>

namespace Aero::Render {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;
using GLfloat = float;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLchar = char;
using GLubyte = unsigned char;

struct GLCallbacks {
    const GLubyte* (*glGetString)(GLenum name);
    GLenum (*glGetError)(void);
    void (*glGenBuffers)(GLsizei n, GLuint* buffers);
    void (*glDeleteBuffers)(GLsizei n, const GLuint* buffers);
    void (*glBindBuffer)(GLenum target, GLuint buffer);
    void (*glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    void (*glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
    void (*glGenVertexArrays)(GLsizei n, GLuint* arrays);
    void (*glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
    void (*glBindVertexArray)(GLuint array);
    void (*glVertexAttribPointer)(GLuint index, GLint size, GLenum type,
        GLboolean normalized, GLsizei stride, const void* pointer);
    void (*glEnableVertexAttribArray)(GLuint index);
    void (*glDisableVertexAttribArray)(GLuint index);
    void (*glVertexAttrib1f)(GLuint index, GLfloat x);
    void (*glVertexAttrib2f)(GLuint index, GLfloat x, GLfloat y);
    void (*glVertexAttrib4f)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    GLuint (*glCreateShader)(GLenum type);
    void (*glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
    void (*glCompileShader)(GLuint shader);
    void (*glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
    void (*glGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
    void (*glDeleteShader)(GLuint shader);
    GLuint (*glCreateProgram)(void);
    void (*glAttachShader)(GLuint program, GLuint shader);
    void (*glLinkProgram)(GLuint program);
    void (*glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
    void (*glGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
    void (*glDeleteProgram)(GLuint program);
    void (*glUseProgram)(GLuint program);
    GLint (*glGetUniformLocation)(GLuint program, const GLchar* name);
    void (*glUniform1i)(GLint location, GLint v0);
    void (*glUniform1f)(GLint location, GLfloat v0);
    void (*glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
    void (*glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void (*glGenTextures)(GLsizei n, GLuint* textures);
    void (*glDeleteTextures)(GLsizei n, const GLuint* textures);
    void (*glBindTexture)(GLenum target, GLuint texture);
    void (*glActiveTexture)(GLenum texture);
    void (*glTexImage2D)(GLenum target, GLint level, GLint internalFormat,
        GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    void (*glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
    void (*glTexParameteri)(GLenum target, GLenum pname, GLint param);
    void (*glPixelStorei)(GLenum pname, GLint param);
    void (*glGenSamplers)(GLsizei n, GLuint* samplers);
    void (*glDeleteSamplers)(GLsizei n, const GLuint* samplers);
    void (*glBindSampler)(GLuint unit, GLuint sampler);
    void (*glSamplerParameteri)(GLuint sampler, GLenum pname, GLint param);
    void (*glSamplerParameterfv)(GLuint sampler, GLenum pname, const GLfloat* param);
    void (*glGenFramebuffers)(GLsizei n, GLuint* framebuffers);
    void (*glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
    void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
    void (*glFramebufferTexture2D)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level);
    void (*glFramebufferRenderbuffer)(GLenum target, GLenum attachment,
        GLenum renderbuffertarget, GLuint renderbuffer);
    GLenum (*glCheckFramebufferStatus)(GLenum target);
    void (*glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
    void (*glDeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
    void (*glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
    void (*glRenderbufferStorage)(GLenum target, GLenum internalFormat,
        GLsizei width, GLsizei height);
    void (*glEnable)(GLenum cap);
    void (*glDisable)(GLenum cap);
    void (*glBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB,
        GLenum sfactorAlpha, GLenum dfactorAlpha);
    void (*glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void (*glClear)(GLbitfield mask);
    void (*glClearStencil)(GLint s);
    void (*glStencilFunc)(GLenum func, GLint ref, GLuint mask);
    void (*glStencilOp)(GLenum sfail, GLenum dpfail, GLenum dppass);
    void (*glStencilMask)(GLuint mask);
    void (*glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void (*glStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
    void (*glStencilOpSeparate)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
    void (*glStencilMaskSeparate)(GLenum face, GLuint mask);
    void (*glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
    void (*glDrawElementsBaseVertex)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex);
    void* (*glMapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
    GLboolean (*glUnmapBuffer)(GLenum target);
};

class OpenGL33Texture final : public Texture {
public:
    OpenGL33Texture(
        Ref<RenderDevice> device,
        unsigned int textureId,
        uint32_t width,
        uint32_t height,
        bool hasMipMaps,
        bool hasAlpha,
        uint32_t glFormat = 0x1908U /* GL_RGBA */,
        const GLCallbacks* gl = nullptr) noexcept;
    ~OpenGL33Texture() noexcept override;

    uint32_t GetWidth() const noexcept override { return width_; }
    uint32_t GetHeight() const noexcept override { return height_; }
    bool HasMipMaps() const noexcept override { return hasMipMaps_; }
    bool IsInverted() const noexcept override { return false; }
    bool HasAlpha() const noexcept override { return hasAlpha_; }

    unsigned int GetNativeTexture() const noexcept { return textureId_; }
    uint32_t GetGLFormat() const noexcept { return glFormat_; }

private:
    Ref<RenderDevice> device_;
    const GLCallbacks* gl_ = nullptr;
    unsigned int textureId_ = 0;
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
    bool hasMipMaps_ = false;
    bool hasAlpha_ = true;
    uint32_t glFormat_ = 0x1908U;
};

class OpenGL33RenderTarget final : public RenderTarget {
public:
    OpenGL33RenderTarget(
        Ref<RenderDevice> device,
        Ref<OpenGL33Texture> texture,
        unsigned int fboId,
        unsigned int rboId,
        uint32_t width,
        uint32_t height,
        bool defaultFbo = false,
        const GLCallbacks* gl = nullptr) noexcept;
    ~OpenGL33RenderTarget() noexcept override;

    Texture* GetTexture() noexcept override { return texture_.Get(); }
    unsigned int GetFBO() const noexcept { return fboId_; }
    unsigned int GetRBO() const noexcept { return rboId_; }
    uint32_t GetWidth() const noexcept { return width_; }
    uint32_t GetHeight() const noexcept { return height_; }
    bool IsDefaultFBO() const noexcept { return defaultFbo_; }

    void SetSize(uint32_t width, uint32_t height) noexcept {
        width_ = width;
        height_ = height;
    }

private:
    const GLCallbacks* gl_ = nullptr;
    Ref<OpenGL33Texture> texture_;
    unsigned int fboId_ = 0;
    unsigned int rboId_ = 0;
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
    bool defaultFbo_ = false;
};

class OpenGL33RenderDevice final : public Aero::Render::RenderDeviceBase {
public:
    explicit OpenGL33RenderDevice(
        const OpenGL33::DeviceOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~OpenGL33RenderDevice() noexcept override;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    const DeviceCaps& GetCaps() const noexcept override { return caps_; }
    const GLCallbacks& Callbacks() const noexcept { return gl_; }

    Ref<RenderTarget> CreateRenderTarget(
        const char* label, uint32_t width, uint32_t height,
        uint32_t sampleCount, bool needsStencil) noexcept override;

    Ref<RenderTarget> CloneRenderTarget(
        const char* label, RenderTarget* surface) noexcept override;

    Ref<Texture> CreateTexture(
        const char* label, uint32_t width, uint32_t height,
        uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept override;

    void BeginUpdatingTextures() noexcept override;
    void UpdateTexture(
        Texture* texture, uint32_t level, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height, const void* data) noexcept override;
    void EndUpdatingTextures(
        Texture** textures, uint32_t count) noexcept override;

    void BeginOffscreenRender() noexcept override;
    void EndOffscreenRender() noexcept override;
    void BeginOnscreenRender() noexcept override;
    void EndOnscreenRender() noexcept override;

    void SetRenderTarget(RenderTarget* surface) noexcept override;
    void BeginTile(RenderTarget* surface, const Tile& tile) noexcept override;
    void EndTile(RenderTarget* surface) noexcept override;
    void ResolveRenderTarget(
        RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept override;

    void* MapVertices(uint32_t bytes) noexcept override;
    void UnmapVertices() noexcept override;
    void* MapIndices(uint32_t bytes) noexcept override;
    void UnmapIndices() noexcept override;

    void DrawBatch(const Batch& batch) noexcept override;

protected:
    RenderBackendKind BackendKind() const noexcept override {
        return RenderBackendKind::OpenGL33;
    }

private:
    void ClearTextures() noexcept;
    void SetVertexFormat(uint8_t format, uintptr_t offset) noexcept;

    OpenGL33::DeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;

    unsigned int vao_ = 0;
    unsigned int dynamicVB_ = 0;
    unsigned int dynamicIB_ = 0;

    RingBufferTracker vbRing_{};
    RingBufferTracker ibRing_{};
    uint32_t activeVBBytes_ = 0U;
    uint32_t activeIBBytes_ = 0U;

    GLCallbacks gl_{};
    bool glResolved_ = false;

    // Program handles
    unsigned int programs_[Shader::Count] = {};
    struct ProgramLocations {
        int uViewportSize = -1;
        int uFlipY = -1;
        int uTextureSize = -1;
        int uPaint0 = -1;
        int uPaint1 = -1;
        int uEffect0 = -1;
        int uEffect1 = -1;
        int uEffectColor = -1;
    };
    ProgramLocations programLocations_[Shader::Count]{};
    ProgramLocations customEffectLocations_{};
    unsigned int customEffectProgram_ = 0;
    unsigned int currentProgram_ = 0;

    // Sampler objects indexed by SamplerState.v (same encoding as D3D11).
    unsigned int samplers_[64] = {};

    uint32_t viewportWidth_ = 0U;
    uint32_t viewportHeight_ = 0U;
    uint32_t offscreenDepth_ = 0U;
    bool renderingOffscreen_ = false;

    // Uniform hash cache to short-circuit glUniform calls
    uint32_t cachedViewportWidth_ = 0U;
    uint32_t cachedViewportHeight_ = 0U;
    float cachedFlipY_ = -1.0f;
    uint32_t cachedTextureSizeWidth_ = 0U;
    uint32_t cachedTextureSizeHeight_ = 0U;
    uint32_t cachedPaint0Hash_ = 0U;
    uint32_t cachedEffectColorHash_ = 0U;

    DeviceCaps caps_{};
    OpenGL33RenderTarget* currentTarget_ = nullptr;
    StateCache stateCache_{};
};

} // namespace Aero::Render
