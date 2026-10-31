#!/bin/sh
# Build an isolated, monolithic diagnostic. Optional upload requires a slot.
set -eu
CHECK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$HOME/.local/opt/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin:$HOME/.local/bin:$PATH"

case "${1:-build}" in
  build) [ "$#" -le 1 ] || exit 2 ;;
  upload)
    case "${2:-}" in
      [1-8]) ;;
      *) echo "Usage: $0 upload SLOT [PORT]  (SLOT: 1..8, replaces that slot)" >&2; exit 2 ;;
    esac
    [ "$#" -le 3 ] || exit 2
    ;;
  *) echo "Usage: $0 [build | upload SLOT [PORT]]" >&2; exit 2 ;;
esac

cd "$CHECK_ROOT"
make USE_PACKAGE=0 SRCDIR=./tools/controller-check \
  BINDIR=./build/controller-check/bin DEPDIR=./build/controller-check/.d \
  EXTRA_CFLAGS= EXTRA_CXXFLAGS=
CHECK_BIN="$CHECK_ROOT/build/controller-check/bin/monolith.bin"
echo "Built: $CHECK_BIN"

if [ "${1:-build}" = upload ]; then
  # PROS CLI 3.5.6 prefers an auto-detected project even with an explicit .bin.
  # Run outside the project so it uploads this diagnostic, not rogue's hot image.
  CHECK_UPLOAD_DIR="$(mktemp -d /tmp/rogue-controller-upload.XXXXXX)"
  trap 'rmdir "$CHECK_UPLOAD_DIR"' EXIT
  cd "$CHECK_UPLOAD_DIR"
  if [ -n "${3:-}" ]; then
    pros upload "$CHECK_BIN" "$3" --target v5 --slot "$2" --name ctrl-check --after screen
  else
    pros upload "$CHECK_BIN" --target v5 --slot "$2" --name ctrl-check --after screen
  fi
fi
