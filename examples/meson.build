shared_module(
  'fib',
  'fib.c',

  name_prefix: '',
  gnu_symbol_visibility: 'default',
  c_args: ['-DJS_SHARED_LIBRARY'],
  dependencies: host_system == 'windows' ? qjs_dep : [],
)

shared_module(
  'point',
  'point.c',

  name_prefix: '',
  gnu_symbol_visibility: 'default',
  c_args: ['-DJS_SHARED_LIBRARY'],
  dependencies: host_system == 'windows' ? qjs_dep : [],
)
