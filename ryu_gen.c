/*
 * Generation of Ryu double->shortest-decimal lookup tables
 *
 * Copyright 2018 Ulf Adams
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Regenerates dtoa-ryu-table.h. Ported from PrintDoubleLookupTable in the
 * ryu project (https://github.com/ulfjack/ryu).
 *
 * usage: ryu_gen [output_file]
 *
 * If no output_file is given, the table is written to stdout.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#define POS_TABLE_SIZE 326
#define NEG_TABLE_SIZE 342
#define POW5_BITCOUNT 125
#define POW5_INV_BITCOUNT 125

/* Minimal big integer, little-endian, enough for 5^325 (755 bits) and
   2^917 (918 bits). */
#define BIG_WORDS 16
typedef struct {
    uint64_t w[BIG_WORDS];
} Big;

static void big_zero(Big *a)
{
    memset(a, 0, sizeof(*a));
}

/* a = 2^e */
static void big_set_pow2(Big *a, int e)
{
    big_zero(a);
    a->w[e >> 6] = (uint64_t)1 << (e & 63);
}

/* 64x64 -> 128 multiply, portable (no __int128, MSVC-safe) */
static void mul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    uint32_t a0 = (uint32_t)a;
    uint32_t a1 = (uint32_t)(a >> 32);
    uint32_t b0 = (uint32_t)b;
    uint32_t b1 = (uint32_t)(b >> 32);
    uint64_t p00 = (uint64_t)a0 * b0;
    uint64_t p01 = (uint64_t)a0 * b1;
    uint64_t p10 = (uint64_t)a1 * b0;
    uint64_t p11 = (uint64_t)a1 * b1;
    uint64_t mid = p01 + p10;
    uint64_t mid_carry = (mid < p01);
    uint64_t sum = p00 + (mid << 32);
    uint64_t carry = (sum < p00);

    *lo = sum;
    *hi = p11 + (mid >> 32) + (mid_carry << 32) + carry;
}

/* a <<= s, 0 <= s < 64 */
static void big_shl_small(Big *a, int s)
{
    uint64_t carry = 0;
    int i;

    if (s == 0)
        return;
    for (i = 0; i < BIG_WORDS; i++) {
        uint64_t v = a->w[i];
        a->w[i] = (v << s) | carry;
        carry = v >> (64 - s);
    }
}

/* a <<= 1 */
static void big_shl1(Big *a)
{
    big_shl_small(a, 1);
}

/* a *= m, m is a small positive integer (5 here) */
static void big_mul_small(Big *a, uint32_t m)
{
    uint64_t carry = 0;
    int i;

    for (i = 0; i < BIG_WORDS; i++) {
        uint64_t lo, hi, sum;
        mul64(a->w[i], m, &lo, &hi);
        sum = lo + carry;
        a->w[i] = sum;
        carry = hi + (sum < lo);
    }
}

/* return -1, 0 or 1 */
static int big_cmp(const Big *a, const Big *b)
{
    int i;

    for (i = BIG_WORDS - 1; i >= 0; i--) {
        if (a->w[i] < b->w[i])
            return -1;
        if (a->w[i] > b->w[i])
            return 1;
    }
    return 0;
}

/* a -= b, assumes a >= b */
static void big_sub(Big *a, const Big *b)
{
    uint64_t borrow = 0;
    int i;

    for (i = 0; i < BIG_WORDS; i++) {
        uint64_t diff = a->w[i] - b->w[i];
        uint64_t b1 = (a->w[i] < b->w[i]);
        uint64_t diff2 = diff - borrow;
        uint64_t b2 = (diff < borrow);
        a->w[i] = diff2;
        borrow = b1 | b2;
    }
}

/* a >>= s */
static void big_shr(Big *a, int s)
{
    int word = s >> 6;
    int bit = s & 63;
    int i;

    for (i = 0; i < BIG_WORDS; i++) {
        uint64_t v = 0;
        if (i + word < BIG_WORDS) {
            v = a->w[i + word] >> bit;
            if (bit && i + word + 1 < BIG_WORDS)
                v |= a->w[i + word + 1] << (64 - bit);
        }
        a->w[i] = v;
    }
}

/* a <<= s */
static void big_shl(Big *a, int s)
{
    int word = s >> 6;
    int bit = s & 63;
    int i;

    for (i = BIG_WORDS - 1; i >= 0; i--) {
        uint64_t v = 0;
        if (i - word >= 0) {
            v = a->w[i - word] << bit;
            if (bit && i - word - 1 >= 0)
                v |= a->w[i - word - 1] >> (64 - bit);
        }
        a->w[i] = v;
    }
}

/* number of significant bits, 0 for zero */
static int big_bitlen(const Big *a)
{
    int i;

    for (i = BIG_WORDS - 1; i >= 0; i--) {
        if (a->w[i]) {
            uint64_t v = a->w[i];
            int bits = 0;
            while (v) {
                bits++;
                v >>= 1;
            }
            return i * 64 + bits;
        }
    }
    return 0;
}

/* a += 1 */
static void big_incr(Big *a)
{
    int i;

    for (i = 0; i < BIG_WORDS; i++) {
        a->w[i]++;
        if (a->w[i] != 0)
            break;
    }
}

