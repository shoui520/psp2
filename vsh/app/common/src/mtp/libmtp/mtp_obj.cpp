/*
 * SceLibMtp: the objects of a storage (OBJ).
 *
 * Every file and folder of a logical storage is a row of its database (tbl_mtpObject,
 * kept as an ObjectDataBasic) and a unique id in its handle map; the objects' properties
 * are read and written through the cache.
 */

#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <sceerror.h>
#include "mtp_internal.h"

#define MTP_TAG_OBJ	"OBJ :"

/* an object path's room, and a path's room under a mount point */
#define MTP_OBJ_PATH_SIZE	1023

/* the protection status Sony gives the folders it makes */
#define MTP_OBJ_PROTECTION_FOLDER	0x8003

/* the picture beside an album (its representative sample) */
#define MTP_OBJ_JPEG_EXTENSION		"jpg"

/* the room for one element of an ObjectPropList */
#define MTP_OBJ_ELEMENT_SIZE		0x206

extern "C" {

/* the bytes of a sample's picture (a .jpg beside the object), packed as an AUINT8 */
static __attribute__((noinline)) int _packObjectPropValueRSD(MtpLogicalStorage *storage,
	SceUInt32 uniqueId, SceUInt8 *buf, SceSize size)
{
	SceUInt32 dataSize = 0;
	SceUInt32 length;
	SceOff fileSize;
	SceUInt32 packed;
	SceSize room;
	char *path;
	char *pfsPath;
	SceUID fd;
	int ret;

	room = size;
	fileSize = 0;
	packed = 0;
	if (storage == NULL || storage->cache == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 334, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	length = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(length * 2 + MTP_OBJ_PATH_SIZE * 2);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 343, "unable to allocate memory for JPEG file name\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilStrcpy(path, storage->info->name);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_FULL_PATH_NAME,
		&path[length], MTP_OBJ_PATH_SIZE);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 355, "unable to get full path name of .alb=0x%x\n", ret);
		goto end;
	}
	mtpUtilStrrchr(path, '.')[1] = '\0';
	mtpUtilStrcat(path, MTP_OBJ_JPEG_EXTENSION);
	pfsPath = &path[length + MTP_OBJ_PATH_SIZE];
	ret = mtpStrgCreatePfsPath(storage, &path[length], pfsPath, length + MTP_OBJ_PATH_SIZE, 1);
	if (ret < 0)
		goto end;
	if (mtpFileExists(pfsPath)) {
		ret = mtpFileGetSize(pfsPath, &fileSize);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 374, "unable to get jpeg file size=0x%x\n", ret);
			goto end;
		}
		packed = (SceUInt32)fileSize;
		dataSize = (SceUInt32)fileSize;
		if (room < (SceUInt32)fileSize) {
			MTP_ERR(MTP_TAG_OBJ, 380, "error buffer too small\n");
			ret = SCE_MTP_ERROR_NOT_INITIALIZED;
			goto end;
		}
		if (buf != NULL) {
			fd = mtpFileOpen(pfsPath, SCE_O_RDONLY, 0);
			if (fd < 0) {
				MTP_ERR(MTP_TAG_OBJ, 389, "unable to open %s(0x%x)\n", pfsPath, fd);
				ret = SCE_MTP_ERROR_NOT_INITIALIZED;
				goto end;
			}
			mtpFileRead(fd, buf + sizeof(dataSize), dataSize);
			mtpFileClose(fd);
			packed = dataSize;
		}
	}
	if (buf != NULL)
		mtpUtilMemcpy(buf, &dataSize, sizeof(dataSize));
	ret = packed + sizeof(dataSize);
end:
	mtpUtilFree(path);
	return ret;
}

int mtpObjPackObjectPropValue(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	SceUInt16 code, void *buf, int size)
{
	const MtpCodeList *props;
	const MtpPropDesc *desc;
	SceUInt16 format;
	SceUInt32 offset = 0;
	SceUInt32 storageId;
	SceUInt32 valueSize = 0;
	MtpPropData value;
	SceUInt32 parent;
	SceUInt32 i;
	int type;
	int ret;

	if (storage == NULL || storage->cache == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2007, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_FORMAT,
		&format, sizeof(format));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2018, "unable to get object format from cache=0x%x\n", ret);
		return ret;
	}
	props = mtpSpecGetObjectPropsSupported(format);
	if (props == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2025, "not supported object format code\n");
		return SCE_MTP_ERROR_UNSUPPORTED_FORMAT;
	}
	for (i = 0; i < props->count; i++) {
		if (props->codes[i] == code)
			break;
	}
	if (i >= props->count) {
		MTP_ERR(MTP_TAG_OBJ, 2037, "object prop code is not supported at this format\n");
		return SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
	}
	desc = mtpSpecGetObjectPropDesc(code);
	if (desc == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2045, "object prop code is not supported by device\n");
		return SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
	}
	switch (code) {
	case MTP_OBJECT_PROP_STORAGE_ID:
		value.dataType = desc->dataType;
		valueSize = sizeof(storageId);
		value.data = mtpUtilMalloc(sizeof(storageId));
		if (value.data == NULL) {
			MTP_ERR(MTP_TAG_OBJ, 2059, "unable to alloc for work buffer\n");
			return SCE_MTP_ERROR_NO_MEMORY;
		}
		value.size = valueSize;
		mtpUtilMemcpy(value.data, &storageId, sizeof(storageId));
		ret = mtpPropPackObjectPropValue(&value, buf);
		break;
	case MTP_OBJECT_PROP_PARENT_OBJECT:
		ret = mtpStrgVTGetParentMtpHandleFromUniqueId(storageId, uniqueId, &parent);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2075, "unable to get parent MTP handle=0x%x\n", ret);
			return ret;
		}
		/* an object at the root has no parent */
		if ((parent & MTP_OBJECT_HANDLE_INDEX_MASK) == MTP_OBJECT_HANDLE_INDEX_MASK)
			parent = 0;
		value.dataType = desc->dataType;
		valueSize = sizeof(parent);
		value.data = mtpUtilMalloc(sizeof(parent));
		if (value.data == NULL) {
			MTP_ERR(MTP_TAG_OBJ, 2091, "unable to alloc for work buffer\n");
			return SCE_MTP_ERROR_NO_MEMORY;
		}
		value.size = valueSize;
		mtpUtilMemcpy(value.data, &parent, sizeof(parent));
		ret = mtpPropPackObjectPropValue(&value, buf);
		break;
	case MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA:
		value.data = NULL;
		ret = _packObjectPropValueRSD(storage, uniqueId, (SceUInt8 *)buf, size);
		break;
	default:
		value.dataType = desc->dataType;
		type = mtpDBObjectPropCode2QueryType(code);
		if (type < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2115, "this property is not handled in DB\n");
			return type;
		}
		ret = mtpDBGetObjectPropertyOffset(type, &offset, &valueSize);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2121, "unable to get size and offset=0x%x\n", ret);
			return ret;
		}
		/* a date is kept as a SceDateTime and sent as a string of twice its size */
		if (desc->formFlag == MTP_FORM_DATETIME)
			valueSize *= 2;
		value.data = mtpUtilMalloc(valueSize);
		if (value.data == NULL) {
			MTP_ERR(MTP_TAG_OBJ, 2133, "unable to alloc for work buffer\n");
			return SCE_MTP_ERROR_NO_MEMORY;
		}
		value.size = valueSize;
		ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, type, value.data,
			valueSize);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2140, "unable to get object property form cache=0x%x\n",
				ret);
			goto free;
		}
		mtpDBFixToObjectPropValue(type, value.data, value.data, valueSize);
		ret = mtpPropPackObjectPropValue(&value, buf);
		break;
	}
	if (ret < 0)
		MTP_ERR(MTP_TAG_OBJ, 2152, "unable to pack object prop value=0x%x\n", ret);
	else if (ret > size)
		MTP_ERR(MTP_TAG_OBJ, 2158,
			"buffer too small...code=%04x bytesPacked=%d bufferSize=%d\n", code, ret,
			size);
