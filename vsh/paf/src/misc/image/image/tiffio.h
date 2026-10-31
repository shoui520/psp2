/*
 * The libtiff 3.x interface as libpaf's TIFF codec uses it. libpaf links
 * libtiff 3 (32-bit file offsets), whose TIFFClientOpen also takes the
 * memory functions and their context, as png_create_read_struct_2 does.
 *
 * PROVISIONAL: declared here until libpaf's libtiff (0x81186000..) is vendored
 * into contrib; this file then gives way to its tiffio.h and tiff.h.
 */
#ifndef _PAF_IMAGE_TIFFIO_H
#define _PAF_IMAGE_TIFFIO_H

#include <stddef.h>
#include <stdint.h>

#define TIFFTAG_IMAGEWIDTH		256
#define TIFFTAG_IMAGELENGTH		257
#define TIFFTAG_BITSPERSAMPLE		258
#define TIFFTAG_PHOTOMETRIC		262
#define PHOTOMETRIC_MINISWHITE		0
#define PHOTOMETRIC_MINISBLACK		1
#define PHOTOMETRIC_RGB			2
#define PHOTOMETRIC_PALETTE		3
#define PHOTOMETRIC_MASK		4
#define PHOTOMETRIC_SEPARATED		5
#define PHOTOMETRIC_YCBCR		6
#define PHOTOMETRIC_CIELAB		8
#define PHOTOMETRIC_ICCLAB		9
#define PHOTOMETRIC_ITULAB		10
#define PHOTOMETRIC_LOGL		32844
#define PHOTOMETRIC_LOGLUV		32845
#define TIFFTAG_SAMPLESPERPIXEL		277
#define TIFFTAG_ROWSPERSTRIP		278
#define TIFFTAG_COLORMAP		320
#define TIFFTAG_TILEWIDTH		322
#define TIFFTAG_TILELENGTH		323

typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint32 ttag_t;
typedef uint32 tstrip_t;
typedef uint32 ttile_t;
typedef int32_t tsize_t;
typedef void *tdata_t;
typedef uint32 toff_t;
typedef void *thandle_t;

typedef struct tiff TIFF;

typedef tsize_t (*TIFFReadWriteProc)(thandle_t, tdata_t, tsize_t);
typedef toff_t (*TIFFSeekProc)(thandle_t, toff_t, int);
typedef int (*TIFFCloseProc)(thandle_t);
typedef toff_t (*TIFFSizeProc)(thandle_t);
typedef int (*TIFFMapFileProc)(thandle_t, tdata_t *, toff_t *);
typedef void (*TIFFUnmapFileProc)(thandle_t, tdata_t, toff_t);
typedef tdata_t (*TIFFMallocProc)(void *, tsize_t);
typedef tdata_t (*TIFFReallocProc)(void *, tdata_t, tsize_t);
typedef void (*TIFFFreeProc)(void *, tdata_t);

#ifdef __cplusplus
extern "C" {
#endif

TIFF *TIFFClientOpen(const char *name, const char *mode, thandle_t clientdata,
		     TIFFReadWriteProc readproc, TIFFReadWriteProc writeproc,
		     TIFFSeekProc seekproc, TIFFCloseProc closeproc, TIFFSizeProc sizeproc,
		     TIFFMapFileProc mapproc, TIFFUnmapFileProc unmapproc, void *mem_ptr,
		     TIFFMallocProc mallocproc, TIFFReallocProc reallocproc,
		     TIFFFreeProc freeproc);
void TIFFClose(TIFF *tif);

int TIFFGetField(TIFF *tif, ttag_t tag, ...);
int TIFFSetField(TIFF *tif, ttag_t tag, ...);
int TIFFIsTiled(TIFF *tif);

tsize_t TIFFScanlineSize(TIFF *tif);
int TIFFReadScanline(TIFF *tif, tdata_t buf, uint32 row, tsize_t sample);
tsize_t TIFFTileRowSize(TIFF *tif);
tsize_t TIFFTileSize(TIFF *tif);
tsize_t TIFFReadTile(TIFF *tif, tdata_t buf, uint32 x, uint32 y, uint32 z, tsize_t s);
int TIFFReadRGBAStrip(TIFF *tif, tstrip_t row, uint32 *raster);
int TIFFReadRGBATile(TIFF *tif, uint32 col, uint32 row, uint32 *raster);

void _TIFFmemcpy(tdata_t d, const tdata_t s, tsize_t c);

#ifdef __cplusplus
}
#endif

#endif /* _PAF_IMAGE_TIFFIO_H */
