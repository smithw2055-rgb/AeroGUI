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
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
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
constexpr GLenum GL_TEXTURE1 = 0x84C1;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_ONE = 1;
constexpr GLenum GL_ZERO = 0;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_DEST_COLOR = 0x0306;
constexpr GLenum GL_ONE_MINUS_SRC_COLOR = 0x0301;
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
constexpr GLenum GL_STENCIL_BUFFER_BIT = 0x00000400;
constexpr GLenum GL_STENCIL_TEST = 0x0B90;
constexpr GLenum GL_KEEP = 0x1E00;
constexpr GLenum GL_INCR = 0x1E02;
constexpr GLenum GL_DECR = 0x1E03;
constexpr GLenum GL_INCR_WRAP = 0x8507;
constexpr GLenum GL_DECR_WRAP = 0x8508;
constexpr GLenum GL_EQUAL = 0x0202;
constexpr GLenum GL_ALWAYS = 0x0207;
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
        load(gl.glUniform4f, "glUniform4f") &&
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
        load(gl.glClearStencil, "glClearStencil") &&
        load(gl.glStencilFunc, "glStencilFunc") &&
        load(gl.glStencilOp, "glStencilOp") &&
        load(gl.glStencilMask, "glStencilMask") &&
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
    "    fragColor = vColor * vCoverage;\n"
    "}\n";

constexpr const char* PatternPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = texture(uTexture, vUV) * (vColor * vCoverage);\n"
    "}\n";

constexpr const char* LinearPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec4 uPaint0;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = (uPaint0.x * texture(uTexture, vUV)) * (vColor * vCoverage);\n"
    "}\n";

constexpr const char* RadialPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec4 uPaint0;\n"
    "uniform vec4 uPaint1;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    float dd = uPaint1.x * vUV.x - uPaint1.y * vUV.y;\n"
    "    float inside = vUV.x * vUV.x + vUV.y * vUV.y - dd * dd;\n"
    "    float u = uPaint0.x * vUV.x + uPaint0.y * vUV.y +\n"
    "        uPaint0.z * sqrt(max(inside, 0.0));\n"
    "    vec4 paint = texture(uTexture, vec2(u, uPaint1.z));\n"
    "    fragColor = (uPaint0.w * paint) * (vColor * vCoverage);\n"
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
    "    fragColor = vColor * (alpha * vCoverage);\n"
    "}\n";

constexpr const char* BlurPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec2 uTextureSize;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "const vec3 kGaussianKernel[33] = vec3[33](\n"
    "    vec3( 0.0000000,  0.0000000,  0.0717812),\n"
    "    vec3( 0.1250000,  0.0000000,  0.0693556),\n"
    "    vec3(-0.1596450,  0.1462479,  0.0647477),\n"
    "    vec3( 0.0244362, -0.2784383,  0.0604458),\n"
    "    vec3( 0.2012222,  0.2624588,  0.0564298),\n"
    "    vec3(-0.3692676, -0.0653182,  0.0526806),\n"
    "    vec3( 0.3498025, -0.2225157,  0.0491805),\n"
    "    vec3(-0.1170021,  0.4352419,  0.0459130),\n"
    "    vec3(-0.2231357, -0.4296341,  0.0428625),\n"
    "    vec3( 0.4841151,  0.1767981,  0.0400147),\n"
    "    vec3(-0.5036411,  0.2078957,  0.0373561),\n"
    "    vec3( 0.2427883, -0.5188245,  0.0348742),\n"
    "    vec3( 0.1794144,  0.5720013,  0.0325572),\n"
    "    vec3(-0.5407570, -0.3133797,  0.0303941),\n"
    "    vec3( 0.6343695, -0.1394644,  0.0283747),\n"
    "    vec3(-0.3871458,  0.5506751,  0.0264895),\n"
    "    vec3(-0.0894397, -0.6901996,  0.0247295),\n"
    "    vec3( 0.5490718,  0.4627583,  0.0230865),\n"
    "    vec3(-0.7388785,  0.0305549,  0.0215526),\n"
    "    vec3( 0.5389551, -0.5363323,  0.0201207),\n"
    "    vec3(-0.0360582,  0.7797915,  0.0187839),\n"
    "    vec3(-0.5128175, -0.6145268,  0.0175359),\n"
    "    vec3( 0.8123596,  0.1093018,  0.0163708),\n"
    "    vec3(-0.6883106,  0.4789086,  0.0152831),\n"
    "    vec3( 0.1880861, -0.8360614,  0.0142677),\n"
    "    vec3( 0.4350333,  0.7591911,  0.0133197),\n"
    "    vec3(-0.8504484, -0.2713162,  0.0124348),\n"
    "    vec3( 0.8261024, -0.3816803,  0.0116086),\n"
    "    vec3(-0.3578882,  0.8551556,  0.0108373),\n"
    "    vec3(-0.3194073, -0.8880338,  0.0101173),\n"
    "    vec3( 0.8499086,  0.4466882,  0.0094451),\n"
    "    vec3(-0.9440346,  0.2488445,  0.0088176),\n"
    "    vec3( 0.5365958, -0.8345298,  0.0082317)\n"
    ");\n"
    "void main() {\n"
    "    vec2 texel = 1.0 / uTextureSize;\n"
    "    vec4 sum = vec4(0.0);\n"
    "    for (int i = 0; i < 33; ++i) {\n"
    "        sum += texture(uTexture, vUV + kGaussianKernel[i].xy * texel) * kGaussianKernel[i].z;\n"
    "    }\n"
    "    fragColor = sum * (vColor * vCoverage);\n"
    "}\n";

