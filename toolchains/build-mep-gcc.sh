#!/bin/bash
# GCC C compiler (cc1 and the driver only, no libraries) for Toshiba MeP (mep-elf), to
# find and reproduce the compiler of the Venezia and VIP firmware images. MeP was in FSF
# GCC from 4.5 to 6 (removed in 7). Run on Linux (see common.sh) after
# build-mep-binutils.sh.
#   V=4.5.4 build-mep-gcc.sh         FSF release from ftp.gnu.org (the project's: 4.5.4)
#   SRC=<gcc source tree> NAME=<n> build-mep-gcc.sh   an existing tree (e.g. Linaro 4.8)
# Installs to <toolchains>/mep-gcc-<V or NAME>.
set -e
. "$(dirname "$0")/common.sh"
BU=${BU:-$TC/mep-binutils-2.27}
W=$HOME/mep-gcc
mkdir -p $W && cd $W
if [ -z "$SRC" ]; then
	V=${V:?set V or SRC}
	NAME=${NAME:-$V}
	[ -f gcc-$V.tar.bz2 ] || [ -f gcc-$V.tar.gz ] || wget -q https://ftp.gnu.org/gnu/gcc/gcc-$V/gcc-$V.tar.bz2 \
		|| wget -q https://ftp.gnu.org/gnu/gcc/gcc-$V/gcc-$V.tar.gz
	[ -d gcc-$V ] || tar xf gcc-$V.tar.*
	SRC=$W/gcc-$V
	# GCC before 4.8 cannot guess an aarch64 build machine: newer autoconf scripts
	cp $HOME/mep-binutils/binutils-2.27/config.guess $HOME/mep-binutils/binutils-2.27/config.sub $SRC/
fi
P=${PREFIX:-$TC/mep-gcc-$NAME}
B=$W/build-$NAME
rm -rf $B && mkdir -p $B && cd $B
# old GCC under a modern host compiler
# (the flags build-csl.sh uses for the ARM 4.8)
export CC="gcc -std=gnu89 -fcommon -w" CXX="g++ -std=gnu++98 -fpermissive -w" MAKEINFO=true
$SRC/configure --target=mep-elf --prefix=$P --with-as=$BU/bin/mep-elf-as --with-ld=$BU/bin/mep-elf-ld \
	--enable-languages=c --disable-nls --disable-werror --disable-libssp --disable-multilib \
	--without-headers --with-newlib --disable-shared --disable-threads --disable-bootstrap \
	> configure.log 2>&1
make -j8 all-gcc MAKEINFO=true > make.log 2>&1 || { tail -30 make.log; exit 1; }
make install-gcc MAKEINFO=true > install.log 2>&1
$P/bin/mep-elf-gcc --version | head -1
# libgcc for the little-endian images (vnzimg), without crt/gcov (no target headers):
# its objects build before those fail, and are archived by hand into lib/gcc/mep-elf/<V>/el.
make -j8 all-target-libgcc MAKEINFO=true CFLAGS_FOR_TARGET="-mel -O2 -g" > libgcc.log 2>&1 || true
L=$P/lib/gcc/mep-elf/$($P/bin/mep-elf-gcc -dumpversion)/el
mkdir -p $L && rm -f $L/libgcc.a
(cd mep-elf/libgcc && $BU/bin/mep-elf-ar rcs $L/libgcc.a $(ls *.o | grep -v gcov))
