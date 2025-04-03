/* INTERFACE TO FUNCTIONS FOR EXACT SUMMATION. */

/* Copyright 2015, 2018, 2021 Radford M. Neal

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef XSUM_H
#define XSUM_H

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>


/* CONSTANTS DEFINING THE FLOATING POINT FORMAT. */

typedef double xsum_flt;           /* C floating point type sums are done for */

typedef int64_t xsum_int;          /* Signed integer type for a fp value */
typedef uint64_t xsum_uint;        /* Unsigned integer type for a fp value */
typedef int_fast16_t xsum_expint;  /* Integer type for holding an exponent */

#define XSUM_MANTISSA_BITS 52      /* Bits in fp mantissa, excludes implict 1 */
#define XSUM_EXP_BITS 11           /* Bits in fp exponent */

#define XSUM_MANTISSA_MASK \
  (((xsum_int)1 << XSUM_MANTISSA_BITS) - 1)  /* Mask for mantissa bits */

#define XSUM_EXP_MASK \
  ((1 << XSUM_EXP_BITS) - 1)                 /* Mask for exponent */

#define XSUM_EXP_BIAS \
  ((1 << (XSUM_EXP_BITS-1)) - 1)             /* Bias added to signed exponent */

#define XSUM_SIGN_BIT \
  (XSUM_MANTISSA_BITS + XSUM_EXP_BITS)       /* Position of sign bit */

#define XSUM_SIGN_MASK \
  ((xsum_uint)1 << XSUM_SIGN_BIT)            /* Mask for sign bit */


/* CONSTANTS DEFINING THE SMALL ACCUMULATOR FORMAT. */

#define XSUM_SCHUNK_BITS 64        /* Bits in chunk of the small accumulator */
typedef int64_t xsum_schunk;       /* Integer type of small accumulator chunk */

#define XSUM_LOW_EXP_BITS 5        /* # of low bits of exponent, in one chunk */

#define XSUM_LOW_EXP_MASK \
  ((1 << XSUM_LOW_EXP_BITS) - 1)       /* Mask for low-order exponent bits */

#define XSUM_HIGH_EXP_BITS \
  (XSUM_EXP_BITS - XSUM_LOW_EXP_BITS)  /* # of high exponent bits for index */

#define XSUM_HIGH_EXP_MASK \
  ((1 << HIGH_EXP_BITS) - 1)           /* Mask for high-order exponent bits */

#define XSUM_SCHUNKS \
  ((1 << XSUM_HIGH_EXP_BITS) + 3)      /* # of chunks in small accumulator */

#define XSUM_LOW_MANTISSA_BITS \
  (1 << XSUM_LOW_EXP_BITS)             /* Bits in low part of mantissa */

#define XSUM_HIGH_MANTISSA_BITS \
  (XSUM_MANTISSA_BITS - XSUM_LOW_MANTISSA_BITS)  /* Bits in high part */

#define XSUM_LOW_MANTISSA_MASK \
  (((xsum_int)1 << XSUM_LOW_MANTISSA_BITS) - 1)  /* Mask for low bits */

#define XSUM_SMALL_CARRY_BITS \
 ((XSUM_SCHUNK_BITS-1) - XSUM_MANTISSA_BITS)     /* Bits sums can carry into */

#define XSUM_SMALL_CARRY_TERMS \
  ((1 << XSUM_SMALL_CARRY_BITS) - 1)   /* # terms can add before need prop. */

typedef struct
{ xsum_schunk chunk[XSUM_SCHUNKS]; /* Chunks making up small accumulator */
  xsum_int Inf;                    /* If non-zero, +Inf, -Inf, or NaN */
  xsum_int NaN;                    /* If non-zero, a NaN value with payload */
  int adds_until_propagate;        /* Number of remaining adds before carry */
} xsum_small_accumulator;          /*     propagation must be done again    */


/* TYPE FOR LENGTHS OF ARRAYS.  Must be a signed integer type.  Set to
   ptrdiff_t here on the assumption that this will be big enough, but
   not unnecessarily big, which seems to be true. */

typedef ptrdiff_t xsum_length;


/* FUNCTIONS FOR EXACT SUMMATION, WITH POSSIBLE DIVISION BY AN INTEGER. */

void xsum_small_init (xsum_small_accumulator *restrict);
void xsum_small_add1 (xsum_small_accumulator *restrict, xsum_flt);
void xsum_small_addv (xsum_small_accumulator *restrict,
                      const xsum_flt *restrict, xsum_length);
void xsum_small_add_sqnorm (xsum_small_accumulator *restrict,
                            const xsum_flt *restrict, xsum_length);
void xsum_small_add_dot (xsum_small_accumulator *restrict,
                         const xsum_flt *, const xsum_flt *, xsum_length);
void xsum_small_add_accumulator (xsum_small_accumulator *,
                                 xsum_small_accumulator *);
void xsum_small_negate (xsum_small_accumulator *restrict);
xsum_flt xsum_small_round (xsum_small_accumulator *restrict);

xsum_flt xsum_small_div_unsigned (xsum_small_accumulator *restrict, unsigned);
xsum_flt xsum_small_div_int (xsum_small_accumulator *restrict, int);


/* DEBUG FLAG.  Set to non-zero for debug ouptut.  Ignored unless xsum.c
   is compiled with -DDEBUG. */

extern int xsum_debug;

#endif
