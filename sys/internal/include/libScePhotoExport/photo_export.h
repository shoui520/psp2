/*
 * ScePhotoExport's calls other modules make that the SDK has no header for: an export from a file
 * that gives the content's id or its path (the shell's background downloads).
 */
#ifndef LIBSCEPHOTOEXPORT_PHOTO_EXPORT_H
#define LIBSCEPHOTOEXPORT_PHOTO_EXPORT_H

#include <scetypes.h>
#include <photo_export.h>

#ifdef __cplusplus
extern "C" {
#endif

SceInt32 ScePhotoExport_76640642(const SceChar8 *photodataPath, const ScePhotoExportParam *param,
				 SceInt32 reserved, void *workMemory,
				 ScePhotoExportCancelFunc cancelFunc, void *userdata,
				 SceInt32 *pContentId);
SceInt32 ScePhotoExport_719ACE85(const SceChar8 *photodataPath, const ScePhotoExportParam *param,
				 SceInt32 reserved, void *workMemory,
				 ScePhotoExportCancelFunc cancelFunc, void *userdata,
				 SceChar8 *exportedPath, SceInt32 exportedPathLength);

#ifdef __cplusplus
}
#endif

#endif /* LIBSCEPHOTOEXPORT_PHOTO_EXPORT_H */
