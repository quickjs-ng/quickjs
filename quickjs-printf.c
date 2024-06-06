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

#include <limits.h>

#ifdef TEST_QUICKJS
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "cutils.h"
//#include "cutils.c"
#include "quickjs-printf.h"
typedef union JSFloat64Union {
    double d;
    uint64_t u64;
    uint32_t u32[2];
} JSFloat64Union;
#define JS_GET_CTX_RT(ctx)  0
#define JS_GET_RT_RT(rt)    0
#define JS_GET_DBUF_RT(s)   0
//#define js_malloc_rt(rt, size)  malloc(size)
//#define js_free_rt(rt, ptr)     free(ptr)
#else
#define JS_GET_RT_RT(rt)    (rt)
#define JS_GET_CTX_RT(ctx)  ((ctx)->rt)
#define JS_GET_DBUF_RT(s)   ((s)->opaque)
#endif

/* Rounding modes: There are six possible rounding modes for values
   exactly half way between 2 numbers:            1.5  2.5  -1.5  -2.5
   ROUND_HALF_UP    Round away from zero:         2    3    -2    -3
   ROUND_HALF_DOWN  Round towards zero:           1    2    -1    -2
   ROUND_HALF_EVEN  Round to nearest even value:  2    2    -2    -2
   ROUND_HALF_ODD   Round to nearest odd value:   1    3    -1    -3
   ROUND_HALF_NEXT  Round toward +Infinity:       2    3    -1    -2
   ROUND_HALF_PREV  Round toward -Infinity:       1    2    -2    -3
   ECMA specifies rounding as ROUND_HALF_UP.
   Standard C printf specifies rounding should be performed according
   to fegetround(), which defaults to FE_TONEAREST, which should
   correspond to ROUND_HALF_EVEN.
   This implementation only supports the first 4 modes.
 */
enum {
    FLAG_ROUND_HALF_ODD  = 0,
    FLAG_ROUND_HALF_EVEN = 1,
    FLAG_ROUND_HALF_UP   = 2,
    FLAG_ROUND_HALF_NEXT = 3,
    FLAG_ROUND_HALF_DOWN = 4,
    FLAG_ROUND_HALF_PREV = 5,
    FLAG_STRIP_ZEROES    = 0x10,
    FLAG_FORCE_DOT       = 0x20,
};

typedef struct JSFormatContext {
    JSRuntime *rt;
    void *ptr;
    size_t size, pos;
    int (*write)(struct JSFormatContext *fcp, const char *str, size_t len);
} JSFormatContext;

enum {
    FLAG_LEFT = 1, FLAG_HASH = 2, FLAG_ZERO = 4,
    FLAG_WIDTH = 8, FLAG_PREC = 16,
};

/*---- floating point conversions ----*/

// handle 9 decimal digits at a time */
#define COMP10            1000000000
#define COMP10_LEN        9
#define COMP10_MAX_SHIFT  34   // 64 - ceil(log2(1e9))

/* Initialize a bignum from a 64-bit unsigned int */
static size_t comp10_init(uint32_t *num, uint64_t mant) {
    size_t i = 0;
    while (mant >= COMP10) {
        num[i++] = mant % COMP10;
        mant /= COMP10;
    }
    num[i++] = (uint32_t)mant;
    return i;
}

/* Shift a bignum by a bit count in 0..COMP10_MAX_SHIFT */
static size_t comp10_shift(uint32_t *num, size_t plen, int shift) {
    uint64_t carry = 0;
    size_t i;
    for (i = 0; i < plen; i++) {
        carry += (uint64_t)num[i] << shift;
        num[i] = carry % COMP10;
        carry /= COMP10;
    }
    if (carry) {
        if (carry >= COMP10) {
            num[plen++] = carry % COMP10;
            carry /= COMP10;
        }
        num[plen++] = (uint32_t)carry;
    }
    return plen;
}

/* Multiply a bignum by a constant <= UINT32_MAX */
static size_t comp10_multiply(uint32_t *num, size_t plen, uint32_t mul) {
    uint64_t carry = 0;
    size_t i;
    for (i = 0; i < plen; i++) {
        carry += num[i] * (uint64_t)mul;
        num[i] = carry % COMP10;
        carry /= COMP10;
    }
    if (carry) {
        if (carry >= COMP10) {
            num[plen++] = carry % COMP10;
            carry /= COMP10;
        }
        num[plen++] = (uint32_t)carry;
    }
    return plen;
}

