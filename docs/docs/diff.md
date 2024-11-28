# Differences with bellard/quickjs

This project aims to be a drop-in replacement for those already using QuickJS.
Minimal API changes might be necessary.

## Community development

NG is developed in the open, interacting with the wider community and through
these interactions many improvements have already been made, including the incorporation
of patches previously maintained in other forks.

Each PR is reviewed, iterated on, and merged in GitHub.

To date, NG has had over 40 distinct contributors and over 400 PRs.

## Consistent release cadence

As the project moves forward, a steady cadence of releases has been maintained, with an
average of a new release every 2 months.

## Testing

Since its inception testing has been a focus. Each PR is tested in over 50 configurations,
involving different operating systems, build types and sanitizers.

The `test262` suite is also ran for every change.

## Cross-platform support

In order to better support other platforms such as Windows the build system was
changed to use [CMake].

In addition, Windows is treated as a first class citizen, with the addition of support
for the MSVC compiler.

[CMake]: https://cmake.org/

## Performance

While being an interpreter limits the performance in comparison with other engines which
use a JIT, several significant performance improvements have been made:

- Opcode fusion
- Polymorphic inline caching
- Memory allocation improvements
- Improved parse speeds

## New ECMAScript APIs

The main focus of NG is to deliver state-of-the-art JavaScript features. Typically once they
are stable (stage 4) but sometimes even at earlier stages. Here is a non-exhaustive list
of ES features present in NG:

- Resizable ArrayBuffer
- Float16Array
- WeakRef
- FinalizationRegistry
- Iterator Helpers
- Promise.try
- Error.isError
- Set operations

Some non-standard but widely used APIs have also been added:

- V8's [stack trace API](https://v8.dev/docs/stack-trace-api)
  - `Error.captureStackTrace`
  - `Error.prepareStackTrace`
  - `Error.stackTraceLimit`
