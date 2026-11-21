/*
 * The checks SceAppUtil's save data calls run on their arguments, each reporting what it rejects
 * with a parameter error number, and the slot parameter copies the calls share.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <apputil.h>
#include <apputil/apputil_psp.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include <shell_svc/shell_util.h>
#include "apputil_private.h"

int checkTitleId(const char *titleId)
{
	SceUInt32 i;

	if (titleId == NULL) {
		paramError(26);
		return 0;
	}
	if (sceClibStrnlen(titleId, SCE_APPUTIL_TITLE_ID_DATA_SIZE) != SCE_APPUTIL_TITLE_ID_DATA_SIZE - 1) {
		paramError(31);
		return 0;
	}
	for (i = 0; i < 4; i++) {
		if (titleId[i] < 'A' || titleId[i] > 'Z') {
			paramError(30);
			return 0;
		}
	}
	for (i = 4; i < SCE_APPUTIL_TITLE_ID_DATA_SIZE - 1; i++) {
		if (titleId[i] < '0' || titleId[i] > '9') {
			paramError(30);
			return 0;
		}
	}
	for (i = 0; i < 6; i++) {
		if (titleId[SCE_APPUTIL_TITLE_ID_DATA_SIZE + i] != '\0') {
			paramError(28);
			return 0;
		}
	}
	return 1;
}

int checkPassCode(const SceUChar8 *passCode)
{
	SceUInt32 i;

	if (passCode == NULL) {
		paramError(20);
		return 0;
	}
	for (i = 0; i < SCE_APPUTIL_PASSCODE_DATA_SIZE; i++) {
		if (!((passCode[i] >= 'a' && passCode[i] <= 'z') || (passCode[i] >= 'A' && passCode[i] <= 'Z') ||
		      (passCode[i] >= '0' && passCode[i] <= '9') || passCode[i] == '-' || passCode[i] == '_')) {
			paramError(21);
			return 0;
		}
	}
	return 1;
}

int checkSlotId(SceAppUtilSaveDataSlotId slotId)
{
	if (slotId < SCE_APPUTIL_SAVEDATA_SLOT_MAX) {
		return 1;
	}
	paramError(32);
	return 0;
}

int checkMountPoint(const char *mountPoint, int kind)
{
	if (mountPoint == NULL) {
		if (kind == MOUNT_POINT_ADD) {
			paramError(33);
			return 0;
		}
		return 1;
	}
	if (sceClibStrnlen(mountPoint, SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE) != 10) {
		paramError(36);
		return 0;
	}
	if (sceClibStrncmp("savedata", mountPoint, 8) == 0 &&
	    ((mountPoint[8] == '0' && kind != MOUNT_POINT_ADD) ||
	     (mountPoint[8] == '1' && kind != MOUNT_POINT_OWN)) &&
	    mountPoint[9] == ':')
		return 1;
	paramError(35);
	return 0;
}

int checkSlotParam(const SceAppUtilSaveDataSlotParam *param)
{
	const char *s;
	SceUInt32 i;
	int newlines, n;

	if (param == NULL) {
		paramError(37);
		return 0;
	}
	if (sceClibStrnlen((const char *)param->title, sizeof(param->title)) > sizeof(param->title) - 1) {
		paramError(40);
		return 0;
	}
	if (sceClibStrnlen((const char *)param->subTitle, sizeof(param->subTitle)) > sizeof(param->subTitle) - 1) {
		paramError(42);
		return 0;
	}
	if (sceClibStrnlen((const char *)param->detail, sizeof(param->detail)) > sizeof(param->detail) - 1) {
		paramError(44);
		return 0;
	}
	if (sceClibStrnlen((const char *)param->iconPath, sizeof(param->iconPath)) > sizeof(param->iconPath) - 1) {
		paramError(46);
		return 0;
	}
	if (g_sdkVersion >= 0x1800000) {
		/* titles of one line, from SDK 1.80 */
		newlines = 0;
		s = (const char *)param->title;
		for (i = 0; i < sizeof(param->title); i++, s++) {
			if (*s == '\0')
				break;
			if (*s == '\n' || *s == '\r')
				newlines++;
		}
		if (i == sizeof(param->title) || newlines > 0) {
			paramError(39);
			return 0;
		}
		newlines = 0;
		s = (const char *)param->subTitle;
		for (i = 0; i < sizeof(param->subTitle); i++, s++) {
			if (*s == '\0')
				break;
			if (*s == '\n' || *s == '\r')
				newlines++;
		}
		if (i == sizeof(param->subTitle) || newlines > 0) {
			paramError(41);
			return 0;
		}
	}
	if (SceAppUtil_7DA34311(param->title, sizeof(param->title)) != 0) {
		paramError(39);
		return 0;
	}
	if (SceAppUtil_7DA34311(param->subTitle, sizeof(param->subTitle)) != 0) {
		paramError(41);
		return 0;
	}
	if (SceAppUtil_7DA34311(param->detail, sizeof(param->detail)) != 0) {
		paramError(43);
		return 0;
	}
	for (n = 0; n < (int)sizeof(param->iconPath); ) {
		SceChar8 c = param->iconPath[n];

		if (c == '\0')
			break;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '-' || c == '_' || c == '.' || c == ':' || c == '/')) {
			paramError(45);
			return 0;
		}
		n++;
		if (n < (int)sizeof(param->iconPath) && c == '.' && param->iconPath[n] == '.') {
			paramError(45);
			return 0;
		}
	}
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0) {
			paramError(49);
			return 0;
		}
	}
	return 1;
}

