# giflib 4.1.6

Pristine upstream source: `giflib-4.1.6.tar.bz2`, Eric S. Raymond's GIFLIB release 4.1.6
from the project's SourceForge downloads
(sha256 e1c1ced9c5bc8f93ef0faf0a8c7717abf784d10a7b270d2285e8e1f3b93f2bed).
The decoder (`lib/dgif_lib.c`), the error variable (`lib/gif_err.c`), the allocation
functions (`lib/gifalloc.c`), their headers (`lib/gif_lib.h`, `lib/gif_lib_private.h`),
COPYING and README are copied unchanged; the encoder, the hash table, the font, quantizer
and utilities, `util/`, `doc/`, `pic/`, `gba/`, `windows/`, the tests and the build system
are left out.

Version pinned from libpaf's decoder: DGifGetImageDesc frees the image's colour map only
while no image is saved, and DGifDecompressLine accepts the EOF code on the line's last
pixel while no pixel is left, both 4.1.x code that 4.2 replaced; DGifDecompressLine has no
4.2 check of the prefix before the suffix is set, and DGifOpen does not record the GIF89
flag 4.2 added. With Sony's changes
(`vsh/paf/src/misc/image/giflib/giflib-4.1.6-sce.patch`) its functions are exact or proven.
