---
sidebar_position: 2
---

# Building

QuickJS uses [CMake] as its main build system, with an additional helper [Makefile].

:::note
Windows users will need to run the CMake commands manually.
:::

## Getting the source

```bash
git clone https://github.com/quickjs-ng/quickjs.git
cd quickjs
```

## Compiling everything

```bash
make
```

This will build the `qjs` and `qjsc` executables and other test tools. Head over [here](./cli) for
instructions on how to use them.

## Debug builds

```bash
make debug
```

This will produce a debug build without optimizations, suitable for developers.

## Running test262

```bash
make test262
```

This will run the test262 suite.

```bash
make test262-update
```

This will run the test262 suite and update the error / pass report, useful after
implementing a new feature that would alter the result of the test suite.

[CMake]: https://cmake.org
[Makefile]: https://www.gnu.org/software/make/
