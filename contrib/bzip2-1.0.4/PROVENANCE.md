bzip2 1.0.4
===========

Upstream: bzip2-1.0.4.tar.gz (Julian Seward, 20 December 2006),
SHA-256 f41c3898fbd53e68e0b052418960e538813cc98afcd72b2b604079d8de3e529c.

Kept, unchanged: `bzlib.h`, `bzlib_private.h`, `bzlib.c`, `decompress.c`, `huffman.c`,
`crctable.c`, `randtable.c`, `LICENSE`. Left out: everything else in the archive (the
compressor's `blocksort.c` and `compress.c`, the command-line programs, tests,
documentation).

Used by: os0:kd/bootimage/bbmc (the modem updater's RAM files are bzip2 streams), with
Sony's changes as patches in its `updater/` folder: `Bool` is an `int`, the default
allocators do nothing (the kernel has no malloc), and the compressor and the
buffer-to-buffer helpers are left out.

The release is pinned by the decompressor: Sony's BZ2_bzDecompress has none of the
`tPos` bounds checks bzip2 1.0.5 added (CVE-2008-1372), and its BZ2_decompress has no
`N >= 2*1024*1024` check (1.0.6, CVE-2010-0405). bzip2 1.0.3 has the same code in these
files; 1.0.4 is the newest release that matches. No version string is linked.
