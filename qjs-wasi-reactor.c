/*
 * QuickJS WASI Reactor Mode
 *
 * In reactor mode, QuickJS exports functions that can be called repeatedly
 * by the host instead of running main() once and blocking in the event loop.
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

#include "quickjs.h"
#include "quickjs-libc.h"
#include "qjs-common.h"

static JSRuntime *reactor_rt = NULL;
static JSContext *reactor_ctx = NULL;

int qjs_init_argv(int argc, char **argv);

__attribute__((export_name("qjs_init")))
int qjs_init(void)
{
    static char *empty_argv[] = { "qjs", NULL };
    return qjs_init_argv(1, empty_argv);
}

__attribute__((export_name("qjs_init_argv")))
int qjs_init_argv(int argc, char **argv)
{
    QJSConfig cfg;
    int optind = 1;

    if (reactor_rt)
        return -1; /* already initialized */

    qjs_config_init(&cfg);
    if (qjs_config_parse_args(&cfg, argc, argv, &optind) < 0)
        return -1;

    qjs_set_argv(argc, argv);

    reactor_rt = JS_NewRuntime();
    if (!reactor_rt)
        return -1;

    if (cfg.memory_limit >= 0)
        JS_SetMemoryLimit(reactor_rt, (size_t)cfg.memory_limit);
    if (cfg.stack_size >= 0)
        JS_SetMaxStackSize(reactor_rt, (size_t)cfg.stack_size);

    qjs_setup_runtime(reactor_rt);

    reactor_ctx = qjs_new_context(reactor_rt);
    if (!reactor_ctx)
        goto fail;

    if (qjs_apply_config(reactor_ctx, &cfg, argc - optind, argv + optind) < 0)
        goto fail;

    /* Evaluate file argument if present */
    if (optind < argc) {
        if (qjs_eval_file(reactor_ctx, argv[optind], cfg.module))
            goto fail;
    }

    return 0;

fail:
    js_std_free_handlers(reactor_rt);
    if (reactor_ctx) {
        JS_FreeContext(reactor_ctx);
        reactor_ctx = NULL;
    }
    JS_FreeRuntime(reactor_rt);
    reactor_rt = NULL;
    return -1;
}

__attribute__((export_name("qjs_get_context")))
JSContext *qjs_get_context(void)
{
    return reactor_ctx;
}

__attribute__((export_name("qjs_destroy")))
void qjs_destroy(void)
{
    if (reactor_ctx) {
        js_std_free_handlers(reactor_rt);
        JS_FreeContext(reactor_ctx);
        reactor_ctx = NULL;
    }
    if (reactor_rt) {
        JS_FreeRuntime(reactor_rt);
        reactor_rt = NULL;
    }
}
