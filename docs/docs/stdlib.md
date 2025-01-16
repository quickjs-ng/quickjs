---
sidebar_position: 5
---

# Standard library

The standard libary is provided as part of the `qjs` executable and the `quickjs-libc.c` source file
and it's not part of the core engine.

## Globals

### `argv0`

Provides the executable path.

### `scriptArgs`

Provides the command line arguments. The first argument is the script name.

### `print(...args)`

Print the arguments separated by spaces and a trailing newline.

### `console.log(...args)`

Same as `print()`.

### `navigator.userAgent`

Returns `quickjs-ng/<version>`.

### `gc()`

Shorthand for `std.gc()`.

## `qjs:bjson` module

### `bjson.write(obj, [flags])`

Serializes the given object into the QuickJS internal serialization format.
Returns an ArrayBuffer with the serialized data.

Supported flags:

- `WRITE_OBJ_BYTECODE`: allow serializing functions and modules
- `WRITE_OBJ_REFERENCE`: allow serializing object references
- `WRITE_OBJ_SAB`: allow serializing SharedArrayBuffer instances
- `WRITE_OBJ_STRIP_DEBUG`: strip debugging information when serializing
- `WRITE_OBJ_STRIP_SOURCE`: strip the source information when serializing

### `bjson.read(buf, [pos], [len], [flags])`

De-serialize the given ArrayBuffer (in QuickJS internal serialization format) back into a JavaScript value.

Supported flags:

- `READ_OBJ_BYTECODE`: allow de-serializing functions and modules
- `READ_OBJ_REFERENCE`: allow de-serializing object references
- `READ_OBJ_SAB`: allow de-serializing SharedArrayBuffer instances

## `qjs:os` module

The `os` module provides Operating System specific functions:

- low level file access
- signals
- timers
- basic asynchronous I/O
- workers (threads)

The OS functions usually return 0 if OK or an OS specific negative
error code.

### `open(filename, flags, mode = 0o666)`

Open a file. Return a handle or < 0 if error.

Supported flags:

- `O_RDONLY`
- `O_WRONLY`
- `O_RDWR`
- `O_APPEND`
- `O_CREAT`
- `O_EXCL`
- `O_TRUNC`

POSIX open flags.

- `O_TEXT`

(Windows specific). Open the file in text mode. The default is binary mode.

### `close(fd)`

Close the file handle `fd`.

### `seek(fd, offset, whence)`

Seek in the file. Use `std.SEEK_*` for
`whence`. `offset` is either a number or a BigInt. If
`offset` is a BigInt, a BigInt is returned too.

### `read(fd, buffer, offset, length)`

Read `length` bytes from the file handle `fd` to the
ArrayBuffer `buffer` at byte position `offset`.
Return the number of read bytes or < 0 if error.

### `write(fd, buffer, offset, length)`

Write `length` bytes to the file handle `fd` from the
ArrayBuffer `buffer` at byte position `offset`.
Return the number of written bytes or < 0 if error.

### `isatty(fd)`

Return `true` is `fd` is a TTY (terminal) handle.

### `ttyGetWinSize(fd)`

Return the TTY size as `[width, height]` or `null` if not available.

### `ttySetRaw(fd)`

Set the TTY in raw mode.

### `remove(filename)`

Remove a file. Return 0 if OK or `-errno`.

### `rename(oldname, newname)`

Rename a file. Return 0 if OK or `-errno`.

### `realpath(path)`

Return `[str, err]` where `str` is the canonicalized absolute
pathname of `path` and `err` the error code.

### `getcwd()`

Return `[str, err]` where `str` is the current working directory
and `err` the error code.

### `chdir(path)`

Change the current directory. Return 0 if OK or `-errno`.

### `mkdir(path, mode = 0o777)`

Create a directory at `path`. Return 0 if OK or `-errno`.

### `stat(path)` / `lstat(path)`

Return `[obj, err]` where `obj` is an object containing the
file status of `path`. `err` is the error code. The
following fields are defined in `obj`: `dev`, `ino`, `mode`, `nlink`,
`uid`, `gid`, `rdev`, `size`, `blocks`, `atime`, `mtime`, `ctime`. The times are
specified in milliseconds since 1970. `lstat()` is the same as
`stat()` excepts that it returns information about the link
itself.

- `S_IFMT`
- `S_IFIFO`
- `S_IFCHR`
- `S_IFDIR`
- `S_IFBLK`
- `S_IFREG`
- `S_IFSOCK`
- `S_IFLNK`
- `S_ISGID`
- `S_ISUID`

Constants to interpret the `mode` property returned by
`stat()`. They have the same value as in the C system header
`sys/stat.h`.

### `utimes(path, atime, mtime)`

Change the access and modification times of the file `path`. The
times are specified in milliseconds since 1970. Return 0 if OK or `-errno`.

