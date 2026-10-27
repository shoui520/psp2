#!/bin/bash
# Build a separate MeP GCC 4.5.4 with the tail-call frame adjustment fix.
# First build the unmodified compiler with V=4.5.4 build-mep-gcc.sh, then run:
#   python3 tools/host.py linux -- bash toolchains/build-mep-gcc-tailcall.sh
# The result installs as mep-gcc-4.5.4-tailcall; the default compiler is untouched.
# The backend otherwise reserves a stack slot for an unmodified link register,
# even when a function ends in a tail jump. The patch keeps the slot when the
# register is set, or when compiling an interrupt handler.
set -e
. "$(dirname "$0")/common.sh"

SOURCE=${MEP_GCC_SOURCE:-$HOME/mep-gcc/gcc-4.5.4}
if [ ! -f "$SOURCE/gcc/config/mep/mep.c" ]; then
	echo "MeP GCC source not found: set MEP_GCC_SOURCE or build GCC 4.5.4 first" >&2
	exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cp -a "$SOURCE" "$WORK/gcc"
(cd "$WORK/gcc" && patch -p1) < "$R/toolchains/mep-tail-call.diff"
SRC="$WORK/gcc" NAME=4.5.4-tailcall bash "$R/toolchains/build-mep-gcc.sh"