int checkDataSlot(const SceAppUtilSaveDataDataSlot *slot)
{
	if (slot != NULL && (slot->id == SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID ||
			     slot->id >= SCE_APPUTIL_SAVEDATA_SLOT_MAX)) {
		paramError(32);
		return 0;
	}
	return 1;
}

/* more than one save may add to its files: 1 MiB, 10 MiB under PlayStation Now */
static inline int isSaveSizeOver(const SceSize *size)
{
	return (sceAppMgrIsPsNowClient() == 0 && *size > SCE_APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE) ||
	       (sceAppMgrIsPsNowClient() != 0 && *size > APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE_PSNOW);
}

int checkSaveItem(SceUInt32 index, const SceChar8 *dataPath, const void *buf, SceSize bufSize,
		  SceOff offset, const SceChar8 *padding, SceSize paddingSize, SceUInt32 mode,
		  const SceChar8 *reserved, SceSize reservedSize, const char *mountPoint, SceSize *size)
{
	char path[SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE + SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE - 6];
	SceIoStat stat;
	SceUInt32 i;
	int n;
	int lastSlash;
	SceUInt32 depth;
	int lead;

	if (dataPath == NULL) {
		dataParamError(54, index);
		return 0;
	}
	if (sceClibStrnlen((const char *)dataPath, SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE) >
	    SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE - 1) {
		dataParamError(56, index);
		return 0;
	}
	lastSlash = 0;
	depth = 1;
	lead = 0;
	for (n = 0; n < SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE; n++) {
		SceChar8 c = dataPath[n];

		if (c == '\0')
			break;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '-' || c == '_' || c == '.' || c == '/' || c == '\\')) {
			dataParamError(55, index);
			return 0;
		}
		if (n + 1 < SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE && c == '.' && dataPath[n + 1] == '.') {
			dataParamError(55, index);
			return 0;
		}
		if (c == '/' || c == '\\') {
			if (n == 0)
				lead = 1;
			if (depth != 1) {
				if (n - lastSlash - 1 > APPUTIL_DATA_NAME_MAXLEN) {
					dataParamError(56, index);
					return 0;
				}
			} else if (n != 0 && n - lastSlash - lead > APPUTIL_DATA_NAME_MAXLEN) {
				dataParamError(56, index);
				return 0;
			}
			lastSlash = n;
			if (depth > APPUTIL_DATA_PATH_DEPTH_MAX) {
				dataParamError(56, index);
				return 0;
			}
			if (n != 0)
				depth++;
		}
	}
	if (lastSlash != 0) {
		if (n - lastSlash - 1 > APPUTIL_DATA_NAME_MAXLEN) {
			dataParamError(56, index);
			return 0;
		}
	} else if (n - lastSlash - lead > APPUTIL_DATA_NAME_MAXLEN) {
		dataParamError(56, index);
		return 0;
	}
	if (sceClibStrncmp((const char *)&dataPath[lead], "sce_", 4) == 0) {
		dataParamError(55, index);
		return 0;
	}
	if (mode == SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY) {
		if (buf != NULL) {
			dataParamError(58, index);
			return 0;
		}
		if (bufSize != 0) {
			dataParamError(59, index);
			return 0;
		}
		if (offset != 0) {
			dataParamError(60, index);
			return 0;
		}
	} else {
		if (buf == NULL && bufSize != 0) {
			dataParamError(57, index);
			return 0;
		}
		sceClibMemset(path, 0, sizeof(path));
		if (mountPoint == NULL)
			sceClibSnprintf(path, sizeof(path), "%s%s", SCE_APPUTIL_SAVEDATA_DRIVE_NAME, dataPath);
		else if (sceClibStrnlen(mountPoint, SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE) == 9)
			sceClibSnprintf(path, sizeof(path), "%s:%s", mountPoint, dataPath);
		else
			sceClibSnprintf(path, sizeof(path), "%s%s", mountPoint, dataPath);
		/* what the save adds to the file */
		if (sceIoGetstat(path, &stat) != 0) {
			*size = bufSize + offset;
			if (isSaveSizeOver(size)) {
				dataParamError(61, index);
				return 0;
			}
		} else if (offset <= stat.st_size) {
			*size = bufSize;
			if (isSaveSizeOver(size)) {
				dataParamError(61, index);
				return 0;
			}
		} else {
			*size = offset - stat.st_size + bufSize;
			if (isSaveSizeOver(size)) {
				dataParamError(61, index);
				return 0;
			}
		}
	}
	for (i = 0; i < paddingSize; i++) {
		if (padding[i] != 0) {
			dataParamError(62, index);
			return 0;
		}
	}
	if (mode != SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE &&
	    mode != SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE &&
	    mode != SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY) {
		dataParamError(63, index);
		return 0;
	}
	for (i = 0; i < reservedSize; i++) {
		if (reserved[i] != 0) {
			dataParamError(64, index);
			return 0;
		}
	}
	return 1;
}

