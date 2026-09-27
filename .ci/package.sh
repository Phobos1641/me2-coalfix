#!/usr/bin/env bash
# Stage the release files for one platform into dist/<platform>/.
#
#   usage: bash ci/package.sh <build-dir> <platform>
#
# Executables are discovered through Meson's introspection data (every
# installed executable target), so no binary names are hard-coded here.
set -euo pipefail

build_dir=$1
platform=$2
out=dist/$platform

mkdir -p "$out"

meson introspect "$build_dir" --targets \
  | jq -r '.[] | select(.type == "executable" and .installed) | .filename[]' \
  | xargs -d '\n' cp -t "$out"

case $platform in
  windows-*)
    # winpthreads is linked statically and its licence asks for the notice to
    # travel with the binary. mingw-w64-crt ships the same upstream file.
    cp /usr/share/licenses/mingw-w64-winpthreads/COPYING.MinGW-w64-runtime.txt "$out/"
    ;;
esac

ls -l "$out"
