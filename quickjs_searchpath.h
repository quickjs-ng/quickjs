/*
 * quickjs_searchpath.h - SearchPath extension for QuickJS
 *
 * Provides Lua-like package.searchpath functionality
 */

#ifndef QUICKJS_SEARCHPATH_H
#define QUICKJS_SEARCHPATH_H

#include "quickjs.h"

/* Initialize searchpath functionality */
void js_init_searchpath(JSContext *ctx);

/* Module initialization (optional) */
int js_searchpath_init(JSContext *ctx, JSModuleDef *m);

#endif /* QUICKJS_SEARCHPATH_H */