int checkSaveItems(const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
		   const SceAppUtilMountPoint *mountPoint)
{
	SceUInt32 i;
	SceSize total;
	SceSize size;

	if (data == NULL) {
		paramError(51);
		return 0;
	}
	if (dataNum == 0 || dataNum > SCE_APPUTIL_SAVEDATA_DATA_MAXNUM) {
		paramError(52);
		return 0;
	}
	size = 0;
	total = 0;
	for (i = 0; i < dataNum; i++) {
		size = 0;
		if (checkSaveItem(i, data[i].dataPath, data[i].buf, data[i].bufSize, data[i].offset,
				  data[i].padding, sizeof(data[i].padding), data[i].mode, data[i].reserved,
				  sizeof(data[i].reserved), (const char *)mountPoint, &size) == 0)
			return 0;
		total += size;
	}
	if ((sceAppMgrIsPsNowClient() == 0 && total > SCE_APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE) ||
	    (sceAppMgrIsPsNowClient() != 0 && total > APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE_PSNOW)) {
		paramError(61);
		return 0;
	}
	return 1;
}

int checkRemoveItem(SceUInt32 index, const SceChar8 *dataPath, SceUInt32 mode,
		    const SceChar8 *reserved, SceSize reservedSize)
{
	SceUInt32 i;
	int n;
	SceUInt32 lastSlash;
	SceUInt32 depth;
	SceUInt32 name;

	if (dataPath == NULL) {
		dataParamError(67, index);
		return 0;
	}
	if (sceClibStrnlen((const char *)dataPath, SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE) >
	    SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE - 1) {
		dataParamError(69, index);
		return 0;
	}
	lastSlash = 0;
	depth = 1;
	for (n = 0; n < SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE; n++) {
		SceChar8 c = dataPath[n];

		if (c == '\0')
			break;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '-' || c == '_' || c == '.' || c == '/' || c == '\\')) {
			dataParamError(68, index);
			return 0;
		}
		if (n + 1 < SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE && c == '.' && dataPath[n + 1] == '.') {
			dataParamError(68, index);
			return 0;
		}
		if (c == '/' || c == '\\') {
			lastSlash = n;
			if (depth++ > APPUTIL_DATA_PATH_DEPTH_MAX) {
				dataParamError(68, index);
				return 0;
			}
		}
	}
	if (sceClibStrncmp((const char *)dataPath, "sce_", 4) == 0)
		goto bad_path;
	name = 0;
	if (lastSlash != 0)
		name = lastSlash + 1;
	if (sceClibStrncmp((const char *)&dataPath[name], "sce_", 4) == 0) {
bad_path:
		dataParamError(68, index);
		return 0;
	}
	if (mode != SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT &&
	    mode != SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_KEEP_SLOT) {
		dataParamError(70, index);
		return 0;
	}
	for (i = 0; i < reservedSize; i++) {
		if (reserved[i] != 0) {
			dataParamError(71, index);
			return 0;
		}
	}
	return 1;
}

