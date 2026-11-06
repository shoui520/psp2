# libtiff 3.9.5

Pristine upstream source: `tiff-3.9.5.tar.gz`, LibTIFF release 3.9.5 from
`https://download.osgeo.org/libtiff/old/`
(sha256 ecf2e30582698dbc61d269203bbd1e701a1a50fb26c87d709e10d89669badb33).
The library sources libpaf links (`libtiff/tif_*.c` but those below), their headers
(`tiff.h`, `tiffio.h`, `tiffiop.h`, `tiffvers.h`, `tif_dir.h`, `tif_fax3.h`, `tif_predict.h`,
`t4.h`, `uvcode.h`), COPYRIGHT, README and VERSION are copied unchanged; the error and warning
handlers (`tif_error.c`, `tif_warning.c`), `tif_print.c`, `tif_version.c`, `tif_extension.c`,
the platform files (`tif_unix.c`, `tif_win32.c`, ...), the generated configuration, `mkg3states.c`,
`tools/`, `contrib/`, `man/`, `html/`, `test/` and the build system are left out.

Version pinned by compiling candidate releases (3.9.0, 3.9.2, 3.9.4, 3.9.5, 3.9.7) against libpaf's
libtiff: 3.9.5 matches the most functions. Sony's tree also carries a few later fixes (3.9.7's
overflow check in the separated-plane readers, libtiff 4.0's DotRange and single-value custom
tags, its LZW and PixarLog checks) and 3.8.2's field lookup; these and Sony's own changes are in
`vsh/paf/src/misc/image/libtiff/tiff-3.9.5-sce.patch`.
