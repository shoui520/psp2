/*
 * The giflib 4.2 decoder interface as libpaf's GIF codec calls it. libpaf links
 * Sony's build of giflib (its libungif), whose DGifOpen also takes the heap and
 * the four memory functions the decoder allocates with.
 *
 * PROVISIONAL: declared here until libpaf's giflib (0x81185a80..) is vendored
 * into contrib with Sony's changes; this file then gives way to its gif_lib.h.
 */
#ifndef _PAF_IMAGE_GIF_LIB_H
#define _PAF_IMAGE_GIF_LIB_H

#include <stddef.h>

#define GIF_ERROR	0
#define GIF_OK		1

#define GRAPHICS_EXT_FUNC_CODE	0xf9	/* graphics control (GIF89) */

typedef unsigned char GifPixelType;
typedef unsigned char GifByteType;
typedef void *VoidPtr;

typedef struct GifColorType {
	GifByteType Red, Green, Blue;
} GifColorType;

typedef struct ColorMapObject {
	int ColorCount;
	int BitsPerPixel;
	GifColorType *Colors;
} ColorMapObject;

typedef struct GifImageDesc {
	int Left, Top, Width, Height;	/* current image dimensions */
	int Interlace;			/* sequential/interlaced lines */
	ColorMapObject *ColorMap;	/* the local color map */
} GifImageDesc;

typedef struct GifFileType {
	int SWidth, SHeight;		/* screen dimensions */
	int SColorResolution;		/* how many colors can we generate? */
	int SBackGroundColor;		/* the background color */
	ColorMapObject *SColorMap;	/* NULL if not exists */
	int ImageCount;			/* number of current image */
	GifImageDesc Image;		/* block describing current image */
	struct SavedImage *SavedImages;	/* use this to accumulate file state */
	VoidPtr UserData;		/* hook to attach user data (TVT) */
	VoidPtr Private;		/* don't mess with this! */
} GifFileType;

typedef enum {
	UNDEFINED_RECORD_TYPE,
	SCREEN_DESC_RECORD_TYPE,
	IMAGE_DESC_RECORD_TYPE,		/* begin with ',' */
	EXTENSION_RECORD_TYPE,		/* begin with '!' */
	TERMINATE_RECORD_TYPE		/* begin with ';' */
} GifRecordType;

typedef int (*InputFunc)(GifFileType *, GifByteType *, int);

/* Sony's: the memory functions DGifOpen takes, each given the heap first. */
typedef void *(*GifMallocFunc)(void *heap, size_t size);
typedef void (*GifFreeFunc)(void *heap, void *ptr);
typedef void *(*GifReallocFunc)(void *heap, void *ptr, size_t size);
typedef void *(*GifCallocFunc)(void *heap, size_t nitems, size_t size);

#ifdef __cplusplus
extern "C" {
#endif

GifFileType *DGifOpen(void *userPtr, void *heap, InputFunc readFunc, GifMallocFunc mallocFunc,
		      GifFreeFunc freeFunc, GifReallocFunc reallocFunc, GifCallocFunc callocFunc);
int DGifGetRecordType(GifFileType *GifFile, GifRecordType *GifType);
int DGifGetImageDesc(GifFileType *GifFile);
int DGifGetLine(GifFileType *GifFile, GifPixelType *GifLine, int GifLineLen);
int DGifGetExtension(GifFileType *GifFile, int *GifExtCode, GifByteType **GifExtension);
int DGifGetExtensionNext(GifFileType *GifFile, GifByteType **GifExtension);
int DGifCloseFile(GifFileType *GifFile);

#ifdef __cplusplus
}
#endif

#endif /* _PAF_IMAGE_GIF_LIB_H */
