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
constexpr GLenum GL_TEXTURE2 = 0x84C2;
constexpr GLenum GL_TEXTURE3 = 0x84C3;
constexpr GLenum GL_TEXTURE4 = 0x84C4;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_ONE = 1;
constexpr GLenum GL_ZERO = 0;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_SRC1_COLOR = 0x88F9;
constexpr GLenum GL_ONE_MINUS_SRC1_COLOR = 0x88FA;
constexpr GLenum GL_SRC1_ALPHA = 0x8589;
constexpr GLenum GL_ONE_MINUS_SRC1_ALPHA = 0x88FB;
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
        load(gl.glVertexAttrib1f, "glVertexAttrib1f") &&
        load(gl.glVertexAttrib2f, "glVertexAttrib2f") &&
        load(gl.glVertexAttrib4f, "glVertexAttrib4f") &&
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
        load(gl.glUniform1i, "glUniform1i") &&
        load(gl.glUniform1f, "glUniform1f") &&
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
        load(gl.glDrawElements, "glDrawElements") &&
        (load(gl.glDrawElementsBaseVertex, "glDrawElementsBaseVertex") || true);
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
        if (gl.glGetProgramInfoLog != nullptr) {
            char log[1024]{};
            gl.glGetProgramInfoLog(program, 1024, nullptr, log);
            std::fprintf(stderr, "Program link error: %s\n", log);
        }
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
    "uniform float uFlipY;\n"
    "out vec4 vColor;\n"
    "out vec2 vUV;\n"
    "out float vCoverage;\n"
    "void main() {\n"
    "    float yndc = 1.0 - aPosition.y / uViewportSize.y * 2.0;\n"
    "    vec2 ndc = vec2(aPosition.x / uViewportSize.x * 2.0 - 1.0,\n"
    "                    mix(yndc, -yndc, uFlipY));\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    vColor = aColor;\n"
    "    vUV = aUV;\n"
    "    vCoverage = aCoverage;\n"
    "}\n";

