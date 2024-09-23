---
sidebar_position: 1
---

# The QuickJS C API

The C API was designed to be simple and efficient. The C API is
defined in the header `quickjs.h`.

## Runtime and contexts

`JSRuntime` represents a JavaScript runtime corresponding to an
object heap. Several runtimes can exist at the same time but they
cannot exchange objects. Inside a given runtime, no multi-threading is
supported.

`JSContext` represents a JavaScript context (or Realm). Each
JSContext has its own global objects and system objects. There can be
several JSContexts per JSRuntime and they can share objects, similar
to frames of the same origin sharing JavaScript objects in a
web browser.

## JSValue

`JSValue` represents a JavaScript value which can be a primitive
type or an object. Reference counting is used, so it is important to
explicitly duplicate (`JS_DupValue()`, increment the reference
count) or free (`JS_FreeValue()`, decrement the reference count)
JSValues.

## C functions

C functions can be created with
`JS_NewCFunction()`. `JS_SetPropertyFunctionList()` is a
shortcut to easily add functions, setters and getters properties to a
given object.

Unlike other embedded JavaScript engines, there is no implicit stack,
so C functions get their parameters as normal C parameters. As a
general rule, C functions take constant `JSValue`s as parameters
(so they don't need to free them) and return a newly allocated (=live)
`JSValue`.

## Exceptions

Most C functions can return a JavaScript exception. It
must be explicitly tested and handled by the C code. The specific
`JSValue` `JS_EXCEPTION` indicates that an exception
occurred. The actual exception object is stored in the
`JSContext` and can be retrieved with `JS_GetException()`.

## Script evaluation

Use `JS_Eval()` to evaluate a script or module source.

If the script or module was compiled to bytecode with `qjsc`, it
can be evaluated by calling `js_std_eval_binary()`. The advantage
is that no compilation is needed so it is faster and smaller because
the compiler can be removed from the executable if no `eval` is
required.

Note: the bytecode format is linked to a given QuickJS
version. Moreover, no security check is done before its
execution. Hence the bytecode should not be loaded from untrusted
sources.

## JS Classes

C opaque data can be attached to a JavaScript object. The type of the
C opaque data is determined with the class ID (`JSClassID`) of
the object. Hence the first step is to register a new class ID and JS
class (`JS_NewClassID()`, `JS_NewClass()`). Then you can
create objects of this class with `JS_NewObjectClass()` and get or
set the C opaque point with `JS_GetOpaque()` / `JS_SetOpaque()`.

When defining a new JS class, it is possible to declare a finalizer
which is called when the object is destroyed. The finalizer should be
used to release C resources. It is invalid to execute JS code from
it. A `gc_mark` method can be provided so that the cycle removal
algorithm can find the other objects referenced by this object. Other
methods are available to define exotic object behaviors.

The Class ID are allocated per-runtime. The
`JSClass` are allocated per `JSRuntime`. `JS_SetClassProto()`
is used to define a prototype for a given class in a given
`JSContext`. `JS_NewObjectClass()` sets this prototype in the
created object.

Examples are available in `quickjs-libc.c`.

## C Modules

Native ES6 modules are supported and can be dynamically or statically
linked. The standard library `quickjs-libc.c` is a good example
of a native module.

## Memory handling

Use `JS_SetMemoryLimit()` to set a global memory allocation limit
to a given `JSRuntime`.

Custom memory allocation functions can be provided with `JS_NewRuntime2()`.

The maximum system stack size can be set with `JS_SetMaxStackSize()`.

## Execution timeout and interrupts

Use `JS_SetInterruptHandler()` to set a callback which is
regularly called by the engine when it is executing code. This
callback can be used to implement an execution timeout.

It is used by the command line interpreter to implement a
`Ctrl-C` handler.