int checkRemoveItems(const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum)
{
	SceUInt32 i;

	if (data == NULL) {
		paramError(65);
		return 0;
	}
	if (dataNum == 0 || dataNum > SCE_APPUTIL_SAVEDATA_DATA_MAXNUM) {
		paramError(66);
		return 0;
	}
	for (i = 0; i < dataNum; i++) {
		if (checkRemoveItem(i, data[i].dataPath, data[i].mode, data[i].reserved, sizeof(data[i].reserved)) == 0)
			return 0;
	}
	return 1;
}

int checkRequiredSize(const SceSize *requiredSizeKiB)
{
	if (requiredSizeKiB != NULL)
		return 1;
	paramError(53);
	return 0;
}

int checkQuota(const SceSize *quotaSizeKiB, const SceSize *usedSizeKiB)
{
	if (quotaSizeKiB != NULL || usedSizeKiB != NULL)
		return 1;
	paramError(72);
	return 0;
}

int checkSearch(const SceAppUtilWorkBuffer *workBuf, const SceAppUtilSaveDataSlotSearchCond *cond,
		const SceAppUtilSlotSearchResult *result)
{
	SceUInt32 entrySize;
	SceUInt32 i;

	if (cond == NULL) {
		paramError(190);
		return 0;
	}
	if (cond->type != SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT &&
	    cond->type != SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT) {
		paramError(191);
		return 0;
	}
	if (cond->from == (SceUInt32)-1 || cond->from >= SCE_APPUTIL_SAVEDATA_SLOT_MAX) {
		paramError(192);
		return 0;
	}
	if (cond->range == 0 || cond->range > SCE_APPUTIL_SAVEDATA_SLOT_MAX ||
	    cond->range + cond->from - 1 >= SCE_APPUTIL_SAVEDATA_SLOT_MAX) {
		paramError(193);
		return 0;
	}
	/* an empty slot has nothing to sort by but its ID */
	if (cond->key != SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SLOT_ID &&
	    cond->key != SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_USER_PARAM &&
	    cond->key != SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SIZE_KIB &&
	    cond->key != SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME) {
		paramError(194);
		return 0;
	}
	if (cond->type == SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT &&
	    cond->key != SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SLOT_ID) {
		paramError(194);
		return 0;
	}
	if (cond->order != SCE_APPUTIL_SAVEDATA_SLOT_SORT_TYPE_ASCENT &&
	    cond->order != SCE_APPUTIL_SAVEDATA_SLOT_SORT_TYPE_DESCENT) {
		paramError(195);
		return 0;
	}
	for (i = 0; i < sizeof(cond->reserved); i++) {
		if (cond->reserved[i] != 0) {
			paramError(196);
			return 0;
		}
	}
	if (workBuf != NULL) {
		if (workBuf->buf == NULL) {
			paramError(197);
			return 0;
		}
		/* a slot in the list, and the key it is sorted by */
		entrySize = sizeof(SceAppUtilSaveDataSlot);
		if (cond->key == SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME)
			entrySize = sizeof(SceAppUtilSaveDataSlot) + sizeof(SceDateTime);
		else if (cond->key == SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SIZE_KIB)
			entrySize = sizeof(SceAppUtilSaveDataSlot) + sizeof(SceSize);
		if (workBuf->bufSize < entrySize * cond->range) {
			paramError(198);
			return 0;
		}
		if (workBuf->bufSize > entrySize * SCE_APPUTIL_SAVEDATA_SLOT_MAX) {
			paramError(198);
			return 0;
		}
		for (i = 0; i < sizeof(workBuf->reserved); i++) {
			if (workBuf->reserved[i] != 0) {
				paramError(199);
				return 0;
			}
		}
	}
	if (result == NULL) {
		paramError(200);
		return 0;
	}
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0) {
			paramError(201);
			return 0;
		}
	}
	return 1;
}