free:
	if (value.data != NULL)
		mtpUtilFree(value.data);
	return ret;
}

int mtpObjBuildFullPathName(MtpLogicalStorage *storage, SceUInt32 parent, const char *name,
	char *path, int withMountPoint)
{
	char *parentPath;
	int ret = 0;

	if (storage == NULL || name == NULL || path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2857, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (withMountPoint == 1)
		mtpUtilStrcpy(path, storage->info->name);
	if (parent != MTP_OBJ_NO_PARENT) {
		parentPath = (char *)mtpUtilMalloc(MTP_OBJ_PATH_SIZE);
		if (parentPath == NULL) {
			MTP_ERR(MTP_TAG_OBJ, 2870, "unable to alloc memory for work\n");
			return SCE_MTP_ERROR_NO_MEMORY;
		}
		ret = mtpCacheGetObjectProperty(storage->cache, parent, MTP_DB_QUERY_FULL_PATH_NAME,
			parentPath, MTP_OBJ_PATH_SIZE);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2877, "unable to get object property=0x%x\n", ret);
			mtpUtilFree(parentPath);
			return ret;
		}
		mtpUtilStrcat(path, parentPath);
		mtpUtilStrcat(path, "/");
		mtpUtilFree(parentPath);
	}
	mtpUtilStrcat(path, name);
	return ret;
}

int mtpObjAddRootObject(MtpLogicalStorage *storage)
{
	ObjectDataBasic *object;
	const char *relative;
	char *path;
	int ret;

	if (storage == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 510, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	path = (char *)mtpUtilMalloc(MTP_OBJ_PATH_SIZE);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 518, "unable to alloc memory for full path\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = mtpObjBuildFullPathName(storage, MTP_OBJ_NO_PARENT, storage->rootFolder, path, 1);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 527, "unable to get full path name=0x%x\n", ret);
		goto end;
	}
	relative = mtpUtilStrchr(path, ':') + 1;
	object = (ObjectDataBasic *)mtpUtilMalloc(sizeof(*object));
	if (object == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 538, "unable to alloc memory for ObjectDataBasic\n");
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto end;
	}
	mtpUtilMemset(object, 0, sizeof(*object));
	object->uid = 0;
	object->parentUid = MTP_OBJ_NO_PARENT;
	object->objectSize = 0;
	object->objectFormat = MTP_FORMAT_ASSOCIATION;
	object->protectionStatus = MTP_OBJ_PROTECTION_FOLDER;
	mtpFileGetDates(path, &object->dateCreated, &object->dateModified);
	mtpUtilStrcpy(object->fullPathName, relative);
	object->nonConsumable = 1;
	mtpUtilStrcpy(object->name, "ROOT");
	mtpUtilMd5(path, &object->dateCreated, object->puoid);
	ret = mtpDBRegisterObject(storage->db, object);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 558, "unable to register object to DB=0x%x\n", ret);
		goto free;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 564, "unable to update logical info=0x%x", ret);
		mtpDBUnregisterObject(storage->db, object->uid);
		goto free;
	}
	mtpUtilFree(object);
	mtpUtilFree(path);
	return ret;
free:
	mtpUtilFree(object);
end:
	mtpUtilFree(path);
	return ret;
}

int mtpObjAddDirectoryObject(MtpLogicalStorage *storage, const char *name, SceUInt32 *uniqueId)
{
	ObjectDataBasic *object;
	const char *leaf;
	SceUInt32 storageId;
	SceUInt32 parent;
	SceUInt32 num = 0;
	SceUInt32 length;
	char *path;
	int ret;

	if (storage == NULL || storage->db == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 606, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 613, "unable to update logical info=0x%x", ret);
		return ret;
	}
	ret = mtpDBGetNumObjectsByType(storage->db, MTP_DB_QUERY_FULL_PATH_NAME, name, &num);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 619, "unable to get num objects by type=0x%x\n", ret);
		return ret;
	}
	if (num != 0) {
		MTP_ERR(MTP_TAG_OBJ, 622, "already registered %s\n", name);
		return SCE_MTP_ERROR_ALREADY;
	}
	/* the parent folder's part of the name */
	length = mtpUtilStrlen(name);
	while (name[length] != '/' && length != 0)
		length--;
	path = (char *)mtpUtilMalloc(MTP_OBJ_PATH_SIZE);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 632, "unable to alloc memory for full path\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	if (length != 0) {
		mtpUtilMemcpy(path, name, length);
		path[length] = '\0';
		num = 0;
		ret = mtpDBGetNumObjectsByType(storage->db, MTP_DB_QUERY_FULL_PATH_NAME, path,
			&num);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 646, "GetNumObjectsByType failed: 0x%x\n", ret);
			goto end;
		}
		if (num == 0) {
			ret = SCE_MTP_ERROR_NOT_FOUND;
			goto end;
		}
		ret = mtpDBGetObjectUniqueIdsByType(storage->db, MTP_DB_QUERY_FULL_PATH_NAME,
			path, 0, 1, &parent);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 656, "couldn't get db unique id: 0x%x\n", ret);
			goto end;
		}
	} else {
		ret = mtpStrgVTConvertMtpHandleToUniqueId(MTP_OBJECT_ROOT_HANDLE(storageId),
			&parent);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 665, "couldn't get root unique id: 0x%x\n", ret);
			goto end;
		}
	}
	ret = mtpStrgCreatePfsPath(storage, name, path, MTP_OBJ_PATH_SIZE, 1);
	if (ret < 0)
		goto end;
	if (!mtpFileExists(path)) {
		ret = mtpFileCreate(path, MTP_FORMAT_ASSOCIATION);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 683, "unable to create dir %s=0x%x\n", path, ret);
			goto end;
		}
	}
	if (storage->numObjects >= storage->maxObjects) {
		MTP_ERR(MTP_TAG_OBJ, 692, "store full\n");
		ret = SCE_MTP_ERROR_STORAGE_FULL;
		goto end;
	}
	object = (ObjectDataBasic *)mtpUtilMalloc(sizeof(*object));
	if (object == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 700, "unable to alloc memory for ObjectDataBasic\n");
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto end;
	}
	mtpUtilMemset(object, 0, sizeof(*object));
	object->uid = 0;
	object->parentUid = parent;
	object->objectSize = 0;
	object->objectFormat = MTP_FORMAT_ASSOCIATION;
	object->protectionStatus = MTP_OBJ_PROTECTION_FOLDER;
	mtpFileGetDates(path, &object->dateCreated, &object->dateModified);
	mtpUtilStrcpy(object->fullPathName, name);
	leaf = mtpUtilStrrchr(name, '/');
	mtpUtilStrcpy(object->name, leaf != NULL ? leaf + 1 : name);
	object->nonConsumable = 1;
	mtpUtilMd5(path, &object->dateCreated, object->puoid);
	ret = mtpDBRegisterObject(storage->db, object);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 726, "unable to register object to DB=0x%x\n", ret);
		goto free;
	}
	ret = mtpCacheWriteBackDirtyCache(storage->cache);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 732, "unablet to write back cache=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpStrgVTValidateUniqueId(storageId, object->uid, MTP_FORMAT_ASSOCIATION, parent,
		NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 739,
			"unable to validate uniqueId in virtual MTP handle list=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 745, "unable to update logical info=0x%x", ret);
		goto unregister;
	}
	if (uniqueId != NULL)
		*uniqueId = object->uid;
	mtpUtilFree(object);
	mtpUtilFree(path);
	return ret;
