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
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "cutils.h"
#include "quickjs.h"
#include "quickjs-libc.h"
#include "qjs-common.h"

#ifdef QJS_USE_MIMALLOC
#include <mimalloc.h>
#endif

extern const uint8_t qjsc_repl[];
extern const uint32_t qjsc_repl_size;
static int qjs__argc;
static char **qjs__argv;

#ifndef QJS_NO_MAIN
extern const uint8_t qjsc_standalone[];
extern const uint32_t qjsc_standalone_size;

// Must match standalone.js
#define TRAILER_SIZE 12
static const char trailer_magic[] = "quickjs2";
static const int trailer_magic_size = sizeof(trailer_magic) - 1;
static const int trailer_size = TRAILER_SIZE;

static bool is_standalone(const char *exe)
{
    FILE *exe_f = fopen(exe, "rb");
    if (!exe_f)
        return false;
    if (fseek(exe_f, -trailer_size, SEEK_END) < 0)
        goto fail;
    uint8_t buf[TRAILER_SIZE];
    if (fread(buf, 1, trailer_size, exe_f) != trailer_size)
        goto fail;
    fclose(exe_f);
    return !memcmp(buf, trailer_magic, trailer_magic_size);
fail:
    fclose(exe_f);
    return false;
}

static JSValue load_standalone_module(JSContext *ctx)
{
    JSModuleDef *m;
    JSValue obj, val;
    obj = JS_ReadObject(ctx, qjsc_standalone, qjsc_standalone_size, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(obj))
        goto exception;
    assert(JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE);
    if (JS_ResolveModule(ctx, obj) < 0) {
        JS_FreeValue(ctx, obj);
        goto exception;
    }
    if (js_module_set_import_meta(ctx, obj, false, true) < 0) {
        JS_FreeValue(ctx, obj);
        goto exception;
    }
    val = JS_EvalFunction(ctx, JS_DupValue(ctx, obj));
    val = js_std_await(ctx, val);

    if (JS_IsException(val)) {
        JS_FreeValue(ctx, obj);
    exception:
        js_std_dump_error(ctx);
        exit(1);
    }
    JS_FreeValue(ctx, val);

    m = JS_VALUE_GET_PTR(obj);
    JS_FreeValue(ctx, obj);
    return JS_GetModuleNamespace(ctx, m);
}
#endif /* !QJS_NO_MAIN */

int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len,
                 const char *filename, int eval_flags)
{
    bool use_realpath;
    JSValue val;
    int ret;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
                      eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            // ex. "<cmdline>" pr "/dev/stdin"
            use_realpath =
                !(*filename == '<' || !strncmp(filename, "/dev/", 5));
            if (js_module_set_import_meta(ctx, val, use_realpath, true) < 0) {
                js_std_dump_error(ctx);
                ret = -1;
                goto end;
            }
            val = JS_EvalFunction(ctx, val);
        }
        val = js_std_await(ctx, val);
    } else {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        ret = -1;
    } else {
        ret = 0;
    }
end:
    JS_FreeValue(ctx, val);
    return ret;
}

int qjs_eval_file(JSContext *ctx, const char *filename, int module)
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
        module = (js__has_suffix(filename, ".mjs") ||
                  JS_DetectModule((const char *)buf, buf_len));
    }
    if (module)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;
    ret = qjs_eval_buf(ctx, buf, buf_len, filename, eval_flags);
    js_free(ctx, buf);
    return ret;
}

