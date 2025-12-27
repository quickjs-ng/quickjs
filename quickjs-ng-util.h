/*
 * QuickJS-NG C library
 *
 * Copyright (c) 2025 QuickJS-NG contributors
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

/*
 * This header defines various macros that mainly cater to Windows requirement
 * of specifying every single interface of a dynamic library.
 */

#ifndef QUICKJS_NG_UTIL_H
#define QUICKJS_NG_UTIL_H

/* Helpers. */
#if defined(_WIN32) || defined(__CYGWIN__)
# define QUICKJS_NG_PLAT_WIN32 1
#endif /* defined(_WIN32) || defined(__CYGWIN__) */

#if defined(__GNUC__) || defined(__clang__)
# define QUICKJS_NG_CC_GNULIKE 1
#endif /* defined(__GNUC__) || defined(__clang__) */

/*
 * `QUICKJS_NG_API` -- helper macro that must be used to mark the API of libqjs.
 *
 * Windows note: The `__declspec` syntax is supported by both Clang and GCC.
 * The QUICKJS_NG_INTERNAL macro must be defined for libqjs (and only for it) to
 * properly export symbols.
 */
#ifdef QUICKJS_NG_PLAT_WIN32
# ifdef QUCICKJS_NG_DLL
#  ifdef QUICKJS_NG_INTERNAL
#   define QUICKJS_NG_API __declspec(dllexport)
#  else
#    define QUICKJS_NG_API __declspec(dllimport)
#  endif
# else
#  define QUICKJS_NG_API /* nothing */
# endif
#else
# ifdef QUICKJS_NG_CC_GNULIKE
#  define QUICKJS_NG_API __attribute__((visibility("default")))
# else
#  define QUICKJS_NG_API /* nothing */
# endif
#endif /* QUICKJS_NG_PLAT_WIN32 */

/*
 * `QUICKJS_NG_FORCE_INLINE` -- helper macro forcing the inlining of a function.
 */
#ifdef QUICKJS_NG_PLAT_WIN32
# ifdef QUICKJS_NG_CC_GNULIKE
#  define QUICKJS_NG_FORCE_INLINE inline __attribute__((always_inline))
# else
#  define QUICKJS_NG_FORCE_INLINE __forceinline
# endif
#else
# ifdef QUICKJS_NG_CC_GNULIKE
#  define QUICKJS_NG_FORCE_INLINE inline __attribute__((always_inline))
# else
#  define QUICKJS_NG_FORCE_INLINE inline
# endif
#endif /* QUICKJS_NG_PLAT_WIN32 */

#endif /* QUICKJS_NG_UTIL_H */
