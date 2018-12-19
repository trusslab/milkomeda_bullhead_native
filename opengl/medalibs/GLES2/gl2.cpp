/*
 ** Copyright 2018, University of California, Irvine
 **
 ** Authors: Zhihao Yao, Ardalan Amiri Sani
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

/*
 ** Copyright 2007, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string>
#include <errno.h>

#include <memory> 
#include <unistd.h>
#include <sys/types.h> 

#include <sys/ioctl.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include "../hooks.h"
#include "../egl_impl.h"
#include "milko_prints.h"

using namespace android;

#undef API_ENTRY
#undef CALL_GL_API
#undef CALL_GL_API_RETURN

#if USE_SLOW_BINDING

    #define API_ENTRY(_api) _api

    #define CALL_GL_API(_api, ...)                                       \
        gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;  \
        if (_c) return _c->_api(__VA_ARGS__);

#elif defined(__arm__)

    #define GET_TLS(reg) "mrc p15, 0, " #reg ", c13, c0, 3 \n"

    #define API_ENTRY(_api) __attribute__((noinline)) _api

    #define CALL_GL_API(_api, ...)                              \
         asm volatile(                                          \
            GET_TLS(r12)                                        \
            "ldr   r12, [r12, %[tls]] \n"                       \
            "cmp   r12, #0            \n"                       \
            "ldrne pc,  [r12, %[api]] \n"                       \
            :                                                   \
            : [tls] "J"(TLS_SLOT_OPENGL_API*4),                 \
              [api] "J"(__builtin_offsetof(gl_hooks_t, gl._api))    \
            : "r12"                                             \
            );

#elif defined(__aarch64__)

    #define API_ENTRY(_api) __attribute__((noinline)) _api

    #define CALL_GL_API(_api, ...)                                  \
        asm volatile(                                               \
            "mrs x16, tpidr_el0\n"                                  \
            "ldr x16, [x16, %[tls]]\n"                              \
            "cbz x16, 1f\n"                                         \
            "ldr x16, [x16, %[api]]\n"                              \
            "br  x16\n"                                             \
            "1:\n"                                                  \
            :                                                       \
            : [tls] "i" (TLS_SLOT_OPENGL_API * sizeof(void*)),      \
              [api] "i" (__builtin_offsetof(gl_hooks_t, gl._api))   \
            : "x16"                                                 \
        );

#elif defined(__i386__)

    #define API_ENTRY(_api) __attribute__((noinline,optimize("omit-frame-pointer"))) _api

    #define CALL_GL_API(_api, ...)                                  \
        register void** fn;                                         \
        __asm__ volatile(                                           \
            "mov %%gs:0, %[fn]\n"                                   \
            "mov %P[tls](%[fn]), %[fn]\n"                           \
            "test %[fn], %[fn]\n"                                   \
            "je 1f\n"                                               \
            "jmp *%P[api](%[fn])\n"                                 \
            "1:\n"                                                  \
            : [fn] "=r" (fn)                                        \
            : [tls] "i" (TLS_SLOT_OPENGL_API*sizeof(void*)),        \
              [api] "i" (__builtin_offsetof(gl_hooks_t, gl._api))   \
            : "cc"                                                  \
            );

#elif defined(__x86_64__)

    #define API_ENTRY(_api) __attribute__((noinline,optimize("omit-frame-pointer"))) _api

    #define CALL_GL_API(_api, ...)                                  \
         register void** fn;                                        \
         __asm__ volatile(                                          \
            "mov %%fs:0, %[fn]\n"                                   \
            "mov %P[tls](%[fn]), %[fn]\n"                           \
            "test %[fn], %[fn]\n"                                   \
            "je 1f\n"                                               \
            "jmp *%P[api](%[fn])\n"                                 \
            "1:\n"                                                  \
            : [fn] "=r" (fn)                                        \
            : [tls] "i" (TLS_SLOT_OPENGL_API*sizeof(void*)),        \
              [api] "i" (__builtin_offsetof(gl_hooks_t, gl._api))   \
            : "cc"                                                  \
            );

#elif defined(__mips64)

    #define API_ENTRY(_api) __attribute__((noinline)) _api

    #define CALL_GL_API(_api, ...)                            \
    register unsigned long _t0 asm("$12");                    \
    register unsigned long _fn asm("$25");                    \
    register unsigned long _tls asm("$3");                    \
    register unsigned long _v0 asm("$2");                     \
    asm volatile(                                             \
        ".set  push\n\t"                                      \
        ".set  noreorder\n\t"                                 \
        "rdhwr %[tls], $29\n\t"                               \
        "ld    %[t0], %[OPENGL_API](%[tls])\n\t"              \
        "beqz  %[t0], 1f\n\t"                                 \
        " move %[fn], $ra\n\t"                                \
        "ld    %[t0], %[API](%[t0])\n\t"                      \
        "beqz  %[t0], 1f\n\t"                                 \
        " nop\n\t"                                            \
        "move  %[fn], %[t0]\n\t"                              \
        "1:\n\t"                                              \
        "jalr  $0, %[fn]\n\t"                                 \
        " move %[v0], $0\n\t"                                 \
        ".set  pop\n\t"                                       \
        : [fn] "=c"(_fn),                                     \
          [tls] "=&r"(_tls),                                  \
          [t0] "=&r"(_t0),                                    \
          [v0] "=&r"(_v0)                                     \
        : [OPENGL_API] "I"(TLS_SLOT_OPENGL_API*sizeof(void*)),\
          [API] "I"(__builtin_offsetof(gl_hooks_t, gl._api))  \
        :                                                     \
        );

#elif defined(__mips__)

    #define API_ENTRY(_api) __attribute__((noinline)) _api

    #define CALL_GL_API(_api, ...)                               \
        register unsigned int _t0 asm("$8");                     \
        register unsigned int _fn asm("$25");                    \
        register unsigned int _tls asm("$3");                    \
        register unsigned int _v0 asm("$2");                     \
        asm volatile(                                            \
            ".set  push\n\t"                                     \
            ".set  noreorder\n\t"                                \
            ".set  mips32r2\n\t"                                 \
            "rdhwr %[tls], $29\n\t"                              \
            "lw    %[t0], %[OPENGL_API](%[tls])\n\t"             \
            "beqz  %[t0], 1f\n\t"                                \
            " move %[fn],$ra\n\t"                                \
            "lw    %[t0], %[API](%[t0])\n\t"                     \
            "beqz  %[t0], 1f\n\t"                                \
            " nop\n\t"                                           \
            "move  %[fn], %[t0]\n\t"                             \
            "1:\n\t"                                             \
            "jalr  $0, %[fn]\n\t"                                \
            " move %[v0], $0\n\t"                                \
            ".set  pop\n\t"                                      \
            : [fn] "=c"(_fn),                                    \
              [tls] "=&r"(_tls),                                 \
              [t0] "=&r"(_t0),                                   \
              [v0] "=&r"(_v0)                                    \
            : [OPENGL_API] "I"(TLS_SLOT_OPENGL_API*4),           \
              [API] "I"(__builtin_offsetof(gl_hooks_t, gl._api)) \
            :                                                    \
            );

#endif

#define CALL_GL_API_RETURN(_api, ...) \
    CALL_GL_API(_api, __VA_ARGS__) \
    return 0;

extern "C" {
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "gl2_api.in"
#include "gl2ext_api.in"
#pragma GCC diagnostic warning "-Wunused-parameter"
}

#undef API_ENTRY
#undef CALL_GL_API
#undef CALL_GL_API_RETURN

/*
 * glGetString() and glGetStringi() are special because we expose some
 * extensions in the wrapper. Also, wrapping glGetXXX() is required because
 * the value returned for GL_NUM_EXTENSIONS may have been altered by the
 * injection of the additional extensions.
 */

extern "C" {
    const GLubyte * __glGetString(GLenum name);
    const GLubyte * __glGetStringi(GLenum name, GLuint index);
    void __glGetBooleanv(GLenum pname, GLboolean * data);
    void __glGetFloatv(GLenum pname, GLfloat * data);
    void __glGetIntegerv(GLenum pname, GLint * data);
    void __glGetInteger64v(GLenum pname, GLint64 * data);
    void __glStencilMaskSeparate(GLenum face, GLuint mask);
void __glGetUniformiv(GLuint program, GLint location, GLint *params);
void __glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
void __glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
void __glBindSampler(GLuint unit, GLuint sampler);
void __glLineWidth(GLfloat width);
void __glGetIntegeri_v(GLenum target, GLuint index, GLint *data);
void __glCompileShader(GLuint shader);
void __glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name);
void __glDepthRangef(GLfloat n, GLfloat f);
void __glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
GLuint __glCreateShader(GLenum type);
GLboolean __glIsBuffer(GLuint buffer);
void __glGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
void __glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void __glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
void __glVertexAttrib1f(GLuint index, GLfloat x);
void __glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
void __glHint(GLenum target, GLenum mode);
void __glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params);
void __glDeleteProgram(GLuint program);
void __glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void __glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
void __glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
void __glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value);
void __glDeleteSamplers(GLsizei count, const GLuint *samplers);
void __glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void __glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params);
void __glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
void __glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
void __glResumeTransformFeedback(void);
void __glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
void __glDrawArrays(GLenum mode, GLint first, GLsizei count);
void __glUniform1ui(GLint location, GLuint v0);
void __glClear(GLbitfield mask);
GLboolean __glIsEnabled(GLenum cap);
void __glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
void __glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLint __glGetFragDataLocation(GLuint program, const GLchar *name);
void __glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
void __glGenFramebuffers(GLsizei n, GLuint *framebuffers);
void __glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
GLboolean __glIsRenderbuffer(GLuint renderbuffer);
void * __glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
void __glDisableVertexAttribArray(GLuint index);
void __glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params);
void __glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices);
GLboolean __glIsShader(GLuint shader);
void __glEnable(GLenum cap);
void __glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params);
GLint __glGetAttribLocation(GLuint program, const GLchar *name);
void __glGetUniformfv(GLuint program, GLint location, GLfloat *params);
void __glGetUniformuiv(GLuint program, GLint location, GLuint *params);
void __glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params);
void __glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value);
void __glFlush(void);
void __glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params);
void __glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer);
GLsync __glFenceSync(GLenum condition, GLbitfield flags);
void __glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
void __glGenSamplers(GLsizei count, GLuint *samplers);
void __glUniform4iv(GLint location, GLsizei count, const GLint *value);
void __glClearStencil(GLint s);
void __glGenTextures(GLsizei n, GLuint *textures);
GLboolean __glIsSync(GLsync sync);
void __glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
void __glUniform2i(GLint location, GLint v0, GLint v1);
void __glUniform2f(GLint location, GLfloat v0, GLfloat v1);
void __glGetProgramiv(GLuint program, GLenum pname, GLint *params);
void __glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
void __glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
void __glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);
void __glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
void __glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data);
void __glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void __glUniform2iv(GLint location, GLsizei count, const GLint *value);
void __glUniform4uiv(GLint location, GLsizei count, const GLuint *value);
void __glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
void __glPolygonOffset(GLfloat factor, GLfloat units);
void __glVertexAttrib1fv(GLuint index, const GLfloat *v);
void __glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
void __glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height);
void __glDeleteSync(GLsync sync);
void __glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void __glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params);
void __glVertexAttrib3fv(GLuint index, const GLfloat *v);
void __glUniform3iv(GLint location, GLsizei count, const GLint *value);
void __glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
void __glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glUseProgram(GLuint program);
void __glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
void __glBindTransformFeedback(GLenum target, GLuint id);
void __glUniform2uiv(GLint location, GLsizei count, const GLuint *value);
void __glFinish(void);
void __glDeleteShader(GLuint shader);
void __glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
void __glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void __glUniform1uiv(GLint location, GLsizei count, const GLuint *value);
void __glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode);
void __glUniform2ui(GLint location, GLuint v0, GLuint v1);
void __glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void __glTexParameteri(GLenum target, GLenum pname, GLint param);
void __glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
void __glPixelStorei(GLenum pname, GLint param);
void __glValidateProgram(GLuint program);
void __glLinkProgram(GLuint program);
void __glBindTexture(GLenum target, GLuint texture);
void __glDetachShader(GLuint program, GLuint shader);
void __glDeleteTextures(GLsizei n, const GLuint *textures);
void __glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
void __glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void __glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);
void __glSampleCoverage(GLfloat value, GLboolean invert);
void __glSamplerParameteri(GLuint sampler, GLenum pname, GLint param);
void __glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
void __glUniform1f(GLint location, GLfloat v0);
void __glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params);
void __glUniform1i(GLint location, GLint v0);
void __glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void __glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
void __glDisable(GLenum cap);
void __glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void __glBindFramebuffer(GLenum target, GLuint framebuffer);
void __glCullFace(GLenum mode);
void __glAttachShader(GLuint program, GLuint shader);
void __glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length);
void __glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void __glUniform1iv(GLint location, GLsizei count, const GLint *value);
void __glReadBuffer(GLenum src);
void __glGenerateMipmap(GLenum target);
void __glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param);
void __glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);
void __glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
GLboolean __glUnmapBuffer(GLenum target);
void __glBindRenderbuffer(GLenum target, GLuint renderbuffer);
GLboolean __glIsProgram(GLuint program);
void __glVertexAttrib4fv(GLuint index, const GLfloat *v);
GLboolean __glIsTransformFeedback(GLuint id);
void __glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void __glActiveTexture(GLenum texture);
void __glEnableVertexAttribArray(GLuint index);
void __glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
void __glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void __glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glStencilFunc(GLenum func, GLint ref, GLuint mask);
void __glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
void __glVertexAttribI4iv(GLuint index, const GLint *v);
void __glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
void __glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
void __glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
void __glGenBuffers(GLsizei n, GLuint *buffers);
void __glBlendFunc(GLenum sfactor, GLenum dfactor);
GLuint __glCreateProgram(void);
void __glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
GLboolean __glIsFramebuffer(GLuint framebuffer);
void __glDeleteBuffers(GLsizei n, const GLuint *buffers);
void __glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void __glUniform3uiv(GLint location, GLsizei count, const GLuint *value);
void __glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void __glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value);
void __glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params);
void __glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
void __glVertexAttribI4uiv(GLuint index, const GLuint *v);
void __glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
void __glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
void __glClearDepthf(GLfloat d);
void __glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glGenTransformFeedbacks(GLsizei n, GLuint *ids);
void __glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params);
void __glDepthFunc(GLenum func);
void __glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
void __glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
GLenum __glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
void __glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
void __glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void __glBlendEquation(GLenum mode);
GLint __glGetUniformLocation(GLuint program, const GLchar *name);
void __glEndTransformFeedback(void);
void __glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
void __glBeginTransformFeedback(GLenum primitiveMode);
GLboolean __glIsSampler(GLuint sampler);
void __glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids);
GLenum __glCheckFramebufferStatus(GLenum target);
void __glBindAttribLocation(GLuint program, GLuint index, const GLchar *name);
void __glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void __glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
void __glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
void __glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
void __glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName);
void __glReleaseShaderCompiler(void);
void __glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values);
void __glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glBindBuffer(GLenum target, GLuint buffer);
void __glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void __glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
void __glPauseTransformFeedback(void);
GLenum __glGetError(void);
void __glVertexAttrib2fv(GLuint index, const GLfloat *v);
void __glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params);
void __glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
void __glStencilMask(GLuint mask);
void __glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param);
GLboolean __glIsTexture(GLuint texture);
void __glUniform1fv(GLint location, GLsizei count, const GLfloat *value);
void __glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
void __glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params);
void __glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
void __glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments);
void __glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
void __glDepthMask(GLboolean flag);
GLuint __glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName);
void __glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void __glFrontFace(GLenum mode);

}

