/* File generated automatically by the QuickJS-ng compiler. */

#include "quickjs-libc.h"

const uint32_t qjsc_function_source_size = 324;

const uint8_t qjsc_function_source[324] = {
 0x13, 0x05, 0x01, 0x30, 0x74, 0x65, 0x73, 0x74,
 0x73, 0x2f, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69,
 0x6f, 0x6e, 0x5f, 0x73, 0x6f, 0x75, 0x72, 0x63,
 0x65, 0x2e, 0x6a, 0x73, 0x01, 0x0c, 0x61, 0x63,
 0x74, 0x75, 0x61, 0x6c, 0x01, 0x02, 0x66, 0x01,
 0x0c, 0x65, 0x78, 0x70, 0x65, 0x63, 0x74, 0x01,
 0x34, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f,
 0x6e, 0x20, 0x66, 0x28, 0x29, 0x20, 0x7b, 0x20,
 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x20, 0x34,
 0x32, 0x20, 0x7d, 0x0d, 0xca, 0x03, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x0c, 0x20, 0x0a, 0x01, 0xa4,
 0x01, 0x00, 0x05, 0x00, 0x03, 0x02, 0x01, 0x74,
 0x05, 0xcc, 0x03, 0x02, 0x00, 0x30, 0xce, 0x03,
 0x04, 0x00, 0x70, 0xcc, 0x03, 0x04, 0x02, 0x70,
 0x10, 0x00, 0x01, 0x00, 0xe6, 0x01, 0x00, 0x01,
 0x00, 0xd0, 0x03, 0x00, 0x0d, 0xce, 0x03, 0x01,
 0x01, 0x0c, 0x43, 0x0a, 0x01, 0xce, 0x03, 0x00,
 0x00, 0x00, 0x01, 0x00, 0x00, 0x03, 0x00, 0xbd,
 0x2a, 0x28, 0xca, 0x03, 0x03, 0x01, 0x00, 0x1a,
 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e,
 0x20, 0x66, 0x28, 0x29, 0x20, 0x7b, 0x20, 0x72,
 0x65, 0x74, 0x75, 0x72, 0x6e, 0x20, 0x34, 0x32,
 0x20, 0x7d, 0x0c, 0x03, 0xc3, 0x04, 0x08, 0xcd,
 0x08, 0xeb, 0x05, 0xc0, 0x00, 0xe3, 0x29, 0x04,
 0xe9, 0x00, 0x00, 0x00, 0xe2, 0x62, 0x00, 0x00,
 0xdf, 0x43, 0x39, 0x00, 0x00, 0x00, 0x24, 0x00,
 0x00, 0xca, 0x63, 0x00, 0x00, 0x66, 0x00, 0x00,
 0xb0, 0xeb, 0x0b, 0x39, 0x96, 0x00, 0x00, 0x00,
 0x63, 0x00, 0x00, 0xf0, 0x30, 0x62, 0x02, 0x00,
 0x62, 0x01, 0x00, 0x39, 0x3c, 0x00, 0x00, 0x00,
 0x66, 0x00, 0x00, 0x04, 0xe7, 0x00, 0x00, 0x00,
 0x9e, 0x32, 0x01, 0x00, 0x03, 0x00, 0xcb, 0x63,
 0x01, 0x00, 0x43, 0x39, 0x00, 0x00, 0x00, 0x24,
 0x00, 0x00, 0xcc, 0x63, 0x02, 0x00, 0x66, 0x00,
 0x00, 0xb0, 0xeb, 0x0b, 0x39, 0x96, 0x00, 0x00,
 0x00, 0x63, 0x02, 0x00, 0xf0, 0x30, 0x69, 0x02,
 0x00, 0x69, 0x01, 0x00, 0x06, 0x2f, 0xca, 0x03,
 0x01, 0x01, 0x18, 0x00, 0x1c, 0x0a, 0x2a, 0x26,
 0x03, 0x20, 0x1c, 0x1b, 0x0c, 0x00, 0x10, 0x08,
 0x27, 0x11, 0x12, 0x67, 0x0d, 0x26, 0x03, 0x20,
 0x1c, 0x1b, 0x0c, 0x00,
};

static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
  JSContext *ctx = JS_NewContext(rt);
  if (!ctx)
    return NULL;
  return ctx;
}

int main(int argc, char **argv)
{
  int r;
  JSRuntime *rt;
  JSContext *ctx;
  r = 0;
  rt = JS_NewRuntime();
  js_std_set_worker_new_context_func(JS_NewCustomContext);
  js_std_init_handlers(rt);
  JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);
  ctx = JS_NewCustomContext(rt);
  js_std_add_helpers(ctx, argc, argv);
  js_std_eval_binary(ctx, qjsc_function_source, qjsc_function_source_size, 0);
  r = js_std_loop(ctx);
  if (r) {
    js_std_dump_error(ctx);
  }
  js_std_free_handlers(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  return r;
}
