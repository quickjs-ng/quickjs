#!/bin/bash
# Build QuickJS with Coroutine Support

set -e

echo "=== Building QuickJS with Coroutine Support ==="
echo ""

CC="${CC:-clang}"
CFLAGS="-O2 -Wall -D_GNU_SOURCE -DCONFIG_VERSION=\"coroutine\""
LDFLAGS="-lm -lpthread"

# Compile object files
echo "Compiling quickjs.c..."
$CC $CFLAGS -c quickjs.c -o quickjs.o

echo "Compiling quickjs_coroutine.c..."
$CC $CFLAGS -c quickjs_coroutine.c -o quickjs_coroutine.o

echo "Compiling cutils.c..."
$CC $CFLAGS -c cutils.c -o cutils.o

echo "Compiling libunicode.c..."
$CC $CFLAGS -c libunicode.c -o libunicode.o

echo "Compiling libregexp.c..."
$CC $CFLAGS -c libregexp.c -o libregexp.o

echo "Compiling dtoa.c..."
$CC $CFLAGS -c dtoa.c -o dtoa.o

# Create static library
echo ""
echo "Creating libquickjs_generator.a..."
ar rcs libquickjs_generator.a \
    quickjs.o \
    quickjs_coroutine.o \
    cutils.o \
    libunicode.o \
    libregexp.o \
    dtoa.o

echo "✅ Library built successfully!"
echo ""

# Create test program
if [ "$1" = "test" ]; then
    echo "Creating test program..."

    cat > test_coroutine.c << 'EOF'
#include "quickjs.h"
#include "quickjs_coroutine.h"
#include <stdio.h>
#include <string.h>

const char *test_code =
"function* testGenerator() {\n"
"    console.log('Generator started');\n"
"    const x = yield 42;\n"
"    console.log('Got:', x);\n"
"    return x * 2;\n"
"}\n"
"\n"
"const gen = testGenerator();\n"
"console.log('Is generator?', __is_generator(gen));\n"
"\n"
"const r1 = gen.next();\n"
"console.log('First:', r1);\n"
"\n"
"const r2 = gen.next(100);\n"
"console.log('Second:', r2);\n";

static JSValue js_print(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            printf("%s%s", i > 0 ? " " : "", str);
            JS_FreeCString(ctx, str);
        }
    }
    printf("\n");
    return JS_UNDEFINED;
}

int main() {
    printf("=== QuickJS Coroutine Test ===\n\n");

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    /* Initialize coroutine support */
    JSCoroutineManager *mgr = JS_NewCoroutineManager(rt);
    JS_EnableCoroutines(ctx, mgr);

    /* Add console.log */
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
                     JS_NewCFunction(ctx, js_print, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console);
    JS_FreeValue(ctx, global);

    /* Run test code */
    JSValue result = JS_Eval(ctx, test_code, strlen(test_code),
                             "test.js", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        printf("\nError:\n");
        JSValue exception = JS_GetException(ctx);
        const char *str = JS_ToCString(ctx, exception);
        if (str) {
            printf("%s\n", str);
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, exception);
    }

    JS_FreeValue(ctx, result);

    /* Cleanup */
    JS_FreeCoroutineManager(mgr);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return 0;
}
EOF

    echo "Compiling test program..."
    $CC $CFLAGS -o test_coroutine test_coroutine.c libquickjs_generator.a $LDFLAGS

    echo "Running test..."
    echo ""
    ./test_coroutine
    echo ""
fi

echo "=== Build Complete ==="
echo ""
echo "Library: libquickjs_generator.a"
echo "Headers: quickjs.h, quickjs_coroutine.h"
echo ""
echo "To run tests: ./build_coroutine.sh test"
echo ""
echo "For JTask integration:"
echo "  Include: -I$(pwd)"
echo "  Link: $(pwd)/libquickjs_generator.a -lm -lpthread"