unregister:
	mtpDBUnregisterObject(storage->db, object->uid);
free:
	mtpUtilFree(object);
end:
	mtpUtilFree(path);
	return ret;
}

int mtpObjAddRegularObject(MtpLogicalStorage *storage, SceUInt32 parent, SceUInt16 format,
	SceUInt32 size, SceUInt32 imagePixWidth, SceUInt32 imagePixHeight, const char *name,
	const char *dateCreated, const char *dateModified, SceUInt32 *handle)
{
	ObjectDataBasic *object;
	const char *relative;
	SceUInt32 storageId;
	SceUInt16 objectFormat;
	char *pfsPath;
	char *path;
	int ret;

	if (storage == NULL || storage->db == NULL || name == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 947, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 954, "unable to update logical info=0x%x", ret);
		return ret;
	}
	if (storage->numObjects >= storage->maxObjects) {
		MTP_ERR(MTP_TAG_OBJ, 959, "store full\n");
		return SCE_MTP_ERROR_STORAGE_FULL;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	path = (char *)mtpUtilMalloc(MTP_OBJ_PATH_SIZE * 2);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 970, "unable to alloc memory for full path\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = mtpObjBuildFullPathName(storage, parent, name, path, 1);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 978, "unable to get full path name=0x%x\n", ret);
		goto end;
	}
	relative = mtpUtilStrchr(path, ':') + 1;
	ret = mtpStrgCreatePfsPath(storage, relative, path + MTP_OBJ_PATH_SIZE, MTP_OBJ_PATH_SIZE,
		1);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 987, "CreatePfsPathFunc is not implemented\n");
		goto end;
	}
	pfsPath = path + MTP_OBJ_PATH_SIZE;
	ret = mtpFileCreate(pfsPath, format);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 993,
			"unable to create file or directory(or already exist)=0x%x, [%s]\n", ret,
			pfsPath);
		goto end;
	}
	objectFormat = format;
	object = (ObjectDataBasic *)mtpUtilMalloc(sizeof(*object));
	if (object == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1000, "unable to alloc memory for ObjectDataBasic\n");
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto delete_file;
	}
	mtpUtilMemset(object, 0, sizeof(*object));
	if (mtpSpecIsSupportedFormat(format) == 0)
		objectFormat = MTP_FORMAT_UNDEFINED;
	object->uid = 0;
	object->parentUid = parent;
	if (size == 0xFFFFFFFF)
		object->objectSize = 0xFFFFFFFFFFFFFFFFULL;
	else
		object->objectSize = size;
	object->objectFormat = objectFormat;
	object->protectionStatus = 0;
	mtpFileGetDates(pfsPath, &object->dateCreated, &object->dateModified);
	if (dateCreated != NULL)
		mtpUtilStringToUtcDateTime(dateCreated, &object->dateCreated);
	if (dateModified != NULL)
		mtpUtilStringToUtcDateTime(dateModified, &object->dateModified);
	mtpUtilStrcpy(object->fullPathName, relative);
	object->nonConsumable = 0;
	mtpUtilMd5(path, &object->dateCreated, object->puoid);
	format = objectFormat;
	ret = mtpDBRegisterObject(storage->db, object);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1034, "unable to register object to DB=0x%x\n", ret);
		goto free;
	}
	ret = mtpCacheSetObjectProperty(storage->cache, object->uid, MTP_DB_QUERY_WIDTH,
		&imagePixWidth, sizeof(imagePixWidth));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1041, "unable to set object width to Cache=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpCacheSetObjectProperty(storage->cache, object->uid, MTP_DB_QUERY_HEIGHT,
		&imagePixHeight, sizeof(imagePixHeight));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1046, "unable to set object width to Cache=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpCacheWriteBackDirtyCache(storage->cache);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1052, "unable to write back cache=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpStrgVTValidateUniqueId(storageId, object->uid, objectFormat, parent, handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1059,
			"unable to validate uniqueId in virtual MTP handle list=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1065, "unable to update logical info=0x%x", ret);
		goto unregister;
	}
	mtpUtilFree(object);
	mtpUtilFree(path);
	return ret;
unregister:
	mtpDBUnregisterObject(storage->db, object->uid);
free:
	mtpUtilFree(object);
delete_file:
	mtpFileDelete(pfsPath, format);
end:
	mtpUtilFree(path);
	return ret;
}

int mtpObjAddObjectData(MtpLogicalStorage *storage, SceUInt32 parent, const MtpObjectData *data,
	const char *path, SceUInt32 *handle)
{
	ObjectDataBasic *object;
	SceUInt16 format = MTP_FORMAT_UNDEFINED;
	const char *relative;
	SceUInt32 storageId;
	char *pfsPath = NULL;
	char *work = NULL;
	int ret;

	if (storage == NULL || storage->db == NULL || data == NULL || path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1109, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1116, "unable to update logical info=0x%x", ret);
		return ret;
	}
	if (storage->numObjects >= storage->maxObjects) {
		MTP_ERR(MTP_TAG_OBJ, 1121, "store full\n");
		return SCE_MTP_ERROR_STORAGE_FULL;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	relative = mtpUtilStrchr(path, ':') + 1;
	object = (ObjectDataBasic *)mtpUtilMalloc(sizeof(*object));
	if (object == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1135, "unable to alloc memory for ObjectDataBasic\n");
		ret = SCE_MTP_ERROR_NO_MEMORY;
		goto delete_file;
	}
	mtpUtilMemset(object, 0, sizeof(*object));
	format = data->objectFormat;
	if (mtpSpecIsSupportedFormat(format) == 0)
		format = MTP_FORMAT_UNDEFINED;
	object->uid = 0;
	object->parentUid = parent;
	object->objectFormat = format;
	object->protectionStatus = 0;
	object->objectSize = data->objectSize;
	mtpUtilMemcpy(&object->dateCreated, &data->dateCreated, sizeof(object->dateCreated));
	mtpUtilMemcpy(&object->dateModified, &data->dateModified, sizeof(object->dateModified));
	mtpUtilStrcpy(object->fullPathName, relative);
	mtpUtilStrcpy(object->name, data->name);
	object->nonConsumable = 0;
	mtpUtilMd5(data->fullPathName, &object->dateCreated, object->puoid);
	ret = mtpDBRegisterObject(storage->db, object);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1162, "unable to register object to DB=0x%x\n", ret);
		goto free;
	}
	ret = mtpCacheSetObjectProperty(storage->cache, object->uid, MTP_DB_QUERY_WIDTH,
		&data->width, sizeof(data->width));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1169, "unable to set object width to Cache=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpCacheSetObjectProperty(storage->cache, object->uid, MTP_DB_QUERY_HEIGHT,
		&data->height, sizeof(data->height));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1174, "unable to set object width to Cache=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpCacheWriteBackDirtyCache(storage->cache);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1180, "unable to write back cache=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpStrgVTValidateUniqueId(storageId, object->uid, format, parent, handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1187,
			"unable to validate uniqueId in virtual MTP handle list=0x%x\n", ret);
		goto unregister;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1193, "unable to update logical info=0x%x", ret);
		goto unregister;
	}
	mtpUtilFree(object);
	mtpUtilFree(work);
	return ret;
unregister:
	mtpDBUnregisterObject(storage->db, object->uid);
free:
	mtpUtilFree(object);
delete_file:
	mtpFileDelete(pfsPath, format);
	mtpUtilFree(work);
	return ret;
}