/* Compute the number of decimal digits in a normalized non-zero COMP10 unit */
static int digits_count(uint32_t val) {
    /* this code hopefully branchless */
    return (1 + (val > 9)
            + (val > 99)
            + (val > 999)
            + (val > 9999)
            + (val > 99999)
            + (val > 999999)
            + (val > 9999999)
#if COMP10_LEN > 8
            + (val > 99999999)
#endif
            );
}

/* Powers of 5 less than UINT32_MAX */
static uint32_t const pow5_table[14] = {
    1UL,
    5UL,
    5UL*5UL,
    5UL*5UL*5UL,
    5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL,
    5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL*5UL,
};

/* Powers of 10 less than UINT32_MAX */
static uint32_t const pow10_table[10] = {
    1UL,
    10UL,
    100UL,
    1000UL,
    10000UL,
    100000UL,
    1000000UL,
    10000000UL,
    100000000UL,
    1000000000UL,
};

/* Increment a bignum by a value `inc` times COMP10 power `from`,
   assuming from <= plen */
static size_t comp10_inc(uint32_t *num, size_t plen, uint32_t inc, size_t from) {
    uint32_t carry = inc;
    size_t i;
    for (i = from; i < plen; i++) {
        if ((num[i] += carry) < COMP10)
            return plen;
        num[i] -= COMP10;
        carry = 1;
    }
    if (carry) {
        num[plen++] = carry;
    }
    return plen;
}

