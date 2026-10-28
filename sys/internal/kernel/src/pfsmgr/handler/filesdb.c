/*
 * files.db: the entry of every file and directory of a pmi (its index,
 * attributes, size and icv hash) and the disk space they take. Each access
 * opens the table again and checks its header against the hmac the pmi
 * remembered.
 */

#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <sysmem/utils.h>
#include "pfsmgr_internal.h"

#define PFS_DB_TYPE		3	/* name unknown */
#define PFS_DB_TYPE_X		5	/* name unknown: for pfsFsTypeIsX */
#define PFS_DB_KEY_PARAM_TYPE	4	/* name unknown: first type with key params */


int pfsDbCreate(ScePfsPmi *pmi, ScePfsNode *node)
{
	const ScePfsModeInfo *info;
	ScePfsDbTable table;
	ScePfsNode dir;
	ScePfsHash key;
	SceUInt16 fsType;
	SceUInt32 type;
	SceUID pid;
	int ret;

	info = pfsModeInfo(pmi->mode);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	ret = file_access_create(node, info->fileMode);
	if (ret != 0)
		goto unlock;
	pid = SceThreadmgrForDriver_0486F239(SCE_KERNEL_PROCESS_ID);
	ret = file_access_open(&pmi->filesDb, node, -1, info->icvOpenFlags | PFS_OPEN_KERNEL);
	SceThreadmgrForDriver_0486F239(pid);
	if (ret == 0) {
		pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
		fsType = pfsModeFsType(pmi->mode);
		type = pfsFsTypeIsX(fsType) ? PFS_DB_TYPE_X : PFS_DB_TYPE;
		pfsDbTableInit(&table, &pmi->filesDbLru);
		ret = pfsDbTableCreate(&table, &pmi->filesDb, &key, fsType, pmi->a6, type, pmi->a8);
		if (ret == 0) {
			pmi->version = table.header.type;
			ret = pfsDbTableFlush(&table);
			icv_copy(&pmi->filesDbHash, &table.header.hmac);
			pfsDbTableTerm(&table);
			goto unlock;
		}
		pfsDbTableFlush(&table);
		pfsDbTableTerm(&table);
		file_access_close(&pmi->filesDb);
	}
	dir.vp = pmi->sceDirVp;
	dir.name = NULL;
	file_access_remove(&dir, node);
	pfsNodeInactive(node);
unlock:
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbOpen(ScePfsPmi *pmi, ScePfsNode *node)
{
	const ScePfsModeInfo *info;
	ScePfsDbTable table;
	ScePfsHash key;
	SceUInt32 type;
	SceUID pid;
	int ret;

	info = pfsModeInfo(pmi->mode);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pid = SceThreadmgrForDriver_0486F239(SCE_KERNEL_PROCESS_ID);
	ret = file_access_open(&pmi->filesDb, node, -1, info->icvOpenFlags | PFS_OPEN_KERNEL);
	SceThreadmgrForDriver_0486F239(pid);
	if (ret != 0)
		goto unlock;
	ret = pfsDbTableReadInfo(&pmi->filesDb, &type, &pmi->a8, &pmi->a6);
	if (ret != 0)
		goto close;

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &key);
	if (ret != 0)
		goto term;
	icv_copy(&pmi->filesDbHash, &table.header.hmac);
	if (pfsFsTypeIsX(pfsModeFsType(pmi->mode))) {
		if (type > PFS_DB_TYPE_X) {
			ret = SCE_PFS_ERROR_DB_TYPE;
			goto termClose;
		}
		if (type >= PFS_DB_KEY_PARAM_TYPE)
			goto checkFsType;
	} else if (type > PFS_DB_TYPE) {
		ret = SCE_PFS_ERROR_DB_TYPE;
		goto termClose;
	}
	if (pmi->a8 != 0 || pmi->a6 != 0) {
		ret = SCE_PFS_ERROR_DB_KEY_PARAM;
		goto termClose;
	}
checkFsType:
	ret = pfsModeCheckFsType(pmi->mode, table.header.fsType);
	if (ret != 0)
		goto term;
	if (table.header.fsType == 1 || table.header.fsType == 4) {
		ret = pfsDbTableVerify(&table);
		if (ret != 0 && !(pmi->flags & PFS_PMI_NO_KLICENSEE)) {
			pfsDbTableTerm(&table);
			goto close;
		}
	}
	pmi->version = table.header.type;
	pfsDbTableTerm(&table);
	ret = 0;
	goto unlock;

termClose:
	pfsDbTableTerm(&table);
	goto close;
term:
	pfsDbTableTerm(&table);
close:
	file_access_close(&pmi->filesDb);
unlock:
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbGetUsage(ScePfsPmi *pmi, SceUInt64 *usage)
{
	ScePfsDbTable table;
	ScePfsHash key;
	int ret;

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &key);
	if (ret == 0) {
		if (!icv_equal(&pmi->filesDbHash, &table.header.hmac))
			ret = SCE_PFS_ERROR_HASH;
		else
			*usage = table.header.usage;
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbAddUsage(ScePfsPmi *pmi, SceInt64 delta)
{
	ScePfsDbTable table;
	ScePfsHash key;
	SceUInt64 usage;
	int ret;

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &key);
	if (ret == 0) {
		if (!icv_equal(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			usage = table.header.usage + delta;
			if (usage != table.header.usage) {
				table.dirty = 1;
				table.header.usage = usage;
			}
			ret = pfsDbTableFlush(&table);
			if (ret == 0)
				icv_copy(&pmi->filesDbHash, &table.header.hmac);
		}
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbGetSize(ScePfsPmi *pmi, SceUInt64 *size)
{
	ScePfsDbTable table;
	ScePfsHash key;
	int ret;

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &key);
	if (ret == 0) {
		if (!icv_equal(&pmi->filesDbHash, &table.header.hmac))
			ret = SCE_PFS_ERROR_HASH;
		else
			ret = pfsDbTableGetSize(&table, size);
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


/* the files.db index of every entry, as many as fit; returns how many */
int filesdb_fix_storage_use(ScePfsPmi *pmi, void *buf, SceSize len)
{
	ScePfsDbTable table;
	char name[PFS_DB_NAME_MAX];
	ScePfsHash key;
	SceUInt32 parentIndex, index;
	SceSize off;
	int ret, count;

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	memset(name, 0, sizeof(name));
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &key);
	if (ret != 0)
		goto out;
	if (!icv_equal(&pmi->filesDbHash, &table.header.hmac)) {
		ret = SCE_PFS_ERROR_HASH;
		goto out;
	}
	ret = pfsDbTableFirst(&table, &parentIndex, name, &index, NULL, NULL);
	if (ret != 0)
		goto out;
	count = 0;
	for (off = 0; off < len; off += sizeof(SceUInt32)) {
		*(SceUInt32 *)((char *)buf + off) = index;
		ret = pfsDbTableNext(&table, &parentIndex, name, &index, NULL, NULL);
		count++;
		if (ret == SCE_PFS_ERROR_DB_END) {
			ret = count;
			break;
		}
		if (ret != 0)
			break;
		ret = count;
	}
out:
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbInsert(ScePfsPmi *pmi, ScePfsDbKey *key, const void *hash, SceUInt32 index,
		SceUInt16 attr, SceUInt64 size)
{
	ScePfsDbTable table;
	ScePfsHash dbKey;
	int ret;

	pfs_get_secret(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!icv_equal(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			ret = pfsDbTableInsert(&table, key, hash, index, attr, size);
			if (ret == 0 && (ret = pfsDbTableFlush(&table)) == 0)
				icv_copy(&pmi->filesDbHash, &table.header.hmac);
		}
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbFind(ScePfsPmi *pmi, ScePfsDbKey *key, void *hash, SceUInt32 *index,
	      SceUInt16 *attr, SceUInt64 *size)
{
	ScePfsDbTable table;
	ScePfsHash dbKey;
	int ret;

	pfs_get_secret(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!icv_equal(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			ret = pfsDbTableFind(&table, key, hash, index, attr, size);
			/* signed file systems are never written: no write bits */
			if (ret == 0 && (table.header.fsType == 1 || table.header.fsType == 4) &&
			    attr != NULL && (*attr & 0x8000))
				*attr &= ~6;
		}
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbUpdate(ScePfsPmi *pmi, ScePfsDbKey *key, const void *hash, SceUInt16 *attr,
		SceUInt64 *size, SceUInt64 *unused, SceInt64 usageDelta)
{
	ScePfsDbTable table;
	ScePfsHash dbKey;
	SceUInt64 usage;
	int ret;

	pfs_get_secret(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!icv_equal(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			ret = pfsDbTableUpdate(&table, key, hash, NULL, attr, size);
			if (ret == 0) {
				usage = table.header.usage + usageDelta;
				if (usage != table.header.usage) {
					table.dirty = 1;
					table.header.usage = usage;
				}
				ret = pfsDbTableFlush(&table);
				if (ret == 0)
					icv_copy(&pmi->filesDbHash, &table.header.hmac);
			}
		}
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbRemove(ScePfsPmi *pmi, ScePfsDbKey *key)
{
	ScePfsDbTable table;
	ScePfsHash dbKey;
	int ret;

	pfs_get_secret(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!icv_equal(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			ret = pfsDbTableRemove(&table, key);
			if (ret == 0 && (ret = pfsDbTableFlush(&table)) == 0)
				icv_copy(&pmi->filesDbHash, &table.header.hmac);
		}
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}


int pfsDbClose(ScePfsPmi *pmi)
{
	ScePfsLowerFile *lf = &pmi->filesDb;

	if (pmi->flags & PFS_PMI_NOT_MOUNTED)
		return file_access_close_force(lf);
	return file_access_close(lf);
}


/* count the disk space of every entry again and correct the usage */
int pfsDbCheck(ScePfsPmi *pmi)
{
	ScePfsDbTable table;
	char name[PFS_DB_NAME_MAX];
	ScePfsHash key;
	SceUInt32 parentIndex, index;
	SceUInt16 attr;
	SceUInt64 size, disk, usage;
	int ret, err;

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = fsdb_restart(&table, &pmi->filesDb, &key);
	err = ret;
	if (ret != 0)
		goto out;
	if (!icv_equal(&pmi->filesDbHash, &table.header.hmac)) {
		err = SCE_PFS_ERROR_HASH;
		goto out;
	}
	usage = 0;
	memset(name, 0, sizeof(name));
	err = pfsDbTableFirst(&table, &parentIndex, name, &index, &attr, &size);
	while (err == 0) {
		disk = 0;
		pfsDiskSize(pmi, attr, index, 0, size, &disk);
		usage += disk;
		err = pfsDbTableNext(&table, &parentIndex, name, &index, &attr, &size);
	}
	if (err == SCE_PFS_ERROR_DB_END) {
		err = ret;
		if (usage != table.header.usage) {
			table.dirty = 1;
			table.header.usage = usage;
			err = pfsDbTableFlush(&table);
			if (err == 0)
				icv_copy(&pmi->filesDbHash, &table.header.hmac);
		}
	}
out:
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return err;
}


/* the key files.db's header signature is checked with (names unknown) */
static const SceUInt8 g_pfsFilesDbModulus[0x100] = {
	0x77, 0x0e, 0x4f, 0x4f, 0x51, 0x4a, 0xa6, 0x29, 0xd0, 0x4f, 0xdf, 0xe3, 0x0f, 0x2f, 0x5b, 0xc6,
	0xe0, 0xd4, 0x4b, 0x8d, 0x7a, 0xee, 0x6a, 0xb1, 0xb4, 0x1b, 0x21, 0xe7, 0x8d, 0x08, 0x08, 0xfd,
	0x11, 0xfe, 0xac, 0xd4, 0x58, 0xa4, 0xb2, 0x59, 0x5a, 0x28, 0x8c, 0x65, 0x5f, 0xeb, 0x3c, 0x27,
	0x4e, 0xd9, 0xd7, 0x05, 0xf5, 0x05, 0xa1, 0x01, 0xdb, 0xb2, 0x13, 0x94, 0xff, 0xfe, 0x73, 0x65,
	0xaf, 0xbf, 0x15, 0x5e, 0x02, 0xf4, 0xc2, 0x7d, 0x9a, 0xf0, 0x3b, 0x49, 0x69, 0xde, 0x06, 0x12,
	0xd8, 0x2d, 0x03, 0x57, 0xe7, 0x4e, 0xb7, 0x72, 0x51, 0x8f, 0x14, 0x8a, 0x5d, 0x38, 0x86, 0x6b,
	0x47, 0x1a, 0x4a, 0x48, 0xeb, 0xb9, 0x5d, 0xfd, 0xfb, 0x20, 0xdf, 0x26, 0x5f, 0x44, 0xc7, 0xde,
	0x4d, 0x2f, 0xce, 0x04, 0x39, 0x99, 0xe5, 0xcd, 0x5f, 0xac, 0x1a, 0x1f, 0x64, 0xf1, 0xe3, 0x38,
	0x65, 0x63, 0x0c, 0xf9, 0xaf, 0xe4, 0xe6, 0x84, 0x81, 0xb3, 0x69, 0xf9, 0xfc, 0x5b, 0xe1, 0xb6,
	0xbc, 0x08, 0xfd, 0xb2, 0xb9, 0x44, 0xf6, 0x38, 0xf7, 0x0b, 0x53, 0xf9, 0xea, 0xa7, 0x54, 0xa1,
	0x16, 0x5d, 0x33, 0x27, 0xf7, 0x34, 0xc8, 0xe4, 0x56, 0x43, 0xcf, 0x78, 0x94, 0x6f, 0x30, 0x99,
	0x76, 0x94, 0x7a, 0x0d, 0x92, 0x09, 0x1d, 0x4c, 0x1c, 0x88, 0xa9, 0x80, 0xfa, 0xa8, 0x95, 0x5f,
	0x70, 0x87, 0x7d, 0x0e, 0x22, 0x62, 0x02, 0x6e, 0x47, 0x48, 0xb7, 0x5e, 0x83, 0x62, 0x32, 0xe3,
	0x35, 0x74, 0x8c, 0x1d, 0x6c, 0xae, 0x9b, 0x17, 0x90, 0xf1, 0x64, 0xca, 0x07, 0xfb, 0xad, 0x40,
	0xb5, 0x13, 0x99, 0x9a, 0xcf, 0x60, 0x6b, 0x37, 0x14, 0x55, 0xaa, 0x6d, 0xda, 0x08, 0x5c, 0x6f,
	0x90, 0x3e, 0xf2, 0x9d, 0x5e, 0xdc, 0x15, 0x7e, 0xad, 0x66, 0x24, 0xb0, 0x0e, 0xe7, 0x06, 0x8f,
};

static const SceUInt32 g_pfsFilesDbExponent = 0x00010001;

/* a signature or hash the RSA check takes (name unknown) */
typedef struct PfsRsaBuffer {
	void *data;
	SceSize size;
} PfsRsaBuffer;

/* the modulus and the exponent of one (name unknown) */
typedef struct PfsRsaKey {
	const void *modulus;
	const void *exponent;
} PfsRsaKey;



/* checks the RSA-2048 signature over files.db's header (SHA-256 of the header,
 * SceSblPostSsMgrForDriver_3DB69911); 0 when it holds, 0x8014091b when not */
int pfsDbTableVerify(ScePfsDbTable *table)
{
	ScePfsDbHeader header;
	PfsRsaBuffer digest;
	PfsRsaBuffer signature;
	PfsRsaKey key;
	SceUInt8 hash[0x20];
	int ret;

	memcpy(&header, &table->header, sizeof(header));
	memset(header.signature, 0, sizeof(header.signature));
	digest.data = hash;
	digest.size = sizeof(hash);
	sceSha256Digest(&header, sizeof(header), hash);
	signature.data = table->header.signature;
	signature.size = sizeof(header.signature);
	key.modulus = g_pfsFilesDbModulus;
	key.exponent = &g_pfsFilesDbExponent;
	ret = SceSblPostSsMgrForDriver_3DB69911(&signature, &digest, &key, 11);
	return ret == 0 ? 0 : (int)0x8014091b;
}