int mtpObjDeleteObjectData(MtpLogicalStorage *storage, SceUInt32 uniqueId)
{
	SceUInt32 storageId;
	int ret;

	if (storage == NULL || storage->db == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1229, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = mtpDBUnregisterObject(storage->db, uniqueId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1238, "unable to register object to DB=0x%x\n", ret);
		return ret;
	}
	ret = mtpCacheWriteBackDirtyCache(storage->cache);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1244, "unable to write back cache=0x%x\n", ret);
		return ret;
	}
	ret = mtpStrgVTInvalidateUniqueId(storageId, uniqueId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1250,
			"unable to validate uniqueId in virtual MTP handle list=0x%x\n", ret);
		return ret;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1256, "unable to update logical info=0x%x", ret);
		return ret;
	}
	ret = mtpCacheInvalidate(storage->cache, uniqueId);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OBJ, 1262, "unable to invalidate cache=0x%x\n", ret);
	return ret;
}

int mtpObjRemoveObject(MtpLogicalStorage *storage, SceUInt32 uniqueId)
{
	SceUInt16 format;
	SceUInt32 parent;
	SceUInt32 storageId;
	SceUInt32 length;
	char *pfsPath;
	char *path;
	int ret;

	if (storage == NULL || storage->db == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1288, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1298, "unable to update logical info=0x%x", ret);
		return ret;
	}
	length = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(length * 2 + MTP_OBJ_PATH_SIZE * 2);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1306, "unable to alloc memory for full path\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilStrcpy(path, storage->info->name);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_FULL_PATH_NAME,
		&path[length], MTP_OBJ_PATH_SIZE);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1318, "unable to get full path from cache=0x%x [%s]\n", ret,
			path);
		goto end;
	}
	pfsPath = &path[length + MTP_OBJ_PATH_SIZE];
	ret = mtpStrgCreatePfsPath(storage, &path[length], pfsPath, length + MTP_OBJ_PATH_SIZE, 1);
	if (ret < 0)
		goto end;
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_FORMAT,
		&format, sizeof(format));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1329, "unable to get object format from cache=0x%x\n", ret);
		goto end;
	}
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_PARENT_UID,
		&parent, sizeof(parent));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1336, "unable to get parent handle from cache=0x%x\n", ret);
		goto end;
	}
	ret = mtpDBUnregisterObject(storage->db, uniqueId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1342, "unable to register object to DB=0x%x\n", ret);
		goto end;
	}
	ret = mtpFileDelete(pfsPath, format);
	if (ret < 0 && ret != SCE_ERROR_ERRNO_ENOENT) {
		MTP_ERR(MTP_TAG_OBJ, 1351, "unable to delete file(%s)=0x%x\n", pfsPath, ret);
		goto end;
	}
	/* a game's sound data takes its album art with it */
	if (format == MTP_FORMAT_ABSTRACT_AUDIO_ALBUM) {
		mtpUtilStrrchr(pfsPath, '.')[1] = '\0';
		mtpUtilStrcat(pfsPath, MTP_OBJ_JPEG_EXTENSION);
		if (mtpFileExists(pfsPath)) {
			ret = mtpFileDelete(pfsPath, format);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_OBJ, 1363, "unable to delete album art(%s)=0x%x\n",
					pfsPath, ret);
				goto end;
			}
		}
	}
	ret = mtpCacheWriteBackDirtyCache(storage->cache);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1371, "unable to write back cache=0x%x\n", ret);
		goto end;
	}
	ret = mtpStrgVTInvalidateUniqueId(storageId, uniqueId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1377,
			"unable to validate uniqueId in virtual MTP handle list=0x%x\n", ret);
		goto end;
	}
	ret = mtpStrgUpdateLogicalInfo(storage);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1384, "unable to update logical info=0x%x", ret);
		goto end;
	}
	ret = mtpCacheInvalidate(storage->cache, uniqueId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1390, "unable to invalidate cache=0x%x\n", ret);
		goto end;
	}
	mtpUtilFree(path);
	return ret;
end:
	mtpUtilFree(path);
	return ret;
}

int mtpObjRemoveFile(MtpLogicalStorage *storage, SceUInt32 parent, const char *name)
{
	SceUInt32 handle;
	SceUInt32 uniqueId;
	char *path;
	int ret;

	if (storage == NULL || name == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	path = (char *)mtpUtilMalloc(MTP_OBJ_PATH_SIZE);
	if (path == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	ret = mtpObjBuildFullPathName(storage, parent, name, path, 1);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1434, "mtpObjBuildFullPathName failed: 0x%x\n", ret);
		goto end;
	}
	ret = mtpStrgGetMtpHandleByPath(path, &handle);
	if (ret < 0)
		goto end;
	ret = mtpStrgVTConvertMtpHandleToUniqueId(handle, &uniqueId);
	if (ret < 0)
		goto end;
	mtpUtilFree(path);
	return mtpObjRemoveObject(storage, uniqueId);
end:
	mtpUtilFree(path);
	return ret;
}

int mtpObjPackObjectInfo(MtpLogicalStorage *storage, SceUInt32 uniqueId, void *buf,
	SceSize size)
{
	SceUInt8 *info = (SceUInt8 *)buf;
	ObjectDataBasic *object;
	SceUInt16 associationType;
	SceUInt32 dimension;
	SceUInt32 storageId;
	SceUInt32 compressed;
	MtpString string;
	SceUInt32 handle;
	SceUInt32 parent;
	char date[20];
	const char *leaf;
	int length;
	int pos;
	int ret;

	if (storage == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1629, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, &handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1639, "unable to convert uniqueId to MTP handle=0x%x\n", ret);
		return ret;
	}
	object = (ObjectDataBasic *)mtpUtilMalloc(sizeof(*object));
	if (object == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1646, "unable to alloc memory for object data basic\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_DATA_BASIC,
		object, sizeof(*object));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1654, "unable to get object property from cache=0x%x\n", ret);
		goto end;
	}
	/* the ObjectInfo dataset, field by field */
	mtpUtilMemcpy(info, &storageId, 4);
	mtpUtilMemcpy(info + 4, &object->objectFormat, 2);
	mtpUtilMemcpy(info + 6, &object->protectionStatus, 2);
	if (handle & MTP_OBJECT_HANDLE_DIR) {
		mtpUtilMemset(info + 8, 0, 4);
	} else {
		compressed = 0;
		if ((SceInt64)object->objectSize <= 0xFFFFFFFFLL)
			compressed = (SceUInt32)object->objectSize;
		else
			compressed = 0xFFFFFFFF;
		mtpUtilMemcpy(info + 8, &compressed, 4);
	}
	mtpUtilMemset(info + 12, 0, 2);
	mtpUtilMemset(info + 14, 0, 4);
	mtpUtilMemset(info + 18, 0, 4);
	mtpUtilMemset(info + 22, 0, 4);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_WIDTH, &dimension,
		sizeof(dimension));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1696, "unable to get object widht from cache=0x%x\n", ret);
		goto end;
	}
	mtpUtilMemcpy(info + 26, &dimension, 4);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_HEIGHT, &dimension,
		sizeof(dimension));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1704, "unable to get object widht from cache=0x%x\n", ret);
		goto end;
	}
	mtpUtilMemcpy(info + 30, &dimension, 4);
	mtpUtilMemset(info + 34, 0, 4);
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, object->parentUid, &parent);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1714,
			"unable to convert paretnUniqueId to MTP handle=0x%x\n", ret);
		goto end;
	}
	if ((parent & MTP_OBJECT_HANDLE_INDEX_MASK) == MTP_OBJECT_HANDLE_INDEX_MASK)
		parent = 0;
	mtpUtilMemcpy(info + 38, &parent, 4);
	associationType = (handle & MTP_OBJECT_HANDLE_DIR) != 0;
	mtpUtilMemcpy(info + 42, &associationType, 2);
	mtpUtilMemset(info + 44, 0, 4);
	mtpUtilMemset(info + 48, 0, 4);
	/* the file name, the dates and the keywords, each an MTP string */
	string.numChars = 0xFF;
	string.chars = info + 53;
	leaf = mtpUtilStrrchr(object->fullPathName, '/');
	length = mtpUtilPackMtpString(&string, leaf != NULL ? leaf + 1 : object->fullPathName);
	if (length < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1747, "unable to pack MTP Extensions=0x%x", length);
		ret = length;
		goto end;
	}
	info[52] = string.numChars;
	pos = 52 + 1 + length;
	mtpUtilDateTimeToString(date, &object->dateCreated);
	string.numChars = 0xFF;
	string.chars = info + pos + 1;
	ret = mtpUtilPackMtpString(&string, date);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1760, "unable to pack MTP Extensions=0x%x", ret);
		goto end;
	}
	info[pos] = string.numChars;
	pos += 1 + ret;
	mtpUtilDateTimeToString(date, &object->dateModified);
	string.numChars = 0xFF;
	string.chars = info + pos + 1;
	ret = mtpUtilPackMtpString(&string, date);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1773, "unable to pack MTP Extensions=0x%x", ret);
		goto end;
	}
	info[pos] = string.numChars;
	pos += 1 + ret;
	string.numChars = 0xFF;
	string.chars = info + pos + 1;
	length = mtpUtilPackMtpString(&string, NULL);
	if (length < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1785, "unable to pack MTP Extensions=0x%x", length);
		ret = length;
		goto end;
	}
	info[pos] = string.numChars;
	ret = pos + 1 + length;
	if (size < (SceSize)ret)
		MTP_ERR(MTP_TAG_OBJ, 1796, "buffer too small...\n");
