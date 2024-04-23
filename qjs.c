/*
 * QuickJS stand alone interpreter
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "cutils.h"
#include "quickjs-libc.h"

extern const uint8_t qjsc_repl[];
extern const uint32_t qjsc_repl_size;

static JSCFunctionListEntry argv0;

static int eval_buf(JSContext *ctx, const void *buf, int buf_len,
                    const char *filename, int eval_flags)
{
    JSValue val;
    int ret;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
                      eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            js_module_set_import_meta(ctx, val, TRUE, TRUE);
            val = JS_EvalFunction(ctx, val);
        }
    } else {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        ret = -1;
    } else {
        ret = 0;
    }
    JS_FreeValue(ctx, val);
    return ret;
}

static int eval_file(JSContext *ctx, const char *filename, int module)
{
    uint8_t *buf;
    int ret, eval_flags;
    size_t buf_len;

    buf = js_load_file(ctx, &buf_len, filename);
    if (!buf) {
        perror(filename);
        exit(1);
    }

    if (module < 0) {
        module = (has_suffix(filename, ".mjs") ||
                  JS_DetectModule((const char *)buf, buf_len));
    }
    if (module)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;
    ret = eval_buf(ctx, buf, buf_len, filename, eval_flags);
    js_free(ctx, buf);
    return ret;
}

static int64_t parse_limit(const char *arg) {
    char *p;
    unsigned long unit = 1024; /* default to traditional KB */
    double d = strtod(arg, &p);

    if (p == arg) {
        fprintf(stderr, "Invalid limit: %s\n", arg);
        return -1;
    }

    if (*p) {
        switch (*p++) {
        case 'b': case 'B': unit = 1UL <<  0; break;
        case 'k': case 'K': unit = 1UL << 10; break; /* IEC kibibytes */
        case 'm': case 'M': unit = 1UL << 20; break; /* IEC mebibytes */
        case 'g': case 'G': unit = 1UL << 30; break; /* IEC gigibytes */
        default:
            fprintf(stderr, "Invalid limit: %s, unrecognized suffix, only k,m,g are allowed\n", arg);
            return -1;
        }
        if (*p) {
            fprintf(stderr, "Invalid limit: %s, only one suffix allowed\n", arg);
            return -1;
        }
    }

    return (int64_t)(d * unit);
}

