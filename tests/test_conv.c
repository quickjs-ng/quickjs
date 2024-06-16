/*
 * Benchmark integer conversion variants
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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//#define USE_SINGLE_CASE  1  // special case single digit numbers
#define USE_SPECIAL_RADIX_10  1  // special case single digit numbers
#define USE_SINGLE_CASE_FAST  1  // special case single digit numbers

#define TEST_SNPRINTF     1  // use snprintf for specific bases (for reference)
#define TEST_NAIVE        1  // naive digit loop and copy loops
#define TEST_REVERSE      1  // naive digit loop and reverse digit string
#define TEST_DIGIT_PAIRS  1  // generate 2 decimal digits at a time
#define TEST_DIGIT_1PASS  1  // generate left to right decimal digits
#define TEST_LENGTH_LOOP  1  // compute length before digit loop using loop
#define TEST_LENGTH_EXPR  1  // compute length before digit loop using expression
#define TEST_SHIFTBUF     1  // generate up to 7 byte chunks in a register
#define TEST_BLOCKMOV     1  // move all digits together
#define TEST_DIV_TABLE    1  // use multiplier table instead of radix divisions
#define TEST_DISPATCH     1  // use dispatch table to optimal 64-bit radix converters

#if (defined(__GNUC__) && !defined(__clang__))
#undef TEST_NAIVE            // 32-bit gcc overoptimizes this code
#undef TEST_DIGIT_PAIRS
#endif

/* definitions from cutils.h */

#if !defined(_MSC_VER) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define minimum_length(n)  static n
#else
#define minimum_length(n)  n
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#  define likely(x)       (x)
#else
#  define likely(x)       __builtin_expect(!!(x), 1)
#endif

#ifndef countof
#define countof(a)  (sizeof(a) / sizeof((a)[0]))
#endif

static inline uint8_t is_be(void) {
    union {
        uint16_t a;
        uint8_t  b;
    } u = { 0x100 };
    return u.b;
}

/* WARNING: undefined if a = 0 */
static inline int clz32(unsigned int a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index;
    _BitScanReverse(&index, a);
    return 31 - index;
#else
    return __builtin_clz(a);
#endif
}

/* WARNING: undefined if a = 0 */
static inline int clz64(uint64_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
#if INTPTR_MAX == INT64_MAX
    unsigned long index;
    _BitScanReverse64(&index, a);
    return 63 - index;
#else
    if (a >> 32)
        return clz32((unsigned)(a >> 32));
    else
        return clz32((unsigned)a) + 32;
#endif	
#else
    return __builtin_clzll(a);
#endif
}
// prototypes for final functions
extern char const digits36[36];
size_t u32toa(char buf[minimum_length(11)], uint32_t n);
size_t i32toa(char buf[minimum_length(12)], int32_t n);
size_t u64toa(char buf[minimum_length(21)], uint64_t n);
size_t i64toa(char buf[minimum_length(22)], int64_t n);
size_t u32toa_radix(char buf[minimum_length(33)], uint32_t n, unsigned base);
size_t i32toa_radix(char buf[minimum_length(34)], int32_t n, unsigned base);
size_t u64toa_radix(char buf[minimum_length(65)], uint64_t n, unsigned base);
size_t i64toa_radix(char buf[minimum_length(66)], int64_t n, unsigned base);

/*---- integer to string conversions ----*/

/* All conversion functions:
   - require a destination array `buf` of sufficient length
   - write the string representation at the beginning of `buf`
   - null terminate the string
   - return the string length
 */

/* 2 <= base <= 36 */
char const digits36[36] = "0123456789abcdefghijklmnopqrstuvwxyz";

/*---- variants ----*/

#define define_i32toa(v) \
size_t i32toa_##v(char buf[minimum_length(12)], int32_t n) \
{ \
    if (likely(n >= 0)) \
        return u32toa_##v(buf, n); \
    buf[0] = '-'; \
    return 1 + u32toa_##v(buf + 1, -(uint32_t)n); \
}

#define define_i64toa(v) \
size_t i64toa_##v(char buf[minimum_length(22)], int64_t n) \
{ \
    if (likely(n >= 0)) \
        return u64toa_##v(buf, n); \
    buf[0] = '-'; \
    return 1 + u64toa_##v(buf + 1, -(uint64_t)n); \
}

#ifdef TEST_SHIFTBUF

#define gen_digit(buf, c)  if (is_be()) \
            buf = (buf >> 8) | ((uint64_t)(c) << ((sizeof(buf) - 1) * 8)); \
        else \
            buf = (buf << 8) | (c)

size_t u7toa_shift(char dest[minimum_length(8)], uint32_t n)
{
    size_t len = 1;
    uint64_t buf = 0;
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        gen_digit(buf, '0' + quo);
        len++;
    }
    gen_digit(buf, '0' + n);
    memcpy(dest, &buf, sizeof buf);
    return len;
}

size_t u07toa_shift(char dest[minimum_length(8)], uint32_t n, size_t len)
{
    size_t i;
    dest += len;
    dest[7] = '\0';
    for (i = 7; i-- > 1;) {
        uint32_t quo = n % 10;
        n /= 10;
        dest[i] = (char)('0' + quo);
    }
    dest[i] = (char)('0' + n);
    return len + 7;
}

size_t u32toa_shift(char buf[minimum_length(11)], uint32_t n)
{
#ifdef USE_SINGLE_CASE_FAST /* 10% */
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
#endif
#define TEN_POW_7 10000000
    if (n >= TEN_POW_7) {
        uint32_t quo = n / TEN_POW_7;
        n %= TEN_POW_7;
        size_t len = u7toa_shift(buf, quo);
        return u07toa_shift(buf, n, len);
    }
    return u7toa_shift(buf, n);
}

size_t u64toa_shift(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_shift(buf, n);

    size_t len;
    if (n >= TEN_POW_7) {
        uint64_t n1 = n / TEN_POW_7;
        n %= TEN_POW_7;
        if (n1 >= TEN_POW_7) {
            uint32_t quo = n1 / TEN_POW_7;
            n1 %= TEN_POW_7;
            len = u7toa_shift(buf, quo);
            len = u07toa_shift(buf, n1, len);
        } else {
            len = u7toa_shift(buf, n1);
        }
        return u07toa_shift(buf, n, len);
    }
    return u7toa_shift(buf, n);
}

define_i32toa(shift)
define_i64toa(shift)

#endif /* TEST_SHIFTBUF */

#if defined(TEST_DIGIT_PAIRS) || defined(TEST_DIGIT_1PASS)
static char const digits100[200] =
    "00010203040506070809"
    "10111213141516171819"
    "20212223242526272829"
    "30313233343536373839"
    "40414243444546474849"
    "50515253545556575859"
    "60616263646566676869"
    "70717273747576777879"
    "80818283848586878889"
    "90919293949596979899";