constexpr const char* UberPixelShaderSource =
    "uniform vec4 uPaint0;\n"
    "uniform vec4 uPaint1;\n"
    "uniform vec4 uEffect0;\n"
    "uniform vec4 uEffect1;\n"
    "uniform vec4 uEffectColor;\n"
    "\n"
    "uniform sampler2D pattern;\n"
    "uniform sampler2D ramps;\n"
    "uniform sampler2D image;\n"
    "uniform sampler2D glyphs;\n"
    "uniform sampler2D shadow;\n"
    "\n"
    "uniform sampler2D uTexture;\n"
    "uniform sampler2D uMask;\n"
    "\n"
    "uniform vec2 uTextureSize;\n"
    "\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vCoverage;\n"
    "\n"
    "#if defined(EFFECT_SDF_LCD)\n"
    "layout(location = 0, index = 0) out vec4 fragColor;\n"
    "layout(location = 0, index = 1) out vec4 fragAlpha;\n"
    "#else\n"
    "out vec4 fragColor;\n"
    "#endif\n"
    "\n"
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
    "\n"
    "void main() {\n"
    "#if defined(PAINT_SOLID)\n"
    "    vec4 paint = vColor;\n"
    "    float opacity_ = 1.0;\n"
    "#elif defined(PAINT_LINEAR)\n"
    "    vec4 paint = texture(ramps, vec2(vUV.x, 0.5)) * vColor;\n"
    "    float opacity_ = uPaint0.x;\n"
    "#elif defined(PAINT_RADIAL)\n"
    "    float dd = uPaint1.x * vUV.x - uPaint1.y * vUV.y;\n"
    "    float inside = vUV.x * vUV.x + vUV.y * vUV.y - dd * dd;\n"
    "    float u = uPaint0.x * vUV.x + uPaint0.y * vUV.y +\n"
    "        uPaint0.z * sqrt(max(inside, 0.0));\n"
    "    vec4 paint = texture(ramps, vec2(u, uPaint1.z)) * vColor;\n"
    "    float opacity_ = uPaint0.w;\n"
    "#elif defined(PAINT_PATTERN)\n"
    "    vec2 uv = vUV;\n"
    "#if defined(REPEAT_PATTERN)\n"
    "    uv = fract(uv);\n"
    "#elif defined(MIRROR_PATTERN)\n"
    "    uv = abs(uv - 2.0 * floor((uv - 1.0) * 0.5) - 2.0);\n"
    "#elif defined(MIRRORU_PATTERN)\n"
    "    uv.x = abs(uv.x - 2.0 * floor((uv.x - 1.0) * 0.5) - 2.0);\n"
    "    uv.y = fract(uv.y);\n"
    "#elif defined(MIRRORV_PATTERN)\n"
    "    uv.x = fract(uv.x);\n"
    "    uv.y = abs(uv.y - 2.0 * floor((uv.y - 1.0) * 0.5) - 2.0);\n"
    "#elif defined(CLAMP_PATTERN)\n"
    "    uv = clamp(uv, 0.0, 1.0);\n"
    "#endif\n"
    "    vec4 paint = texture(pattern, uv) * vColor;\n"
    "    float opacity_ = (uPaint0.x > 0.0) ? uPaint0.x : 1.0;\n"
    "#else\n"
    "    vec4 paint = vColor;\n"
    "    float opacity_ = 1.0;\n"
    "#endif\n"
    "\n"
    "#if defined(EFFECT_RGBA)\n"
    "    fragColor = uPaint0;\n"
    "#elif defined(EFFECT_MASK)\n"
    "    float maskAlpha = texture(shadow, vUV).a;\n"
    "    fragColor = texture(image, vUV) * (maskAlpha * vColor * vCoverage);\n"
    "#elif defined(EFFECT_CLEAR)\n"
    "    fragColor = vec4(0.0);\n"
    "#elif defined(EFFECT_PATH)\n"
    "    fragColor = (opacity_ * vCoverage) * paint;\n"
    "#elif defined(EFFECT_PATH_AA)\n"
    "    fragColor = (opacity_ * vCoverage) * paint;\n"
    "#elif defined(EFFECT_OPACITY)\n"
    "#if defined(PAINT_PATTERN)\n"
    "    fragColor = (opacity_ * vCoverage) * paint;\n"
    "#else\n"
    "    fragColor = texture(image, vUV) * (opacity_ * paint * vCoverage);\n"
    "#endif\n"
    "#elif defined(EFFECT_SDF)\n"
    "    float dist = texture(glyphs, vUV).r;\n"
    "    float smoothing = max(fwidth(dist), 1.0 / 512.0);\n"
    "    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);\n"
    "    fragColor = (alpha * opacity_ * vCoverage) * paint;\n"
    "#elif defined(EFFECT_SDF_LCD)\n"
    "    vec2 grad = dFdx(vUV);\n"
    "    vec2 offset = grad * vec2(0.333333, 0.0);\n"
    "    vec4 red = texture(glyphs, vUV - offset);\n"
    "    vec4 green = texture(glyphs, vUV);\n"
    "    vec4 blue = texture(glyphs, vUV + offset);\n"
    "    vec3 dist3 = vec3(red.r, green.r, blue.r);\n"
    "    float sm = max(fwidth(dist3.g), 1.0 / 512.0);\n"
    "    vec3 a3 = smoothstep(0.5 - sm, 0.5 + sm, dist3);\n"
    "    fragColor = vec4((opacity_ * vCoverage) * paint.rgb * a3.rgb, a3.g * vCoverage);\n"
    "    fragAlpha = vec4((opacity_ * paint.a * vCoverage) * a3.rgb, a3.g * vCoverage);\n"
    "#elif defined(EFFECT_DOWNSAMPLE)\n"
    "    vec2 texSize = uPaint0.xy;\n"
    "    vec2 delta = 1.0 / max(texSize, vec2(1.0));\n"
    "    fragColor = (\n"
    "        texture(pattern, vUV + vec2(-delta.x, -delta.y)) +\n"
    "        texture(pattern, vUV + vec2( delta.x, -delta.y)) +\n"
    "        texture(pattern, vUV + vec2(-delta.x,  delta.y)) +\n"
    "        texture(pattern, vUV + vec2( delta.x,  delta.y))\n"
    "    ) * 0.25;\n"
    "#elif defined(EFFECT_UPSAMPLE)\n"
    "    fragColor = mix(texture(image, vUV), texture(pattern, vUV), vColor.a);\n"
    "#elif defined(EFFECT_SHADOW)\n"
    "    vec2 texel = 1.0 / uTextureSize;\n"
    "    float sumA = 0.0;\n"
    "    for (int i = 0; i < 33; ++i) {\n"
    "        sumA += texture(shadow, vUV + kGaussianKernel[i].xy * texel).a * kGaussianKernel[i].z;\n"
    "    }\n"
    "    fragColor = vColor * (sumA * vCoverage);\n"
    "#elif defined(EFFECT_BLUR)\n"
    "    vec2 texel = 1.0 / uTextureSize;\n"
    "    vec4 sum = vec4(0.0);\n"
    "    for (int i = 0; i < 33; ++i) {\n"
    "        sum += texture(image, vUV + kGaussianKernel[i].xy * texel) * kGaussianKernel[i].z;\n"
    "    }\n"
    "    fragColor = sum * (vColor * vCoverage);\n"
    "#elif defined(EFFECT_CUSTOM)\n"
    "    vec4 eff = (uEffectColor != vec4(0.0)) ? uEffectColor : ((uEffect0 != vec4(0.0)) ? uEffect0 : vec4(1.0));\n"
    "    fragColor = texture(image, vUV) * eff * (vColor * vCoverage);\n"
    "#else\n"
    "    fragColor = opacity_ * paint;\n"
    "#endif\n"
    "}\n";

