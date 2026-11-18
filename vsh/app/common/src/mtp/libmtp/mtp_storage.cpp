/*
 * SceLibMtp: the storages (STRG).
 *
 * The storage manager keeps the physical storages that registered (the host's, from
 * mtpHostInitialize), each with its logical storages, and one handle map per physical
 * storage that turns the database's unique ids into the handles the host sees (VT).
 * A storage ID is the physical storage's index (from 1) in its high half and the logical
 * storage's in its low half.
 */

#include "mtp_internal.h"

#define MTP_TAG_STRG	"STRG:"

/* the storages that can register */
#define MTP_STRG_MAX_STORAGES	3

/* the room for a path a storage converts */
#define MTP_STRG_PATH_SIZE	1023

/* the storage manager */
typedef struct MtpStorageManager {
	SceUInt32 numStorages;
	MtpStorage *storages[MTP_STRG_MAX_STORAGES];
	MtpObjmap objmaps[MTP_STRG_MAX_STORAGES];	/* by physical storage index - 1 */
} MtpStorageManager;

/* the storage drivers mtpStrgInitialize starts */
static int (*const s_storageInitialize[])(void) = {
	mtpHostInitialize,
};
static int (*const s_storageUninitialize[])(MtpStorage *storage) = {
	mtpHostUninitialize,
};
#define MTP_STRG_NUM_DRIVERS	(sizeof(s_storageInitialize) / sizeof(s_storageInitialize[0]))

static MtpStorageManager *s_storageManager;

/* a storage ID's physical storage index */
#define MTP_STRG_PHYSICAL_INDEX(storageId)	((storageId) >> 16)
#define MTP_STRG_LOGICAL_INDEX(storageId)	((storageId) & 0xFFFF)