#endif

#ifdef TEST_DIGIT_PAIRS
size_t u32toa_pair(char buf[minimum_length(11)], uint32_t n)
{
#ifdef USE_SINGLE_CASE
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
#endif
    char *p = buf;
    char *q = buf + 10;
    while (n >= 100) {
        uint32_t quo = n % 100;
        n /= 100;
        *--q = digits100[2 * quo + 1];
        *--q = digits100[2 * quo];
    }
    *p = digits100[2 * n];
    p += *p != '0';
    *p++ = digits100[2 * n + 1];
    while (q < buf + 10) {
        *p++ = *q++;
        *p++ = *q++;
    }
    *p = '\0';
    return p - buf;
}

size_t u64toa_pair(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_pair(buf, n);

    char *p = buf;
    char *q = buf + 20;
    while (n >= 100) {
        uint32_t quo = n % 100;
        n /= 100;
        *--q = digits100[2 * quo + 1];
        *--q = digits100[2 * quo];
    }
    *p = digits100[2 * n];
    p += *p != '0';
    *p++ = digits100[2 * n + 1];
    while (q < buf + 20) {
        *p++ = *q++;
        *p++ = *q++;
    }
    *p = '\0';
    return p - buf;
}

define_i32toa(pair)
define_i64toa(pair)

#endif /* TEST_DIGIT_PAIRS */

#if TEST_DIGIT_1PASS
static char *u4toa(char p[minimum_length(4)], uint32_t n)
{
    const char *digits = digits100;
    if (n >= 100) {
        uint32_t n1 = n / 100;
        n -= n1 * 100;
        *p = digits[2 * n1];
        p += digits[2 * n1] != '0';
        *p++ = digits[2 * n1 + 1];
        *p++ = digits[2 * n];
        *p++ = digits[2 * n + 1];
        return p;
    } else {
        *p = digits[2 * n];
        p += digits[2 * n] != '0';
        *p++ = digits[2 * n + 1];
        return p;
    }
}

static char *u04toa(char p[minimum_length(4)], uint32_t n)
{
    const char *digits = digits100;
    uint32_t n1 = n / 100;
    n -= n1 * 100;
    *p++ = digits[2 * n1];
    *p++ = digits[2 * n1 + 1];
    *p++ = digits[2 * n];
    *p++ = digits[2 * n + 1];
    return p;
}

static char *u8toa(char p[minimum_length(8)], uint32_t n)
{
    if (n >= 10000) {
        uint32_t n1 = n / 10000;
        n -= n1 * 10000;
        p = u4toa(p, n1);
        return u04toa(p, n);
    }
    return u4toa(p, n);
}

static char *u08toa(char p[minimum_length(8)], uint32_t n)
{
    uint32_t n1 = n / 10000;
    n -= n1 * 10000;
    p = u04toa(p, n1);
    return u04toa(p, n);
}

size_t u32toa_pair_1pass(char buf[minimum_length(11)], uint32_t n)
{
#ifdef USE_SINGLE_CASE /* 6% */
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
#endif
    char *p = buf;
    /* division by known base uses multiplication */
    if (n >= 100000000) {
        uint32_t n1 = n / 100000000;
        n -= n1 * 100000000;
        /* 1 <= n1 <= 42 */
        *p = digits100[2 * n1];
        p += *p != '0';
        *p++ = digits100[2 * n1 + 1];
        p = u08toa(p, n);
    } else {
        p = u8toa(p, n);
    }
    *p = '\0';
    return p - buf;
}

size_t u64toa_pair_1pass(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_pair_1pass(buf, n);

    char *p = buf;

    /* division by known base uses multiplication */
    if (n >= 100000000) {
        uint64_t n1 = n / 100000000;
        n -= n1 * 100000000;
        if (n1 >= 100000000) {
            uint32_t n2 = n1 / 100000000;
            n1 -= n2 * 100000000;
            // 1 <= n2 <= 1844
            p = u4toa(p, n2);
            p = u08toa(p, n1);
            p = u08toa(p, n);
        } else {
            p = u8toa(p, n1);
            p = u08toa(p, n);
        }
    } else {
        p = u8toa(p, n);
    }
    *p = '\0';
    return p - buf;
}

define_i32toa(pair_1pass)
define_i64toa(pair_1pass)

#endif /* TEST_DIGIT_1PASS */

#ifdef TEST_SNPRINTF
size_t u32toa_snprintf(char buf[minimum_length(11)], uint32_t n)
{
    return snprintf(buf, 11, "%"PRIu32, n);
}

size_t i32toa_snprintf(char buf[minimum_length(12)], int32_t n)
{
    return snprintf(buf, 12, "%"PRId32, n);
}

size_t u64toa_snprintf(char buf[minimum_length(21)], uint64_t n)
{
    return snprintf(buf, 21, "%"PRIu64, n);
}

size_t i64toa_snprintf(char buf[minimum_length(22)], int64_t n)
{
    return snprintf(buf, 22, "%"PRId64, n);
}
#endif /* TEST_SNPRINTF */

#ifdef TEST_NAIVE
size_t u32toa_naive(char buf[minimum_length(11)], uint32_t n)
{
#ifdef USE_SINGLE_CASE
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
#endif
    char *p = buf;
    char *q = buf + 10;
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *--q = (char)('0' + quo);
    }
    *p++ = (char)('0' + n);
    while (q < buf + 10)
        *p++ = *q++;
    *p = '\0';
    return p - buf;
}

size_t u64toa_naive(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_naive(buf, n);

    char *p = buf;
    char *q = buf + 20;
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *--q = (char)('0' + quo);
    }
    *p++ = (char)('0' + n);
    while (q < buf + 20)
        *p++ = *q++;
    *p = '\0';
    return p - buf;
}

define_i32toa(naive)
define_i64toa(naive)

#endif /* TEST_NAIVE */

#ifdef TEST_LENGTH_EXPR
size_t u32toa_length_expr(char buf[minimum_length(11)], uint32_t n)
{
#ifdef USE_SINGLE_CASE_FAST /* 8% */
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
    size_t len = (2 + (n > 99) + (n > 999) +
                  (n > 9999) + (n > 99999) + (n > 999999) +
                  (n > 9999999) + (n > 99999999) + (n > 999999999));
#else
    size_t len = (1 + (n > 9) + (n > 99) + (n > 999) +
                  (n > 9999) + (n > 99999) + (n > 999999) +
                  (n > 9999999) + (n > 99999999) + (n > 999999999));
#endif
    char *end = buf + len;
    *end-- = '\0';
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *end-- = (char)('0' + quo);
    }
    *end = (char)('0' + n);
    return len;
}