### `symlink(target, linkpath)`

Create a link at `linkpath` containing the string `target`. Return 0 if OK or `-errno`.

### `readlink(path)`

Return `[str, err]` where `str` is the link target and `err`
the error code.

### `readdir(path)`

Return `[array, err]` where `array` is an array of strings
containing the filenames of the directory `path`. `err` is
the error code.

### `setReadHandler(fd, func)`

Add a read handler to the file handle `fd`. `func` is called
each time there is data pending for `fd`. A single read handler
per file handle is supported. Use `func = null` to remove the
handler.

### `setWriteHandler(fd, func)`

Add a write handler to the file handle `fd`. `func` is
called each time data can be written to `fd`. A single write
handler per file handle is supported. Use `func = null` to remove
the handler.

### `signal(signal, func)`

Call the function `func` when the signal `signal`
happens. Only a single handler per signal number is supported. Use
`null` to set the default handler or `undefined` to ignore
the signal. Signal handlers can only be defined in the main thread.

- `SIGINT`
- `SIGABRT`
- `SIGFPE`
- `SIGILL`
- `SIGSEGV`
- `SIGTERM`

POSIX signal numbers.

### `kill(pid, sig)`

Send the signal `sig` to the process `pid`.

### `exec(args[, options])`

Execute a process with the arguments `args`. `options` is an
object containing optional parameters:

- `block` - Boolean (default = true). If true, wait until the process is
  terminated. In this case, `exec` return the exit code if positive
  or the negated signal number if the process was interrupted by a
  signal. If false, do not block and return the process id of the child.
- `usePath` - Boolean (default = true). If true, the file is searched in the
  `PATH` environment variable.
- `file` - String (default = `args[0]`). Set the file to be executed.
- `cwd` - String. If present, set the working directory of the new process.
- `stdin`, `stdout`, `stderr` - If present, set the handle in the child for stdin, stdout or stderr.
- `env` - Object. If present, set the process environment from the object
  key-value pairs. Otherwise use the same environment as the current
  process.
- `uid` - Integer. If present, the process uid with `setuid`.
- `gid` - Integer. If present, the process gid with `setgid`.

### `waitpid(pid, options)`

`waitpid` Unix system call. Return the array `[ret, status]`.
`ret` contains `-errno` in case of error.

- `WNOHANG`

Constant for the `options` argument of `waitpid`.

### `dup(fd)`

`dup` Unix system call.

### `dup2(oldfd, newfd)`

`dup2` Unix system call.

### `pipe()`

`pipe` Unix system call. Return two handles as `[read_fd, write_fd]` or null in case of error.

### `sleep(delay_ms)`

Sleep during `delay_ms` milliseconds.

### `sleepAsync(delay_ms)`

Asynchronouse sleep during `delay_ms` milliseconds. Returns a promise. Example:

```js
await os.sleepAsync(500);
```

### `setTimeout(func, delay)`

Call the function `func` after `delay` ms. Return a timer ID.

### `clearTimeout(id)`

Cancel a timer.

### `platform`

Return a string representing the platform: `"linux"`, `"darwin"`, `"win32"` or `"js"`.

### `Worker(module_filename)`

Constructor to create a new thread (worker) with an API close to that of
`WebWorkers`. `module_filename` is a string specifying the
module filename which is executed in the newly created thread. As for
dynamically imported module, it is relative to the current script or
module path. Threads normally don't share any data and communicate
between each other with messages. Nested workers are not supported. An
example is available in `tests/test_worker.js`.

The worker class has the following static properties:

- `parent` - In the created worker, `Worker.parent` represents the parent
  worker and is used to send or receive messages.

The worker instances have the following properties:

- `postMessage(msg)` - Send a message to the corresponding worker. `msg` is cloned in
  the destination worker using an algorithm similar to the `HTML`
  structured clone algorithm. `SharedArrayBuffer` are shared
  between workers.

- `onmessage` - Getter and setter. Set a function which is called each time a
  message is received. The function is called with a single
  argument. It is an object with a `data` property containing the
  received message. The thread is not terminated if there is at least
  one non `null` `onmessage` handler.

## `qjs:std` module

The `std` module provides wrappers to libc (`stdlib.h` and `stdio.h`) and a few other utilities.

### `exit(n)`

Exit the process.

### `evalScript(str, options = undefined)`

Evaluate the string `str` as a script (global
eval). `options` is an optional object containing the following
optional properties:

- `backtrace_barrier` - Boolean (default = false). If true, error backtraces do not list the
  stack frames below the evalScript.
- `async` - Boolean (default = false). If true, `await` is accepted in the
  script and a promise is returned. The promise is resolved with an
  object whose `value` property holds the value returned by the
  script.

### `loadScript(filename)`

Evaluate the file `filename` as a script (global eval).

### `loadFile(filename, [options])`

