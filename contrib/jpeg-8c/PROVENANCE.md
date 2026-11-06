# libjpeg 8c

Pristine upstream source: `jpegsrc.v8c.tar.gz`, the Independent JPEG Group's release 8c
(16-Jan-2011; sha256 31636b743936d310a4ec447b508b41997f33f8864b1c62b76e2c99f8223f6fd6),
from https://www.ijg.org/files/. The library sources libpaf links (decoder and compressor,
arithmetic coding, the integer, fast and float DCTs, the memory manager with jmemnobs),
their headers, README (the IJG license, under LEGAL ISSUES) and change.log are copied
unchanged. Left out: the applications (cjpeg, djpeg, jpegtran, rdjpgcom, wrjpgcom and their
rd*/wr* and cd*/cdj* helpers), the data source and destination managers (jdatasrc.c,
jdatadst.c), transcoding (jctrans.c, jdtrans.c), transupp, the other memory back ends
(jmemansi, jmemname, jmemdos, jmemmac), example.c, the build files, the test images and the
documentation other than README. The build's jconfig.h is the consumer's.

Version pinned from libpaf (vs0:sys/external/libpaf.suprx) by compiling candidate releases
with SNC: 8b and 8c reproduce the decoder; only 8c reproduces
the compressor's write_scan_header, initial_setup and jpeg_calc_jpeg_dimensions, and 8d
changes the decoder (jdarith decode_mcu, jdcoefct, jdcolor, jdmaster) away from libpaf's.