end:
	mtpUtilFree(object);
	return ret;
}

int mtpObjGetObjectData(MtpLogicalStorage *storage, SceUInt32 uniqueId, MtpObjectData *data)
{
	ObjectDataBasic *object;
	int ret;

	if (storage == NULL || data == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1819, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	object = (ObjectDataBasic *)mtpUtilMalloc(sizeof(*object));
	if (object == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1825, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_DATA_BASIC,
		object, sizeof(*object));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1831, "mtpCacheGetObjectProperty for 0x%x failed: 0x%x\n",
			uniqueId, ret);
		goto end;
	}
	mtpUtilMemset(data, 0, sizeof(*data));
	data->objectFormat = object->objectFormat;
	data->objectSize = object->objectSize;
	mtpUtilMemcpy(&data->dateCreated, &object->dateCreated, sizeof(data->dateCreated));
	mtpUtilMemcpy(&data->dateModified, &object->dateModified, sizeof(data->dateModified));
	mtpUtilStrncpy(data->name, object->name, sizeof(data->name));
	ret = mtpStrgCreatePfsPath(storage, object->fullPathName, data->fullPathName,
		sizeof(data->fullPathName), 0);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 1844, "CreatePfsPathFunc is not implemented\n");
		goto end;
	}
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_WIDTH, &data->width,
		sizeof(data->width));
	if (ret > 0) {
		MTP_ERR(MTP_TAG_OBJ, 1850, "mtpCacheGetObjectProperty for 0x%x failed: 0x%x\n",
			uniqueId, ret);
		goto end;
	}
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_HEIGHT,
		&data->height, sizeof(data->height));
	if (ret > 0)
		MTP_ERR(MTP_TAG_OBJ, 1855, "mtpCacheGetObjectProperty for 0x%x failed: 0x%x\n",
			uniqueId, ret);
end:
	mtpUtilFree(object);
	return ret;
}

/* one property UpdateObjectData writes to the cache */
typedef struct MtpObjCacheProperty {
	int type;		/* MTP_DB_QUERY_* */
	const void *value;
	SceSize size;
} MtpObjCacheProperty;

int mtpObjUpdateObjectData(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	const MtpObjectUpdateData *data)
{
	SceUInt16 format = MTP_FORMAT_UNDEFINED;
	int ret;
	int i;

	if (storage == NULL || storage->db == NULL || data == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 1939, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	MtpObjCacheProperty props[] = {
		{ MTP_DB_QUERY_OBJECT_SIZE, &data->objectSize, sizeof(data->objectSize) },
		{ MTP_DB_QUERY_OBJECT_FORMAT, &format, sizeof(format) },
		{ MTP_DB_QUERY_DATE_CREATED, &data->dateCreated, sizeof(data->dateCreated) },
		{ MTP_DB_QUERY_DATE_MODIFIED, &data->dateModified, sizeof(data->dateModified) },
		{ MTP_DB_QUERY_NAME, data->name, sizeof(data->name) },
		{ MTP_DB_QUERY_WIDTH, &data->width, sizeof(data->width) },
		{ MTP_DB_QUERY_HEIGHT, &data->height, sizeof(data->height) },
	};
	format = data->objectFormat;
	if (mtpSpecIsSupportedFormat(format) == 0)
		format = MTP_FORMAT_UNDEFINED;
	for (i = 0; i < (int)(sizeof(props) / sizeof(props[0])); i++) {
		ret = mtpCacheSetObjectProperty(storage->cache, uniqueId, props[i].type,
			props[i].value, props[i].size);
		if (ret > 0) {
			MTP_ERR(MTP_TAG_OBJ, 1969, "mtpCacheSetObjectProperty %d failed: 0x%x\n", i,
				ret);
			return ret;
		}
	}
	ret = mtpCacheWriteBackDirtyCache(storage->cache);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OBJ, 1975, "mtpCacheWriteBackDirtyCache failed: 0x%x\n", ret);
	return ret;
}

/* writes the sample's picture the host sent (an AUINT8) to the .jpg beside the object */
static inline int _setObjectPropValueRSD(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	const SceUInt8 *buf)
{
	SceUInt32 dataSize = 0;
	SceUInt32 length;
	char *path;
	char *pfsPath;
	SceUID fd;
	int ret;

	if (storage == NULL || storage->cache == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 434, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	length = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(length * 2 + MTP_OBJ_PATH_SIZE * 2);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 443, "unable to allocate memory for JPEG file name\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	pfsPath = &path[length + MTP_OBJ_PATH_SIZE];
	mtpUtilStrcpy(path, storage->info->name);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_FULL_PATH_NAME,
		&path[length], MTP_OBJ_PATH_SIZE);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 455, "unable to get full path name of .alb=0x%x\n", ret);
		goto end;
	}
	mtpUtilStrrchr(path, '.')[1] = '\0';
	mtpUtilStrcat(path, MTP_OBJ_JPEG_EXTENSION);
	ret = mtpStrgCreatePfsPath(storage, &path[length], pfsPath, length + MTP_OBJ_PATH_SIZE, 1);
	if (ret < 0)
		goto end;
	fd = mtpFileOpen(pfsPath, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6);
	if (fd < 0) {
		MTP_ERR(MTP_TAG_OBJ, 471, "unable to open %s(0x%x)\n", pfsPath, fd);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto end;
	}
	mtpUtilMemcpy(&dataSize, buf, sizeof(dataSize));
	mtpFileWrite(fd, buf + sizeof(dataSize), dataSize);
	mtpFileClose(fd);
end:
	mtpUtilFree(path);
	return ret;
}

