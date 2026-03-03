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

int main(void)
{
    oob_save_index();
    invalid_opcode_byte_swap();
    return 0;
}
