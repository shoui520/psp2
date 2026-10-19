/*
 * files.db: the entry of every file and directory of a pmi (its index,
 * attributes, size and icv hash) and the disk space they take. Each access
 * opens the table again and checks its header against the hmac the pmi
 * remembered.
 */

#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
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
	SceUID pid;
	int ret;

	info = pfsModeInfo(pmi->mode);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	ret = pfsNodeCreate(node, info->fileMode);
	if (ret != 0)
		goto unlock;
	pid = SceThreadmgrForDriver_0486F239(SCE_KERNEL_PROCESS_ID);
	ret = pfsLowerOpen(&pmi->filesDb, node, -1, info->icvOpenFlags | PFS_OPEN_KERNEL);
	SceThreadmgrForDriver_0486F239(pid);
	if (ret == 0) {
		pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
		fsType = pfsModeFsType(pmi->mode);
		pfsDbTableInit(&table, &pmi->filesDbLru);
		ret = pfsDbTableCreate(&table, &pmi->filesDb, &key, fsType, pmi->a6,
				       pfsFsTypeIsX(fsType) ? PFS_DB_TYPE_X : PFS_DB_TYPE, pmi->a8);
		if (ret == 0) {
			pmi->version = table.header.type;
			ret = pfsDbTableFlush(&table);
			pfsHashCopy(&pmi->filesDbHash, &table.header.hmac);
			pfsDbTableTerm(&table);
			goto unlock;
		}
		pfsDbTableFlush(&table);
		pfsDbTableTerm(&table);
		pfsLowerClose(&pmi->filesDb);
	}
	dir.vp = pmi->sceDirVp;
	dir.name = NULL;
	pfsNodeRemove(&dir, node);
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
	ret = pfsLowerOpen(&pmi->filesDb, node, -1, info->icvOpenFlags | PFS_OPEN_KERNEL);
	SceThreadmgrForDriver_0486F239(pid);
	if (ret != 0)
		goto unlock;
	ret = pfsDbTableReadInfo(&pmi->filesDb, &type, &pmi->a8, &pmi->a6);
	if (ret != 0)
		goto close;

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &key);
	if (ret != 0)
		goto term;
	pfsHashCopy(&pmi->filesDbHash, &table.header.hmac);
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
	pfsLowerClose(&pmi->filesDb);
unlock:
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}

int pfsDbGetUsage(ScePfsPmi *pmi, SceUInt64 *usage)
{
	ScePfsDbTable table;
	ScePfsHash key;
	int ret;

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &key);
	if (ret == 0) {
		if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac))
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

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &key);
	if (ret == 0) {
		if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			usage = table.header.usage + delta;
			if (usage != table.header.usage) {
				table.dirty = 1;
				table.header.usage = usage;
			}
			ret = pfsDbTableFlush(&table);
			if (ret == 0)
				pfsHashCopy(&pmi->filesDbHash, &table.header.hmac);
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

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &key);
	if (ret == 0) {
		if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac))
			ret = SCE_PFS_ERROR_HASH;
		else
			ret = pfsDbTableGetSize(&table, size);
	}
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return ret;
}

/* the files.db index of every entry, as many as fit; returns how many */
int pfsDbDump(ScePfsPmi *pmi, void *buf, SceSize len)
{
	ScePfsDbTable table;
	char name[PFS_DB_NAME_MAX];
	ScePfsHash key;
	SceUInt32 parentIndex, index;
	SceSize off;
	int ret, count;

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	memset(name, 0, sizeof(name));
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &key);
	if (ret != 0)
		goto out;
	if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac)) {
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

	pfsDeriveKey(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			ret = pfsDbTableInsert(&table, key, hash, index, attr, size);
			if (ret == 0 && (ret = pfsDbTableFlush(&table)) == 0)
				pfsHashCopy(&pmi->filesDbHash, &table.header.hmac);
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

	pfsDeriveKey(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac)) {
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

	pfsDeriveKey(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac)) {
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
					pfsHashCopy(&pmi->filesDbHash, &table.header.hmac);
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

	pfsDeriveKey(&dbKey, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &dbKey);
	if (ret == 0) {
		if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac)) {
			ret = SCE_PFS_ERROR_HASH;
		} else {
			ret = pfsDbTableRemove(&table, key);
			if (ret == 0 && (ret = pfsDbTableFlush(&table)) == 0)
				pfsHashCopy(&pmi->filesDbHash, &table.header.hmac);
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
		return pfsLowerCloseForce(lf);
	return pfsLowerClose(lf);
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

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, 0, pmi->a6);
	sceKernelLockMutex(pmi->filesDbMutex, 1, NULL);
	pfsDbTableInit(&table, &pmi->filesDbLru);
	ret = pfsDbTableOpen(&table, &pmi->filesDb, &key);
	err = ret;
	if (ret != 0)
		goto out;
	if (!pfsHashEqual(&pmi->filesDbHash, &table.header.hmac)) {
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
				pfsHashCopy(&pmi->filesDbHash, &table.header.hmac);
		}
	}
out:
	pfsDbTableTerm(&table);
	sceKernelUnlockMutex(pmi->filesDbMutex, 1);
	return err;
}