size_t u64toa_length_expr(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_length_expr(buf, n);
#if 0
    size_t len = 10 + ((n >= 10000000000ULL) +
                       (n >= 100000000000ULL) +
                       (n >= 1000000000000ULL) +
                       (n >= 10000000000000ULL) +
                       (n >= 100000000000000ULL) +
                       (n >= 1000000000000000ULL) +
                       (n >= 10000000000000000ULL) +
                       (n >= 100000000000000000ULL) +
                       (n >= 1000000000000000000ULL) +
                       (n >= 10000000000000000000ULL));
    char *end = buf + len;
    *end-- = '\0';
#else
    uint64_t n10 = 1000000000;
    uint32_t last = n % 10;
    n /= 10;
    size_t len = 10;
    while (n >= n10) {
        n10 *= 10;
        len++;
    }
    char *end = buf + len;
    *end-- = '\0';
    *end-- = (char)('0' + last);
#endif
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *end-- = (char)('0' + quo);
    }
    *end = (char)('0' + n);
    return len;
}

define_i32toa(length_expr)
define_i64toa(length_expr)

#endif /* TEST_LENGTH_EXPR */

#ifdef TEST_LENGTH_LOOP
size_t u32toa_length_loop(char buf[minimum_length(11)], uint32_t n)
{
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
    uint32_t last = n % 10;
    n /= 10;
    uint32_t n10 = 10;
    size_t len = 2;
    while (n >= n10) {
        n10 *= 10;
        len++;
    }
    char *end = buf + len;
    *end-- = '\0';
    *end-- = (char)('0' + last);
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *end-- = (char)('0' + quo);
    }
    *end = (char)('0' + n);
    return len;
}

size_t u64toa_length_loop(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_length_loop(buf, n);

    uint32_t last = n % 10;
    n /= 10;
    uint64_t n10 = 1000000000;
    size_t len = 10;
    while (n >= n10) {
        n10 *= 10;
        len++;
    }
    char *end = buf + len;
    *end-- = '\0';
    *end-- = (char)('0' + last);
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *end-- = (char)('0' + quo);
    }
    *end = (char)('0' + n);
    return len;
}

define_i32toa(length_loop)
define_i64toa(length_loop)

#endif /* TEST_LENGTH_LOOP */

#if defined(TEST_REVERSE) || defined(TEST_DISPATCH)
size_t u32toa_reverse(char buf[minimum_length(11)], uint32_t n)
{
#ifdef USE_SINGLE_CASE
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
#endif
    char *end;
    size_t len = 0;
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        buf[len++] = (char)('0' + quo);
    }
    buf[len++] = (char)('0' + n);
    buf[len] = '\0';
    for (end = buf + len - 1; buf < end;) {
        char c = *buf;
        *buf++ = *end;
        *end-- = c;
    }
    return len;
}

size_t u64toa_reverse(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_reverse(buf, n);

    char *end;
    size_t len = 0;
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        buf[len++] = (char)('0' + quo);
    }
    buf[len++] = (char)('0' + n);
    buf[len] = '\0';
    for (end = buf + len - 1; buf < end;) {
        char c = *buf;
        *buf++ = *end;
        *end-- = c;
    }
    return len;
}

define_i32toa(reverse)
define_i64toa(reverse)

#endif /* TEST_REVERSE */

#ifdef TEST_BLOCKMOV
size_t u32toa_blockmov(char buf[minimum_length(11)], uint32_t n)
{
#ifdef USE_SINGLE_CASE_FAST /* 6% */
    if (n < 10) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
#endif
    char buf1[10+10];
    char *p = buf;
    char *q = buf1 + 10;
    *q = '\0';
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *--q = (char)('0' + quo);
    }
    *p++ = (char)('0' + n);
    memcpy(p, q, 10);
    return (buf1 + 10) - q + 1;
}

size_t u64toa_blockmov(char buf[minimum_length(21)], uint64_t n)
{
    if (likely(n < 0x100000000))
        return u32toa_blockmov(buf, n);

    char buf1[20+20];
    char *p = buf;
    char *q = buf1 + 20;
    *q = '\0';
    while (n >= 10) {
        uint32_t quo = n % 10;
        n /= 10;
        *--q = (char)('0' + quo);
    }
    *p++ = (char)('0' + n);
    memcpy(p, q, 20);
    return (buf1 + 20) - q + 1;
}

define_i32toa(blockmov)
define_i64toa(blockmov)

#endif /* TEST_BLOCKMOV */

/*---- radix conversion variants ----*/

#define define_i32toa_radix(v) \
size_t i32toa_radix_##v(char buf[minimum_length(34)], int32_t n, unsigned base) \
{ \
    if (likely(n >= 0)) \
        return u32toa_radix_##v(buf, n, base); \
    buf[0] = '-'; \
    return 1 + u32toa_radix_##v(buf + 1, -(uint32_t)n, base); \
}

#define define_i64toa_radix(v) \
size_t i64toa_radix_##v(char buf[minimum_length(66)], int64_t n, unsigned base) \
{ \
    if (likely(n >= 0)) \
        return u64toa_radix_##v(buf, n, base); \
    buf[0] = '-'; \
    return 1 + u64toa_radix_##v(buf + 1, -(uint64_t)n, base); \
}

#ifdef TEST_NAIVE
size_t u32toa_radix_naive(char buf[minimum_length(33)], uint32_t n, unsigned base)
{
#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u32toa_naive(buf, n);
#endif
#ifdef USE_SINGLE_CASE
    if (n < base) {
        buf[0] = digits36[n];
        buf[1] = '\0';
        return 1;
    }
#endif
    char buf1[32];
    char *q = buf1 + 32;
    char *p = buf;
    while (n >= base) {
        size_t digit = n % base;
        n /= base;
        *--q = digits36[digit];
    }
    *--q = digits36[n];
    while (q < buf1 + 32) {
        *p++ = *q++;
    }
    *p = '\0';
    return p - buf;
}

size_t u64toa_radix_naive(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    if (likely(n < 0x100000000))
        return u32toa_radix_naive(buf, n, base);
#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u64toa_naive(buf, n);
#endif
    char buf1[64];
    char *q = buf1 + 64;
    char *p = buf;
    while (n >= base) {
        size_t digit = n % base;
        n /= base;
        *--q = digits36[digit];
    }
    *--q = digits36[n];
    while (q < buf1 + 64) {
        *p++ = *q++;
    }
    *p = '\0';
    return p - buf;
}

define_i32toa_radix(naive)
define_i64toa_radix(naive)

