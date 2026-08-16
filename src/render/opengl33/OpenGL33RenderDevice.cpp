#include "OpenGL33RenderDevice.hpp"
#include <cstddef>
#include <cstring>
#include <algorithm>

namespace Aero::Render {

namespace {

// GL type aliases kept local so the backend never depends on platform GL headers.
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

// OpenGL 3.3 core constants.
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
constexpr GLenum GL_STREAM_DRAW = 0x88E0;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B32;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_UNSIGNED_SHORT = 0x1403;
constexpr GLboolean GL_TRUE = 1;
constexpr GLboolean GL_FALSE = 0;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_RED = 0x1903;
constexpr GLint GL_RGBA8 = 0x8058;
constexpr GLint GL_R8 = 0x8229;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_TEXTURE_BORDER_COLOR = 0x1004;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_CLAMP_TO_BORDER = 0x812D;
constexpr GLenum GL_REPEAT = 0x2901;
constexpr GLenum GL_MIRRORED_REPEAT = 0x8370;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_ONE = 1;
constexpr GLenum GL_ZERO = 0;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_DST_ALPHA = 0x0304;
constexpr GLenum GL_ONE_MINUS_DST_ALPHA = 0x0305;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_CULL_FACE = 0x0B44;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_RENDERBUFFER = 0x8D41;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_DEPTH_STENCIL_ATTACHMENT = 0x821A;
constexpr GLenum GL_DEPTH24_STENCIL8 = 0x88F0;
constexpr GLenum GL_DEPTH_STENCIL = 0x84F9;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
constexpr GLenum GL_VENDOR = 0x1F00;
constexpr GLenum GL_VERSION = 0x1F02;

// Vertex2D layout (24 bytes): float2 pos @0, uint32 color @8, float2 uv @12,
// float coverage @20. Matches UiFrameEncoder::Vertex2D and the D3D11 backend.
constexpr GLint VertexStride = 24;
constexpr GLintptr ColorOffset = 8;
constexpr GLintptr UVOffset = 12;
constexpr GLintptr CoverageOffset = 20;

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
    void (*glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
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
    void (*glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void (*glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
};

// Resolves the OpenGL 3.3 core entry points through the host-provided loader.
bool ResolveCallbacks(
    OpenGL33::ProcResolver resolve,
    void* context,
    GLCallbacks& gl) noexcept {
    if (resolve == nullptr) return false;
    auto load = [&](auto& target, const char* name) noexcept {
        OpenGL33::ProcAddress address = resolve(context, name);
        if (address == nullptr) return false;
        target = reinterpret_cast<decltype(target)>(address);
        return target != nullptr;
    };
    return
        load(gl.glGetString, "glGetString") &&
        load(gl.glGetError, "glGetError") &&
        load(gl.glGenBuffers, "glGenBuffers") &&
        load(gl.glDeleteBuffers, "glDeleteBuffers") &&
        load(gl.glBindBuffer, "glBindBuffer") &&
        load(gl.glBufferData, "glBufferData") &&
        load(gl.glBufferSubData, "glBufferSubData") &&
        load(gl.glGenVertexArrays, "glGenVertexArrays") &&
        load(gl.glDeleteVertexArrays, "glDeleteVertexArrays") &&
        load(gl.glBindVertexArray, "glBindVertexArray") &&
        load(gl.glVertexAttribPointer, "glVertexAttribPointer") &&
        load(gl.glEnableVertexAttribArray, "glEnableVertexAttribArray") &&
        load(gl.glDisableVertexAttribArray, "glDisableVertexAttribArray") &&
        load(gl.glCreateShader, "glCreateShader") &&
        load(gl.glShaderSource, "glShaderSource") &&
        load(gl.glCompileShader, "glCompileShader") &&
        load(gl.glGetShaderiv, "glGetShaderiv") &&
        load(gl.glGetShaderInfoLog, "glGetShaderInfoLog") &&
        load(gl.glDeleteShader, "glDeleteShader") &&
        load(gl.glCreateProgram, "glCreateProgram") &&
        load(gl.glAttachShader, "glAttachShader") &&
        load(gl.glLinkProgram, "glLinkProgram") &&
        load(gl.glGetProgramiv, "glGetProgramiv") &&
        load(gl.glGetProgramInfoLog, "glGetProgramInfoLog") &&
        load(gl.glDeleteProgram, "glDeleteProgram") &&
        load(gl.glUseProgram, "glUseProgram") &&
        load(gl.glGetUniformLocation, "glGetUniformLocation") &&
        load(gl.glUniform2f, "glUniform2f") &&
        load(gl.glGenTextures, "glGenTextures") &&
        load(gl.glDeleteTextures, "glDeleteTextures") &&
        load(gl.glBindTexture, "glBindTexture") &&
        load(gl.glActiveTexture, "glActiveTexture") &&
        load(gl.glTexImage2D, "glTexImage2D") &&
        load(gl.glTexSubImage2D, "glTexSubImage2D") &&
        load(gl.glTexParameteri, "glTexParameteri") &&
        load(gl.glPixelStorei, "glPixelStorei") &&
        load(gl.glGenSamplers, "glGenSamplers") &&
        load(gl.glDeleteSamplers, "glDeleteSamplers") &&
        load(gl.glBindSampler, "glBindSampler") &&
        load(gl.glSamplerParameteri, "glSamplerParameteri") &&
        load(gl.glSamplerParameterfv, "glSamplerParameterfv") &&
        load(gl.glGenFramebuffers, "glGenFramebuffers") &&
        load(gl.glDeleteFramebuffers, "glDeleteFramebuffers") &&
        load(gl.glBindFramebuffer, "glBindFramebuffer") &&
        load(gl.glFramebufferTexture2D, "glFramebufferTexture2D") &&
        load(gl.glFramebufferRenderbuffer, "glFramebufferRenderbuffer") &&
        load(gl.glCheckFramebufferStatus, "glCheckFramebufferStatus") &&
        load(gl.glGenRenderbuffers, "glGenRenderbuffers") &&
        load(gl.glDeleteRenderbuffers, "glDeleteRenderbuffers") &&
        load(gl.glBindRenderbuffer, "glBindRenderbuffer") &&
        load(gl.glRenderbufferStorage, "glRenderbufferStorage") &&
        load(gl.glEnable, "glEnable") &&
        load(gl.glDisable, "glDisable") &&
        load(gl.glBlendFuncSeparate, "glBlendFuncSeparate") &&
        load(gl.glViewport, "glViewport") &&
        load(gl.glScissor, "glScissor") &&
        load(gl.glClearColor, "glClearColor") &&
        load(gl.glClear, "glClear") &&
        load(gl.glColorMask, "glColorMask") &&
        load(gl.glDrawElements, "glDrawElements");
}

// OpenGL function table shared by the device and its resources. Populated on
// the first device Initialize; GL entry points do not vary between contexts.
GLCallbacks g_gl{};
bool g_glResolved = false;

GLuint CompileShader(
    const GLCallbacks& gl,
    GLenum type,
    const char* source) noexcept {
    if (source == nullptr) return 0;
    GLuint shader = gl.glCreateShader(type);
    if (shader == 0) return 0;
    const GLchar* sources[] = {source};
    gl.glShaderSource(shader, 1, sources, nullptr);
    gl.glCompileShader(shader);
    GLint compiled = GL_FALSE;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        gl.glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint LinkProgram(
    const GLCallbacks& gl,
    GLuint vertexShader,
    GLuint fragmentShader) noexcept {
    GLuint program = gl.glCreateProgram();
    if (program == 0) return 0;
    gl.glAttachShader(program, vertexShader);
    gl.glAttachShader(program, fragmentShader);
    gl.glLinkProgram(program);
    GLint linked = GL_FALSE;
    gl.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        gl.glDeleteProgram(program);
        return 0;
    }
    return program;
}

constexpr const char* VertexShaderSource =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "layout(location = 2) in vec2 aUV;\n"
    "layout(location = 3) in float aCoverage;\n"
    "uniform vec2 uViewportSize;\n"
    "out vec4 vColor;\n"
    "out vec2 vUV;\n"
    "out float vCoverage;\n"
    "void main() {\n"
    "    vec2 ndc = vec2(aPosition.x / uViewportSize.x * 2.0 - 1.0,\n"
    "                    1.0 - aPosition.y / uViewportSize.y * 2.0);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    vColor = aColor;\n"
    "    vUV = aUV;\n"
    "    vCoverage = aCoverage;\n"
    "}\n";

constexpr const char* SolidPixelShaderSource =
    "#version 330 core\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec4 color = vColor;\n"
    "    color.a *= vCoverage;\n"
    "    fragColor = color;\n"
    "}\n";

constexpr const char* PatternPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec4 color = texture(uTexture, vUV) * vColor;\n"
    "    color.a *= vCoverage;\n"
    "    fragColor = color;\n"
    "}\n";

constexpr const char* SDFPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    float distance = texture(uTexture, vUV).r;\n"
    "    float smoothing = max(fwidth(distance), 1.0 / 512.0);\n"
    "    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);\n"
    "    vec4 color = vColor;\n"
    "    color.a *= alpha * vCoverage;\n"
    "    fragColor = color;\n"
    "}\n";

} // namespace

OpenGL33Texture::OpenGL33Texture(
    unsigned int textureId,
    uint32_t width,
    uint32_t height,
    bool hasMipMaps,
    bool hasAlpha,
    uint32_t glFormat) noexcept
    : textureId_(textureId), width_(width), height_(height),
      hasMipMaps_(hasMipMaps), hasAlpha_(hasAlpha), glFormat_(glFormat) {}

OpenGL33Texture::~OpenGL33Texture() noexcept {
    if (textureId_ != 0 && g_glResolved && g_gl.glDeleteTextures != nullptr) {
        GLuint texture = textureId_;
        g_gl.glDeleteTextures(1, &texture);
    }
}

OpenGL33RenderTarget::OpenGL33RenderTarget(
    Ref<RenderDevice> device,
    Ref<OpenGL33Texture> texture,
    unsigned int fboId,
    unsigned int rboId,
    uint32_t width,
    uint32_t height,
    bool defaultFbo) noexcept
    : texture_(std::move(texture)), fboId_(fboId), rboId_(rboId),
      width_(width), height_(height), defaultFbo_(defaultFbo) {
    device_ = std::move(device);
    kind_ = defaultFbo ? RenderTargetKind::Window : RenderTargetKind::Embedded;
    state_ = RenderTargetState::Ready;
}

OpenGL33RenderTarget::~OpenGL33RenderTarget() noexcept {
    if (g_glResolved) {
        if (rboId_ != 0 && g_gl.glDeleteRenderbuffers != nullptr) {
            GLuint rbo = rboId_;
            g_gl.glDeleteRenderbuffers(1, &rbo);
        }
        if (fboId_ != 0 && g_gl.glDeleteFramebuffers != nullptr) {
            GLuint fbo = fboId_;
            g_gl.glDeleteFramebuffers(1, &fbo);
        }
    }
}

OpenGL33RenderDevice::OpenGL33RenderDevice(
    const OpenGL33::DeviceOptions& options,
    Base::IAllocator* allocator) noexcept
    : options_(options), allocator_(allocator) {
    backend_ = RenderBackendKind::OpenGL33;
    caps_.centerPixelOffset = 0.0f;
    caps_.linearRendering = false;
    caps_.subpixelRendering = false;
    caps_.depthRangeZeroToOne = false;
    caps_.clipSpaceYInverted = false;
}

OpenGL33RenderDevice::~OpenGL33RenderDevice() noexcept {
    Shutdown();
}

Base::Result<void> OpenGL33RenderDevice::Initialize() noexcept {
    if (vao_ != 0) return {};

    if (!g_glResolved) {
        if (!ResolveCallbacks(
                options_.resolve, options_.callbackContext, g_gl)) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Unable to resolve the OpenGL 3.3 core entry points");
        }
        g_glResolved = true;
    }
    const GLCallbacks& gl = g_gl;

    GLuint vertexShader = CompileShader(gl, GL_VERTEX_SHADER, VertexShaderSource);
    if (vertexShader == 0) {
        return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to compile the GL vertex shader");
    }
    GLuint solidFS = CompileShader(gl, GL_FRAGMENT_SHADER, SolidPixelShaderSource);
    GLuint patternFS = CompileShader(gl, GL_FRAGMENT_SHADER, PatternPixelShaderSource);
    GLuint sdfFS = CompileShader(gl, GL_FRAGMENT_SHADER, SDFPixelShaderSource);
    if (solidFS == 0 || patternFS == 0 || sdfFS == 0) {
        if (solidFS != 0) gl.glDeleteShader(solidFS);
        if (patternFS != 0) gl.glDeleteShader(patternFS);
        if (sdfFS != 0) gl.glDeleteShader(sdfFS);
        gl.glDeleteShader(vertexShader);
        return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to compile a GL fragment shader");
    }

    solidProgram_ = LinkProgram(gl, vertexShader, solidFS);
    patternProgram_ = LinkProgram(gl, vertexShader, patternFS);
    sdfProgram_ = LinkProgram(gl, vertexShader, sdfFS);
    gl.glDeleteShader(solidFS);
    gl.glDeleteShader(patternFS);
    gl.glDeleteShader(sdfFS);
    gl.glDeleteShader(vertexShader);
    if (solidProgram_ == 0 || patternProgram_ == 0 || sdfProgram_ == 0) {
        Shutdown();
        return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to link a GL program");
    }

    gl.glGenVertexArrays(1, &vao_);
    gl.glGenBuffers(1, &dynamicVB_);
    gl.glGenBuffers(1, &dynamicIB_);
    if (vao_ == 0 || dynamicVB_ == 0 || dynamicIB_ == 0) {
        Shutdown();
        return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to create the GL vertex buffers");
    }

    gl.glBindVertexArray(vao_);
    gl.glBindBuffer(GL_ARRAY_BUFFER, dynamicVB_);
    gl.glBufferData(GL_ARRAY_BUFFER, DYNAMIC_VB_SIZE, nullptr, GL_DYNAMIC_DRAW);
    gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamicIB_);
    gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, DYNAMIC_IB_SIZE, nullptr, GL_DYNAMIC_DRAW);