int64_t qjs_parse_limit(const char *arg) {
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

void qjs_config_init(QJSConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->module = -1;
    cfg->memory_limit = -1;
    cfg->stack_size = -1;
}

int qjs_config_parse_args(QJSConfig *cfg, int argc, char **argv, int *poptind)
{
    int optind = *poptind;

    /* cannot use getopt because we want to pass the command line to
       the script */
    while (optind < argc && *argv[optind] == '-') {
        char *arg = argv[optind] + 1;
        const char *longopt = "";
        char *optarg = NULL;
        /* a single - is not an option, it also stops argument scanning */
        if (!*arg)
            break;
        optind++;
        if (*arg == '-') {
            longopt = arg + 1;
            optarg = strchr(longopt, '=');
            if (optarg)
                *optarg++ = '\0';
            arg += strlen(arg);
            /* -- stops argument scanning */
            if (!*longopt)
                break;
        }
        for (; *arg || *longopt; longopt = "") {
            char opt = *arg;
            if (opt) {
                arg++;
                if (!optarg && *arg)
                    optarg = arg;
            }
#ifndef QJS_NO_MAIN
            if (opt == 'h' || opt == '?' || !strcmp(longopt, "help")) {
                return QJS_PARSE_HELP;
            }
#endif
            if (opt == 'e' || !strcmp(longopt, "eval")) {
                if (!optarg) {
                    if (optind >= argc) {
                        fprintf(stderr, "qjs: missing expression for -e\n");
                        return QJS_PARSE_ERROR;
                    }
                    optarg = argv[optind++];
                }
                cfg->expr = optarg;
                break;
            }
            if (opt == 'I' || !strcmp(longopt, "include")) {
                if (optind >= argc) {
                    fprintf(stderr, "expecting filename");
                    return QJS_PARSE_ERROR;
                }
                if (cfg->include_count >= QJS_INCLUDE_MAX) {
                    fprintf(stderr, "too many included files");
                    return QJS_PARSE_ERROR;
                }
                cfg->include_list[cfg->include_count++] = argv[optind++];
                continue;
            }
#ifndef QJS_NO_MAIN
            if (opt == 'i' || !strcmp(longopt, "interactive")) {
                cfg->interactive++;
                continue;
            }
#endif
            if (opt == 'm' || !strcmp(longopt, "module")) {
                cfg->module = 1;
                continue;
            }
#ifndef QJS_NO_MAIN
            if (opt == 'C' || !strcmp(longopt, "script")) {
                cfg->module = 0;
                continue;
            }
            if (opt == 'd' || !strcmp(longopt, "dump")) {
                cfg->dump_memory++;
                continue;
            }
            if (opt == 'D' || !strcmp(longopt, "dump-flags")) {
                cfg->dump_flags = optarg ? strtol(optarg, NULL, 16) : 0;
                break;
            }
            if (opt == 'T' || !strcmp(longopt, "trace")) {
                cfg->trace_memory++;
                continue;
            }
#endif
            if (!strcmp(longopt, "std")) {
                cfg->load_std = 1;
                continue;
            }
#ifndef QJS_NO_MAIN
            if (opt == 'q' || !strcmp(longopt, "quit")) {
                cfg->empty_run++;
                continue;
            }
#endif
            if (!strcmp(longopt, "memory-limit")) {
                if (!optarg) {
                    if (optind >= argc) {
                        fprintf(stderr, "expecting memory limit");
                        return QJS_PARSE_ERROR;
                    }
                    optarg = argv[optind++];
                }
                cfg->memory_limit = qjs_parse_limit(optarg);
                break;
            }
            if (!strcmp(longopt, "stack-size")) {
                if (!optarg) {
                    if (optind >= argc) {
                        fprintf(stderr, "expecting stack size");
                        return QJS_PARSE_ERROR;
                    }
                    optarg = argv[optind++];
                }
                cfg->stack_size = qjs_parse_limit(optarg);
                break;
            }
#ifndef QJS_NO_MAIN
            if (opt == 'c' || !strcmp(longopt, "compile")) {
                if (!optarg) {
                    if (optind >= argc) {
                        fprintf(stderr, "qjs: missing file for -c\n");
                        return QJS_PARSE_ERROR;
                    }
                    optarg = argv[optind++];
                }
                cfg->compile_file = optarg;
                break;
            }
            if (opt == 'o' || !strcmp(longopt, "out")) {
                if (!optarg) {
                    if (optind >= argc) {
                        fprintf(stderr, "qjs: missing file for -o\n");
                        return QJS_PARSE_ERROR;
                    }
                    optarg = argv[optind++];
                }
                cfg->out = optarg;
                break;
            }
            if (!strcmp(longopt, "exe")) {
                if (!optarg) {
                    if (optind >= argc) {
                        fprintf(stderr, "qjs: missing file for --exe\n");
                        return QJS_PARSE_ERROR;
                    }
                    optarg = argv[optind++];
                }
                cfg->exe = optarg;
                break;
            }
            if (opt) {
                fprintf(stderr, "qjs: unknown option '-%c'\n", opt);
            } else {
                fprintf(stderr, "qjs: unknown option '--%s'\n", longopt);
            }
            return QJS_PARSE_HELP;
#else
            break; /* ignore unknown options in reactor builds */
#endif
        }
    }

    *poptind = optind;
    return QJS_PARSE_OK;
}

void qjs_set_argv(int argc, char **argv)
{
    qjs__argc = argc;
    qjs__argv = argv;
}

void qjs_setup_runtime(JSRuntime *rt)
{
    js_std_set_worker_new_context_func(qjs_new_context);
    js_std_init_handlers(rt);
    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader, js_module_check_attributes, NULL);
    JS_SetHostPromiseRejectionTracker(rt, js_std_promise_rejection_tracker, NULL);
}

