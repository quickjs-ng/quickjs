/* FUNCTIONS FOR EXACT SUMMATION. */

/* Copyright 2015, 2018, 2021, 2024 Radford M. Neal

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

#include <string.h>
#include <math.h>
#include "xsum.h"


/* ---------------------- IMPLEMENTATION ASSUMPTIONS ----------------------- */

/* This code makes the following assumptions:

     o The 'double' type is a IEEE-754 standard 64-bit floating-point value.

     o The 'int64_t' and 'uint64_t' types exist, for 64-bit signed and
       unsigned integers.

     o The 'endianness' of 'double' and 64-bit integers is consistent
       between these types - that is, looking at the bits of a 'double'
       value as an 64-bit integer will have the expected result.

     o Right shifts of a signed operand produce the results expected for
       a two's complement representation.

     o Rounding should be done in the "round to nearest, ties to even" mode.
*/


/* --------------------------- CONFIGURATION ------------------------------- */


/* IMPLEMENTATION OPTIONS.  Can be set to either 0 or 1, whichever seems
   to be fastest. */

#define USE_SIMD 1          /* Use SIMD intrinsics (SSE2/AVX) if available?   */

#define USE_MEMSET_SMALL 1  /* Use memset rather than a loop (for small mem)? */

#define OPT_SMALL 0         /* Class of manual optimization for operations on */
                            /*   small accumulator: 0 (none), 1, 2, 3 (SIMD)  */
#define OPT_CARRY 1         /* Use manually optimized carry propagation?      */

#define INLINE_SMALL 1      /* Inline more of the small accumulator routines? */
                            /*   (Not currently used)                         */


/* INCLUDE INTEL INTRINSICS IF USED AND AVAILABLE. */

#if USE_SIMD && __SSE2__
# include <immintrin.h>
#endif


/* COPY A 64-BIT QUANTITY - DOUBLE TO 64-BIT INT OR VICE VERSA.  The
   arguments are destination and source variables (not values). */

#define COPY64(dst,src) memcpy(&(dst),&(src),sizeof(double))


/* SET UP DEBUG FLAG.  It's a variable if debuging is enabled, and a
   constant if disabled (so that no code will be generated then). */

int xsum_debug = 0;

#ifndef DEBUG
# define xsum_debug 0
#endif


/* SET UP INLINE / NOINLINE MACROS. */

#if __GNUC__
# define INLINE inline __attribute__ ((always_inline))
# define NOINLINE __attribute__ ((noinline))
#else
# define INLINE inline
# define NOINLINE
#endif


/* ------------------------ INTERNAL ROUTINES ------------------------------- */


/* ADD AN INF OR NAN TO A SMALL ACCUMULATOR.  This only changes the flags,
   not the chunks in the accumulator, which retains the sum of the finite
   terms (which is perhaps sometimes useful to access, though no function
   to do so is defined at present).  A NaN with larger payload (seen as a
   52-bit unsigned integer) takes precedence, with the sign of the NaN always
   being positive.  This ensures that the order of summing NaN values doesn't
   matter. */

static NOINLINE void xsum_small_add_inf_nan
                       (xsum_small_accumulator *restrict sacc, xsum_int ivalue)
{
  xsum_int mantissa;
  double fltv;

  mantissa = ivalue & XSUM_MANTISSA_MASK;

  if (mantissa == 0) /* Inf */
  { if (sacc->Inf == 0)
    { /* no previous Inf */
      sacc->Inf = ivalue;
    }
    else if (sacc->Inf != ivalue)
    { /* previous Inf was opposite sign */
      COPY64 (fltv, ivalue);
      fltv = fltv - fltv;  /* result will be a NaN */
      COPY64 (sacc->Inf, fltv);
    }
  }
  else /* NaN */
  { /* Choose the NaN with the bigger payload and clear its sign.  Using <=
       ensures that we will choose the first NaN over the previous zero. */
    if ((sacc->NaN & XSUM_MANTISSA_MASK) <= mantissa)
    { sacc->NaN = ivalue & ~XSUM_SIGN_MASK;
    }
  }
}


/* PROPAGATE CARRIES TO NEXT CHUNK IN A SMALL ACCUMULATOR.  Needs to
   be called often enough that accumulated carries don't overflow out
   the top, as indicated by sacc->adds_until_propagate.  Returns the
   index of the uppermost non-zero chunk (0 if number is zero).

   After carry propagation, the uppermost non-zero chunk will indicate
   the sign of the number, and will not be -1 (all 1s).  It will be in
   the range -2^XSUM_LOW_MANTISSA_BITS to 2^XSUM_LOW_MANTISSA_BITS - 1.
   Lower chunks will be non-negative, and in the range from 0 up to
   2^XSUM_LOW_MANTISSA_BITS - 1. */