// minimum buffer length is 1077 bytes for printf("%.1074f", 5e-324)
// including the null terminator
static int js_format_f(double value, char dest[minimum_length(2+1074+1)],
                       int prec, int use_exp,
                       int fflags, size_t *trailing_zeroes, int *pexp)
{
    uint32_t digits[(1022 + 52 + COMP10_LEN - 1 - 297) / COMP10_LEN];
    JSFloat64Union u;
    uint64_t mant;
    char *p1, *p = dest;
    size_t plen;
    int exp2, i, j, numd, maxd, spt;
    unsigned int val;

    /* assuming IEEE 64-bit format */
    u.d = value;
    exp2 = (int)((u.u64 >> 52U) & 0x07FFU) - 1023;
    mant = u.u64 & ((1ULL << 52U) - 1);
    // uncomment to support FLAG_ROUND_HALF_NEXT and FLAG_ROUND_HALF_PREV
    // fflags += ((fflags & 2) - (fflags & 4)) & -((int)(u.u64 >> 63) & fflags & 1);
    if (exp2 == -1023) {
        exp2++;
        if (mant == 0)
            goto has_zero;
    } else {
        mant |= (1ULL << 52U);
    }
    j = ctz64(mant);
    mant >>= j;
    exp2 -= 52 - j;
    if (exp2 >= 0) {
        /* integer value: compute mant * 2**exp2 */
#if 1
        j += 59 - 52;
        if (j >= exp2)
            j = exp2;
        mant <<= j;
        exp2 -= j;
#else
        if (exp2 + 52 - j < 64) {
            /* if value fits in a uint64_t, shift it */
            mant <<= exp2;
            exp2 = 0;
        }
#endif
        plen = comp10_init(digits, mant);
        while (exp2 > 0) {
            int k = exp2 >= COMP10_MAX_SHIFT ? COMP10_MAX_SHIFT : exp2;
            plen = comp10_shift(digits, plen, k);
            exp2 -= k;
        }
    } else {
        /* fractional part: compute power of 2 and multiply by mant */
        /* this is faster than using 32-bit digits and multiply by 10**9 */
        /* -exp2 is the number of bits after the decimal point. */
        /* multiply mant by 5 to the power of -exp2 */
        int exp;
        plen = comp10_init(digits, mant);
        for (exp = -exp2; exp > 0;) {
            int k = exp >= 13 ? 13 : exp;
            plen = comp10_multiply(digits, plen, pow5_table[k]);
            exp -= k;
        }
    }
    /* round converted number according to prec:
       if !use_exp, the number is an integer, no rounding necessary
       if use_exp, add 5*pow(10, numd-1-prec) unless numd==1+prec and
       digit 1 has even parity.
     */
    val = digits[--plen]; // leading digits: 1..COMP10_LEN
    j = digits_count(val);
    numd = j + plen * COMP10_LEN;  // number of significant digits
    i = numd + exp2;      // number of digits before the .

    if (use_exp) {
        maxd = prec + 1;
    } else {
        maxd = prec + i;
        /* if maxd < 0, the result is 0 */
        if (maxd < 0) {
        has_zero:
            *dest = '0';
            *pexp = 0;
            i = 1;
            if (fflags & FLAG_STRIP_ZEROES)
                prec = 0;
            *trailing_zeroes = prec;
            if (prec || (fflags & FLAG_FORCE_DOT))
                dest[i++] = '.';
            return i;
        }
    }
    if (maxd < numd) {
        /* round converted number to maxd digits */
        unsigned int trunc = numd - maxd;
        /* add 0.5 * 10**trunc unless rounding to even */
        // initialize trail to non zero if FLAG_ROUND_HALF_UP
        uint32_t inc, half, low, trail = fflags & FLAG_ROUND_HALF_UP;
        size_t start = 0;
        while (trunc > COMP10_LEN) {
            trail |= digits[start++];
            trunc -= COMP10_LEN;
        }
        inc = pow10_table[trunc]; // trunc is in range 1..COMP10_LEN
        half = inc / 2;
        low = digits[start] % inc;
        // round to nearest, tie to even
        if (low > half ||
            (low == half && !(fflags & FLAG_ROUND_HALF_DOWN) &&
             (trail ||
              (trunc == COMP10_LEN ?
               digits[start + 1] % 2 == (fflags & FLAG_ROUND_HALF_EVEN) :
               digits[start] / inc % 2 == (fflags & FLAG_ROUND_HALF_EVEN)))))
        {
            /* add inc to the number */
            plen = comp10_inc(digits, plen + 1, inc, start) - 1;
            if (val != digits[plen]) {
                val = digits[plen];
                j = digits_count(val);
                numd = j + plen * COMP10_LEN;
                i = numd + exp2;
                if (use_exp) {
                    maxd = prec + 1;
                } else {
                    maxd = prec + i;
                }
            }
        }
    } else {
        maxd = numd;
    }
    if (use_exp) {
        spt = 1;
        *pexp = i - 1;
    } else {
        while (i <= 0) {
            *p++ = '0';
            i++;
        }
        spt = i;
        *pexp = 0;
    }
    /* only output maxd digits? */
    p1 = p += j;
    while (val > 9) {
        *--p1 = '0' + val % 10;
        val /= 10;
    }
    *--p1 = (char)('0' + val);
    maxd -= j;
    for (i = plen; maxd > 0 && i --> 0; maxd -= COMP10_LEN) {
        val = digits[i];
        p1 = p += COMP10_LEN;
        for (j = 0; j < COMP10_LEN - 1; j++) {
            *--p1 = '0' + val % 10;
            val /= 10;
        }
        *--p1 = (char)('0' + val);
    }
    p += maxd;
    i = p - dest;

    /* strip trailing zeroes after the decimal point */
    while (i > spt && dest[i - 1] == '0')
        i--;
    if (fflags & FLAG_STRIP_ZEROES)
        prec = i - spt;
    *trailing_zeroes = spt + prec - i;
    /* insert decimal point */
    if (prec || (fflags & FLAG_FORCE_DOT)) {
        for (j = ++i; j --> spt;)
            dest[j] = dest[j - 1];
        dest[spt] = '.';
    }
    return i;
}

static int js_format_a(double d, char dest[minimum_length(2+13+6+1)],
                       int prec, const char *digits,
                       int fflags, size_t *trailing_zeroes, int *pexp)
{
    JSFloat64Union u;
    int shift, exp2, ndig, zeroes, tzcount;
    uint64_t mant, half;
    char *p = dest;

    u.d = d;

    /* extract mantissa and binary exponent */
    mant = u.u64 & ((1ULL << 52) - 1);
    exp2 = (u.u64 >> 52) & 0x07FF;
    // uncomment to support FLAG_ROUND_HALF_NEXT and FLAG_ROUND_HALF_PREV
    // fflags += ((fflags & 2) - (fflags & 4)) & -((int)(u.u64 >> 63) & fflags & 1);
    if (exp2 == 0) {
        /* subnormal */
        ndig = 0;
        tzcount = 0;
        if (mant == 0)
            goto next;
        shift = clz64(mant) - 11;
        mant <<= shift;
        exp2 = 1 - shift;
    }
    mant |= 1ULL << 52;
    exp2 -= 1023;
    tzcount = ctz64(mant);
    ndig = 13 - (tzcount >> 2);
 next:
    *pexp = exp2;
    zeroes = 0;
    if (prec >= 0) {
        if (prec >= ndig) {
            zeroes = prec - ndig;
        } else {
            // round to nearest according to flags
            shift = 52 - prec * 4 - 1;
            ndig = prec;
            half = (1ULL << shift) -
                !(tzcount == shift && !(fflags & FLAG_ROUND_HALF_DOWN) &&
                  ((fflags & FLAG_ROUND_HALF_UP) ||
                   (mant >> (shift + 1)) % 2 == (fflags & FLAG_ROUND_HALF_EVEN)));
            mant += half;
        }
    }
    *trailing_zeroes = zeroes;
    *p++ = (char)('0' + (int)(mant >> 52));
    if ((fflags & FLAG_FORCE_DOT) | zeroes | ndig) {
        *p++ = '.';
        for (shift = 52 - 4; ndig --> 0; shift -= 4)
            *p++ = digits[(size_t)(mant >> shift) & 15];
    }
    return p - dest;
}

