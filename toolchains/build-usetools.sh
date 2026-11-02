#!/bin/bash
# The SGX543's assemblers, for the GPU programs gpuinit_es4 builds: Imagination's USE (USSE)
# encoder with our text front end (usetools/useparse.c, usetools/main.c: useasm), and
# Imagination's PDS assembler (pdsasm), from PVR_PSP2, the SGX DDK source GrapheneCt published.
# Configured for the PSP2's core, SGX543MP4 revision 216. Run on Linux (see common.sh).
#
# usage: toolchains/build-usetools.sh   (installs $TC/usetools/bin/useasm and pdsasm)
set -e
. "$(dirname "$0")/common.sh"
C=ae4df4b723f3f1aa629b1f3b276b9cca6d663891
SHA=30f88995434266af96d359adf5e3977ae0202956dc38d2755a2ec9995d698004
A=PVR_PSP2-$C.tar.gz
D=$TC/downloads
P=$TC/usetools
W=$HOME/usetools-build                            # build on the machine's own disk

mkdir -p $D
if [ ! -f $D/$A ]; then
	curl -fsSL -o $D/$A.part https://codeload.github.com/GrapheneCt/PVR_PSP2/tar.gz/$C
	mv $D/$A.part $D/$A
fi
echo "$SHA  $D/$A" | sha256sum -c --quiet

rm -rf $W && mkdir -p $W && tar -xzf $D/$A -C $W
S=$W/PVR_PSP2-$C
(cd $S/host/pdsasm && patch -s -p3 < $R/toolchains/usetools/pdsasm-lp64.diff)

U=$S/tools/intern/useasm
CC="${CC:-cc} -O2 -w -std=gnu89"
DEFS="-DSUPPORT_SGX543 -DSGX543 -DSGX_CORE_REV=216 -DSGX_FEATURE_MP -DSGX_FEATURE_MP_CORE_COUNT=4
	-DSGX_FEATURE_SYSTEM_CACHE -DUSER -DLINUX -D__linux__ -DNDEBUG -DIMG_ABORT=abort"
INCS="-I$S/intermediates/sgxsupport -I$S/intermediates/errata -I$S/include/gpu_es4/eurasia/hwdefs
	-I$S/include/gpu_es4/eurasia/include4 -I$S/include/gpu_es4 -I$U"
# the DDK's Windows C library names
WIN="-D_strdup=strdup -D_fileno=fileno -D_unlink=unlink -D_stricmp=strcasecmp -DYY_NO_UNISTD_H
	-include unistd.h -include string.h -include stdlib.h"

mkdir -p $P/bin
$CC $DEFS $INCS -o $P/bin/useasm $R/toolchains/usetools/main.c $R/toolchains/usetools/useparse.c \
	$U/useasm.c $U/usedisasm.c $U/usetab.c $U/specialregs.c $U/specialregs_vec.c $U/utils.c
$CC $DEFS $INCS $WIN -I$S/host/pdsasm -o $P/bin/pdsasm $S/host/pdsasm/main.c \
	$S/host/pdsasm/pdsasm.l.c $S/host/pdsasm/pdsasm.tab.c $S/host/pdsasm/pdsdisasm.c
rm -rf $W
echo "$P/bin: useasm pdsasm"
