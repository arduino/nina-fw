#!/bin/sh
set -eu

script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH='' cd -- "$script_dir/../.." && pwd)"

qemu_root="${QEMU_ROOT:-$repo_root/build/qemu}"
source_dir="$qemu_root/source"
build_dir="$qemu_root/build"
calib_repository="${CALIB_REPOSITORY:-https://github.com/Ebiroll/calib.git}"
calib_ref="${CALIB_REF:-73bee9bab5a78b0aaa584610b1564d533e7879ce}"

for command_name in git make python3 pkg-config; do
  command -v "$command_name" >/dev/null 2>&1 || {
    echo "error: required command not found: $command_name" >&2
    exit 2
  }
done

if [ -z "${QEMU_JOBS:-}" ]; then
  if command -v nproc >/dev/null 2>&1; then
    QEMU_JOBS=$(nproc)
  elif command -v sysctl >/dev/null 2>&1; then
    QEMU_JOBS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 2)
  else
    QEMU_JOBS=2
  fi
fi

mkdir -p "$qemu_root"
if [ ! -e "$source_dir" ]; then
  mkdir "$source_dir"
  git -C "$source_dir" init -q
  git -C "$source_dir" remote add origin "$calib_repository"
  new_checkout=1
elif [ ! -d "$source_dir/.git" ]; then
  echo "error: $source_dir exists but is not a Git checkout" >&2
  exit 2
else
  new_checkout=0
fi

actual_repository="$(git -C "$source_dir" remote get-url origin)"
if [ "$actual_repository" != "$calib_repository" ]; then
  echo "error: $source_dir uses unexpected origin: $actual_repository" >&2
  echo "expected: $calib_repository" >&2
  exit 2
fi

if [ "$new_checkout" -eq 0 ] && [ -n "$(git -C "$source_dir" status --porcelain --untracked-files=no)" ]; then
  echo "error: refusing to modify dirty managed checkout: $source_dir" >&2
  exit 2
fi

if ! git -C "$source_dir" cat-file -e "$calib_ref^{commit}" 2>/dev/null; then
  git -C "$source_dir" fetch --depth 1 --no-tags origin "$calib_ref"
fi
git -C "$source_dir" checkout --detach "$calib_ref"

for rom in esp32-v3-rom.bin esp32-v3-rom-app.bin; do
  if [ ! -f "$source_dir/pc-bios/$rom" ]; then
    echo "error: required ESP32 ROM is missing: $source_dir/pc-bios/$rom" >&2
    exit 2
  fi
done

# Homebrew does not always add keg-only pkg-config directories globally.
if command -v brew >/dev/null 2>&1; then
  for formula in libgcrypt libslirp; do
    formula_prefix=$(brew --prefix "$formula" 2>/dev/null || true)
    if [ -n "$formula_prefix" ]; then
      PKG_CONFIG_PATH="$formula_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    fi
  done
  export PKG_CONFIG_PATH
fi

pkg-config --exists libgcrypt || {
  echo "error: libgcrypt development files are required" >&2
  exit 2
}
pkg-config --exists slirp || {
  echo "error: libslirp development files are required" >&2
  exit 2
}

mkdir -p "$build_dir"
set -- \
  --target-list=xtensa-softmmu \
  --enable-gcrypt \
  --enable-slirp \
  --disable-gtk \
  --disable-werror
if [ "$(uname -s)" = Darwin ]; then
  set -- "$@" --disable-fuse --disable-fuse-lseek
fi

signature_file=$build_dir/.nina-qemu-config
expected_signature=$(printf '%s\n' "$calib_ref" "$@")
configuration_current=0

if [ -f "$build_dir/build.ninja" ]; then
  if [ -f "$signature_file" ] && [ "$(cat "$signature_file")" = "$expected_signature" ]; then
    configuration_current=1
  elif [ ! -f "$signature_file" ] && [ -f "$build_dir/config.status" ]; then
    # Adopt a compatible build created before configuration stamps existed.
    configuration_current=1
    for configure_option in "$@"; do
      if ! grep -Fq "'$configure_option'" "$build_dir/config.status"; then
        configuration_current=0
        break
      fi
    done
  fi
fi

if [ "$configuration_current" -eq 1 ]; then
  printf '%s\n' "$expected_signature" > "$signature_file"
  echo "+ reusing existing QEMU configuration"
elif [ -e "$build_dir/build.ninja" ]; then
  echo "error: QEMU configuration changed; remove $build_dir and run qemu-prepare again" >&2
  exit 2
else
  echo "+ configuring Ebiroll/calib at $calib_ref"
  (cd "$build_dir" && "$source_dir/configure" "$@")
  printf '%s\n' "$expected_signature" > "$signature_file"
fi

echo "+ building qemu-system-xtensa with $QEMU_JOBS jobs"
make -C "$build_dir" -j"$QEMU_JOBS" qemu-system-xtensa

qemu_binary=$build_dir/qemu-system-xtensa
if [ ! -x "$qemu_binary" ]; then
  echo "error: QEMU build did not produce $qemu_binary" >&2
  exit 2
fi

echo "QEMU ready: $qemu_binary"
echo "ESP32 ROMs: $source_dir/pc-bios"
