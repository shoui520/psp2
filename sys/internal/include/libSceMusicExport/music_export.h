/*
 * SceMusicExport's call other modules make that the SDK has no header for: the export of up to 10
 * files in one call (SceMusicExport_D1011570, the shell's background downloads). No SDK header
 * and no debug record names the types; the names are ours.
 */
#ifndef LIBSCEMUSICEXPORT_MUSIC_EXPORT_H
#define LIBSCEMUSICEXPORT_MUSIC_EXPORT_H

#include <scetypes.h>
#include <music_export.h>

/* an item to export (SceMusicExport_D1011570's input; sceMusicExportFromFile sends one) */
typedef struct MusicExportItem {
	char path[SCE_MUSIC_EXPORT_MAX_FS_PATH];
	SceInt32 mode;			/* 1: the data goes in pieces (SEND_DATA), else it is read from the path */
	SceUInt8 reserved[64];
} MusicExportItem;

/* what the service answers for an item */
typedef struct MusicExportResult {
	char path[SCE_MUSIC_EXPORT_MAX_FS_PATH];	/* where the content is */
	SceInt32 reserved0;
	SceInt32 reserved1;
} MusicExportResult;

#ifdef __cplusplus
extern "C" {
#endif

/* not in the SDK: the items to export in one call (at most 10), each answered in pResults */
SceInt32 SceMusicExport_D1011570(const MusicExportItem *pItems, SceUInt32 count, void *workMemory,
				 SceMusicExportCancelFunc cancelFunc,
				 SceMusicExportProgressInfoFunc progressFunc, void *userdata,
				 SceInt32 mode, MusicExportResult *pResults);

#ifdef __cplusplus
}
#endif

#endif /* LIBSCEMUSICEXPORT_MUSIC_EXPORT_H */
