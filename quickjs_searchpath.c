/*
 * quickjs_searchpath.c - SearchPath extension for QuickJS
 *
 * Based on Lua's package.searchpath (loadlib.c:458-482)
 * Provides file searching functionality similar to Lua
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "quickjs.h"

/* Path configuration - identical to Lua */
#define JS_PATH_SEP     ";"     /* separator for path templates */
#define JS_PATH_MARK    "?"     /* mark for name substitution */
#ifdef _WIN32
    #define JS_DIRSEP   "\\"
#else
    #define JS_DIRSEP   "/"
#endif

/* Check if file exists and is readable - identical to Lua's readable() */
static int file_readable(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

/* Replace all occurrences of 'from' with 'to' in string */
static void str_replace_char(char *dest, const char *src, char from, char to) {
    while (*src) {
        *dest++ = (*src == from) ? to : *src;
        src++;
    }
    *dest = '\0';
}

/* Replace '?' with name in path template */
static int expand_template(char *dest, size_t destsize,
                           const char *template, const char *name) {
    const char *p = template;
    size_t name_len = strlen(name);
    size_t written = 0;

    while (*p) {
        if (*p == '?') {
            if (written + name_len >= destsize) return 0;
            strcpy(dest + written, name);
            written += name_len;
            p++;
        } else {
            if (written >= destsize - 1) return 0;
            dest[written++] = *p++;
        }
    }
    dest[written] = '\0';
    return 1;
}

/* Main searchpath implementation - aligned with Lua */
static JSValue js_searchpath(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    const char *name;
    const char *path_template;
    char name_normalized[PATH_MAX];
    char expanded_path[PATH_MAX * 2];
    char single_template[PATH_MAX];
    char final_path[PATH_MAX];
    const char *template_start;
    const char *template_end;

    /* Get required arguments */
    name = JS_ToCString(ctx, argv[0]);
    if (!name)
        return JS_EXCEPTION;

    path_template = JS_ToCString(ctx, argv[1]);
    if (!path_template) {
        JS_FreeCString(ctx, name);
        return JS_EXCEPTION;
    }

    /* Normalize name: replace '.' with '/' (like Lua does) */
    str_replace_char(name_normalized, name, '.', '/');

    /* Process each template in path_template (separated by ';') */
    template_start = path_template;
    while (*template_start) {
        /* Find end of current template */
        template_end = strchr(template_start, ';');
        if (template_end == NULL) {
            template_end = template_start + strlen(template_start);
        }

        /* Extract single template */
        size_t template_len = template_end - template_start;
        if (template_len >= PATH_MAX) {
            template_start = (*template_end) ? template_end + 1 : template_end;
            continue;
        }
        strncpy(single_template, template_start, template_len);
        single_template[template_len] = '\0';

        /* Expand template by replacing '?' with normalized name */
        if (expand_template(final_path, sizeof(final_path),
                          single_template, name_normalized)) {
            /* Check if file exists */
            if (file_readable(final_path)) {
                /* Found it! Return the path */
                JS_FreeCString(ctx, name);
                JS_FreeCString(ctx, path_template);
                return JS_NewString(ctx, final_path);
            }
        }

        /* Move to next template */
        template_start = (*template_end) ? template_end + 1 : template_end;
    }

    /* Not found, return null */
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, path_template);
    return JS_NULL;
}

/* Initialize searchpath module */
void js_init_searchpath(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);

    /* Register global searchPath function */
    JS_SetPropertyStr(ctx, global, "searchPath",
                      JS_NewCFunction(ctx, js_searchpath, "searchPath", 2));

    /* Export path constants for user convenience */
    JSValue path_config = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, path_config, "sep", JS_NewString(ctx, JS_PATH_SEP));
    JS_SetPropertyStr(ctx, path_config, "mark", JS_NewString(ctx, JS_PATH_MARK));
    JS_SetPropertyStr(ctx, path_config, "dirsep", JS_NewString(ctx, JS_DIRSEP));
    JS_SetPropertyStr(ctx, global, "PATH_CONFIG", path_config);

    JS_FreeValue(ctx, global);
}

/* Optional: Convenience wrapper for module initialization */
int js_searchpath_init(JSContext *ctx, JSModuleDef *m) {
    js_init_searchpath(ctx);
    return 0;
}