int mtpObjSetObjectPropValue(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	SceUInt16 code, const void *value, int writeBack)
{
	SceUInt16 format;
	SceUInt32 offset;
	SceUInt32 valueSize = 0;
	MtpPropData prop;
	const MtpCodeList *props;
	const MtpPropDesc *desc;
	SceUInt32 i;
	int type;
	int ret;

	if (storage == NULL || storage->cache == NULL || value == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2197, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_FORMAT,
		&format, sizeof(format));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2205, "unable to get object format from cache=0x%x\n", ret);
		return ret;
	}
	props = mtpSpecGetObjectPropsSupported(format);
	if (props == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2212, "not supported object format code\n");
		return SCE_MTP_ERROR_UNSUPPORTED_FORMAT;
	}
	for (i = 0; i < props->count; i++) {
		if (props->codes[i] == code)
			break;
	}
	if (i >= props->count) {
		MTP_ERR(MTP_TAG_OBJ, 2224, "object prop code is not supported at this format\n");
		return SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
	}
	desc = mtpSpecGetObjectPropDesc(code);
	if (desc == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2232, "object prop code is not supported by device\n");
		return SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
	}
	if (desc->getSet == MTP_GET) {
		MTP_ERR(MTP_TAG_OBJ, 2239, "unable to set object property(get only)\n");
		return SCE_MTP_ERROR_READ_ONLY_PROPERTY;
	}
	if (code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA) {
		ret = _setObjectPropValueRSD(storage, uniqueId, (const SceUInt8 *)value);
		if (ret < 0)
			MTP_ERR(MTP_TAG_OBJ, 2305,
				"unable to set Representative Sample Data=0x%x\n", ret);
		return ret;
	}
	prop.dataType = desc->dataType;
	type = mtpDBObjectPropCode2QueryType(code);
	ret = mtpDBGetObjectPropertyOffset(type, &offset, &valueSize);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2256, "unable to get property size=0x%x\n", ret);
		return ret;
	}
	/* a date comes as a string of twice its SceDateTime's size */
	if (desc->formFlag == MTP_FORM_DATETIME)
		valueSize *= 2;
	prop.data = mtpUtilMalloc(valueSize);
	if (prop.data == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2268, "unable to alloc memory for work buffer\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(prop.data, 0, valueSize);
	prop.size = valueSize;
	if (mtpPropUnpackObjectPropValue(&prop, value) < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2278, "unable to unpack object property value\n");
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto free;
	}
	mtpDBFixFromObjectPropValue(type, prop.data, prop.data, valueSize);
	ret = mtpCacheSetObjectProperty(storage->cache, uniqueId, type, prop.data, valueSize);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2289, "unable to set object property=0x%x\n", ret);
		goto free;
	}
	if (writeBack == 1) {
		ret = mtpCacheWriteBackDirtyCache(storage->cache);
		if (ret < 0)
			MTP_ERR(MTP_TAG_OBJ, 2296, "unable to write back cache=0x%x\n", ret);
	}
free:
	mtpUtilFree(prop.data);
	return ret;
}

/* one element of an ObjectPropList: handle, code, data type, value */
static __attribute__((noinline)) int _packOneObjectPropListElement(MtpLogicalStorage *storage,
	SceUInt32 uniqueId, SceUInt32 handle, SceUInt16 code, SceUInt16 dataType,
	SceUInt8 *buf, SceSize size)
{
	int ret;

	mtpUtilMemset(buf, 0, size);
	mtpUtilMemcpy(buf, &handle, 4);
	mtpUtilMemcpy(buf + 4, &code, 2);
	mtpUtilMemcpy(buf + 6, &dataType, 2);
	ret = mtpObjPackObjectPropValue(storage, uniqueId, code, buf + 8, size - 8) + 8;
	if (ret < 0)
		MTP_ERR(MTP_TAG_OBJ, 77, "unable to pack object prop value=0x%x\n", ret);
	return ret;
}

/* the representative sample's element; with buf NULL, only its size */
static __attribute__((noinline)) int _packRSDObjectPropListElement(MtpLogicalStorage *storage,
	SceUInt32 uniqueId, SceUInt32 handle, SceUInt16 code, SceUInt16 dataType,
	SceUInt8 *buf, SceSize size)
{
	int ret;

	if (buf != NULL) {
		mtpUtilMemset(buf, 0, size);
		mtpUtilMemcpy(buf, &handle, 4);
		mtpUtilMemcpy(buf + 4, &code, 2);
		mtpUtilMemcpy(buf + 6, &dataType, 2);
		ret = _packObjectPropValueRSD(storage, uniqueId, buf + 8, size - 8);
	} else {
		ret = _packObjectPropValueRSD(storage, uniqueId, NULL, size - 8);
	}
	ret += 8;
	if (ret < 0)
		MTP_ERR(MTP_TAG_OBJ, 124, "unable to pack representative sample data\n");
	return ret;
}

/*
 * An object's elements of an ObjectPropList: of one property (code), of a group (code 0)
 * or all but the sample (code 0xFFFF). With buf NULL they are counted in *count.
 */
static __attribute__((noinline)) int _packObjectPropListElements(MtpLogicalStorage *storage,
	SceUInt32 uniqueId, SceUInt16 code, SceUInt16 group, SceUInt8 *buf, SceUInt32 *count)
{
	SceUInt32 storageId = MTP_LOGICAL_STORAGE_ID(storage);
	const MtpCodeList *props;
	const MtpPropDesc *desc;
	SceUInt16 format;
	SceUInt32 handle;
	SceUInt8 *work;
	SceUInt32 i;
	int size = 0;
	int n;
	int ret;

	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_FORMAT,
		&format, sizeof(format));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 162, "unable to get object property form cache=0x%x\n", ret);
		return ret;
	}
	props = mtpSpecGetObjectPropsSupported(format);
	if (props == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 169, "object format is not supported=%04x\n", ret);
		return SCE_MTP_ERROR_UNSUPPORTED_FORMAT;
	}
	work = (SceUInt8 *)mtpUtilMalloc(MTP_OBJ_ELEMENT_SIZE);
	if (work == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 177, "unable to alloc memory for work buffer\n");
		return ret;
	}
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, &handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 183, "unable to convert uniqueId to MTP handle=0x%x", ret);
		goto free;
	}
	if (code == 0) {
		for (i = 0; i < props->count; i++) {
			desc = mtpSpecGetObjectPropDesc(props->codes[i]);
			if (desc == NULL) {
				MTP_ERR(MTP_TAG_OBJ, 194, "unable to find object prop desc=%04x\n",
					props->codes[i]);
				ret = SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
				goto free;
			}
			if (desc->groupCode != group)
				continue;
			if (desc->code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA)
				n = _packRSDObjectPropListElement(storage, uniqueId, handle, desc->code,
					desc->dataType, buf, MTP_OBJECT_PROP_VALUE_SIZE_LARGE);
			else
				n = _packOneObjectPropListElement(storage, uniqueId, handle, desc->code,
					desc->dataType, work, MTP_OBJ_ELEMENT_SIZE);
			if (n < 0) {
				ret = n;
				MTP_ERR(MTP_TAG_OBJ, 213, "unable to pack object prop value=0x%x\n", n);
				goto free;
			}
			if (buf != NULL) {
				if (desc->code != MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA)
					mtpUtilMemcpy(&buf[size], work, n);
			} else {
				(*count)++;
			}
			size += n;
		}
	} else if (code == 0xFFFF) {
		for (i = 0; i < props->count; i++) {
			desc = mtpSpecGetObjectPropDesc(props->codes[i]);
			if (desc == NULL) {
				MTP_ERR(MTP_TAG_OBJ, 236, "unable to find object prop desc=%04x\n",
					props->codes[i]);
				ret = SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
				goto free;
			}
			if (desc->code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA)
				continue;
			n = _packOneObjectPropListElement(storage, uniqueId, handle, desc->code,
				desc->dataType, work, MTP_OBJ_ELEMENT_SIZE);
			if (n < 0) {
				ret = n;
				MTP_ERR(MTP_TAG_OBJ, 252, "unable to pack object prop value=0x%x\n", n);
				goto free;
			}
			if (buf != NULL)
				mtpUtilMemcpy(&buf[size], work, n);
			else
				(*count)++;
			size += n;
		}
	} else {
		desc = mtpSpecGetObjectPropDesc(code);
		if (desc == NULL) {
			MTP_ERR(MTP_TAG_OBJ, 270, "unable to find object prop desc=%04x\n", code);
			ret = SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
			goto free;
		}
		if (desc->code == MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA)
			size = _packRSDObjectPropListElement(storage, uniqueId, handle, desc->code,
				desc->dataType, buf, MTP_OBJECT_PROP_VALUE_SIZE_LARGE);
		else
			size = _packOneObjectPropListElement(storage, uniqueId, handle, desc->code,
				desc->dataType, work, MTP_OBJ_ELEMENT_SIZE);
		if (size < 0) {
			ret = size;
			MTP_ERR(MTP_TAG_OBJ, 285, "unable to pack object prop value=0x%x\n", size);
			goto free;
		}
		if (buf != NULL) {
			if (desc->code != MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA)
				mtpUtilMemcpy(buf, work, size);
		} else {
			(*count)++;
		}
	}
	ret = size;