static NOINLINE int xsum_carry_propagate (xsum_small_accumulator *restrict sacc)
{
  int i, u, uix;

  /* Set u to the index of the uppermost non-zero (for now) chunk, or
     return with value 0 if there is none. */

# if OPT_CARRY

  { u = XSUM_SCHUNKS-1;
    switch (XSUM_SCHUNKS & 0x3)   /* get u to be a multiple of 4 minus one  */
    {
      case 3: if (sacc->chunk[u] != 0)
              { goto found2;
              }
              u -= 1;                            /* XSUM_SCHUNKS is a */
      case 2: if (sacc->chunk[u] != 0)           /* constant, so the  */
              { goto found2;                     /* compiler will do  */
              }                                  /* simple code here  */
              u -= 1;
      case 1: if (sacc->chunk[u] != 0)
              { goto found2;
              }
              u -= 1;
      case 0: ;
    }

    do  /* here, u should be a multiple of 4 minus one, and at least 3 */
    {
#     if USE_SIMD && __AVX__
      { __m256i ch;
        ch = _mm256_loadu_si256 ((__m256i *)(sacc->chunk+u-3));
        if (!_mm256_testz_si256(ch,ch))
        { goto found;
        }
        u -= 4;
        if (u < 0)  /* never actually happens, because value of XSUM_SCHUNKS */
        { break;    /*   is such that u < 0 occurs at end of do loop instead */
        }
        ch = _mm256_loadu_si256 ((__m256i *)(sacc->chunk+u-3));
        if (!_mm256_testz_si256(ch,ch))
        { goto found;
        }
        u -= 4;
      }
#     else
      { if (sacc->chunk[u] | sacc->chunk[u-1]
          | sacc->chunk[u-2] | sacc->chunk[u-3])
        { goto found;
        }
        u -= 4;
      }
#     endif

    } while (u >= 0);

    uix = 0;
    goto done;

  found:
    if (sacc->chunk[u] != 0)
    { goto found2;
    }
    u -= 1;
    if (sacc->chunk[u] != 0)
    { goto found2;
    }
    u -= 1;
    if (sacc->chunk[u] != 0)
    { goto found2;
    }
    u -= 1;

   found2: ;
  }

# else  /* Non-optimized search for uppermost non-zero chunk */

  { for (u = XSUM_SCHUNKS-1; sacc->chunk[u] == 0; u--)
    { if (u == 0)
      {
        uix = 0;
        goto done;
      }
    }
  }

# endif

  /* At this point, sacc->chunk[u] must be non-zero */

  /* Carry propagate, starting at the low-order chunks.  Note that the
     loop limit of u may be increased inside the loop. */

  i = 0;     /* set to the index of the next non-zero chunck, from bottom */

# if OPT_CARRY
  {
    /* Quickly skip over unused low-order chunks.  Done here at the start
       on the theory that there are often many unused low-order chunks,
       justifying some overhead to begin, but later stretches of unused
       chunks may not be as large. */

    int e = u-3;  /* go only to 3 before so won't access beyond chunk array */

    do
    {
#     if USE_SIMD && __AVX__
      { __m256i ch;
        ch = _mm256_loadu_si256 ((__m256i *)(sacc->chunk+i));
        if (!_mm256_testz_si256(ch,ch))
        { break;
        }
        i += 4;
        if (i >= e)
        { break;
        }
        ch = _mm256_loadu_si256 ((__m256i *)(sacc->chunk+i));
        if (!_mm256_testz_si256(ch,ch))
        { break;
        }
      }
#     else
      { if (sacc->chunk[i] | sacc->chunk[i+1]
          | sacc->chunk[i+2] | sacc->chunk[i+3])
        { break;
        }
      }
#     endif

      i += 4;

    } while (i <= e);
  }
# endif

  uix = -1;  /* indicates that a non-zero chunk has not been found yet */

  do
  { xsum_schunk c;       /* Set to the chunk at index i (next non-zero one) */
    xsum_schunk clow;    /* Low-order bits of c */
    xsum_schunk chigh;   /* High-order bits of c */

    /* Find the next non-zero chunk, setting i to its index, or break out
       of loop if there is none.  Note that the chunk at index u is not
       necessarily non-zero - it was initially, but u or the chunk at u
       may have changed. */

#   if OPT_CARRY
    {
      c = sacc->chunk[i];
      if (c != 0)
      { goto nonzero;
      }
      i += 1;
      if (i > u)
      { break;  /* reaching here is only possible when u == i initially, */
      }         /*   with the last add to a chunk having changed it to 0 */

      for (;;)
      { c = sacc->chunk[i];
        if (c != 0)
        { goto nonzero;
        }
        i += 1;
        c = sacc->chunk[i];
        if (c != 0)
        { goto nonzero;
        }
        i += 1;
        c = sacc->chunk[i];
        if (c != 0)
        { goto nonzero;
        }
        i += 1;
        c = sacc->chunk[i];
        if (c != 0)
        { goto nonzero;
        }
        i += 1;
      }
    }
#   else
    {
      do
      { c = sacc->chunk[i];
        if (c != 0)
        { goto nonzero;
        }
        i += 1;
      } while (i <= u);

      break;
    }
#   endif

    /* Propagate possible carry from this chunk to next chunk up. */

  nonzero:
    chigh = c >> XSUM_LOW_MANTISSA_BITS;
    if (chigh == 0)
    { uix = i;
      i += 1;
      continue;  /* no need to change this chunk */
    }

    if (u == i)
    { if (chigh == -1)
      { uix = i;
        break;   /* don't propagate -1 into the region of all zeros above */
      }
      u = i+1;   /* we will change chunk[u+1], so we'll need to look at it */
    }

    clow = c & XSUM_LOW_MANTISSA_MASK;
    if (clow != 0)
    { uix = i;
    }

    /* We now change chunk[i] and add to chunk[i+1]. Note that i+1 should be
       in range (no bigger than XSUM_CHUNKS-1) if summing memory, since
       the number of chunks is big enough to hold any sum, and we do not
       store redundant chunks with values 0 or -1 above previously non-zero
       chunks.  But other add operations might cause overflow, in which
       case we produce a NaN with all 1s as payload.  (We can't reliably produce
       an Inf of the right sign.) */

    sacc->chunk[i] = clow;
    if (i+1 >= XSUM_SCHUNKS)
    { xsum_small_add_inf_nan (sacc,
        ((xsum_int)XSUM_EXP_MASK << XSUM_MANTISSA_BITS) | XSUM_MANTISSA_MASK);
      u = i;
    }
    else
    { sacc->chunk[i+1] += chigh;  /* note: this could make this chunk be zero */
    }

    i += 1;

  } while (i <= u);

  /* Check again for the number being zero, since carry propagation might
     have created zero from something that initially looked non-zero. */

  if (uix < 0)
  {
    uix = 0;
    goto done;
  }

  /* While the uppermost chunk is negative, with value -1, combine it with
     the chunk below (if there is one) to produce the same number but with
     one fewer non-zero chunks. */

  while (sacc->chunk[uix] == -1 && uix > 0)
  { /* Left shift of a negative number is undefined according to the standard,
       so do a multiply - it's all presumably constant-folded by the compiler.*/
    sacc->chunk[uix-1] += ((xsum_schunk) -1)
                             * (((xsum_schunk) 1) << XSUM_LOW_MANTISSA_BITS);
    sacc->chunk[uix] = 0;
    uix -= 1;
  }

  /* We can now add one less than the total allowed terms before the
     next carry propagate. */

done:
  sacc->adds_until_propagate = XSUM_SMALL_CARRY_TERMS-1;

  /* Return index of uppermost non-zero chunk. */

  return uix;
}