// construct the exponent. dest minimum length is 7 bytes
// (including space for the null terminator, which is not set here)
static size_t js_format_exp(char *dest, int exp, char pref, int min_digits) {
    size_t i, len;
    dest[0] = pref;
    dest[1] = '+';
    if (exp < 0) {
        dest[1] = '-';
        exp = -exp;
    }
    len = 3 + (exp >= 1000) + (exp >= 100) + (exp >= 10 || min_digits > 1);
    for (i = len; i --> 3;) {
        dest[i] = (char)('0' + exp % 10);
        exp /= 10;
    }
    dest[i] = (char)('0' + exp);
    return len;
}

#if 1
static size_t js_format_spaces(JSFormatContext *fcp, size_t count)
{
    static char const buf[16] = "                ";
    size_t len = count;

    while (count > 0) {
        size_t chunk = count < sizeof(buf) ? count : sizeof(buf);
        fcp->write(fcp, buf, chunk);
        count -= chunk;
    }
    return len;
}

static size_t js_format_zeroes(JSFormatContext *fcp, size_t count)
{
    static char const buf[16] = "0000000000000000";
    size_t len = count;

    while (count > 0) {
        size_t chunk = count < sizeof(buf) ? count : sizeof(buf);
        fcp->write(fcp, buf, chunk);
        count -= chunk;
    }
    return len;
}
#else
static size_t js_format_run(JSFormatContext *fcp, char c, size_t count)
{
    char buf[128];
    size_t len = 0;

    while (count > 0) {
        size_t chunk = count < sizeof(buf) ? count : sizeof(buf);
        memset(buf, c, chunk);
        fcp->write(fcp, buf, chunk);
        len += chunk;
        count -= chunk;
    }
    return len;
}
#define js_format_spaces(fcp, count) js_format_run(fcp, ' ', count)
#define js_format_zeroes(fcp, count) js_format_run(fcp, '0', count)
#endif

static int js_format_str(JSFormatContext *fcp, int flags, int width, int prec, const char *str)
{
    size_t slen, pad, pos;

    if (flags & FLAG_PREC) {
        // emulate slen = strnlen(str, prec);
        for (slen = 0; slen < (size_t)prec && str[slen]; slen++)
            continue;
    } else {
        slen = strlen(str);
    }

    pos = pad = 0;
    if (slen < (size_t)width) {
        pad = width - slen;
        if (!(flags & FLAG_LEFT)) {
            /* left pad with spaces */
            pos += js_format_spaces(fcp, pad);
            pad = 0;
        }
    }
    pos += fcp->write(fcp, str, slen);
    if (pad)
        pos += js_format_spaces(fcp, pad);
    return pos;
}

static int js_format_wstr(JSFormatContext *fcp, int flags, int width, int prec,
                          char *dest, size_t size, const wchar_t *wstr)
{
    size_t i, j, k, pos = 0;
    uint32_t c;

    if (!(flags & FLAG_PREC))
        prec = INT_MAX;
    pos = 0;
    if (width > 0) {
        // compute the converted string length
        for (i = j = 0; (c = wstr[i++]) != 0;) {
            if (sizeof(wchar_t) == 2 && is_hi_surrogate(c) && is_lo_surrogate(wstr[i]))
                c = from_surrogate(c, wstr[i++]);
            j += utf8_encode((uint8_t *)dest, c);
        }
        if (j < (size_t)width && !(flags & FLAG_LEFT)) {
            /* left pad with spaces */
            pos += js_format_spaces(fcp, width - j);
        }
    }

    for (i = j = 0; (c = wstr[i++]) != 0;) {
        if (sizeof(wchar_t) == 2 && is_hi_surrogate(c) && is_lo_surrogate(wstr[i]))
            c = from_surrogate(c, wstr[i++]);
        if (j + UTF8_CHAR_LEN_MAX > size) {
            pos += fcp->write(fcp, dest, j);
            j = 0;
        }
        k = utf8_encode((uint8_t *)dest + j, c);
        if ((size_t)prec < k)
            break;
        prec -= k;
        j += k;
    }
    pos += fcp->write(fcp, dest, j);
    if (pos < (size_t)width)
        pos += js_format_spaces(fcp, width - pos);
    return pos;
}

