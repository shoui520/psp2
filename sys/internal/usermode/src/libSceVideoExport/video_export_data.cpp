/*
 * SceVideoExportData (vs0:sys/external/libSceVideoExport.suprx): exports a video in pieces. The
 * first call mounts the application's work directory and starts a file there, the second writes the
 * data into it, the third gives the file to the media service (sceVideoExportFromFile) and ends the
 * export. The state between the calls is the library's own. No Sony record names the library's
 * files or the calls; the names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <ces/libces_ucs.h>
#include "video_export_internal.h"

extern "C" int sceCesUtf8ToUcs4(const uint8_t *utf8buf, uint32_t utf8max, uint32_t *convSize, uint32_t *ucs4);
extern "C" int sceAppMgrGetDevInfo(const char *dev, SceUInt64 *pMaxSize, SceUInt64 *pFreeSize);
extern "C" int sceAppMgrAppParamGetString(SceUID pid, int param, char *string, SceSize length);
extern "C" int sceAppMgrWorkDirMountById(int id, const char *titleId, char *mountPoint);
extern "C" int sceAppMgrUmount(const char *mountPoint);
extern "C" int sceRegMgrSystemParamGetInt(int id, int *pValue);
extern "C" SceUID sceKernelGetProcessId(void);
extern "C" SceInt32 sceVideoExportFromFile(const VideoExportInputParam *pItems, SceInt32 count,
					   void *workMemory, VideoExportCancelFunc cancelFunc,
					   VideoExportProgressFunc progressFunc, void *userdata,
					   SceInt32 mode, VideoExportOutputParam *pResults);

/* the registry setting that tells the media is full */
#define VIDEO_EXPORT_PARAM_MEDIA_FULL		1395244

/* the application's work directory the data goes into (SceAppMgr's id) */
#define VIDEO_EXPORT_WORK_DIR_ID		207

/* the free space the export needs on ux0: */
#define VIDEO_EXPORT_MIN_FREE_SIZE		0x3200000

/* the file the data goes into before it is renamed */
#define VIDEO_EXPORT_DATA_FILE			"videoexport.dat"

/* the file open flags and mode the library uses (SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC, 0600) */
#define VIDEO_EXPORT_OPEN_CREATE		1539
#define VIDEO_EXPORT_OPEN_WRITE			3
#define VIDEO_EXPORT_FILE_MODE			384

/* the parameter of the first call */
typedef struct VideoExportStartParam {
	SceUInt32 reserved0;
	SceUInt32 reserved1;
	const char *path;		/* the file's name, UTF-8, ends in .mp4 or .m4v */
	SceUInt32 pathLength;
	VideoExportCancelFunc cancelFunc;
	VideoExportProgressFunc progressFunc;
	void *userdata;
	void *workMemory;
	SceSize workMemorySize;
	SceUInt8 reserved2[0x60];
} VideoExportStartParam;

/* what the first call keeps for the next two */
typedef struct VideoExportState {
	SceUInt32 reserved0;
	SceUInt32 reserved1;
	SceUInt32 reserved2;
	SceUInt32 pathLength;
	VideoExportCancelFunc cancelFunc;
	VideoExportProgressFunc progressFunc;
	void *userdata;
	void *workMemory;
	SceUInt8 reserved3[0x68];
	char path[0x100];		/* the file's name */
	char mountPoint[0x10];		/* where the work directory is mounted */
	char titleId[10];
	char dataPath[0x106];		/* the file the data is written to */
} VideoExportState;

static SceUInt8 videoExportStarted;
static VideoExportState videoExportState;

