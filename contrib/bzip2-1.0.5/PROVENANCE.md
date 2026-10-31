bzip2 1.0.5
===========

Upstream: bzip2-1.0.5.tar.gz (Julian Seward, 10 December 2007),
SHA-256 f7bf5368309d76e5daf3a89d4d1bea688dac7780742e7a0ae1af19be9316fe22.

Kept, unchanged: `bzlib.h`, `LICENSE`. Left out: everything else in the archive
(the compressor, the command-line programs, tests, documentation). The library
sources the decompressor needs (`bzlib.c`, `decompress.c`, `huffman.c`, `crctable.c`,
`randtable.c`, `bzlib_private.h`) are added from the same archive when a module first
builds them.

Used by: os0:kd/bootimage/bbmc (the modem updater's RAM files are bzip2 streams). The
release is pinned by the decompressor: Sony's BZ2_decompress has no `N >= 2*1024*1024`
check, which bzip2 1.0.6 added (CVE-2010-0405); no version string is linked.
