#!/bin/sh
set -e
: ${QJS:=build/qjs}
"$QJS" v8.js $* 2>&1 | tee v8.txt$$
diff -uw v8.txt v8.txt$$ || exit 1
rm v8.txt$$
