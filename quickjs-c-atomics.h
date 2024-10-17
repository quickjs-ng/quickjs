/*
 * QuickJS C atomics definitions
 *
 * Copyright (c) 2023 Marcin Kolny
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

#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
   // Use GCC builtins for version < 4.9
#  if((__GNUC__ << 16) + __GNUC_MINOR__ < ((4) << 16) + 9)
#    define GCC_BUILTIN_ATOMICS
#  endif
#endif

#ifdef GCC_BUILTIN_ATOMICS
#define atomic_fetch_add(obj, arg) \
    __atomic_fetch_add(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_strong(obj, expected, desired) \
    __atomic_compare_exchange_n(obj, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_exchange(obj, desired) \
    __atomic_exchange_n (obj, desired, __ATOMIC_SEQ_CST)
#define atomic_load(obj) \
    __atomic_load_n(obj, __ATOMIC_SEQ_CST)
#define atomic_store(obj, desired) \
    __atomic_store_n(obj, desired, __ATOMIC_SEQ_CST)
#define atomic_fetch_or(obj, arg) \
    __atomic_fetch_or(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_fetch_xor(obj, arg) \
    __atomic_fetch_xor(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_fetch_and(obj, arg) \
    __atomic_fetch_and(obj, arg, __ATOMIC_SEQ_CST)
#define atomic_fetch_sub(obj, arg) \
    __atomic_fetch_sub(obj, arg, __ATOMIC_SEQ_CST)
#define _Atomic
#else
#if defined(_MSC_VER) && _MSC_VER < 1935
#define _Atomic
/*
 * Copypasted from: https://github.com/videolan/dav1d/blob/master/include/compat/msvc/stdatomic.h
 */
#include <windows.h>

typedef volatile LONG  atomic_int;
typedef volatile ULONG atomic_uint;

typedef enum {
    memory_order_relaxed,
    memory_order_acquire
} msvc_atomic_memory_order;

#define atomic_init(p_a, v)           do { *(p_a) = (v); } while(0)
#define atomic_store(p_a, v) _Generic((p_a), \
                     long volatile *: _InterlockedCompareExchange, \
                    __int64 volatile *: _InterlockedCompareExchange64  \
                    short volatile *: _InterlockedCompareExchange16,  \
              )(p_a, v)
#define atomic_load(p_a) _Generic((p_a), \
                     long volatile *: _InterlockedCompareExchange, \
                    __int64 volatile *: _InterlockedCompareExchange64  \
                    short volatile *: _InterlockedCompareExchange16,  \
              )(p_a, 0, 0)
#define atomic_exchange(p_a, v) _Generic((p_a), \
                     long volatile *: _InterlockedExchange, \
                    __int64 volatile *: _InterlockedExchange64  \
                    short volatile *: _InterlockedExchange16,  \
                    char volatile *: _InterlockedExchange8  \
              )(p_a, v)
#define atomic_load_explicit(p_a, mo) atomic_load(p_a)

static inline int atomic_compare_exchange_strong_int(LONG *obj, LONG *expected,
                                                     LONG desired)
{
    LONG orig = *expected;
    *expected = InterlockedCompareExchange(obj, desired, orig);
    return *expected == orig;
}
#define atomic_compare_exchange_strong(p_a, expected, desired) atomic_compare_exchange_strong_int((LONG *)p_a, (LONG *)expected, (LONG)desired)

/*
 * TODO use a special call to increment/decrement
 * using InterlockedIncrement/InterlockedDecrement
 */
#define atomic_fetch_add(p_a, inc) _Generic((p_a), \
                     long volatile *: _InterlockedExchangeAdd, \
                    __int64 volatile *: _InterlockedExchangeAdd64  \
                    short volatile *: _InterlockedExchangeAdd16,  \
                    char volatile *: _InterlockedExchangeAdd8  \
              )(p_a, inc)
#define atomic_fetch_sub(p_a, dec) _Generic((p_a), \
                     long volatile *: _InterlockedExchangeAdd, \
                    __int64 volatile *: _InterlockedExchangeAdd64  \
                    short volatile *: _InterlockedExchangeAdd16,  \
                    char volatile *: _InterlockedExchangeAdd8  \
              )(p_a, -(dec))
#define atomic_fetch_or(p_a, v) _Generic((p_a), \
                     long volatile *: _InterlockedOr, \
                    __int64 volatile *: _InterlockedOr64  \
                    short volatile *: _InterlockedOr16,  \
                    char volatile *: _InterlockedOr8  \
              )(p_a, v)
#define atomic_fetch_add_explicit(p_a, inc, mo) atomic_fetch_add(p_a, inc)

#else
#include <stdatomic.h>
#endif
#endif
