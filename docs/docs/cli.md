---
sidebar_position: 4
---

# The qjs and qjsc CLI tools

## `qjs` - The QuickJS JavaScript interpreter

The `qjs` executable runs the JavaScript interpreter. It includes a simple standard
library and REPL.

```
$ qjs
usage: qjs [options] [file [args]]
-h  --help         list options
-e  --eval EXPR    evaluate EXPR
-i  --interactive  go to interactive mode
-m  --module       load as ES6 module (default=autodetect)
    --script       load as ES6 script (default=autodetect)
-I  --include file include an additional file
    --std          make 'std', 'os' and 'bjson' available to script
-T  --trace        trace memory allocation
-d  --dump         dump the memory usage stats
-D  --dump-flags   flags for dumping debug data (see DUMP_* defines)
-c  --compile FILE compile the given JS file as a standalone executable
-o  --out FILE     output file for standalone executables
    --exe          select the executable to use as the base, defaults to the current one
    --memory-limit n       limit the memory usage to 'n' Kbytes
    --stack-size n         limit the stack size to 'n' Kbytes
    --unhandled-rejection  dump unhandled promise rejections
-q  --quit         just instantiate the interpreter and quit
```

The following dump flags are supported:

```
DUMP_BYTECODE_FINAL   0x01  /* dump pass 3 final byte code */
DUMP_BYTECODE_PASS2   0x02  /* dump pass 2 code */
DUMP_BYTECODE_PASS1   0x04  /* dump pass 1 code */
DUMP_BYTECODE_HEX     0x10  /* dump bytecode in hex */
DUMP_BYTECODE_PC2LINE 0x20  /* dump line number table */
DUMP_BYTECODE_STACK   0x40  /* dump compute_stack_size */
DUMP_BYTECODE_STEP    0x80  /* dump executed bytecode */
DUMP_READ_OBJECT     0x100  /* dump the marshalled objects at load time */
DUMP_FREE            0x200  /* dump every object free */
DUMP_GC              0x400  /* dump the occurrence of the automatic GC */
DUMP_GC_FREE         0x800  /* dump objects freed by the GC */
DUMP_MODULE_RESOLVE 0x1000  /* dump module resolution steps */
DUMP_PROMISE        0x2000  /* dump promise steps */
DUMP_LEAKS          0x4000  /* dump leaked objects and strings in JS_FreeRuntime */
DUMP_ATOM_LEAKS     0x8000  /* dump leaked atoms in JS_FreeRuntime */
DUMP_MEM           0x10000  /* dump memory usage in JS_FreeRuntime */
DUMP_OBJECTS       0x20000  /* dump objects in JS_FreeRuntime */
DUMP_ATOMS         0x40000  /* dump atoms in JS_FreeRuntime */
DUMP_SHAPES        0x80000  /* dump shapes in JS_FreeRuntime */
```

### Creating standalone executables

With the `qjs` CLI it's possible to create standalone executables that will bundle the given JavaScript file
alongside the binary.

```
$ qjs -c app.js -o app --exe qjs
```

The resulting `app` binary will have the same runtime dependencies as the `qjs` binary. This is acomplished
by compiling the target JavaScript file to bytecode and adding it a copy of the executable, with a little
trailer to help locate it.

Rather than using the current executable, it's possible to use the `--exe` switch to create standalone
executables for other platforms.

No JavaScript bundling is performed, the specified JS file cannot depend on other files. A bundler such
as `esbuild` can be used to generate an app bundle which can then be turned into the executable.

```
npx esbuild my-app/index.js \
    --bundle \
    --outfile=app.js \
    --external:qjs:* \
    --minify \
    --target=es2023 \
    --platform=neutral \
    --format=esm \
    --main-fields=main,module
```

## `qjsc` - The QuickJS JavaScript compiler

The `qjsc` executable runs the JavaScript compiler, it can generate bytecode from
source files which can then be embedded in an executable, or it can generate the necessary
scaffolding to build a C application which embeds QuickJS.

```
$ qjsc
usage: qjsc [options] [files]

options are:
-b          output raw bytecode instead of C code
-e          output main() and bytecode in a C file
-o output   set the output filename
-n script_name    set the script name (as used in stack traces)
-N cname    set the C name of the generated data
-m          compile as JavaScript module (default=autodetect)
-D module_name         compile a dynamically loaded module or worker
-M module_name[,cname] add initialization code for an external C module
-p prefix   set the prefix of the generated C names
-s          strip the source code, specify twice to also strip debug info
-S n        set the maximum stack size to 'n' bytes (default=262144)
```

Here is an example on how to create a standalone executable that embeds QuickJS
and the `examples/hello.js` JavaScript file:

```bash
# Make sure you are in the QuickJS source directory.
$ cc hello.c cutils.c libbf.c libregexp.c libunicode.c quickjs.c quickjs-libc.c -I. -o hello
```

The resulting binary `hello` will be in the current directory.

```bash
$ ./hello
Hello World
```

:::note
See the ["Creating standalone executables"](#creating-standalone-executables) section for a simpler way.
:::