#endif // TEST_NAIVE

#if defined(TEST_REVERSE) || defined(TEST_DISPATCH)
size_t u32toa_radix_reverse(char buf[minimum_length(33)], uint32_t n, unsigned base)
{
#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u32toa_reverse(buf, n);
#endif
#ifdef USE_SINGLE_CASE
    if (n < base) {
        buf[0] = digits36[n];
        buf[1] = '\0';
        return 1;
    }
#endif
    char *end;
    size_t len = 0;
    while (n >= base) {
        uint32_t quo = n % base;
        n /= base;
        buf[len++] = digits36[quo];
    }
    buf[len++] = digits36[n];
    buf[len] = '\0';
    for (end = buf + len - 1; buf < end;) {
        char c = *buf;
        *buf++ = *end;
        *end-- = c;
    }
    return len;
}

size_t u64toa_radix_reverse(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    if (likely(n < 0x100000000))
        return u32toa_radix_reverse(buf, n, base);
#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u64toa_reverse(buf, n);
#endif
    char *end;
    size_t len = 0;
    while (n >= base) {
        uint32_t quo = n % base;
        n /= base;
        buf[len++] = digits36[quo];
    }
    buf[len++] = digits36[n];
    buf[len] = '\0';
    for (end = buf + len - 1; buf < end;) {
        char c = *buf;
        *buf++ = *end;
        *end-- = c;
    }
    return len;
}

define_i32toa_radix(reverse)
define_i64toa_radix(reverse)

#endif // TEST_REVERSE

#ifdef TEST_LENGTH_LOOP

