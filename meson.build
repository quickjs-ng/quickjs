project(
  'quickjs-ng',
  'c',
  version: '0.8.0',
  default_options: [
    'c_std=gnu11,c11',
    'warning_level=3',
    'default_library=static',
  ],
  license: 'MIT',
  license_files: 'LICENSE',
  meson_version: '>=1.3.0',
)

host_system = host_machine.system()
cc = meson.get_compiler('c')

qjs_gcc_warning_args = [
  '-Wno-unsafe-buffer-usage',
  '-Wno-sign-conversion',
  '-Wno-nonportable-system-include-path',
  '-Wno-implicit-int-conversion',
  '-Wno-shorten-64-to-32',
  '-Wno-reserved-macro-identifier',
  '-Wno-reserved-identifier',
  '-Wdeprecated-declarations',

  '-Wno-implicit-fallthrough',
  '-Wno-sign-compare',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
  '-Wno-unused-but-set-variable',
  '-Wno-array-bounds',
  '-Wno-format-truncation',
]
qjs_gcc_args = [
  '-funsigned-char',
]

if host_system == 'darwin'
  # https://github.com/quickjs-ng/quickjs/issues/453
  qjs_gcc_warning_args += '-Wno-maybe-uninitialized'
endif

# https://github.com/microsoft/cpp-docs/tree/main/docs/error-messages/compiler-warnings
qjs_msvc_warning_args = [
  '/wd4018', # -Wno-sign-conversion
  '/wd4061', # -Wno-implicit-fallthrough
  '/wd4100', # -Wno-unused-parameter
  '/wd4200', # -Wno-zero-length-array
  '/wd4242', # -Wno-shorten-64-to-32
  '/wd4244', # -Wno-shorten-64-to-32
  '/wd4245', # -Wno-sign-compare
  '/wd4267', # -Wno-shorten-64-to-32
  '/wd4388', # -Wno-sign-compare
  '/wd4389', # -Wno-sign-compare
  '/wd4710', # Function not inlined
  '/wd4711', # Function was inlined
  '/wd4820', # Padding added after construct
  '/wd4996', # -Wdeprecated-declarations
  '/wd5045', # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
]
qjs_msvc_args = [
  '/experimental:c11atomics',
  '/J', # -funsigned-char
]

if cc.get_argument_syntax() == 'msvc'
  add_project_arguments(
    cc.get_supported_arguments(qjs_msvc_warning_args),
    cc.get_id().contains('clang') ? cc.get_supported_arguments(qjs_gcc_warning_args) : [],
    qjs_msvc_args,
    language: 'c',
  )
else
  add_project_arguments(
    cc.get_supported_arguments(qjs_gcc_warning_args),
    qjs_gcc_args,
    language: 'c',
  )
endif

if host_system == 'windows'
  # Set a 8MB default stack size on Windows.
  # It defaults to 1MB on MSVC, which is the same as our current JS stack size,
  # so it will overflow and crash otherwise.
  # On MinGW it defaults to 2MB.
  stack_size = 8 * 1024 * 1024
  if cc.get_argument_syntax() == 'msvc'
    add_project_link_arguments(f'/STACK:@stack_size@', language: 'c')
  else
    add_project_link_arguments(f'-Wl,--stack,@stack_size@', language: 'c')
  endif
endif

if meson.is_cross_build()
  native_cc = meson.get_compiler('c', native: true)

  if native_cc.get_argument_syntax() == 'msvc'
    # https://github.com/microsoft/cpp-docs/tree/main/docs/error-messages/compiler-warnings
    add_project_arguments(
      native_cc.get_supported_arguments(qjs_msvc_warning_args),
      native_cc.get_id().contains('clang') ? native_cc.get_supported_arguments(qjs_gcc_warning_args) : [],
      qjs_msvc_args,

      language: 'c',
      native: true,
    )
  else
    add_project_arguments(
      native_cc.get_supported_arguments(qjs_gcc_warning_args),
      qjs_gcc_args,

      language: 'c',
      native: true,
    )
  endif
endif
if get_option('debug')
  add_project_arguments(
    cc.get_supported_arguments('-fno-omit-frame-pointer'),
    language: 'c',
  )
endif
if get_option('disable_parser')
  add_project_arguments(
    ['-DQJS_DISABLE_PARSER'],
    language: 'c',
  )
endif

qjs_sys_deps = []

m_dep = cc.find_library('m', required: false)
qjs_sys_deps += m_dep
qjs_sys_deps += dependency('threads', required: false)
qjs_sys_deps += dependency('dl', required: false)