    gl.glEnableVertexAttribArray(0);
    gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VertexStride, nullptr);
    gl.glEnableVertexAttribArray(1);
    gl.glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, VertexStride, reinterpret_cast<const void*>(ColorOffset));
    gl.glEnableVertexAttribArray(2);
    gl.glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, VertexStride, reinterpret_cast<const void*>(UVOffset));
    gl.glEnableVertexAttribArray(3);
    gl.glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, VertexStride, reinterpret_cast<const void*>(CoverageOffset));
    gl.glBindVertexArray(0);

    // Create the 64 sampler states indexed by SamplerState.v
    // (wrapMode:3 bits | minmagFilter:1 << 3 | mipFilter:2 << 4).
    gl.glGenSamplers(64, samplers_);
    for (uint8_t v = 0; v < 64; ++v) {
        const uint8_t wrapMode = v & 0x7;
        const uint8_t minmag = (v >> 3) & 0x1;
        const uint8_t mip = (v >> 4) & 0x3;

        const GLint filter = (minmag == MinMagFilter::Linear)
            ? static_cast<GLint>(GL_LINEAR)
            : static_cast<GLint>(GL_NEAREST);
        g_gl.glSamplerParameteri(samplers_[v], GL_TEXTURE_MAG_FILTER, filter);
        g_gl.glSamplerParameteri(samplers_[v], GL_TEXTURE_MIN_FILTER, filter);

        GLint wrapS = GL_CLAMP_TO_EDGE;
        GLint wrapT = GL_CLAMP_TO_EDGE;
        switch (wrapMode) {
        case WrapMode::ClampToZero:
            wrapS = GL_CLAMP_TO_BORDER;
            wrapT = GL_CLAMP_TO_BORDER;
            {
                const GLfloat borderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                g_gl.glSamplerParameterfv(samplers_[v], GL_TEXTURE_BORDER_COLOR, borderColor);
            }
            break;
        case WrapMode::Repeat:
            wrapS = GL_REPEAT;
            wrapT = GL_REPEAT;
            break;
        case WrapMode::MirrorU:
            wrapS = GL_MIRRORED_REPEAT;
            break;
        case WrapMode::MirrorV:
            wrapT = GL_MIRRORED_REPEAT;
            break;
        case WrapMode::Mirror:
            wrapS = GL_MIRRORED_REPEAT;
            wrapT = GL_MIRRORED_REPEAT;
            break;
        case WrapMode::ClampToEdge:
        default:
            break;
        }
        static_cast<void>(mip);
        g_gl.glSamplerParameteri(samplers_[v], GL_TEXTURE_WRAP_S, wrapS);
        g_gl.glSamplerParameteri(samplers_[v], GL_TEXTURE_WRAP_T, wrapT);
    }

    gl.glDisable(GL_CULL_FACE);
    gl.glDisable(GL_DEPTH_TEST);
    gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    state_ = RenderDeviceState::Ready;
    return {};
}