static int js_format(JSFormatContext *fcp, const char *fmt, va_list ap)
{
    char buf[1080]; // used for integer and floating point conversions
    char prefix[4]; // sign and/or 0x, 0X, 0b or 0B prefixes
    char suffix[8]; // floating point exponent, range 'p-1074 to p+1023'
    size_t pos = 0, i, slen, prefix_len, suffix_len, leading_zeroes, trailing_zeroes, prec, pad;
    const char *str;
    const char *digits;
    char cc, lc;
    int width, flags, length, wc, shift;
    unsigned ww;
    uint64_t uval, signbit, mask;
    double val;
    int exp, fprec, fflags;

    str = fmt;
    for (;;) {
        cc = *fmt++;
        if (cc != '%' && cc != '\0')
            continue;
        slen = fmt - str - 1;
        if (slen)
            pos += fcp->write(fcp, str, slen);
        if (cc == '\0')
            break;
        /* quick dispatch for special and common formats */
        switch (*fmt) {
        case '%':
            str = fmt++;
            continue;
        case 'd':
            str = buf;
            slen = i32toa(buf, va_arg(ap, int));
            goto hasstr;
        case 's':
            str = va_arg(ap, const char *);
            if (!str)
                str = "(null)";
            slen = strlen(str);
        hasstr:
            pos += fcp->write(fcp, str, slen);
            str = ++fmt;
            continue;
        }
        prefix[0] = '\0';
        flags = 0;
        length = sizeof(unsigned int) * CHAR_BIT;
        str = fmt - 1;
        prefix_len = leading_zeroes = slen = trailing_zeroes = suffix_len = prec = width = 0;
    moreflags:
        switch (cc = *fmt) {
        case ' ':
        case '+':   fmt++; prefix[0] |= cc; goto moreflags; /* assuming ASCII */
        case '-':   fmt++; flags |= FLAG_LEFT; goto moreflags;
        case '#':   fmt++; flags |= FLAG_HASH; goto moreflags;
        case '0':   fmt++; flags |= FLAG_ZERO; goto moreflags;
        case '*':
            //flags |= FLAG_WIDTH;
            fmt++;
            wc = va_arg(ap, int);
            if (wc < 0) {
                flags |= FLAG_LEFT;
                if (wc != INT_MIN)
                    width = -wc;
            } else {
                width = wc;
            }
            if (*fmt == '.')
                goto hasprec;
            break;
        case '1': case '2': case '3':
        case '4': case '5': case '6':
        case '7': case '8': case '9':
            ww = *fmt++ - '0';
            //flags |= FLAG_WIDTH;
            while (*fmt >= '0' && *fmt <= '9') {
                unsigned digit = *fmt++ - '0';
                if (ww >= UINT_MAX / 10 &&
                    (ww > UINT_MAX / 10 || digit > UINT_MAX % 10))
                    ww = UINT_MAX;
                else
                    ww = ww * 10 + digit;
            }
            if (ww <= INT_MAX)
                width = ww;
            if (*fmt == '.')
                goto hasprec;
            break;
        case '.':
        hasprec:
            fmt++;
            if (*fmt == '*') {
                fmt++;
                wc = va_arg(ap, int);
                if (wc >= 0) {
                    flags |= FLAG_PREC;
                    prec = wc;
                }
            } else {
                ww = 0;
                while (*fmt >= '0' && *fmt <= '9') {
                    unsigned digit = *fmt++ - '0';
                    if (ww >= UINT_MAX / 10 &&
                        (ww > UINT_MAX / 10 || digit > UINT_MAX % 10))
                        ww = UINT_MAX;
                    else
                        ww = ww * 10 + digit;
                }
                if (ww <= INT_MAX) {
                    flags |= FLAG_PREC;
                    prec = ww;
                }
            }
            break;
        }
        switch (lc = *fmt++) {
        case 'j':
            length = sizeof(intmax_t) * CHAR_BIT;
            break;
        case 'z':
            length = sizeof(size_t) * CHAR_BIT;
            break;
        case 't':
            length = sizeof(ptrdiff_t) * CHAR_BIT;
            break;
        case 'l':
            length = sizeof(unsigned long) * CHAR_BIT;
            if (*fmt == 'l') {
                fmt++;
                length = sizeof(unsigned long long) * CHAR_BIT;
            }
            break;
        case 'h':
            length = sizeof(unsigned short) * CHAR_BIT;
            if (*fmt == 'h') {
                fmt++;
                length = sizeof(unsigned char) * CHAR_BIT;
            }
            break;
        case 'w':
            if (!(*fmt >= '1' && *fmt <= '9'))
                goto error;
            length = *fmt++ - '0';
            if (*fmt >= '0' && *fmt <= '9')
                length = length * 10 + *fmt++ - '0';
            if (length > 64)
                goto error;
            break;
        default:
            fmt--;
            break;
        }
        digits = digits36;
        switch (cc = *fmt++) {
        case 's':
            if (lc == 'l') {
                const wchar_t *wstr = va_arg(ap, const wchar_t *);
                if (!wstr)
                    wstr = L"(null)";
                pos += js_format_wstr(fcp, flags, width, prec,
                                      buf, sizeof buf, wstr);
            } else {
                str = va_arg(ap, const char *);
                if (!str)
                    str = "(null)";
                pos += js_format_str(fcp, flags, width, prec, str);
            }
            str = fmt;
            continue;
        case 'c':
            flags &= ~FLAG_ZERO;
            wc = va_arg(ap, int);
            if (lc == 'l') {
                slen = 0;
                if (wc)
                    slen = utf8_encode((uint8_t *)buf, wc);
            } else {
                *buf = (char)wc;
                slen = 1;
            }
            break;
        case 'p':
#ifndef TEST_QUICKJS
            if (*fmt == 's' && fcp->rt) {
                // %ps -> JSString converted to quoted string
                // TODO(chqrlie) allocate buffer if conversion does not fit
                JSString *pstr = va_arg(ap, void *);
                JS_FormatString(fcp->rt, buf, sizeof(buf), pstr, '"');
                pos += js_format_str(fcp, flags, width, prec, buf);
                str = ++fmt;
                continue;
            }
#endif
            shift = 4;
            prefix[0] = '0';
            prefix[1] = cc = 'x';
            prefix_len = 2;
            length = sizeof(void *) * CHAR_BIT;
            goto radix_number;
        case 'B':
            digits = digits36_upper;
            /* fall thru */
        case 'b':
            shift = 1;
            goto radix_number;
        case 'X':
            digits = digits36_upper;
            /* fall thru */
        case 'x':
            shift = 4;
            goto radix_number;
        case 'o':
#ifndef TEST_QUICKJS
            if (*fmt == 'a' && length == sizeof(JSAtom) * CHAR_BIT && fcp->rt) {
                // %oa -> JSAtom converted to utf-8 string
                // %#oa -> JSAtom converted to identifier, number or quoted string
                // TODO(chqrlie) allocate buffer if conversion does not fit
                JSAtom atom = va_arg(ap, unsigned);
                JS_FormatAtom(fcp->rt, buf, sizeof(buf), atom, flags & FLAG_HASH);
                pos += js_format_str(fcp, flags, width, prec, buf);
                str = ++fmt;
                continue;
            }
#endif
            shift = 3;
        radix_number:
            uval = (length <= 32) ? va_arg(ap, uint32_t) : va_arg(ap, uint64_t);
            signbit = (uint64_t)1 << (length - 1);
            /* mask off extra bits, keep value bits */
            uval &= (signbit << 1) - 1;
            slen = uval ? (64 - clz64(uval) + shift - 1) / shift : 1;
            mask = (1ULL << shift) - 1;
            for (wc = slen * shift, i = 0; wc > 0; i++) {
                wc -= shift;
                buf[i] = digits[(uval >> wc) & mask];
            }
            if (flags & FLAG_PREC) {
                if (prec == 0 && uval == 0)
                    slen = 0;
                if (slen < prec)
                    leading_zeroes = prec - slen;
                flags &= ~FLAG_ZERO;
            }
            if (flags & FLAG_HASH) {
                if (shift == 3) {
                    /* at least one leading `0` */
                    if (leading_zeroes == 0 && (uval != 0 || slen == 0))
                        leading_zeroes = 1;
                } else {
                    if (uval) {
                        /* output a 0x/0X or 0b/0B prefix */
                        prefix[0] = '0';
                        prefix[1] = cc;
                        prefix_len = 2;
                    }
                }
            }
            break;
        case 'u':
        case 'd':
        case 'i':
            uval = (length <= 32) ? va_arg(ap, uint32_t) : va_arg(ap, uint64_t);
            signbit = 1ULL << (length - 1);
            if (cc != 'u') {
                if (uval & signbit) {
                    prefix[0] = '-';
                    uval = -uval;
                }
                prefix_len = (prefix[0] != '\0');
            }
            /* mask off extra bits, keep value bits */
            uval &= (signbit << 1) - 1;
            slen = u64toa(buf, uval);
            if (flags & FLAG_PREC) {
                if (prec == 0 && uval == 0)
                    slen = 0;
                if (slen < prec)
                    leading_zeroes = prec - slen;
                flags &= ~FLAG_ZERO;
            }
            break;
        case 'A':
        case 'E':
        case 'F':
        case 'G':
            cc += 'a' - 'A';
            digits = digits36_upper;
            /* fall through */
        case 'a':
        case 'e':
        case 'f':
        case 'g':
            val = va_arg(ap, double);
            fflags = FLAG_ROUND_HALF_EVEN;
            fprec = -1;
            if (flags & FLAG_PREC)
                fprec = prec;
#define LETTER(c)  digits[(c) - 'a' + 10]

            if (signbit(val)) {
                prefix[0] = '-';
                //val = -val;
            }
            prefix_len = (prefix[0] != '\0');
            if (!isfinite(val)) {
                flags &= ~FLAG_ZERO;
                slen = 3;
                buf[0] = LETTER('i');
                buf[1] = LETTER('n');
                buf[2] = LETTER('f');
                if (isnan(val)) {
                    prefix_len = 0;
                    buf[0] = LETTER('n');
                    buf[1] = LETTER('a');
                    buf[2] = LETTER('n');
                }
                break;
            }
            if (flags & FLAG_HASH)
                fflags |= FLAG_FORCE_DOT;
            if (cc == 'a') {
                prefix[prefix_len++] = '0';
                prefix[prefix_len++] = LETTER('x');
                slen = js_format_a(val, buf, fprec, digits, fflags, &trailing_zeroes, &exp);
                suffix_len = js_format_exp(suffix, exp, LETTER('p'), 1);
                break;
            }
            if (fprec < 0)
                fprec = 6;
            if (cc == 'g') {
                fprec -= (fprec != 0);
                if (!(flags & FLAG_HASH))
                    fflags |= FLAG_STRIP_ZEROES;
                if (val != 0) {
                    exp = (int)+floor(log10(fabs(val)));
                    if (fprec <= exp || exp < -4) {
                        /* convert with exponent, then re-test border cases */
                        // TODO(chqrlie): avoid computing digits twice
                        slen = js_format_f(val, buf, fprec, TRUE, fflags,
                                           &trailing_zeroes, &exp);
                        if (fprec < exp || exp < -4) {
                            suffix_len = js_format_exp(suffix, exp, LETTER('e'), 2);
                            break;
                        }
                    }
                    fprec -= exp;
                }
            }
            slen = js_format_f(val, buf, fprec, cc == 'e', fflags,
                               &trailing_zeroes, &exp);
            if (cc == 'e') {
                suffix_len = js_format_exp(suffix, exp, LETTER('e'), 2);
            }
            break;
        case '\0':
            fmt--;
            continue;
        error:
        default:
            /* invalid format: stop conversions and print format string */
            fmt += strlen(fmt);
            continue;
        }
        pad = 0;
        // XXX: potential overflow
        wc = prefix_len + leading_zeroes + slen + trailing_zeroes + suffix_len;
        if (width > wc) {
            pad = width - wc;
            if (!(flags & FLAG_LEFT)) {
                if (flags & FLAG_ZERO) {
                    /* left pad with zeroes between prefix and string */
                    leading_zeroes += pad;
                    pad = 0;
                } else {
                    /* left pad with spaces */
                    pos += js_format_spaces(fcp, pad);
                    pad = 0;
                }
            }
        }
        /* output prefix: sign and/or 0x/0b */
        if (prefix_len)
            pos += fcp->write(fcp, prefix, prefix_len);
        if (leading_zeroes)
            pos += js_format_zeroes(fcp, leading_zeroes);
        /* output string fragment */
        pos += fcp->write(fcp, buf, slen);
        if (trailing_zeroes)
            pos += js_format_zeroes(fcp, trailing_zeroes);
        /* output suffix: exponent */
        if (suffix_len)
            pos += fcp->write(fcp, suffix, suffix_len);
        /* right pad with spaces */
        if (pad)
            pos += js_format_spaces(fcp, pad);
        str = fmt;
    }
    return (int)pos;
}