qjs_srcs = files(
  'cutils.c',
  'libregexp.c',
  'libunicode.c',
  'quickjs.c',
  'xsum.c'
)
qjs_hdrs = files(
  'quickjs.h',
)

qjs_libc = get_option('libc')
qjs_libc_srcs = files('quickjs-libc.c')
qjs_libc_hdrs = files('quickjs-libc.h')

if qjs_libc
  qjs_hdrs += qjs_libc_hdrs
endif

qjs_c_args = ['-D_GNU_SOURCE']

if host_system == 'windows'
  qjs_c_args += ['-DWIN32_LEAN_AND_MEAN', '-D_WIN32_WINNT=0x0602']
endif

qjs_libc_lib = static_library(
  'quickjs-libc',
  qjs_libc_srcs,

  dependencies: qjs_sys_deps,
  c_args: qjs_c_args,
  gnu_symbol_visibility: 'hidden',
)

qjs_lib = library(
  'qjs',
  qjs_srcs,

  # export public headers
  generator(
    find_program('cp', 'xcopy'),
    output: ['@PLAINNAME@'],
    arguments: ['@INPUT@', '@OUTPUT@'],
  ).process(qjs_hdrs),

  dependencies: qjs_sys_deps,
  link_whole: qjs_libc ? qjs_libc_lib : [],
  c_args: qjs_c_args,
  gnu_symbol_visibility: 'hidden',

  install: true,
  version: meson.project_version(),
)

qjs_dep = declare_dependency(
  link_with: qjs_lib,
  dependencies: qjs_sys_deps,
  include_directories: qjs_lib.private_dir_include(),
)

if host_system == 'emscripten'
  qjs_wasm_export_name = 'getQuickJs'
  executable(
    'qjs_wasm',
    qjs_srcs,
    link_args: cc.get_supported_link_arguments(
      # in emscripten 3.x, this will be set to 16k which is too small for quickjs.
      '-sSTACK_SIZE=@0@'.format(2 * 1024 * 1024), # let it be 2m = 2 * 1024 * 1024, otherwise, stack overflow may be occured at bootstrap
      '-sNO_INVOKE_RUN',
      '-sNO_EXIT_RUNTIME',
      '-sMODULARIZE', # do not mess the global
      '-sEXPORT_ES6', # export js file to morden es module
      '-sEXPORT_NAME=@0@'.format(qjs_wasm_export_name), # give a name
      '-sTEXTDECODER=1', # it will be 2 if we use -Oz, and that will cause js -> c string convertion fail
      '-sNO_DEFAULT_TO_CXX', # this project is pure c project, no need for c plus plus handle
      '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap',
    ),
    dependencies: m_dep,
    c_args: qjs_c_args,
  )
endif

install_headers(qjs_hdrs, subdir: 'quickjs-ng')

if not meson.is_subproject()
  docdir = get_option('docdir')
  datadir = get_option('datadir')

  if docdir == ''
    docdir = datadir / 'doc' / meson.project_name()
  endif
  install_data(
    'LICENSE',
    install_dir: docdir,

    install_tag: 'doc',
  )
  install_subdir(
    'examples',
    install_dir: docdir,

    strip_directory: false,
    install_tag: 'doc',
  )
endif

meson.override_dependency('quickjs-ng', qjs_dep)

import('pkgconfig').generate(
  qjs_lib,
  subdirs: 'quickjs-ng',
  name: 'quickjs-ng',
  description: 'QuickJS, the Next Generation: a mighty JavaScript engine',
  url: 'https://github.com/quickjs-ng/quickjs',
  version: meson.project_version(),
)

# QuickJS bytecode compiler
qjsc_srcs = files(
  'qjsc.c',
)
qjsc_exe = executable(
  'qjsc',
  qjsc_srcs,

  c_args: qjs_c_args,
  link_with: qjs_libc ? [] : qjs_libc_lib,
  dependencies: qjs_dep,

  install: true,
)

mimalloc_dep = []
mimalloc_sys_dep = dependency('mimalloc', required: get_option('cli_mimalloc'))
if mimalloc_sys_dep.found()
  mimalloc_dep = declare_dependency(
    dependencies: mimalloc_sys_dep,
    compile_args: '-DQJS_USE_MIMALLOC',
  )
endif

# QuickJS CLI
qjs_exe_srcs = files(
  'gen/repl.c',
  'gen/standalone.c',
  'qjs.c',
)
qjs_exe = executable(
  'qjs',
  qjs_exe_srcs,

  c_args: qjs_c_args,
  link_with: qjs_libc ? [] : qjs_libc_lib,
  dependencies: [qjs_dep, mimalloc_dep],

  install: true,
)

