// note: file is not actually compiled, only checked for C++ syntax errors
#include "quickjs.h"

JSCFunctionListEntry funcs[] = {
    JS_ALIAS_BASE_DEF("", "", 0),
    JS_ALIAS_DEF("", ""),
    JS_CFUNC_DEF("", 0, 0),
    JS_CFUNC_DEF2("", 0, 0, 0),
    JS_CFUNC_MAGIC_DEF("", 0, 0, 0),
    JS_CFUNC_SPECIAL_DEF("", 0, f_f, 0),
    JS_CFUNC_SPECIAL_DEF("", 0, f_f_f, 0),
    JS_CGETSET_DEF("", 0, 0),
    JS_CGETSET_DEF2("", 0, 0, 0),
    JS_CGETSET_MAGIC_DEF("", 0, 0, 0),
    JS_ITERATOR_NEXT_DEF("", 0, 0, 0),
    JS_OBJECT_DEF("", 0, 0, 0),
    JS_PROP_DOUBLE_DEF("", 0, 0),
    JS_PROP_INT32_DEF("", 0, 0),
    JS_PROP_INT64_DEF("", 0, 0),
    JS_PROP_STRING_DEF("", "", 0),
    JS_PROP_UNDEFINED_DEF("", 0),
};

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JS_FreeValue(ctx, JS_NAN);
    JS_FreeValue(ctx, JS_UNDEFINED);
    JS_FreeValue(ctx, JS_NewFloat64(ctx, 42));
    // not a legal way of using JS_MKPTR but this is here
    // to have the compiler syntax-check its definition
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_UNINITIALIZED, 0));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
