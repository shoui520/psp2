/*
 * thread.c: per-thread tracking list, thread exit handler, and raw frame mbuf builder
 * for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

static int g_adhocThreadListInit = -1;
static SceUID g_adhocThreadEventHandler = -1;

typedef struct AdhocThreadList {
	AdhocThreadInfo *head;
	AdhocThreadInfo **tail;
} AdhocThreadList;

static SceKernelLwMutexWork g_adhocThreadListMutex;
static AdhocThreadList g_adhocThreadList;

__attribute__((noinline))
int adhocThreadCleanup(SceUID threadId)
{
	AdhocThreadInfo *ti;
	int ret = 0;

	sceKernelLockLwMutex(&g_adhocThreadListMutex, 1, NULL);
	for (ti = g_adhocThreadList.head; ti != NULL; ti = ti->next) {
		if (ti->threadId == threadId)
			break;
	}
	if (ti != NULL) {
		if (ti->refCount <= 0) {
			if (ti->next != NULL)
				ti->next->prev = ti->prev;
			else
				g_adhocThreadList.tail = ti->prev;
			*ti->prev = ti->next;
			adhocCalloutNoop();
			sceKernelDeleteSema(ti->sema);
			adhocFree(ti);
		} else {
			ret = SCE_ERROR_NET_ADHOC_BUSY;
		}
	} else {
		ret = SCE_ERROR_NET_ADHOC_NO_ENTRY;
	}
	sceKernelUnlockLwMutex(&g_adhocThreadListMutex, 1);
	return ret;
}

__attribute__((noinline))
int adhocThreadModuleStart(void)
{
	return 0;
}

__attribute__((noinline))
int adhocThreadModuleStop(void)
{
	int ret = 0;

	if (g_adhocThreadEventHandler != -1)
		ret = sceKernelUnregisterThreadEventHandler(g_adhocThreadEventHandler);
	g_adhocThreadEventHandler = -1;
	return ret;
}

__attribute__((noinline))
int adhocThreadInit(void)
{
	int ret;
	SceUID handler;

	g_adhocThreadList.head = NULL;
	g_adhocThreadList.tail = &g_adhocThreadList.head;
	ret = sceKernelCreateLwMutex(&g_adhocThreadListMutex, "SceNetThreadList",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				     0, NULL);
	if (ret < 0)
		return ret;
	g_adhocThreadListInit = 0;
	handler = sceKernelRegisterThreadEventHandler("SceNetPspnetAdhoc",
						      0x10027, 8,
						      adhocThreadEventHandler, NULL);
	if (handler < 0) {
		sceKernelDeleteLwMutex(&g_adhocThreadListMutex);
		g_adhocThreadListInit = -1;
		return handler;
	}
	g_adhocThreadEventHandler = handler;
	return 0;
}

__attribute__((noinline))
AdhocThreadInfo *adhocThreadFind(SceUID threadId)
{
	AdhocThreadInfo *ti;

	sceKernelLockLwMutex(&g_adhocThreadListMutex, 1, NULL);
	for (ti = g_adhocThreadList.head; ti != NULL; ti = ti->next) {
		if (ti->threadId == threadId)
			break;
	}
	sceKernelUnlockLwMutex(&g_adhocThreadListMutex, 1);
	return ti;
}

__attribute__((noinline))
AdhocThreadInfo *adhocThreadEnter(int *errOut)
{
	SceUID threadId;
	AdhocThreadInfo *ti;
	int ret;
	SceUID sema;

	threadId = sceKernelGetThreadId();
	if (threadId < 0) {
		*errOut = threadId;
		return NULL;
	}
	ret = sceKernelLockLwMutex(&g_adhocThreadListMutex, 1, NULL);
	if (ret < 0) {
		*errOut = ret;
		return NULL;
	}
	for (ti = g_adhocThreadList.head; ti != NULL; ti = ti->next) {
		if (ti->threadId == threadId)
			break;
	}
	if (ti == NULL) {
		ti = (AdhocThreadInfo *)adhocMalloc(sizeof(*ti));
		if (ti == NULL) {
			*errOut = SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
			sceKernelUnlockLwMutex(&g_adhocThreadListMutex, 1);
			return NULL;
		}
		sceClibMemset(ti, 0, sizeof(*ti));
		ti->threadId = threadId;
		ti->next = NULL;
		ti->prev = g_adhocThreadList.tail;
		*g_adhocThreadList.tail = ti;
		g_adhocThreadList.tail = &ti->next;
		sema = sceKernelCreateSema("SceNetThreadInfo", 0, 0, 1, NULL);
		ti->sema = sema;
		if (sema < 0) {
			ti->lastError = 46;
			*errOut = sema;
			sceKernelUnlockLwMutex(&g_adhocThreadListMutex, 1);
			return NULL;
		}
	}
	ti->lastError = 0;
	ti->refCount++;
	*errOut = 0;
	sceKernelUnlockLwMutex(&g_adhocThreadListMutex, 1);
	return ti;
}

__attribute__((noinline))
int adhocThreadLeave(AdhocThreadInfo *ti, int err)
{
	sceKernelLockLwMutex(&g_adhocThreadListMutex, 1, NULL);
	ti->refCount--;
	ti->lastError = err;
	sceKernelUnlockLwMutex(&g_adhocThreadListMutex, 1);
	if (err != 0)
		return -1;
	return 0;
}

__attribute__((noinline))
void adhocThreadTerm(void)
{
	AdhocThreadInfo *ti;

	sceKernelLockLwMutex(&g_adhocThreadListMutex, 1, NULL);
	if (g_adhocThreadListInit >= 0) {
		for (ti = g_adhocThreadList.head; ti != NULL; ti = ti->next)
			sceKernelDeleteSema(ti->sema);
		sceKernelDeleteLwMutex(&g_adhocThreadListMutex);
		if (g_adhocThreadEventHandler != -1)
			sceKernelUnregisterThreadEventHandler(g_adhocThreadEventHandler);
		g_adhocThreadEventHandler = -1;
	}
	g_adhocThreadListInit = -1;
}

__attribute__((noinline))
SceInt32 adhocThreadEventHandler(SceInt32 evId, SceUID thId, SceInt32 evOpt, void *common)
{
	(void)evId;
	(void)evOpt;
	(void)common;
	adhocThreadCleanup(thId);
	return 0;
}

__attribute__((noinline))
AdhocMbuf *adhocMbufDevGet(const SceUChar8 *buf, SceSize totlen)
{
	int align = 2;
	AdhocMbuf *m;
	char *ext;

	m = (AdhocMbuf *)adhocMalloc(PSPNET_ADHOC_MSIZE);
	if (m == NULL)
		return NULL;
	m->next = NULL;
	m->nextPkt = NULL;
	m->data = m->storage.mh.body.pktDat;
	m->type = PSPNET_ADHOC_MT_DATA;
	m->flags = PSPNET_ADHOC_M_PKTHDR;
	m->storage.mh.pktHdr.aux = NULL;
	m->storage.mh.pktHdr.len = (int)totlen;
	if (totlen >= 14 && buf[12] == 0x88 && buf[13] == 0xc8)
		align = 0;
	if (totlen > (SceSize)(PSPNET_ADHOC_MHLEN - align)) {
		ext = (char *)adhocMalloc(PSPNET_ADHOC_MCLBYTES);
		m->storage.mh.body.ext.extBuf = ext;
		if (ext == NULL) {
			ext = (char *)adhocMalloc(PSPNET_ADHOC_MCLBYTES);
			m->storage.mh.body.ext.extBuf = ext;
		}
		if (ext != NULL) {
			m->data = ext;
			m->flags |= (PSPNET_ADHOC_M_EXT | PSPNET_ADHOC_M_CLUSTER);
			m->storage.mh.body.ext.extFree = NULL;
			m->storage.mh.body.ext.extArg = NULL;
			m->storage.mh.body.ext.extSize = PSPNET_ADHOC_MCLBYTES;
			m->storage.mh.body.ext.extNextRef = m;
			m->storage.mh.body.ext.extPrevRef = m;
		}
		if ((m->flags & PSPNET_ADHOC_M_EXT) == 0) {
			adhocMbufFree(m);
			return NULL;
		}
	}
	m->len = (int)totlen;
	m->data += align;
	sceClibMemcpy_safe(m->data, buf, totlen);
	return m;
}
