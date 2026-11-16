# libmd5-rfc 2002-04-13

L. Peter Deutsch's independent MD5 implementation (Aladdin Enterprises), the
2002-04-13 release of the SourceForge project libmd5-rfc:
`https://downloads.sourceforge.net/project/libmd5-rfc/libmd5-rfc/2002-04-13/md5.tar.gz`,
SHA-256 `81ea60e5694b61153a9ba1b8eac31cef90a922336e26a66000a55559163153f2`.

Kept, unchanged: `md5.c`, `md5.h` (the license is in their headers). Left out:
`md5main.c` (the test driver).

Built by libfios2 (`sys/internal/usermode/src/fios2/md5.cpp`), which compiles `md5.c`
little-endian with its `memcpy` bound to the library's own `sceClibMemcpy`.