/* ------------------------ EXTERNAL ROUTINES ------------------------------- */


/* INITIALIZE A SMALL ACCUMULATOR TO ZERO. */

void xsum_small_init (xsum_small_accumulator *restrict sacc)
{
  sacc->adds_until_propagate = XSUM_SMALL_CARRY_TERMS;
  sacc->Inf = sacc->NaN = 0;
# if USE_MEMSET_SMALL
  { memset (sacc->chunk, 0, XSUM_SCHUNKS * sizeof(xsum_schunk));
  }
# elif USE_SIMD && __AVX__ && XSUM_SCHUNKS==67
  { xsum_schunk *ch = sacc->chunk;
    __m256i z = _mm256_setzero_si256();
    _mm256_storeu_si256 ((__m256i *)(ch+0), z);
    _mm256_storeu_si256 ((__m256i *)(ch+4), z);
    _mm256_storeu_si256 ((__m256i *)(ch+8), z);
    _mm256_storeu_si256 ((__m256i *)(ch+12), z);
    _mm256_storeu_si256 ((__m256i *)(ch+16), z);
    _mm256_storeu_si256 ((__m256i *)(ch+20), z);
    _mm256_storeu_si256 ((__m256i *)(ch+24), z);
    _mm256_storeu_si256 ((__m256i *)(ch+28), z);
    _mm256_storeu_si256 ((__m256i *)(ch+32), z);
    _mm256_storeu_si256 ((__m256i *)(ch+36), z);
    _mm256_storeu_si256 ((__m256i *)(ch+40), z);
    _mm256_storeu_si256 ((__m256i *)(ch+44), z);
    _mm256_storeu_si256 ((__m256i *)(ch+48), z);
    _mm256_storeu_si256 ((__m256i *)(ch+52), z);
    _mm256_storeu_si256 ((__m256i *)(ch+56), z);
    _mm256_storeu_si256 ((__m256i *)(ch+60), z);
    _mm_storeu_si128    ((__m128i *)(ch+64), _mm256_castsi256_si128(z));
    _mm_storeu_si64     (ch+66, _mm256_castsi256_si128(z));
  }
# else
  { xsum_schunk *p;
    int n;
    p = sacc->chunk;
    n = XSUM_SCHUNKS;
    do { *p++ = 0; n -= 1; } while (n > 0);
  }
# endif
}