static uint8_t const radix_shift[64] = {
    0, 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
    4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

size_t u32toa_radix_length(char buf[minimum_length(33)], uint32_t n, unsigned base)
{
    int shift;

#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u32toa_length_loop(buf, n);
#endif
    if (n < base) {
        buf[0] = digits36[n];
        buf[1] = '\0';
        return 1;
    }
    shift = radix_shift[base & 63];
    if (shift) {
        uint32_t mask = (1 << shift) - 1;
        size_t len = (32 - clz32(n) + shift - 1) / shift;
        size_t last = n & mask;
        char *end = buf + len;
        n >>= shift;
        *end-- = '\0';
        *end-- = digits36[last];
        while (n >= base) {
            size_t quo = n & mask;
            n >>= shift;
            *end-- = digits36[quo];
        }
        *end = digits36[n];
        return len;
    } else {
        size_t len = 2;
        size_t last = n % base;
        n /= base;
        uint32_t nbase = base;
        while (n >= nbase) {
            nbase *= base;
            len++;
        }
        char *end = buf + len;
        *end-- = '\0';
        *end-- = digits36[last];
        while (n >= base) {
            size_t quo = n % base;
            n /= base;
            *end-- = digits36[quo];
        }
        *end = digits36[n];
        return len;
    }
}

size_t u64toa_radix_length(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    int shift;

#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u64toa_length_loop(buf, n);
#endif
    shift = radix_shift[base & 63];
    if (shift) {
        if (n < base) {
            buf[0] = digits36[n];
            buf[1] = '\0';
            return 1;
        }
        uint64_t mask = (1 << shift) - 1;
        size_t len = (64 - clz64(n) + shift - 1) / shift;
        size_t last = n & mask;
        char *end = buf + len;
        n >>= shift;
        *end-- = '\0';
        *end-- = digits36[last];
        while (n >= base) {
            size_t quo = n & mask;
            n >>= shift;
            *end-- = digits36[quo];
        }
        *end = digits36[n];
        return len;
    } else {
        if (likely(n < 0x100000000))
            return u32toa_radix_length(buf, n, base);
        size_t last = n % base;
        n /= base;
        uint64_t nbase = base;
        size_t len = 2;
        while (n >= nbase) {
            nbase *= base;
            len++;
        }
        char *end = buf + len;
        *end-- = '\0';
        *end-- = digits36[last];
        while (n >= base) {
            size_t quo = n % base;
            n /= base;
            *end-- = digits36[quo];
        }
        *end = digits36[n];
        return len;
    }
}

define_i32toa_radix(length)
define_i64toa_radix(length)

#endif // TEST_LENGTH_LOOP

#ifdef TEST_DIV_TABLE
static struct {
    uint32_t chunk : 27;
    uint32_t ndig : 5;
    uint32_t mul;
} const div_table32[37] = {
    { 0, 0, 0 }, // 0
    { 1, 1, 1 }, // 1
    { 67108864, 26, 2147483648 }, // 2
    { 129140163, 17, 1431655776 }, // 3
    { 67108864, 13, 1073741824 }, // 4
    { 48828125, 11, 858993464 }, // 5
    { 60466176, 10, 715827888 }, // 6
    { 40353607, 9, 613566760 }, // 7
    { 16777216, 8, 536870912 }, // 8
    { 43046721, 8, 477218592 }, // 9
    { 100000000, 8, 429496732 }, // 10
    { 19487171, 7, 390451574 }, // 11
    { 35831808, 7, 357913944 }, // 12
    { 62748517, 7, 330382100 }, // 13
    { 105413504, 7, 306783380 }, // 14
    { 11390625, 6, 286331154 }, // 15
    { 16777216, 6, 268435456 }, // 16
    { 24137569, 6, 252645136 }, // 17
    { 34012224, 6, 238609296 }, // 18
    { 47045881, 6, 226050912 }, // 19
    { 64000000, 6, 214748366 }, // 20
    { 85766121, 6, 204522253 }, // 21
    { 113379904, 6, 195225787 }, // 22
    { 6436343, 5, 186737709 }, // 23
    { 7962624, 5, 178956972 }, // 24
    { 9765625, 5, 171798692 }, // 25
    { 11881376, 5, 165191050 }, // 26
    { 14348907, 5, 159072864 }, // 27
    { 17210368, 5, 153391690 }, // 28
    { 20511149, 5, 148102321 }, // 29
    { 24300000, 5, 143165577 }, // 30
    { 28629151, 5, 138547333 }, // 31
    { 33554432, 5, 134217728 }, // 32
    { 39135393, 5, 130150525 }, // 33
    { 45435424, 5, 126322568 }, // 34
    { 52521875, 5, 122713352 }, // 35
    { 60466176, 5, 119304648 }, // 36
};

size_t u32toa_radix_div_table(char buf[minimum_length(33)], uint32_t n, unsigned base)
{
#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u32toa_shift(buf, n);
#endif
#ifdef USE_SINGLE_CASE
    if (n < base) {
        buf[0] = digits36[n];
        buf[1] = '\0';
        return 1;
    }
#endif
    char buf1[32];
    char *q = buf1 + 32;
    char *p = buf;
    uint32_t chunk = div_table32[base].chunk;
    uint32_t ndig = div_table32[base].ndig;
    uint32_t mul = div_table32[base].mul;
    while (n >= chunk) {
        uint32_t quo = n / chunk;
        uint32_t n1 = n - quo * chunk;
        n = quo;
        for (uint32_t i = ndig; i-- > 0;) {
            uint32_t quo1 = ((uint64_t)n1 * mul) >> 32;
            size_t digit = n1 - quo1 * base;
            n1 = quo1;
            *--q = digits36[digit];
        }
    }
    while (n >= base) {
        uint32_t quo = ((uint64_t)n * mul) >> 32;
        size_t digit = n - quo * base;
        n = quo;
        *--q = digits36[digit];
    }
    *--q = digits36[n];
    while (q < buf1 + 32) {
        *p++ = *q++;
    }
    *p = '\0';
    return p - buf;
}

size_t u64toa_radix_div_table(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    if (likely(n < 0x100000000))
        return u32toa_radix_div_table(buf, n, base);
#ifdef USE_SPECIAL_RADIX_10
    if (likely(base == 10))
        return u64toa_shift(buf, n);
#endif
    char buf1[64];
    char *q = buf1 + 64;
    char *p = buf;
    uint32_t chunk = div_table32[base].chunk;
    uint32_t ndig = div_table32[base].ndig;
    uint32_t mul = div_table32[base].mul;
    while (n >= chunk) {
        uint64_t quo = n / chunk;
        uint32_t n1 = n - quo * chunk;
        n = quo;
        for (uint32_t i = ndig; i-- > 0;) {
            uint32_t quo1 = ((uint64_t)n1 * mul) >> 32;
            size_t digit = n1 - quo1 * base;
            n1 = quo1;
            *--q = digits36[digit];
        }
    }
    uint32_t n1 = n;
    while (n1 >= base) {
        uint32_t quo1 = ((uint64_t)n1 * mul) >> 32;
        size_t digit = n1 - quo1 * base;
        n1 = quo1;
        *--q = digits36[digit];
    }
    *--q = digits36[n1];
    while (q < buf1 + 64) {
        *p++ = *q++;
    }
    *p = '\0';
    return p - buf;
}

define_i32toa_radix(div_table)
define_i64toa_radix(div_table)

#endif // TEST_DIV_TABLE

#ifdef TEST_DISPATCH

static size_t u64toa_radix_d10(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    return u64toa_shift(buf, n);
}

static size_t u64toa_radix_d2(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    if (n < 2) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
    size_t len = (64 - clz64(n));
    char *end = buf + len;
    *end-- = '\0';
    while (n >= 2) {
        uint32_t quo = n & 1;
        n >>= 1;
        *end-- = (char)('0' + quo);
    }
    *end-- = (char)('0' + n);
    return len;
}

static size_t u64toa_radix_d8(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    if (n < 8) {
        buf[0] = (char)('0' + n);
        buf[1] = '\0';
        return 1;
    }
    size_t len = (64 - clz64(n) + 2) / 3;
    char *end = buf + len;
    *end-- = '\0';
    while (n >= 8) {
        uint32_t quo = n & 7;
        n >>= 3;
        *end-- = (char)('0' + quo);
    }
    *end-- = (char)('0' + n);
    return len;
}

static size_t u64toa_radix_d16(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    if (n < 16) {
        buf[0] = digits36[n];
        buf[1] = '\0';
        return 1;
    }
    size_t len = (64 - clz64(n) + 3) / 4;
    char *end = buf + len;
    *end-- = '\0';
    while (n >= 16) {
        uint32_t quo = n & 15;
        n >>= 4;
        *end-- = digits36[quo];
    }
    *end = digits36[n];
    return len;
}

#define u64toa_reverse_base(radix) \
static size_t u64toa_reverse_##radix(char buf[minimum_length(65)], uint64_t n, unsigned base) \
{ \
    char *end; \
    size_t len = 0; \
    while (n >= radix) { \
        size_t quo = n % radix; \
        n /= radix; \
        if (radix > 9) buf[len++] = digits36[quo]; \
        else buf[len++] = (char)('0' + quo); \
    } \
    if (radix > 9) buf[len++] = digits36[n]; \
    else buf[len++] = (char)('0' + n); \
    buf[len] = '\0'; \
    for (end = buf + len - 1; buf < end;) { \
        char c = *buf; \
        *buf++ = *end; \
        *end-- = c; \
    } \
    return len; \
}

u64toa_reverse_base(3)
u64toa_reverse_base(4)
u64toa_reverse_base(5)
u64toa_reverse_base(6)
u64toa_reverse_base(7)
u64toa_reverse_base(9)
u64toa_reverse_base(11)
u64toa_reverse_base(12)
u64toa_reverse_base(13)
u64toa_reverse_base(14)
u64toa_reverse_base(15)
u64toa_reverse_base(17)
u64toa_reverse_base(18)
u64toa_reverse_base(19)
u64toa_reverse_base(20)
u64toa_reverse_base(21)
u64toa_reverse_base(22)
u64toa_reverse_base(23)
u64toa_reverse_base(24)
u64toa_reverse_base(25)
u64toa_reverse_base(26)
u64toa_reverse_base(27)
u64toa_reverse_base(28)
u64toa_reverse_base(29)
u64toa_reverse_base(30)
u64toa_reverse_base(31)
u64toa_reverse_base(32)
u64toa_reverse_base(33)
u64toa_reverse_base(34)
u64toa_reverse_base(35)
u64toa_reverse_base(36)

typedef size_t (*u64toa_radix_func)(char buf[minimum_length(65)], uint64_t n, unsigned base);
static const u64toa_radix_func u64toa_radix_table[37] = {
#if 0
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_d2, u64toa_radix_reverse,
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse,
    u64toa_radix_d8, u64toa_radix_reverse, u64toa_radix_d10, u64toa_radix_reverse,
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse,
    u64toa_radix_d16, u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse,
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse,
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse,
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse,
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_reverse,
    u64toa_radix_reverse,
#else
    u64toa_radix_reverse, u64toa_radix_reverse, u64toa_radix_d2, u64toa_reverse_3,
    u64toa_reverse_4, u64toa_reverse_5, u64toa_reverse_6, u64toa_reverse_7,
    u64toa_radix_d8, u64toa_reverse_9, u64toa_radix_d10, u64toa_reverse_11,
    u64toa_reverse_12, u64toa_reverse_13, u64toa_reverse_14, u64toa_reverse_15,
    u64toa_radix_d16, u64toa_reverse_17, u64toa_reverse_18, u64toa_reverse_19,
    u64toa_reverse_20, u64toa_reverse_21, u64toa_reverse_22, u64toa_reverse_23,
    u64toa_reverse_24, u64toa_reverse_25, u64toa_reverse_26, u64toa_reverse_27,
    u64toa_reverse_28, u64toa_reverse_29, u64toa_reverse_30, u64toa_reverse_31,
    u64toa_reverse_32, u64toa_reverse_33, u64toa_reverse_34, u64toa_reverse_35,
    u64toa_reverse_36,
#endif
};

size_t u32toa_radix_dispatch(char buf[minimum_length(33)], uint32_t n, unsigned base)
{
    return u64toa_radix_table[base % 37](buf, n, base);
}

size_t u64toa_radix_dispatch(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    return u64toa_radix_table[base % 37](buf, n, base);
}

define_i32toa_radix(dispatch)
define_i64toa_radix(dispatch)

#endif // TEST_DISPATCH

#ifdef TEST_SNPRINTF
size_t u32toa_radix_snprintf(char buf[minimum_length(33)], uint32_t n, unsigned base)
{
    switch (base) {
#ifdef PRIb32
    case 2: return snprintf(buf, 33, "%"PRIb32, n);
#endif
    case 8: return snprintf(buf, 33, "%"PRIo32, n);
    case 10: return snprintf(buf, 33, "%"PRIu32, n);
    case 16: return snprintf(buf, 33, "%"PRIx32, n);
#ifdef TEST_NAIVE
    default: return u32toa_radix_naive(buf, n, base);
#else
    default: return u32toa_radix_reverse(buf, n, base);
#endif
    }
}

size_t u64toa_radix_snprintf(char buf[minimum_length(65)], uint64_t n, unsigned base)
{
    switch (base) {
#ifdef PRIb64
    case 2: return snprintf(buf, 65, "%"PRIb64, n);
#endif
    case 8: return snprintf(buf, 65, "%"PRIo64, n);
    case 10: return snprintf(buf, 65, "%"PRIu64, n);
    case 16: return snprintf(buf, 65, "%"PRIx64, n);
#ifdef TEST_NAIVE
    default: return u64toa_radix_naive(buf, n, base);
#else
    default: return u64toa_radix_reverse(buf, n, base);
#endif
    }
}

define_i32toa_radix(snprintf)
define_i64toa_radix(snprintf)

#endif // TEST_SNPRINTF

/*---- Benchmarking framework ----*/

/* Benchmark various alternatives and bases:
   - u32toa(), u64toa(), i32toa(), i64toa()
   - u32toa_radix(), u64toa_radix(), i32toa_radix(), i64toa_radix()
   - different implementations: naive, ...
   - various sets of values
 */

struct {
    const char *name;
    int enabled;
    size_t (*u32toa)(char buf[minimum_length(11)], uint32_t n);
    size_t (*i32toa)(char buf[minimum_length(12)], int32_t n);
    size_t (*u64toa)(char buf[minimum_length(21)], uint64_t n);
    size_t (*i64toa)(char buf[minimum_length(22)], int64_t n);
} impl[] =
{
#define TESTCASE(v) { #v, 2, u32toa_##v, i32toa_##v, u64toa_##v, i64toa_##v }
#ifdef TEST_SNPRINTF
    TESTCASE(snprintf),
#endif
#ifdef TEST_NAIVE
    TESTCASE(naive),
#endif
#ifdef TEST_BLOCKMOV
    TESTCASE(blockmov),
#endif
#ifdef TEST_REVERSE
    TESTCASE(reverse),
#endif
#ifdef TEST_LENGTH_EXPR
    TESTCASE(length_expr),
#endif
#ifdef TEST_LENGTH_LOOP
    TESTCASE(length_loop),
#endif
#ifdef TEST_SHIFTBUF
    TESTCASE(shift),
#endif
#ifdef TEST_DIGIT_PAIRS
    TESTCASE(pair),
#endif
#ifdef TEST_DIGIT_1PASS
    TESTCASE(pair_1pass),
#endif
#undef TESTCASE
};

struct {
    const char *name;
    int enabled;
    size_t (*u32toa_radix)(char buf[minimum_length(33)], uint32_t n, unsigned base);
    size_t (*i32toa_radix)(char buf[minimum_length(34)], int32_t n, unsigned base);
    size_t (*u64toa_radix)(char buf[minimum_length(65)], uint64_t n, unsigned base);
    size_t (*i64toa_radix)(char buf[minimum_length(66)], int64_t n, unsigned base);
} impl1[] =
{
#define TESTCASE(v) { #v, 2, u32toa_radix_##v, i32toa_radix_##v, u64toa_radix_##v, i64toa_radix_##v }
#ifdef TEST_SNPRINTF
    TESTCASE(snprintf),
#endif
#ifdef TEST_NAIVE
    TESTCASE(naive),
#endif
#ifdef TEST_REVERSE
    TESTCASE(reverse),
#endif
#ifdef TEST_DIV_TABLE
    TESTCASE(div_table),
#endif
#ifdef TEST_LENGTH_LOOP
    TESTCASE(length),
#endif
#ifdef TEST_DISPATCH
    TESTCASE(dispatch),
#endif
#undef TESTCASE
};

static clock_t start_timer(void) {
    clock_t t0, t;
    t0 = clock();
    // wait for next transition
    while ((t = clock()) == t0)
        continue;
    return t;
}

static clock_t stop_timer(clock_t t) {
    return clock() - t;
}

#define TIME(time, iter, s, w, e) {                                     \
        t = start_timer();                                              \
        uint64_t r0 = 0;                                                \
        for (int nn = 0; nn < iter; nn++) {                             \
            r0 = r0 * 1103515245 + 12345;                               \
            uint##w##_t mask, r = r0;                                   \
            for (int kk = 0; kk < 64; kk += w) {                        \
                for (mask = 1; mask != 0; ) {                           \
                    mask += mask;                                       \
                    if (s) {                                            \
                        int##w##_t x = (r & (mask - 1)) - (r & mask);   \
                        e;                                              \
                    } else {                                            \
                        uint##w##_t x = r & (mask - 1);                 \
                        e;                                              \
                    }                                                   \
                }                                                       \
                r ^= (r >> 1);                                          \
            }                                                           \
        }                                                               \
        t = stop_timer(t);                                              \
        if (time == 0 || time > t)                                      \
            time = t;                                                   \
    }

