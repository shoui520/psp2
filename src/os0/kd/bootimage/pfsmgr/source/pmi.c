/*
 * PFS mount info (pmi): the list of mounted pseudo drives and the program
 * authority IDs approved for each.
 */

#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/kernel/aimgr.h>
#include "pfsmgr_internal.h"

#define PFS_PMI_COUNT		0x5c

ScePfsBufferList g_pfsPmiBufferList;

/* the pmi mounted as "<name>0:", or NULL */
int pfsPmiFind(ScePfsListNode *list, const char *name, ScePfsPmi **pmi)
{
	char assignName[0x24];
	ScePfsListNode *node;

	snprintf(assignName, 0x21, "%s0:", name);
	for (node = list->next; node != list; node = node->next) {
		if (node == NULL)
			return SCE_PFS_ERROR_NOT_IN_LIST;
		if (strncmp(((ScePfsPmi *)node)->assignName, assignName, 0x21) == 0) {
			if (node == list)
				node = NULL;
			*pmi = (ScePfsPmi *)node;
			return 0;
		}
	}
	*pmi = NULL;
	return 0;
}

int pfsPmiListCount(ScePfsListNode *list, int *count)
{
	ScePfsListNode *node = list->next;
	int n = 0;

	if (node != list) {
		if (node == NULL)
			return SCE_PFS_ERROR_NOT_IN_LIST;
		do {
			node = node->next;
			n++;
			if (node == list)
				goto done;
		} while (node != NULL);
		return SCE_PFS_ERROR_NOT_IN_LIST;
	}
done:
	*count = n;
	return 0;
}

int pfsPmiCreate(ScePfsPmi **outPmi, const char *path, const char *name, const void *klicensee,
		 SceUInt32 mode)
{
	ScePfsPmi *pmi;
	SceSize pathLen, nameLen;
	SceUID mutex;
	int i;

	pathLen = strnlen(path, PFS_PATH_MAX);
	if (pathLen > PFS_PATH_MAX - 1 || (nameLen = strnlen(name, PFS_MOUNT_NAME_MAX)) > PFS_MOUNT_NAME_MAX - 1 ||
	    (klicensee != NULL && mode == 10) ||
	    ((mode - 2 < 2 || mode == 10 || mode == 13) && klicensee == NULL && ksceSblAimgrIsCEX()))
		return SCE_ERROR_ERRNO_EINVAL;

	pmi = pfsBufferListAlloc(&g_pfsPmiBufferList);
	if (pmi == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	mutex = ksceKernelCreateMutex("ScePfsPmi", PFS_MUTEX_ATTR, 0, NULL);
	pmi->mutex = mutex;
	if (mutex < 0) {
		pfsBufferListFree(&g_pfsPmiBufferList, pmi);
		return mutex;
	}
	mutex = ksceKernelCreateMutex("ScePfsFilesDb", PFS_MUTEX_ATTR, 0, NULL);
	pmi->filesDbMutex = mutex;
	if (mutex < 0) {
		pfsBufferListFree(&g_pfsPmiBufferList, pmi);
		ksceKernelDeleteMutex(pmi->mutex);
		return mutex;
	}

	strncpy(pmi->path, path, pathLen + 1);
	snprintf(pmi->rootPath, nameLen + 2, "/%s", name);
	snprintf(pmi->assignName, nameLen + 3, "%s0:", name);
	pmi->mode = mode;
	pmi->flags = 0;
	if (klicensee == NULL) {
		memset(pmi->klicensee, 0, sizeof(pmi->klicensee));
		pmi->flags |= PFS_PMI_NO_KLICENSEE;
	} else {
		memcpy(pmi->klicensee, klicensee, sizeof(pmi->klicensee));
	}
	if (!ksceSblACMgrIsDevelopmentMode())
		pmi->flags |= PFS_PMI_NOT_DEVELOPMENT;

	if (mode == 3) {
		pmi->a6 = 0;
		pmi->a8 = 0;
		pmi->quota = 0;
		pmi->flags |= PFS_PMI_MODE_3;
	} else {
		if ((mode == 10 && pmi->flags == PFS_PMI_NO_KLICENSEE) || mode == 7 || mode == 9)
			pmi->flags |= PFS_PMI_MODE_7_9_10;
		pmi->a6 = 0;
		pmi->a8 = 0;
		pmi->quota = 0;
		if ((mode & ~8) == 4 || mode == 8) {
			pmi->acidDirs.next = &pmi->acidDirs;
			pmi->acidDirs.prev = &pmi->acidDirs;
		}
	}
	if (mode - 0x1f > 2) {
		pmi->mountData.assign_name = pmi->assignName;
		pmi->approvedMax = PFS_APPROVED_MAX;
		pmi->mountData.blockdev_name = NULL;
		pmi->mountData.blockdev_name_no_part = NULL;
		for (i = 0; i < PFS_APPROVED_MAX; i++)
			pmi->approved[i] = -1;
	}
	*outPmi = pmi;
	return 0;
}

int pfsPmiDestroy(ScePfsPmi *pmi)
{
	ksceKernelDeleteMutex(pmi->filesDbMutex);
	ksceKernelDeleteMutex(pmi->mutex);
	pfsBufferListFree(&g_pfsPmiBufferList, pmi);
	return 0;
}

/* where authId is, or the first free slot; approved[approvedMax] when neither */
static inline int pfsPmiApprovedIndex(ScePfsPmi *pmi, SceUInt64 authId)	/* name unknown */
{
	int i;

	for (i = 0; i < pmi->approvedMax; i++)
		if (pmi->approved[i] == authId || pmi->approved[i] == (SceUInt64)-1)
			break;
	return i;
}

int pfsPmiApprove(ScePfsPmi *pmi, SceUInt64 authId)
{
	int i, ret;

	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	if (pmi->approved[pfsPmiApprovedIndex(pmi, authId)] == authId) {
		ret = SCE_ERROR_ERRNO_EEXIST;
	} else {
		for (i = 0; i < pmi->approvedMax; i++)
			if (pmi->approved[i] == (SceUInt64)-1)
				break;
		if (i == pmi->approvedMax) {
			ret = SCE_ERROR_ERRNO_ENOSPC;
		} else {
			pmi->approved[i] = authId;
			ret = 0;
		}
	}
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return ret;
}

int pfsPmiDisapprove(ScePfsPmi *pmi, SceUInt64 authId)
{
	int i, ret;

	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	if (pmi->approved[pfsPmiApprovedIndex(pmi, authId)] != authId) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		for (i = 0; i < pmi->approvedMax; i++)
			if (pmi->approved[i] == authId)
				break;
		/* the later IDs move down one */
		for (i++; i < pmi->approvedMax; i++) {
			pmi->approved[i - 1] = pmi->approved[i];
			if (pmi->approved[i] == (SceUInt64)-1)
				break;
			pmi->approved[i] = -1;
		}
		ret = 0;
	}
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return ret;
}

int pfsPmiIsApproved(ScePfsPmi *pmi, SceUInt64 authId)
{
	int found;

	ksceKernelLockMutex(pmi->mutex, 1, NULL);
	found = pmi->approved[pfsPmiApprovedIndex(pmi, authId)] == authId;
	ksceKernelUnlockMutex(pmi->mutex, 1);
	return found;
}

int pfsPmiInit(void)
{
	return pfsBufferListInit(&g_pfsPmiBufferList, "ScePfsPmiBufferList", sizeof(ScePfsPmi), 4,
				 PFS_PMI_COUNT);
}

void pfsPmiTerm(void)
{
	pfsBufferListTerm(&g_pfsPmiBufferList);
}