void *handle;
void *eglhandle; 

__attribute__((visibility("default"))) EGLBoolean eglMakeCurrent(EGLDisplay display,
                                                                 EGLSurface draw,
                                                                 EGLSurface read,
                                                                 EGLContext context) {

    EGLBoolean egl_result;

    dlerror(); 
    EGLBoolean (*egl_forward)(EGLDisplay, EGLSurface, EGLSurface, EGLContext) = 
        reinterpret_cast<EGLBoolean(*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext)>
        (dlsym(eglhandle, "eglMakeCurrent"));
    if (!egl_forward) { 
        LOGERR("%s: %s", __func__, dlerror());
        abort();
    } else { 
        egl_result = egl_forward(display, draw, read, context); 
    }

    dlerror(); 
    void (*milko_create_fn)(void *) = reinterpret_cast<void(*)(void *)>
       (dlsym(handle, "milko_create"));
    if (!milko_create_fn) {
        LOGERR("%s: %s", __func__, dlerror());
        abort(); 
    } else {
        milko_create_fn((void *) context);
    }

    return egl_result;
}

__attribute__((constructor)) void load(void) {

    handle = dlopen ("/data/local/lib64/libgpu.cr.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        LOGERR("%s: %s (fatal)", __func__, dlerror());
        abort();
    }
    eglhandle = dlopen("/data/local/lib64/libEGL_Secure.so", RTLD_NOW | RTLD_GLOBAL);
    if (!eglhandle) {
        LOGERR("%s: %s (fatal)", __func__, dlerror());
        abort();
    }
}

__attribute__((destructor)) void unload(void) {   
    dlclose(handle);
    dlclose(eglhandle);
}

const GLubyte * glGetString(GLenum name) {
    const GLubyte * ret = egl_get_string_for_current_context(name);
    if (ret == NULL) {
        gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;
        if(_c) ret = _c->glGetString(name);
    }
    return ret;
}

const GLubyte * glGetStringi(GLenum name, GLuint index) {
    const GLubyte * ret = egl_get_string_for_current_context(name, index);
    if (ret == NULL) {
        gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;
        if(_c) ret = _c->glGetStringi(name, index);
    }
    return ret;
}

void glGetBooleanv(GLenum pname, GLboolean * data) {
    if (pname == GL_NUM_EXTENSIONS) {
        int num_exts = egl_get_num_extensions_for_current_context();
        if (num_exts >= 0) {
            *data = num_exts > 0 ? GL_TRUE : GL_FALSE;
            return;
        }
    }

    gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;
    if (_c) _c->glGetBooleanv(pname, data);
}

void glGetFloatv(GLenum pname, GLfloat * data) {
    if (pname == GL_NUM_EXTENSIONS) {
        int num_exts = egl_get_num_extensions_for_current_context();
        if (num_exts >= 0) {
            *data = (GLfloat)num_exts;
            return;
        }
    }

    gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;
    if (_c) _c->glGetFloatv(pname, data);
}

void glGetIntegerv(GLenum pname, GLint * data) {
    if (pname == GL_NUM_EXTENSIONS) {
        int num_exts = egl_get_num_extensions_for_current_context();
        if (num_exts >= 0) {
            *data = (GLint)num_exts;
            return;
        }
    }

    gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;
    if (_c) _c->glGetIntegerv(pname, data);
}

void glGetInteger64v(GLenum pname, GLint64 * data) {
    if (pname == GL_NUM_EXTENSIONS) {
        int num_exts = egl_get_num_extensions_for_current_context();
        if (num_exts >= 0) {
            *data = (GLint64)num_exts;
            return;
        }
    }

    gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;
    if (_c) _c->glGetInteger64v(pname, data);
}

void glStencilMaskSeparate(GLenum face, GLuint mask) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint) = reinterpret_cast<void(*)(GLenum, GLuint)>
      (dlsym(handle, "milko_glStencilMaskSeparate"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(face, mask); }
}

void glGetUniformiv(GLuint program, GLint location, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLint, GLint*) = reinterpret_cast<void(*)(GLuint, GLint, GLint*)>
      (dlsym(handle, "milko_glGetUniformiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, location, params); }
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLenum, GLuint) = reinterpret_cast<void(*)(GLenum, GLenum, GLenum, GLuint)>
      (dlsym(handle, "milko_glFramebufferRenderbuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, attachment, renderbuffertarget, renderbuffer); }
}

void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*)>
      (dlsym(handle, "milko_glCompressedTexSubImage3D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data); }
}

void glBindSampler(GLuint unit, GLuint sampler) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint) = reinterpret_cast<void(*)(GLuint, GLuint)>
      (dlsym(handle, "milko_glBindSampler"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(unit, sampler); }
}

void glLineWidth(GLfloat width) {
    dlerror(); 
    void (*checkedGL)(GLfloat) = reinterpret_cast<void(*)(GLfloat)>
      (dlsym(handle, "milko_glLineWidth"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(width); }
}

void glGetIntegeri_v(GLenum target, GLuint index, GLint *data) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint, GLint*) = reinterpret_cast<void(*)(GLenum, GLuint, GLint*)>
      (dlsym(handle, "milko_glGetIntegeri_v"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, index, data); }
}

void glCompileShader(GLuint shader) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glCompileShader"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(shader); }
}

void glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, GLsizei, GLsizei*, GLsizei*, GLenum*, GLchar*) = reinterpret_cast<void(*)(GLuint, GLuint, GLsizei, GLsizei*, GLsizei*, GLenum*, GLchar*)>
      (dlsym(handle, "milko_glGetTransformFeedbackVarying"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, index, bufSize, length, size, type, name); }
}

void glDepthRangef(GLfloat n, GLfloat f) {
    dlerror(); 
    void (*checkedGL)(GLfloat, GLfloat) = reinterpret_cast<void(*)(GLfloat, GLfloat)>
      (dlsym(handle, "milko_glDepthRangef"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, f); }
}

void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLint, GLenum, GLsizei, const void*) = reinterpret_cast<void(*)(GLuint, GLint, GLenum, GLsizei, const void*)>
      (dlsym(handle, "milko_glVertexAttribIPointer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, size, type, stride, pointer); }
}

GLuint glCreateShader(GLenum type) {
    dlerror(); 
    GLuint (*checkedGL)(GLenum) = reinterpret_cast<GLuint(*)(GLenum)>
      (dlsym(handle, "milko_glCreateShader"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(type); }
}

GLboolean glIsBuffer(GLuint buffer) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsBuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(buffer); }
}

void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, GLuint*) = reinterpret_cast<void(*)(GLsizei, GLuint*)>
      (dlsym(handle, "milko_glGenRenderbuffers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, renderbuffers); }
}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei)>
      (dlsym(handle, "milko_glCopyTexSubImage2D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, xoffset, yoffset, x, y, width, height); }
}

void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*)>
      (dlsym(handle, "milko_glCompressedTexImage2D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, internalformat, width, height, border, imageSize, data); }
}

void glVertexAttrib1f(GLuint index, GLfloat x) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLfloat) = reinterpret_cast<void(*)(GLuint, GLfloat)>
      (dlsym(handle, "milko_glVertexAttrib1f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, x); }
}

void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLenum, GLenum) = reinterpret_cast<void(*)(GLenum, GLenum, GLenum, GLenum)>
      (dlsym(handle, "milko_glBlendFuncSeparate"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha); }
}

void glHint(GLenum target, GLenum mode) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum) = reinterpret_cast<void(*)(GLenum, GLenum)>
      (dlsym(handle, "milko_glHint"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, mode); }
}

void glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix3x2fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLenum, GLsizei, GLint*) = reinterpret_cast<void(*)(GLenum, GLenum, GLenum, GLsizei, GLint*)>
      (dlsym(handle, "milko_glGetInternalformativ"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, internalformat, pname, bufSize, params); }
}

void glDeleteProgram(GLuint program) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glDeleteProgram"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program); }
}

void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLsizei, GLsizei) = reinterpret_cast<void(*)(GLenum, GLenum, GLsizei, GLsizei)>
      (dlsym(handle, "milko_glRenderbufferStorage"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, internalformat, width, height); }
}

void glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    dlerror(); 
    void (*checkedGL)(GLsync, GLbitfield, GLuint64) = reinterpret_cast<void(*)(GLsync, GLbitfield, GLuint64)>
      (dlsym(handle, "milko_glWaitSync"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sync, flags, timeout); }
}

void glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix4x3fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    dlerror(); 
    void (*checkedGL)(GLint, GLint, GLint, GLint) = reinterpret_cast<void(*)(GLint, GLint, GLint, GLint)>
      (dlsym(handle, "milko_glUniform3i"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1, v2); }
}

void glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, const GLfloat*) = reinterpret_cast<void(*)(GLenum, GLint, const GLfloat*)>
      (dlsym(handle, "milko_glClearBufferfv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(buffer, drawbuffer, value); }
}

void glDeleteSamplers(GLsizei count, const GLuint *samplers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glDeleteSamplers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(count, samplers); }
}

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    dlerror(); 
    void (*checkedGL)(GLint, GLfloat, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLint, GLfloat, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glUniform3f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1, v2); }
}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLint*) = reinterpret_cast<void(*)(GLenum, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetBufferParameteriv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, params); }
}

void glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLfloat, GLint) = reinterpret_cast<void(*)(GLenum, GLint, GLfloat, GLint)>
      (dlsym(handle, "milko_glClearBufferfi"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(buffer, drawbuffer, depth, stencil); }
}

void glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei) = reinterpret_cast<void(*)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei)>
      (dlsym(handle, "milko_glTexStorage3D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, levels, internalformat, width, height, depth); }
}

void glResumeTransformFeedback(void) {
    dlerror(); 
    void (*checkedGL)() = reinterpret_cast<void(*)()>
      (dlsym(handle, "milko_glResumeTransformFeedback"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(); }
}

void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glDeleteFramebuffers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, framebuffers); }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLsizei) = reinterpret_cast<void(*)(GLenum, GLint, GLsizei)>
      (dlsym(handle, "milko_glDrawArrays"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(mode, first, count); }
}

void glUniform1ui(GLint location, GLuint v0) {
    dlerror(); 
    void (*checkedGL)(GLint, GLuint) = reinterpret_cast<void(*)(GLint, GLuint)>
      (dlsym(handle, "milko_glUniform1ui"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0); }
}

GLboolean glIsEnabled(GLenum cap) {
    dlerror(); 
    GLboolean (*checkedGL)(GLenum) = reinterpret_cast<GLboolean(*)(GLenum)>
      (dlsym(handle, "milko_glIsEnabled"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(cap); }
}

void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLenum) = reinterpret_cast<void(*)(GLenum, GLenum, GLenum)>
      (dlsym(handle, "milko_glStencilOp"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(fail, zfail, zpass); }
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLenum, GLuint, GLint) = reinterpret_cast<void(*)(GLenum, GLenum, GLenum, GLuint, GLint)>
      (dlsym(handle, "milko_glFramebufferTexture2D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, attachment, textarget, texture, level); }
}

GLint glGetFragDataLocation(GLuint program, const GLchar *name) {
    dlerror(); 
    GLint (*checkedGL)(GLuint, const GLchar*) = reinterpret_cast<GLint(*)(GLuint, const GLchar*)>
      (dlsym(handle, "milko_glGetFragDataLocation"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(program, name); }
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, const GLint*) = reinterpret_cast<void(*)(GLenum, GLenum, const GLint*)>
      (dlsym(handle, "milko_glTexParameteriv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, params); }
}

void glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, GLuint*) = reinterpret_cast<void(*)(GLsizei, GLuint*)>
      (dlsym(handle, "milko_glGenFramebuffers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, framebuffers); }
}

void glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, GLsizei*, GLuint*) = reinterpret_cast<void(*)(GLuint, GLsizei, GLsizei*, GLuint*)>
      (dlsym(handle, "milko_glGetAttachedShaders"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, maxCount, count, shaders); }
}

GLboolean glIsRenderbuffer(GLuint renderbuffer) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsRenderbuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(renderbuffer); }
}

void * glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    dlerror(); 
    void * (*checkedGL)(GLenum, GLintptr, GLsizeiptr, GLbitfield) = reinterpret_cast<void *(*)(GLenum, GLintptr, GLsizeiptr, GLbitfield)>
      (dlsym(handle, "milko_glMapBufferRange"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { return checkedGL(target, offset, length, access); }
}

void glDisableVertexAttribArray(GLuint index) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glDisableVertexAttribArray"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index); }
}

void glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLfloat*) = reinterpret_cast<void(*)(GLuint, GLenum, GLfloat*)>
      (dlsym(handle, "milko_glGetSamplerParameterfv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sampler, pname, params); }
}

void glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, const GLchar *const*, GLuint*) = reinterpret_cast<void(*)(GLuint, GLsizei, const GLchar *const*, GLuint*)>
      (dlsym(handle, "milko_glGetUniformIndices"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, uniformCount, uniformNames, uniformIndices); }
}

GLboolean glIsShader(GLuint shader) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsShader"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(shader); }
}

void glEnable(GLenum cap) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glEnable"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(cap); }
}

void glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, const GLuint*, GLenum, GLint*) = reinterpret_cast<void(*)(GLuint, GLsizei, const GLuint*, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetActiveUniformsiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, uniformCount, uniformIndices, pname, params); }
}

