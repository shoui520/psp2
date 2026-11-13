Vendored upstream source
========================

Open-source code Sony built into the firmware, kept pristine and built from here.
Each project was identified in the modules that contain it, and its version pinned
from the version strings in those binaries or by compiling candidate releases and
keeping the one that matches. Sony's changes live with the consuming module as patches or marked SCE code,
never in these trees.

Each tree keeps only the files a build reads, unchanged, with the upstream license.
Its README, PROVENANCE.md or ORIGIN names the upstream release (archive and checksum,
or repository and revision) and what was left out, so every file can be checked against
upstream. When a build needs another file, add it from the same release.

| Tree | Upstream | Kept | Built by |
|---|---|---|---|
| bsd-stdio-net2-5.47 | 4.3BSD Net/2 `vfprintf.c` 5.47 | the file | sysclib (`bsd_prnt.c` + patch) |
| bsd-stdlib-7.0.0 | FreeBSD 7.0 `strtoll.c` | the file | sysclib |
| bsd-string-1998 | FreeBSD 7.0 `strlcpy.c` | the file | sysclib |
| bzip2-1.0.4 | bzip2 1.0.4 | the decompressor: `bzlib.c`, `decompress.c`, `huffman.c`, `crctable.c`, `randtable.c` and their headers | bbmc (updater, with patches) |
| dlmalloc-2.8.3 | Doug Lea's malloc 2.8.3 | `malloc.c` | libkernel (heap) |
| dlmalloc-2.8.4 | Doug Lea's malloc 2.8.4 | `malloc.c` | sysmem, libpaf |
| gcc-4.8-2014.05 | Sourcery CodeBench Lite 2014.05-28 GCC, the base of Sony's GCC | the ARM runtime files libkernel reads | libkernel (through `toolchains/libgcc`) |
| giflib-4.1.6 | GIFLIB 4.1.6 | the decoder (`dgif_lib.c`), `gif_err.c`, `gifalloc.c`, their headers, COPYING, README (`PROVENANCE.md`) | libpaf (`vsh/paf/src/misc/image/giflib`, with Sony's patch) |
| img-sgx-ddk-1.7.862890 | Imagination SGX DDK for Linux 1.7 | the full import (not trimmed yet) | gpu_es4, gpuinit_es4, gpucoredump_es4 (through the shared SGX headers) |
| jpeg-8c | IJG libjpeg 8c | the library sources libpaf links, headers, README, change.log | libpaf (`vsh/paf/src/misc/image/libjpeg`, with Sony's patch) |
| libpng-1.5.10 | libpng 1.5.10 | the library sources libpaf links, headers, `arm/filter_neon.S`, LICENSE, README | libpaf (`vsh/paf/src/misc/image/libpng`, with Sony's configuration) |
| netbsd-2.1, netbsd-3.0 | NetBSD `syssrc` 2.1 and 3.0 | the network stack files and the system headers net_ps reads | net_ps, net_ps_dev, wlanbt (through `net_ps/include`) |
| rt2860-7bc30c23 | Linux staging rt2860 `cmm_wpa.c` | the file | bootimage wlanbt |
| stm32f10x-fwlib-2.0.3 | ST's STM32F10x Firmware Library V2.0.3 | the drivers motionC uses | barkley (`src/STM32/EWARMv5`, with `src/STM32/Libraries/fwlib.patch`) |
| sce-dtrace-cddl-2015.01 | Sony's CDDL DTrace sources (the Vita SDK's libdtrace source package, 2015/01) | the kernel driver files and their headers | deci4p_dtracep |
| silk-1.0.8 | Skype's fixed-point SILK SDK 1.0.8, as FreeSWITCH v1.2.0 carries it | the library sources, interface headers, COPYING, readme.txt (`PROVENANCE.md`) | vnzimg (Venezia's SILK codec, with Sony's files in `firmware/application/silk`) |
| sqlite-3.6.23.1 | SQLite 3.6.23.1 amalgamation | `sqlite3.c`, `sqlite3.h` | libSceSqlite |
| sqlite-3.7.6.3 | SQLite 3.7.6.3 source archive | what configure and the amalgamation build read | sqlite |
| squish-1.11 | Simon Brown's libsquish 1.11 | the library sources and headers (`PROVENANCE.md`) | libpaf (`squish.h` for the DXT filters, and the library itself with Sony's NEON Vec4: `vsh/paf/src/misc/image/squish`) |
| ssleay-0.9.1b | Eric Young's SSLeay 0.9.1b, retained in OpenSSL's historical repository | LHASH, STACK, ex-data, BIO core/socket/memory/formatting, buffer management, SSL state/alert diagnostics, connection access/I/O/error classification, SSL3 lifetime/buffer setup, session allocation/access, runtime callbacks, memory-control state and their headers (`PROVENANCE.md`) | libssl (with separate SSL-C adaptation patches) |
| tiff-3.9.5 | LibTIFF 3.9.5 | the library sources libpaf links, headers, COPYRIGHT, README, VERSION (`PROVENANCE.md`) | libpaf (`vsh/paf/src/misc/image/libtiff`, with Sony's patch) |
| xnu-1456.1.26 | Apple's xnu (Mac OS X 10.6), FreeBSD's blist and 4.4BSD's qsort | `bsd/dev/dtrace/blist.c`, `blist.h`, `bsd/kern/qsort.c` (`PROVENANCE.md`) | deci4p_dtracep (with patches in `dtrace/bsd/`) |
| zlib-1.2.5 | zlib 1.2.5 | the library sources | post_ss_mgr (`post_ss_mgr/zlib`), libhttp, libpaf (`vsh/paf/src/misc/zlib`) |

Upstream code that no module builds from yet is not vendored; it is added here when a
module first compiles from it. Search for uses with `#include` as well as CMake: gpu_es4
includes the SGX DDK's headers by relative path.