struct ShaderDefEntry {
    uint32_t shaderEnum;
    const char* defines;
};

constexpr ShaderDefEntry kShaderDefs[] = {
    {Shader::RGBA, "#define EFFECT_RGBA 1\n"},
    {Shader::Mask, "#define EFFECT_MASK 1\n"},
    {Shader::Clear, "#define EFFECT_CLEAR 1\n"},

    {Shader::Path_Solid, "#define EFFECT_PATH 1\n#define PAINT_SOLID 1\n"},
    {Shader::Path_Linear, "#define EFFECT_PATH 1\n#define PAINT_LINEAR 1\n"},
    {Shader::Path_Radial, "#define EFFECT_PATH 1\n#define PAINT_RADIAL 1\n"},
    {Shader::Path_Pattern, "#define EFFECT_PATH 1\n#define PAINT_PATTERN 1\n"},
    {Shader::Path_Pattern_Clamp, "#define EFFECT_PATH 1\n#define PAINT_PATTERN 1\n#define CLAMP_PATTERN 1\n"},
    {Shader::Path_Pattern_Repeat, "#define EFFECT_PATH 1\n#define PAINT_PATTERN 1\n#define REPEAT_PATTERN 1\n"},
    {Shader::Path_Pattern_MirrorU, "#define EFFECT_PATH 1\n#define PAINT_PATTERN 1\n#define MIRRORU_PATTERN 1\n"},
    {Shader::Path_Pattern_MirrorV, "#define EFFECT_PATH 1\n#define PAINT_PATTERN 1\n#define MIRRORV_PATTERN 1\n"},
    {Shader::Path_Pattern_Mirror, "#define EFFECT_PATH 1\n#define PAINT_PATTERN 1\n#define MIRROR_PATTERN 1\n"},

    {Shader::Path_AA_Solid, "#define EFFECT_PATH_AA 1\n#define PAINT_SOLID 1\n"},
    {Shader::Path_AA_Linear, "#define EFFECT_PATH_AA 1\n#define PAINT_LINEAR 1\n"},
    {Shader::Path_AA_Radial, "#define EFFECT_PATH_AA 1\n#define PAINT_RADIAL 1\n"},
    {Shader::Path_AA_Pattern, "#define EFFECT_PATH_AA 1\n#define PAINT_PATTERN 1\n"},
    {Shader::Path_AA_Pattern_Clamp, "#define EFFECT_PATH_AA 1\n#define PAINT_PATTERN 1\n#define CLAMP_PATTERN 1\n"},
    {Shader::Path_AA_Pattern_Repeat, "#define EFFECT_PATH_AA 1\n#define PAINT_PATTERN 1\n#define REPEAT_PATTERN 1\n"},
    {Shader::Path_AA_Pattern_MirrorU, "#define EFFECT_PATH_AA 1\n#define PAINT_PATTERN 1\n#define MIRRORU_PATTERN 1\n"},
    {Shader::Path_AA_Pattern_MirrorV, "#define EFFECT_PATH_AA 1\n#define PAINT_PATTERN 1\n#define MIRRORV_PATTERN 1\n"},
    {Shader::Path_AA_Pattern_Mirror, "#define EFFECT_PATH_AA 1\n#define PAINT_PATTERN 1\n#define MIRROR_PATTERN 1\n"},

    {Shader::SDF_Solid, "#define EFFECT_SDF 1\n#define PAINT_SOLID 1\n"},
    {Shader::SDF_Linear, "#define EFFECT_SDF 1\n#define PAINT_LINEAR 1\n"},
    {Shader::SDF_Radial, "#define EFFECT_SDF 1\n#define PAINT_RADIAL 1\n"},
    {Shader::SDF_Pattern, "#define EFFECT_SDF 1\n#define PAINT_PATTERN 1\n"},
    {Shader::SDF_Pattern_Clamp, "#define EFFECT_SDF 1\n#define PAINT_PATTERN 1\n#define CLAMP_PATTERN 1\n"},
    {Shader::SDF_Pattern_Repeat, "#define EFFECT_SDF 1\n#define PAINT_PATTERN 1\n#define REPEAT_PATTERN 1\n"},
    {Shader::SDF_Pattern_MirrorU, "#define EFFECT_SDF 1\n#define PAINT_PATTERN 1\n#define MIRRORU_PATTERN 1\n"},
    {Shader::SDF_Pattern_MirrorV, "#define EFFECT_SDF 1\n#define PAINT_PATTERN 1\n#define MIRRORV_PATTERN 1\n"},
    {Shader::SDF_Pattern_Mirror, "#define EFFECT_SDF 1\n#define PAINT_PATTERN 1\n#define MIRROR_PATTERN 1\n"},

    {Shader::SDF_LCD_Solid, "#define EFFECT_SDF_LCD 1\n#define PAINT_SOLID 1\n"},
    {Shader::SDF_LCD_Linear, "#define EFFECT_SDF_LCD 1\n#define PAINT_LINEAR 1\n"},
    {Shader::SDF_LCD_Radial, "#define EFFECT_SDF_LCD 1\n#define PAINT_RADIAL 1\n"},
    {Shader::SDF_LCD_Pattern, "#define EFFECT_SDF_LCD 1\n#define PAINT_PATTERN 1\n"},
    {Shader::SDF_LCD_Pattern_Clamp, "#define EFFECT_SDF_LCD 1\n#define PAINT_PATTERN 1\n#define CLAMP_PATTERN 1\n"},
    {Shader::SDF_LCD_Pattern_Repeat, "#define EFFECT_SDF_LCD 1\n#define PAINT_PATTERN 1\n#define REPEAT_PATTERN 1\n"},
    {Shader::SDF_LCD_Pattern_MirrorU, "#define EFFECT_SDF_LCD 1\n#define PAINT_PATTERN 1\n#define MIRRORU_PATTERN 1\n"},
    {Shader::SDF_LCD_Pattern_MirrorV, "#define EFFECT_SDF_LCD 1\n#define PAINT_PATTERN 1\n#define MIRRORV_PATTERN 1\n"},
    {Shader::SDF_LCD_Pattern_Mirror, "#define EFFECT_SDF_LCD 1\n#define PAINT_PATTERN 1\n#define MIRROR_PATTERN 1\n"},

    {Shader::Opacity_Solid, "#define EFFECT_OPACITY 1\n#define PAINT_SOLID 1\n"},
    {Shader::Opacity_Linear, "#define EFFECT_OPACITY 1\n#define PAINT_LINEAR 1\n"},
    {Shader::Opacity_Radial, "#define EFFECT_OPACITY 1\n#define PAINT_RADIAL 1\n"},
    {Shader::Opacity_Pattern, "#define EFFECT_OPACITY 1\n#define PAINT_PATTERN 1\n"},
    {Shader::Opacity_Pattern_Clamp, "#define EFFECT_OPACITY 1\n#define PAINT_PATTERN 1\n#define CLAMP_PATTERN 1\n"},
    {Shader::Opacity_Pattern_Repeat, "#define EFFECT_OPACITY 1\n#define PAINT_PATTERN 1\n#define REPEAT_PATTERN 1\n"},
    {Shader::Opacity_Pattern_MirrorU, "#define EFFECT_OPACITY 1\n#define PAINT_PATTERN 1\n#define MIRRORU_PATTERN 1\n"},
    {Shader::Opacity_Pattern_MirrorV, "#define EFFECT_OPACITY 1\n#define PAINT_PATTERN 1\n#define MIRRORV_PATTERN 1\n"},
    {Shader::Opacity_Pattern_Mirror, "#define EFFECT_OPACITY 1\n#define PAINT_PATTERN 1\n#define MIRROR_PATTERN 1\n"},

    {Shader::Upsample, "#define EFFECT_UPSAMPLE 1\n"},
    {Shader::Downsample, "#define EFFECT_DOWNSAMPLE 1\n"},
    {Shader::Shadow, "#define EFFECT_SHADOW 1\n"},
    {Shader::Blur, "#define EFFECT_BLUR 1\n"},
    {Shader::Custom_Effect, "#define EFFECT_CUSTOM 1\n"}
};