static int js_snprintf_write(JSFormatContext *fcp, const char *str, size_t len)
{
    size_t i, pos = fcp->pos;
    char *dest = fcp->ptr;
    for (i = 0; i < len; i++) {
        if (pos < fcp->size)
            dest[pos] = str[i];
        pos++;
    }
    fcp->pos = pos;
    return len;
}

int js_snprintf(JSContext *ctx, char *dest, size_t size, const char *fmt, ...)
{
    JSFormatContext fc = { JS_GET_CTX_RT(ctx), dest, size, 0, js_snprintf_write };
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = js_format(&fc, fmt, ap);
    va_end(ap);
    if (fc.pos < fc.size)
        dest[fc.pos] = '\0';
    else if (fc.size > 0)
        dest[fc.size - 1] = '\0';
    return len;
}

int js_vsnprintf(JSContext *ctx, char *dest, size_t size, const char *fmt, va_list ap)
{
    JSFormatContext fc = { JS_GET_CTX_RT(ctx), dest, size, 0, js_snprintf_write };
    int len;

    len = js_format(&fc, fmt, ap);
    if (fc.pos < fc.size)
        dest[fc.pos] = '\0';
    else if (fc.size > 0)
        dest[fc.size - 1] = '\0';
    return len;
}

static int dbuf_printf_write(JSFormatContext *fcp, const char *str, size_t len)
{
    return dbuf_put(fcp->ptr, str, len);
}

