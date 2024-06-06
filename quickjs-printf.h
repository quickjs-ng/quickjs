/*
 * QuickJS Javascript printf functions
 *
 * Copyright (c) 2024 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef QUICKJS_PRINTF
#define QUICKJS_PRINTF

#include <stdarg.h>
#include <stddef.h>

#ifdef TEST_QUICKJS
#define JS_EXTERN
#define __js_printf_like(f, a)  __attribute__((format(printf, f-1, a-1)))
#define DynBuf void
#define JSContext void
#define JSRuntime void
#define js_snprintf(ctx, ...)   qjs_snprintf(__VA_ARGS__)
#define js_printf(ctx, ...)     qjs_printf(__VA_ARGS__)
#define js_printf_RT(rt, ...)   qjs_printf_RT(__VA_ARGS__)
#define js_fprintf(ctx, ...)    qjs_fprintf(__VA_ARGS__)
#define js_fprintf_RT(rt, ...)  qjs_fprintf_RT(__VA_ARGS__)
#else
#define __js_printf_like(f, a)  __attribute__((format(printf, f, a)))
#endif

__js_printf_like(4, 5)
int js_snprintf(JSContext *ctx, char *dest, size_t size, const char *fmt, ...);
int js_vsnprintf(JSContext *ctx, char *dest, size_t size, const char *fmt, va_list ap);
int dbuf_vprintf_ext(DynBuf *s, const char *fmt, va_list ap);
__js_printf_like(2, 3)
int js_printf(JSContext *ctx, const char *fmt, ...);
int js_vprintf(JSContext *ctx, const char *fmt, va_list ap);
__js_printf_like(2, 3)
int js_printf_RT(JSRuntime *rt, const char *fmt, ...);
int js_vprintf_RT(JSRuntime *rt, const char *fmt, va_list ap);
__js_printf_like(3, 4)
int js_fprintf(JSContext *ctx, FILE *fp, const char *fmt, ...);
int js_vfprintf(JSContext *ctx, FILE *fp, const char *fmt, va_list ap);
__js_printf_like(3, 4)
int js_fprintf_RT(JSRuntime *rt, FILE *fp, const char *fmt, ...);
int js_vfprintf_RT(JSRuntime *rt, FILE *fp, const char *fmt, va_list ap);

#endif // QUICKJS_PRINTF
