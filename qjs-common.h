/*
 * QuickJS shared initialization types and functions
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

#ifndef QJS_COMMON_H
#define QJS_COMMON_H

#include "quickjs.h"

#define QJS_INCLUDE_MAX 32

/* Return values for qjs_config_parse_args() */
#define QJS_PARSE_OK        0
#define QJS_PARSE_ERROR    -1
#define QJS_PARSE_HELP     -2  /* Help requested, caller should display help and exit */

/* Configuration for QJS initialization */
typedef struct {
    char *expr;
    int module;           /* -1 = auto, 0 = script, 1 = module */
    int load_std;
    char *include_list[QJS_INCLUDE_MAX];
    int include_count;
    int64_t memory_limit; /* -1 = default */
    int64_t stack_size;   /* -1 = default */
#ifndef QJS_NO_MAIN
    /* Main-specific options (not used in reactor builds) */
    int interactive;
    int dump_memory;
    int dump_flags;
    int trace_memory;
    int empty_run;
    char *compile_file;
    char *out;
    char *exe;
#endif
} QJSConfig;

/* Initialize config with defaults */
void qjs_config_init(QJSConfig *cfg);

/* Parse command line arguments into config. Updates *poptind.
 * Returns 0 on success, -1 on error. */
int qjs_config_parse_args(QJSConfig *cfg, int argc, char **argv, int *poptind);

/* Parse a size limit string (e.g., "256m", "1g"). Returns -1 on error. */
int64_t qjs_parse_limit(const char *arg);

/* Create a new custom context with std/os/bjson modules */
JSContext *qjs_new_context(JSRuntime *rt);

/* Set argc/argv for use by qjs_new_context (for execArgv global) */
void qjs_set_argv(int argc, char **argv);

/* Set up runtime with config (memory limit, stack size, module loader, etc.) */
void qjs_setup_runtime(JSRuntime *rt);

/* Apply config to context: load std modules, includes, eval expr/file.
 * Returns 0 on success, -1 on error. */
int qjs_apply_config(JSContext *ctx, const QJSConfig *cfg, int argc, char **argv);

/* Evaluate a buffer */
int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len,
                 const char *filename, int eval_flags);

/* Evaluate a file */
int qjs_eval_file(JSContext *ctx, const char *filename, int module);

#endif /* QJS_COMMON_H */