free:
	if (work != NULL)
		mtpUtilFree(work);
	return ret;
}

int mtpObjPackObjectPropList(MtpLogicalStorage *storage, SceUInt32 uniqueId, SceUInt16 format,
	SceUInt16 code, SceUInt32 group, SceUInt32 depth, void *buf, SceUInt32 *count,
	SceUInt32 *size, int *send)
{
	SceUInt32 storageId;
	SceUInt16 objectFormat;
	SceUInt32 handle;
	SceUInt32 child;
	SceUInt32 num = 0;
	SceUInt32 *children;
	SceUInt32 room;
	SceUInt32 left;
	SceUInt32 pos = 0;
	SceUInt32 i;
	int n;
	int ret;

	if (storage == NULL || storage->cache == NULL || storage->db == NULL || count == NULL ||
	    size == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2351, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, &handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2361, "unable to convert uniqueId to MTP handle=0x%x\n", ret);
		return ret;
	}
	/* a storage's root has no properties, only children */
	if ((handle & MTP_OBJECT_HANDLE_INDEX_MASK) != MTP_OBJECT_HANDLE_INDEX_MASK) {
		ret = mtpCacheGetObjectProperty(storage->cache, uniqueId,
			MTP_DB_QUERY_OBJECT_FORMAT, &objectFormat, sizeof(objectFormat));
		if (ret < 0) {
			if (ret != SCE_MTP_ERROR_NO_SUCH_OBJECT)
				MTP_ERR(MTP_TAG_OBJ, 2372,
					"unable to get object format from cache=0x%x\n", ret);
			return ret;
		}
		if (format == 0 || format == objectFormat) {
			n = _packObjectPropListElements(storage, uniqueId, code, (SceUInt16)group,
				(SceUInt8 *)buf, count);
			if (n < 0) {
				MTP_ERR(MTP_TAG_OBJ, 2384,
					"unable to pack object prop list element=0x%x\n", n);
				return n;
			}
			if (buf != NULL) {
				if (send != NULL)
					*send = 1;
				ret = mtpCoreSendData(buf, n, *size);
				if (ret < 0) {
					MTP_ERR(MTP_TAG_OBJ, 2394, "unable to send data=0x%x\n", ret);
					if (send != NULL)
						*send = 0;
					return ret;
				}
			} else {
				*size += n;
			}
		}
	}
	if (depth == 0 || !(handle & MTP_OBJECT_HANDLE_DIR))
		return ret;
	ret = mtpStrgVTGetNumUniqueIds(storageId, uniqueId, &num);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2412, "unable to get num objects=0x%x\n", ret);
		return ret;
	}
	room = 4096;
	if (num * 4 < 4096)
		room = num * 4;
	children = (SceUInt32 *)mtpUtilMalloc(room);
	if (children == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2421, "unable to alloc memory for children\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	/* the children a buffer at a time */
	left = 0;
	for (i = 0; i < num; i++) {
		if (left == 0) {
			pos = 0;
			left = room / 4;
			ret = mtpStrgVTGetUniqueIds(storageId, uniqueId, i, room / 4, children);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_OBJ, 2436, "unable to get object handles=0x%x\n", ret);
				break;
			}
		}
		mtpUtilMemcpy(&child, &children[pos], sizeof(child));
		ret = mtpObjPackObjectPropList(storage, child, format, code, group, depth - 1, buf,
			count, size, send);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2444, "unable to get object prop list=0x%x\n", ret);
			break;
		}
		left--;
		pos++;
	}
	mtpUtilFree(children);
	return ret;
}

/*
 * The ObjectReferences of an object: a folder's children, or the tracks an album or a
 * playlist file lists (an album's only while they are still of that album).
 */
int mtpObjPackObjectReferences(MtpLogicalStorage *storage, SceUInt32 uniqueId, void *buf,
	SceSize size)
{
	SceUInt8 *refs = (SceUInt8 *)buf;
	SceUInt32 storageId;
	SceUInt16 format;
	SceUInt16 refFormat;
	SceUInt32 num = 0;
	SceUInt32 count = 0;
	SceUInt32 ref;
	SceUInt32 handle;
	SceUInt32 length;
	SceUInt8 *out;
	char *path;
	char *album;
	char *trackAlbum;
	SceUID fd = -1;
	SceUInt32 i;
	int packed;
	int ret;

	if (storage == NULL || storage->cache == NULL || storage->db == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2484, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	storageId = MTP_LOGICAL_STORAGE_ID(storage);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_FORMAT,
		&format, sizeof(format));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2495, "unable to get object format from cache=0x%x\n", ret);
		return ret;
	}
	if (format == MTP_FORMAT_ASSOCIATION) {
		ret = mtpStrgVTGetNumUniqueIds(storageId, uniqueId, &num);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2502, "unable to get num objects=0x%x\n", ret);
			return ret;
		}
		mtpUtilMemcpy(refs, &num, sizeof(num));
		ret = mtpStrgVTGetUniqueIds(storageId, uniqueId, 0, num, (SceUInt32 *)(refs + 4));
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2508, "unable to get object handles=0x%x\n", ret);
			return ret;
		}
		path = NULL;
		packed = num * 4 + 4;
		goto convert;
	}
	if (format != MTP_FORMAT_ABSTRACT_AUDIO_ALBUM && format != MTP_FORMAT_ABSTRACT_AV_PLAYLIST) {
		mtpUtilMemcpy(refs, &num, sizeof(num));
		path = NULL;
		packed = 4;
		goto convert;
	}
	length = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(length + MTP_OBJ_PATH_SIZE + 256 + 256);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2522, "unable to alloc memory for full path\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	album = &path[length + MTP_OBJ_PATH_SIZE];
	trackAlbum = &path[length + MTP_OBJ_PATH_SIZE + 256];
	mtpUtilStrcpy(path, storage->info->name);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_FULL_PATH_NAME,
		&path[length], MTP_OBJ_PATH_SIZE);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2535, "unable to get full path name from cache=0x%x\n", ret);
		goto free;
	}
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_NAME, album, 256);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2542, "unable to get name from cache=0x%x\n", ret);
		goto free;
	}
	fd = mtpFileOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2548, "unable to open %s\n", path);
		ret = fd;
		goto free;
	}
	/* the file: the number of unique ids, then the ids */
	ret = mtpFileRead(fd, &num, sizeof(num));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2555, "unable to read=0x%x\n", ret);
		goto close;
	}
	packed = ret;
	if (num != 0) {
		out = &refs[ret];
		for (i = 0; i < num; i++) {
			ret = mtpFileRead(fd, &ref, sizeof(ref));
			if (ret < 0) {
				MTP_ERR(MTP_TAG_OBJ, 2564, "unable to read=0x%x\n", ret);
				goto close;
			}
			if (mtpCacheGetObjectProperty(storage->cache, ref, MTP_DB_QUERY_OBJECT_FORMAT,
			    &refFormat, sizeof(refFormat)) >= 0 &&
			    mtpSpecIsReferenceFormat(refFormat) &&
			    mtpCacheGetObjectProperty(storage->cache, ref, MTP_DB_QUERY_ALBUM,
			    trackAlbum, 256) >= 0 &&
			    (format == MTP_FORMAT_ABSTRACT_AV_PLAYLIST ||
			     mtpUtilStrcmp(album, trackAlbum) == 0)) {
				mtpUtilMemcpy(out, &ref, sizeof(ref));
				count++;
				out += 4;
				packed += 4;
			}
		}
	}
	mtpUtilMemcpy(refs, &count, sizeof(count));
	num = count;
