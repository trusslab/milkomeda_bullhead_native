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

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

#include "../egl_impl.h"

using namespace android;

// ----------------------------------------------------------------------------
// Actual GL entry-points
// ----------------------------------------------------------------------------

#undef API_ENTRY
#undef CALL_GL_API
#undef CALL_GL_API_RETURN

#if USE_SLOW_BINDING

#if 0
    #define API_ENTRY(_api) _api

    #define CALL_GL_API(_api, ...)                                       \
        gl_hooks_t::gl_t const * const _c = &getGlThreadSpecific()->gl;  \
        if (_c) return _c->_api(__VA_ARGS__);
#endif /* if 0 */
    Not implemented.

/*
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
*/

#elif defined(__aarch64__) || defined(__arm__)

    #define API_ENTRY(_api) __attribute__((noinline)) _api

#if 0
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
#endif /* if 0 */

void *gl_stub_p = NULL;
void *gl_stub_long_p = NULL;
typedef uint64_t (*gl_proc_0)(uint64_t);
typedef uint64_t (*gl_proc_1)(uint64_t, uint64_t);
typedef uint64_t (*gl_proc_2)(uint64_t, uint64_t, uint64_t);
typedef uint64_t (*gl_proc_3)(uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*gl_proc_4)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*gl_proc_5)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*gl_proc_6)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					uint64_t);
typedef uint64_t (*gl_proc_7)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					uint64_t, uint64_t);
typedef uint64_t (*gl_proc_8)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					uint64_t, uint64_t, uint64_t);
typedef uint64_t (*gl_proc_9)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*gl_proc_10)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					 uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*gl_proc_11)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					 uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					 uint64_t);
typedef uint64_t (*gl_proc_15)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					 uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
					 uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    #define GET_GL_API(_api) __builtin_offsetof(gl_hooks_t, gl._api)
    #define GET_EGL_API(_api) __builtin_offsetof(egl_t, _api) + 10000

    #define CALL_GL_API_0(_api)                                     			\
        (*((gl_proc_0) gl_stub_p))(GET_GL_API(_api));

    #define CALL_GL_API_1(_api, arg1)                               			\
        (*((gl_proc_1) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1);

    #define CALL_GL_API_2(_api, arg1, arg2)                         			\
        (*((gl_proc_2) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2);

    #define CALL_GL_API_3(_api, arg1, arg2, arg3)                   			\
        (*((gl_proc_3) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2,	\
			(uint64_t) arg3);

    #define CALL_GL_API_4(_api, arg1, arg2, arg3, arg4)             			\
        (*((gl_proc_4) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2,	\
			(uint64_t) arg3, (uint64_t) arg4);

    #define CALL_GL_API_5(_api, arg1, arg2, arg3, arg4, arg5)       			\
        (*((gl_proc_5) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2,	\
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5);

    #define CALL_GL_API_6(_api, arg1, arg2, arg3, arg4, arg5, arg6)                	\
        (*((gl_proc_6) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2,  \
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5,    		\
			(uint64_t) arg6);

    #define CALL_GL_API_7(_api, arg1, arg2, arg3, arg4, arg5, arg6, arg7)          	\
        (*((gl_proc_7) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2,  \
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5,    		\
			(uint64_t) arg6, (uint64_t) arg7);

    #define CALL_GL_API_8(_api, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)    	\
        (*((gl_proc_8) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2,  \
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5,    		\
			(uint64_t) arg6, (uint64_t) arg7, (uint64_t) arg8);

    #define CALL_GL_API_9(_api, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,    	\
		    		arg9)    					   	\
        (*((gl_proc_9) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2,  \
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5,    		\
			(uint64_t) arg6, (uint64_t) arg7, (uint64_t) arg8,		\
			(uint64_t) arg9);

    #define CALL_GL_API_10(_api, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,    	\
		    		 arg9, arg10)    					\
        (*((gl_proc_10) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2, \
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5,    		\
			(uint64_t) arg6, (uint64_t) arg7, (uint64_t) arg8,		\
			(uint64_t) arg9, (uint64_t) arg10);

    #define CALL_GL_API_11(_api, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,    	\
		    		 arg9, arg10, arg11)    				\
        (*((gl_proc_11) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2, \
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5,    		\
			(uint64_t) arg6, (uint64_t) arg7, (uint64_t) arg8,		\
			(uint64_t) arg9, (uint64_t) arg10, (uint64_t) arg11);

    #define CALL_GL_API_15(_api, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,    	\
		    		 arg9, arg10, arg11, arg12, arg13, arg14, arg15)        \
        (*((gl_proc_15) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1, (uint64_t) arg2, \
			(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5,    		\
			(uint64_t) arg6, (uint64_t) arg7, (uint64_t) arg8,		\
			(uint64_t) arg9, (uint64_t) arg10, (uint64_t) arg11,		\
			(uint64_t) arg12, (uint64_t) arg13, (uint64_t) arg14,		\
			(uint64_t) arg15);

    #define CALL_GL_API_RETURN_0(rtype, _api)                                     	\
        return (rtype) (*((gl_proc_0) gl_stub_p))(GET_GL_API(_api));

    #define CALL_GL_API_RETURN_1(rtype, _api, arg1)                               	\
        return (rtype) (*((gl_proc_1) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1);

    #define CALL_GL_API_RETURN_2(rtype, _api, arg1, arg2)                         	\
        return (rtype) (*((gl_proc_2) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2);

    #define CALL_GL_API_RETURN_3(rtype, _api, arg1, arg2, arg3)                   	\
        return (rtype) (*((gl_proc_3) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3);

    #define CALL_GL_API_RETURN_4(rtype, _api, arg1, arg2, arg3, arg4)             	\
        return (rtype) (*((gl_proc_4) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4);

    #define CALL_GL_API_RETURN_5(rtype, _api, arg1, arg2, arg3, arg4, arg5)       	\
        return (rtype) (*((gl_proc_5) gl_stub_p))(GET_GL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4,		\
			(uint64_t) arg5);

    #define CALL_GL_API_RETURN_6(rtype, _api, arg1, arg2, arg3, arg4, arg5, arg6)	\
        return (rtype) (*((gl_proc_6) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4,		\
			(uint64_t) arg5, (uint64_t) arg6);

    #define CALL_GL_API_RETURN_8(rtype, _api, arg1, arg2, arg3, arg4, arg5, arg6, arg7, \
		    			      arg8)					\
        return (rtype) (*((gl_proc_8) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4,		\
			(uint64_t) arg5, (uint64_t) arg6, (uint64_t) arg7,		\
			(uint64_t) arg8);

    #define CALL_GL_API_RETURN_9(rtype, _api, arg1, arg2, arg3, arg4, arg5, arg6, arg7, \
		    			      arg8, arg9)    			   	\
        return (rtype) (*((gl_proc_9) gl_stub_long_p))(GET_GL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4,		\
			(uint64_t) arg5, (uint64_t) arg6, (uint64_t) arg7,		\
			(uint64_t) arg8, (uint64_t) arg9);

    #define CALL_EGL_API_RETURN_0(rtype, _api)                                     	\
        return (rtype) (*((gl_proc_0) gl_stub_p))(GET_EGL_API(_api));

    #define CALL_EGL_API_RETURN_1(rtype, _api, arg1)                               	\
        return (rtype) (*((gl_proc_1) gl_stub_p))(GET_EGL_API(_api), (uint64_t) arg1);

    #define CALL_EGL_API_RETURN_2(rtype, _api, arg1, arg2)                         	\
        return (rtype) (*((gl_proc_2) gl_stub_p))(GET_EGL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2);

    #define CALL_EGL_API_RETURN_3(rtype, _api, arg1, arg2, arg3)                   	\
        return (rtype) (*((gl_proc_3) gl_stub_p))(GET_EGL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3);

    #define CALL_EGL_API_RETURN_4(rtype, _api, arg1, arg2, arg3, arg4)             	\
        return (rtype) (*((gl_proc_4) gl_stub_p))(GET_EGL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4);

    #define CALL_EGL_API_RETURN_5(rtype, _api, arg1, arg2, arg3, arg4, arg5)       	\
        return (rtype) (*((gl_proc_5) gl_stub_p))(GET_EGL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4,		\
			(uint64_t) arg5);

    #define CALL_EGL_API_RETURN_6(rtype, _api, arg1, arg2, arg3, arg4, arg5, arg6)	\
        return (rtype) (*((gl_proc_6) gl_stub_long_p))(GET_EGL_API(_api), (uint64_t) arg1,	\
			(uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4,		\
			(uint64_t) arg5, (uint64_t) arg6);

#if 0
    #define CALL_GL_API(_api, ...)                                  \
        asm volatile (                                              \
	    "mov     x5, %[api]\n"				    \
            "mov     x16, %[func]\n"				    \
            "br      x16\n"                                         \
            "1:\n"                                                  \
            :                                                       \
            : [api] "i" (__builtin_offsetof(gl_hooks_t, gl._api)),  \
              [func] "r" (gl_stub_p)				    \
            : "x5", "x16"                                           \
        );

    #define CALL_EGL_API(_api, ...) {                               \
        asm volatile(						    \
	    "mov     x5, #10000\n"				    \
	    "add     x5, x5, %[api]\n"				    \
            "mov     x16, %[func]\n"				    \
            "br      x16\n"					    \
            "1:\n"                                                  \
            :							    \
            : [api] "i" (__builtin_offsetof(egl_t, _api)),   	    \
              [func] "r" (gl_stub_p)				    \
            : "x5", "x16");					    \
    }
#endif

#elif defined(__i386__)

#if 0
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
#endif /* if 0 */
    Not implemented.

#elif defined(__x86_64__)

#if 0
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
#endif /* if 0 */
    Not implemented.

#elif defined(__mips64)

#if 0
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
#endif /* if 0 */
    Not implemented.

#elif defined(__mips__)

#if 0
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
#endif /* if 0 */
    Not implemented.

#endif

#define CALL_GL_API_RETURN(_api, ...) \
    CALL_GL_API(_api, __VA_ARGS__) \
    return 0;
#if !defined(__aarch64__) && !defined(__arm__)
#define CALL_EGL_API	CALL_GL_API
#endif
#define CALL_EGL_API_RETURN(_api, ...) \
    CALL_EGL_API(_api, __VA_ARGS__) \
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

