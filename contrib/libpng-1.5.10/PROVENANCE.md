# libpng 1.5.10

Pristine upstream source: `libpng-1.5.10.tar.gz`, libpng release 1.5.10 (29-Mar-2012;
sha256 89ecc0005b9fa1d54a3579a1c508ee764ac66a747141e120ce4724251465107f), from
https://download.sourceforge.net/libpng/libpng-1.5.10.tar.gz. The library sources libpaf
links (png*.c), their headers, the NEON filters (arm/filter_neon.S), LICENSE and README are
copied unchanged. Left out: pngtest.c and the test scripts and images, example.c, contrib/,
projects/, scripts/ (the build's pnglibconf.h is the consumer's), the build files
(configure, CMakeLists.txt, Makefiles, autotools helpers), pngusr.dfa, ANNOUNCE, CHANGES,
TODO, INSTALL and the manuals.

Version pinned from libpaf (vs0:sys/external/libpaf.suprx): its version string is "1.5.10",
which png_create_read_struct_2 and png_create_write_struct_2 check the codec's against, and
the release's sources compiled with SNC reproduce libpaf's libpng functions with libpaf's
configuration.
