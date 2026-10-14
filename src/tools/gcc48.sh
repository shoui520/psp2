#!/bin/sh
# The project's GCC 4.8 as a compiler command (Makefiles: CC := src/tools/gcc48.sh):
# Linaro GCC 4.8-2014.04 with the project's corrections (toolchains/build-strd-variant.sh).
# It is a Linux program; sonycc.py / host.py run it natively on Linux, in Lima on macOS
# and in WSL2 on Windows. GCC48_DRIVER=g++ selects the C++ driver, GCC48_TOOLCHAIN
# another build in PSP2_TOOLCHAINS.
exec "${PYTHON:-python3}" -I "$(dirname "$0")/sonycc.py" gcc48 "$@"
