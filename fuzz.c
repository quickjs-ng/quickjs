// clang -g -O1 -fsanitize=fuzzer -o fuzz fuzz.c
#include "quickjs.h"
#include "quickjs.c"
#include "cutils.c"
#include "libregexp.c"
#include "libunicode.c"
#include "xsum.c"
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    JSRuntime *rt = JS_NewRuntime();
    if (!rt)
        exit(1);
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx)
        exit(1);
    JSValue val = JS_ReadObject(ctx, buf, len, /*flags*/0);
    JS_FreeValue(ctx, val);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
