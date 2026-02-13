#!/bin/sh

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <major.minor.patch>"
    exit 1
fi

VERSION="$1"
MAJOR=$(echo "$VERSION" | cut -d. -f1)
MINOR=$(echo "$VERSION" | cut -d. -f2)
PATCH=$(echo "$VERSION" | cut -d. -f3)

if [ -z "$MAJOR" ] || [ -z "$MINOR" ] || [ -z "$PATCH" ]; then
    echo "Error: version must be in major.minor.patch format"
    exit 1
fi

sed -i.bak \
    -e "s/^#define QJS_VERSION_MAJOR .*/#define QJS_VERSION_MAJOR $MAJOR/" \
    -e "s/^#define QJS_VERSION_MINOR .*/#define QJS_VERSION_MINOR $MINOR/" \
    -e "s/^#define QJS_VERSION_PATCH .*/#define QJS_VERSION_PATCH $PATCH/" \
    quickjs.h

sed -i.bak \
    -e "s/^  version: '.*'/  version: '$VERSION'/" \
    meson.build

rm -f quickjs.h.bak meson.build.bak

echo "Version updated to $VERSION"
