#!/bin/bash
# GNU binutils for arm-none-eabi: the assembler and linker of the project's GCC 4.8, installed
# into its prefix (build-csl.sh is configured with this assembler; the driver finds as and ld
# there). Run on Linux (see common.sh), before build-csl.sh.
#
# Sony's GCC assembled with GNU AS 2.24.51 (the producer of the assembler units in the SDK's
# GCC-built libraries, e.g. libSceSndp.a): CodeSourcery 2014.05's binutils. V picks the
# release; the default is the GNU release the project's output is checked with.
#
# usage: toolchains/build-binutils.sh   (PREFIX: where to install, default the GCC 4.8 prefix)
set -e
. "$(dirname "$0")/common.sh"
V=${V:-2.27}
SHA=${SHA:-369737ce51587f92466041a97ab7d2358c6d9e1b6490b3940eb09fb0a9a6ac88}
A=binutils-$V.tar.bz2
D=$TC/downloads
P=${PREFIX:-$TC/csl-4.8-2014.05-strong-strd}
W=$HOME/arm-binutils                              # build on the machine's own disk (fast)

mkdir -p $D
if [ ! -f $D/$A ]; then
	curl -fsSL -o $D/$A.part https://ftp.gnu.org/gnu/binutils/$A
	mv $D/$A.part $D/$A
fi
echo "$SHA  $D/$A" | sha256sum -c --quiet

rm -rf $W && mkdir -p $W && tar -xjf $D/$A -C $W
mkdir $W/build && cd $W/build
../binutils-$V/configure --target=arm-none-eabi --prefix=$P --disable-nls --disable-werror \
	--disable-gdb --disable-sim --disable-multilib > configure.log 2>&1
make -j8 MAKEINFO=true CFLAGS="-O2 -g0 -fcommon -Wno-implicit-function-declaration" > make.log 2>&1
make install MAKEINFO=true > install.log 2>&1
$P/bin/arm-none-eabi-as --version | head -1