/* the parameter's file name, its mount point, the data file and the registry */
extern "C" SceInt32 SceVideoExportData_3488EA69(const VideoExportStartParam *param)
{
	SceIoStat stat;
	SceUInt64 maxSize;
	SceUInt64 freeSize;
	char file[0x400];
	char title[16];
	char mount[16];
	char unmount[16];
	SceCesUcsContext ucs;
	SceUInt32 convSize;
	SceUInt32 ucs4;
	SceUInt32 utf16Len;
	const char *path;
	const char *extension;
	SceUInt32 length;
	SceUInt32 i;
	SceUID fd;
	int value;
	int ret;

	if (param == NULL)
		return VIDEO_EXPORT_ERROR_PARAM;
	path = param->path;
	if (path == NULL || param->workMemory == NULL)
		return VIDEO_EXPORT_ERROR_PARAM;
	for (i = 0; i < sizeof(param->reserved2); i++) {
		if (param->reserved2[i] != 0)
			return VIDEO_EXPORT_ERROR_PARAM;
	}
	ret = 0;
	value = 0;
	if (param->workMemorySize < VIDEO_EXPORT_HEAP_SIZE) {
		ret = VIDEO_EXPORT_ERROR_PARAM;
		goto end;
	}
	length = param->pathLength;
	if (sceCesUtf8ToUcs4((const uint8_t *)path, length, &convSize, &ucs4) != 0 ||
	    (convSize == 3 && ucs4 == 0xFEFF) ||
	    (sceClibMemset(&ucs, 0, sizeof(ucs)), sceCesUcsContextInit(&ucs) != 0) ||
	    sceCesUtf8StrGetUtf16Len(&ucs, (const uint8_t *)path, length, &convSize,
				     &utf16Len) != 0) {
		ret = VIDEO_EXPORT_ERROR_NOT_SUPPORTED_FORMAT;
		goto end;
	}
	extension = sceClibStrrchr(path, '.');
	if (extension == NULL ||
	    (sceClibStrncasecmp(".mp4", extension, length - (extension - path)) != 0 &&
	     sceClibStrncasecmp(".m4v", extension, length - (extension - path)) != 0)) {
		ret = VIDEO_EXPORT_ERROR_INVALID_PATH;
		goto end;
	}
	for (i = 0; i < length; i++) {
		if (path[i] == '\\' || path[i] == '/') {
			ret = VIDEO_EXPORT_ERROR_INVALID_PATH;
			goto end;
		}
	}
	sceAppMgrGetDevInfo("ux0:", &maxSize, &freeSize);
	if (freeSize < VIDEO_EXPORT_MIN_FREE_SIZE) {
		ret = VIDEO_EXPORT_ERROR_NO_SPACE;
	} else if (sceRegMgrSystemParamGetInt(VIDEO_EXPORT_PARAM_MEDIA_FULL, &value) == 0 &&
		   value == 1) {
		ret = VIDEO_EXPORT_ERROR_NO_SPACE;
	} else {
		sceClibMemset(title, 0, sizeof(title));
		if (sceAppMgrAppParamGetString(sceKernelGetProcessId(), 12, title, 10) < 0) {
			ret = VIDEO_EXPORT_ERROR_INTERNAL;
		} else {
			sceClibStrncpy(videoExportState.titleId, title, 10);
			sceClibMemset(mount, 0, sizeof(mount));
			if (sceAppMgrWorkDirMountById(VIDEO_EXPORT_WORK_DIR_ID, title, mount) < 0) {
				ret = VIDEO_EXPORT_ERROR_INTERNAL;
			} else {
				sceClibStrncpy(videoExportState.mountPoint, mount, sizeof(mount));
				path = param->path;
				length = param->pathLength;
				sceClibMemset(file, 0, sizeof(file));
				sceClibStrncat(file, videoExportState.mountPoint, sizeof(mount));
				sceClibStrncat(file, path, length + 1);
				if ((sceIoGetstat(file, &stat) != 0 || sceIoRemove(file) >= 0) &&
				    (fd = sceIoOpen(file, VIDEO_EXPORT_OPEN_CREATE,
						    VIDEO_EXPORT_FILE_MODE)) >= 0) {
					sceIoClose(fd);
					sceIoRemove(file);
					sceClibStrncpy(videoExportState.path, param->path, param->pathLength);
					videoExportState.reserved2 = param->reserved1;
					videoExportState.reserved0 = param->reserved0;
					videoExportState.pathLength = param->pathLength;
					videoExportState.cancelFunc = param->cancelFunc;
					videoExportState.progressFunc = param->progressFunc;
					videoExportState.userdata = param->userdata;
					videoExportState.workMemory = param->workMemory;
					sceClibStrncat(videoExportState.dataPath, videoExportState.mountPoint,
						       sizeof(mount));
					sceClibStrncat(videoExportState.dataPath, VIDEO_EXPORT_DATA_FILE, 0x20);
					if (sceIoGetstat(videoExportState.dataPath, &stat) != 0)
						videoExportStarted = 1;
					else if (sceIoRemove(videoExportState.dataPath) >= 0)
						videoExportStarted = 1;
					else
						ret = VIDEO_EXPORT_ERROR_CREATE_FILE;
				} else {
					ret = VIDEO_EXPORT_ERROR_INVALID_PATH;
				}
			}
		}
	}
end:
	if (videoExportStarted == 0 && videoExportState.mountPoint[0] != '\0') {
		sceClibMemset(unmount, 0, sizeof(unmount));
		sceClibStrncpy(unmount, videoExportState.mountPoint, sizeof(unmount));
		sceAppMgrUmount(unmount);
	}
	return ret;
}

