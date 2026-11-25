#!/bin/bash
# GNU GCC 3.3.6 with Sony Allegrex (PSP / Vita ePSP) modifications, reconstructed
# from binary evidence of the 6.61 pcff firmware (confirmed by the 6.60 SDK:
# "GCC: (GNU) 3.3.6+allegrex-2.2.2-psp-1.21.0.internal").
#
# Run on Linux (see common.sh):
#   . agent/env.sh && python3 tools/host.py linux -- bash toolchains/build-psp-gcc.sh
#
# Requires psp-as / psp-ld / psp-ar (from $PSPDEV or $TC/pspdev-v20261001).
# Installs to <toolchains>/psp-gcc-3.3.6-allegrex.
set -e
. "$(dirname "$0")/common.sh"

V=3.3.6
P=${PREFIX:-$TC/psp-gcc-${V}-allegrex}
PSP_BIN=${PSP_BIN:-${PSPDEV:-$TC/pspdev-v20261001}/bin}
if [ ! -x "$PSP_BIN/psp-as" ] || [ ! -x "$PSP_BIN/psp-ld" ]; then
	echo "psp-as / psp-ld not found in $PSP_BIN (set PSPDEV or PSP_BIN)" >&2
	exit 1
fi

W=$HOME/psp-gcc
mkdir -p "$W" && cd "$W"
TARBALL="$TC/downloads/gcc-$V.tar.bz2"
if [ ! -f "$TARBALL" ]; then
	TARBALL="$W/gcc-$V.tar.bz2"
	[ -f "$TARBALL" ] || wget -q "https://ftp.gnu.org/gnu/gcc/gcc-$V/gcc-$V.tar.bz2" -O "$TARBALL"
fi

SRC=$W/gcc-$V-allegrex
rm -rf "$SRC"
mkdir -p "$W/extract"
tar -xjf "$TARBALL" -C "$W/extract"
mv "$W/extract/gcc-$V" "$SRC"
rmdir "$W/extract"

patch -d "$SRC" -p1 < "$R/toolchains/psp-allegrex-3.3.6.diff"

B=$W/build-$V-allegrex
rm -rf "$B" && mkdir -p "$B" && cd "$B"
export PATH="$PSP_BIN:$PATH"
ln -sf "$PSP_BIN/psp-as" "$B/as"
ln -sf "$PSP_BIN/psp-ld" "$B/ld"

export CC="gcc -O1 -g0 -std=gnu89 -fcommon -fpermissive -w -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
export MAKEINFO=true

HOST_TRIPLE=$("$SRC/config.guess" 2>/dev/null || echo "$(uname -m)-unknown-linux-gnu")
"$SRC/configure" \
	--target=mipsallegrexel-psp-elf \
	--build="$HOST_TRIPLE" \
	--host="$HOST_TRIPLE" \
	--prefix="$P" \
	--with-as="$PSP_BIN/psp-as" \
	--with-ld="$PSP_BIN/psp-ld" \
	--enable-languages=c \
	--disable-nls \
	--disable-shared \
	--disable-threads \
	--disable-multilib \
	--without-headers \
	--with-newlib \
	> configure.log 2>&1

# Build cc1, xgcc, target headers (stddef.h/stdarg.h/limits.h/float.h), and tconfig.h
# (libgcc mips16.S fails on Allegrex single-float-only as, so we build libgcc.a below).
make -j8 MAKEINFO=true -C libiberty > make-libiberty.log 2>&1
make -j8 MAKEINFO=true -C gcc xgcc cc1 cpp specs stmp-int-hdrs tconfig.h > make-gcc.log 2>&1 || { tail -40 make-gcc.log; exit 1; }
make -C gcc install-common install-driver install-cpp install-headers MAKEINFO=true > install.log 2>&1 || true