GLuint CompilePermutationPS(
    const GLCallbacks& gl,
    const char* defines) noexcept {
    GLuint shader = gl.glCreateShader(GL_FRAGMENT_SHADER);
    if (shader == 0) return 0;
    const GLchar* sources[] = {
        "#version 330 core\n",
        defines != nullptr ? defines : "",
        UberPixelShaderSource
    };
    gl.glShaderSource(shader, 3, sources, nullptr);
    gl.glCompileShader(shader);
    GLint compiled = GL_FALSE;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        if (gl.glGetShaderInfoLog != nullptr) {
            char log[1024]{};
            gl.glGetShaderInfoLog(shader, 1024, nullptr, log);
            std::fprintf(stderr, "Shader compile error (defines: %s): %s\n", defines ? defines : "", log);
        }
        gl.glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateAndInitProgram(
    const GLCallbacks& gl,
    GLuint vertexShader,
    GLuint fragmentShader) noexcept {
    GLuint prog = LinkProgram(gl, vertexShader, fragmentShader);
    if (prog != 0 && gl.glUniform1i != nullptr) {
        gl.glUseProgram(prog);
        GLint loc = gl.glGetUniformLocation(prog, "pattern");
        if (loc != -1) gl.glUniform1i(loc, 0);
        loc = gl.glGetUniformLocation(prog, "ramps");
        if (loc != -1) gl.glUniform1i(loc, 1);
        loc = gl.glGetUniformLocation(prog, "image");
        if (loc != -1) gl.glUniform1i(loc, 2);
        loc = gl.glGetUniformLocation(prog, "glyphs");
        if (loc != -1) gl.glUniform1i(loc, 3);
        loc = gl.glGetUniformLocation(prog, "shadow");
        if (loc != -1) gl.glUniform1i(loc, 4);

        loc = gl.glGetUniformLocation(prog, "uTexture");
        if (loc != -1) gl.glUniform1i(loc, 0);
        loc = gl.glGetUniformLocation(prog, "uMask");
        if (loc != -1) gl.glUniform1i(loc, 1);
        gl.glUseProgram(0);
    }
    return prog;
}

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
    for (const auto& entry : kShaderDefs) {
        if (entry.shaderEnum < Shader::Count) {
            GLuint fs = CompilePermutationPS(gl, entry.defines);
            if (fs != 0) {
                programs_[entry.shaderEnum] = CreateAndInitProgram(gl, vertexShader, fs);
                gl.glDeleteShader(fs);
            }
        }
    }
    gl.glDeleteShader(vertexShader);

    // Populate backward-compatible program member fields
    solidProgram_        = programs_[Shader::Path_Solid];
    patternProgram_      = programs_[Shader::Path_Pattern];
    sdfProgram_          = programs_[Shader::SDF_Solid];
    blurProgram_         = programs_[Shader::Blur];
    shadowProgram_       = programs_[Shader::Shadow];
    maskProgram_         = programs_[Shader::Mask];
    customEffectProgram_ = programs_[Shader::Custom_Effect];
    linearProgram_       = programs_[Shader::Path_Linear];
    radialProgram_       = programs_[Shader::Path_Radial];

    if (solidProgram_ == 0 || patternProgram_ == 0 || sdfProgram_ == 0 ||
        blurProgram_ == 0 || shadowProgram_ == 0 || maskProgram_ == 0 ||
        linearProgram_ == 0 || radialProgram_ == 0) {
        Shutdown();
        return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to compile/link a GL program");
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
    if (gl.glVertexAttrib4f != nullptr) gl.glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
    if (gl.glVertexAttrib2f != nullptr) gl.glVertexAttrib2f(2, 0.0f, 0.0f);
    if (gl.glVertexAttrib1f != nullptr) gl.glVertexAttrib1f(3, 1.0f);
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

void OpenGL33RenderDevice::SetVertexFormat(uint8_t format, uintptr_t offset) noexcept {
    const GLCallbacks& gl = g_gl;
    const GLsizei stride = SizeForFormat[format] != 0U
        ? static_cast<GLsizei>(SizeForFormat[format]) : 24;

    switch (format) {
    case Shader::Vertex::Format::PosColor:
        gl.glEnableVertexAttribArray(0);
        gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 0));
        gl.glEnableVertexAttribArray(1);
        gl.glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<const void*>(offset + 8));
        gl.glDisableVertexAttribArray(2);
        gl.glDisableVertexAttribArray(3);
        if (gl.glVertexAttrib2f != nullptr) gl.glVertexAttrib2f(2, 0.0f, 0.0f);
        if (gl.glVertexAttrib1f != nullptr) gl.glVertexAttrib1f(3, 1.0f);
        break;

    case Shader::Vertex::Format::PosTex0:
        gl.glEnableVertexAttribArray(0);
        gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 0));
        gl.glDisableVertexAttribArray(1);
        gl.glEnableVertexAttribArray(2);
        gl.glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 8));
        gl.glDisableVertexAttribArray(3);
        if (gl.glVertexAttrib4f != nullptr) gl.glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
        if (gl.glVertexAttrib1f != nullptr) gl.glVertexAttrib1f(3, 1.0f);
        break;

    case Shader::Vertex::Format::PosColorCoverage:
        gl.glEnableVertexAttribArray(0);
        gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 0));
        gl.glEnableVertexAttribArray(1);
        gl.glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<const void*>(offset + 8));
        gl.glDisableVertexAttribArray(2);
        gl.glEnableVertexAttribArray(3);
        gl.glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 12));
        if (gl.glVertexAttrib2f != nullptr) gl.glVertexAttrib2f(2, 0.0f, 0.0f);
        break;

    case Shader::Vertex::Format::PosTex0Coverage:
        gl.glEnableVertexAttribArray(0);
        gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 0));
        gl.glDisableVertexAttribArray(1);
        gl.glEnableVertexAttribArray(2);
        gl.glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 8));
        gl.glEnableVertexAttribArray(3);
        gl.glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 16));
        if (gl.glVertexAttrib4f != nullptr) gl.glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
        break;

    case Shader::Vertex::Format::PosColorTex1:
        gl.glEnableVertexAttribArray(0);
        gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 0));
        gl.glEnableVertexAttribArray(1);
        gl.glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<const void*>(offset + 8));
        gl.glEnableVertexAttribArray(2);
        gl.glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 12));
        gl.glDisableVertexAttribArray(3);
        if (gl.glVertexAttrib1f != nullptr) gl.glVertexAttrib1f(3, 1.0f);
        break;

    case Shader::Vertex::Format::PosTex0Tex1:
        gl.glEnableVertexAttribArray(0);
        gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 0));
        gl.glDisableVertexAttribArray(1);
        gl.glEnableVertexAttribArray(2);
        gl.glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset + 8));
        gl.glDisableVertexAttribArray(3);
        if (gl.glVertexAttrib4f != nullptr) gl.glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
        if (gl.glVertexAttrib1f != nullptr) gl.glVertexAttrib1f(3, 1.0f);
        break;

    default:
        // Full Vertex2D layout
        gl.glEnableVertexAttribArray(0);
        gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<const void*>(offset + 0));
        gl.glEnableVertexAttribArray(1);
        gl.glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 24, reinterpret_cast<const void*>(offset + 8));
        gl.glEnableVertexAttribArray(2);
        gl.glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<const void*>(offset + 12));
        gl.glEnableVertexAttribArray(3);
        gl.glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<const void*>(offset + 20));
        break;
    }
}