static JSValue js_gc(JSContext *ctx, JSValue this_val,
                     int argc, JSValue *argv)
{
    JS_RunGC(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry navigator_obj[] = {
    JS_PROP_STRING_DEF("userAgent", "quickjs-ng", JS_PROP_ENUMERABLE),
};

static const JSCFunctionListEntry global_obj[] = {
    JS_CFUNC_DEF("gc", 0, js_gc),
    JS_OBJECT_DEF("navigator", navigator_obj, countof(navigator_obj), JS_PROP_C_W_E),
};

/* also used to initialize the worker context */
static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyFunctionList(ctx, global, global_obj, countof(global_obj));
    JS_SetPropertyFunctionList(ctx, global, &argv0, 1);
    JS_FreeValue(ctx, global);

    return ctx;
}

#if defined(__APPLE__)
#define MALLOC_OVERHEAD  0
#else
#define MALLOC_OVERHEAD  8
#endif

struct trace_malloc_data {
    uint8_t *base;
};

static inline unsigned long long js_trace_malloc_ptr_offset(uint8_t *ptr,
                                                struct trace_malloc_data *dp)
{
    return ptr - dp->base;
}

static void
#if defined(_WIN32) && !defined(__clang__)
/* mingw printf is used */
__attribute__((format(gnu_printf, 2, 3)))
#else
__attribute__((format(printf, 2, 3)))
#endif
    js_trace_malloc_printf(JSMallocState *s, const char *fmt, ...)
{
    va_list ap;
    int c;

    va_start(ap, fmt);
    while ((c = *fmt++) != '\0') {
        if (c == '%') {
            /* only handle %p and %zd */
            if (*fmt == 'p') {
                uint8_t *ptr = va_arg(ap, void *);
                if (ptr == NULL) {
                    printf("NULL");
                } else {
                    printf("H%+06lld.%zd",
                           js_trace_malloc_ptr_offset(ptr, s->opaque),
                           js__malloc_usable_size(ptr));
                }
                fmt++;
                continue;
            }
            if (fmt[0] == 'z' && fmt[1] == 'd') {
                size_t sz = va_arg(ap, size_t);
                printf("%zd", sz);
                fmt += 2;
                continue;
            }
        }
        putc(c, stdout);
    }
    va_end(ap);
}

static void js_trace_malloc_init(struct trace_malloc_data *s)
{
    free(s->base = malloc(8));
}

static void *js_trace_malloc(JSMallocState *s, size_t size)
{
    void *ptr;

    /* Do not allocate zero bytes: behavior is platform dependent */
    assert(size != 0);

    /* When malloc_limit is 0 (unlimited), malloc_limit - 1 will be SIZE_MAX. */
    if (unlikely(s->malloc_size + size > s->malloc_limit - 1))
        return NULL;
    ptr = malloc(size);
    js_trace_malloc_printf(s, "A %zd -> %p\n", size, ptr);
    if (ptr) {
        s->malloc_count++;
        s->malloc_size += js__malloc_usable_size(ptr) + MALLOC_OVERHEAD;
    }
    return ptr;
}

static void js_trace_free(JSMallocState *s, void *ptr)
{
    if (!ptr)
        return;

    js_trace_malloc_printf(s, "F %p\n", ptr);
    s->malloc_count--;
    s->malloc_size -= js__malloc_usable_size(ptr) + MALLOC_OVERHEAD;
    free(ptr);
}

static void *js_trace_realloc(JSMallocState *s, void *ptr, size_t size)
{
    size_t old_size;

    if (!ptr) {
        if (size == 0)
            return NULL;
        return js_trace_malloc(s, size);
    }
    old_size = js__malloc_usable_size(ptr);
    if (size == 0) {
        js_trace_malloc_printf(s, "R %zd %p\n", size, ptr);
        s->malloc_count--;
        s->malloc_size -= old_size + MALLOC_OVERHEAD;
        free(ptr);
        return NULL;
    }
    /* When malloc_limit is 0 (unlimited), malloc_limit - 1 will be SIZE_MAX. */
    if (s->malloc_size + size - old_size > s->malloc_limit - 1)
        return NULL;

    js_trace_malloc_printf(s, "R %zd %p", size, ptr);

    ptr = realloc(ptr, size);
    js_trace_malloc_printf(s, " -> %p\n", ptr);
    if (ptr) {
        s->malloc_size += js__malloc_usable_size(ptr) - old_size;
    }
    return ptr;
}

static const JSMallocFunctions trace_mf = {
    js_trace_malloc,
    js_trace_free,
    js_trace_realloc,
    js__malloc_usable_size
};

#define PROG_NAME "qjs"

void help(void)
{
    printf("QuickJS-ng version %s\n"
           "usage: " PROG_NAME " [options] [file [args]]\n"
           "-h  --help         list options\n"
           "-e  --eval EXPR    evaluate EXPR\n"
           "-i  --interactive  go to interactive mode\n"
           "-m  --module       load as ES6 module (default=autodetect)\n"
           "    --script       load as ES6 script (default=autodetect)\n"
           "-I  --include file include an additional file\n"
           "    --std          make 'std' and 'os' available to the loaded script\n"
           "-T  --trace        trace memory allocation\n"
           "-d  --dump         dump the memory usage stats\n"
           "    --memory-limit n       limit the memory usage to 'n' Kbytes\n"
           "    --stack-size n         limit the stack size to 'n' Kbytes\n"
           "    --unhandled-rejection  dump unhandled promise rejections\n"
           "-q  --quit         just instantiate the interpreter and quit\n", JS_GetVersion());
    exit(1);
}

int main(int argc, char **argv)
{
    JSRuntime *rt;
    JSContext *ctx;
    struct trace_malloc_data trace_data = { NULL };
    int optind;
    char *expr = NULL;
    int interactive = 0;
    int dump_memory = 0;
    int dump_flags = 0;
    int trace_memory = 0;
    int empty_run = 0;
    int module = -1;
    int load_std = 0;
    int dump_unhandled_promise_rejection = 0;
    char *include_list[32];
    int i, include_count = 0;
    int64_t memory_limit = -1;
    int64_t stack_size = -1;

    argv0 = (JSCFunctionListEntry)JS_PROP_STRING_DEF("argv0", argv[0],
                                                     JS_PROP_C_W_E);

    /* cannot use getopt because we want to pass the command line to
       the script */
    optind = 1;
    while (optind < argc && *argv[optind] == '-') {
        char *arg = argv[optind] + 1;
        const char *longopt = "";
        char *opt_arg = NULL;
        /* a single - is not an option, it also stops argument scanning */
        if (!*arg)
            break;
        optind++;
        if (*arg == '-') {
            longopt = arg + 1;
            opt_arg = strchr(longopt, '=');
            if (opt_arg)
                *opt_arg++ = '\0';
            arg += strlen(arg);
            /* -- stops argument scanning */
            if (!*longopt)
                break;
        }
        for (; *arg || *longopt; longopt = "") {
            char opt = *arg;
            if (opt) {
                arg++;
                if (!opt_arg && *arg)
                    opt_arg = arg;
            }
            if (opt == 'h' || opt == '?' || !strcmp(longopt, "help")) {
                help();
                continue;
            }
            if (opt == 'e' || !strcmp(longopt, "eval")) {
                if (!opt_arg) {
                    if (optind >= argc) {
                        fprintf(stderr, "qjs: missing expression for -e\n");
                        exit(2);
                    }
                    opt_arg = argv[optind++];
                }
                expr = opt_arg;
                break;
            }
            if (opt == 'I' || !strcmp(longopt, "include")) {
                if (optind >= argc) {
                    fprintf(stderr, "expecting filename");
                    exit(1);
                }
                if (include_count >= countof(include_list)) {
                    fprintf(stderr, "too many included files");
                    exit(1);
                }
                include_list[include_count++] = argv[optind++];
                continue;
            }
            if (opt == 'i' || !strcmp(longopt, "interactive")) {
                interactive++;
                continue;
            }
            if (opt == 'm' || !strcmp(longopt, "module")) {
                module = 1;
                continue;
            }
            if (!strcmp(longopt, "script")) {
                module = 0;
                continue;
            }
            if (opt == 'd' || !strcmp(longopt, "dump")) {
                dump_memory++;
                continue;
            }
            if (opt == 'D' || !strcmp(longopt, "dump-flags")) {
                dump_flags = opt_arg ? strtol(opt_arg, NULL, 16) : 0;
                break;
            }
            if (opt == 'T' || !strcmp(longopt, "trace")) {
                trace_memory++;
                continue;
            }
            if (!strcmp(longopt, "std")) {
                load_std = 1;
                continue;
            }
            if (!strcmp(longopt, "unhandled-rejection")) {
                dump_unhandled_promise_rejection = 1;
                continue;
            }
            if (opt == 'q' || !strcmp(longopt, "quit")) {
                empty_run++;
                continue;
            }
            if (!strcmp(longopt, "memory-limit")) {
                if (!opt_arg) {
                    if (optind >= argc) {
                        fprintf(stderr, "expecting memory limit");
                        exit(1);
                    }
                    opt_arg = argv[optind++];
                }
                memory_limit = parse_limit(opt_arg);
                break;
            }
            if (!strcmp(longopt, "stack-size")) {
                if (!opt_arg) {
                    if (optind >= argc) {
                        fprintf(stderr, "expecting stack size");
                        exit(1);
                    }
                    opt_arg = argv[optind++];
                }
                stack_size = parse_limit(opt_arg);
                break;
            }
            if (opt) {
                fprintf(stderr, "qjs: unknown option '-%c'\n", opt);
            } else {
                fprintf(stderr, "qjs: unknown option '--%s'\n", longopt);
            }
            help();
        }
    }

    if (trace_memory) {
        js_trace_malloc_init(&trace_data);
        rt = JS_NewRuntime2(&trace_mf, &trace_data);
    } else {
        rt = JS_NewRuntime();
    }
    if (!rt) {
        fprintf(stderr, "qjs: cannot allocate JS runtime\n");
        exit(2);
    }
    if (memory_limit >= 0)
        JS_SetMemoryLimit(rt, (size_t)memory_limit);
    if (stack_size >= 0)
        JS_SetMaxStackSize(rt, (size_t)stack_size);
    if (dump_flags != 0)
        JS_SetDumpFlags(rt, dump_flags);
    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);
    ctx = JS_NewCustomContext(rt);
    if (!ctx) {
        fprintf(stderr, "qjs: cannot allocate JS context\n");
        exit(2);
    }

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

    if (dump_unhandled_promise_rejection) {
        JS_SetHostPromiseRejectionTracker(rt, js_std_promise_rejection_tracker,
                                          NULL);
    }

    if (!empty_run) {
        js_std_add_helpers(ctx, argc - optind, argv + optind);

        /* make 'std' and 'os' visible to non module code */
        if (load_std) {
            const char *str = "import * as std from 'std';\n"
                "import * as os from 'os';\n"
                "globalThis.std = std;\n"
                "globalThis.os = os;\n";
            eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
        }

        for(i = 0; i < include_count; i++) {
            if (eval_file(ctx, include_list[i], module))
                goto fail;
        }

        if (expr) {
            if (eval_buf(ctx, expr, strlen(expr), "<cmdline>", 0))
                goto fail;
        } else
        if (optind >= argc) {
            /* interactive mode */
            interactive = 1;
        } else {
            const char *filename;
            filename = argv[optind];
            if (eval_file(ctx, filename, module))
                goto fail;
        }
        if (interactive) {
            js_std_eval_binary(ctx, qjsc_repl, qjsc_repl_size, 0);
        }
        js_std_loop(ctx);
    }

    if (dump_memory) {
        JSMemoryUsage stats;
        JS_ComputeMemoryUsage(rt, &stats);
        JS_DumpMemoryUsage(stdout, &stats, rt);
    }
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    if (empty_run && dump_memory) {
        clock_t t[5];
        double best[5] = {0};
        int i, j;
        for (i = 0; i < 100; i++) {
            t[0] = clock();
            rt = JS_NewRuntime();
            t[1] = clock();
            ctx = JS_NewContext(rt);
            t[2] = clock();
            JS_FreeContext(ctx);
            t[3] = clock();
            JS_FreeRuntime(rt);
            t[4] = clock();
            for (j = 4; j > 0; j--) {
                double ms = 1000.0 * (t[j] - t[j - 1]) / CLOCKS_PER_SEC;
                if (i == 0 || best[j] > ms)
                    best[j] = ms;
            }
        }
        printf("\nInstantiation times (ms): %.3f = %.3f+%.3f+%.3f+%.3f\n",
               best[1] + best[2] + best[3] + best[4],
               best[1], best[2], best[3], best[4]);
    }
    return 0;
 fail:
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 1;
}
