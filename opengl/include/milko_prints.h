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

#ifndef __milkomeda_gl_h_
#define __milkomeda_gl_h_  

#include <stdlib.h>
#include <cutils/log.h>

#define CHECK(condition) condition ? (void(0)): (exit(1))

#define LIBNAME "milko_shim"
#define LOGINFO(...) ((void)__android_log_print(ANDROID_LOG_INFO, LIBNAME, __VA_ARGS__))
#define LOGWARN(...) ((void)__android_log_print(ANDROID_LOG_WARN, LIBNAME, __VA_ARGS__))
#define LOGERR(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LIBNAME, __VA_ARGS__))

#define LOGDBG1(...)
#define LOGDBG2(...)
#define LOGDBG3(...)
#define LOGDBG4(...)

#endif /*__milkomeda_gl_h_ */