void OpenGL33RenderDevice::Shutdown() noexcept {
    if (g_glResolved) {
        const GLCallbacks& gl = g_gl;
        if (samplers_[0] != 0 && gl.glDeleteSamplers != nullptr) {
            gl.glDeleteSamplers(StateCache::kSamplerTableSize, samplers_);
            for (uint8_t i = 0; i < StateCache::kSamplerTableSize; ++i) samplers_[i] = 0;
        }
        stateCache_.Reset();
        if (gl.glDeleteProgram != nullptr) {
            for (uint32_t i = 0; i < Shader::Count; ++i) {
                if (programs_[i] != 0) {
                    gl.glDeleteProgram(programs_[i]);
                    programs_[i] = 0;
                }
            }
        }
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

    Ref<RenderDevice> self = Ref<RenderDevice>::TryFromBorrowed(*this);
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
    renderingOffscreen_ = true;
    if (vao_ == 0 || currentTarget_ == nullptr) return;
    if (currentTarget_->IsDefaultFBO()) return;
    stateCache_.Reset();
    g_gl.glDisable(GL_SCISSOR_TEST);
    g_gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    g_gl.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    g_gl.glClear(GL_COLOR_BUFFER_BIT);
}
void OpenGL33RenderDevice::EndOffscreenRender() noexcept {
    renderingOffscreen_ = false;
}
void OpenGL33RenderDevice::BeginOnscreenRender() noexcept {
    renderingOffscreen_ = false;
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
void OpenGL33RenderDevice::EndOnscreenRender() noexcept {
    renderingOffscreen_ = false;
}

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
    activeVBBytes_ = bytes > 0U ? bytes : DYNAMIC_VB_SIZE;
    return mappedVBMemory_;
}

void OpenGL33RenderDevice::UnmapVertices() noexcept {
    if (vao_ == 0 || activeVBBytes_ == 0U) return;
    g_gl.glBindBuffer(GL_ARRAY_BUFFER, dynamicVB_);
    g_gl.glBufferSubData(GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(activeVBBytes_), mappedVBMemory_);
}

void* OpenGL33RenderDevice::MapIndices(uint32_t bytes) noexcept {
    activeIBBytes_ = bytes > 0U ? bytes : DYNAMIC_IB_SIZE;
    return mappedIBMemory_;
}

void OpenGL33RenderDevice::UnmapIndices() noexcept {
    if (vao_ == 0 || activeIBBytes_ == 0U) return;
    g_gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamicIB_);
    g_gl.glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(activeIBBytes_), mappedIBMemory_);
}