/* ADD ONE NUMBER TO A SMALL ACCUMULATOR ASSUMING NO CARRY PROPAGATION REQ'D.
   This function is declared INLINE regardless of the setting of INLINE_SMALL
   and for good performance it must be inlined by the compiler (otherwise the
   procedure call overhead will result in substantial inefficiency). */

static INLINE void xsum_add1_no_carry (xsum_small_accumulator *restrict sacc,
                                       xsum_flt value)
{
  xsum_int ivalue;
  xsum_int mantissa;
  xsum_expint exp, low_exp, high_exp;
  xsum_schunk *chunk_ptr;

  /* Extract exponent and mantissa.  Split exponent into high and low parts. */

  COPY64 (ivalue, value);

  exp = (ivalue >> XSUM_MANTISSA_BITS) & XSUM_EXP_MASK;
  mantissa = ivalue & XSUM_MANTISSA_MASK;
  high_exp = exp >> XSUM_LOW_EXP_BITS;
  low_exp = exp & XSUM_LOW_EXP_MASK;

  /* Categorize number as normal, denormalized, or Inf/NaN according to
     the value of the exponent field. */

  if (exp == 0) /* zero or denormalized */
  { /* If it's a zero (positive or negative), we do nothing. */
    if (mantissa == 0)
    { return;
    }
    /* Denormalized mantissa has no implicit 1, but exponent is 1 not 0. */
    exp = low_exp = 1;
  }
  else if (exp == XSUM_EXP_MASK)  /* Inf or NaN */
  { /* Just update flags in accumulator structure. */
    xsum_small_add_inf_nan (sacc, ivalue);
    return;
  }
  else /* normalized */
  { /* OR in implicit 1 bit at top of mantissa */
    mantissa |= (xsum_int)1 << XSUM_MANTISSA_BITS;
  }

  /* Use high part of exponent as index of chunk, and low part of
     exponent to give position within chunk.  Fetch the two chunks
     that will be modified. */

  chunk_ptr = sacc->chunk + high_exp;

  /* Separate mantissa into two parts, after shifting, and add to (or
     subtract from) this chunk and the next higher chunk (which always
     exists since there are three extra ones at the top).

     Note that low_mantissa will have at most XSUM_LOW_MANTISSA_BITS bits,
     while high_mantissa will have at most XSUM_MANTISSA_BITS bits, since
     even though the high mantissa includes the extra implicit 1 bit, it will
     also be shifted right by at least one bit. */

  xsum_int split_mantissa[2];
  split_mantissa[0] = ((xsum_uint)mantissa << low_exp) & XSUM_LOW_MANTISSA_MASK;
  split_mantissa[1] = mantissa >> (XSUM_LOW_MANTISSA_BITS - low_exp);

  /* Add to, or subtract from, the two affected chunks. */

# if OPT_SMALL==1
  { xsum_int ivalue_sign = ivalue<0 ? -1 : 1;
    chunk_ptr[0] += ivalue_sign * split_mantissa[0];
    chunk_ptr[1] += ivalue_sign * split_mantissa[1];
  }
# elif OPT_SMALL==2
  { xsum_int ivalue_neg
              = ivalue>>(XSUM_SCHUNK_BITS-1); /* all 0s if +ve, all 1s if -ve */
    chunk_ptr[0] += (split_mantissa[0] ^ ivalue_neg) + (ivalue_neg & 1);
    chunk_ptr[1] += (split_mantissa[1] ^ ivalue_neg) + (ivalue_neg & 1);
  }
# elif OPT_SMALL==3 && USE_SIMD && __SSE2__
  { xsum_int ivalue_neg
              = ivalue>>(XSUM_SCHUNK_BITS-1); /* all 0s if +ve, all 1s if -ve */
    _mm_storeu_si128 ((__m128i *)chunk_ptr,
                      _mm_add_epi64 (_mm_loadu_si128 ((__m128i *)chunk_ptr),
                       _mm_add_epi64 (_mm_set1_epi64((__m64)(ivalue_neg&1)),
                        _mm_xor_si128 (_mm_set1_epi64((__m64)ivalue_neg),
                         _mm_loadu_si128 ((__m128i *)split_mantissa)))));
  }
# else
  { if (ivalue < 0)
    { chunk_ptr[0] -= split_mantissa[0];
      chunk_ptr[1] -= split_mantissa[1];
    }
    else
    { chunk_ptr[0] += split_mantissa[0];
      chunk_ptr[1] += split_mantissa[1];
    }
  }
# endif
}