int qjs_apply_config(JSContext *ctx, const QJSConfig *cfg, int argc, char **argv)
{
    int i;

    js_std_add_helpers(ctx, argc, argv);

    if (cfg->load_std) {
        const char *str =
            "import * as bjson from 'qjs:bjson';\n"
            "import * as std from 'qjs:std';\n"
            "import * as os from 'qjs:os';\n"
            "globalThis.bjson = bjson;\n"
            "globalThis.std = std;\n"
            "globalThis.os = os;\n";
        if (qjs_eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE))
            return -1;
    }

    for (i = 0; i < cfg->include_count; i++) {
        if (qjs_eval_file(ctx, cfg->include_list[i], 0))
            return -1;
    }

    if (cfg->expr) {
        int flags = cfg->module == 1 ? JS_EVAL_TYPE_MODULE : 0;
        if (qjs_eval_buf(ctx, cfg->expr, strlen(cfg->expr), "<cmdline>", flags))
            return -1;
    }

    return 0;
}

static JSValue js_gc(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv)
{
    JS_RunGC(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

static JSValue js_navigator_get_userAgent(JSContext *ctx, JSValueConst this_val)
{
    char version[32];
    snprintf(version, sizeof(version), "quickjs-ng/%s", JS_GetVersion());
    return JS_NewString(ctx, version);
}

static const JSCFunctionListEntry navigator_proto_funcs[] = {
    JS_CGETSET_DEF2("userAgent", js_navigator_get_userAgent, NULL, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Navigator", JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry global_obj[] = {
    JS_CFUNC_DEF("gc", 0, js_gc),
};

/* also used to initialize the worker context */
JSContext *qjs_new_context(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
    /* system modules */
    js_init_module_std(ctx, "qjs:std");
    js_init_module_os(ctx, "qjs:os");
    js_init_module_bjson(ctx, "qjs:bjson");

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyFunctionList(ctx, global, global_obj, countof(global_obj));
    JSValue args = JS_NewArray(ctx);
    int i;
    for(i = 0; i < qjs__argc; i++) {
        JS_SetPropertyUint32(ctx, args, i, JS_NewString(ctx, qjs__argv[i]));
    }
    JS_SetPropertyStr(ctx, global, "execArgv", args);
    JS_SetPropertyStr(ctx, global, "argv0", JS_NewString(ctx, qjs__argv[0]));
    JSValue navigator_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, navigator_proto, navigator_proto_funcs, countof(navigator_proto_funcs));
    JSValue navigator = JS_NewObjectProto(ctx, navigator_proto);
    JS_DefinePropertyValueStr(ctx, global, "navigator", navigator, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, navigator_proto);

    return ctx;
}

#ifndef QJS_NO_MAIN
struct trace_malloc_data {
    uint8_t *base;
};

static inline unsigned long long js_trace_malloc_ptr_offset(uint8_t *ptr,
                                                struct trace_malloc_data *dp)
{
    return ptr - dp->base;
}

static void JS_PRINTF_FORMAT_ATTR(2, 3) js_trace_malloc_printf(void *opaque, JS_PRINTF_FORMAT const char *fmt, ...)
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
                           js_trace_malloc_ptr_offset(ptr, opaque),
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

static void *js_trace_calloc(void *opaque, size_t count, size_t size)
{
    void *ptr;
    ptr = calloc(count, size);
    js_trace_malloc_printf(opaque, "C %zd %zd -> %p\n", count, size, ptr);
    return ptr;
}

static void *js_trace_malloc(void *opaque, size_t size)
{
    void *ptr;
    ptr = malloc(size);
    js_trace_malloc_printf(opaque, "A %zd -> %p\n", size, ptr);
    return ptr;
}

static void js_trace_free(void *opaque, void *ptr)
{
    if (!ptr)
        return;
    js_trace_malloc_printf(opaque, "F %p\n", ptr);
    free(ptr);
}

static void *js_trace_realloc(void *opaque, void *ptr, size_t size)
{
    js_trace_malloc_printf(opaque, "R %zd %p", size, ptr);
    ptr = realloc(ptr, size);
    js_trace_malloc_printf(opaque, " -> %p\n", ptr);
    return ptr;
}

static const JSMallocFunctions trace_mf = {
    js_trace_calloc,
    js_trace_malloc,
    js_trace_free,
    js_trace_realloc,
    js__malloc_usable_size
};
#endif /* !QJS_NO_MAIN */

#ifdef QJS_USE_MIMALLOC
static void *js_mi_calloc(void *opaque, size_t count, size_t size)
{
    return mi_calloc(count, size);
}

static void *js_mi_malloc(void *opaque, size_t size)
{
    return mi_malloc(size);
}

static void js_mi_free(void *opaque, void *ptr)
{
    if (!ptr)
        return;
    mi_free(ptr);
}

static void *js_mi_realloc(void *opaque, void *ptr, size_t size)
{
    return mi_realloc(ptr, size);
}

static const JSMallocFunctions mi_mf = {
    js_mi_calloc,
    js_mi_malloc,
    js_mi_free,
    js_mi_realloc,
    mi_malloc_usable_size
};
#endif

#define PROG_NAME "qjs"

void help(void)
{
    printf("QuickJS-ng version %s\n"
           "usage: " PROG_NAME " [options] [file [args]]\n"
           "-h  --help         list options\n"
           "-e  --eval EXPR    evaluate EXPR\n"
           "-i  --interactive  go to interactive mode\n"
           "-C  --script       load as JS classic script (default=autodetect)\n"
           "-m  --module       load as ES module (default=autodetect)\n"
           "-I  --include file include an additional file\n"
           "    --std          make 'std', 'os' and 'bjson' available to script\n"
           "-T  --trace        trace memory allocation\n"
           "-d  --dump         dump the memory usage stats\n"
           "-D  --dump-flags   flags for dumping debug data (see DUMP_* defines)\n"
           "-c  --compile FILE compile the given JS file as a standalone executable\n"
           "-o  --out FILE     output file for standalone executables\n"
           "    --exe          select the executable to use as the base, defaults to the current one\n"
           "    --memory-limit n       limit the memory usage to 'n' Kbytes\n"
           "    --stack-size n         limit the stack size to 'n' Kbytes\n"
           "-q  --quit         just instantiate the interpreter and quit\n", JS_GetVersion());
    exit(1);
}

#ifndef QJS_NO_MAIN
int main(int argc, char **argv)
{
    JSRuntime *rt;
    JSContext *ctx;
    JSValue ret = JS_UNDEFINED;
    struct trace_malloc_data trace_data = { NULL };
    QJSConfig cfg;
    int r = 0;
    int optind = 1;
    char exebuf[JS__PATH_MAX];
    size_t exebuf_size = sizeof(exebuf);
    char *dump_flags_str = NULL;
    int standalone = 0;
    int i;

    /* save for later */
    qjs_set_argv(argc, argv);

    qjs_config_init(&cfg);

    /* check if this is a standalone executable */
    if (!js_exepath(exebuf, &exebuf_size) && is_standalone(exebuf)) {
        standalone = 1;
        goto start;
    }

    dump_flags_str = getenv("QJS_DUMP_FLAGS");
    cfg.dump_flags = dump_flags_str ? strtol(dump_flags_str, NULL, 16) : 0;

    r = qjs_config_parse_args(&cfg, argc, argv, &optind);
    if (r == QJS_PARSE_HELP) {
        help();
    } else if (r == QJS_PARSE_ERROR) {
        exit(1);
    }

    if (cfg.compile_file && !cfg.out)
        help();

start:

    if (cfg.trace_memory) {
        js_trace_malloc_init(&trace_data);
        rt = JS_NewRuntime2(&trace_mf, &trace_data);
    } else {
#ifdef QJS_USE_MIMALLOC
        rt = JS_NewRuntime2(&mi_mf, NULL);
#else
        rt = JS_NewRuntime();
#endif
    }
    if (!rt) {
        fprintf(stderr, "qjs: cannot allocate JS runtime\n");
        exit(2);
    }
    if (cfg.memory_limit >= 0)
        JS_SetMemoryLimit(rt, (size_t)cfg.memory_limit);
    if (cfg.stack_size >= 0)
        JS_SetMaxStackSize(rt, (size_t)cfg.stack_size);
    if (cfg.dump_flags != 0)
        JS_SetDumpFlags(rt, cfg.dump_flags);

    qjs_setup_runtime(rt);

    ctx = qjs_new_context(rt);
    if (!ctx) {
        fprintf(stderr, "qjs: cannot allocate JS context\n");
        exit(2);
    }

    if (!cfg.empty_run) {
        js_std_add_helpers(ctx, argc - optind, argv + optind);

        /* make 'std' and 'os' visible to non module code */
        if (cfg.load_std) {
            const char *str =
                "import * as bjson from 'qjs:bjson';\n"
                "import * as std from 'qjs:std';\n"
                "import * as os from 'qjs:os';\n"
                "globalThis.bjson = bjson;\n"
                "globalThis.std = std;\n"
                "globalThis.os = os;\n";
            qjs_eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
        }

        for (i = 0; i < cfg.include_count; i++) {
            if (qjs_eval_file(ctx, cfg.include_list[i], 0))
                goto fail;
        }

        if (standalone) {
            JSValue ns = load_standalone_module(ctx);
            if (JS_IsException(ns))
                goto fail;
            JSValue func = JS_GetPropertyStr(ctx, ns, "runStandalone");
            JS_FreeValue(ctx, ns);
            if (JS_IsException(func))
                goto fail;
            ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
            JS_FreeValue(ctx, func);
        } else if (cfg.compile_file) {
            JSValue ns = load_standalone_module(ctx);
            if (JS_IsException(ns))
                goto fail;
            JSValue func = JS_GetPropertyStr(ctx, ns, "compileStandalone");
            JS_FreeValue(ctx, ns);
            if (JS_IsException(func))
                goto fail;
            JSValue args[3];
            args[0] = JS_NewString(ctx, cfg.compile_file);
            args[1] = JS_NewString(ctx, cfg.out);
            args[2] = cfg.exe != NULL ? JS_NewString(ctx, cfg.exe) : JS_UNDEFINED;
            ret = JS_Call(ctx, func, JS_UNDEFINED, 3, (JSValueConst *)args);
            JS_FreeValue(ctx, func);
            JS_FreeValue(ctx, args[0]);
            JS_FreeValue(ctx, args[1]);
            JS_FreeValue(ctx, args[2]);
        } else if (cfg.expr) {
            int flags = cfg.module == 1 ? JS_EVAL_TYPE_MODULE : 0;
            if (qjs_eval_buf(ctx, cfg.expr, strlen(cfg.expr), "<cmdline>", flags))
                goto fail;
        } else if (optind >= argc) {
            /* interactive mode */
            cfg.interactive = 1;
        } else {
            const char *filename;
            filename = argv[optind];
            if (qjs_eval_file(ctx, filename, cfg.module))
                goto fail;
        }
        if (cfg.interactive) {
            JS_SetHostPromiseRejectionTracker(rt, NULL, NULL);
            js_std_eval_binary(ctx, qjsc_repl, qjsc_repl_size, 0);
        }
        if (standalone || cfg.compile_file) {
            if (JS_IsException(ret)) {
                r = 1;
            } else {
                JS_FreeValue(ctx, ret);
                r = js_std_loop(ctx);
            }
        } else {
            r = js_std_loop(ctx);
        }
        if (r) {
            js_std_dump_error(ctx);
            goto fail;
        }
    }

    if (cfg.dump_memory) {
        JSMemoryUsage stats;
        JS_ComputeMemoryUsage(rt, &stats);
        JS_DumpMemoryUsage(stdout, &stats, rt);
    }
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    if (cfg.empty_run && cfg.dump_memory) {
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
#endif /* QJS_NO_MAIN */
