/*
 * Acid directories: a directory at the top of an AC mount that is mounted as
 * a pmi of its own ("0x<slot>ACD"), with its own klicensee, and can be
 * sealed against further writes.
 */

#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include "pfsmgr_internal.h"

#define PFS_ACID_NAME_SIZE	0x21	/* name unknown: "0x%xACD" */
#define PFS_ACID_MAX		16	/* name unknown: acid directories modes 4 and 12 allow */
#define PFS_ACID_MAX_MODE_8	3	/* name unknown */

int pfsAcidFindLocked(ScePfsPmi **acid, ScePfsPmi *pmi, SceVfsVnode *vp)
{
	char name[PFS_ACID_NAME_SIZE];
	ScePfsVd *vd = vp->core.node_data;
	int ret;

	snprintf(name, sizeof(name), "0x%xACD", vd->slot);
	ret = pfsPmiFind(&pmi->acidDirs, name, acid);
	if (ret == 0 && *acid == NULL)
		ret = SCE_PFS_ERROR_NO_ACID;
	return ret;
}

int pfsAcidFind(ScePfsPmi **acid, ScePfsPmi *pmi, SceVfsVnode *vp)
{
	ScePfsVd *vd = vp->core.node_data;
	int ret;

	if (vd->slot == -1)
		return SCE_PFS_ERROR_INVALID_ARG;
	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	ret = pfsAcidFindLocked(acid, pmi, vp);
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return ret;
}

/* make a directory at the top of the mount an acid directory */
int pfsAcidDirSet(ScePfsPmi *pmi, SceVfsVnode *vp)
{
	char name[PFS_DB_NAME_MAX];
	ScePfsNode node;
	ScePfsVd *vd;
	SceUInt64 size;
	SceSize len;
	SceUInt16 attr;
	int ret;

	if (vp->core.dd->core.type < SCE_VNODE_TYPE_ROOTDIR)
		return SCE_PFS_ERROR_INVALID_ARG;
	if ((SceUInt16)vp->core.acl_data[0] & PFS_ATTR_ACID_DIR)
		return SCE_PFS_ERROR_ACID_EXISTS;
	ret = vfsGetVnodeName(vp, name, sizeof(name), &len);
	if (ret != 0)
		return ret;
	node.vp = vp;
	node.name = name;
	ret = pfsCoreGetAttr(pmi, &node, &attr, &size);
	if (ret != 0)
		return ret;
	if (!(attr & PFS_ATTR_DIR))
		return SCE_PFS_ERROR_ACID_NOT_DIR;
	attr |= PFS_ATTR_ACID_DIR;
	node.vp = vp;
	node.name = name;
	ret = pfsCoreSetAttr(pmi, &node, attr);
	if (ret == 0) {
		vd = vp->core.node_data;
		*(SceUInt64 *)vp->core.acl_data = attr;
		vd->slot = vd->index;
		vd->index = 0;
	}
	return ret;
}

int pfsAcidDirMount(ScePfsPmi *pmi, SceVfsVnode *vp, const void *klicensee, SceSize len)
{
	char name[PFS_ACID_NAME_SIZE];
	char path[PFS_DB_NAME_MAX];
	ScePfsPmi *acid;
	ScePfsNode root;
	ScePfsVd *vd;
	SceSize pathLen;
	int count, max, ret;

	if (!((SceUInt16)vp->core.acl_data[0] & PFS_ATTR_ACID_DIR))
		return SCE_PFS_ERROR_INVALID_ARG;
	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	if (pfsAcidFindLocked(&acid, pmi, vp) == 0) {
		ret = SCE_PFS_ERROR_ACID_EXISTS;
		goto unlock;
	}
	ret = pfsPmiListCount(&pmi->acidDirs, &count);
	if (ret != 0)
		goto unlock;
	if ((pmi->mode & ~8) == 4)
		max = PFS_ACID_MAX;
	else if (pmi->mode == 8)
		max = PFS_ACID_MAX_MODE_8;
	else
		max = 0;
	if (count >= max) {
		ret = SCE_PFS_ERROR_ACID_TOO_MANY;
		goto unlock;
	}
	snprintf(name, sizeof(name), "0x%xACD", ((ScePfsVd *)vp->core.node_data)->slot);
	ret = vfsGetVnodeName(vp, path, sizeof(path), &pathLen);
	if (ret != 0)
		goto unlock;
	ret = pfsPmiCreate(&acid, path, name, klicensee, PFS_MODE_ACID);
	if (ret != 0)
		goto unlock;
	acid->parent = pmi;
	acid->openCount = 0;
	vd = vp->core.node_data;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		root.vp = vd->lowerVp;
		root.name = NULL;
		ret = pfsCoreMount(acid, &root);
		if (ret == 0) {
			acid->link.next = &pmi->acidDirs;
			acid->link.prev = pmi->acidDirs.prev;
			pmi->acidDirs.prev = &acid->link;
			acid->link.prev->next = &acid->link;
			goto unlock;
		}
	}
	pfsPmiDestroy(acid);