convert:
	/* the unique ids become the host's handles */
	for (i = 0; i < num; i++) {
		mtpUtilMemcpy(&ref, refs + 4 + i * 4, sizeof(ref));
		ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, ref, &handle);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2618, "unable to convert MTP handle=0x%x\n", ret);
			goto close;
		}
		mtpUtilMemcpy(refs + 4 + i * 4, &handle, sizeof(handle));
	}
	ret = packed;
close:
	if (fd >= 0)
		mtpFileClose(fd);
free:
	if (path != NULL)
		mtpUtilFree(path);
	return ret;
}

/* writes the tracks the host gives an album or a playlist to its file */
int mtpObjSetObjectReferences(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	const void *references, SceSize size)
{
	const SceUInt8 *refs = (const SceUInt8 *)references;
	SceUInt16 format;
	SceUInt16 refFormat;
	SceUInt32 num = 0;
	SceUInt32 written = 0;
	SceUInt32 handle;
	SceUInt32 ref;
	SceOff fileSize;
	SceUInt32 length;
	char *path;
	char *album;
	char *trackAlbum;
	char *pfsPath;
	const char *file = NULL;
	SceUID fd;
	SceUInt32 i;
	int ret;

	if (storage == NULL || storage->cache == NULL || references == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2665, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpUtilMemcpy(&num, refs, sizeof(num));
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_FORMAT,
		&format, sizeof(format));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2676, "unable to get object format from cache=0x%x\n", ret);
		return ret;
	}
	if (format != MTP_FORMAT_ABSTRACT_AUDIO_ALBUM && format != MTP_FORMAT_ABSTRACT_AV_PLAYLIST)
		return ret;
	length = mtpUtilStrlen(storage->info->name);
	path = (char *)mtpUtilMalloc(length * 2 + MTP_OBJ_PATH_SIZE * 2 + 256 + 256);
	if (path == NULL) {
		MTP_ERR(MTP_TAG_OBJ, 2685, "unable to alloc memory for full path\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	album = &path[length + MTP_OBJ_PATH_SIZE];
	trackAlbum = &path[length + MTP_OBJ_PATH_SIZE + 256];
	pfsPath = &path[length + MTP_OBJ_PATH_SIZE + 256 + 256];
	mtpUtilStrcpy(path, storage->info->name);
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_FULL_PATH_NAME,
		&path[length], MTP_OBJ_PATH_SIZE);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2699, "unable to get full path name from cache=0x%x\n", ret);
		goto free;
	}
	ret = mtpStrgCreatePfsPath(storage, &path[length], pfsPath, length + MTP_OBJ_PATH_SIZE, 1);
	if (ret < 0)
		goto free;
	ret = mtpCacheGetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_NAME, album, 256);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2710, "unable to get name from cache=0x%x\n", ret);
		goto free;
	}
	file = pfsPath;
	fd = mtpFileOpen(file, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6);
	if (fd < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2716, "unable to open %s\n", pfsPath);
		ret = fd;
		goto free;
	}
	/* the count goes first, once the tracks that are kept are known */
	if (mtpFileSeek(fd, 4, SCE_SEEK_SET) < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2723, "unable to seek\n");
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto close;
	}
	written = num;
	refs += 4;
	for (i = 0; i < num; i++) {
		mtpUtilMemcpy(&handle, refs, sizeof(handle));
		refs += 4;
		if (mtpStrgVTConvertMtpHandleToUniqueId(handle, &ref) >= 0 &&
		    mtpCacheGetObjectProperty(storage->cache, ref, MTP_DB_QUERY_OBJECT_FORMAT,
		    &refFormat, sizeof(refFormat)) >= 0 &&
		    mtpSpecIsReferenceFormat(refFormat) &&
		    mtpCacheGetObjectProperty(storage->cache, ref, MTP_DB_QUERY_ALBUM, trackAlbum,
		    256) >= 0 &&
		    (format == MTP_FORMAT_ABSTRACT_AV_PLAYLIST ||
		     mtpUtilStrcmp(album, trackAlbum) == 0)) {
			ret = mtpFileWrite(fd, &ref, sizeof(ref));
			if (ret < 0) {
				MTP_ERR(MTP_TAG_OBJ, 2793, "unable to write=0x%x\n", ret);
				goto close;
			}
		} else if (written != 0) {
			written--;
		}
	}
	if (mtpFileSeek(fd, 0, SCE_SEEK_SET) < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2800, "unable to seek to head of file\n");
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
	} else {
		ret = mtpFileWrite(fd, &written, sizeof(written));
		if (ret < 0)
			MTP_ERR(MTP_TAG_OBJ, 2807, "unable to write=0x%x\n", ret);
	}
close:
	if (fd >= 0)
		mtpFileClose(fd);
	/* the file's new size goes to the cache */
	if (file != NULL && mtpFileGetSize(file, &fileSize) >= 0) {
		ret = mtpCacheSetObjectProperty(storage->cache, uniqueId, MTP_DB_QUERY_OBJECT_SIZE,
			&fileSize, sizeof(fileSize));
		if (ret < 0) {
			MTP_ERR(MTP_TAG_OBJ, 2821, "unable to set size to cache=0x%x\n", ret);
		} else {
			ret = mtpCacheWriteBackDirtyCache(storage->cache);
			if (ret < 0)
				MTP_ERR(MTP_TAG_OBJ, 2826, "unable to write back cache=0x%x\n", ret);
		}
	}
free:
	if (path != NULL)
		mtpUtilFree(path);
	return ret;
}

int mtpObjNotifyProprtyUpdate(SceUInt32 uniqueId, SceUInt32 storageId)
{
	MtpEvent event;
	SceUInt32 handle;
	int ret;

	ret = mtpStrgVTConvertUniqueIdToMtpHandle(storageId, uniqueId, &handle);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OBJ, 2908,
			"mtpStrgVTConvertUniqueIdToMtpHandle failed: 0x%x, %u\n", ret, uniqueId);
		return ret;
	}
	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = handle;
	event.code = MTP_EVENT_OBJECT_PROP_CHANGED;
	event.numParams = 2;
	event.param[1] = MTP_OBJECT_PROP_OBJECT_SIZE;
	ret = mtpCoreSendResponse(&event, 0);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OBJ, 2920, "mtpCoreSendEvent failed: 0x%x\n", ret);
	return ret;
}

} /* extern "C" */