#define CHECK(tab, iter, s, w, e, check) {                              \
        uint64_t r0 = 0;                                                \
        for (int nn = 0; nn < iter; nn++) {                             \
            r0 = r0 * 1103515245 + 12345;                               \
            uint##w##_t mask, r = r0;                                   \
            for (int kk = 0; kk < 64; kk += w) {                        \
                for (mask = 1; mask != 0; ) {                           \
                    mask += mask;                                       \
                    if (s) {                                            \
                        int##w##_t x = (r & (mask - 1)) - (r & mask);   \
                        size_t len = tab.e;                             \
                        errno = 0;                                      \
                        int64_t y = check;                              \
                        if (errno || x != y || len != strlen(buf)) {    \
                            printf("error: %.*s_%s(%lld) base=%d -> %s -> %lld (errno=%d)\n", \
                                   (int)strcspn(#e, "("), #e, tab.name, \
                                   (long long)x, base, buf, (long long)y, errno); \
                            if (nerrors > 20) exit(1); \
                        }                                               \
                    } else {                                            \
                        uint##w##_t x = r & (mask - 1);                 \
                        size_t len = tab.e;                             \
                        errno = 0;                                      \
                        uint64_t y = check;                             \
                        if (errno || x != y || len != strlen(buf)) {    \
                            printf("error: %.*s_%s(%llu) base=%d -> %s -> %llu (errno=%d)\n", \
                                   (int)strcspn(#e, "("), #e, tab.name, \
                                   (unsigned long long)x, base, buf, (unsigned long long)y, errno); \
                            if (nerrors > 20) exit(1); \
                        }                                               \
                    }                                                   \
                }                                                       \
                r ^= (r >> 1);                                          \
            }                                                           \
        }                                                               \
    }

void show_usage(void) {
    printf("usage: test_conv [options] [bases] [filters]\n"
           "  options:\n"
           "    -h  --help     output this help\n"
           "    -t  --terse    only output average stats\n"
           "    -v  --verbose  output stats for all tested bases\n"
           "  bases\n"
           "     bases can be specified individually, as ranges or enumerations\n"
           "     supported bases are 2-36\n"
           "     examples:  10  2,8,16  2-10,16\n"
           "  filters are words that must be contained in variant names\n"
           "     examples:  naive  pri  len  rev\n"
           "  variants:\n"
#ifdef TEST_SNPRINTF
           "    snprintf     use snprintf for supported bases for reference\n"
#endif
#ifdef TEST_NAIVE
           "    naive        naive digit loop and copy loops\n"
#endif
#ifdef TEST_BLOCKMOV
           "    blockmov     same but move all digits together\n"
#endif
#ifdef TEST_REVERSE
           "    reverse      naive digit loop and reverse digit string\n"
#endif
#ifdef TEST_LENGTH_LOOP
           "    length_loop  compute length before digit loop using loop\n"
#endif
#ifdef TEST_LENGTH_EXPR
           "    length_expr  compute length before digit loop using expression\n"
#endif
#ifdef TEST_SHIFTBUF
           "    shift        generate up to 7 digit chunks in a register\n"
#endif
#ifdef TEST_DIGIT_PAIRS
           "    pair         generate 2 decimal digits at a time\n"
#endif
#ifdef TEST_DIGIT_1PASS
           "    pair_1pass   same but as a single left to right pass\n"
#endif
#ifdef TEST_DIV_TABLE
           "    div_table    use multiplier table instead of radix divisions\n"
#endif
#ifdef TEST_DISPATCH
           "    dispatch     use dispatch table to optimal 64-bit radix converters\n"
#endif
           );
}

int main(int argc, char *argv[]) {
    clock_t t;
    clock_t times[countof(impl)][4];
    clock_t times1[countof(impl1)][4][37];
    char buf[100];
    uint64_t bases = 0;
#define set_base(bases, b)  (*(bases) |= (1ULL << (b)))
#define has_base(bases, b)  ((bases) & (1ULL << (b)))
#define single_base(bases)  (!((bases) & ((bases) - 1)))
    int verbose = 0;
    int average = 1;
    int enabled = 3;

    memset(times, 0, sizeof times);
    memset(times1, 0, sizeof times1);

    for (int a = 1; a < argc; a++) {
        char *arg = argv[a];
        if (isdigit((unsigned char)*arg)) {
            while (isdigit((unsigned char)*arg)) {
                int b1 = strtol(arg, &arg, 10);
                set_base(&bases, b1);
                if (*arg == '-') {
                    int b2 = strtol(arg + 1, &arg, 10);
                    while (++b1 <= b2)
                        set_base(&bases, b1);
                }
                if (*arg == ',') {
                    arg++;
                    continue;
                }
            }
            if (*arg) {
                fprintf(stderr, "invalid option syntax: %s\n", argv[a]);
                return 2;
            }
            continue;
        } else if (!strcmp(arg, "-t") || !strcmp(arg, "--terse")) {
            verbose = 0;
            average = 1;
            continue;
        } else if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose")) {
            verbose = 1;
            continue;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "-?") || !strcmp(arg, "--help")) {
            show_usage();
            return 0;
        } else {
            int found = 0;
            for (size_t i = 0; i < countof(impl); i++) {
                if (strstr(impl[i].name, arg)) {
                    impl[i].enabled = 1;
                    found = 1;
                }
            }
            for (size_t i = 0; i < countof(impl1); i++) {
                if (strstr(impl1[i].name, arg)) {
                    impl1[i].enabled = 1;
                    found = 1;
                }
            }
            if (!found) {
                fprintf(stderr, "no variant for filter: %s\n", argv[a]);
                return 2;
            }
            enabled = 1;
            continue;
        }
    }
    if (!bases)
        bases = -1;
    if (single_base(bases)) {
        average = 0;
        verbose = 1;
    } else {
        average = 1;
    }

    int numvariant = 0;
    int numvariant1 = 0;
    int nerrors = 0;

    /* Checking for correctness */
    if (has_base(bases, 10)) {
        for (size_t i = 0; i < countof(impl); i++) {
            unsigned base = 10;
            if (impl[i].enabled & enabled) {
                CHECK(impl[i], 100000, 0, 32, u32toa(buf, x), strtoull(buf, NULL, 10));
                CHECK(impl[i], 100000, 1, 32, i32toa(buf, x), strtoll(buf, NULL, 10));
                CHECK(impl[i], 100000, 0, 64, u64toa(buf, x), strtoull(buf, NULL, 10));
                CHECK(impl[i], 100000, 1, 64, i64toa(buf, x), strtoll(buf, NULL, 10));
            }
        }
    }
    for (size_t i = 0; i < countof(impl1); i++) {
        if (impl1[i].enabled & enabled) {
            for (unsigned base = 2; base <= 36; base++) {
                if (has_base(bases, base)) {
                    CHECK(impl1[i], 1000, 0, 32, u32toa_radix(buf, x, base), strtoull(buf, NULL, base));
                    CHECK(impl1[i], 1000, 1, 32, i32toa_radix(buf, x, base), strtoll(buf, NULL, base));
                    CHECK(impl1[i], 1000, 0, 64, u64toa_radix(buf, x, base), strtoull(buf, NULL, base));
                    CHECK(impl1[i], 1000, 1, 64, i64toa_radix(buf, x, base), strtoll(buf, NULL, base));
                }
            }
        }
    }
    if (nerrors)
        return 1;

    /* Timing conversions */
    if (has_base(bases, 10)) {
        for (int rep = 0; rep < 100; rep++) {
            for (size_t i = 0; i < countof(impl); i++) {
                if (impl[i].enabled & enabled) {
                    numvariant++;
#ifdef TEST_SNPRINTF
                    if (strstr(impl[i].name, "snprintf")) { // avoid wrapper overhead
                        TIME(times[i][0], 1000, 0, 32, snprintf(buf, 11, "%"PRIu32, x));
                        TIME(times[i][1], 1000, 1, 32, snprintf(buf, 12, "%"PRIi32, x));
                        TIME(times[i][2], 1000, 0, 64, snprintf(buf, 21, "%"PRIu64, x));
                        TIME(times[i][3], 1000, 1, 64, snprintf(buf, 22, "%"PRIi64, x));
                    } else
#endif
                    {
                        TIME(times[i][0], 1000, 0, 32, impl[i].u32toa(buf, x));
                        TIME(times[i][1], 1000, 1, 32, impl[i].i32toa(buf, x));
                        TIME(times[i][2], 1000, 0, 64, impl[i].u64toa(buf, x));
                        TIME(times[i][3], 1000, 1, 64, impl[i].i64toa(buf, x));
                    }
                }
            }
        }
    }
    for (int rep = 0; rep < 10; rep++) {
        for (size_t i = 0; i < countof(impl1); i++) {
            if (impl1[i].enabled & enabled) {
                numvariant1++;
#ifdef TEST_SNPRINTF
                if (strstr(impl[i].name, "snprintf")) { // avoid wrapper overhead
#ifdef PRIb32
                    if (has_base(bases, 2)) {
                        unsigned base = 2;
                        TIME(times1[i][0][base], 1000, 0, 32, snprintf(buf, 33, "%"PRIb32, x));
                        TIME(times1[i][1][base], 1000, 1, 32, impl1[i].i32toa_radix(buf, x, base));
                        TIME(times1[i][2][base], 1000, 0, 64, snprintf(buf, 65, "%"PRIb64, x));
                        TIME(times1[i][3][base], 1000, 1, 64, impl1[i].i64toa_radix(buf, x, base));
                    }
#endif
                    if (has_base(bases, 8)) {
                        unsigned base = 8;
                        TIME(times1[i][0][base], 1000, 0, 32, snprintf(buf, 33, "%"PRIo32, x));
                        TIME(times1[i][1][base], 1000, 1, 32, impl1[i].i32toa_radix(buf, x, base));
                        TIME(times1[i][2][base], 1000, 0, 64, snprintf(buf, 65, "%"PRIo64, x));
                        TIME(times1[i][3][base], 1000, 1, 64, impl1[i].i64toa_radix(buf, x, base));
                    }
                    if (has_base(bases, 10)) {
                        unsigned base = 10;
                        TIME(times1[i][0][base], 1000, 0, 32, snprintf(buf, 33, "%"PRIu32, x));
                        TIME(times1[i][1][base], 1000, 1, 32, snprintf(buf, 34, "%"PRIi32, x));
                        TIME(times1[i][2][base], 1000, 0, 64, snprintf(buf, 64, "%"PRIu64, x));
                        TIME(times1[i][3][base], 1000, 1, 64, snprintf(buf, 65, "%"PRIi64, x));
                    }
                    if (has_base(bases, 16)) {
                        unsigned base = 16;
                        TIME(times1[i][0][base], 1000, 0, 32, snprintf(buf, 33, "%"PRIx32, x));
                        TIME(times1[i][1][base], 1000, 1, 32, impl1[i].i32toa_radix(buf, x, base));
                        TIME(times1[i][2][base], 1000, 0, 64, snprintf(buf, 65, "%"PRIx64, x));
                        TIME(times1[i][3][base], 1000, 1, 64, impl1[i].i64toa_radix(buf, x, base));
                    }
                } else
#endif
                {
                    for (unsigned base = 2; base <= 36; base++) {
                        if (has_base(bases, base)) {
                            TIME(times1[i][0][base], 1000, 0, 32, impl1[i].u32toa_radix(buf, x, base));
                            TIME(times1[i][1][base], 1000, 1, 32, impl1[i].i32toa_radix(buf, x, base));
                            TIME(times1[i][2][base], 1000, 0, 64, impl1[i].u64toa_radix(buf, x, base));
                            TIME(times1[i][3][base], 1000, 1, 64, impl1[i].i64toa_radix(buf, x, base));
                        }
                    }
                }
            }
        }
    }

    if (numvariant) {
        printf("%13s %10s %12s %12s %12s\n",
               "variant", "u32toa", "i32toa", "u64toa", "i64toa");
        for (size_t i = 0; i < countof(impl); i++) {
            if (impl[i].enabled & enabled) {
                printf("%13s", impl[i].name);
                for (int j = 0; j < 4; j++)
                    printf("   %6.2fns  ", times[i][j] * (1e9 / 1000 / 64 / CLOCKS_PER_SEC));
                printf("\n");
            }
        }
    }
    if (numvariant1) {
        printf("%9s rx  %12s %12s %12s %12s\n",
               "variant", "u32toa_radix", "i32toa_radix", "u64toa_radix", "i64toa_radix");
        for (size_t i = 0; i < countof(impl1); i++) {
            int numbases = 0;
            for (unsigned base = 2; base <= 36; base++) {
                if (has_base(bases, base)) {
                    if (times1[i][0][base]) {
                        numbases++;
                        for (int j = 0; j < 4; j++)
                            times1[i][j][1] += times1[i][j][base];
                        if (verbose) {
                            printf("%9s %-3d", impl1[i].name, base);
                            for (int j = 0; j < 4; j++) {
                                printf("   %6.2fns  ", times1[i][j][base] * (1e9 / 1000 / 64 / CLOCKS_PER_SEC));
                            }
                            printf("\n");
                        }
                    }
                }
            }
            if (average && numbases) {
                printf("%9s avg", impl1[i].name);
                for (int j = 0; j < 4; j++) {
                    printf("   %6.2fns  ", times1[i][j][1] * (1e9 / 1000 / 64 / numbases / CLOCKS_PER_SEC));
                }
                printf("\n");
            }
        }
    }
    return 0;
}
