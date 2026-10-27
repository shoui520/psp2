/*
 * SceDeci4pRDrfp file descriptors: the numbers the host gets for the files
 * and directories it opens (Sony's file name unknown).
 */
#include <sysmem/cpu.h>
#include <string.h>
#include "rdrfp.h"

static int rdrfpFdCounter;
static RdrfpList rdrfpFdList;

static int compareFd(void *data, const void *fd)
{
	return ((RdrfpFd *)data)->fd == (SceUInt32)fd ? 0 : -1;
}

void rdrfpInitFdList(void)
{
	rdrfpInitList(&rdrfpFdList);
}

RdrfpFd *rdrfpCreateFd(RdrfpVDEntry *entry, SceUID uid)
{
	RdrfpFd *f = rdrfpAlloc(sizeof(*f));

	if (f) {
		memset(f, 0, sizeof(*f));
		f->fd = sceKernelAtomicGetAndAdd32(&rdrfpFdCounter, 1);
		f->uid = uid;
		f->entry = entry;
		rdrfpAddToList(&rdrfpFdList, f);
	}
	return f;
}

void rdrfpDeleteFd(SceUInt32 fd)
{
	RdrfpListElem *elem = rdrfpFindListElem(compareFd, &rdrfpFdList, (const void *)fd);
	RdrfpFd *f;

	if (elem) {
		f = elem->data;
		rdrfpRemoveFromList(&rdrfpFdList, elem);
		rdrfpFree(f);
	}
}

RdrfpFd *rdrfpFindFd(SceUInt32 fd)
{
	RdrfpListElem *elem = rdrfpFindListElem(compareFd, &rdrfpFdList, (const void *)fd);

	return elem ? elem->data : NULL;
}

void rdrfpSetFdFlags(RdrfpFd *f, SceUInt32 flags)
{
	f->flags |= flags;
}

int rdrfpFdIsRootDir(RdrfpFd *f)
{
	return f->flags & RDRFP_FD_ROOT_DIR;
}

int rdrfpFdIsElevated(RdrfpFd *f)
{
	return f->flags & RDRFP_FD_ELEVATED;
}

int rdrfpFdIsSceSysDir(RdrfpFd *f)
{
	return f->flags & RDRFP_FD_SCE_SYS_DIR;
}
