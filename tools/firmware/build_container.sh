#!/bin/sh
set -eu

usage() {
  echo "usage: $0 [--check] [--target MAKE_TARGET] [SOURCE_DIRECTORY]" >&2
  exit 2
}

check_only=0
make_target=firmware
while [ "$#" -gt 0 ]; do
  case "$1" in
    --check)
      check_only=1
      shift
      ;;
    --target)
      [ "$#" -ge 2 ] || usage
      make_target=$2
      shift 2
      ;;
    --)
      shift
      break
      ;;
    -*)
      usage
      ;;
    *)
      break
      ;;
  esac
done

if { [ "$check_only" -eq 0 ] && [ "$#" -ne 1 ]; } ||
   { [ "$check_only" -eq 1 ] && [ "$#" -ne 0 ]; }; then
  usage
fi

engine=${FIRMWARE_CONTAINER_ENGINE:-auto}
image=${FIRMWARE_IMAGE:-espressif/idf:v4.4.8}

if [ "$engine" = auto ]; then
  if command -v container >/dev/null 2>&1 && container list >/dev/null 2>&1; then
    engine=container
  elif command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
    engine=docker
  elif command -v podman >/dev/null 2>&1 && podman info >/dev/null 2>&1; then
    engine=podman
  else
    echo "error: no running Apple container, Docker, or Podman engine found" >&2
    exit 2
  fi
fi

case "$engine" in
  container)
    command -v container >/dev/null 2>&1 || { echo "error: Apple container is not installed" >&2; exit 2; }
    container list >/dev/null 2>&1 || { echo "error: start Apple container with: container system start" >&2; exit 2; }
    ;;
  docker)
    command -v docker >/dev/null 2>&1 || { echo "error: Docker is not installed" >&2; exit 2; }
    docker info >/dev/null 2>&1 || { echo "error: the Docker daemon is unavailable" >&2; exit 2; }
    ;;
  podman)
    command -v podman >/dev/null 2>&1 || { echo "error: Podman is not installed" >&2; exit 2; }
    podman info >/dev/null 2>&1 || { echo "error: the Podman service or machine is unavailable" >&2; exit 2; }
    ;;
  *)
    echo "error: unsupported FIRMWARE_CONTAINER_ENGINE: $engine" >&2
    exit 2
    ;;
esac

if [ "$check_only" -eq 1 ]; then
  echo "Firmware build engine ready: $engine ($image)"
  exit 0
fi

source_dir=$(CDPATH='' cd -- "$1" && pwd)
firmware_jobs=${FIRMWARE_JOBS:-}
# Expanded by the shell inside the container, not by this wrapper.
# shellcheck disable=SC2016
build_command='jobs=$1; target=$2; if [ -z "$jobs" ]; then jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2); fi; RELEASE=1 make -j"$jobs" "$target"'

case "$engine" in
  container)
    if [ "$(uname -m)" = arm64 ]; then
      exec container run --rm --platform linux/amd64 --rosetta \
        --volume "$source_dir:/data" --workdir /data "$image" \
        sh -c "$build_command" firmware-build "$firmware_jobs" "$make_target"
    fi
    exec container run --rm --platform linux/amd64 \
      --volume "$source_dir:/data" --workdir /data "$image" \
      sh -c "$build_command" firmware-build "$firmware_jobs" "$make_target"
    ;;
  docker)
    exec docker run --rm --platform linux/amd64 \
      --user "$(id -u):$(id -g)" --env HOME=/tmp \
      --volume "$source_dir:/data" --workdir /data "$image" \
      sh -c "$build_command" firmware-build "$firmware_jobs" "$make_target"
    ;;
  podman)
    exec podman run --rm --platform linux/amd64 \
      --user "$(id -u):$(id -g)" --env HOME=/tmp \
      --volume "$source_dir:/data:rw" --workdir /data "$image" \
      sh -c "$build_command" firmware-build "$firmware_jobs" "$make_target"
    ;;
esac
