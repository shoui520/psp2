#!/bin/bash
# binutils for Toshiba MeP (mep-elf): assembler, linker and objdump for the Venezia and VIP
# firmware images (os0:kd/vnzimg.skprx, os0:kd/vipimg.skprx). 2.27 is the version
# TeamMolecule's mepsdk uses; its CGEN-generated MeP tables include the C5 core and the
# IVC2 coprocessor. Run on Linux (see common.sh). Installs to <toolchains>/mep-binutils-<v>.
set -e
. "$(dirname "$0")/common.sh"
V=${V:-2.27}
SHA=${SHA:-369737ce51587f92466041a97ab7d2358c6d9e1b6490b3940eb09fb0a9a6ac88}
P=${PREFIX:-$TC/mep-binutils-$V}
W=$HOME/mep-binutils
mkdir -p $W && cd $W
[ -f binutils-$V.tar.bz2 ] || wget -q https://ftp.gnu.org/gnu/binutils/binutils-$V.tar.bz2
echo "$SHA  binutils-$V.tar.bz2" | sha256sum -c -
rm -rf binutils-$V build && tar xjf binutils-$V.tar.bz2 && mkdir build && cd build
../binutils-$V/configure --target=mep-elf --prefix=$P --disable-nls --disable-werror \
	--disable-gdb --disable-sim > configure.log 2>&1
make -j8 MAKEINFO=true CFLAGS="-O2 -g0 -fcommon -Wno-implicit-function-declaration" > make.log 2>&1
make install MAKEINFO=true > install.log 2>&1
$P/bin/mep-elf-objdump --version | head -1