/* ADD ONE DOUBLE TO A SMALL ACCUMULATOR.  This is equivalent to, but
   somewhat faster than, calling xsum_small_addv with a vector of one
   value. */

void xsum_small_add1 (xsum_small_accumulator *restrict sacc, xsum_flt value)
{
  if (sacc->adds_until_propagate == 0)
  { (void) xsum_carry_propagate(sacc);
  }

  xsum_add1_no_carry (sacc, value);

  sacc->adds_until_propagate -= 1;
}


/* ADD A VECTOR OF FLOATING-POINT NUMBERS TO A SMALL ACCUMULATOR.  Mixes
   calls of xsum_carry_propagate with calls of xsum_add1_no_carry. */

void xsum_small_addv (xsum_small_accumulator *restrict sacc,
                      const xsum_flt *restrict vec,
                      xsum_length n)
{ xsum_length m, i;

  while (n > 0)
  { if (sacc->adds_until_propagate == 0)
    { (void) xsum_carry_propagate(sacc);
    }
    m = n <= sacc->adds_until_propagate ? n : sacc->adds_until_propagate;
    for (i = 0; i < m; i++)
    { xsum_add1_no_carry (sacc, vec[i]);
    }
    sacc->adds_until_propagate -= m;
    vec += m;
    n -= m;
  }
}


/* ADD SQUARED NORM OF VECTOR OF FLOATING-POINT NUMBERS TO SMALL ACCUMULATOR.
   Mixes calls of xsum_carry_propagate with calls of xsum_add1_no_carry. */

void xsum_small_add_sqnorm (xsum_small_accumulator *restrict sacc,
                            const xsum_flt *restrict vec,
                            xsum_length n)
{ xsum_length m, i;

  while (n > 0)
  { if (sacc->adds_until_propagate == 0)
    { (void) xsum_carry_propagate(sacc);
    }
    m = n <= sacc->adds_until_propagate ? n : sacc->adds_until_propagate;
    for (i = 0; i < m; i++)
    { xsum_add1_no_carry (sacc, vec[i] * vec[i]);
    }
    sacc->adds_until_propagate -= m;
    vec += m;
    n -= m;
  }
}


/* ADD DOT PRODUCT OF VECTORS OF FLOATING-POINT NUMBERS TO SMALL ACCUMULATOR.
   Mixes calls of xsum_carry_propagate with calls of xsum_add1_no_carry. */

void xsum_small_add_dot (xsum_small_accumulator *restrict sacc,
                         const xsum_flt *vec1, const xsum_flt *vec2,
                         xsum_length n)
{ xsum_length m, i;

  while (n > 0)
  { if (sacc->adds_until_propagate == 0)
    { (void) xsum_carry_propagate(sacc);
    }
    m = n <= sacc->adds_until_propagate ? n : sacc->adds_until_propagate;
    for (i = 0; i < m; i++)
    { xsum_add1_no_carry (sacc, vec1[i] * vec2[i]);
    }
    sacc->adds_until_propagate -= m;
    vec1 += m;
    vec2 += m;
    n -= m;
  }
}


/* ADD A SMALL ACCUMULATOR TO ANOTHER SMALL ACCUMULATOR.  The first argument
   is the destination, which is modified.  The second is the accumulator to
   add, which may also be modified, but should still represent the same
   number.  Source and destination may be the same. */

void xsum_small_add_accumulator (xsum_small_accumulator *dst_sacc,
                                 xsum_small_accumulator *src_sacc)
{
  int i;

  xsum_carry_propagate (dst_sacc);

  if (dst_sacc == src_sacc)
  { for (i = 0; i < XSUM_SCHUNKS; i++)
    { dst_sacc->chunk[i] += dst_sacc->chunk[i];
    }
  }
  else
  {
    xsum_carry_propagate (src_sacc);

    if (src_sacc->Inf) xsum_small_add_inf_nan (dst_sacc, src_sacc->Inf);
    if (src_sacc->NaN) xsum_small_add_inf_nan (dst_sacc, src_sacc->NaN);

    for (i = 0; i < XSUM_SCHUNKS; i++)
    { dst_sacc->chunk[i] += src_sacc->chunk[i];
    }
  }

  dst_sacc->adds_until_propagate = XSUM_SMALL_CARRY_TERMS-2;
}


/* NEGATE THE VALUE IN A SMALL ACCUMULATOR. */

void xsum_small_negate (xsum_small_accumulator *restrict sacc)
{
  int i;

  for (i = 0; i < XSUM_SCHUNKS; i++)
  { sacc->chunk[i] = -sacc->chunk[i];
  }

  if (sacc->Inf != 0)
  { sacc->Inf ^= XSUM_SIGN_MASK;
  }
}


