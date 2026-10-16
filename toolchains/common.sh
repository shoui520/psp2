# Sourced by the recipes. They build Linux programs and run on Linux: natively, or from
# macOS and Windows through the host runner, e.g.
#   python3 tools/host.py linux -- bash toolchains/build-cxx.sh
# Sets R (the repository) and TC (where toolchains install: PSP2_TOOLCHAINS, default
# toolchains/install, as tools/host.py).
R=$(cd "$(dirname "$0")/.." && pwd)
if [ "$(uname -s)" != Linux ]; then
	echo "$0 runs on Linux: python3 tools/host.py linux -- bash $0" >&2
	exit 1
fi
TC=${PSP2_TOOLCHAINS:-$R/toolchains/install}
mkdir -p "$TC"