# Ensure bin/ and lib/gcc-lib/ layout is complete and clean.
GCC_LIB="$P/lib/gcc-lib/mipsallegrexel-psp-elf/$V"
mkdir -p "$P/bin" "$GCC_LIB/include" "$P/mipsallegrexel-psp-elf/bin"
cp -f gcc/xgcc "$P/bin/mipsallegrexel-psp-elf-gcc"
cp -f gcc/cpp "$P/bin/mipsallegrexel-psp-elf-cpp"
cp -f gcc/cc1 gcc/specs "$GCC_LIB/"
cp -f gcc/include/*.h "$GCC_LIB/include/" 2>/dev/null || true
ln -sf mipsallegrexel-psp-elf-gcc "$P/bin/psp-gcc"
ln -sf "$PSP_BIN/psp-as" "$P/mipsallegrexel-psp-elf/bin/as"
ln -sf "$PSP_BIN/psp-ld" "$P/mipsallegrexel-psp-elf/bin/ld"
ln -sf "$PSP_BIN/psp-ar" "$P/mipsallegrexel-psp-elf/bin/ar"
ln -sf "$PSP_BIN/psp-ranlib" "$P/mipsallegrexel-psp-elf/bin/ranlib"
ln -sf "$PSP_BIN/psp-as" "$P/bin/psp-as"
ln -sf "$PSP_BIN/psp-ld" "$P/bin/psp-ld"
ln -sf "$PSP_BIN/psp-ar" "$P/bin/psp-ar"
ln -sf "$PSP_BIN/psp-ranlib" "$P/bin/psp-ranlib"
ln -sf "$PSP_BIN/psp-objdump" "$P/bin/psp-objdump"
ln -sf "$PSP_BIN/psp-objcopy" "$P/bin/psp-objcopy"
ln -sf "$PSP_BIN/psp-nm" "$P/bin/psp-nm"
ln -sf "$PSP_BIN/psp-strip" "$P/bin/psp-strip"
ln -sf "$PSP_BIN/psp-size" "$P/bin/psp-size"

# Build libgcc.a (64-bit integer helpers + clz_tab, compiled with -O2 matching SCE)
LIBGCC_DIR="$B/libgcc-allegrex"
rm -rf "$LIBGCC_DIR" && mkdir -p "$LIBGCC_DIR"
LIBGCC_OBJS=""
for sym in _muldi3 _negdi2 _lshrdi3 _ashldi3 _ashrdi3 _ffsdi2 _clz \
           _cmpdi2 _ucmpdi2 _floatdidf _floatdisf _fixunsdfsi _fixunssfsi \
           _fixunsdfdi _fixdfdi _fixunssfdi _fixsfdi _fixxfdi _fixunsxfdi \
           _floatdixf _fixunsxfsi _fixtfdi _fixunstfdi _floatditf \
           _clear_cache _trampoline __main _exit _absvsi2 _absvdi2 \
           _addvsi3 _addvdi3 _subvsi3 _subvdi3 _mulvsi3 _mulvdi3 \
           _negvsi2 _negvdi2 _ctors _divdi3 _moddi3 _udivdi3 _umoddi3 \
           _udiv_w_sdiv _udivmoddi4; do
	out="$LIBGCC_DIR/${sym}.o"
	if "$P/bin/psp-gcc" -O2 -G0 -march=allegrex -mabi=eabi -mgp32 -mfp32 -msingle-float -mbranch-likely \
		-DIN_GCC -DCROSS_COMPILE -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED -Dinhibit_libc \
		-I"$B/gcc" -I"$SRC/gcc" -I"$SRC/gcc/config" -I"$SRC/include" \
		-DL${sym} -c "$SRC/gcc/libgcc2.c" -o "$out" 2>/dev/null; then
		LIBGCC_OBJS="$LIBGCC_OBJS $out"
	fi
done
"$PSP_BIN/psp-ar" rcs "$GCC_LIB/libgcc.a" $LIBGCC_OBJS
"$PSP_BIN/psp-ranlib" "$GCC_LIB/libgcc.a"

"$P/bin/psp-gcc" --version | head -1
echo "Installed to $P"