/* RETURN THE RESULT OF ROUNDING A SMALL ACCUMULATOR.  The rounding mode
   is to nearest, with ties to even.  The small accumulator may be modified
   by this operation (by carry propagation being done), but the value it
   represents should not change. */

xsum_flt xsum_small_round (xsum_small_accumulator *restrict sacc)
{
  xsum_int ivalue;
  xsum_schunk lower;
  int i, j, e, more;
  xsum_int intv;
  double fltv;

  /* See if we have a NaN from one of the numbers being a NaN, in
     which case we return the NaN with largest payload, or an infinite
     result (+Inf, -Inf, or a NaN if both +Inf and -Inf occurred).
     Note that we do NOT return NaN if we have both an infinite number
     and a sum of other numbers that overflows with opposite sign,
     since there is no real ambiguity regarding the sign in such a case. */

  if (sacc->NaN != 0)
  { COPY64(fltv, sacc->NaN);
    return fltv;
  }

  if (sacc->Inf != 0)
  { COPY64 (fltv, sacc->Inf);
    return fltv;
  }

  /* If none of the numbers summed were infinite or NaN, we proceed to
     propagate carries, as a preliminary to finding the magnitude of
     the sum.  This also ensures that the sign of the result can be
     determined from the uppermost non-zero chunk.

     We also find the index, i, of this uppermost non-zero chunk, as
     the value returned by xsum_carry_propagate, and set ivalue to
     sacc->chunk[i].  Note that ivalue will not be 0 or -1, unless
     i is 0 (the lowest chunk), in which case it will be handled by
     the code for denormalized numbers. */

  i = xsum_carry_propagate(sacc);

  ivalue = sacc->chunk[i];

  /* Handle a possible denormalized number, including zero. */

  if (i <= 1)
  {
    /* Check for zero value, in which case we can return immediately. */

    if (ivalue == 0)
    { return 0.0;
    }

    /* Check if it is actually a denormalized number.  It always is if only
       the lowest chunk is non-zero.  If the highest non-zero chunk is the
       next-to-lowest, we check the magnitude of the absolute value.
       Note that the real exponent is 1 (not 0), so we need to shift right
       by 1 here. */

    if (i == 0)
    { intv = ivalue >= 0 ? ivalue : -ivalue;
      intv >>= 1;
      if (ivalue < 0)
      { intv |= XSUM_SIGN_MASK;
      }
      COPY64 (fltv, intv);
      return fltv;
    }
    else
    { /* Note: Left shift of -ve number is undefined, so do a multiply instead,
               which is probably optimized to a shift. */
      intv = ivalue * ((xsum_int)1 << (XSUM_LOW_MANTISSA_BITS-1))
               + (sacc->chunk[0] >> 1);
      if (intv < 0)
      { if (intv > - ((xsum_int)1 << XSUM_MANTISSA_BITS))
        { intv = (-intv) | XSUM_SIGN_MASK;
          COPY64 (fltv, intv);
          return fltv;
        }
      }
      else /* non-negative */
      { if ((xsum_uint)intv < (xsum_uint)1 << XSUM_MANTISSA_BITS)
        {
          COPY64 (fltv, intv);
          return fltv;
        }
      }
      /* otherwise, it's not actually denormalized, so fall through to below */
    }
  }

  /* Find the location of the uppermost 1 bit in the absolute value of
     the upper chunk by converting it (as a signed integer) to a
     floating point value, and looking at the exponent.  Then set
     'more' to the number of bits from the lower chunk (and maybe the
     next lower) that are needed to fill out the mantissa of the
     result (including the top implicit 1 bit), plus two extra bits to
     help decide on rounding.  For negative numbers, it may turn out
     later that we need another bit, because negating a negative value
     may carry out of the top here, but not carry out of the top once
     more bits are shifted into the bottom later on. */

  fltv = (xsum_flt) ivalue;  /* finds position of topmost 1 bit of |ivalue| */
  COPY64 (intv, fltv);
  e = (intv >> XSUM_MANTISSA_BITS) & XSUM_EXP_MASK; /* e-bias is in 0..32 */
  more = 2 + XSUM_MANTISSA_BITS + XSUM_EXP_BIAS - e;

  /* Change 'ivalue' to put in 'more' bits from lower chunks into the bottom.
     Also set 'j' to the index of the lowest chunk from which these bits came,
     and 'lower' to the remaining bits of that chunk not now in 'ivalue'.
     Note that 'lower' initially has at least one bit in it, which we can
     later move into 'ivalue' if it turns out that one more bit is needed. */

  ivalue *= (xsum_int)1 << more;  /* multiply, since << of negative undefined */

  j = i-1;
  lower = sacc->chunk[j];  /* must exist, since denormalized if i==0 */
  if (more >= XSUM_LOW_MANTISSA_BITS)
  { more -= XSUM_LOW_MANTISSA_BITS;
    ivalue += lower << more;
    j -= 1;
    lower = j < 0 ? 0 : sacc->chunk[j];
  }
  ivalue += lower >> (XSUM_LOW_MANTISSA_BITS - more);
  lower &= ((xsum_schunk)1 << (XSUM_LOW_MANTISSA_BITS - more)) - 1;

  /* Decide on rounding, with separate code for positive and negative values.

     At this point, 'ivalue' has the signed mantissa bits, plus two extra
     bits, with 'e' recording the exponent position for these within their
     top chunk.  For positive 'ivalue', the bits in 'lower' and chunks
     below 'j' add to the absolute value; for negative 'ivalue' they
     subtract.

     After setting 'ivalue' to the tentative unsigned mantissa
     (shifted left 2), and 'intv' to have the correct sign, this
     code goes to done_rounding if it finds that just discarding lower
     order bits is correct, and to round_away_from_zero if instead the
     magnitude should be increased by one in the lowest mantissa bit. */

  if (ivalue >= 0)  /* number is positive, lower bits are added to magnitude */
  {
    intv = 0;  /* positive sign */

    if ((ivalue & 2) == 0)  /* extra bits are 0x */
    {
      goto done_rounding;
    }

    if ((ivalue & 1) != 0)  /* extra bits are 11 */
    {
      goto round_away_from_zero;
    }

    if ((ivalue & 4) != 0)  /* low bit is 1 (odd), extra bits are 10 */
    {
      goto round_away_from_zero;
    }

    if (lower == 0)  /* see if any lower bits are non-zero */
    { while (j > 0)
      { j -= 1;
        if (sacc->chunk[j] != 0)
        { lower = 1;
          break;
        }
      }
    }

    if (lower != 0)  /* low bit 0 (even), extra bits 10, non-zero lower bits */
    {
      goto round_away_from_zero;
    }
    else  /* low bit 0 (even), extra bits 10, all lower bits 0 */
    {
      goto done_rounding;
    }
  }

  else  /* number is negative, lower bits are subtracted from magnitude */
  {
    /* Check for a negative 'ivalue' that when negated doesn't contain a full
       mantissa's worth of bits, plus one to help rounding.  If so, move one
       more bit into 'ivalue' from 'lower' (and remove it from 'lower').
       This happens when the negation of the upper part of 'ivalue' has the
       form 10000... but the negation of the full 'ivalue' is not 10000... */

    if (((-ivalue) & ((xsum_int)1 << (XSUM_MANTISSA_BITS+2))) == 0)
    { int pos = (xsum_schunk)1 << (XSUM_LOW_MANTISSA_BITS - 1 - more);
      ivalue *= 2;  /* note that left shift undefined if ivalue is negative */
      if (lower & pos)
      { ivalue += 1;
        lower &= ~pos;
      }
      e -= 1;
    }

    intv = XSUM_SIGN_MASK;    /* negative sign */
    ivalue = -ivalue;         /* ivalue now contains the absolute value */

    if ((ivalue & 3) == 3)  /* extra bits are 11 */
    {
      goto round_away_from_zero;
    }

    if ((ivalue & 3) <= 1)  /* extra bits are 00 or 01 */
    {
      goto done_rounding;
    }

    if ((ivalue & 4) == 0)  /* low bit is 0 (even), extra bits are 10 */
    {
      goto done_rounding;
    }

    if (lower == 0)  /* see if any lower bits are non-zero */
    { while (j > 0)
      { j -= 1;
        if (sacc->chunk[j] != 0)
        { lower = 1;
          break;
        }
      }
    }

    if (lower != 0)  /* low bit 1 (odd), extra bits 10, non-zero lower bits */
    {
      goto done_rounding;
    }
    else  /* low bit 1 (odd), extra bits are 10, lower bits are all 0 */
    {
      goto round_away_from_zero;
    }

  }

round_away_from_zero:

  /* Round away from zero, then check for carry having propagated out the
     top, and shift if so. */

  ivalue += 4;  /* add 1 to low-order mantissa bit */
  if (ivalue & ((xsum_int)1 << (XSUM_MANTISSA_BITS+3)))
  { ivalue >>= 1;
    e += 1;
  }

done_rounding: ;

  /* Get rid of the bottom 2 bits that were used to decide on rounding. */

  ivalue >>= 2;

  /* Adjust to the true exponent, accounting for where this chunk is. */

  e += (i<<XSUM_LOW_EXP_BITS) - XSUM_EXP_BIAS - XSUM_MANTISSA_BITS;

  /* If exponent has overflowed, change to plus or minus Inf and return. */

  if (e >= XSUM_EXP_MASK)
  { intv |= (xsum_int) XSUM_EXP_MASK << XSUM_MANTISSA_BITS;
    COPY64 (fltv, intv);

    return fltv;
  }

  /* Put exponent and mantissa into intv, which already has the sign,
     then copy into fltv. */

  intv += (xsum_int)e << XSUM_MANTISSA_BITS;
  intv += ivalue & XSUM_MANTISSA_MASK;  /* mask out the implicit 1 bit */
  COPY64 (fltv, intv);

  if (xsum_debug)
  {
    if ((ivalue >> XSUM_MANTISSA_BITS) != 1) abort();
  }

  return fltv;
}