GLint glGetAttribLocation(GLuint program, const GLchar *name) {
    dlerror(); 
    GLint (*checkedGL)(GLuint, const GLchar*) = reinterpret_cast<GLint(*)(GLuint, const GLchar*)>
      (dlsym(handle, "milko_glGetAttribLocation"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(program, name); }
}

void glGetUniformfv(GLuint program, GLint location, GLfloat *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLint, GLfloat*) = reinterpret_cast<void(*)(GLuint, GLint, GLfloat*)>
      (dlsym(handle, "milko_glGetUniformfv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, location, params); }
}

void glGetUniformuiv(GLuint program, GLint location, GLuint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLint, GLuint*) = reinterpret_cast<void(*)(GLuint, GLint, GLuint*)>
      (dlsym(handle, "milko_glGetUniformuiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, location, params); }
}

void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLint*) = reinterpret_cast<void(*)(GLuint, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetVertexAttribIiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, pname, params); }
}

void glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, const GLuint*) = reinterpret_cast<void(*)(GLenum, GLint, const GLuint*)>
      (dlsym(handle, "milko_glClearBufferuiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(buffer, drawbuffer, value); }
}

void glFlush(void) {
    dlerror(); 
    void (*checkedGL)() = reinterpret_cast<void(*)()>
      (dlsym(handle, "milko_glFlush"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(); }
}

void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLint*) = reinterpret_cast<void(*)(GLenum, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetRenderbufferParameteriv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, params); }
}

void glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, void**) = reinterpret_cast<void(*)(GLuint, GLenum, void**)>
      (dlsym(handle, "milko_glGetVertexAttribPointerv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, pname, pointer); }
}

GLsync glFenceSync(GLenum condition, GLbitfield flags) {
    dlerror(); 
    GLsync (*checkedGL)(GLenum, GLbitfield) = reinterpret_cast<GLsync(*)(GLenum, GLbitfield)>
      (dlsym(handle, "milko_glFenceSync"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(condition, flags); }
}

void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLint, GLuint) = reinterpret_cast<void(*)(GLenum, GLenum, GLint, GLuint)>
      (dlsym(handle, "milko_glStencilFuncSeparate"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(face, func, ref, mask); }
}

void glGenSamplers(GLsizei count, GLuint *samplers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, GLuint*) = reinterpret_cast<void(*)(GLsizei, GLuint*)>
      (dlsym(handle, "milko_glGenSamplers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(count, samplers); }
}

void glUniform4iv(GLint location, GLsizei count, const GLint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLint*)>
      (dlsym(handle, "milko_glUniform4iv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glClearStencil(GLint s) {
    dlerror(); 
    void (*checkedGL)(GLint) = reinterpret_cast<void(*)(GLint)>
      (dlsym(handle, "milko_glClearStencil"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(s); }
}

void glGenTextures(GLsizei n, GLuint *textures) {
    dlerror(); 
    void (*checkedGL)(GLsizei, GLuint*) = reinterpret_cast<void(*)(GLsizei, GLuint*)>
      (dlsym(handle, "milko_glGenTextures"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, textures); }
}

GLboolean glIsSync(GLsync sync) {
    dlerror(); 
    GLboolean (*checkedGL)(GLsync) = reinterpret_cast<GLboolean(*)(GLsync)>
      (dlsym(handle, "milko_glIsSync"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(sync); }
}

void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glDeleteRenderbuffers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, renderbuffers); }
}

void glUniform2i(GLint location, GLint v0, GLint v1) {
    dlerror(); 
    void (*checkedGL)(GLint, GLint, GLint) = reinterpret_cast<void(*)(GLint, GLint, GLint)>
      (dlsym(handle, "milko_glUniform2i"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1); }
}

void glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    dlerror(); 
    void (*checkedGL)(GLint, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLint, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glUniform2f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1); }
}

void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLint*) = reinterpret_cast<void(*)(GLuint, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetProgramiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, pname, params); }
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = reinterpret_cast<void(*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)>
      (dlsym(handle, "milko_glVertexAttribPointer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, size, type, normalized, stride, pointer); }
}

void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLuint, GLint, GLint) = reinterpret_cast<void(*)(GLenum, GLenum, GLuint, GLint, GLint)>
      (dlsym(handle, "milko_glFramebufferTextureLayer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, attachment, texture, level, layer); }
}

void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLintptr, GLsizeiptr) = reinterpret_cast<void(*)(GLenum, GLintptr, GLsizeiptr)>
      (dlsym(handle, "milko_glFlushMappedBufferRange"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, offset, length); }
}

void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*)>
      (dlsym(handle, "milko_glTexSubImage3D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels); }
}

void glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint, GLint64*) = reinterpret_cast<void(*)(GLenum, GLuint, GLint64*)>
      (dlsym(handle, "milko_glGetInteger64i_v"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, index, data); }
}

void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) = reinterpret_cast<void(*)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint)>
      (dlsym(handle, "milko_glCopyTexImage2D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, internalformat, x, y, width, height, border); }
}

void glUniform2iv(GLint location, GLsizei count, const GLint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLint*)>
      (dlsym(handle, "milko_glUniform2iv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glUniform4uiv(GLint location, GLsizei count, const GLuint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glUniform4uiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLint*) = reinterpret_cast<void(*)(GLuint, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetShaderiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(shader, pname, params); }
}

void glPolygonOffset(GLfloat factor, GLfloat units) {
    dlerror(); 
    void (*checkedGL)(GLfloat, GLfloat) = reinterpret_cast<void(*)(GLfloat, GLfloat)>
      (dlsym(handle, "milko_glPolygonOffset"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(factor, units); }
}

void glVertexAttrib1fv(GLuint index, const GLfloat *v) {
    dlerror(); 
    void (*checkedGL)(GLuint, const GLfloat*) = reinterpret_cast<void(*)(GLuint, const GLfloat*)>
      (dlsym(handle, "milko_glVertexAttrib1fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, v); }
}

void glUniform3fv(GLint location, GLsizei count, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLfloat*)>
      (dlsym(handle, "milko_glUniform3fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLsizei, const GLenum*, GLint, GLint, GLsizei, GLsizei) = reinterpret_cast<void(*)(GLenum, GLsizei, const GLenum*, GLint, GLint, GLsizei, GLsizei)>
      (dlsym(handle, "milko_glInvalidateSubFramebuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, numAttachments, attachments, x, y, width, height); }
}

void glDeleteSync(GLsync sync) {
    dlerror(); 
    void (*checkedGL)(GLsync) = reinterpret_cast<void(*)(GLsync)>
      (dlsym(handle, "milko_glDeleteSync"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sync); }
}

void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei)>
      (dlsym(handle, "milko_glCopyTexSubImage3D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, xoffset, yoffset, zoffset, x, y, width, height); }
}

void glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLint*) = reinterpret_cast<void(*)(GLuint, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetVertexAttribiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, pname, params); }
}

void glVertexAttrib3fv(GLuint index, const GLfloat *v) {
    dlerror(); 
    void (*checkedGL)(GLuint, const GLfloat*) = reinterpret_cast<void(*)(GLuint, const GLfloat*)>
      (dlsym(handle, "milko_glVertexAttrib3fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, v); }
}

void glUniform3iv(GLint location, GLsizei count, const GLint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLint*)>
      (dlsym(handle, "milko_glUniform3iv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, GLenum, GLint*) = reinterpret_cast<void(*)(GLuint, GLuint, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetActiveUniformBlockiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, uniformBlockIndex, pname, params); }
}

void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix2fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glUseProgram(GLuint program) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glUseProgram"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program); }
}

void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, GLsizei*, GLchar*) = reinterpret_cast<void(*)(GLuint, GLsizei, GLsizei*, GLchar*)>
      (dlsym(handle, "milko_glGetProgramInfoLog"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, bufSize, length, infoLog); }
}

void glBindTransformFeedback(GLenum target, GLuint id) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint) = reinterpret_cast<void(*)(GLenum, GLuint)>
      (dlsym(handle, "milko_glBindTransformFeedback"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, id); }
}

void glUniform2uiv(GLint location, GLsizei count, const GLuint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glUniform2uiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glFinish(void) {
    dlerror(); 
    void (*checkedGL)() = reinterpret_cast<void(*)()>
      (dlsym(handle, "milko_glFinish"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(); }
}

void glDeleteShader(GLuint shader) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glDeleteShader"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(shader); }
}

void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void*)>
      (dlsym(handle, "milko_glCompressedTexImage3D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, internalformat, width, height, depth, border, imageSize, data); }
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    dlerror(); 
    void (*checkedGL)(GLint, GLint, GLsizei, GLsizei) = reinterpret_cast<void(*)(GLint, GLint, GLsizei, GLsizei)>
      (dlsym(handle, "milko_glViewport"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(x, y, width, height); }
}

void glUniform1uiv(GLint location, GLsizei count, const GLuint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glUniform1uiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, const GLchar *const*, GLenum) = reinterpret_cast<void(*)(GLuint, GLsizei, const GLchar *const*, GLenum)>
      (dlsym(handle, "milko_glTransformFeedbackVaryings"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, count, varyings, bufferMode); }
}

void glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    dlerror(); 
    void (*checkedGL)(GLint, GLuint, GLuint) = reinterpret_cast<void(*)(GLint, GLuint, GLuint)>
      (dlsym(handle, "milko_glUniform2ui"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1); }
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLfloat) = reinterpret_cast<void(*)(GLenum, GLenum, GLfloat)>
      (dlsym(handle, "milko_glTexParameterf"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, param); }
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLint) = reinterpret_cast<void(*)(GLenum, GLenum, GLint)>
      (dlsym(handle, "milko_glTexParameteri"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, param); }
}

void glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, GLsizei*, GLchar*) = reinterpret_cast<void(*)(GLuint, GLsizei, GLsizei*, GLchar*)>
      (dlsym(handle, "milko_glGetShaderSource"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(shader, bufSize, length, source); }
}

void glPixelStorei(GLenum pname, GLint param) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint) = reinterpret_cast<void(*)(GLenum, GLint)>
      (dlsym(handle, "milko_glPixelStorei"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(pname, param); }
}

void glValidateProgram(GLuint program) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glValidateProgram"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program); }
}

void glLinkProgram(GLuint program) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glLinkProgram"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program); }
}

void glBindTexture(GLenum target, GLuint texture) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint) = reinterpret_cast<void(*)(GLenum, GLuint)>
      (dlsym(handle, "milko_glBindTexture"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, texture); }
}

void glDetachShader(GLuint program, GLuint shader) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint) = reinterpret_cast<void(*)(GLuint, GLuint)>
      (dlsym(handle, "milko_glDetachShader"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, shader); }
}

void glDeleteTextures(GLsizei n, const GLuint *textures) {
    dlerror(); 
    void (*checkedGL)(GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glDeleteTextures"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, textures); }
}

void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLenum, GLenum) = reinterpret_cast<void(*)(GLenum, GLenum, GLenum, GLenum)>
      (dlsym(handle, "milko_glStencilOpSeparate"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(face, sfail, dpfail, dppass); }
}

void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glVertexAttrib4f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, x, y, z, w); }
}

void glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix3x4fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLint*) = reinterpret_cast<void(*)(GLenum, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetTexParameteriv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, params); }
}

void glSampleCoverage(GLfloat value, GLboolean invert) {
    dlerror(); 
    void (*checkedGL)(GLfloat, GLboolean) = reinterpret_cast<void(*)(GLfloat, GLboolean)>
      (dlsym(handle, "milko_glSampleCoverage"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(value, invert); }
}

void glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLint) = reinterpret_cast<void(*)(GLuint, GLenum, GLint)>
      (dlsym(handle, "milko_glSamplerParameteri"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sampler, pname, param); }
}

void glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLfloat) = reinterpret_cast<void(*)(GLuint, GLenum, GLfloat)>
      (dlsym(handle, "milko_glSamplerParameterf"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sampler, pname, param); }
}

void glUniform1f(GLint location, GLfloat v0) {
    dlerror(); 
    void (*checkedGL)(GLint, GLfloat) = reinterpret_cast<void(*)(GLint, GLfloat)>
      (dlsym(handle, "milko_glUniform1f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0); }
}

void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLfloat*) = reinterpret_cast<void(*)(GLuint, GLenum, GLfloat*)>
      (dlsym(handle, "milko_glGetVertexAttribfv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, pname, params); }
}

void glUniform1i(GLint location, GLint v0) {
    dlerror(); 
    void (*checkedGL)(GLint, GLint) = reinterpret_cast<void(*)(GLint, GLint)>
      (dlsym(handle, "milko_glUniform1i"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0); }
}

void glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*) = reinterpret_cast<void(*)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*)>
      (dlsym(handle, "milko_glGetActiveAttrib"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, index, bufSize, length, size, type, name); }
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)>
      (dlsym(handle, "milko_glTexSubImage2D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, xoffset, yoffset, width, height, format, type, pixels); }
}

void glDisable(GLenum cap) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glDisable"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(cap); }
}

void glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    dlerror(); 
    void (*checkedGL)(GLint, GLuint, GLuint, GLuint, GLuint) = reinterpret_cast<void(*)(GLint, GLuint, GLuint, GLuint, GLuint)>
      (dlsym(handle, "milko_glUniform4ui"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1, v2, v3); }
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint) = reinterpret_cast<void(*)(GLenum, GLuint)>
      (dlsym(handle, "milko_glBindFramebuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, framebuffer); }
}

void glCullFace(GLenum mode) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glCullFace"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(mode); }
}

void glAttachShader(GLuint program, GLuint shader) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint) = reinterpret_cast<void(*)(GLuint, GLuint)>
      (dlsym(handle, "milko_glAttachShader"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, shader); }
}

void glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length) {
    dlerror(); 
    void (*checkedGL)(GLsizei, const GLuint*, GLenum, const void*, GLsizei) = reinterpret_cast<void(*)(GLsizei, const GLuint*, GLenum, const void*, GLsizei)>
      (dlsym(handle, "milko_glShaderBinary"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(count, shaders, binaryformat, binary, length); }
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLsizei, GLenum, const void*) = reinterpret_cast<void(*)(GLenum, GLsizei, GLenum, const void*)>
      (dlsym(handle, "milko_glDrawElements"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(mode, count, type, indices); }
}

void glUniform1iv(GLint location, GLsizei count, const GLint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLint*)>
      (dlsym(handle, "milko_glUniform1iv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glReadBuffer(GLenum src) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glReadBuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(src); }
}

void glGenerateMipmap(GLenum target) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glGenerateMipmap"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target); }
}

void glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, const GLint*) = reinterpret_cast<void(*)(GLuint, GLenum, const GLint*)>
      (dlsym(handle, "milko_glSamplerParameteriv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sampler, pname, param); }
}

void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLfloat, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLuint, GLfloat, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glVertexAttrib3f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, x, y, z); }
}

void glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    dlerror(); 
    void (*checkedGL)(GLfloat, GLfloat, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLfloat, GLfloat, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glBlendColor"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(red, green, blue, alpha); }
}

GLboolean glUnmapBuffer(GLenum target) {
    dlerror(); 
    GLboolean (*checkedGL)(GLenum) = reinterpret_cast<GLboolean(*)(GLenum)>
      (dlsym(handle, "milko_glUnmapBuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(target); }
}

void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint) = reinterpret_cast<void(*)(GLenum, GLuint)>
      (dlsym(handle, "milko_glBindRenderbuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, renderbuffer); }
}

GLboolean glIsProgram(GLuint program) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsProgram"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(program); }
}

void glVertexAttrib4fv(GLuint index, const GLfloat *v) {
    dlerror(); 
    void (*checkedGL)(GLuint, const GLfloat*) = reinterpret_cast<void(*)(GLuint, const GLfloat*)>
      (dlsym(handle, "milko_glVertexAttrib4fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, v); }
}

GLboolean glIsTransformFeedback(GLuint id) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsTransformFeedback"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(id); }
}

void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    dlerror(); 
    void (*checkedGL)(GLint, GLint, GLint, GLint, GLint) = reinterpret_cast<void(*)(GLint, GLint, GLint, GLint, GLint)>
      (dlsym(handle, "milko_glUniform4i"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1, v2, v3); }
}

void glActiveTexture(GLenum texture) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glActiveTexture"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(texture); }
}

void glEnableVertexAttribArray(GLuint index) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glEnableVertexAttribArray"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index); }
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
    dlerror(); 
    void (*checkedGL)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = reinterpret_cast<void(*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)>
      (dlsym(handle, "milko_glReadPixels"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(x, y, width, height, format, type, pixels); }
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    dlerror(); 
    void (*checkedGL)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glUniform4f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1, v2, v3); }
}

void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix3fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLuint) = reinterpret_cast<void(*)(GLenum, GLint, GLuint)>
      (dlsym(handle, "milko_glStencilFunc"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(func, ref, mask); }
}

void glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, GLuint) = reinterpret_cast<void(*)(GLuint, GLuint, GLuint)>
      (dlsym(handle, "milko_glUniformBlockBinding"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, uniformBlockIndex, uniformBlockBinding); }
}

void glVertexAttribI4iv(GLuint index, const GLint *v) {
    dlerror(); 
    void (*checkedGL)(GLuint, const GLint*) = reinterpret_cast<void(*)(GLuint, const GLint*)>
      (dlsym(handle, "milko_glVertexAttribI4iv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, v); }
}

void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, GLsizei*, GLchar*) = reinterpret_cast<void(*)(GLuint, GLsizei, GLsizei*, GLchar*)>
      (dlsym(handle, "milko_glGetShaderInfoLog"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(shader, bufSize, length, infoLog); }
}

void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLint, GLint, GLint, GLint) = reinterpret_cast<void(*)(GLuint, GLint, GLint, GLint, GLint)>
      (dlsym(handle, "milko_glVertexAttribI4i"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, x, y, z, w); }
}

void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum) = reinterpret_cast<void(*)(GLenum, GLenum)>
      (dlsym(handle, "milko_glBlendEquationSeparate"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(modeRGB, modeAlpha); }
}

void glGenBuffers(GLsizei n, GLuint *buffers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, GLuint*) = reinterpret_cast<void(*)(GLsizei, GLuint*)>
      (dlsym(handle, "milko_glGenBuffers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, buffers); }
}

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum) = reinterpret_cast<void(*)(GLenum, GLenum)>
      (dlsym(handle, "milko_glBlendFunc"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sfactor, dfactor); }
}

GLuint glCreateProgram(void) {
    dlerror(); 
    GLuint (*checkedGL)() = reinterpret_cast<GLuint(*)()>
      (dlsym(handle, "milko_glCreateProgram"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(); }
}

void glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)>
      (dlsym(handle, "milko_glTexImage3D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, internalformat, width, height, depth, border, format, type, pixels); }
}

GLboolean glIsFramebuffer(GLuint framebuffer) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsFramebuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(framebuffer); }
}

void glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    dlerror(); 
    void (*checkedGL)(GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glDeleteBuffers"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, buffers); }
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    dlerror(); 
    void (*checkedGL)(GLint, GLint, GLsizei, GLsizei) = reinterpret_cast<void(*)(GLint, GLint, GLsizei, GLsizei)>
      (dlsym(handle, "milko_glScissor"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(x, y, width, height); }
}

void glUniform3uiv(GLint location, GLsizei count, const GLuint *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glUniform3uiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    dlerror(); 
    void (*checkedGL)(GLfloat, GLfloat, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLfloat, GLfloat, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glClearColor"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(red, green, blue, alpha); }
}

void glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, const GLint*) = reinterpret_cast<void(*)(GLenum, GLint, const GLint*)>
      (dlsym(handle, "milko_glClearBufferiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(buffer, drawbuffer, value); }
}

void glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLint64*) = reinterpret_cast<void(*)(GLenum, GLenum, GLint64*)>
      (dlsym(handle, "milko_glGetBufferParameteri64v"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, params); }
}

void glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    dlerror(); 
    void (*checkedGL)(GLint, GLuint, GLuint, GLuint) = reinterpret_cast<void(*)(GLint, GLuint, GLuint, GLuint)>
      (dlsym(handle, "milko_glUniform3ui"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, v0, v1, v2); }
}

void glVertexAttribI4uiv(GLuint index, const GLuint *v) {
    dlerror(); 
    void (*checkedGL)(GLuint, const GLuint*) = reinterpret_cast<void(*)(GLuint, const GLuint*)>
      (dlsym(handle, "milko_glVertexAttribI4uiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, v); }
}

void glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLfloat*)>
      (dlsym(handle, "milko_glUniform2fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr) = reinterpret_cast<void(*)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr)>
      (dlsym(handle, "milko_glBindBufferRange"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, index, buffer, offset, size); }
}

void glClearDepthf(GLfloat d) {
    dlerror(); 
    void (*checkedGL)(GLfloat) = reinterpret_cast<void(*)(GLfloat)>
      (dlsym(handle, "milko_glClearDepthf"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(d); }
}

void glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix2x3fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glGenTransformFeedbacks(GLsizei n, GLuint *ids) {
    dlerror(); 
    void (*checkedGL)(GLsizei, GLuint*) = reinterpret_cast<void(*)(GLsizei, GLuint*)>
      (dlsym(handle, "milko_glGenTransformFeedbacks"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, ids); }
}

void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLuint*) = reinterpret_cast<void(*)(GLuint, GLenum, GLuint*)>
      (dlsym(handle, "milko_glGetVertexAttribIuiv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, pname, params); }
}

void glDepthFunc(GLenum func) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glDepthFunc"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(func); }
}

void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*)>
      (dlsym(handle, "milko_glCompressedTexSubImage2D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, xoffset, yoffset, width, height, format, imageSize, data); }
}

void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLfloat*) = reinterpret_cast<void(*)(GLenum, GLenum, GLfloat*)>
      (dlsym(handle, "milko_glGetTexParameterfv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, params); }
}

GLenum glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    dlerror(); 
    GLenum (*checkedGL)(GLsync, GLbitfield, GLuint64) = reinterpret_cast<GLenum(*)(GLsync, GLbitfield, GLuint64)>
      (dlsym(handle, "milko_glClientWaitSync"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(sync, flags, timeout); }
}

void glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, GLuint, GLuint, GLuint) = reinterpret_cast<void(*)(GLuint, GLuint, GLuint, GLuint, GLuint)>
      (dlsym(handle, "milko_glVertexAttribI4ui"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, x, y, z, w); }
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    dlerror(); 
    void (*checkedGL)(GLboolean, GLboolean, GLboolean, GLboolean) = reinterpret_cast<void(*)(GLboolean, GLboolean, GLboolean, GLboolean)>
      (dlsym(handle, "milko_glColorMask"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(red, green, blue, alpha); }
}

void glBlendEquation(GLenum mode) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glBlendEquation"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(mode); }
}

GLint glGetUniformLocation(GLuint program, const GLchar *name) {
    dlerror(); 
    GLint (*checkedGL)(GLuint, const GLchar*) = reinterpret_cast<GLint(*)(GLuint, const GLchar*)>
      (dlsym(handle, "milko_glGetUniformLocation"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(program, name); }
}

void glEndTransformFeedback(void) {
    dlerror(); 
    void (*checkedGL)() = reinterpret_cast<void(*)()>
      (dlsym(handle, "milko_glEndTransformFeedback"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(); }
}

void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLfloat*)>
      (dlsym(handle, "milko_glUniform4fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glBeginTransformFeedback(GLenum primitiveMode) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glBeginTransformFeedback"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(primitiveMode); }
}

GLboolean glIsSampler(GLuint sampler) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsSampler"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(sampler); }
}

void glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids) {
    dlerror(); 
    void (*checkedGL)(GLsizei, const GLuint*) = reinterpret_cast<void(*)(GLsizei, const GLuint*)>
      (dlsym(handle, "milko_glDeleteTransformFeedbacks"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(n, ids); }
}

GLenum glCheckFramebufferStatus(GLenum target) {
    dlerror(); 
    GLenum (*checkedGL)(GLenum) = reinterpret_cast<GLenum(*)(GLenum)>
      (dlsym(handle, "milko_glCheckFramebufferStatus"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(target); }
}

void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, const GLchar*) = reinterpret_cast<void(*)(GLuint, GLuint, const GLchar*)>
      (dlsym(handle, "milko_glBindAttribLocation"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, index, name); }
}

void glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix4x2fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint, GLuint) = reinterpret_cast<void(*)(GLenum, GLuint, GLuint)>
      (dlsym(handle, "milko_glBindBufferBase"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, index, buffer); }
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLintptr, GLsizeiptr, const void*) = reinterpret_cast<void(*)(GLenum, GLintptr, GLsizeiptr, const void*)>
      (dlsym(handle, "milko_glBufferSubData"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, offset, size, data); }
}

void glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLint*, GLint*) = reinterpret_cast<void(*)(GLenum, GLenum, GLint*, GLint*)>
      (dlsym(handle, "milko_glGetShaderPrecisionFormat"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(shadertype, precisiontype, range, precision); }
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLsizei, const GLchar *const*, const GLint*) = reinterpret_cast<void(*)(GLuint, GLsizei, const GLchar *const*, const GLint*)>
      (dlsym(handle, "milko_glShaderSource"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(shader, count, string, length); }
}

void glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, GLsizei, GLsizei*, GLchar*) = reinterpret_cast<void(*)(GLuint, GLuint, GLsizei, GLsizei*, GLchar*)>
      (dlsym(handle, "milko_glGetActiveUniformBlockName"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, uniformBlockIndex, bufSize, length, uniformBlockName); }
}

void glReleaseShaderCompiler(void) {
    dlerror(); 
    void (*checkedGL)() = reinterpret_cast<void(*)()>
      (dlsym(handle, "milko_glReleaseShaderCompiler"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(); }
}

void glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values) {
    dlerror(); 
    void (*checkedGL)(GLsync, GLenum, GLsizei, GLsizei*, GLint*) = reinterpret_cast<void(*)(GLsync, GLenum, GLsizei, GLsizei*, GLint*)>
      (dlsym(handle, "milko_glGetSynciv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sync, pname, bufSize, length, values); }
}

void glBindBuffer(GLenum target, GLuint buffer) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLuint) = reinterpret_cast<void(*)(GLenum, GLuint)>
      (dlsym(handle, "milko_glBindBuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, buffer); }
}

void glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, GLboolean, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, GLboolean, const GLfloat*)>
      (dlsym(handle, "milko_glUniformMatrix2x4fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, transpose, value); }
}

void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLsizeiptr, const void*, GLenum) = reinterpret_cast<void(*)(GLenum, GLsizeiptr, const void*, GLenum)>
      (dlsym(handle, "milko_glBufferData"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, size, data, usage); }
}

