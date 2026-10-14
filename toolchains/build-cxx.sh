#!/bin/bash
# Build Linaro GCC 4.8-2014.04 + the stack-protector-strong backport (strong-full.diff)
# with C and C++ for arm-none-eabi, on Linux (see common.sh). build-strd-variant.sh then
# builds the project's compiler from the same source tree.
#
# configure tests the assembler's features (e.g. SHF_MERGE string sections), so it is
# pointed at vitasdk's ARM assembler (VITASDK must be set).
#
# usage: toolchains/build-cxx.sh   (needs gmp, mpfr, mpc, flex, bison, texinfo, git;
#                                    installed with apt-get where it exists)
set -e
. "$(dirname "$0")/common.sh"
S=$R/toolchains
T=gcc-linaro-4.8-2014.04
P=$TC/linaro-4.8-2014.04-strong-cxx
W=$HOME/gcc48-cxx                                 # build on the machine's own disk (fast)
AS=${VITASDK:?set VITASDK}/bin/arm-vita-eabi-as

if command -v apt-get >/dev/null; then
	sudo apt-get update -qq
	sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq build-essential libgmp-dev \
		libmpfr-dev libmpc-dev flex bison texinfo git >/dev/null
fi

mkdir -p $W/$T && cd $W/$T
[ -d .git ] || git init -q
git fetch -q --depth 1 https://gcc.gnu.org/git/gcc.git refs/vendors/linaro/tags/$T:refs/tags/$T
git checkout -q -f $T
git apply --check $S/strong-full.diff -p1 --directory=. 2>/dev/null && git apply -p1 $S/strong-full.diff \
	|| patch -p1 -N < $S/strong-full.diff

rm -rf $W/build && mkdir -p $W/build && cd $W/build
CC="gcc -std=gnu89 -fcommon -w" CXX="g++ -std=gnu++98 -fpermissive -w" $W/$T/configure \
	--target=arm-none-eabi --prefix=$P --enable-languages=c,c++ --disable-multilib \
	--disable-nls --without-headers --with-newlib --disable-libssp --disable-shared \
	--disable-threads --with-as=$AS > configure.log 2>&1
make all-gcc -j8 > make.log 2>&1
make install-gcc > install.log 2>&1
echo "built $P"