/* FIND RESULT OF DIVIDING SMALL ACCUMULATOR BY UNSIGNED INTEGER. */

xsum_flt xsum_small_div_unsigned
           (xsum_small_accumulator *restrict sacc, unsigned div)
{
  xsum_flt result;
  unsigned rem;
  double fltv;
  int sign;
  int i, j;

  /* Return NaN or an Inf if that's what's in the superaccumulator. */

  if (sacc->NaN != 0)
  { COPY64(fltv, sacc->NaN);
    return fltv;
  }

  if (sacc->Inf != 0)
  { COPY64 (fltv, sacc->Inf);
    return fltv;
  }

  /* Make a copy of the superaccumulator, so we can change it here without
     changing *sacc. */

  xsum_small_accumulator tacc = *sacc;

  /* Carry propagate in the temporary copy of the superaccumulator.
     Sets 'i' to the index of the topmost nonzero chunk. */

  i = xsum_carry_propagate(&tacc);

  /* Check for division by zero, and if so, return +Inf, -Inf, or NaN,
     depending on whether the superaccumulator is positive, negative,
     or zero. */

  if (div == 0)
  {
    return tacc.chunk[i] > 0 ? INFINITY : tacc.chunk[i] < 0 ? -INFINITY : NAN;
  }

  /* Record sign of accumulator, and if it's negative, negate and
     re-propagate so that it will be positive. */

  sign = +1;

  if (tacc.chunk[i] < 0)
  { xsum_small_negate(&tacc);
    i = xsum_carry_propagate(&tacc);
    if (xsum_debug)
    {
      if (tacc.chunk[i] < 0) abort();
    }
    sign = -1;
  }

  /* Do the division in the small accumulator, putting the remainder after
     dividing the bottom chunk in 'rem'. */

  rem = 0;
  for (j = i; j>=0; j--)
  { xsum_uint num = ((xsum_uint) rem << XSUM_LOW_MANTISSA_BITS) + tacc.chunk[j];
    xsum_uint quo = num / div;
    rem = num - quo*div;
    tacc.chunk[j] = quo;
  }

  /* Find new top chunk. */

  while (i > 0 && tacc.chunk[i] == 0)
  { i -= 1;
  }

  /* Do rounding, with separate approachs for a normal number with biased
     exponent greater than 1, and for a normal number with exponent of 1
     or a denormalized number (also having true biased exponent of 1). */

  if (i > 1 || tacc.chunk[1] >= (1 << (XSUM_HIGH_MANTISSA_BITS+2)))
  {
    /* Normalized number with at least two bits at bottom of chunk 0
       below the mantissa.  Just need to 'or' in a 1 at the bottom if
       remainder is non-zero to break a tie if bits below bottom of
       mantissa are exactly 1/2. */

    if (rem > 0)
    { tacc.chunk[0] |= 1;
    }
  }
  else
  {
    /* Denormalized number or normal number with biased exponent of 1.
       Lowest bit of bottom chunk is just below lowest bit of
       mantissa.  Need to explicitly round here using the bottom bit
       and the remainder - round up if lower > 1/2 or >= 1/2 and
       odd. */

    if (tacc.chunk[0] & 1)  /* lower part is >= 1/2 */
    {
      if (tacc.chunk[0] & 2)  /* lowest bit of mantissa is 1 (odd) */
      { tacc.chunk[0] += 2;     /* round up */
      }
      else                    /* lowest bit of mantissa is 0 (even) */
      { if (rem > 0)            /* lower part is > 1/2 */
        { tacc.chunk[0] += 2;     /* round up */
        }
      }

      tacc.chunk[0] &= ~1;  /* clear low bit (but should anyway be ignored) */
    }
  }

  /* Do the final rounding, with the lowest bit set as above. */

  result = xsum_small_round (&tacc);

  return sign*result;
}


/* FIND RESULT OF DIVIDING SMALL ACCUMULATOR BY SIGNED INTEGER. */

xsum_flt xsum_small_div_int
           (xsum_small_accumulator *restrict sacc, int div)
{
  if (div < 0)
  { return -xsum_small_div_unsigned (sacc, (unsigned) -div);
  }
  else
  { return xsum_small_div_unsigned (sacc, (unsigned) div);
  }
}
