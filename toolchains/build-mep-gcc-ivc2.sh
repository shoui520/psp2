#!/bin/bash
# GCC 4.5.4 mep-elf whose IVC2 intrinsics schedule into VLIW bundles: the 4.5.4 tree
# with intrinsics.md patched by ivc2-deps.py from the IVC2 User's Manual's resource
# table (see there), and two FSF bugs fixed (mep-ivc2-fixes.diff).
# Run on Linux (see common.sh) after "V=4.5.4 build-mep-gcc.sh", which leaves the 4.5.4
# source tree in ~/mep-gcc. Needs pdftotext (poppler-utils) and the manual:
#   IVC2_MANUAL=<IVC2 User Manual PDF> toolchains/build-mep-gcc-ivc2.sh
# Installs to <toolchains>/mep-gcc-4.5.4-ivc2.
set -e
. "$(dirname "$0")/common.sh"
MANUAL=${IVC2_MANUAL:?set IVC2_MANUAL to the IVC2 User\'s Manual PDF}
command -v pdftotext >/dev/null || { echo "$0: pdftotext is not installed (poppler-utils)" >&2; exit 1; }
W=$HOME/mep-gcc
[ -d $W/gcc-4.5.4 ] || { echo "$0: build GCC 4.5.4 first: V=4.5.4 toolchains/build-mep-gcc.sh" >&2; exit 1; }
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
rm -rf $W/gcc-4.5.4-ivc2 && cp -a $W/gcc-4.5.4 $W/gcc-4.5.4-ivc2
pdftotext -layout "$MANUAL" $T/manual.txt
python3 -I "$R/toolchains/ivc2-deps.py" $T/manual.txt $W/gcc-4.5.4/gcc/config/mep/intrinsics.md \
	$W/gcc-4.5.4-ivc2/gcc/config/mep/intrinsics.md
# FSF fixes: the lmcpi post-increment pattern tied the address to the coprocessor register,
# and DCE asserted on its REG_EQUIV note; both first reached once the intrinsics stopped being barriers
(cd $W/gcc-4.5.4-ivc2 && patch -p1) < "$R/toolchains/mep-ivc2-fixes.diff"
SRC=$W/gcc-4.5.4-ivc2 NAME=4.5.4-ivc2 bash "$R/toolchains/build-mep-gcc.sh"