constexpr const char* CustomEffectPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec4 uEffectColor;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec4 src = texture(uTexture, vUV);\n"
    "    fragColor = src * uEffectColor * (vColor * vCoverage);\n"
    "}\n";

constexpr const char* ShadowPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec2 uTextureSize;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "const vec3 kGaussianKernel[33] = vec3[33](\n"
    "    vec3( 0.0000000,  0.0000000,  0.0717812),\n"
    "    vec3( 0.1250000,  0.0000000,  0.0693556),\n"
    "    vec3(-0.1596450,  0.1462479,  0.0647477),\n"
    "    vec3( 0.0244362, -0.2784383,  0.0604458),\n"
    "    vec3( 0.2012222,  0.2624588,  0.0564298),\n"
    "    vec3(-0.3692676, -0.0653182,  0.0526806),\n"
    "    vec3( 0.3498025, -0.2225157,  0.0491805),\n"
    "    vec3(-0.1170021,  0.4352419,  0.0459130),\n"
    "    vec3(-0.2231357, -0.4296341,  0.0428625),\n"
    "    vec3( 0.4841151,  0.1767981,  0.0400147),\n"
    "    vec3(-0.5036411,  0.2078957,  0.0373561),\n"
    "    vec3( 0.2427883, -0.5188245,  0.0348742),\n"
    "    vec3( 0.1794144,  0.5720013,  0.0325572),\n"
    "    vec3(-0.5407570, -0.3133797,  0.0303941),\n"
    "    vec3( 0.6343695, -0.1394644,  0.0283747),\n"
    "    vec3(-0.3871458,  0.5506751,  0.0264895),\n"
    "    vec3(-0.0894397, -0.6901996,  0.0247295),\n"
    "    vec3( 0.5490718,  0.4627583,  0.0230865),\n"
    "    vec3(-0.7388785,  0.0305549,  0.0215526),\n"
    "    vec3( 0.5389551, -0.5363323,  0.0201207),\n"
    "    vec3(-0.0360582,  0.7797915,  0.0187839),\n"
    "    vec3(-0.5128175, -0.6145268,  0.0175359),\n"
    "    vec3( 0.8123596,  0.1093018,  0.0163708),\n"
    "    vec3(-0.6883106,  0.4789086,  0.0152831),\n"
    "    vec3( 0.1880861, -0.8360614,  0.0142677),\n"
    "    vec3( 0.4350333,  0.7591911,  0.0133197),\n"
    "    vec3(-0.8504484, -0.2713162,  0.0124348),\n"
    "    vec3( 0.8261024, -0.3816803,  0.0116086),\n"
    "    vec3(-0.3578882,  0.8551556,  0.0108373),\n"
    "    vec3(-0.3194073, -0.8880338,  0.0101173),\n"
    "    vec3( 0.8499086,  0.4466882,  0.0094451),\n"
    "    vec3(-0.9440346,  0.2488445,  0.0088176),\n"
    "    vec3( 0.5365958, -0.8345298,  0.0082317)\n"
    ");\n"
    "void main() {\n"
    "    vec2 texel = 1.0 / uTextureSize;\n"
    "    float sumA = 0.0;\n"
    "    for (int i = 0; i < 33; ++i) {\n"
    "        sumA += texture(uTexture, vUV + kGaussianKernel[i].xy * texel).a * kGaussianKernel[i].z;\n"
    "    }\n"
    "    fragColor = vColor * (sumA * vCoverage);\n"
    "}\n";