void glPauseTransformFeedback(void) {
    dlerror(); 
    void (*checkedGL)() = reinterpret_cast<void(*)()>
      (dlsym(handle, "milko_glPauseTransformFeedback"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(); }
}

GLenum glGetError(void) {
    dlerror(); 
    GLenum (*checkedGL)() = reinterpret_cast<GLenum(*)()>
      (dlsym(handle, "milko_glGetError"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(); }
}

void glVertexAttrib2fv(GLuint index, const GLfloat *v) {
    dlerror(); 
    void (*checkedGL)(GLuint, const GLfloat*) = reinterpret_cast<void(*)(GLuint, const GLfloat*)>
      (dlsym(handle, "milko_glVertexAttrib2fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, v); }
}

void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLenum, GLint*) = reinterpret_cast<void(*)(GLenum, GLenum, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetFramebufferAttachmentParameteriv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, attachment, pname, params); }
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = reinterpret_cast<void(*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)>
      (dlsym(handle, "milko_glTexImage2D"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, level, internalformat, width, height, border, format, type, pixels); }
}

void glStencilMask(GLuint mask) {
    dlerror(); 
    void (*checkedGL)(GLuint) = reinterpret_cast<void(*)(GLuint)>
      (dlsym(handle, "milko_glStencilMask"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(mask); }
}

void glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, const GLfloat*) = reinterpret_cast<void(*)(GLuint, GLenum, const GLfloat*)>
      (dlsym(handle, "milko_glSamplerParameterfv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sampler, pname, param); }
}

GLboolean glIsTexture(GLuint texture) {
    dlerror(); 
    GLboolean (*checkedGL)(GLuint) = reinterpret_cast<GLboolean(*)(GLuint)>
      (dlsym(handle, "milko_glIsTexture"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(texture); }
}

void glUniform1fv(GLint location, GLsizei count, const GLfloat *value) {
    dlerror(); 
    void (*checkedGL)(GLint, GLsizei, const GLfloat*) = reinterpret_cast<void(*)(GLint, GLsizei, const GLfloat*)>
      (dlsym(handle, "milko_glUniform1fv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(location, count, value); }
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, const GLfloat*) = reinterpret_cast<void(*)(GLenum, GLenum, const GLfloat*)>
      (dlsym(handle, "milko_glTexParameterfv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, pname, params); }
}

void glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLenum, GLint*) = reinterpret_cast<void(*)(GLuint, GLenum, GLint*)>
      (dlsym(handle, "milko_glGetSamplerParameteriv"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(sampler, pname, params); }
}

void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr) = reinterpret_cast<void(*)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr)>
      (dlsym(handle, "milko_glCopyBufferSubData"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(readTarget, writeTarget, readOffset, writeOffset, size); }
}

void glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments) {
    dlerror(); 
    void (*checkedGL)(GLenum, GLsizei, const GLenum*) = reinterpret_cast<void(*)(GLenum, GLsizei, const GLenum*)>
      (dlsym(handle, "milko_glInvalidateFramebuffer"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(target, numAttachments, attachments); }
}

void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLfloat, GLfloat) = reinterpret_cast<void(*)(GLuint, GLfloat, GLfloat)>
      (dlsym(handle, "milko_glVertexAttrib2f"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(index, x, y); }
}

void glDepthMask(GLboolean flag) {
    dlerror(); 
    void (*checkedGL)(GLboolean) = reinterpret_cast<void(*)(GLboolean)>
      (dlsym(handle, "milko_glDepthMask"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(flag); }
}

GLuint glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName) {
    dlerror(); 
    GLuint (*checkedGL)(GLuint, const GLchar*) = reinterpret_cast<GLuint(*)(GLuint, const GLchar*)>
      (dlsym(handle, "milko_glGetUniformBlockIndex"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else {return checkedGL(program, uniformBlockName); }
}

void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    dlerror(); 
    void (*checkedGL)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*) = reinterpret_cast<void(*)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*)>
      (dlsym(handle, "milko_glGetActiveUniform"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(program, index, bufSize, length, size, type, name); }
}

void glFrontFace(GLenum mode) {
    dlerror(); 
    void (*checkedGL)(GLenum) = reinterpret_cast<void(*)(GLenum)>
      (dlsym(handle, "milko_glFrontFace"));
    if (!checkedGL) { LOGERR("%s: %s", __func__, dlerror()); exit(0); } 
    else { checkedGL(mode); }
}

#if defined(__aarch64__)
#include <fcntl.h>

typedef __u64 (*func_proc)(__u64, __u64, __u64, __u64, __u64, __u64, __u64,
			      __u64, __u64, __u64, __u64, __u64, __u64, __u64, __u64);

func_proc gl_funcs[897] {
	reinterpret_cast<func_proc>(&glActiveShaderProgram),
	reinterpret_cast<func_proc>(&glActiveShaderProgramEXT),
	reinterpret_cast<func_proc>(&glActiveTexture),
	0,
	reinterpret_cast<func_proc>(&glAlphaFuncQCOM),
	0,
	0,
	reinterpret_cast<func_proc>(&glApplyFramebufferAttachmentCMAAINTEL),
	reinterpret_cast<func_proc>(&glAttachShader),
	reinterpret_cast<func_proc>(&glBeginConditionalRenderNV),
	reinterpret_cast<func_proc>(&glBeginPerfMonitorAMD),
	reinterpret_cast<func_proc>(&glBeginPerfQueryINTEL),
	reinterpret_cast<func_proc>(&glBeginQuery),
	reinterpret_cast<func_proc>(&glBeginQueryEXT),
	reinterpret_cast<func_proc>(&glBeginTransformFeedback),
	reinterpret_cast<func_proc>(&glBindAttribLocation),
	reinterpret_cast<func_proc>(&glBindBuffer),
	reinterpret_cast<func_proc>(&glBindBufferBase),
	reinterpret_cast<func_proc>(&glBindBufferRange),
	reinterpret_cast<func_proc>(&glBindFragDataLocationEXT),
	reinterpret_cast<func_proc>(&glBindFragDataLocationIndexedEXT),
	reinterpret_cast<func_proc>(&glBindFramebuffer),
	0,
	reinterpret_cast<func_proc>(&glBindImageTexture),
	reinterpret_cast<func_proc>(&glBindProgramPipeline),
	reinterpret_cast<func_proc>(&glBindProgramPipelineEXT),
	reinterpret_cast<func_proc>(&glBindRenderbuffer),
	0,
	reinterpret_cast<func_proc>(&glBindSampler),
	reinterpret_cast<func_proc>(&glBindTexture),
	reinterpret_cast<func_proc>(&glBindTransformFeedback),
	reinterpret_cast<func_proc>(&glBindVertexArray),
	reinterpret_cast<func_proc>(&glBindVertexArrayOES),
	reinterpret_cast<func_proc>(&glBindVertexBuffer),
	reinterpret_cast<func_proc>(&glBlendBarrier),
	reinterpret_cast<func_proc>(&glBlendBarrierKHR),
	reinterpret_cast<func_proc>(&glBlendBarrierNV),
	reinterpret_cast<func_proc>(&glBlendColor),
	reinterpret_cast<func_proc>(&glBlendEquation),
	0,
	reinterpret_cast<func_proc>(&glBlendEquationSeparate),
	0,
	reinterpret_cast<func_proc>(&glBlendEquationSeparatei),
	reinterpret_cast<func_proc>(&glBlendEquationSeparateiEXT),
	reinterpret_cast<func_proc>(&glBlendEquationSeparateiOES),
	reinterpret_cast<func_proc>(&glBlendEquationi),
	reinterpret_cast<func_proc>(&glBlendEquationiEXT),
	reinterpret_cast<func_proc>(&glBlendEquationiOES),
	reinterpret_cast<func_proc>(&glBlendFunc),
	reinterpret_cast<func_proc>(&glBlendFuncSeparate),
	0,
	reinterpret_cast<func_proc>(&glBlendFuncSeparatei),
	reinterpret_cast<func_proc>(&glBlendFuncSeparateiEXT),
	reinterpret_cast<func_proc>(&glBlendFuncSeparateiOES),
	reinterpret_cast<func_proc>(&glBlendFunci),
	reinterpret_cast<func_proc>(&glBlendFunciEXT),
	reinterpret_cast<func_proc>(&glBlendFunciOES),
	reinterpret_cast<func_proc>(&glBlendParameteriNV),
	reinterpret_cast<func_proc>(&glBlitFramebuffer),
	reinterpret_cast<func_proc>(&glBlitFramebufferANGLE),
	reinterpret_cast<func_proc>(&glBlitFramebufferNV),
	reinterpret_cast<func_proc>(&glBufferData),
	reinterpret_cast<func_proc>(&glBufferStorageEXT),
	reinterpret_cast<func_proc>(&glBufferSubData),
	reinterpret_cast<func_proc>(&glCheckFramebufferStatus),
	0,
	reinterpret_cast<func_proc>(&glClear),
	reinterpret_cast<func_proc>(&glClearBufferfi),
	reinterpret_cast<func_proc>(&glClearBufferfv),
	reinterpret_cast<func_proc>(&glClearBufferiv),
	reinterpret_cast<func_proc>(&glClearBufferuiv),
	reinterpret_cast<func_proc>(&glClearColor),
	0,
	0,
	reinterpret_cast<func_proc>(&glClearDepthf),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glClearStencil),
	0,
	reinterpret_cast<func_proc>(&glClientWaitSync),
	reinterpret_cast<func_proc>(&glClientWaitSyncAPPLE),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glColorMask),
	reinterpret_cast<func_proc>(&glColorMaski),
	reinterpret_cast<func_proc>(&glColorMaskiEXT),
	reinterpret_cast<func_proc>(&glColorMaskiOES),
	0,
	reinterpret_cast<func_proc>(&glCompileShader),
	reinterpret_cast<func_proc>(&glCompressedTexImage2D),
	reinterpret_cast<func_proc>(&glCompressedTexImage3D),
	reinterpret_cast<func_proc>(&glCompressedTexImage3DOES),
	reinterpret_cast<func_proc>(&glCompressedTexSubImage2D),
	reinterpret_cast<func_proc>(&glCompressedTexSubImage3D),
	reinterpret_cast<func_proc>(&glCompressedTexSubImage3DOES),
	reinterpret_cast<func_proc>(&glCopyBufferSubData),
	reinterpret_cast<func_proc>(&glCopyBufferSubDataNV),
	reinterpret_cast<func_proc>(&glCopyImageSubData),
	reinterpret_cast<func_proc>(&glCopyImageSubDataEXT),
	reinterpret_cast<func_proc>(&glCopyImageSubDataOES),
	reinterpret_cast<func_proc>(&glCopyPathNV),
	reinterpret_cast<func_proc>(&glCopyTexImage2D),
	reinterpret_cast<func_proc>(&glCopyTexSubImage2D),
	reinterpret_cast<func_proc>(&glCopyTexSubImage3D),
	reinterpret_cast<func_proc>(&glCopyTexSubImage3DOES),
	reinterpret_cast<func_proc>(&glCopyTextureLevelsAPPLE),
	reinterpret_cast<func_proc>(&glCoverFillPathInstancedNV),
	reinterpret_cast<func_proc>(&glCoverFillPathNV),
	reinterpret_cast<func_proc>(&glCoverStrokePathInstancedNV),
	reinterpret_cast<func_proc>(&glCoverStrokePathNV),
	reinterpret_cast<func_proc>(&glCoverageMaskNV),
	reinterpret_cast<func_proc>(&glCoverageModulationNV),
	reinterpret_cast<func_proc>(&glCoverageModulationTableNV),
	reinterpret_cast<func_proc>(&glCoverageOperationNV),
	reinterpret_cast<func_proc>(&glCreatePerfQueryINTEL),
	reinterpret_cast<func_proc>(&glCreateProgram),
	reinterpret_cast<func_proc>(&glCreateShader),
	reinterpret_cast<func_proc>(&glCreateShaderProgramv),
	reinterpret_cast<func_proc>(&glCreateShaderProgramvEXT),
	reinterpret_cast<func_proc>(&glCullFace),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glDebugMessageControl),
	reinterpret_cast<func_proc>(&glDebugMessageControlKHR),
	reinterpret_cast<func_proc>(&glDebugMessageInsert),
	reinterpret_cast<func_proc>(&glDebugMessageInsertKHR),
	reinterpret_cast<func_proc>(&glDeleteBuffers),
	reinterpret_cast<func_proc>(&glDeleteFencesNV),
	reinterpret_cast<func_proc>(&glDeleteFramebuffers),
	0,
	reinterpret_cast<func_proc>(&glDeletePathsNV),
	reinterpret_cast<func_proc>(&glDeletePerfMonitorsAMD),
	reinterpret_cast<func_proc>(&glDeletePerfQueryINTEL),
	reinterpret_cast<func_proc>(&glDeleteProgram),
	reinterpret_cast<func_proc>(&glDeleteProgramPipelines),
	reinterpret_cast<func_proc>(&glDeleteProgramPipelinesEXT),
	reinterpret_cast<func_proc>(&glDeleteQueries),
	reinterpret_cast<func_proc>(&glDeleteQueriesEXT),
	reinterpret_cast<func_proc>(&glDeleteRenderbuffers),
	0,
	reinterpret_cast<func_proc>(&glDeleteSamplers),
	reinterpret_cast<func_proc>(&glDeleteShader),
	reinterpret_cast<func_proc>(&glDeleteSync),
	reinterpret_cast<func_proc>(&glDeleteSyncAPPLE),
	reinterpret_cast<func_proc>(&glDeleteTextures),
	reinterpret_cast<func_proc>(&glDeleteTransformFeedbacks),
	reinterpret_cast<func_proc>(&glDeleteVertexArrays),
	reinterpret_cast<func_proc>(&glDeleteVertexArraysOES),
	reinterpret_cast<func_proc>(&glDepthFunc),
	reinterpret_cast<func_proc>(&glDepthMask),
	reinterpret_cast<func_proc>(&glDepthRangeArrayfvNV),
	reinterpret_cast<func_proc>(&glDepthRangeIndexedfNV),
	reinterpret_cast<func_proc>(&glDepthRangef),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glDetachShader),
	reinterpret_cast<func_proc>(&glDisable),
	0,
	reinterpret_cast<func_proc>(&glDisableDriverControlQCOM),
	reinterpret_cast<func_proc>(&glDisableVertexAttribArray),
	reinterpret_cast<func_proc>(&glDisablei),
	reinterpret_cast<func_proc>(&glDisableiEXT),
	reinterpret_cast<func_proc>(&glDisableiNV),
	reinterpret_cast<func_proc>(&glDisableiOES),
	reinterpret_cast<func_proc>(&glDiscardFramebufferEXT),
	reinterpret_cast<func_proc>(&glDispatchCompute),
	reinterpret_cast<func_proc>(&glDispatchComputeIndirect),
	reinterpret_cast<func_proc>(&glDrawArrays),
	reinterpret_cast<func_proc>(&glDrawArraysIndirect),
	reinterpret_cast<func_proc>(&glDrawArraysInstanced),
	reinterpret_cast<func_proc>(&glDrawArraysInstancedANGLE),
	reinterpret_cast<func_proc>(&glDrawArraysInstancedBaseInstanceEXT),
	reinterpret_cast<func_proc>(&glDrawArraysInstancedEXT),
	reinterpret_cast<func_proc>(&glDrawArraysInstancedNV),
	reinterpret_cast<func_proc>(&glDrawBuffers),
	reinterpret_cast<func_proc>(&glDrawBuffersEXT),
	reinterpret_cast<func_proc>(&glDrawBuffersIndexedEXT),
	reinterpret_cast<func_proc>(&glDrawBuffersNV),
	reinterpret_cast<func_proc>(&glDrawElements),
	reinterpret_cast<func_proc>(&glDrawElementsBaseVertex),
	reinterpret_cast<func_proc>(&glDrawElementsBaseVertexEXT),
	reinterpret_cast<func_proc>(&glDrawElementsBaseVertexOES),
	reinterpret_cast<func_proc>(&glDrawElementsIndirect),
	reinterpret_cast<func_proc>(&glDrawElementsInstanced),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedANGLE),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedBaseInstanceEXT),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedBaseVertex),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedBaseVertexBaseInstanceEXT),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedBaseVertexEXT),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedBaseVertexOES),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedEXT),
	reinterpret_cast<func_proc>(&glDrawElementsInstancedNV),
	reinterpret_cast<func_proc>(&glDrawRangeElements),
	reinterpret_cast<func_proc>(&glDrawRangeElementsBaseVertex),
	reinterpret_cast<func_proc>(&glDrawRangeElementsBaseVertexEXT),
	reinterpret_cast<func_proc>(&glDrawRangeElementsBaseVertexOES),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glEGLImageTargetRenderbufferStorageOES),
	reinterpret_cast<func_proc>(&glEGLImageTargetTexture2DOES),
	reinterpret_cast<func_proc>(&glEnable),
	0,
	reinterpret_cast<func_proc>(&glEnableDriverControlQCOM),
	reinterpret_cast<func_proc>(&glEnableVertexAttribArray),
	reinterpret_cast<func_proc>(&glEnablei),
	reinterpret_cast<func_proc>(&glEnableiEXT),
	reinterpret_cast<func_proc>(&glEnableiNV),
	reinterpret_cast<func_proc>(&glEnableiOES),
	reinterpret_cast<func_proc>(&glEndConditionalRenderNV),
	reinterpret_cast<func_proc>(&glEndPerfMonitorAMD),
	reinterpret_cast<func_proc>(&glEndPerfQueryINTEL),
	reinterpret_cast<func_proc>(&glEndQuery),
	reinterpret_cast<func_proc>(&glEndQueryEXT),
	reinterpret_cast<func_proc>(&glEndTilingQCOM),
	reinterpret_cast<func_proc>(&glEndTransformFeedback),
	reinterpret_cast<func_proc>(&glExtGetBufferPointervQCOM),
	reinterpret_cast<func_proc>(&glExtGetBuffersQCOM),
	reinterpret_cast<func_proc>(&glExtGetFramebuffersQCOM),
	reinterpret_cast<func_proc>(&glExtGetProgramBinarySourceQCOM),
	reinterpret_cast<func_proc>(&glExtGetProgramsQCOM),
	reinterpret_cast<func_proc>(&glExtGetRenderbuffersQCOM),
	reinterpret_cast<func_proc>(&glExtGetShadersQCOM),
	reinterpret_cast<func_proc>(&glExtGetTexLevelParameterivQCOM),
	reinterpret_cast<func_proc>(&glExtGetTexSubImageQCOM),
	reinterpret_cast<func_proc>(&glExtGetTexturesQCOM),
	reinterpret_cast<func_proc>(&glExtIsProgramBinaryQCOM),
	reinterpret_cast<func_proc>(&glExtTexObjectStateOverrideiQCOM),
	reinterpret_cast<func_proc>(&glFenceSync),
	reinterpret_cast<func_proc>(&glFenceSyncAPPLE),
	reinterpret_cast<func_proc>(&glFinish),
	reinterpret_cast<func_proc>(&glFinishFenceNV),
	reinterpret_cast<func_proc>(&glFlush),
	reinterpret_cast<func_proc>(&glFlushMappedBufferRange),
	reinterpret_cast<func_proc>(&glFlushMappedBufferRangeEXT),
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glFragmentCoverageColorNV),
	reinterpret_cast<func_proc>(&glFramebufferParameteri),
	reinterpret_cast<func_proc>(&glFramebufferRenderbuffer),
	0,
	reinterpret_cast<func_proc>(&glFramebufferSampleLocationsfvNV),
	reinterpret_cast<func_proc>(&glFramebufferTexture),
	reinterpret_cast<func_proc>(&glFramebufferTexture2D),
	reinterpret_cast<func_proc>(&glFramebufferTexture2DMultisampleEXT),
	reinterpret_cast<func_proc>(&glFramebufferTexture2DMultisampleIMG),
	0,
	reinterpret_cast<func_proc>(&glFramebufferTexture3DOES),
	reinterpret_cast<func_proc>(&glFramebufferTextureEXT),
	reinterpret_cast<func_proc>(&glFramebufferTextureLayer),
	reinterpret_cast<func_proc>(&glFramebufferTextureMultisampleMultiviewOVR),
	reinterpret_cast<func_proc>(&glFramebufferTextureMultiviewOVR),
	reinterpret_cast<func_proc>(&glFramebufferTextureOES),
	reinterpret_cast<func_proc>(&glFrontFace),
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glGenBuffers),
	reinterpret_cast<func_proc>(&glGenFencesNV),
	reinterpret_cast<func_proc>(&glGenFramebuffers),
	0,
	reinterpret_cast<func_proc>(&glGenPathsNV),
	reinterpret_cast<func_proc>(&glGenPerfMonitorsAMD),
	reinterpret_cast<func_proc>(&glGenProgramPipelines),
	reinterpret_cast<func_proc>(&glGenProgramPipelinesEXT),
	reinterpret_cast<func_proc>(&glGenQueries),
	reinterpret_cast<func_proc>(&glGenQueriesEXT),
	reinterpret_cast<func_proc>(&glGenRenderbuffers),
	0,
	reinterpret_cast<func_proc>(&glGenSamplers),
	reinterpret_cast<func_proc>(&glGenTextures),
	reinterpret_cast<func_proc>(&glGenTransformFeedbacks),
	reinterpret_cast<func_proc>(&glGenVertexArrays),
	reinterpret_cast<func_proc>(&glGenVertexArraysOES),
	reinterpret_cast<func_proc>(&glGenerateMipmap),
	0,
	reinterpret_cast<func_proc>(&glGetActiveAttrib),
	reinterpret_cast<func_proc>(&glGetActiveUniform),
	reinterpret_cast<func_proc>(&glGetActiveUniformBlockName),
	reinterpret_cast<func_proc>(&glGetActiveUniformBlockiv),
	reinterpret_cast<func_proc>(&glGetActiveUniformsiv),
	reinterpret_cast<func_proc>(&glGetAttachedShaders),
	reinterpret_cast<func_proc>(&glGetAttribLocation),
	reinterpret_cast<func_proc>(&glGetBooleani_v),
	reinterpret_cast<func_proc>(&glGetBooleanv),
	reinterpret_cast<func_proc>(&glGetBufferParameteri64v),
	reinterpret_cast<func_proc>(&glGetBufferParameteriv),
	reinterpret_cast<func_proc>(&glGetBufferPointerv),
	reinterpret_cast<func_proc>(&glGetBufferPointervOES),
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glGetCoverageModulationTableNV),
	reinterpret_cast<func_proc>(&glGetDebugMessageLog),
	reinterpret_cast<func_proc>(&glGetDebugMessageLogKHR),
	reinterpret_cast<func_proc>(&glGetDriverControlStringQCOM),
	reinterpret_cast<func_proc>(&glGetDriverControlsQCOM),
	reinterpret_cast<func_proc>(&glGetError),
	reinterpret_cast<func_proc>(&glGetFenceivNV),
	reinterpret_cast<func_proc>(&glGetFirstPerfQueryIdINTEL),
	0,
	0,
	reinterpret_cast<func_proc>(&glGetFloati_vNV),
	reinterpret_cast<func_proc>(&glGetFloatv),
	reinterpret_cast<func_proc>(&glGetFragDataIndexEXT),
	reinterpret_cast<func_proc>(&glGetFragDataLocation),
	reinterpret_cast<func_proc>(&glGetFramebufferAttachmentParameteriv),
	0,
	reinterpret_cast<func_proc>(&glGetFramebufferParameteriv),
	reinterpret_cast<func_proc>(&glGetGraphicsResetStatus),
	reinterpret_cast<func_proc>(&glGetGraphicsResetStatusEXT),
	reinterpret_cast<func_proc>(&glGetGraphicsResetStatusKHR),
	reinterpret_cast<func_proc>(&glGetImageHandleNV),
	reinterpret_cast<func_proc>(&glGetInteger64i_v),
	reinterpret_cast<func_proc>(&glGetInteger64v),
	reinterpret_cast<func_proc>(&glGetInteger64vAPPLE),
	reinterpret_cast<func_proc>(&glGetIntegeri_v),
	reinterpret_cast<func_proc>(&glGetIntegeri_vEXT),
	reinterpret_cast<func_proc>(&glGetIntegerv),
	reinterpret_cast<func_proc>(&glGetInternalformatSampleivNV),
	reinterpret_cast<func_proc>(&glGetInternalformativ),
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glGetMultisamplefv),
	reinterpret_cast<func_proc>(&glGetNextPerfQueryIdINTEL),
	reinterpret_cast<func_proc>(&glGetObjectLabel),
	reinterpret_cast<func_proc>(&glGetObjectLabelEXT),
	reinterpret_cast<func_proc>(&glGetObjectLabelKHR),
	reinterpret_cast<func_proc>(&glGetObjectPtrLabel),
	reinterpret_cast<func_proc>(&glGetObjectPtrLabelKHR),
	reinterpret_cast<func_proc>(&glGetPathCommandsNV),
	reinterpret_cast<func_proc>(&glGetPathCoordsNV),
	reinterpret_cast<func_proc>(&glGetPathDashArrayNV),
	reinterpret_cast<func_proc>(&glGetPathLengthNV),
	reinterpret_cast<func_proc>(&glGetPathMetricRangeNV),
	reinterpret_cast<func_proc>(&glGetPathMetricsNV),
	reinterpret_cast<func_proc>(&glGetPathParameterfvNV),
	reinterpret_cast<func_proc>(&glGetPathParameterivNV),
	reinterpret_cast<func_proc>(&glGetPathSpacingNV),
	reinterpret_cast<func_proc>(&glGetPerfCounterInfoINTEL),
	reinterpret_cast<func_proc>(&glGetPerfMonitorCounterDataAMD),
	reinterpret_cast<func_proc>(&glGetPerfMonitorCounterInfoAMD),
	reinterpret_cast<func_proc>(&glGetPerfMonitorCounterStringAMD),
	reinterpret_cast<func_proc>(&glGetPerfMonitorCountersAMD),
	reinterpret_cast<func_proc>(&glGetPerfMonitorGroupStringAMD),
	reinterpret_cast<func_proc>(&glGetPerfMonitorGroupsAMD),
	reinterpret_cast<func_proc>(&glGetPerfQueryDataINTEL),
	reinterpret_cast<func_proc>(&glGetPerfQueryIdByNameINTEL),
	reinterpret_cast<func_proc>(&glGetPerfQueryInfoINTEL),
	reinterpret_cast<func_proc>(&glGetPointerv),
	reinterpret_cast<func_proc>(&glGetPointervKHR),
	reinterpret_cast<func_proc>(&glGetProgramBinary),
	reinterpret_cast<func_proc>(&glGetProgramBinaryOES),
	reinterpret_cast<func_proc>(&glGetProgramInfoLog),
	reinterpret_cast<func_proc>(&glGetProgramInterfaceiv),
	reinterpret_cast<func_proc>(&glGetProgramPipelineInfoLog),
	reinterpret_cast<func_proc>(&glGetProgramPipelineInfoLogEXT),
	reinterpret_cast<func_proc>(&glGetProgramPipelineiv),
	reinterpret_cast<func_proc>(&glGetProgramPipelineivEXT),
	reinterpret_cast<func_proc>(&glGetProgramResourceIndex),
	reinterpret_cast<func_proc>(&glGetProgramResourceLocation),
	reinterpret_cast<func_proc>(&glGetProgramResourceLocationIndexEXT),
	reinterpret_cast<func_proc>(&glGetProgramResourceName),
	reinterpret_cast<func_proc>(&glGetProgramResourcefvNV),
	reinterpret_cast<func_proc>(&glGetProgramResourceiv),
	reinterpret_cast<func_proc>(&glGetProgramiv),
	reinterpret_cast<func_proc>(&glGetQueryObjecti64vEXT),
	reinterpret_cast<func_proc>(&glGetQueryObjectivEXT),
	reinterpret_cast<func_proc>(&glGetQueryObjectui64vEXT),
	reinterpret_cast<func_proc>(&glGetQueryObjectuiv),
	reinterpret_cast<func_proc>(&glGetQueryObjectuivEXT),
	reinterpret_cast<func_proc>(&glGetQueryiv),
	reinterpret_cast<func_proc>(&glGetQueryivEXT),
	reinterpret_cast<func_proc>(&glGetRenderbufferParameteriv),
	0,
	reinterpret_cast<func_proc>(&glGetSamplerParameterIiv),
	reinterpret_cast<func_proc>(&glGetSamplerParameterIivEXT),
	reinterpret_cast<func_proc>(&glGetSamplerParameterIivOES),
	reinterpret_cast<func_proc>(&glGetSamplerParameterIuiv),
	reinterpret_cast<func_proc>(&glGetSamplerParameterIuivEXT),
	reinterpret_cast<func_proc>(&glGetSamplerParameterIuivOES),
	reinterpret_cast<func_proc>(&glGetSamplerParameterfv),
	reinterpret_cast<func_proc>(&glGetSamplerParameteriv),
	reinterpret_cast<func_proc>(&glGetShaderInfoLog),
	reinterpret_cast<func_proc>(&glGetShaderPrecisionFormat),
	reinterpret_cast<func_proc>(&glGetShaderSource),
	reinterpret_cast<func_proc>(&glGetShaderiv),
	reinterpret_cast<func_proc>(&glGetString),
	reinterpret_cast<func_proc>(&glGetStringi),
	reinterpret_cast<func_proc>(&glGetSynciv),
	reinterpret_cast<func_proc>(&glGetSyncivAPPLE),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glGetTexLevelParameterfv),
	reinterpret_cast<func_proc>(&glGetTexLevelParameteriv),
	reinterpret_cast<func_proc>(&glGetTexParameterIiv),
	reinterpret_cast<func_proc>(&glGetTexParameterIivEXT),
	reinterpret_cast<func_proc>(&glGetTexParameterIivOES),
	reinterpret_cast<func_proc>(&glGetTexParameterIuiv),
	reinterpret_cast<func_proc>(&glGetTexParameterIuivEXT),
	reinterpret_cast<func_proc>(&glGetTexParameterIuivOES),
	reinterpret_cast<func_proc>(&glGetTexParameterfv),
	reinterpret_cast<func_proc>(&glGetTexParameteriv),
	0,
	0,
	reinterpret_cast<func_proc>(&glGetTextureHandleNV),
	reinterpret_cast<func_proc>(&glGetTextureSamplerHandleNV),
	reinterpret_cast<func_proc>(&glGetTransformFeedbackVarying),
	reinterpret_cast<func_proc>(&glGetTranslatedShaderSourceANGLE),
	reinterpret_cast<func_proc>(&glGetUniformBlockIndex),
	reinterpret_cast<func_proc>(&glGetUniformIndices),
	reinterpret_cast<func_proc>(&glGetUniformLocation),
	reinterpret_cast<func_proc>(&glGetUniformfv),
	reinterpret_cast<func_proc>(&glGetUniformiv),
	reinterpret_cast<func_proc>(&glGetUniformuiv),
	reinterpret_cast<func_proc>(&glGetVertexAttribIiv),
	reinterpret_cast<func_proc>(&glGetVertexAttribIuiv),
	reinterpret_cast<func_proc>(&glGetVertexAttribPointerv),
	reinterpret_cast<func_proc>(&glGetVertexAttribfv),
	reinterpret_cast<func_proc>(&glGetVertexAttribiv),
	reinterpret_cast<func_proc>(&glGetnUniformfv),
	reinterpret_cast<func_proc>(&glGetnUniformfvEXT),
	reinterpret_cast<func_proc>(&glGetnUniformfvKHR),
	reinterpret_cast<func_proc>(&glGetnUniformiv),
	reinterpret_cast<func_proc>(&glGetnUniformivEXT),
	reinterpret_cast<func_proc>(&glGetnUniformivKHR),
	reinterpret_cast<func_proc>(&glGetnUniformuiv),
	reinterpret_cast<func_proc>(&glGetnUniformuivKHR),
	reinterpret_cast<func_proc>(&glHint),
	reinterpret_cast<func_proc>(&glInsertEventMarkerEXT),
	reinterpret_cast<func_proc>(&glInterpolatePathsNV),
	reinterpret_cast<func_proc>(&glInvalidateFramebuffer),
	reinterpret_cast<func_proc>(&glInvalidateSubFramebuffer),
	reinterpret_cast<func_proc>(&glIsBuffer),
	reinterpret_cast<func_proc>(&glIsEnabled),
	reinterpret_cast<func_proc>(&glIsEnabledi),
	reinterpret_cast<func_proc>(&glIsEnablediEXT),
	reinterpret_cast<func_proc>(&glIsEnablediNV),
	reinterpret_cast<func_proc>(&glIsEnablediOES),
	reinterpret_cast<func_proc>(&glIsFenceNV),
	reinterpret_cast<func_proc>(&glIsFramebuffer),
	0,
	reinterpret_cast<func_proc>(&glIsImageHandleResidentNV),
	reinterpret_cast<func_proc>(&glIsPathNV),
	reinterpret_cast<func_proc>(&glIsPointInFillPathNV),
	reinterpret_cast<func_proc>(&glIsPointInStrokePathNV),
	reinterpret_cast<func_proc>(&glIsProgram),
	reinterpret_cast<func_proc>(&glIsProgramPipeline),
	reinterpret_cast<func_proc>(&glIsProgramPipelineEXT),
	reinterpret_cast<func_proc>(&glIsQuery),
	reinterpret_cast<func_proc>(&glIsQueryEXT),
	reinterpret_cast<func_proc>(&glIsRenderbuffer),
	0,
	reinterpret_cast<func_proc>(&glIsSampler),
	reinterpret_cast<func_proc>(&glIsShader),
	reinterpret_cast<func_proc>(&glIsSync),
	reinterpret_cast<func_proc>(&glIsSyncAPPLE),
	reinterpret_cast<func_proc>(&glIsTexture),
	reinterpret_cast<func_proc>(&glIsTextureHandleResidentNV),
	reinterpret_cast<func_proc>(&glIsTransformFeedback),
	reinterpret_cast<func_proc>(&glIsVertexArray),
	reinterpret_cast<func_proc>(&glIsVertexArrayOES),
	reinterpret_cast<func_proc>(&glLabelObjectEXT),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glLineWidth),
	0,
	0,
	reinterpret_cast<func_proc>(&glLinkProgram),
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glMakeImageHandleNonResidentNV),
	reinterpret_cast<func_proc>(&glMakeImageHandleResidentNV),
	reinterpret_cast<func_proc>(&glMakeTextureHandleNonResidentNV),
	reinterpret_cast<func_proc>(&glMakeTextureHandleResidentNV),
	reinterpret_cast<func_proc>(&glMapBufferOES),
	reinterpret_cast<func_proc>(&glMapBufferRange),
	reinterpret_cast<func_proc>(&glMapBufferRangeEXT),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glMatrixLoad3x2fNV),
	reinterpret_cast<func_proc>(&glMatrixLoad3x3fNV),
	reinterpret_cast<func_proc>(&glMatrixLoadTranspose3x3fNV),
	0,
	reinterpret_cast<func_proc>(&glMatrixMult3x2fNV),
	reinterpret_cast<func_proc>(&glMatrixMult3x3fNV),
	reinterpret_cast<func_proc>(&glMatrixMultTranspose3x3fNV),
	reinterpret_cast<func_proc>(&glMemoryBarrier),
	reinterpret_cast<func_proc>(&glMemoryBarrierByRegion),
	reinterpret_cast<func_proc>(&glMinSampleShading),
	reinterpret_cast<func_proc>(&glMinSampleShadingOES),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glMultiDrawArraysEXT),
	reinterpret_cast<func_proc>(&glMultiDrawArraysIndirectEXT),
	reinterpret_cast<func_proc>(&glMultiDrawElementsBaseVertexEXT),
	reinterpret_cast<func_proc>(&glMultiDrawElementsBaseVertexOES),
	reinterpret_cast<func_proc>(&glMultiDrawElementsEXT),
	reinterpret_cast<func_proc>(&glMultiDrawElementsIndirectEXT),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glNamedFramebufferSampleLocationsfvNV),
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glObjectLabel),
	reinterpret_cast<func_proc>(&glObjectLabelKHR),
	reinterpret_cast<func_proc>(&glObjectPtrLabel),
	reinterpret_cast<func_proc>(&glObjectPtrLabelKHR),
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glPatchParameteri),
	reinterpret_cast<func_proc>(&glPatchParameteriEXT),
	reinterpret_cast<func_proc>(&glPatchParameteriOES),
	reinterpret_cast<func_proc>(&glPathCommandsNV),
	reinterpret_cast<func_proc>(&glPathCoordsNV),
	reinterpret_cast<func_proc>(&glPathCoverDepthFuncNV),
	reinterpret_cast<func_proc>(&glPathDashArrayNV),
	reinterpret_cast<func_proc>(&glPathGlyphIndexArrayNV),
	reinterpret_cast<func_proc>(&glPathGlyphIndexRangeNV),
	reinterpret_cast<func_proc>(&glPathGlyphRangeNV),
	reinterpret_cast<func_proc>(&glPathGlyphsNV),
	reinterpret_cast<func_proc>(&glPathMemoryGlyphIndexArrayNV),
	reinterpret_cast<func_proc>(&glPathParameterfNV),
	reinterpret_cast<func_proc>(&glPathParameterfvNV),
	reinterpret_cast<func_proc>(&glPathParameteriNV),
	reinterpret_cast<func_proc>(&glPathParameterivNV),
	reinterpret_cast<func_proc>(&glPathStencilDepthOffsetNV),
	reinterpret_cast<func_proc>(&glPathStencilFuncNV),
	reinterpret_cast<func_proc>(&glPathStringNV),
	reinterpret_cast<func_proc>(&glPathSubCommandsNV),
	reinterpret_cast<func_proc>(&glPathSubCoordsNV),
	reinterpret_cast<func_proc>(&glPauseTransformFeedback),
	reinterpret_cast<func_proc>(&glPixelStorei),
	reinterpret_cast<func_proc>(&glPointAlongPathNV),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glPolygonModeNV),
	reinterpret_cast<func_proc>(&glPolygonOffset),
	0,
	0,
	reinterpret_cast<func_proc>(&glPopDebugGroup),
	reinterpret_cast<func_proc>(&glPopDebugGroupKHR),
	reinterpret_cast<func_proc>(&glPopGroupMarkerEXT),
	0,
	reinterpret_cast<func_proc>(&glPrimitiveBoundingBox),
	reinterpret_cast<func_proc>(&glPrimitiveBoundingBoxEXT),
	reinterpret_cast<func_proc>(&glPrimitiveBoundingBoxOES),
	reinterpret_cast<func_proc>(&glProgramBinary),
	reinterpret_cast<func_proc>(&glProgramBinaryOES),
	reinterpret_cast<func_proc>(&glProgramParameteri),
	reinterpret_cast<func_proc>(&glProgramParameteriEXT),
	reinterpret_cast<func_proc>(&glProgramPathFragmentInputGenNV),
	reinterpret_cast<func_proc>(&glProgramUniform1f),
	reinterpret_cast<func_proc>(&glProgramUniform1fEXT),
	reinterpret_cast<func_proc>(&glProgramUniform1fv),
	reinterpret_cast<func_proc>(&glProgramUniform1fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniform1i),
	reinterpret_cast<func_proc>(&glProgramUniform1iEXT),
	reinterpret_cast<func_proc>(&glProgramUniform1iv),
	reinterpret_cast<func_proc>(&glProgramUniform1ivEXT),
	reinterpret_cast<func_proc>(&glProgramUniform1ui),
	reinterpret_cast<func_proc>(&glProgramUniform1uiEXT),
	reinterpret_cast<func_proc>(&glProgramUniform1uiv),
	reinterpret_cast<func_proc>(&glProgramUniform1uivEXT),
	reinterpret_cast<func_proc>(&glProgramUniform2f),
	reinterpret_cast<func_proc>(&glProgramUniform2fEXT),
	reinterpret_cast<func_proc>(&glProgramUniform2fv),
	reinterpret_cast<func_proc>(&glProgramUniform2fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniform2i),
	reinterpret_cast<func_proc>(&glProgramUniform2iEXT),
	reinterpret_cast<func_proc>(&glProgramUniform2iv),
	reinterpret_cast<func_proc>(&glProgramUniform2ivEXT),
	reinterpret_cast<func_proc>(&glProgramUniform2ui),
	reinterpret_cast<func_proc>(&glProgramUniform2uiEXT),
	reinterpret_cast<func_proc>(&glProgramUniform2uiv),
	reinterpret_cast<func_proc>(&glProgramUniform2uivEXT),
	reinterpret_cast<func_proc>(&glProgramUniform3f),
	reinterpret_cast<func_proc>(&glProgramUniform3fEXT),
	reinterpret_cast<func_proc>(&glProgramUniform3fv),
	reinterpret_cast<func_proc>(&glProgramUniform3fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniform3i),
	reinterpret_cast<func_proc>(&glProgramUniform3iEXT),
	reinterpret_cast<func_proc>(&glProgramUniform3iv),
	reinterpret_cast<func_proc>(&glProgramUniform3ivEXT),
	reinterpret_cast<func_proc>(&glProgramUniform3ui),
	reinterpret_cast<func_proc>(&glProgramUniform3uiEXT),
	reinterpret_cast<func_proc>(&glProgramUniform3uiv),
	reinterpret_cast<func_proc>(&glProgramUniform3uivEXT),
	reinterpret_cast<func_proc>(&glProgramUniform4f),
	reinterpret_cast<func_proc>(&glProgramUniform4fEXT),
	reinterpret_cast<func_proc>(&glProgramUniform4fv),
	reinterpret_cast<func_proc>(&glProgramUniform4fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniform4i),
	reinterpret_cast<func_proc>(&glProgramUniform4iEXT),
	reinterpret_cast<func_proc>(&glProgramUniform4iv),
	reinterpret_cast<func_proc>(&glProgramUniform4ivEXT),
	reinterpret_cast<func_proc>(&glProgramUniform4ui),
	reinterpret_cast<func_proc>(&glProgramUniform4uiEXT),
	reinterpret_cast<func_proc>(&glProgramUniform4uiv),
	reinterpret_cast<func_proc>(&glProgramUniform4uivEXT),
	reinterpret_cast<func_proc>(&glProgramUniformHandleui64NV),
	reinterpret_cast<func_proc>(&glProgramUniformHandleui64vNV),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix2fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix2fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix2x3fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix2x3fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix2x4fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix2x4fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix3fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix3fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix3x2fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix3x2fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix3x4fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix3x4fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix4fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix4fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix4x2fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix4x2fvEXT),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix4x3fv),
	reinterpret_cast<func_proc>(&glProgramUniformMatrix4x3fvEXT),
	reinterpret_cast<func_proc>(&glPushDebugGroup),
	reinterpret_cast<func_proc>(&glPushDebugGroupKHR),
	reinterpret_cast<func_proc>(&glPushGroupMarkerEXT),
	0,
	reinterpret_cast<func_proc>(&glQueryCounterEXT),
	0,
	reinterpret_cast<func_proc>(&glRasterSamplesEXT),
	reinterpret_cast<func_proc>(&glReadBuffer),
	reinterpret_cast<func_proc>(&glReadBufferIndexedEXT),
	reinterpret_cast<func_proc>(&glReadBufferNV),
	reinterpret_cast<func_proc>(&glReadPixels),
	reinterpret_cast<func_proc>(&glReadnPixels),
	reinterpret_cast<func_proc>(&glReadnPixelsEXT),
	reinterpret_cast<func_proc>(&glReadnPixelsKHR),
	reinterpret_cast<func_proc>(&glReleaseShaderCompiler),
	reinterpret_cast<func_proc>(&glRenderbufferStorage),
	reinterpret_cast<func_proc>(&glRenderbufferStorageMultisample),
	reinterpret_cast<func_proc>(&glRenderbufferStorageMultisampleANGLE),
	reinterpret_cast<func_proc>(&glRenderbufferStorageMultisampleAPPLE),
	reinterpret_cast<func_proc>(&glRenderbufferStorageMultisampleEXT),
	reinterpret_cast<func_proc>(&glRenderbufferStorageMultisampleIMG),
	reinterpret_cast<func_proc>(&glRenderbufferStorageMultisampleNV),
	0,
	reinterpret_cast<func_proc>(&glResolveDepthValuesNV),
	reinterpret_cast<func_proc>(&glResolveMultisampleFramebufferAPPLE),
	reinterpret_cast<func_proc>(&glResumeTransformFeedback),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glSampleCoverage),
	0,
	0,
	reinterpret_cast<func_proc>(&glSampleMaski),
	reinterpret_cast<func_proc>(&glSamplerParameterIiv),
	reinterpret_cast<func_proc>(&glSamplerParameterIivEXT),
	reinterpret_cast<func_proc>(&glSamplerParameterIivOES),
	reinterpret_cast<func_proc>(&glSamplerParameterIuiv),
	reinterpret_cast<func_proc>(&glSamplerParameterIuivEXT),
	reinterpret_cast<func_proc>(&glSamplerParameterIuivOES),
	reinterpret_cast<func_proc>(&glSamplerParameterf),
	reinterpret_cast<func_proc>(&glSamplerParameterfv),
	reinterpret_cast<func_proc>(&glSamplerParameteri),
	reinterpret_cast<func_proc>(&glSamplerParameteriv),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glScissor),
	reinterpret_cast<func_proc>(&glScissorArrayvNV),
	reinterpret_cast<func_proc>(&glScissorIndexedNV),
	reinterpret_cast<func_proc>(&glScissorIndexedvNV),
	reinterpret_cast<func_proc>(&glSelectPerfMonitorCountersAMD),
	reinterpret_cast<func_proc>(&glSetFenceNV),
	0,
	reinterpret_cast<func_proc>(&glShaderBinary),
	reinterpret_cast<func_proc>(&glShaderSource),
	reinterpret_cast<func_proc>(&glStartTilingQCOM),
	reinterpret_cast<func_proc>(&glStencilFillPathInstancedNV),
	reinterpret_cast<func_proc>(&glStencilFillPathNV),
	reinterpret_cast<func_proc>(&glStencilFunc),
	reinterpret_cast<func_proc>(&glStencilFuncSeparate),
	reinterpret_cast<func_proc>(&glStencilMask),
	reinterpret_cast<func_proc>(&glStencilMaskSeparate),
	reinterpret_cast<func_proc>(&glStencilOp),
	reinterpret_cast<func_proc>(&glStencilOpSeparate),
	reinterpret_cast<func_proc>(&glStencilStrokePathInstancedNV),
	reinterpret_cast<func_proc>(&glStencilStrokePathNV),
	reinterpret_cast<func_proc>(&glStencilThenCoverFillPathInstancedNV),
	reinterpret_cast<func_proc>(&glStencilThenCoverFillPathNV),
	reinterpret_cast<func_proc>(&glStencilThenCoverStrokePathInstancedNV),
	reinterpret_cast<func_proc>(&glStencilThenCoverStrokePathNV),
	reinterpret_cast<func_proc>(&glSubpixelPrecisionBiasNV),
	reinterpret_cast<func_proc>(&glTestFenceNV),
	reinterpret_cast<func_proc>(&glTexBuffer),
	reinterpret_cast<func_proc>(&glTexBufferEXT),
	reinterpret_cast<func_proc>(&glTexBufferOES),
	reinterpret_cast<func_proc>(&glTexBufferRange),
	reinterpret_cast<func_proc>(&glTexBufferRangeEXT),
	reinterpret_cast<func_proc>(&glTexBufferRangeOES),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glTexImage2D),
	reinterpret_cast<func_proc>(&glTexImage3D),
	reinterpret_cast<func_proc>(&glTexImage3DOES),
	reinterpret_cast<func_proc>(&glTexPageCommitmentEXT),
	reinterpret_cast<func_proc>(&glTexParameterIiv),
	reinterpret_cast<func_proc>(&glTexParameterIivEXT),
	reinterpret_cast<func_proc>(&glTexParameterIivOES),
	reinterpret_cast<func_proc>(&glTexParameterIuiv),
	reinterpret_cast<func_proc>(&glTexParameterIuivEXT),
	reinterpret_cast<func_proc>(&glTexParameterIuivOES),
	reinterpret_cast<func_proc>(&glTexParameterf),
	reinterpret_cast<func_proc>(&glTexParameterfv),
	reinterpret_cast<func_proc>(&glTexParameteri),
	reinterpret_cast<func_proc>(&glTexParameteriv),
	0,
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glTexStorage1DEXT),
	reinterpret_cast<func_proc>(&glTexStorage2D),
	reinterpret_cast<func_proc>(&glTexStorage2DEXT),
	reinterpret_cast<func_proc>(&glTexStorage2DMultisample),
	reinterpret_cast<func_proc>(&glTexStorage3D),
	reinterpret_cast<func_proc>(&glTexStorage3DEXT),
	reinterpret_cast<func_proc>(&glTexStorage3DMultisample),
	reinterpret_cast<func_proc>(&glTexStorage3DMultisampleOES),
	reinterpret_cast<func_proc>(&glTexSubImage2D),
	reinterpret_cast<func_proc>(&glTexSubImage3D),
	reinterpret_cast<func_proc>(&glTexSubImage3DOES),
	reinterpret_cast<func_proc>(&glTextureStorage1DEXT),
	reinterpret_cast<func_proc>(&glTextureStorage2DEXT),
	reinterpret_cast<func_proc>(&glTextureStorage3DEXT),
	reinterpret_cast<func_proc>(&glTextureViewEXT),
	reinterpret_cast<func_proc>(&glTextureViewOES),
	reinterpret_cast<func_proc>(&glTransformFeedbackVaryings),
	reinterpret_cast<func_proc>(&glTransformPathNV),
	0,
	0,
	0,
	reinterpret_cast<func_proc>(&glUniform1f),
	reinterpret_cast<func_proc>(&glUniform1fv),
	reinterpret_cast<func_proc>(&glUniform1i),
	reinterpret_cast<func_proc>(&glUniform1iv),
	reinterpret_cast<func_proc>(&glUniform1ui),
	reinterpret_cast<func_proc>(&glUniform1uiv),
	reinterpret_cast<func_proc>(&glUniform2f),
	reinterpret_cast<func_proc>(&glUniform2fv),
	reinterpret_cast<func_proc>(&glUniform2i),
	reinterpret_cast<func_proc>(&glUniform2iv),
	reinterpret_cast<func_proc>(&glUniform2ui),
	reinterpret_cast<func_proc>(&glUniform2uiv),
	reinterpret_cast<func_proc>(&glUniform3f),
	reinterpret_cast<func_proc>(&glUniform3fv),
	reinterpret_cast<func_proc>(&glUniform3i),
	reinterpret_cast<func_proc>(&glUniform3iv),
	reinterpret_cast<func_proc>(&glUniform3ui),
	reinterpret_cast<func_proc>(&glUniform3uiv),
	reinterpret_cast<func_proc>(&glUniform4f),
	reinterpret_cast<func_proc>(&glUniform4fv),
	reinterpret_cast<func_proc>(&glUniform4i),
	reinterpret_cast<func_proc>(&glUniform4iv),
	reinterpret_cast<func_proc>(&glUniform4ui),
	reinterpret_cast<func_proc>(&glUniform4uiv),
	reinterpret_cast<func_proc>(&glUniformBlockBinding),
	reinterpret_cast<func_proc>(&glUniformHandleui64NV),
	reinterpret_cast<func_proc>(&glUniformHandleui64vNV),
	reinterpret_cast<func_proc>(&glUniformMatrix2fv),
	reinterpret_cast<func_proc>(&glUniformMatrix2x3fv),
	reinterpret_cast<func_proc>(&glUniformMatrix2x3fvNV),
	reinterpret_cast<func_proc>(&glUniformMatrix2x4fv),
	reinterpret_cast<func_proc>(&glUniformMatrix2x4fvNV),
	reinterpret_cast<func_proc>(&glUniformMatrix3fv),
	reinterpret_cast<func_proc>(&glUniformMatrix3x2fv),
	reinterpret_cast<func_proc>(&glUniformMatrix3x2fvNV),
	reinterpret_cast<func_proc>(&glUniformMatrix3x4fv),
	reinterpret_cast<func_proc>(&glUniformMatrix3x4fvNV),
	reinterpret_cast<func_proc>(&glUniformMatrix4fv),
	reinterpret_cast<func_proc>(&glUniformMatrix4x2fv),
	reinterpret_cast<func_proc>(&glUniformMatrix4x2fvNV),
	reinterpret_cast<func_proc>(&glUniformMatrix4x3fv),
	reinterpret_cast<func_proc>(&glUniformMatrix4x3fvNV),
	reinterpret_cast<func_proc>(&glUnmapBuffer),
	reinterpret_cast<func_proc>(&glUnmapBufferOES),
	reinterpret_cast<func_proc>(&glUseProgram),
	reinterpret_cast<func_proc>(&glUseProgramStages),
	reinterpret_cast<func_proc>(&glUseProgramStagesEXT),
	reinterpret_cast<func_proc>(&glValidateProgram),
	reinterpret_cast<func_proc>(&glValidateProgramPipeline),
	reinterpret_cast<func_proc>(&glValidateProgramPipelineEXT),
	reinterpret_cast<func_proc>(&glVertexAttrib1f),
	reinterpret_cast<func_proc>(&glVertexAttrib1fv),
	reinterpret_cast<func_proc>(&glVertexAttrib2f),
	reinterpret_cast<func_proc>(&glVertexAttrib2fv),
	reinterpret_cast<func_proc>(&glVertexAttrib3f),
	reinterpret_cast<func_proc>(&glVertexAttrib3fv),
	reinterpret_cast<func_proc>(&glVertexAttrib4f),
	reinterpret_cast<func_proc>(&glVertexAttrib4fv),
	reinterpret_cast<func_proc>(&glVertexAttribBinding),
	reinterpret_cast<func_proc>(&glVertexAttribDivisor),
	reinterpret_cast<func_proc>(&glVertexAttribDivisorANGLE),
	reinterpret_cast<func_proc>(&glVertexAttribDivisorEXT),
	reinterpret_cast<func_proc>(&glVertexAttribDivisorNV),
	reinterpret_cast<func_proc>(&glVertexAttribFormat),
	reinterpret_cast<func_proc>(&glVertexAttribI4i),
	reinterpret_cast<func_proc>(&glVertexAttribI4iv),
	reinterpret_cast<func_proc>(&glVertexAttribI4ui),
	reinterpret_cast<func_proc>(&glVertexAttribI4uiv),
	reinterpret_cast<func_proc>(&glVertexAttribIFormat),
	reinterpret_cast<func_proc>(&glVertexAttribIPointer),
	reinterpret_cast<func_proc>(&glVertexAttribPointer),
	reinterpret_cast<func_proc>(&glVertexBindingDivisor),
	0,
	reinterpret_cast<func_proc>(&glViewport),
	reinterpret_cast<func_proc>(&glViewportArrayvNV),
	reinterpret_cast<func_proc>(&glViewportIndexedfNV),
	reinterpret_cast<func_proc>(&glViewportIndexedfvNV),
	reinterpret_cast<func_proc>(&glWaitSync),
	reinterpret_cast<func_proc>(&glWaitSyncAPPLE),
	reinterpret_cast<func_proc>(&glWeightPathsNV),
	0,
};

