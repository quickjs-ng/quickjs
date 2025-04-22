import {loadFile, writeFile} from "qjs:std"

const cutils_c = loadFile("cutils.c")
const cutils_h = loadFile("cutils.h")
const libregexp_c = loadFile("libregexp.c")
const libregexp_h = loadFile("libregexp.h")
const libregexp_opcode_h = loadFile("libregexp-opcode.h")
const libunicode_c = loadFile("libunicode.c")
const libunicode_h = loadFile("libunicode.h")
const libunicode_table_h = loadFile("libunicode-table.h")
const list_h = loadFile("list.h")
const quickjs_atom_h = loadFile("quickjs-atom.h")
const quickjs_c = loadFile("quickjs.c")
const quickjs_c_atomics_h = loadFile("quickjs-c-atomics.h")
const quickjs_h = loadFile("quickjs.h")
const quickjs_libc_c = loadFile("quickjs-libc.c")
const quickjs_libc_h = loadFile("quickjs-libc.h")
const quickjs_opcode_h = loadFile("quickjs-opcode.h")
const xsum_c = loadFile("xsum.c")
const xsum_h = loadFile("xsum.h")
const gen_builtin_array_fromasync_h = loadFile("builtin-array-fromasync.h")

let source = "#if defined(QJS_BUILD_LIBC) && defined(__linux__) && !defined(_GNU_SOURCE)\n"
           + "#define _GNU_SOURCE\n"
           + "#endif\n"
           + quickjs_c_atomics_h
           + cutils_h
           + list_h
           + libunicode_h // exports lre_is_id_start, used by libregexp.h
           + libregexp_h
           + libunicode_table_h
           + xsum_h
           + quickjs_h
           + quickjs_c
           + cutils_c
           + libregexp_c
           + libunicode_c
           + xsum_c
           + "#ifdef QJS_BUILD_LIBC\n"
           + quickjs_libc_h
           + quickjs_libc_c
           + "#endif // QJS_BUILD_LIBC\n"
source = source.replace(/#include "quickjs-atom.h"/g, quickjs_atom_h)
source = source.replace(/#include "quickjs-opcode.h"/g, quickjs_opcode_h)
source = source.replace(/#include "libregexp-opcode.h"/g, libregexp_opcode_h)
source = source.replace(/#include "builtin-array-fromasync.h"/g,
                        gen_builtin_array_fromasync_h)
source = source.replace(/#include "[^"]+"/g, "")
writeFile(execArgv[2] ?? "quickjs-amalgam.c", source)
