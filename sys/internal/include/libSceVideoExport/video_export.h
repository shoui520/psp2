/*
 * SceVideoExport's calls other modules make that the SDK has no header for: an export of files
 * (sceVideoExportFromFile, the shell's background downloads). No SDK header and no debug record
 * names the types; the names are ours.
 */
#ifndef LIBSCEVIDEOEXPORT_VIDEO_EXPORT_H
#define LIBSCEVIDEOEXPORT_VIDEO_EXPORT_H

#include <scetypes.h>

#define VIDEO_EXPORT_MAX_FS_PATH		1024

/* an item to export (sceVideoExportFromFile's input) */
typedef struct VideoExportInputParam {
	char path[VIDEO_EXPORT_MAX_FS_PATH];
	SceUInt32 mode;
	SceUInt8 reserved[60];
} VideoExportInputParam;

/* what the service answers for an item (sceVideoExportFromFile's output); 8-byte aligned, as
   the content id is 64-bit (Sony's callers place it at 8 in their frames) */
typedef struct VideoExportOutputParam {
	char path[VIDEO_EXPORT_MAX_FS_PATH];	/* where the content is */
	/* PLACEHOLDER names: the content id the export gave, the shell's key 233 */
	SceInt32 contentIdLow;
	SceInt32 contentIdHigh;
} __attribute__((aligned(8))) VideoExportOutputParam;

/* the cancel and progress callbacks of an export; the progress one gets the percent and, in the
   first form, the count the service reports */
typedef SceInt32 (*VideoExportCancelFunc)(void *userdata);
typedef void (*VideoExportProgressFunc)(void *userdata, SceInt32 progress, ...);

#ifdef __cplusplus
extern "C" {
#endif

SceInt32 sceVideoExportFromFile(const VideoExportInputParam *pItems, SceInt32 count,
				void *workMemory, VideoExportCancelFunc cancelFunc,
				VideoExportProgressFunc progressFunc, void *userdata, SceInt32 mode,
				VideoExportOutputParam *pResults);

#ifdef __cplusplus
}
#endif

#endif /* LIBSCEVIDEOEXPORT_VIDEO_EXPORT_H */