Load the file `filename` and return it as a string assuming UTF-8
encoding. Return `null` in case of I/O error.

If `options.binary` is set to `true` a `Uint8Array` is returned instead.

### `open(filename, flags, errorObj = undefined)`

Open a file (wrapper to the libc `fopen()`). Return the FILE
object or `null` in case of I/O error. If `errorObj` is not
undefined, set its `errno` property to the error code or to 0 if
no error occurred.

### `popen(command, flags, errorObj = undefined)`

Open a process by creating a pipe (wrapper to the libc
`popen()`). Return the FILE
object or `null` in case of I/O error. If `errorObj` is not
undefined, set its `errno` property to the error code or to 0 if
no error occurred.

### `fdopen(fd, flags, errorObj = undefined)`

Open a file from a file handle (wrapper to the libc
`fdopen()`). Return the FILE
object or null` in case of I/O error. If `errorObj` is not
undefined, set its `errno` property to the error code or to 0 if
no error occurred.

### `tmpfile(errorObj = undefined)`

Open a temporary file. Return the FILE
object or `null` in case of I/O error. If `errorObj` is not
undefined, set its `errno` property to the error code or to 0 if
no error occurred.

### `puts(str)`

Equivalent to `std.out.puts(str)`.

### `printf(fmt, ...args)`

Equivalent to `std.out.printf(fmt, ...args)`.

### `sprintf(fmt, ...args)`

Equivalent to the libc sprintf().

### `in`, `out`, `err`

Wrappers to the libc file `stdin`, `stdout`, `stderr`.

### `Error`

Enumeration object containing the integer value of common errors
(additional error codes may be defined):

- `EINVAL`
- `EIO`
- `EACCES`
- `EEXIST`
- `ENOSPC`
- `ENOSYS`
- `EBUSY`
- `ENOENT`
- `EPERM`
- `EPIPE`

### `strerror(errno)`

Return a string that describes the error `errno`.

### `gc()`

Manually invoke the cycle removal algorithm. The cycle removal
algorithm is automatically started when needed, so this function is
useful in case of specific memory constraints or for testing.

### `getenv(name)`

Return the value of the environment variable `name` or
`undefined` if it is not defined.

### `setenv(name, value)`

Set the value of the environment variable `name` to the string
`value`.

### `unsetenv(name)`

Delete the environment variable `name`.

### `getenviron()`

Return an object containing the environment variables as key-value pairs.

### `urlGet(url, options = undefined)`

Download `url` using the `curl` command line
utility. `options` is an optional object containing the following
optional properties:

- `binary` - Boolean (default = false). If true, the response is an ArrayBuffer
  instead of a string. When a string is returned, the data is assumed
  to be UTF-8 encoded.
- `full` - Boolean (default = false). If true, return the an object contains
  the properties `response` (response content),
  `responseHeaders` (headers separated by CRLF), `status`
  (status code). `response` is `null` is case of protocol or
  network error. If `full` is false, only the response is
  returned if the status is between 200 and 299. Otherwise `null`
  is returned.

### `FILE`

File object.

#### `close()`

Close the file. Return 0 if OK or `-errno` in case of I/O error.

#### `puts(str)`

Outputs the string with the UTF-8 encoding.

#### `printf(fmt, ...args)`

Formatted printf.

The same formats as the standard C library `printf` are
supported. Integer format types (e.g. `%d`) truncate the Numbers
or BigInts to 32 bits. Use the `l` modifier (e.g. `%ld`) to
truncate to 64 bits.

#### `flush()`

Flush the buffered file.

#### `seek(offset, whence)`

Seek to a give file position (whence is
`std.SEEK_*`). `offset` can be a number or a BigInt. Return
0 if OK or `-errno` in case of I/O error.

- `SEEK_SET`
- `SEEK_CUR`
- `SEEK_END`

Constants for seek().

#### `tell()`

Return the current file position.

#### `tello()`

Return the current file position as a BigInt.

#### `eof()`

Return true if end of file.

#### `fileno()`

Return the associated OS handle.

#### `error()`

Return true if there was an error.

#### `clearerr()`

Clear the error indication.

#### `read(buffer, position, length)`

Read `length` bytes from the file to the ArrayBuffer `buffer` at byte
position `position` (wrapper to the libc `fread`).

#### `write(buffer, position, length)`

Write `length` bytes to the file from the ArrayBuffer `buffer` at byte
position `position` (wrapper to the libc `fwrite`).

#### `getline()`

Return the next line from the file, assuming UTF-8 encoding, excluding
the trailing line feed.

#### `readAsString(max_size = undefined)`

Read `max_size` bytes from the file and return them as a string
assuming UTF-8 encoding. If `max_size` is not present, the file
is read up its end.

#### `getByte()`

Return the next byte from the file. Return -1 if the end of file is reached.

#### `putByte(c)`

Write one byte to the file.
