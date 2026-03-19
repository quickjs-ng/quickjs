#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "libregexp.h"

bool lre_check_stack_overflow(void *opaque, size_t alloca_size)
{
    return false;
}

int lre_check_timeout(void *opaque)
{
    return 0;
}

void *lre_realloc(void *opaque, void *ptr, size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

// https://github.com/quickjs-ng/quickjs/issues/1375
static void oob_save_index(void)
{
    // Bytecode with REOP_save_start index=100, but capture_count=1.
    // Without validation this causes a heap-buffer-overflow in lre_exec_backtrack.
    uint8_t bc[] = {
        0x00, 0x00,              // RE_HEADER_FLAGS = 0
        0x01,                    // RE_HEADER_CAPTURE_COUNT = 1
        0x00,                    // RE_HEADER_STACK_SIZE = 0
        0x04, 0x00, 0x00, 0x00, // RE_HEADER_BYTECODE_LEN = 4 (little-endian)
        0x05,                    // REOP_any
        0x0C, 0x64,             // REOP_save_start, index=100
        0x0B,                    // REOP_match
    };

    uint8_t *capture[2] = {NULL, NULL};
    int ret = lre_exec(capture, bc, (const uint8_t *)"a", 0, 1, 0, NULL);
    assert(ret < 0);
}

// https://github.com/quickjs-ng/quickjs/issues/1376
static void invalid_opcode_byte_swap(void)
{
    // Bytecode with an opcode byte >= REOP_COUNT triggers an OOB read
    // of the reopcode_info array in lre_byte_swap. Simulate the real
    // big-endian deserialization path (is_byte_swapped=true) which is
    // how JS_ReadRegExp calls it. The bytecode_len is stored as
    // little-endian on disk, so it appears byte-swapped to a BE reader.
    uint8_t bc[] = {
        0x00, 0x00,              // RE_HEADER_FLAGS = 0
        0x01,                    // RE_HEADER_CAPTURE_COUNT = 1
        0x00,                    // RE_HEADER_STACK_SIZE = 0
        0x00, 0x00, 0x00, 0x01, // RE_HEADER_BYTECODE_LEN = 1 (big-endian / byte-swapped)
        0x1E,                    // opcode 30 >= REOP_COUNT (invalid)
    };

    int ret = lre_byte_swap(bc, sizeof(bc), /*is_byte_swapped*/true);
    assert(ret < 0);
}

// https://github.com/quickjs-ng/quickjs/issues/1122
static void range_byte_swap(void)
{
    // Test that lre_byte_swap correctly handles REOP_range instructions.
    // lre_byte_swap used to swap the wrong variable (n instead of nw),
    // corrupting the bytecode walk and leaving subsequent instructions
    // un-swapped.
    //
    // REOP_range = opcode 22 (0x16), size field = 3 (variable length)
    // Layout: [opcode] [nw:u16] [low0:u16] [high0:u16] ...
    //
    // This bytecode represents /[ab]/ with 1 range pair: 0x0061-0x0063
    // Multi-byte values are in native byte order (as produced by the
    // regex compiler on this platform).
    uint16_t flags = 0;
    uint32_t bc_len = 8;
    uint16_t nw = 1;
    uint16_t low = 0x0061;
    uint16_t high = 0x0063;

    uint8_t bc[16];
    memcpy(&bc[0], &flags, 2);       // RE_HEADER_FLAGS
    bc[2] = 1;                        // RE_HEADER_CAPTURE_COUNT
    bc[3] = 0;                        // RE_HEADER_STACK_SIZE
    memcpy(&bc[4], &bc_len, 4);      // RE_HEADER_BYTECODE_LEN
    bc[8] = 0x16;                     // REOP_range (22)
    memcpy(&bc[9], &nw, 2);          // nw = 1
    memcpy(&bc[11], &low, 2);        // low = 0x0061 'a'
    memcpy(&bc[13], &high, 2);       // high = 0x0063 'c'
    bc[15] = 0x0B;                    // REOP_match (11)

    // Swap as if converting from native to non-native byte order
    int ret = lre_byte_swap(bc, sizeof(bc), /*is_byte_swapped*/false);
    assert(ret == 0);

    // Swap back from non-native to native
    ret = lre_byte_swap(bc, sizeof(bc), /*is_byte_swapped*/true);
    assert(ret == 0);

    // Verify round-trip: all values should be back in native format
    uint16_t got_flags, got_nw, got_low, got_high;
    uint32_t got_bc_len;
    memcpy(&got_flags, &bc[0], 2);
    memcpy(&got_bc_len, &bc[4], 4);
    memcpy(&got_nw, &bc[9], 2);
    memcpy(&got_low, &bc[11], 2);
    memcpy(&got_high, &bc[13], 2);
    assert(got_flags == 0);
    assert(got_bc_len == 8);
    assert(bc[8] == 0x16);          // REOP_range opcode unchanged
    assert(got_nw == 1);
    assert(got_low == 0x0061);
    assert(got_high == 0x0063);
    assert(bc[15] == 0x0B);         // REOP_match unchanged
}

int main(void)
{
    oob_save_index();
    invalid_opcode_byte_swap();
    range_byte_swap();
    return 0;
}