constexpr const char* MaskPixelShaderSource =
    "#version 330 core\n"
    "uniform sampler2D uTexture;\n"
    "uniform sampler2D uMask;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    float maskAlpha = texture(uMask, vUV).a;\n"
    "    fragColor = texture(uTexture, vUV) * (maskAlpha * vColor * vCoverage);\n"
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
    GLuint blurFS = CompileShader(gl, GL_FRAGMENT_SHADER, BlurPixelShaderSource);
    GLuint shadowFS = CompileShader(gl, GL_FRAGMENT_SHADER, ShadowPixelShaderSource);
    GLuint maskFS = CompileShader(gl, GL_FRAGMENT_SHADER, MaskPixelShaderSource);
    GLuint customFS = CompileShader(gl, GL_FRAGMENT_SHADER, CustomEffectPixelShaderSource);
    GLuint linearFS = CompileShader(gl, GL_FRAGMENT_SHADER, LinearPixelShaderSource);
    GLuint radialFS = CompileShader(gl, GL_FRAGMENT_SHADER, RadialPixelShaderSource);
    if (solidFS == 0 || patternFS == 0 || sdfFS == 0 ||
        blurFS == 0 || shadowFS == 0 || maskFS == 0 || customFS == 0 ||
        linearFS == 0 || radialFS == 0) {
        if (solidFS != 0) gl.glDeleteShader(solidFS);
        if (patternFS != 0) gl.glDeleteShader(patternFS);
        if (sdfFS != 0) gl.glDeleteShader(sdfFS);
        if (blurFS != 0) gl.glDeleteShader(blurFS);
        if (shadowFS != 0) gl.glDeleteShader(shadowFS);
        if (maskFS != 0) gl.glDeleteShader(maskFS);
        if (customFS != 0) gl.glDeleteShader(customFS);
        if (linearFS != 0) gl.glDeleteShader(linearFS);
        if (radialFS != 0) gl.glDeleteShader(radialFS);
        gl.glDeleteShader(vertexShader);
        return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to compile a GL fragment shader");
    }

    solidProgram_ = LinkProgram(gl, vertexShader, solidFS);
    patternProgram_ = LinkProgram(gl, vertexShader, patternFS);
    sdfProgram_ = LinkProgram(gl, vertexShader, sdfFS);
    blurProgram_ = LinkProgram(gl, vertexShader, blurFS);
    shadowProgram_ = LinkProgram(gl, vertexShader, shadowFS);
    maskProgram_ = LinkProgram(gl, vertexShader, maskFS);
    customEffectProgram_ = LinkProgram(gl, vertexShader, customFS);
    linearProgram_ = LinkProgram(gl, vertexShader, linearFS);
    radialProgram_ = LinkProgram(gl, vertexShader, radialFS);
    gl.glDeleteShader(solidFS);
    gl.glDeleteShader(patternFS);
    gl.glDeleteShader(sdfFS);
    gl.glDeleteShader(blurFS);
    gl.glDeleteShader(shadowFS);
    gl.glDeleteShader(maskFS);
    gl.glDeleteShader(customFS);
    gl.glDeleteShader(linearFS);
    gl.glDeleteShader(radialFS);
    gl.glDeleteShader(vertexShader);
    if (solidProgram_ == 0 || patternProgram_ == 0 || sdfProgram_ == 0 ||
        blurProgram_ == 0 || shadowProgram_ == 0 || maskProgram_ == 0 ||
        customEffectProgram_ == 0 || linearProgram_ == 0 || radialProgram_ == 0) {
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
    gl.glGenSamplers(StateCache::kSamplerTableSize, samplers_);
    for (uint8_t v = 0; v < StateCache::kSamplerTableSize; ++v) {
        const uint8_t wrapMode = StateCache::UnpackWrap(v);
        const uint8_t minmag = StateCache::UnpackMinMag(v);
        const uint8_t mip = StateCache::UnpackMip(v);

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
            gl.glDeleteSamplers(StateCache::kSamplerTableSize, samplers_);
            for (uint8_t i = 0; i < StateCache::kSamplerTableSize; ++i) samplers_[i] = 0;
        }
        stateCache_.Reset();
        if (solidProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(solidProgram_);
        if (patternProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(patternProgram_);
        if (sdfProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(sdfProgram_);
        if (blurProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(blurProgram_);
        if (shadowProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(shadowProgram_);
        if (maskProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(maskProgram_);
        if (customEffectProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(customEffectProgram_);
        if (linearProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(linearProgram_);
        if (radialProgram_ != 0 && gl.glDeleteProgram != nullptr) gl.glDeleteProgram(radialProgram_);
        solidProgram_ = 0;
        patternProgram_ = 0;
        sdfProgram_ = 0;
        blurProgram_ = 0;
        shadowProgram_ = 0;
        maskProgram_ = 0;
        customEffectProgram_ = 0;
        linearProgram_ = 0;
        radialProgram_ = 0;
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

    g_gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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

void OpenGL33RenderDevice::BeginOffscreenRender() noexcept {
    if (vao_ == 0 || currentTarget_ == nullptr) return;
    if (currentTarget_->IsDefaultFBO()) return;
    stateCache_.Reset();
    g_gl.glDisable(GL_SCISSOR_TEST);
    g_gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    g_gl.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    g_gl.glClear(GL_COLOR_BUFFER_BIT);
}
void OpenGL33RenderDevice::EndOffscreenRender() noexcept {}
void OpenGL33RenderDevice::BeginOnscreenRender() noexcept {
    // Clear the window color buffer once per frame. SetRenderTarget must not
    // clear the default FBO: offscreen compositing rebinds it after drawing
    // background/sidebar content and a second clear would wipe those pixels.
    if (vao_ == 0 || currentTarget_ == nullptr) return;
    if (!currentTarget_->IsDefaultFBO()) return;
    stateCache_.Reset();
    g_gl.glDisable(GL_SCISSOR_TEST);
    g_gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    g_gl.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    g_gl.glClear(GL_COLOR_BUFFER_BIT);
}
void OpenGL33RenderDevice::EndOnscreenRender() noexcept {}

void OpenGL33RenderDevice::SetRenderTarget(RenderTarget* surface) noexcept {
    if (vao_ == 0) return;
    stateCache_.Reset();
    currentTarget_ = static_cast<OpenGL33RenderTarget*>(surface);
    if (currentTarget_ == nullptr) return;

    viewportWidth_ = currentTarget_->GetWidth();
    viewportHeight_ = currentTarget_->GetHeight();
    g_gl.glBindFramebuffer(GL_FRAMEBUFFER, currentTarget_->GetFBO());
    g_gl.glViewport(0, 0,
        static_cast<GLsizei>(viewportWidth_), static_cast<GLsizei>(viewportHeight_));
}

void OpenGL33RenderDevice::BeginTile(RenderTarget* surface, const Tile& tile) noexcept {
    static_cast<void>(surface);
    if (vao_ == 0) return;

    if (g_gl.glClearStencil != nullptr && g_gl.glClear != nullptr) {
        g_gl.glClearStencil(0);
        g_gl.glClear(GL_STENCIL_BUFFER_BIT);
    }

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
    GLuint maskTexture = 0;
    GLuint maskSampler = 0;
    uint8_t samplerIndex0 = 0U;
    uint8_t samplerIndex1 = 0U;
    bool needsTextureSize = false;
    switch (batch.shader.v) {
    case Shader::Path_Linear:
        program = linearProgram_;
        if (batch.ramps != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.ramps)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.rampsSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        break;
    case Shader::Path_Radial:
        program = radialProgram_;
        if (batch.ramps != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.ramps)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.rampsSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        break;
    case Shader::Path_Pattern:
        program = patternProgram_;
        if (batch.image != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.image)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.imageSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        break;
    case Shader::SDF_Solid:
        program = sdfProgram_;
        if (batch.glyphs != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.glyphs)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.glyphsSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        break;
    case Shader::Blur:
        program = blurProgram_;
        needsTextureSize = true;
        if (batch.image != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.image)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.imageSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        break;
    case Shader::Custom_Effect:
        program = customEffectProgram_;
        if (batch.image != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.image)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.imageSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        break;
    case Shader::Shadow:
        program = shadowProgram_;
        needsTextureSize = true;
        if (batch.image != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.image)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.imageSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        break;
    case Shader::Mask:
        program = maskProgram_;
        if (batch.image != nullptr) {
            texture = static_cast<OpenGL33Texture*>(batch.image)->GetNativeTexture();
            samplerIndex0 = StateCache::ClampSamplerIndex(batch.imageSampler.v);
            sampler = samplers_[samplerIndex0];
        }
        if (batch.shadow != nullptr) {
            maskTexture = static_cast<OpenGL33Texture*>(batch.shadow)->GetNativeTexture();
            samplerIndex1 = StateCache::ClampSamplerIndex(batch.shadowSampler.v);
            maskSampler = samplers_[samplerIndex1];
        }
        break;
    case Shader::Path_Solid:
    case Shader::Path_AA_Solid:
    default:
        break;
    }

    if (program == 0) return;

    const ShaderPipelineKey pipelineKey{batch.shader.v};
    const bool pipelineEnumChanged = stateCache_.UpdatePipeline(pipelineKey);
    const bool pipelineHandleChanged = stateCache_.UpdatePipelineHandle(
        static_cast<std::uintptr_t>(program));
    if (pipelineEnumChanged || pipelineHandleChanged || currentProgram_ != program) {
        g_gl.glUseProgram(program);
        currentProgram_ = program;
    }

    const GLint viewportLocation = g_gl.glGetUniformLocation(program, "uViewportSize");
    if (viewportLocation != -1) {
        g_gl.glUniform2f(viewportLocation,
            static_cast<GLfloat>(viewportWidth_),
            static_cast<GLfloat>(viewportHeight_));
    }

    if (needsTextureSize && batch.pixelUniforms[0].values != nullptr &&
        batch.pixelUniforms[0].numDwords >= 2U) {
        const GLint textureSizeLocation =
            g_gl.glGetUniformLocation(program, "uTextureSize");
        if (textureSizeLocation != -1) {
            const float* size =
                static_cast<const float*>(batch.pixelUniforms[0].values);
            g_gl.glUniform2f(
                textureSizeLocation, size[0], size[1]);
        }
    }

    if ((batch.shader.v == Shader::Path_Linear ||
         batch.shader.v == Shader::Path_Radial) &&
        batch.pixelUniforms[0].values != nullptr &&
        batch.pixelUniforms[0].numDwords >= 1U &&
        g_gl.glUniform4f != nullptr) {
        const float* paint =
            static_cast<const float*>(batch.pixelUniforms[0].values);
        const GLint paint0 =
            g_gl.glGetUniformLocation(program, "uPaint0");
        if (paint0 != -1) {
            g_gl.glUniform4f(paint0, paint[0], paint[1], paint[2], paint[3]);
        }
        if (batch.shader.v == Shader::Path_Radial &&
            batch.pixelUniforms[0].numDwords >= 8U) {
            const GLint paint1 =
                g_gl.glGetUniformLocation(program, "uPaint1");
            if (paint1 != -1) {
                g_gl.glUniform4f(paint1, paint[4], paint[5], paint[6], paint[7]);
            }
        }
    }

    if (batch.shader.v == Shader::Custom_Effect &&
        batch.pixelUniforms[1].values != nullptr &&
        batch.pixelUniforms[1].numDwords >= 4U) {
        const GLint colorLocation =
            g_gl.glGetUniformLocation(program, "uEffectColor");
        if (colorLocation != -1) {
            const float* color =
                static_cast<const float*>(batch.pixelUniforms[1].values);
            g_gl.glUniform4f(
                colorLocation, color[0], color[1], color[2], color[3]);
        }
    } else if (batch.shader.v == Shader::Custom_Effect) {
        const GLint colorLocation =
            g_gl.glGetUniformLocation(program, "uEffectColor");
        if (colorLocation != -1) {
            g_gl.glUniform4f(colorLocation, 1.0F, 1.0F, 1.0F, 1.0F);
        }
    }

    g_gl.glBindBuffer(GL_ARRAY_BUFFER, dynamicVB_);
    g_gl.glBufferSubData(GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(batch.numVertices) * VertexStride,
        mappedVBMemory_ + batch.vertexOffset);
    g_gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamicIB_);
    g_gl.glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(batch.numIndices) * sizeof(uint16_t),
        mappedIBMemory_ + static_cast<GLsizeiptr>(batch.startIndex) * sizeof(uint16_t));

    // Blend + color-mask via shared StateCache (dedupe parallel D3D path).
    const BlendStateKey blendKey{
        batch.renderState.f.blendMode,
        batch.renderState.f.colorEnable};
    if (stateCache_.UpdateBlend(blendKey)) {
        if (blendKey.blendMode == BlendMode::Src) {
            g_gl.glDisable(GL_BLEND);
        } else {
            g_gl.glEnable(GL_BLEND);
            switch (blendKey.blendMode) {
            case BlendMode::SrcOver_Multiply:
                g_gl.glBlendFuncSeparate(
                    GL_DEST_COLOR, GL_ONE_MINUS_SRC_ALPHA,
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::SrcOver_Screen:
                g_gl.glBlendFuncSeparate(
                    GL_ONE, GL_ONE_MINUS_SRC_COLOR,
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::SrcOver_Additive:
                g_gl.glBlendFuncSeparate(
                    GL_ONE, GL_ONE, GL_ONE, GL_ONE);
                break;
            case BlendMode::SrcOver_Dual:
                g_gl.glBlendFuncSeparate(
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::SrcOver:
            default:
                g_gl.glBlendFuncSeparate(
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            }
        }
        const GLboolean colorMask =
            blendKey.colorEnable != 0 ? GL_TRUE : GL_FALSE;
        g_gl.glColorMask(colorMask, colorMask, colorMask, colorMask);
    }

    const DepthStencilStateKey dsKey{
        batch.renderState.f.stencilMode,
        batch.stencilRef};
    if (g_gl.glStencilFunc != nullptr && g_gl.glStencilOp != nullptr &&
        stateCache_.UpdateDepthStencil(dsKey)) {
        switch (dsKey.stencilMode) {
        case StencilMode::Equal_Keep:
        case StencilMode::Equal_Keep_ZTest:
            g_gl.glEnable(GL_STENCIL_TEST);
            g_gl.glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            g_gl.glStencilFunc(GL_EQUAL, static_cast<GLint>(dsKey.stencilRef), 0xFF);
            break;
        case StencilMode::Equal_Incr:
            g_gl.glEnable(GL_STENCIL_TEST);
            g_gl.glStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP);
            g_gl.glStencilFunc(GL_EQUAL, static_cast<GLint>(dsKey.stencilRef), 0xFF);
            break;
        case StencilMode::Equal_Decr:
            g_gl.glEnable(GL_STENCIL_TEST);
            g_gl.glStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP);
            g_gl.glStencilFunc(GL_EQUAL, static_cast<GLint>(dsKey.stencilRef), 0xFF);
            break;
        case StencilMode::Clear:
            g_gl.glEnable(GL_STENCIL_TEST);
            g_gl.glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
            g_gl.glStencilFunc(GL_ALWAYS, 0, 0xFF);
            break;
        case StencilMode::Disabled:
        case StencilMode::Disabled_ZTest:
        default:
            g_gl.glDisable(GL_STENCIL_TEST);
            break;
        }
    }

    if (texture != 0) {
        g_gl.glActiveTexture(GL_TEXTURE0);
        g_gl.glBindTexture(GL_TEXTURE_2D, texture);
        if (stateCache_.UpdateSampler(
                0, SamplerBindKey{samplerIndex0, true})) {
            g_gl.glBindSampler(0, sampler);
        }
    } else {
        static_cast<void>(stateCache_.UpdateSampler(0, SamplerBindKey{}));
    }
    if (maskTexture != 0) {
        g_gl.glActiveTexture(GL_TEXTURE1);
        g_gl.glBindTexture(GL_TEXTURE_2D, maskTexture);
        if (stateCache_.UpdateSampler(
                1, SamplerBindKey{samplerIndex1, true})) {
            g_gl.glBindSampler(1, maskSampler);
        }
    } else {
        static_cast<void>(stateCache_.UpdateSampler(1, SamplerBindKey{}));
    }

    g_gl.glBindVertexArray(vao_);
    g_gl.glDrawElements(GL_TRIANGLES,
        static_cast<GLsizei>(batch.numIndices), GL_UNSIGNED_SHORT, nullptr);

    if (maskTexture != 0) {
        g_gl.glActiveTexture(GL_TEXTURE1);
        g_gl.glBindSampler(1, 0);
        g_gl.glBindTexture(GL_TEXTURE_2D, 0);
        static_cast<void>(stateCache_.UpdateSampler(1, SamplerBindKey{}));
    }
    if (texture != 0) {
        g_gl.glActiveTexture(GL_TEXTURE0);
        g_gl.glBindSampler(0, 0);
        g_gl.glBindTexture(GL_TEXTURE_2D, 0);
        static_cast<void>(stateCache_.UpdateSampler(0, SamplerBindKey{}));
    }
    g_gl.glBindVertexArray(0);
}

} // namespace Aero::Render