/* writes the data at the offset of the file */
extern "C" SceInt32 SceVideoExportData_8DDE620F(const void *pData, SceSize size, SceOff offset)
{
	SceIoStat stat;
	SceSSize written;
	SceUID fd;
	int value;

	if (videoExportStarted == 0)
		return VIDEO_EXPORT_ERROR_NOT_STARTED;
	if (pData == NULL)
		return VIDEO_EXPORT_ERROR_PARAM;
	value = 0;
	if (sceRegMgrSystemParamGetInt(VIDEO_EXPORT_PARAM_MEDIA_FULL, &value) == 0 && value == 1)
		return VIDEO_EXPORT_ERROR_MEDIA_FULL;
	if (sceIoGetstat(videoExportState.dataPath, &stat) != 0)
		fd = sceIoOpen(videoExportState.dataPath, VIDEO_EXPORT_OPEN_CREATE,
			       VIDEO_EXPORT_FILE_MODE);
	else
		fd = sceIoOpen(videoExportState.dataPath, VIDEO_EXPORT_OPEN_WRITE,
			       VIDEO_EXPORT_FILE_MODE);
	if (fd < 0)
		return VIDEO_EXPORT_ERROR_WRITE_OPEN;
	sceIoLseek(fd, offset, SCE_SEEK_SET);
	written = sceIoWrite(fd, pData, size);
	sceIoSyncByFd(fd, 0);
	if (written != (SceSSize)size) {
		sceIoClose(fd);
		return VIDEO_EXPORT_ERROR_MEDIA_FULL;
	}
	if (written < 0)
		return VIDEO_EXPORT_ERROR_INTERNAL;
	sceIoClose(fd);
	return 0;
}

/* renames the data file, unmounts the work directory and hands the file to the media service */
extern "C" SceInt32 SceVideoExportData_34953CC0(const SceUInt8 *reserved)
{
	VideoExportInputParam inParam;
	VideoExportOutputParam outParam;
	char unmount[16];
	char dataFile[0x100];
	char newName[0x200];
	SceUInt32 ret;
	SceUInt32 i;

	if (videoExportStarted == 0)
		return VIDEO_EXPORT_ERROR_NOT_STARTED;
	for (i = 0; i < 0x40; i++) {
		if (*reserved++ != 0)
			return VIDEO_EXPORT_ERROR_PARAM;
	}
	sceClibMemset(newName, 0, sizeof(newName));
	sceClibMemset(dataFile, 0, sizeof(dataFile));
	sceClibStrncat(dataFile, videoExportState.mountPoint, 0x10);
	sceClibStrncat(dataFile, VIDEO_EXPORT_DATA_FILE, 0x20);
	sceClibStrncat(newName, videoExportState.mountPoint, 0x10);
	sceClibStrncat(newName, videoExportState.path, 0x100);
	if (sceIoRename(dataFile, newName) < 0)
		return VIDEO_EXPORT_ERROR_CREATE_FILE;
	sceClibMemset(&inParam, 0, sizeof(inParam));
	sceClibMemset(&outParam, 0, sizeof(outParam));
	sceClibStrncat(inParam.path, "rec0:", 5);
	sceClibStrncat(inParam.path, videoExportState.titleId, 10);
	sceClibStrncat(inParam.path, "/", 1);
	sceClibStrncat(inParam.path, videoExportState.path, 0x100);
	sceClibMemset(unmount, 0, sizeof(unmount));
	sceClibStrncpy(unmount, videoExportState.mountPoint, sizeof(unmount));
	if (sceAppMgrUmount(unmount) < 0)
		return VIDEO_EXPORT_ERROR_INTERNAL;
	ret = sceVideoExportFromFile(&inParam, 1, videoExportState.workMemory,
				     videoExportState.cancelFunc, videoExportState.progressFunc,
				     videoExportState.userdata, 0, &outParam);
	sceClibMemset(&videoExportState, 0, sizeof(videoExportState));
	videoExportStarted = 0;
	if ((ret > 0x8010540F || ret < 0x80105400) && ret != 0)
		return VIDEO_EXPORT_ERROR_INTERNAL;
	return 0;
}
