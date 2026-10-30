#!/bin/sh
set -eu

script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH='' cd -- "$script_dir/../.." && pwd)
build_dir=$repo_root/build

if [ -d "$build_dir" ]; then
  for path in "$build_dir"/* "$build_dir"/.[!.]* "$build_dir"/..?*; do
    [ -e "$path" ] || continue
    [ "$path" = "$build_dir/qemu" ] && continue
    rm -rf -- "$path"
  done
fi

rm -f -- \
  "$repo_root/x509_crt_bundle" \
  "$repo_root"/NINA_W102*

echo "Removed firmware build artifacts; preserved $build_dir/qemu"