func_proc egl_funcs[66] {
	reinterpret_cast<func_proc>(&eglGetDisplay),
	reinterpret_cast<func_proc>(&eglInitialize),
	reinterpret_cast<func_proc>(&eglTerminate),
	reinterpret_cast<func_proc>(&eglGetConfigs),
	reinterpret_cast<func_proc>(&eglChooseConfig),
	reinterpret_cast<func_proc>(&eglGetConfigAttrib),
	reinterpret_cast<func_proc>(&eglCreateWindowSurface),
	reinterpret_cast<func_proc>(&eglCreatePixmapSurface),
	reinterpret_cast<func_proc>(&eglCreatePbufferSurface),
	reinterpret_cast<func_proc>(&eglDestroySurface),
	reinterpret_cast<func_proc>(&eglQuerySurface),
	reinterpret_cast<func_proc>(&eglCreateContext),
	reinterpret_cast<func_proc>(&eglDestroyContext),
	reinterpret_cast<func_proc>(&eglMakeCurrent),
	reinterpret_cast<func_proc>(&eglGetCurrentContext),
	reinterpret_cast<func_proc>(&eglGetCurrentSurface),
	reinterpret_cast<func_proc>(&eglGetCurrentDisplay),
	reinterpret_cast<func_proc>(&eglQueryContext),
	reinterpret_cast<func_proc>(&eglWaitGL),
	reinterpret_cast<func_proc>(&eglWaitNative),
	reinterpret_cast<func_proc>(&eglSwapBuffers),
	reinterpret_cast<func_proc>(&eglCopyBuffers),
	reinterpret_cast<func_proc>(&eglGetError),
	reinterpret_cast<func_proc>(&eglQueryString),
	reinterpret_cast<func_proc>(&eglGetProcAddress),
	reinterpret_cast<func_proc>(&eglSurfaceAttrib),
	reinterpret_cast<func_proc>(&eglBindTexImage),
	reinterpret_cast<func_proc>(&eglReleaseTexImage),
	reinterpret_cast<func_proc>(&eglSwapInterval),
	reinterpret_cast<func_proc>(&eglBindAPI),
	reinterpret_cast<func_proc>(&eglQueryAPI),
	reinterpret_cast<func_proc>(&eglWaitClient),
	reinterpret_cast<func_proc>(&eglReleaseThread),
	reinterpret_cast<func_proc>(&eglCreatePbufferFromClientBuffer),
	reinterpret_cast<func_proc>(&eglLockSurfaceKHR),
	reinterpret_cast<func_proc>(&eglUnlockSurfaceKHR),
	reinterpret_cast<func_proc>(&eglCreateImageKHR),
	reinterpret_cast<func_proc>(&eglDestroyImageKHR),
	reinterpret_cast<func_proc>(&eglCreateSyncKHR),
	reinterpret_cast<func_proc>(&eglDestroySyncKHR),
	reinterpret_cast<func_proc>(&eglClientWaitSyncKHR),
	reinterpret_cast<func_proc>(&eglSignalSyncKHR),
	reinterpret_cast<func_proc>(&eglGetSyncAttribKHR),
	reinterpret_cast<func_proc>(&eglCreateStreamKHR),
	reinterpret_cast<func_proc>(&eglDestroyStreamKHR),
	reinterpret_cast<func_proc>(&eglStreamAttribKHR),
	reinterpret_cast<func_proc>(&eglQueryStreamKHR),
	reinterpret_cast<func_proc>(&eglQueryStreamu64KHR),
	reinterpret_cast<func_proc>(&eglStreamConsumerGLTextureExternalKHR),
	reinterpret_cast<func_proc>(&eglStreamConsumerAcquireKHR),
	reinterpret_cast<func_proc>(&eglStreamConsumerReleaseKHR),
	reinterpret_cast<func_proc>(&eglCreateStreamProducerSurfaceKHR),
	reinterpret_cast<func_proc>(&eglQueryStreamTimeKHR),
	reinterpret_cast<func_proc>(&eglGetStreamFileDescriptorKHR),
	reinterpret_cast<func_proc>(&eglCreateStreamFromFileDescriptorKHR),
	reinterpret_cast<func_proc>(&eglWaitSyncKHR),
	0,
	0,
	reinterpret_cast<func_proc>(&eglDupNativeFenceFDANDROID),
	reinterpret_cast<func_proc>(&eglCreateNativeClientBufferANDROID),
	reinterpret_cast<func_proc>(&eglGetSystemTimeFrequencyNV),
	reinterpret_cast<func_proc>(&eglGetSystemTimeNV),
	0,
	0,
	reinterpret_cast<func_proc>(&eglSwapBuffersWithDamageKHR),
	reinterpret_cast<func_proc>(&eglSetDamageRegionKHR)
};

extern "C" long handle_gl_call(uint64_t api, uint64_t arg1, uint64_t arg2, uint64_t arg3,
		    uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7,
		    uint64_t arg8, uint64_t arg9, uint64_t arg10, uint64_t arg11,
		    uint64_t arg12, uint64_t arg13, uint64_t arg14, uint64_t arg15)
{
	func_proc func;
	uint64_t ret_val;

	/* Safety checks */ 
	if (api >= 10000 && api <= 10520) { /* 520 = last EGL entry (eglSetDamageRegionKHR) */
		api -= 10000;
		printf("EGL API detected, api = %d\n", (int) api);
		func = egl_funcs[api/8];
	} else if (api <= 7168) { /* 7168 = last GLESv2 entry (glWeightPointerOES) */
		printf("GLESv2 API detected, api = %d\n", (int) api);
		func = gl_funcs[api/8];
	} else {
		fprintf(stderr, "Unsupported GLES2/EGL API\n");
		return -1;
	}

	if (!func) {
		fprintf(stderr, "Unsupported function\n");
		return -1;
	}

	ret_val = (*func)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,
	      		  arg9, arg10, arg11, arg12, arg13, arg14, arg15);

	return (long) ret_val;
}

#endif /* defined(__aarch64__) */
