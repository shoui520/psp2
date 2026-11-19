/*
 * music_export.h: exporting a music file to the system's music content, under Sony's names and
 * with Sony's definitions (SDK Release 03.570.011, target/include/music_export.h).
 * SceMusicExport exports the function.
 */
#ifndef _SCE_MUSIC_EXPORT_MUSIC_EXPORT_H
#define _SCE_MUSIC_EXPORT_MUSIC_EXPORT_H

#include <scetypes.h>
#include <music_export/error.h>

#ifdef __cplusplus
extern "C" {
#endif

/* maximum size of path strings (device name and NUL terminator included) */
#define SCE_MUSIC_EXPORT_MAX_FS_PATH			(1024)

/* maximum size of the memory block */
#define SCE_MUSIC_EXPORT_MAX_MEMBLOCK_SIZE		(64 * 1024)

/* the metadata of an export */
typedef struct SceMusicExportParam {
	SceChar8 reserved[128];		/* reserved range (must be filled by zero) */
} SceMusicExportParam;

typedef SceBool (*SceMusicExportCancelFunc)(void *);
typedef void (*SceMusicExportProgressInfoFunc)(void *, SceUInt32);

/* music export by file path */
SceInt32 sceMusicExportFromFile(const SceChar8 *musicfilePath, const SceMusicExportParam *param,
				void *workMemory, SceMusicExportCancelFunc cancelFunc,
				SceMusicExportProgressInfoFunc progressFunc, void *userdata,
				SceChar8 *exportedPath, SceInt32 exportedPathLength);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_MUSIC_EXPORT_MUSIC_EXPORT_H */
