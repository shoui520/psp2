#!/bin/sh
# Build a MeP firmware tree with GCC 4.5.4 and binutils 2.27 (toolchains/build-mep-*.sh,
# installed in PSP2_TOOLCHAINS). make and the toolchain are Linux programs; host.py runs
# them natively on Linux, in Lima on macOS and in WSL2 on Windows.
#   src/tools/mepbuild.sh src/os0/kd/vipimg/firmware [make targets]
# Objects go to $MEP_OUT/<module>/ (default build-mep/ in the repository).
H=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$H/../.." && pwd)
O=$R/build-mep
T=${PSP2_TOOLCHAINS:-$("${PYTHON:-python3}" -I -c "import sys; sys.path.insert(0, '$H'); import host; print(host.toolchains())")}
D=$(cd "${1:?firmware directory}" && pwd) || exit 1
shift
M=$(basename "$(dirname "$D")")
exec "${PYTHON:-python3}" -I "$H/host.py" linux -- make -C "$D" -j8 \
	GCC="$T/mep-gcc-4.5.4/bin/mep-elf-" \
	BINUTILS="$T/mep-binutils-2.27/bin/mep-elf-" \
	O="${MEP_OUT:-$O}/$M" "$@"