/* a character a PSP file system name may not hold */
static inline int isPspNameInvalid(int c)
{
	return c < ' ' || c > '~' || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
	       c == '"' || c == '<' || c == '>' || c == '|';
}

/* a secure file ID is not all zero */
static inline int hasSecureFileId(const SceUInt8 *id)
{
	SceUInt32 i;

	for (i = 0; i < SCE_APPUTIL_PSP_SAVEDATA_SECUREFILEID_SIZE; i++) {
		if (id[i] != 0)
			return 1;
	}
	return 0;
}

int checkPspSaveDataParams(const SceAppUtilPspSaveDataParams *params)
{
	const char *name;
	SceUInt32 i;
	int n;
	int base;
	int ext;
	int dot;

	if (params == NULL) {
		paramError(230);
		return 0;
	}
	/* the directory: the game's product code, then any name */
	for (n = 0; n < 4; n++) {
		if (params->dirName[n] < 'A' || params->dirName[n] > 'Z')
			goto bad_dir;
	}
	for (n = 4; n < 9; n++) {
		if (params->dirName[n] < '0' || params->dirName[n] > '9')
			goto bad_dir;
	}
	for (i = 9; i < sizeof(params->dirName); i++) {
		if (params->dirName[i] == '\0')
			break;
		if (isPspNameInvalid(params->dirName[i])) {
bad_dir:
			paramError(233);
			return 0;
		}
	}
	/* the file: an 8.3 name of the system's own characters */
	if (sceClibStrncmp("_", (const char *)params->fileName, 1) == 0 ||
	    sceClibStrncmp("sce_", (const char *)params->fileName, 4) == 0 ||
	    sceClibStrncmp(".", (const char *)params->fileName, 1) == 0)
		goto bad_file;
	base = 0;
	ext = -1;
	dot = 0;
	name = (const char *)params->fileName;
	for (n = 0; n < (int)sizeof(params->fileName) - 1; n++, name++) {
		SceChar8 c = *name;

		if (c == '\0')
			break;
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
			goto bad_file;
		if (c == '.')
			dot = 1;
		else if (!dot)
			base++;
		else
			ext++;
	}
	if ((n >= (int)sizeof(params->fileName) - 1 && params->fileName[n] != '\0') || base > 8 || ext > 3) {
bad_file:
		paramError(235);
		return 0;
	}
	if (params->fileType != SCE_APPUTIL_PSP_SAVEDATA_TYPE_SECUREFILE &&
	    params->fileType != SCE_APPUTIL_PSP_SAVEDATA_TYPE_NORMALFILE) {
		paramError(236);
		return 0;
	}
	for (i = 0; i < sizeof(params->reserved); i++) {
		if (params->reserved[i] != 0) {
			paramError(231);
			return 0;
		}
	}
	if (params->dataVersion != SCE_APPUTIL_PSP_SAVEDATA_VERSION_0 &&
	    params->dataVersion != SCE_APPUTIL_PSP_SAVEDATA_VERSION_1 &&
	    params->dataVersion != SCE_APPUTIL_PSP_SAVEDATA_VERSION_2) {
		paramError(237);
		return 0;
	}
	/* a secure file of a version above 0 needs its file ID, nothing else takes one */
	if (params->fileType == SCE_APPUTIL_PSP_SAVEDATA_TYPE_SECUREFILE) {
		if (params->dataVersion == SCE_APPUTIL_PSP_SAVEDATA_VERSION_0) {
			if (params->secureFileId != NULL) {
				paramError(239);
				return 0;
			}
		} else {
			if (params->secureFileId == NULL) {
				paramError(238);
				return 0;
			}
			if (!hasSecureFileId(params->secureFileId)) {
				paramError(240);
				return 0;
			}
		}
	} else if (params->fileType == SCE_APPUTIL_PSP_SAVEDATA_TYPE_NORMALFILE && params->secureFileId != NULL) {
		paramError(239);
		return 0;
	}
	if (params->paramSfo == NULL && params->dataBuf == NULL) {
		paramError(241);
		return 0;
	}
	if (params->dataBuf != NULL) {
		if (params->dataBufSize == 0) {
			paramError(242);
			return 0;
		}
		if (params->dataBufSize > SCE_APPUTIL_PSP_SAVEDATA_DATA_BUF_SIZE_MAX) {
			paramError(242);
			return 0;
		}
	}
	if (g_sdkVersion < 0x2100000)
		return 1;
	/* reserved2 is checked from SDK 2.10 */
	for (i = 0; i < sizeof(params->reserved2); i++) {
		if (params->reserved2[i] != 0) {
			paramError(232);
			return 0;
		}
	}
	return 1;
}