if meson.is_cross_build()
  mimalloc_native_dep = []
  mimalloc_sys_native_dep = dependency('mimalloc', required: get_option('cli_mimalloc'), native: true)
  if mimalloc_sys_dep.found()
    mimalloc_native_dep = declare_dependency(
      dependencies: mimalloc_sys_native_dep,
      compile_args: '-DQJS_USE_MIMALLOC',
    )
  endif

  qjs_sys_native_deps = [
    native_cc.find_library('m', required: false),
    dependency('threads', required: false, native: true),
    dependency('dl', required: false, native: true),
  ]
  qjs_native_lib = static_library(
    'qjs_native',
    qjs_srcs,
    qjs_libc_srcs,

    dependencies: qjs_sys_native_deps,
    c_args: qjs_c_args,
    gnu_symbol_visibility: 'hidden',

    build_by_default: false,
    native: true,
    install: false,
  )

  meson.override_find_program(
    'qjsc',
    executable(
      'qjsc_native',
      qjsc_srcs,

      c_args: qjs_c_args,
      link_with: qjs_native_lib,
      dependencies: qjs_sys_native_deps,

      build_by_default: false,
      native: true,
      install: false,
    ),
  )
  meson.override_find_program(
    'qjs',
    executable(
      'qjs_native',
      qjs_exe_srcs,

      c_args: qjs_c_args,
      link_with: qjs_native_lib,
      dependencies: [qjs_sys_native_deps, mimalloc_native_dep],

      build_by_default: false,
      native: true,
      install: false,
    ),
  )
else
  meson.override_find_program('qjsc', qjsc_exe)
  meson.override_find_program('qjs', qjs_exe)
endif

tests = get_option('tests').disable_auto_if(meson.is_subproject())
examples = get_option('examples').disable_auto_if(meson.is_subproject())

if tests.allowed()
  if host_system != 'emscripten'
    # Test262 runner
    run262_exe = executable(
      'run-test262',
      'run-test262.c',
      qjs_libc_srcs,

      c_args: qjs_c_args,
      dependencies: qjs_dep,
    )

    test(
      'test',
      run262_exe,
      args: ['-c', files('tests.conf')],
      workdir: meson.current_source_dir(),
    )

    foreach bench : [
      'empty_loop',

      'date_now',

      'prop_read',
      'prop_write',
      'prop_create',
      'prop_delete',

      'array_read',
      'array_write',
      'array_prop_create',
      'array_length_decr',
      'array_hole_length_decr',
      'array_push',
      'array_pop',

      'typed_array_read',
      'typed_array_write',

      'global_read',
      'global_write',
      'global_write_strict',

      'local_destruct',
      'global_destruct',
      'global_destruct_strict',

      'func_call',
      'closure_var',

      'int_arith',
      'float_arith',

      'set_collection_add',

      'array_for',
      'array_for_in',
      'array_for_of',

      'math_min',

      'regexp_ascii',
      'regexp_utf16',

      'string_build1',
      'string_build2',

      'sort_bench',

      'int_to_string',
      'int_toString',

      'float_to_string',
      'float_toString',
      'float_toFixed',
      'float_toPrecision',
      'float_toExponential',

      'string_to_int',
      'string_to_float',

      'bigint64_arith',
      'bigint256_arith',
    ]
      benchmark(
        bench,
        qjs_exe,
        args: [files('tests/microbench.js'), bench],
      )
    endforeach
  endif

  # API test
  test(
    'api',
    executable(
      'api-test',
      'api-test.c',

      c_args: qjs_c_args,
      dependencies: qjs_dep,
    ),
  )
endif

# Unicode generator
executable(
  'unicode_gen',
  'cutils.c',
  'libunicode.c',
  'unicode_gen.c',

  c_args: qjs_c_args,
  build_by_default: false,
)

executable(
  'function_source',
  'gen/function_source.c',
  qjs_libc_srcs,

  c_args: qjs_c_args,
  dependencies: qjs_dep,
)

if examples.allowed()
  executable(
    'hello',
    'gen/hello.c',
    qjs_libc_srcs,

    c_args: qjs_c_args,
    dependencies: qjs_dep,
  )

  executable(
    'hello_module',
    'gen/hello_module.c',
    qjs_libc_srcs,

    c_args: qjs_c_args,
    dependencies: qjs_dep,
  )

  subdir('examples')

  executable(
    'test_fib',
    'examples/fib.c',
    'gen/test_fib.c',
    qjs_libc_srcs,

    c_args: qjs_c_args,
    dependencies: qjs_dep,
    export_dynamic: true,
  )
endif