int dbuf_vprintf_ext(DynBuf *s, const char *fmt, va_list ap)
{
    JSFormatContext fc = { JS_GET_DBUF_RT(s), s, 0, 0, dbuf_printf_write };
    return js_format(&fc, fmt, ap);
}

static int js_fprintf_write(JSFormatContext *fcp, const char *str, size_t len)
{
    return fwrite(str, 1, len, fcp->ptr);
}

int js_printf(JSContext *ctx, const char *fmt, ...)
{
    JSFormatContext fc = { JS_GET_CTX_RT(ctx), stdout, 0, 0, js_fprintf_write };
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = js_format(&fc, fmt, ap);
    va_end(ap);
    return len;
}

int js_vprintf(JSContext *ctx, const char *fmt, va_list ap)
{
    JSFormatContext fc = { JS_GET_CTX_RT(ctx), stdout, 0, 0, js_fprintf_write };
    return js_format(&fc, fmt, ap);
}

int js_printf_RT(JSRuntime *rt, const char *fmt, ...)
{
    JSFormatContext fc = { JS_GET_RT_RT(rt), stdout, 0, 0, js_fprintf_write };
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = js_format(&fc, fmt, ap);
    va_end(ap);
    return len;
}

int js_vprintf_RT(JSRuntime *rt, const char *fmt, va_list ap)
{
    JSFormatContext fc = { JS_GET_RT_RT(rt), stdout, 0, 0, js_fprintf_write };
    return js_format(&fc, fmt, ap);
}

int js_fprintf_RT(JSRuntime *rt, FILE *fp, const char *fmt, ...)
{
    JSFormatContext fc = { JS_GET_RT_RT(rt), fp, 0, 0, js_fprintf_write };
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = js_format(&fc, fmt, ap);
    va_end(ap);
    return len;
}

int js_vfprintf_RT(JSRuntime *rt, FILE *fp, const char *fmt, va_list ap)
{
    JSFormatContext fc = { JS_GET_RT_RT(rt), fp, 0, 0, js_fprintf_write };
    return js_format(&fc, fmt, ap);
}

#undef JS_GET_CTX_RT
#undef JS_GET_RT_RT
#undef JS_GET_DBUF_RT
#undef COMP10
#undef COMP10_LEN
#undef COMP10_MAX_SHIFT