void OpenGL33RenderDevice::DrawBatch(const Batch& batch) noexcept {
    if (vao_ == 0 || batch.numIndices == 0U) return;

    unsigned int program = 0;
    if (batch.shader.v == Shader::Custom_Effect) {
        program = customEffectProgram_ != 0
            ? customEffectProgram_ : programs_[Shader::Path_Pattern];
    } else if (batch.shader.v < Shader::Count && programs_[batch.shader.v] != 0) {
        program = programs_[batch.shader.v];
    } else {
        program = solidProgram_;
    }
    if (program == 0) return;

    GLuint texIds[5] = {0, 0, 0, 0, 0};
    uint8_t samplerIndices[5] = {0, 0, 0, 0, 0};
    GLuint samplerIds[5] = {0, 0, 0, 0, 0};

    // Slot 0: Pattern (with fallback to image, ramps or glyphs for legacy single-slot draws)
    Texture* patTex = batch.pattern ? batch.pattern : batch.image;
    if (patTex != nullptr) {
        texIds[0] = static_cast<OpenGL33Texture*>(patTex)->GetNativeTexture();
        samplerIndices[0] = StateCache::ClampSamplerIndex(
            batch.pattern ? batch.patternSampler.v : batch.imageSampler.v);
        samplerIds[0] = samplers_[samplerIndices[0]];
    } else if (batch.ramps != nullptr) {
        texIds[0] = static_cast<OpenGL33Texture*>(batch.ramps)->GetNativeTexture();
        samplerIndices[0] = StateCache::ClampSamplerIndex(batch.rampsSampler.v);
        samplerIds[0] = samplers_[samplerIndices[0]];
    } else if (batch.glyphs != nullptr) {
        texIds[0] = static_cast<OpenGL33Texture*>(batch.glyphs)->GetNativeTexture();
        samplerIndices[0] = StateCache::ClampSamplerIndex(batch.glyphsSampler.v);
        samplerIds[0] = samplers_[samplerIndices[0]];
    }

    // Slot 1: Ramps (for legacy Mask, fallback to shadow if ramps is null)
    Texture* rampsTex = batch.ramps ? batch.ramps : (batch.shader.v == Shader::Mask ? batch.shadow : nullptr);
    if (rampsTex != nullptr) {
        texIds[1] = static_cast<OpenGL33Texture*>(rampsTex)->GetNativeTexture();
        samplerIndices[1] = StateCache::ClampSamplerIndex(
            batch.ramps ? batch.rampsSampler.v : batch.shadowSampler.v);
        samplerIds[1] = samplers_[samplerIndices[1]];
    }

    // Slot 2: Image
    if (batch.image != nullptr) {
        texIds[2] = static_cast<OpenGL33Texture*>(batch.image)->GetNativeTexture();
        samplerIndices[2] = StateCache::ClampSamplerIndex(batch.imageSampler.v);
        samplerIds[2] = samplers_[samplerIndices[2]];
    }

    // Slot 3: Glyphs
    if (batch.glyphs != nullptr) {
        texIds[3] = static_cast<OpenGL33Texture*>(batch.glyphs)->GetNativeTexture();
        samplerIndices[3] = StateCache::ClampSamplerIndex(batch.glyphsSampler.v);
        samplerIds[3] = samplers_[samplerIndices[3]];
    }

    // Slot 4: Shadow (with fallback to image when shader == Shadow)
    Texture* shadowTex = batch.shadow ? batch.shadow : (batch.shader.v == Shader::Shadow ? batch.image : nullptr);
    if (shadowTex != nullptr) {
        texIds[4] = static_cast<OpenGL33Texture*>(shadowTex)->GetNativeTexture();
        samplerIndices[4] = StateCache::ClampSamplerIndex(
            batch.shadow ? batch.shadowSampler.v : batch.imageSampler.v);
        samplerIds[4] = samplers_[samplerIndices[4]];
    }

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
    const GLint flipLocation = g_gl.glGetUniformLocation(program, "uFlipY");
    if (flipLocation != -1) {
        g_gl.glUniform1f(flipLocation, renderingOffscreen_ ? 1.0F : 0.0F);
    }

    const bool setTextureSize = (batch.shader.v == Shader::Blur ||
                                 batch.shader.v == Shader::Shadow ||
                                 batch.shader.v == Shader::Downsample ||
                                 batch.shader.v == Shader::Custom_Effect);
    if (setTextureSize && batch.pixelUniforms[0].values != nullptr &&
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

    if (batch.pixelUniforms[0].values != nullptr &&
        batch.pixelUniforms[0].numDwords >= 1U &&
        g_gl.glUniform4f != nullptr) {
        const float* vals =
            static_cast<const float*>(batch.pixelUniforms[0].values);
        const uint32_t num = batch.pixelUniforms[0].numDwords;
        const GLint paint0Loc =
            g_gl.glGetUniformLocation(program, "uPaint0");
        if (paint0Loc != -1) {
            float p0[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (uint32_t i = 0; i < std::min(num, 4U); ++i) p0[i] = vals[i];
            g_gl.glUniform4f(paint0Loc, p0[0], p0[1], p0[2], p0[3]);
        }
        if (num > 4U) {
            const GLint paint1Loc =
                g_gl.glGetUniformLocation(program, "uPaint1");
            if (paint1Loc != -1) {
                float p1[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                for (uint32_t i = 4U; i < std::min(num, 8U); ++i) p1[i - 4U] = vals[i];
                g_gl.glUniform4f(paint1Loc, p1[0], p1[1], p1[2], p1[3]);
            }
        }
    }

    if (batch.pixelUniforms[1].values != nullptr &&
        batch.pixelUniforms[1].numDwords >= 4U &&
        g_gl.glUniform4f != nullptr) {
        const GLint colorLocation =
            g_gl.glGetUniformLocation(program, "uEffectColor");
        if (colorLocation != -1) {
            const float* color =
                static_cast<const float*>(batch.pixelUniforms[1].values);
            g_gl.glUniform4f(
                colorLocation, color[0], color[1], color[2], color[3]);
        }
    } else if (batch.shader.v == Shader::Custom_Effect &&
               g_gl.glUniform4f != nullptr) {
        const GLint colorLocation =
            g_gl.glGetUniformLocation(program, "uEffectColor");
        if (colorLocation != -1) {
            g_gl.glUniform4f(colorLocation, 1.0F, 1.0F, 1.0F, 1.0F);
        }
    }

    const uint8_t vertexEnum = (batch.shader.v < Shader::Count)
        ? VertexForShader[batch.shader.v] : 0U;
    const uint8_t format = (vertexEnum < Shader::Vertex::Count)
        ? FormatForVertex[vertexEnum] : 0U;
    const uint32_t stride = SizeForFormat[format] != 0U
        ? SizeForFormat[format] : 24U;

    if (g_gl.glDrawElementsBaseVertex == nullptr) {
        g_gl.glBindBuffer(GL_ARRAY_BUFFER, dynamicVB_);
        g_gl.glBufferSubData(GL_ARRAY_BUFFER, 0,
            static_cast<GLsizeiptr>(batch.numVertices) * stride,
            mappedVBMemory_ + batch.vertexOffset);
        g_gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamicIB_);
        g_gl.glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
            static_cast<GLsizeiptr>(batch.numIndices) * sizeof(uint16_t),
            mappedIBMemory_ + static_cast<GLsizeiptr>(batch.startIndex) * sizeof(uint16_t));
    }

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
                    GL_ONE, GL_ONE_MINUS_SRC1_COLOR,
                    GL_ONE, GL_ONE_MINUS_SRC1_ALPHA);
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

    for (uint8_t slot = 0U; slot < 5U; ++slot) {
        if (texIds[slot] != 0) {
            g_gl.glActiveTexture(GL_TEXTURE0 + slot);
            g_gl.glBindTexture(GL_TEXTURE_2D, texIds[slot]);
            if (stateCache_.UpdateSampler(
                    slot, SamplerBindKey{samplerIndices[slot], true})) {
                g_gl.glBindSampler(slot, samplerIds[slot]);
            }
        } else {
            static_cast<void>(stateCache_.UpdateSampler(slot, SamplerBindKey{}));
        }
    }

    g_gl.glBindVertexArray(vao_);
    g_gl.glBindBuffer(GL_ARRAY_BUFFER, dynamicVB_);
    const uintptr_t vbOffset = (g_gl.glDrawElementsBaseVertex != nullptr)
        ? batch.vertexOffset : 0U;
    SetVertexFormat(format, vbOffset);

    if (g_gl.glDrawElementsBaseVertex != nullptr) {
        g_gl.glDrawElementsBaseVertex(GL_TRIANGLES,
            static_cast<GLsizei>(batch.numIndices), GL_UNSIGNED_SHORT,
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(batch.startIndex) * sizeof(uint16_t)),
            0);
    } else {
        g_gl.glDrawElements(GL_TRIANGLES,
            static_cast<GLsizei>(batch.numIndices), GL_UNSIGNED_SHORT, nullptr);
    }

    for (uint8_t slot = 0U; slot < 5U; ++slot) {
        if (texIds[slot] != 0) {
            g_gl.glActiveTexture(GL_TEXTURE0 + slot);
            g_gl.glBindSampler(slot, 0);
            g_gl.glBindTexture(GL_TEXTURE_2D, 0);
            static_cast<void>(stateCache_.UpdateSampler(slot, SamplerBindKey{}));
        }
    }
    g_gl.glActiveTexture(GL_TEXTURE0);
    g_gl.glBindVertexArray(0);
}

} // namespace Aero::Render