int checkPspSaveDataDirName(const char *dirName)
{
	int code;
	int i;

	if (dirName == NULL) {
		code = 243;
		goto error;
	}
	if (dirName[0] == '\0' || dirName[0] == '_' || sceClibStrncmp(dirName, "SCE_", 4) == 0) {
		code = 244;
		goto error;
	}
	for (i = 0; i < SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DIRECTORY_SIZE; i++) {
		if (dirName[i] == '\0')
			break;
		if (isPspNameInvalid(dirName[i])) {
			code = 244;
			goto error;
		}
	}
	return 1;

error:
	paramError(code);
	return 0;
}

void getModifiedTime(SceDateTime *time)
{
	SceDateTime now;

	if (sceRtcGetCurrentClockLocalTime(&now) == 0) {
		time->year = now.year;
		time->month = now.month;
		time->day = now.day;
		time->hour = now.hour;
		time->minute = now.minute;
		time->second = now.second;
		time->microsecond = now.microsecond;
	}
}

void copySlotParamIn(const SceAppUtilSaveDataSlotParam *param, SceAppMgrSaveDataSlotParam *slotParam)
{
	slotParam->status = param->status;
	slotParam->userParam = param->userParam;
	slotParam->sizeKiB = param->sizeKiB;
	slotParam->modifiedTime = param->modifiedTime;
	sceClibStrncpy(slotParam->title, (const char *)param->title, sizeof(slotParam->title) - 1);
	sceClibStrncpy(slotParam->subTitle, (const char *)param->subTitle, sizeof(slotParam->subTitle) - 1);
	sceClibStrncpy(slotParam->detail, (const char *)param->detail, sizeof(slotParam->detail) - 1);
	sceClibStrncpy(slotParam->iconPath, (const char *)param->iconPath, sizeof(slotParam->iconPath) - 1);
}

void copySlotParamOut(const SceAppMgrSaveDataSlotParam *slotParam, SceAppUtilSaveDataSlotParam *param)
{
	param->status = slotParam->status;
	param->userParam = slotParam->userParam;
	param->sizeKiB = slotParam->sizeKiB;
	param->modifiedTime = slotParam->modifiedTime;
	sceClibStrncpy((char *)param->title, slotParam->title, sizeof(param->title) - 1);
	sceClibStrncpy((char *)param->subTitle, slotParam->subTitle, sizeof(param->subTitle) - 1);
	sceClibStrncpy((char *)param->detail, slotParam->detail, sizeof(param->detail) - 1);
	sceClibStrncpy((char *)param->iconPath, slotParam->iconPath, sizeof(param->iconPath) - 1);
}

/* sceAppUtilSaveDataDataSave's checks, for SceAppUtilExt */
int SceAppUtil_E8CADCA9(const SceAppUtilSaveDataDataSlot *slot,
			const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
			const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB)
{
	if (checkDataSlot(slot) == 0 || checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0 ||
	    checkSaveItems(data, dataNum, mountPoint) == 0 || checkRequiredSize(requiredSizeKiB) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	if (slot != NULL && slot->slotParam != NULL && checkSlotParam(slot->slotParam) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	return 0;
}

/* sceAppUtilSaveDataDataRemove's checks, for SceAppUtilExt */
int SceAppUtil_1CFFE892(const SceAppUtilSaveDataDataSlot *slot,
			const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum,
			const SceAppUtilMountPoint *mountPoint)
{
	if (checkDataSlot(slot) == 0 || checkRemoveItems(data, dataNum) == 0 ||
	    checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	return 0;
}