/* q = a / b, b != 0, remainder discarded */
static void big_div(Big *q, const Big *a, const Big *b)
{
    Big r, qq;
    int n, i;

    big_zero(&r);
    big_zero(&qq);
    n = big_bitlen(a);
    for (i = n - 1; i >= 0; i--) {
        big_shl1(&r);
        if ((a->w[i >> 6] >> (i & 63)) & 1)
            r.w[0] |= 1;
        if (big_cmp(&r, b) >= 0) {
            big_sub(&r, b);
            qq.w[i >> 6] |= (uint64_t)1 << (i & 63);
        }
    }
    *q = qq;
}

/* ceil(log_2(5^e)), same as ryu's pow5bits */
static int pow5bits(int e)
{
    return (int)(((uint32_t)e * 1217359) >> 19) + 1;
}

/* invMultiplier(i) = [2^j / 5^i] + 1, j = pow5bits(i) - 1 + POW5_INV_BITCOUNT */
static void inv_multiplier(int i, Big *out)
{
    Big pow, a;
    int k, pow5len, j;

    big_zero(&pow);
    pow.w[0] = 1;
    for (k = 0; k < i; k++)
        big_mul_small(&pow, 5);
    pow5len = big_bitlen(&pow);
    j = pow5len - 1 + POW5_INV_BITCOUNT;
    big_set_pow2(&a, j);
    big_div(out, &a, &pow);
    big_incr(out);
}

/* multiplier(i) = [5^i / 2^(ceil(log_2(5^i)) - POW5_BITCOUNT)], shifts left
   when 5^i is shorter than POW5_BITCOUNT bits (small i) */
static void multiplier(int i, Big *out)
{
    Big pow;
    int k, pow5len, shift;

    big_zero(&pow);
    pow.w[0] = 1;
    for (k = 0; k < i; k++)
        big_mul_small(&pow, 5);
    pow5len = big_bitlen(&pow);
    shift = pow5len - POW5_BITCOUNT;
    if (shift >= 0)
        big_shr(&pow, shift);
    else
        big_shl(&pow, -shift);
    *out = pow;
}

static void print_table(FILE *f, const char *name, const char *size_macro,
                        const Big *table, int len)
{
    int i;

    fprintf(f, "static const uint64_t %s[%s][2] = {\n", name, size_macro);
    for (i = 0; i < len; i++) {
        char low[32], high[32];
        snprintf(low, sizeof(low), "%" PRIu64, table[i].w[0]);
        snprintf(high, sizeof(high), "%" PRIu64, table[i].w[1]);
        fprintf(f, i % 2 == 0 ? "  " : " ");
        fprintf(f, "{ %20su, %18su }", low, high);
        if (i != len - 1)
            fprintf(f, ",");
        if (i % 2 == 1)
            fprintf(f, "\n");
    }
    fprintf(f, "};\n");
}

int main(int argc, char **argv)
{
    const char *outfilename;
    FILE *f;
    Big large_inv_table[NEG_TABLE_SIZE];
    Big large_table[POS_TABLE_SIZE];
    int i;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [output_file]\n", argv[0]);
        exit(1);
    }
    outfilename = (argc == 2) ? argv[1] : NULL;
    if (outfilename) {
        f = fopen(outfilename, "wb");
        if (!f) {
            perror(outfilename);
            exit(1);
        }
    } else {
        f = stdout;
    }

    for (i = 0; i < NEG_TABLE_SIZE; i++)
        inv_multiplier(i, &large_inv_table[i]);
    for (i = 0; i < POS_TABLE_SIZE; i++)
        multiplier(i, &large_table[i]);

    fprintf(f,
            "/* Ryu double->shortest-decimal lookup tables.\n"
            " *\n"
            " * Automatically generated by ryu_gen.c; see dtoa.c for the algorithm.\n"
            " *\n"
            " * Copyright 2018 Ulf Adams\n"
            " *\n"
            " * Licensed under the Apache License, Version 2.0 (Apache-2.0) or, at your\n"
            " * option, the Boost Software License, Version 1.0 (BSL-1.0).\n"
            " */\n"
            "#ifndef DTOA_RYU_TABLE_H\n"
            "#define DTOA_RYU_TABLE_H\n"
            "\n"
            "#include <stdint.h>\n"
            "\n"
            "// These tables are generated by ryu_gen.c.\n"
            "#define DOUBLE_POW5_INV_BITCOUNT %d\n"
            "#define DOUBLE_POW5_BITCOUNT %d\n"
            "\n"
            "#define DOUBLE_POW5_INV_TABLE_SIZE %d\n"
            "#define DOUBLE_POW5_TABLE_SIZE %d\n"
            "\n",
            POW5_INV_BITCOUNT, POW5_BITCOUNT,
            NEG_TABLE_SIZE, POS_TABLE_SIZE);

    print_table(f, "DOUBLE_POW5_INV_SPLIT", "DOUBLE_POW5_INV_TABLE_SIZE",
                large_inv_table, NEG_TABLE_SIZE);
    fprintf(f, "\n\n");
    print_table(f, "DOUBLE_POW5_SPLIT", "DOUBLE_POW5_TABLE_SIZE",
                large_table, POS_TABLE_SIZE);

    fprintf(f, "\n#endif // DTOA_RYU_TABLE_H\n");

    if (outfilename)
        fclose(f);
    return 0;
}
