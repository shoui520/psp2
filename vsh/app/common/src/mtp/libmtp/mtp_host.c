/*
 * SceLibMtp: the host storage (HOST), the one storage the storage manager starts.
 *
 * It is the memory card (ux0:), seen by the host as one logical storage whose root is the
 * folder the system software gives ux0: (mtpSpecFindStorage). Its pictures and its music
 * are kept on their own devices (photo0:, music0:), shown under the root's /picture and
 * /music folders.
 */

#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>

#include "mtp_internal.h"

#define MTP_TAG_HOST	"HOST:"

/* the cache size mtpDBInitGenericWay gives its database */
#define MTP_HOST_CACHE_SIZE	10
/* the objects its database has room for */
#define MTP_HOST_MAX_OBJECTS	0x2000000

/* the first half of the volume identifier: its label, in hex */
#define MTP_HOST_VOLUME_LABEL	"HOST0: STORAGE  "
#define MTP_HOST_VOLUME_LABEL_LENGTH	16

/* a folder of the root whose objects are on a device of their own */
typedef struct MtpHostContentDevice {
	int readOnly;			/* the host may not make files there */
	const char *folder;		/* under the root */
	const char *device;
} MtpHostContentDevice;
#define MTP_HOST_NUM_CONTENT_DEVICES	2

static int _hostGetStorageInfo(MtpLogicalStorage *storage);
static int _hostGetContentRoot(MtpLogicalStorage *storage, SceUInt16 format, char *path);
static char *_hostConvertNormalPath(MtpLogicalStorage *storage, const char *path,
	char *normal, SceSize size);
static char *_hostCreatePfsPath(MtpLogicalStorage *storage, const char *path, char *pfsPath,
	SceSize size, int create);

/* the folders the host may make at the root */
static const char *const s_hostRootDirs[] = {
	"music",
	"photo",
	"video",
	NULL,
};

static const MtpHostContentDevice s_hostContentDevices[MTP_HOST_NUM_CONTENT_DEVICES] = {
	{ 1, "/picture", "photo0:" },
	{ 0, "/music", "music0:" },
};

/* the root folder, under ux0:, and the volume identifier */
static char s_hostRootFolder[256];
static char s_hostVolumeIdentifier[256];

static MtpLogicalStorage s_hostLogicalStorage = {
	.rootFolder = s_hostRootFolder,
	.rootDirs = (const char **)s_hostRootDirs,
	.index = 1,
	.largeObjectSupported = 1,
	.maxObjects = MTP_HOST_MAX_OBJECTS,
	.dbType = MTP_DB_TYPE_SQLITE,
	.getStorageInfo = _hostGetStorageInfo,
	.getContentRoot = _hostGetContentRoot,
	.convertNormalPath = _hostConvertNormalPath,
	.createPfsPath = _hostCreatePfsPath,
	.dataset = {
		.storageType = 3,		/* fixed RAM */
		.filesystemType = 2,		/* generic hierarchical */
		.storageDescription = "Host Storage",
		.volumeIdentifier = s_hostVolumeIdentifier,
	},
};

static MtpStorageInfo s_hostStorageInfo = {
	.name = "ux0:",
	.numLogicalStorages = 1,
	.logicalStorages = &s_hostLogicalStorage,
};

static MtpStorage *s_hostStorage = NULL;

/* the storage's device can be opened */
static inline int _hostCheckStorage(const MtpStorageInfo *info)
{
	SceUID fd;

	fd = sceIoDopen(info->name);
	if (fd < 0) {
		MTP_ERR(MTP_TAG_HOST, 193, "unable to open MS=0x%x", fd);
		return fd;
	}
	sceIoDclose(fd);
	return 0;
}

