/*
 * libpaf's libtiff configuration (configure's tiffconf.h): a little-endian 32-bit target with
 * every codec of tif_codec.c's table but JBIG (Sony's _TIFFBuiltinCODECS, TOOL 0x81302e20, has
 * an init function for each of them: CCITT, LZW, PackBits, ThunderScan, NeXT, JPEG and old
 * JPEG, SGILog, Deflate, PixarLog).
 */
#ifndef _TIFFCONF_
#define _TIFFCONF_

#define HAVE_IEEEFP		1
#define HOST_FILLORDER		FILLORDER_LSB2MSB
#define HOST_BIGENDIAN		0
#define SIZEOF_INT		4
#define SIZEOF_LONG		4

#define CCITT_SUPPORT		1
#define JPEG_SUPPORT		1
#define LOGLUV_SUPPORT		1
#define LZW_SUPPORT		1
#define NEXT_SUPPORT		1
#define OJPEG_SUPPORT		1
#define PACKBITS_SUPPORT	1
#define PIXARLOG_SUPPORT	1
#define THUNDER_SUPPORT		1
#define ZIP_SUPPORT		1

#define STRIPCHOP_DEFAULT	TIFF_STRIPCHOP
#define SUBIFD_SUPPORT		1
#define DEFAULT_EXTRASAMPLE_AS_ALPHA	1
#define CHECK_JPEG_YCBCR_SUBSAMPLING	1
#define MDI_SUPPORT		1

#define COLORIMETRY_SUPPORT
#define YCBCR_SUPPORT
#define CMYK_SUPPORT
#define ICC_SUPPORT
#define PHOTOSHOP_SUPPORT
#define IPTC_SUPPORT

#endif /* _TIFFCONF_ */