unlock:
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return ret;
}

int pfsAcidGetDirs(ScePfsPmi *pmi, ScePfsAcidDirs *dirs)
{
	ScePfsListNode *node;
	int ret;

	dirs->count = 0;
	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	node = pmi->acidDirs.next;
	if (node == NULL)
		goto notInList;
	while (node != &pmi->acidDirs) {
		strncpy(dirs->path[dirs->count], ((ScePfsPmi *)node)->path, sizeof(dirs->path[0]));
		dirs->path[dirs->count][sizeof(dirs->path[0]) - 1] = '\0';
		dirs->count++;
		if (dirs->count >= PFS_ACID_MAX)
			break;
		node = node->next;
		if (node == NULL)
			goto notInList;
	}
	ret = 0;
	goto unlock;
notInList:
	ret = SCE_PFS_ERROR_NOT_IN_LIST;
unlock:
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return ret;
}

int pfsAcidDirSeal(ScePfsPmi *pmi, SceVfsVnode *vp)
{
	ScePfsPmi *acid;
	int ret;

	if (!((SceUInt16)vp->core.acl_data[0] & PFS_ATTR_ACID_DIR))
		return SCE_PFS_ERROR_INVALID_ARG;
	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	ret = pfsAcidFindLocked(&acid, pmi, vp);
	if (ret == 0) {
		ksceKernelLockMutex(acid->mutex, 1, NULL);
		if (acid->mode == PFS_MODE_ACID)
			acid->mode = PFS_MODE_ACID_SEALED;
		else
			ret = SCE_PFS_ERROR_ACID_SEALED;
		ksceKernelUnlockMutex(acid->mutex, 1);
	}
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return ret;
}

int pfsAcidDirUmount(ScePfsPmi *pmi, SceVfsVnode *vp)
{
	ScePfsPmi *acid;
	int ret;

	if (!((SceUInt16)vp->core.acl_data[0] & PFS_ATTR_ACID_DIR))
		return SCE_PFS_ERROR_INVALID_ARG;
	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	ret = pfsAcidFindLocked(&acid, pmi, vp);
	if (ret == 0) {
		ksceKernelLockMutex(acid->mutex, 1, NULL);
		if (acid->openCount < 1) {
			acid->link.next->prev = acid->link.prev;
			acid->link.prev->next = acid->link.next;
			ksceKernelUnlockMutex(acid->mutex, 1);
			pfsCoreUmount(acid, 0);
			pfsPmiDestroy(acid);
		} else {
			ksceKernelUnlockMutex(acid->mutex, 1);
			ret = SCE_PFS_ERROR_ACID_BUSY;
		}
	}
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return ret;
}

/* PLACEHOLDER: cryptographic routine, deferred: checks the RSA-2048
 * signature over files.db's header (SHA-256 of the header, ksceSblRSA2048VerifySignature);
 * 0 when it holds, 0x8014091b when not */
int pfsDbTableVerify(ScePfsDbTable *table)
{
	return 0;
}