extern "C" {

int mtpStrgVTConvertMtpHandleToUniqueId(SceUInt32 handle, SceUInt32 *uniqueId)
{
	SceUInt32 index;

	if (s_storageManager == NULL || uniqueId == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1687, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	index = MTP_STRG_PHYSICAL_INDEX(MTP_OBJECT_STORAGE_ID(handle));
	if (index > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1696, "invalid storageId=0x%08x\n",
			MTP_OBJECT_STORAGE_ID(handle));
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapConvertMtpHandleToUniqueId(&s_storageManager->objmaps[index - 1], handle,
		uniqueId);
}

int mtpStrgVTConvertUniqueIdToMtpHandle(SceUInt32 storageId, SceUInt32 uniqueId,
	SceUInt32 *handle)
{
	if (s_storageManager == NULL || handle == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1745, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1753, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapConvertUniqueIdToMtpHandle(
		&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1], uniqueId, handle);
}

/*
 * The logical storage a path is on: the storage whose mount point it starts with (the
 * photo and music aliases are on ux0:), and, for a path on the mount point itself, under
 * the logical storage's root folder.
 */
static __attribute__((noinline)) int _findLogicalStorageByPath(const char *path,
	MtpLogicalStorage **storage)
{
	const char *aliases[2] = { "photo0:", "music0:" };
	const char *mount = path;
	MtpStorageInfo *info;
	MtpLogicalStorage *logical;
	SceUInt32 length;
	SceUInt32 rootLength;
	SceUInt32 i;

	for (i = 0; i < 2; i++) {
		if (mtpUtilStrncmp(path, aliases[i], mtpUtilStrlen(aliases[i])) == 0)
			mount = "ux0:";
	}
	for (i = 0; i < s_storageManager->numStorages; i++) {
		info = s_storageManager->storages[i]->info;
		length = mtpUtilStrlen(info->name);
		if (mtpUtilStrncmp(mount, info->name, length) == 0)
			break;
	}
	if (i >= s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 101, "couldn't find storage for [%s]\n", path);
		return SCE_MTP_ERROR_STORAGE_NOT_FOUND;
	}
	logical = info->logicalStorages;
	if (mount == path && (rootLength = mtpUtilStrlen(logical->rootFolder)) != 0 &&
	    mtpUtilStrncmp(&path[length], logical->rootFolder, rootLength) != 0) {
		MTP_ERR(MTP_TAG_STRG, 92, "wrong path [%s] root dir [%s]\n", path,
			logical->rootFolder);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	*storage = logical;
	return 0;
}

/* adds a directory's object, and the objects of the parents it does not have yet */
static inline int _registerDirectory(MtpLogicalStorage *storage, const char *path,
	SceUInt32 *uniqueId)
{
	SceUInt32 length;
	SceUInt32 end;
	char *dir;
	char *p;
	char c;
	int ret;

	length = mtpUtilStrlen(path);
	if (length == 0) {
		MTP_ERR(MTP_TAG_STRG, 2171, "wrong path\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	dir = (char *)mtpUtilMalloc(MTP_STRG_PATH_SIZE);
	if (dir == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2177, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilStrcpy(dir, path);
	end = length;
	/* back to the deepest parent that is there */
	for (;;) {
		ret = mtpObjAddDirectoryObject(storage, dir, uniqueId);
		if (ret != SCE_MTP_ERROR_NOT_FOUND)
			break;
		p = &dir[end];
		if (*p != '/') {
			do {
				if (end == 0)
					break;
				c = *--p;
				end--;
			} while (c != '/');
		}
		if (end == 0) {
			MTP_ERR(MTP_TAG_STRG, 2187, "not found: %s\n", path);
			mtpUtilFree(dir);
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		dir[end] = '\0';
	}
	if (ret < 0) {
		MTP_ERR(MTP_TAG_STRG, 2194, "AddDirectory %s failed: 0x%x\n", dir, ret);
		mtpUtilFree(dir);
		return ret;
	}
	/* then down again, one folder at a time */
	while (length > end) {
		dir[end] = '/';
		while (dir[end] != '\0')
			end++;
		ret = mtpObjAddDirectoryObject(storage, dir, uniqueId);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_STRG, 2206, "AddDirectory %s failed: 0x%x\n", dir, ret);
			break;
		}
	}
	mtpUtilFree(dir);
	return ret;
}

int mtpStrgRegisterDirectory(const char *path, SceUInt32 *handle)
{
	MtpLogicalStorage *storage = NULL;
	SceUInt32 storageId;
	SceUInt32 uniqueId;
	char *normal;
	int ret;

	ret = _findLogicalStorageByPath(path, &storage);
	if (ret < 0)
		return ret;
	normal = (char *)mtpUtilMalloc(MTP_STRG_PATH_SIZE);
	if (normal == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2403, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	if (storage->convertNormalPath == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2410, "ConvertNormalPathFunc is not implemented");
		mtpUtilFree(normal);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	storage->convertNormalPath(storage, path, normal, MTP_STRG_PATH_SIZE);
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = _registerDirectory(storage, mtpUtilStrchr(normal, ':') + 1, &uniqueId);
	if (ret < 0) {
		mtpUtilFree(normal);
		return ret;
	}
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, handle);
	if (ret < 0)
		MTP_ERR(MTP_TAG_STRG, 2425, "convert mtp handle failed: 0x%x, uid=%x\n", ret,
			uniqueId);
	mtpUtilFree(normal);
	return ret;
}

int mtpStrgGetMtpHandleByPath(const char *path, SceUInt32 *handle)
{
	MtpLogicalStorage *storage = NULL;
	SceUInt32 storageId;
	SceUInt32 uniqueId;
	char *normal;
	int ret;

	ret = _findLogicalStorageByPath(path, &storage);
	if (ret < 0)
		return ret;
	if (storage->info == NULL)
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	normal = (char *)mtpUtilMalloc(MTP_STRG_PATH_SIZE);
	if (normal == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2347, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	if (storage->convertNormalPath == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2354, "ConvertNormalPathFunc is not implemented");
		mtpUtilFree(normal);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	storage->convertNormalPath(storage, path, normal, MTP_STRG_PATH_SIZE);
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = mtpDBGetObjectUniqueIdsByType(storage->db, MTP_DB_QUERY_FULL_PATH_NAME,
		mtpUtilStrchr(normal, ':') + 1, 0, 1, &uniqueId);
	if (ret < 0) {
		if (ret != SCE_MTP_ERROR_NOT_FOUND)
			MTP_ERR(MTP_TAG_STRG, 2366, "couldn't get db unique id for %s: 0x%x\n",
				normal, ret);
		mtpUtilFree(normal);
		return ret;
	}
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, handle);
	if (ret < 0)
		MTP_ERR(MTP_TAG_STRG, 2373, "convert mtp handle failed: 0x%x, uid=%x\n", ret,
			uniqueId);
	mtpUtilFree(normal);
	return ret;
}

int mtpStrgIsStorageValid(SceUInt32 storageId)
{
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(storageId);
	int logical = MTP_STRG_LOGICAL_INDEX(storageId);
	MtpStorageInfo *info;
	SceUInt32 i;
	int valid = 1;

	if (physical == 0)
		return 0;
	if (physical > s_storageManager->numStorages || logical <= 0)
		return 0;
	for (i = 0; i < s_storageManager->numStorages; i++) {
		info = s_storageManager->storages[i]->info;
		if (info->index == physical) {
			if ((SceUInt32)logical > info->numLogicalStorages) {
				MTP_ERR(MTP_TAG_STRG, 226, "invalid storage id\n");
				valid = 0;
			}
			break;
		}
	}
	if (i >= s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 233, "invalid storage id\n");
		return 0;
	}
	return valid;
}

int mtpStrgIsObjectHandleValid(SceUInt32 handle)
{
	return mtpStrgIsStorageValid(MTP_OBJECT_STORAGE_ID(handle)) != 0 &&
	       (handle & MTP_OBJECT_HANDLE_INDEX_MASK) != MTP_OBJECT_HANDLE_INDEX_MASK;
}

int mtpStrgIsStorageAvailable(SceUInt32 storageId)
{
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(storageId);

	return physical != 0 && physical <= s_storageManager->numStorages &&
	       MTP_STRG_LOGICAL_INDEX(storageId) != 0 &&
	       s_storageManager->storages[physical - 1]->info->started != 0;
}

int mtpStrgGetAccessCapability(SceUInt32 storageId)
{
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(storageId);
	SceUInt32 logical = MTP_STRG_LOGICAL_INDEX(storageId);
	MtpStorageInfo *info;

	if (physical == 0 || physical > s_storageManager->numStorages || logical == 0)
		return 0;
	info = s_storageManager->storages[physical - 1]->info;
	return info->numLogicalStorages >= logical &&
	       info->logicalStorages[logical - 1].dataset.accessCapability != 0;
}

int mtpStrgIsLargeObjectSupported(SceUInt32 storageId)
{
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(storageId);
	SceUInt32 logical = MTP_STRG_LOGICAL_INDEX(storageId);
	MtpStorageInfo *info;

	if (physical == 0 || physical > s_storageManager->numStorages || logical == 0)
		return 0;
	info = s_storageManager->storages[physical - 1]->info;
	return info->numLogicalStorages >= logical &&
	       info->logicalStorages[logical - 1].largeObjectSupported != 0;
}

int mtpStrgIsRootDirAllowed(MtpLogicalStorage *storage, const char *name)
{
	const char **dirs = storage->rootDirs;

	if (dirs == NULL)
		return 1;
	for (; *dirs != NULL; dirs++) {
		if (mtpUtilStrcmp(name, *dirs) == 0)
			return 1;
	}
	return 0;
}

/* makes the logical storage's root folder, and the folders above it */
int mtpStrgSetupMtpRootFolder(MtpLogicalStorage *storage)
{
	const MtpStorageInfo *info;
	char *path;
	char *p;
	char c;
	int ret = 0;

	if (storage == NULL || (info = storage->info) == NULL) {
		MTP_ERR(MTP_TAG_STRG, 414, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	path = (char *)mtpUtilMalloc(256);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_STRG, 425, "unable to alloc memory for work buffer\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilStrcpy(path, info->name);
	if (storage->rootFolder != NULL && mtpUtilStrlen(storage->rootFolder) != 0)
		mtpUtilStrcat(path, storage->rootFolder);
	if (!mtpFileExists(path)) {
		p = path;
		c = *p;
		for (;;) {
			/* to the end of the next folder (a device name is passed whole) */
			while (c != '/' && c != '\0') {
				if (c == ':') {
					p += 2;
					c = *p;
				} else {
					p++;
					c = *p;
				}
			}
			*p = '\0';
			if (!mtpFileExists(path)) {
				ret = mtpFileCreate(path, MTP_FORMAT_ASSOCIATION);
				if (ret < 0) {
					MTP_ERR(MTP_TAG_STRG, 450, "unable to create %s=0x%x\n", path,
						ret);
					break;
				}
			}
			*p = c;
			if (c == '\0')
				break;
			p++;
			c = *p;
		}
	}
	mtpUtilFree(path);
	return ret;
}

int mtpStrgGetNumActiveStorages(void)
{
	SceUInt32 i;
	int num = 0;

	for (i = 0; i < s_storageManager->numStorages; i++) {
		if (s_storageManager->storages[i]->info->started == 1)
			num++;
	}
	return num;
}

/* whether the system software lets a storage of that mount point start */
static inline int mtpStrgIsStorageStartable(const MtpStorage *storage)
{
	const char *name = storage->info->name;

	if (mtpSpecFindStorage(name) != NULL)
		return 1;
	MTP_ERR(MTP_TAG_STRG, 292, "storage(%s) is not startable\n", name);
	return 0;
}

MtpStorage *mtpStrgFindStorageByName(const char *name)
{
	MtpStorage *storage;
	SceUInt32 length;
	SceUInt32 i;

	length = mtpUtilStrlen(name);
	for (i = 0; i < MTP_STRG_MAX_STORAGES; i++) {
		storage = s_storageManager->storages[i];
		if (storage != NULL && mtpUtilStrncmp(name, storage->info->name, length) == 0 &&
		    mtpStrgIsStorageStartable(storage))
			return storage;
	}
	MTP_ERR(MTP_TAG_STRG, 516, "not found: %s\n", name);
	return NULL;
}

int mtpStrgInitialize(void)
{
	SceUInt32 i;
	int j;
	int ret;

	s_storageManager = (MtpStorageManager *)mtpUtilMalloc(sizeof(*s_storageManager));
	if (s_storageManager == NULL) {
		MTP_ERR(MTP_TAG_STRG, 536, "unable to alloc memory for storage manager\n");
		ret = SCE_MTP_ERROR_NO_MEMORY;
		s_storageManager = NULL;
		return ret;
	}
	mtpUtilMemset(s_storageManager, 0, sizeof(*s_storageManager));
	for (i = 0; i < MTP_STRG_NUM_DRIVERS; i++) {
		ret = s_storageInitialize[i]();
		if (ret < 0)
			goto error;
	}
	return ret;
error:
	MTP_ERR(MTP_TAG_STRG, 548, "unable to initialize Memory Stick Storage=0x%x\n", ret);
	for (j = i; j >= 0; j--) {
		ret = s_storageUninitialize[j](s_storageManager->storages[j]);
		if (ret < 0)
			MTP_ERR(MTP_TAG_STRG, 560, "unable to uninitialize storage[%d]=0x%x\n", j,
				ret);
	}
	mtpUtilFree(s_storageManager);
	s_storageManager = NULL;
	return ret;
}

int mtpStrgUninitialize(void)
{
	SceUInt32 i;
	int ret = 0;

	if (s_storageManager == NULL)
		return ret;
	for (i = 0; i < MTP_STRG_NUM_DRIVERS; i++) {
		ret = s_storageUninitialize[i](s_storageManager->storages[i]);
		if (ret < 0)
			MTP_ERR(MTP_TAG_STRG, 586, "unable to uninitialize storage[%d]=0x%x\n", i,
				ret);
	}
	mtpUtilFree(s_storageManager);
	s_storageManager = NULL;
	return ret;
}

int mtpStrgRegister(MtpStorage *storage)
{
	MtpStorageManager *manager = s_storageManager;
	MtpStorageInfo *info;
	SceUInt32 num;
	SceUInt32 i;

	if (s_storageManager == NULL || storage == NULL || (info = storage->info)->numLogicalStorages == 0 ||
	    info->logicalStorages == NULL || storage->start == NULL || storage->stop == NULL) {
		MTP_ERR(MTP_TAG_STRG, 615, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	num = manager->numStorages;
	if (num == MTP_STRG_MAX_STORAGES) {
		MTP_ERR(MTP_TAG_STRG, 622, "unable to register no more\n");
		return SCE_MTP_ERROR_STORAGE_FULL;
	}
	for (i = 0; i < num; i++) {
		if (manager->storages[i] == storage) {
			MTP_ERR(MTP_TAG_STRG, 630, "already registered\n");
			return SCE_MTP_ERROR_ALREADY;
		}
	}
	s_storageManager->numStorages = num + 1;
	manager->storages[num] = storage;
	info->index = manager->numStorages;
	return 0;
}

int mtpStrgUnregister(MtpStorage *storage)
{
	MtpStorageManager *manager = s_storageManager;
	MtpStorage **storages;
	SceUInt32 last;
	SceUInt32 num;
	SceUInt32 i;

	if (manager == NULL || storage == NULL) {
		MTP_ERR(MTP_TAG_STRG, 658, "invalid parameter ");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	num = manager->numStorages;
	for (i = 0; i < num; i++) {
		if (manager->storages[i] == storage)
			break;
	}
	if (i >= num)
		return 0;
	last = num - 1;
	if (i < last) {
		storages = manager->storages;
		do {
			storages[i] = storages[i + 1];
			i++;
			last = manager->numStorages - 1;
		} while (i < last);
	}
	manager->numStorages = last;
	storage->info->index = 0;
	return 0;
}

SceUInt32 mtpStrgGetNumPhysicalStorages(void)
{
	if (s_storageManager == NULL)
		return 0;
	return s_storageManager->numStorages;
}

int mtpStrgActivate(MtpStorage *storage)
{
	MtpStorageInfo *info;
	SceUInt32 i;
	int ret;

	if (s_storageManager == NULL || storage == NULL || storage->activate == NULL) {
		MTP_ERR(MTP_TAG_STRG, 700, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	info = storage->info;
	for (i = 0; i < s_storageManager->numStorages; i++) {
		if (s_storageManager->storages[i] == storage)
			break;
	}
	if (i >= s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 713, "storage is not registered\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpCoreWaitSema();
	ret = storage->activate(info);
	if (ret >= 0)
		info->started = 1;
	else
		MTP_ERR(MTP_TAG_STRG, 722, "unable to initialize DataBase=0x%x\n", ret);
	mtpCoreSignalSema();
	return ret;
}

int mtpStrgDeactivate(MtpStorage *storage, int arg)
{
	MtpStorageInfo *info;
	SceUInt32 i;
	int ret;

	if (s_storageManager == NULL || storage == NULL || storage->deactivate == NULL) {
		MTP_ERR(MTP_TAG_STRG, 767, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	info = storage->info;
	for (i = 0; i < s_storageManager->numStorages; i++) {
		if (s_storageManager->storages[i] == storage)
			break;
	}
	if (i >= s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 781, "storage is not registered\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpCoreWaitSema();
	info->started = 0;
	ret = storage->deactivate(info, arg);
	if (ret < 0)
		MTP_ERR(MTP_TAG_STRG, 807, "unable to uninitialize DataBase=0x%x\n", ret);
	mtpCoreSignalSema();
	return ret;
}

int mtpStrgGetLogicalStorage(SceUInt32 storageId, MtpLogicalStorage **storage)
{
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(storageId);
	MtpStorageInfo *info;

	if (physical == 0 || physical > s_storageManager->numStorages ||
	    MTP_STRG_LOGICAL_INDEX(storageId) == 0 ||
	    (info = s_storageManager->storages[physical - 1]->info)->started == 0) {
		MTP_ERR(MTP_TAG_STRG, 913, "this storage is not available=%08x\n", storageId);
		return SCE_MTP_ERROR_DEVICE;
	}
	*storage = &info->logicalStorages[MTP_STRG_LOGICAL_INDEX(storageId) - 1];
	return 0;
}

int mtpStrgGetPhysicalStorageByIndex(SceUInt32 index, const MtpStorageInfo **storage)
{
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(index);

	*storage = NULL;
	if (physical == 0 || physical > s_storageManager->numStorages ||
	    MTP_STRG_LOGICAL_INDEX(index) == 0 ||
	    s_storageManager->storages[physical - 1]->info->started == 0) {
		MTP_ERR(MTP_TAG_STRG, 934, "this storage of index %d is not available\n", index);
		return SCE_MTP_ERROR_DEVICE;
	}
	*storage = s_storageManager->storages[index]->info;
	return 0;
}

int mtpStrgUpdateLogicalInfo(MtpLogicalStorage *storage)
{
	int ret;

	if (storage == NULL || storage->db == NULL) {
		MTP_ERR(MTP_TAG_STRG, 955, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = mtpDBGetUniqueIdInfo(storage->db, &storage->numObjects);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_STRG, 962, "unable to get num of object handles from DB=0x%x\n",
			ret);
		return ret;
	}
	if (storage->getStorageInfo != NULL) {
		ret = storage->getStorageInfo(storage);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_STRG, 969, "unable to get storage info=0x%x\n", ret);
		} else if (mtpCoreGetObjectTransferHandle2() != 0) {
			/* the object the host is sending takes its room already */
			storage->dataset.freeSpaceInBytes -= mtpCoreGetObjectTransferSize();
			storage->dataset.freeSpaceInObjects--;
		}
	} else {
		MTP_ERR(MTP_TAG_STRG, 979, "get storage info method is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return ret;
}

int mtpStrgGetStorageIds(MtpStorageIds *ids)
{
	if (s_storageManager == NULL || ids == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1006, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ids->num = 0;
	return 0;
}

int mtpStrgGetStorageInfo(SceUInt32 storageId, const MtpStorageDataset **dataset)
{
	MtpStorageManager *manager = s_storageManager;
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(storageId);
	SceUInt32 logical = MTP_STRG_LOGICAL_INDEX(storageId);
	MtpLogicalStorage *storage;
	MtpStorageInfo *info = NULL;
	SceUInt32 i;
	int ret;

	if (manager == NULL || dataset == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1078, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (physical == 0 || physical > manager->numStorages || logical == 0 ||
	    manager->storages[physical - 1]->info->started == 0) {
		MTP_ERR(MTP_TAG_STRG, 1085, "storage is not available=%08x\n", storageId);
		return SCE_MTP_ERROR_DEVICE;
	}
	for (i = 0; i < manager->numStorages; i++) {
		info = manager->storages[i]->info;
		if (info->index == physical)
			break;
	}
	if (i >= manager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1100, "unable to find physical storage\n");
		return SCE_MTP_ERROR_DEVICE;
	}
	storage = &info->logicalStorages[logical - 1];
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_STRG, 1108, "unable to update storage info=0x%x\n", ret);
		return SCE_MTP_ERROR_DEVICE;
	}
	*dataset = &storage->dataset;
	return ret;
}

int mtpStrgVTGetNumUniqueIds(SceUInt32 storageId, SceUInt32 parent, SceUInt32 *num)
{
	if (s_storageManager == NULL || num == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1991, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1999, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapGetNumUniqueIds(
		&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1], parent, num);
}

/* the objects under a parent (all storages: storageId -1; their roots: parent -1) */
int mtpStrgGetNumObjects(SceUInt32 storageId, SceUInt32 parent, SceUInt32 *num)
{
	MtpLogicalStorage *storage;
	MtpStorageInfo *info;
	SceUInt32 physical;
	SceUInt32 logical;
	SceUInt32 uniqueId;
	SceUInt32 handle;
	SceUInt32 count = 0;
	int ret = 0;

	if (s_storageManager == NULL || num == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1140, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	for (physical = 1; physical <= s_storageManager->numStorages; physical++) {
		if (storageId != 0xFFFFFFFF && physical != MTP_STRG_PHYSICAL_INDEX(storageId))
			continue;
		info = s_storageManager->storages[physical - 1]->info;
		if (info->started == 0)
			continue;
		for (logical = 1; logical <= info->numLogicalStorages; logical++) {
			storage = &info->logicalStorages[logical - 1];
			handle = parent;
			if (parent == 0xFFFFFFFF)
				handle = MTP_OBJECT_ROOT_HANDLE(storage->index |
					(SceUInt32)info->index << 16);
			ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_STRG, 1169,
					"unable to convert MTP handle to uniqueId=0x%x\n", ret);
				return ret;
			}
			ret = mtpStrgVTGetNumUniqueIds(storage->index | (SceUInt32)info->index << 16,
				uniqueId, &count);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_STRG, 1177, "unable to get num objects(%08x)=0x%x\n",
					storage->index | (SceUInt32)info->index << 16, ret);
				return ret;
			}
			*num += count;
		}
	}
	return ret;
}

int mtpStrgVTGetUniqueIds(SceUInt32 storageId, SceUInt32 parent, SceUInt32 offset,
	SceUInt32 count, SceUInt32 *ids)
{
	if (s_storageManager == NULL || ids == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2057, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 2065, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapGetUniqueIds(
		&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1], parent, offset,
		count, ids);
}

/* sends the handles under a parent, a buffer at a time */
int mtpStrgGetMtpHandles(SceUInt32 storageId, SceUInt32 parent, SceUInt32 num, int *send)
{
	MtpLogicalStorage *storage;
	MtpStorageInfo *info;
	SceUInt32 physical;
	SceUInt32 logical;
	SceUInt32 uniqueId;
	SceUInt32 handle;
	SceUInt32 target;
	SceUInt32 *ids;
	SceUInt32 total;
	SceUInt64 totalSize;
	SceSize room;
	SceUInt32 count;
	SceUInt32 offset;
	SceUInt32 sent = 0;
	SceUInt32 i;
	int n;
	int ret;

	if (s_storageManager == NULL || send == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1209, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	total = num * 4;
	totalSize = num * 4 + 4;
	room = total;
	if (total > 0x2000)
		room = 0x2000;
	ids = (SceUInt32 *)mtpUtilMalloc(room);
	if (ids == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1226, "unable to alloc memory for xfer work\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	count = room >> 2;
	for (physical = 1; physical <= s_storageManager->numStorages; physical++) {
		if (storageId != 0xFFFFFFFF && physical != MTP_STRG_PHYSICAL_INDEX(storageId))
			continue;
		info = s_storageManager->storages[physical - 1]->info;
		if (info->started == 0)
			continue;
		for (logical = 1; logical <= info->numLogicalStorages; logical++) {
			storage = &info->logicalStorages[logical - 1];
			target = parent;
			if (parent == 0xFFFFFFFF)
				target = MTP_OBJECT_ROOT_HANDLE(storage->index |
					(SceUInt32)info->index << 16);
			ret = mtpStrgVTConvertMtpHandleToUniqueId(target, &uniqueId);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_STRG, 1256,
					"unable to convert target MTP handle to taregetUniqueId=0x%x\n",
					ret);
				goto free;
			}
			offset = 0;
			do {
				n = mtpStrgVTGetUniqueIds(storageId, uniqueId, offset, count, ids);
				if (n < 0) {
					ret = n;
					MTP_ERR(MTP_TAG_STRG, 1266,
						"unable to get object handles(%08x)=0x%x\n",
						storage->index | (SceUInt32)info->index << 16, ret);
					goto free;
				}
				if (n <= 0)
					break;
				for (i = 0; i < (SceUInt32)n >> 2; i++) {
					mtpUtilMemcpy(&uniqueId, &ids[i], sizeof(uniqueId));
					ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId,
						&handle);
					if (ret < 0) {
						MTP_ERR(MTP_TAG_STRG, 1279,
							"unable to convert uniqueId to MTP handle=0x%x\n",
							ret);
						goto free;
					}
					mtpUtilMemcpy(&ids[i], &handle, sizeof(handle));
				}
				offset += (SceUInt32)n >> 2;
				sent += n;
				ret = mtpCoreSendData(ids, n, totalSize);
				if (ret < 0) {
					MTP_ERR(MTP_TAG_STRG, 1292, "unable to send data=0x%x\n", ret);
					*send = 0;
					goto free;
				}
			} while (ret > 0 && sent < total);
		}
	}
	ret = sent;
free:
	mtpUtilFree(ids);
	return ret;
}

int mtpStrgVTCreateMtpHandles(MtpStorage *storage)
{
	MtpObjmap *objmap;
	MtpStorageInfo *info;

	if (s_storageManager == NULL || storage == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1328, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	info = storage->info;
	objmap = &s_storageManager->objmaps[info->index - 1];
	objmap->magic = MTP_OBJMAP_MAGIC;
	objmap->nextIndex = 0;
	return mtpObjmapCreate(objmap,
		info->logicalStorages[0].index | (SceUInt32)info->index << 16);
}

int mtpStrgVTDestroyMtpHandles(MtpStorage *storage)
{
	MtpObjmap *objmap;

	if (s_storageManager == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1389, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	objmap = &s_storageManager->objmaps[storage->info->index - 1];
	objmap->magic = 0;
	return mtpObjmapDestroy(objmap);
}

int mtpStrgVTSetupMtpHandles(SceUInt32 storageId)
{
	SceUInt32 physical = MTP_STRG_PHYSICAL_INDEX(storageId);
	MtpDB *db;

	if (s_storageManager == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1428, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (physical > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1436, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	db = s_storageManager->storages[physical - 1]->info->logicalStorages[0].db;
	if (db == NULL || db->handle == -1) {
		MTP_ERR(MTP_TAG_STRG, 1449, "DB has not been prepared yet\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return mtpObjmapSetupHandles(&s_storageManager->objmaps[physical - 1]);
}

int mtpStrgVTUnsetupMtpHandles(SceUInt32 storageId)
{
	if (s_storageManager == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1485, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1493, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapUnsetupHandles(
		&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1]);
}

int mtpStrgVTInvalidateUniqueId(SceUInt32 storageId, SceUInt32 uniqueId)
{
	if (s_storageManager == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1542, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1550, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapDeallocate(
		&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1], uniqueId);
}

int mtpStrgVTValidateUniqueId(SceUInt32 storageId, SceUInt32 uniqueId, SceUInt16 format,
	SceUInt32 parent, SceUInt32 *handle)
{
	SceUInt32 flags = 0;

	if (s_storageManager == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1612, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1620, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (format == MTP_FORMAT_ASSOCIATION)
		flags = MTP_OBJECT_HANDLE_DIR;
	return mtpObjmapAllocate(&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1],
		flags, uniqueId, parent, handle);
}

int mtpStrgVTGetParentMtpHandleFromUniqueId(SceUInt32 storageId, SceUInt32 uniqueId,
	SceUInt32 *parent)
{
	if (s_storageManager == NULL || parent == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1813, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1821, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapGetParentMtpHandleFromUniqueId(
		&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1], uniqueId, parent);
}

static inline int mtpStrgVTGetParentMtpHandleFromMtpHandle(SceUInt32 storageId,
	SceUInt32 handle, SceUInt32 *parent)
{
	if (s_storageManager == NULL) {
		MTP_ERR(MTP_TAG_STRG, 1891, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (MTP_STRG_PHYSICAL_INDEX(storageId) > s_storageManager->numStorages) {
		MTP_ERR(MTP_TAG_STRG, 1898, "invalid storageId=0x%08x\n", storageId);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	return mtpObjmapGetParentMtpHandleFromMtpHandle(
		&s_storageManager->objmaps[MTP_STRG_PHYSICAL_INDEX(storageId) - 1], handle, parent);
}

/* whether a folder of an import lies under the import's own folder */
static inline int checkRootParentIsValid(SceUInt32 storageId, SceUInt32 handle,
	const MtpContext *context)
{
	SceUInt32 parent;
	SceUInt32 start = handle;
	int ret;

	for (;;) {
		ret = mtpStrgVTGetParentMtpHandleFromMtpHandle(storageId, handle, &parent);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_STRG, 154,
				"mtpStrgVTGetParentMtpHandleFromMtpHandle for 0x%x failed: 0x%x\n",
				handle, ret);
			return SCE_MTP_ERROR_READ_ONLY_PROPERTY;
		}
		if ((parent & MTP_OBJECT_HANDLE_INDEX_MASK) == MTP_OBJECT_HANDLE_INDEX_MASK) {
			MTP_ERR(MTP_TAG_STRG, 158, "valid root for 0x%x is not found\n", start);
			return SCE_MTP_ERROR_READ_ONLY_PROPERTY;
		}
		if (mtpContextCheckImportHandle(parent, context) == 0)
			return 0;
		handle = parent;
	}
}

int mtpStrgCheckParentHandle(SceUInt32 storageId, SceUInt32 format, SceUInt32 *parent,
	MtpContext *context)
{
	MtpLogicalStorage *storage;
	int ret;

	if ((SceUInt16)format == MTP_FORMAT_ASSOCIATION)
		return 0;
	storage = NULL;
	if (mtpContextCheckImportHandle(*parent, context) == 0)
		return 0;
	ret = mtpStrgGetLogicalStorage(storageId, &storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_STRG, 2248, "unable to get logical storage=0x%x\n", ret);
		return ret;
	}
	return checkRootParentIsValid(storageId, *parent, context);
}

/* the unique id of the folder a path is in, registering the folder when it is not there */
static inline int findParentUniqueIdByPath(const char *path, SceUInt32 *parent)
{
	SceUInt32 handle;
	SceUInt32 length;
	char *dir;
	int ret;

	dir = (char *)mtpUtilMalloc(MTP_STRG_PATH_SIZE);
	if (dir == NULL) {
		MTP_ERR(MTP_TAG_STRG, 117, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	length = mtpUtilStrrchr(path, '/') - path;
	mtpUtilStrncpy(dir, path, length);
	dir[length] = '\0';
	ret = mtpStrgGetMtpHandleByPath(dir, &handle);
	if (ret == SCE_MTP_ERROR_NOT_FOUND)
		ret = mtpStrgRegisterDirectory(dir, &handle);
	mtpUtilFree(dir);
	if (ret >= 0) {
		ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, parent);
		if (ret < 0)
			MTP_ERR(MTP_TAG_STRG, 138,
				"ConvertMtpHandleToUniqueId failed: 0x%x, handle=0x%x\n", ret,
				handle);
	}
	return ret;
}

int mtpStrgRegisterObject(const MtpObjectData *data, SceUInt32 *handle)
{
	MtpLogicalStorage *storage = NULL;
	SceUInt32 parent;
	char *normal;
	int ret;

	ret = _findLogicalStorageByPath(data->fullPathName, &storage);
	if (ret < 0)
		return ret;
	normal = (char *)mtpUtilMalloc(MTP_STRG_PATH_SIZE);
	if (normal == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2457, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(normal, 0, MTP_STRG_PATH_SIZE);
	if (storage->convertNormalPath == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2465, "ConvertNormalPathFunc is not implemented");
		mtpUtilFree(normal);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	storage->convertNormalPath(storage, data->fullPathName, normal, MTP_STRG_PATH_SIZE);
	if (mtpUtilStrlen(normal) == 0) {
		mtpUtilFree(normal);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = findParentUniqueIdByPath(normal, &parent);
	if (ret < 0) {
		mtpUtilFree(normal);
		return ret;
	}
	ret = mtpObjAddObjectData(storage, parent, data, normal, handle);
	mtpUtilFree(normal);
	return ret;
}

int mtpStrgCreatePfsPath(MtpLogicalStorage *storage, const char *path, char *pfsPath,
	SceSize size, int create)
{
	if (storage->createPfsPath == NULL) {
		MTP_ERR(MTP_TAG_STRG, 2508, "CreatePfsPathFunc is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	storage->createPfsPath(storage, path, pfsPath, size, create);
	return 0;
}

} /* extern "C" */