void OpenGL33RenderDevice::Shutdown() noexcept {
    if (g_glResolved) {
        const GLCallbacks& gl = g_gl;
        if (samplers_[0] != 0 && gl.glDeleteSamplers != nullptr) {
            gl.glDeleteSamplers(64, samplers_);
            for (int i = 0; i < 64; ++i) samplers_[i] = 0;
        }
        if (solidProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(solidProgram_);
        if (patternProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(patternProgram_);
        if (sdfProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(sdfProgram_);
        solidProgram_ = 0;
        patternProgram_ = 0;
        sdfProgram_ = 0;
        currentProgram_ = 0;
        if (dynamicVB_ != 0 && gl.glDeleteBuffers != nullptr) gl.glDeleteBuffers(1, &dynamicVB_);
        if (dynamicIB_ != 0 && gl.glDeleteBuffers != nullptr) gl.glDeleteBuffers(1, &dynamicIB_);
        if (vao_ != 0 && gl.glDeleteVertexArrays != nullptr) gl.glDeleteVertexArrays(1, &vao_);
        dynamicVB_ = 0;
        dynamicIB_ = 0;
        vao_ = 0;
    }
    currentTarget_ = nullptr;
    state_ = RenderDeviceState::Shutdown;
}

Ref<RenderTarget> OpenGL33RenderDevice::CreateRenderTarget(
    const char* label, uint32_t width, uint32_t height,
    uint32_t sampleCount, bool needsStencil) noexcept {
    static_cast<void>(label);
    static_cast<void>(sampleCount);
    if (vao_ == 0 || width == 0 || height == 0) return {};

    const GLCallbacks& gl = g_gl;

    GLuint texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
        static_cast<GLsizei>(width), static_cast<GLsizei>(height),
        0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    GLuint rbo = 0;
    if (needsStencil) {
        gl.glGenRenderbuffers(1, &rbo);
        gl.glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        gl.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        gl.glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
        gl.glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    const GLenum status = gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (rbo != 0) gl.glDeleteRenderbuffers(1, &rbo);
        gl.glDeleteFramebuffers(1, &fbo);
        gl.glDeleteTextures(1, &texture);
        return {};
    }

    Ref<OpenGL33Texture> glTex = Base::MakeRef<OpenGL33Texture>(
        texture, width, height, false, true, GL_RGBA).Value();

    Ref<RenderDevice> self = Ref<RenderDevice>::FromBorrowed(*this);
    return Base::MakeRef<OpenGL33RenderTarget>(
        std::move(self), std::move(glTex), fbo, rbo, width, height, false).Value();
}

Ref<RenderTarget> OpenGL33RenderDevice::CloneRenderTarget(
    const char* label, RenderTarget* surface) noexcept {
    if (surface == nullptr) return {};
    auto* src = static_cast<OpenGL33RenderTarget*>(surface);
    return CreateRenderTarget(label, src->GetWidth(), src->GetHeight(), 1, src->GetRBO() != 0);
}

Ref<Texture> OpenGL33RenderDevice::CreateTexture(
    const char* label, uint32_t width, uint32_t height,
    uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept {
    static_cast<void>(label);
    if (vao_ == 0 || width == 0 || height == 0) return {};

    const GLCallbacks& gl = g_gl;
    const bool isR8 = format == TextureFormat::R8;
    const GLint internalFormat = isR8 ? GL_R8 : GL_RGBA8;
    const GLenum glFormat = isR8 ? GL_RED : GL_RGBA;
    const void* initial = (data != nullptr && data[0] != nullptr) ? data[0] : nullptr;

    GLuint texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
        static_cast<GLsizei>(width), static_cast<GLsizei>(height),
        0, glFormat, GL_UNSIGNED_BYTE, initial);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glBindTexture(GL_TEXTURE_2D, 0);

    return Base::MakeRef<OpenGL33Texture>(
        texture, width, height, numLevels > 1, format != TextureFormat::RGBX8, glFormat).Value();
}

void OpenGL33RenderDevice::BeginUpdatingTextures() noexcept {}

void OpenGL33RenderDevice::UpdateTexture(
    Texture* texture, uint32_t level, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, const void* data) noexcept {
    if (vao_ == 0 || texture == nullptr || data == nullptr) return;
    auto* glTex = static_cast<OpenGL33Texture*>(texture);

    g_gl.glBindTexture(GL_TEXTURE_2D, glTex->GetNativeTexture());
    g_gl.glTexSubImage2D(GL_TEXTURE_2D, static_cast<GLint>(level),
        static_cast<GLint>(x), static_cast<GLint>(y),
        static_cast<GLsizei>(width), static_cast<GLsizei>(height),
        glTex->GetGLFormat(), GL_UNSIGNED_BYTE, data);
    g_gl.glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGL33RenderDevice::EndUpdatingTextures(Texture** textures, uint32_t count) noexcept {
    static_cast<void>(textures);
    static_cast<void>(count);
}

void OpenGL33RenderDevice::BeginOffscreenRender() noexcept {}
void OpenGL33RenderDevice::EndOffscreenRender() noexcept {}
void OpenGL33RenderDevice::BeginOnscreenRender() noexcept {}
void OpenGL33RenderDevice::EndOnscreenRender() noexcept {}

void OpenGL33RenderDevice::SetRenderTarget(RenderTarget* surface) noexcept {
    if (vao_ == 0) return;
    currentTarget_ = static_cast<OpenGL33RenderTarget*>(surface);
    if (currentTarget_ == nullptr) return;

    viewportWidth_ = currentTarget_->GetWidth();
    viewportHeight_ = currentTarget_->GetHeight();
    g_gl.glBindFramebuffer(GL_FRAMEBUFFER, currentTarget_->GetFBO());
    g_gl.glViewport(0, 0,
        static_cast<GLsizei>(viewportWidth_), static_cast<GLsizei>(viewportHeight_));

    if (currentTarget_->IsDefaultFBO()) {
        g_gl.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        g_gl.glClear(GL_COLOR_BUFFER_BIT);
    }
}

void OpenGL33RenderDevice::BeginTile(RenderTarget* surface, const Tile& tile) noexcept {
    static_cast<void>(surface);
    if (vao_ == 0) return;
    // Scissor origin is bottom-left; the encoder issues top-left coordinates.
    const GLint y = static_cast<GLint>(viewportHeight_) -
        static_cast<GLint>(tile.y + tile.height);
    g_gl.glScissor(static_cast<GLint>(tile.x), y,
        static_cast<GLsizei>(tile.width), static_cast<GLsizei>(tile.height));
    g_gl.glEnable(GL_SCISSOR_TEST);
}

void OpenGL33RenderDevice::EndTile(RenderTarget* surface) noexcept {
    static_cast<void>(surface);
    if (vao_ == 0) return;
    g_gl.glDisable(GL_SCISSOR_TEST);
}

void OpenGL33RenderDevice::ResolveRenderTarget(
    RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept {
    static_cast<void>(surface);
    static_cast<void>(tiles);
    static_cast<void>(numTiles);
}

void* OpenGL33RenderDevice::MapVertices(uint32_t bytes) noexcept {
    static_cast<void>(bytes);
    return mappedVBMemory_;
}

void OpenGL33RenderDevice::UnmapVertices() noexcept {}

void* OpenGL33RenderDevice::MapIndices(uint32_t bytes) noexcept {
    static_cast<void>(bytes);
    return mappedIBMemory_;
}

void OpenGL33RenderDevice::UnmapIndices() noexcept {}

void OpenGL33RenderDevice::DrawBatch(const Batch& batch) noexcept {
    if (vao_ == 0 || batch.numIndices == 0U) return;

    unsigned int program = solidProgram_;
    GLuint texture = 0;
    GLuint sampler = 0;
    switch (batch.shader.v) {
    case Shader::Path_Pattern:
        program = patternProgram_;
        if (batch.image != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.image)->GetNativeTexture();
            sampler = samplers_[batch.imageSampler.v & 0x3F];
        }
        break;
    case Shader::SDF_Solid:
        program = sdfProgram_;
        if (batch.glyphs != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.glyphs)->GetNativeTexture();
            sampler = samplers_[batch.glyphsSampler.v & 0x3F];
        }
        break;
    case Shader::Path_Solid:
    case Shader::Path_AA_Solid:
    default:
        break;
    }

    if (program == 0) return;

    if (currentProgram_ != program) {
        g_gl.glUseProgram(program);
        currentProgram_ = program;
    }

    const GLint viewportLocation = g_gl.glGetUniformLocation(program, "uViewportSize");
    if (viewportLocation != -1) {
        g_gl.glUniform2f(viewportLocation,
            static_cast<GLfloat>(viewportWidth_),
            static_cast<GLfloat>(viewportHeight_));
    }

    g_gl.glBindBuffer(GL_ARRAY_BUFFER, dynamicVB_);
    g_gl.glBufferSubData(GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(batch.numVertices) * VertexStride,
        mappedVBMemory_ + batch.vertexOffset);
    g_gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamicIB_);
    g_gl.glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(batch.numIndices) * sizeof(uint16_t),
        mappedIBMemory_ + static_cast<GLsizeiptr>(batch.startIndex) * sizeof(uint16_t));

    // Blend state.
    const uint8_t blendMode = batch.renderState.f.blendMode;
    if (blendMode == BlendMode::Src) {
        g_gl.glDisable(GL_BLEND);
    } else {
        g_gl.glEnable(GL_BLEND);
        if (blendMode == BlendMode::SrcOver_Additive) {
            g_gl.glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
        } else {
            g_gl.glBlendFuncSeparate(
                GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
                GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    const GLboolean colorMask = batch.renderState.f.colorEnable != 0 ? GL_TRUE : GL_FALSE;
    g_gl.glColorMask(colorMask, colorMask, colorMask, colorMask);

    if (texture != 0) {
        g_gl.glActiveTexture(GL_TEXTURE0);
        g_gl.glBindTexture(GL_TEXTURE_2D, texture);
        g_gl.glBindSampler(0, sampler);
    }

    g_gl.glBindVertexArray(vao_);
    g_gl.glDrawElements(GL_TRIANGLES,
        static_cast<GLsizei>(batch.numIndices), GL_UNSIGNED_SHORT, nullptr);

    if (texture != 0) {
        g_gl.glBindSampler(0, 0);
        g_gl.glBindTexture(GL_TEXTURE_2D, 0);
    }
    g_gl.glBindVertexArray(0);
}

} // namespace Aero::Render