static int _hostStart(MtpStorage *storage)
{
	MtpStorageInfo *info = storage->info;
	MtpLogicalStorage *logical = info->logicalStorages;
	const SceUInt8 *label;
	const SceUInt8 *p;
	char *path, *dst;
	SceSize length;
	SceUInt8 c;
	int i;
	int ret;

	if ((storage->flags & MTP_STORAGE_FLAG_STOPPED) == 0)
		return SCE_MTP_ERROR_NOT_STOPPED;
	if (_hostCheckStorage(info) < 0) {
		MTP_ERR(MTP_TAG_HOST, 230, "this storage is something wrong\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	logical->writeProtected = 0;
	dst = s_hostVolumeIdentifier;
	label = (const SceUInt8 *)MTP_HOST_VOLUME_LABEL;
	for (i = MTP_HOST_VOLUME_LABEL_LENGTH; i != 0; i--) {
		c = *label++;
		dst[0] = mtpUtilUInt2HexChar((c >> 4) & 0xF);
		dst[1] = mtpUtilUInt2HexChar(c & 0xF);
		dst += 2;
	}
	s_hostVolumeIdentifier[MTP_HOST_VOLUME_LABEL_LENGTH * 2] = '\0';
	mtpUtilStrcat(s_hostVolumeIdentifier, mtpSpecGetSerialNumber());
	logical->dbType = mtpSpecGetStorageAttribute(info->name);
	/* the root: the system software's path for ux0:, without its device and last '/' */
	dst = s_hostRootFolder;
	path = mtpSpecFindStorage("ux0:");
	if (path != NULL) {
		mtpUtilMemset(s_hostRootFolder, 0, sizeof(s_hostRootFolder));
		for (p = (const SceUInt8 *)path; *p != '\0' && *p != ':'; p++)
			;
		for (p++; *p != '\0'; p++)
			*dst++ = *p;
		length = mtpUtilStrlen(s_hostRootFolder);
		if (length != 0 && s_hostRootFolder[length - 1] == '/')
			s_hostRootFolder[length - 1] = '\0';
	}
	ret = mtpStrgActivate(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HOST, 244, "unable to activate Host Storage=0x%x\n\n", ret);
		return ret;
	}
	storage->flags &= ~MTP_STORAGE_FLAG_STOPPED;
	return ret;
}

static int _hostStop(MtpStorage *storage)
{
	int ret = 0;

	if ((storage->flags & MTP_STORAGE_FLAG_STOPPED) == 0) {
		ret = mtpStrgDeactivate(storage, 0);
		storage->flags |= MTP_STORAGE_FLAG_STOPPED;
	}
	return ret;
}

static int _hostActivate(MtpStorageInfo *info)
{
	return mtpDBInitGenericWay(info, MTP_HOST_CACHE_SIZE);
}

static int _hostDeactivate(MtpStorageInfo *info, int arg)
{
	return mtpDBUninitGenericWay(info);
}

static int _hostGetStorageInfo(MtpLogicalStorage *storage)
{
	const MtpStorageInfo *info;
	int ret;

	if (storage == NULL || (info = storage->info) == NULL) {
		MTP_ERR(MTP_TAG_HOST, 326, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (storage->writeProtected == 1)
		storage->dataset.accessCapability = 2;	/* read-only with object deletion */
	else
		storage->dataset.accessCapability = 0;	/* read-write */
	ret = mtpFileGetStorageInfo(info->name, &storage->dataset.maxCapacity,
		&storage->dataset.freeSpaceInBytes);
	if (ret >= 0)
		storage->dataset.freeSpaceInObjects = storage->maxObjects - storage->numObjects;
	return ret;
}

static int _hostGetContentRoot(MtpLogicalStorage *storage, SceUInt16 format, char *path)
{
	mtpUtilStrcpy(path, storage->rootFolder);
	switch (format) {
	case 0x3000:			/* undefined */
		break;
	case 0x3008:			/* WAV */
	case 0x3009:			/* MP3 */
	case 0x300C:			/* ASF */
	case 0xB901:			/* WMA */
	case 0xB903:			/* AAC */
		mtpUtilStrcat(path, "/music");
		break;
	case 0x3801:			/* EXIF/JPEG */
	case 0x3804:			/* BMP */
	case 0x3807:			/* GIF */
	case 0x3808:			/* JFIF */
	case 0x380B:			/* PNG */
	case 0x380D:			/* TIFF */
		mtpUtilStrcat(path, "/picture");
		break;
	case 0xB411:
	case 0xB981:			/* WMV */
	case 0xB982:			/* MP4 */
		mtpUtilStrcat(path, "/video");
		break;
	case 0xB006:
	case 0xB014:
		mtpUtilStrcat(path, "/game");
		break;
	default:
		MTP_ERR(MTP_TAG_HOST, 416, "unsupported format: 0x%x\n", format);
		return SCE_MTP_ERROR_UNSUPPORTED_FORMAT;
	}
	return 0;
}

/* a path on a content device as the same path under the root */
static char *_hostConvertNormalPath(MtpLogicalStorage *storage, const char *path,
	char *normal, SceSize size)
{
	const MtpHostContentDevice *device = s_hostContentDevices;
	SceSize length, used;
	char *ret;
	int i;

	for (i = 0; i < MTP_HOST_NUM_CONTENT_DEVICES; i++, device++) {
		length = mtpUtilStrlen(device->device);
		ret = (char *)mtpUtilStrncmp(path, device->device, length);
		if (ret == NULL) {
			if (storage->info == NULL)
				return ret;
			sceClibSnprintf(normal, size, "%s%s%s", storage->info->name,
				storage->rootFolder, device->folder);
			used = mtpUtilStrlen(normal);
			return mtpUtilStrncpy(normal + used, path + length, size - used);
		}
	}
	return mtpUtilStrncpy(normal, path, size);
}

/* a path under the root as the path the PFS layer opens */
static char *_hostCreatePfsPath(MtpLogicalStorage *storage, const char *path, char *pfsPath,
	SceSize size, int create)
{
	const MtpHostContentDevice *device = s_hostContentDevices;
	SceSize length, used;
	int i;

	for (i = 0; i < MTP_HOST_NUM_CONTENT_DEVICES; i++, device++) {
		if (create && device->readOnly)
			continue;
		length = mtpUtilStrlen(device->folder);
		if (mtpUtilStrncmp(path, device->folder, length) == 0) {
			mtpUtilStrcpy(pfsPath, device->device);
			used = mtpUtilStrlen(pfsPath);
			return mtpUtilStrncpy(pfsPath + used, path + length, size - used);
		}
	}
	mtpUtilStrcpy(pfsPath, storage->info->name);
	used = mtpUtilStrlen(pfsPath);
	return mtpUtilStrncpy(pfsPath + used, path, size - used);
}

int mtpHostInitialize(void)
{
	MtpStorage *storage;
	int ret;

	storage = (MtpStorage *)mtpUtilMalloc(sizeof(*storage));
	if (storage == NULL) {
		MTP_ERR(MTP_TAG_HOST, 499, "unable to alloc memory for MS storage\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(storage, 0, sizeof(*storage));
	storage->info = &s_hostStorageInfo;
	storage->flags = MTP_STORAGE_FLAG_STOPPED;
	s_hostStorage = storage;
	storage->start = _hostStart;
	storage->stop = _hostStop;
	storage->activate = _hostActivate;
	storage->deactivate = _hostDeactivate;
	ret = mtpStrgRegister(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HOST, 515, "unable to register MS storage to manager=0x%x\n", ret);
		mtpUtilFree(storage);
	}
	return ret;
}

int mtpHostUninitialize(MtpStorage *storage)
{
	int ret;

	if ((storage->flags & MTP_STORAGE_FLAG_STOPPED) == 0)
		return SCE_MTP_ERROR_NOT_STOPPED;
	ret = mtpStrgUnregister(storage);
	if (ret < 0)
		MTP_ERR(MTP_TAG_HOST, 547, "unable to unregister MS storage from manager=0x%x\n",
			ret);
	mtpUtilFree(storage);
	return ret;
}
