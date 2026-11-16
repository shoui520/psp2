/*
 * op.cpp: an op's life (allocation, submission, completion and the waits on it), the overlay
 * copies, and the library's paths: FiosPath and the normalizer behind sceFiosPathNormalize.
 */
#include <string.h>
#include <alloca.h>
#include <sce_atomic.h>
#include "fios2_internal.h"

/* the TLS slot holding the op whose callback the thread runs */
#define FIOS_TLS_CALLBACK_OP	77

/* the op flags an op may have: the SDK's and FIOS2's own */
#define FIOS_OPFLAGS_ALL	0x3ff

/* the overlay orders a process may resolve through: the user's, and all of them for the system */
#define OVERLAY_ORDER_USER_MAX	127
#define OVERLAY_ORDER_MAX	255

/* how sceFiosPathNormalize reads a path */
enum PathStyle {
	PATH_STYLE_DEFAULT,	/* PATH_STYLE_PSP2 */
	PATH_STYLE_WINDOWS,	/* a drive letter, '\\' between components, case folded */
	PATH_STYLE_PSP2		/* a device of up to 16 letters and digits, '/' between components */
};

Wws::Platform::Mutex s_opWaitMutex;
Wws::Platform::Cond s_opWaitCond;
static int s_callbackOpWarned;		/* the TLS slot's warning was printed */
bool s_pathWarnings = true;		/* FiosPath prints why a path does not fit */
int s_overlayCount;			/* the process's overlays at start */
static size_t s_longestPath;		/* the longest path FiosPath::Set normalized */

/* an op's handle, when it owns it, is released when it fails */
static inline void releaseHandle(FiosOp *pOp)
{
	if (pOp->internalFlags & FIOS_OP_OWNS_HANDLE) {
		SceFiosHandle h = pOp->fh;

		if (s_pFHTable->Lookup(h) != NULL)
			s_pFHTable->Free(h);
		else if (s_pDHTable->Lookup(h) != NULL)
			s_pDHTable->Free(h);
	}
}

FiosOp *fiosOpAllocate(const SceFiosOpAttr *pAttr, uint16_t what)
{
	SceFiosOp op;
	FiosOp *pOp;

	if (!s_libraryInitialized)
		return NULL;
	op = s_pOpTable->Allocate();
	if (op == 0)
		return NULL;
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pAttr != NULL)
		pOp->attr = *pAttr;
	else
		sceFiosGetDefaultOpAttr(&pOp->attr);
	pOp->what = what;
	return pOp;
}

/* the thread's TLS slot holding the op whose callback it runs */
SceFiosOp *fiosGetCallbackOp()
{
	SceFiosOp *pOp = (SceFiosOp *)sceKernelGetTLSAddr(FIOS_TLS_CALLBACK_OP);

	if (pOp == NULL) {
		if (!s_callbackOpWarned) {
			sceFiosPrintf("FIOS2 WARNING: Could not retrieve thread callback op address.\n");
			s_callbackOpWarned = 1;
		}
	}
	return pOp;
}

/* the caller's callback for an event, then the op is done and its waiters wake */
SceFiosOp fiosOpCallback(FiosOp *pOp, SceFiosOpEvent event)
{
	SceFiosOp op = pOp->h;
	int err = pOp->err;
	FiosOp *pDone;

	(void)pOp->waiters;
	FIOS_TRACE(SCE_FIOS_PROFILE_OP, "op", "%-15s %s %s", "Callback",
		   event == SCE_FIOS_OPEVENT_COMPLETE ? "COMPLETE" : "DELETE",
		   sceFiosDebugDumpOp(op, NULL, 0));
	if (pOp->attr.pCallback != NULL) {
		*fiosGetCallbackOp() = op;
		pOp->attr.pCallback(pOp->attr.pCallbackContext, op, event, err);
		*fiosGetCallbackOp() = 0;
	}
	pDone = (FiosOp *)s_pOpTable->Lookup(op);
	if (pDone != NULL)
		sceAtomicExchange32Release((volatile int32_t *)&pDone->done, 1);
	s_opWaitMutex.Lock();
	s_opWaitCond.Broadcast();
	s_opWaitMutex.Unlock();
	return op;
}

SceFiosOp fiosOpCompleteWithError(FiosOp *pOp, int err)
{
	pOp->err = err;
	releaseHandle(pOp);
	return fiosOpCallback(pOp, SCE_FIOS_OPEVENT_COMPLETE);
}

SceFiosOp fiosOpComplete(FiosOp *pOp)
{
	pOp->err = SCE_OK;
	return fiosOpCallback(pOp, SCE_FIOS_OPEVENT_COMPLETE);
}

/* chunk the op and schedule its tree; an op that needs no chunks completes at once */
SceFiosOp fiosOpSubmit(FiosOp *pOp)
{
	bool done;
	SceFiosOp op;
	uint16_t root;

	FIOS_TRACE(SCE_FIOS_PROFILE_OP, "op", "%-15s %s", "Accept",
		   sceFiosDebugDumpOp(pOp->h, NULL, 0));
	done = false;
	pOp->done = 0;
	if (pOp->attr.opflags & ~FIOS_OPFLAGS_ALL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_FLAGS);
	op = pOp->h;
	pOp->err = SCE_OK;
	root = Chunker::ChunkOp(pOp, &done);
	pOp->rootChunk = root;
	if (root == CHUNKID_INVALID) {
		if (done)
			return op;
		if (pOp->err != SCE_OK)
			return fiosOpCompleteWithError(pOp, pOp->err);
		return fiosOpComplete(pOp);
	}
	fiosScheduleTree(root, pOp->attr.opflags, (pOp->attr.opflags & FIOS_OPFLAG_SYNC) != 0);
	return op;
}

/*
 * Submit a path op: the path normalized into the op, then resolved through the process's
 * overlays (a rename's or mount's second path too).
 */
SceFiosOp fiosOpSubmitPath(FiosOp *pOp, const char *pPath, bool resolve)
{
	size_t pathMax = g_FiosParams.pathMax;
	uint16_t what;

	if (pPath == NULL || *pPath == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PATH);
	if (!pOp->path.Set(pPath, false))
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_PATH_TOO_LONG);
	what = pOp->what;
	if (what == OP_RENAME || what == OP_ARCHIVE_MOUNT) {
		const char *pPath2 = (const char *)pOp->pPath2;
		size_t length = fiosStrlen(pPath2);
		char *pNormalized;

		if (length == 0)
			return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PATH);
		pNormalized = (char *)alloca(length + 2 < pathMax ? length + 2 : pathMax);
		if (fiosPathNormalize(pNormalized, pPath2, pathMax) >= pathMax)
			return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_PATH_TOO_LONG);
		pOp->pPath2 = pNormalized;
		what = pOp->what;
	}
	if (what != OP_DH_OPEN && s_overlayCount > 0) {
		char *pResolved = (char *)alloca(pathMax);
		int err;

		err = fiosPlatformOverlayResolve(sceKernelGetProcessId(), resolve,
						 fiosPathGet(&pOp->path), pResolved, pathMax, 0,
						 OVERLAY_ORDER_USER_MAX);
		if (err != SCE_OK)
			return fiosOpCompleteWithError(pOp, err);
		pOp->path.Set(pResolved, false);
		if (pOp->what == OP_RENAME || pOp->what == OP_ARCHIVE_MOUNT) {
			const char *pPath2 = (const char *)pOp->pPath2;
			char *pResolved2 = (char *)alloca(pathMax);

			err = fiosPlatformOverlayResolve(sceKernelGetProcessId(),
							 pOp->what == OP_RENAME, pPath2, pResolved2,
							 pathMax, 0, OVERLAY_ORDER_USER_MAX);
			if (err != SCE_OK)
				return fiosOpCompleteWithError(pOp, err);
			pOp->pPath2 = pResolved2;
		}
	}
	return fiosOpSubmit(pOp);
}

/* the op is done, or the thread runs its callback (it would wait on itself) */
static inline bool opFinished(FiosOp *pOp)
{
	SceFiosOp op = pOp->h;

	return pOp->done || op == *fiosGetCallbackOp();
}

int fiosOpWait(FiosOp *pOp)
{
	bool blocked;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_OP, "op", "%-15s blocking on %s", "Wait",
		   sceFiosDebugDumpOp(pOp->h, NULL, 0));
	s_opWaitMutex.Lock();
	sceAtomicIncrement32Relaxed(&pOp->waiters);
	for (blocked = false; !opFinished(pOp); blocked = true)
		s_opWaitCond.Wait();
	sceAtomicDecrement32Relaxed(&pOp->waiters);
	sceAtomicMemoryBarrier();
	err = pOp->err;
	FIOS_TRACE(SCE_FIOS_PROFILE_OP, "op", "%-15s finished %sfor %s", "Wait",
		   blocked ? "" : "without blocking ", sceFiosDebugDumpOp(pOp->h, NULL, 0));
	s_opWaitMutex.Unlock();
	return err;
}

int fiosOpWaitUntil(FiosOp *pOp, SceFiosTime deadline)
{
	bool blocked;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_OP, "op", "%-15s blocking with deadline %lld on %s",
		   "WaitUntil", deadline, sceFiosDebugDumpOp(pOp->h, NULL, 0));
	s_opWaitMutex.Lock();
	sceAtomicIncrement32Relaxed(&pOp->waiters);
	for (blocked = false; !opFinished(pOp) && deadline > sceKernelGetProcessTimeWide();
	     blocked = true)
		s_opWaitCond.WaitUntil(deadline);
	sceAtomicDecrement32Relaxed(&pOp->waiters);
	if (opFinished(pOp)) {
		sceAtomicMemoryBarrier();
		err = pOp->err;
	} else {
		err = SCE_FIOS_ERROR_TIMEOUT;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_OP, "op", "%-15s %s %sfor %s", "WaitUntil",
		   blocked ? "" : "without blocking ", opFinished(pOp) ? "timed out" : "finished",
		   sceFiosDebugDumpOp(pOp->h, NULL, 0));
	s_opWaitMutex.Unlock();
	return err;
}

/* executeRoot's completions: it has released the op's handle itself */
SceFiosOp fiosOpFinishWithError(FiosOp *pOp, int err)
{
	pOp->err = err;
	return fiosOpCallback(pOp, SCE_FIOS_OPEVENT_COMPLETE);
}

SceFiosOp fiosOpFinish(FiosOp *pOp)
{
	pOp->err = SCE_OK;
	return fiosOpCallback(pOp, SCE_FIOS_OPEVENT_COMPLETE);
}

/* a point's length, SCE_FIOS_OVERLAY_POINT_MAX when it does not end in it */
static inline size_t pointLength(const char *pPoint)
{
	size_t length = 0;

	while (*pPoint != 0) {
		length++;
		pPoint++;
		if (length >= SCE_FIOS_OVERLAY_POINT_MAX) {
			length = SCE_FIOS_OVERLAY_POINT_MAX;
			break;
		}
	}
	return length;
}

/* the caller's overlay as the system keeps it, for this process */
int fiosOverlayCopy(const SceFiosOverlay *pOverlay, SceFiosOverlay *pOut)
{
	FiosProcessOverlay *pCopy = (FiosProcessOverlay *)pOut;
	size_t dstLength, srcLength;

	if (pOverlay == NULL || pOut == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (pOverlay->type > SCE_FIOS_OVERLAY_TYPE_WRITABLE ||
	    pOverlay->order > OVERLAY_ORDER_USER_MAX)
		return SCE_FIOS_ERROR_BAD_OVERLAY;
	dstLength = pointLength(pOverlay->dst);
	srcLength = pointLength(pOverlay->src);
	if (dstLength >= SCE_FIOS_OVERLAY_POINT_MAX || srcLength >= SCE_FIOS_OVERLAY_POINT_MAX)
		return SCE_FIOS_ERROR_PATH_TOO_LONG;
	sceClibMemset(pCopy, 0, sizeof(*pCopy));
	pCopy->type = pOverlay->type;
	pCopy->order = pOverlay->order;
	pCopy->dstLength = dstLength;
	pCopy->srcLength = srcLength;
	pCopy->id = 0;
	pCopy->pid = sceKernelGetProcessId();
	sceClibMemcpy(pCopy->dst, pOverlay->dst, dstLength + 1);
	sceClibMemcpy(pCopy->src, pOverlay->src, srcLength + 1);
	return SCE_OK;
}

/* remove a process's overlays (pid -1: every order's) */
void fiosOverlaysTerm(SceUID pid)
{
	uint8_t maxOrder = pid == -1 ? OVERLAY_ORDER_MAX : OVERLAY_ORDER_USER_MAX;
	SceFiosOverlayID id;
	SceSize count = 0;

	while (fiosPlatformOverlayGetList(pid, 0, maxOrder, &id, 1, &count) == SCE_OK &&
	       count != 0)
		fiosPlatformOverlayRemove(pid, id);
}

/* the cache's digest of a buffer */
uint8_t *fiosHash(const void *pData, int size, uint8_t *pDigest)
{
	return fiosMD5(pData, size, pDigest);
}

/* a string's length, counted here */
static inline size_t stringLength(const char *s)
{
	const char *p = s;

	while (*p != 0)
		p++;
	return p - s;
}

/* a path this platform never allocates has nothing to free */
void FiosPath::Free()
{
}

void FiosPath::Clear()
{
	path[g_FiosParams.pathMax - 1] = 0;
	path[0] = 0;
	memset(hash, 0, sizeof(hash));
}

/* why a path does not fit */
void fiosPathPrintError(const char *pPath)
{
	size_t length = fiosStrlen(pPath);

	if (length <= g_FiosParams.pathMax)
		sceFiosPrintf("Invalid path: %s\n", pPath);
	else
		sceFiosPrintf("Path too long (length %d, pathMax %d): %s\n", length,
			      g_FiosParams.pathMax, pPath);
}

__attribute__((noinline))
size_t fiosPathNormalize(char *pOut, const char *pPath, size_t size)
{
	return sceFiosPathNormalize(pOut, pPath, size);
}

bool FiosPath::Set(const char *pPath, bool verbose)
{
	size_t pathMax = g_FiosParams.pathMax;
	size_t length;

	Clear();
	length = sceFiosPathNormalize(path, pPath, pathMax);
	if (length != 0 && length < pathMax) {
		if (s_longestPath < length)
			s_longestPath = length;
		fiosMD5(path, stringLength(path), (uint8_t *)hash);
		return true;
	}
	path[g_FiosParams.pathMax - 1] = 0;
	if (verbose)
		fiosPathPrintError(pPath);
	return false;
}

bool FiosPath::SetPointer(const char *pPath)
{
	size_t pathMax = g_FiosParams.pathMax;

	Clear();
	if (pathMax <= sizeof(char *)) {
		if (s_pathWarnings)
			sceFiosPrintf("Can't use pointer-type paths because g_FiosParams.pathMax (%d) <= ptr size (%d)\n",
				      pathMax, sizeof(char *));
		return false;
	}
	path[pathMax - 1] = 1;
	*(const char **)path = pPath;
	fiosMD5(pPath, stringLength(pPath), (uint8_t *)hash);
	return true;
}

/* this platform keeps no allocated paths */
bool FiosPath::IsAllocated() const
{
	return false;
}

/* an allocated copy of the path: this platform has no allocator for it */
bool FiosPath::SetAllocated(const char *pPath)
{
	size_t pathMax = g_FiosParams.pathMax;

	Clear();
	if (pathMax <= sizeof(char *)) {
		if (s_pathWarnings)
			sceFiosPrintf("Can't use pointer-type paths because g_FiosParams.pathMax (%d) <= ptr size (%d)\n",
				      pathMax, sizeof(char *));
	} else {
		size_t size = fiosStrlen(pPath) + 1;

		if (s_pathWarnings)
			sceFiosPrintf("Failed to allocate %d bytes for path: %s!\n", size, pPath);
	}
	return false;
}

static inline bool isSeparator(char c)
{
	return c == '/' || c == '\\';
}

static inline bool isLetter(char c)
{
	return (c | 0x20) >= 'a' && (c | 0x20) <= 'z';
}

static inline bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

/*
 * Normalize a path: its device (or drive) as it is, one separator between components, "."
 * components dropped and ".." ones removing the component before them. A path that does not
 * fit in size bytes leaves pOut empty and returns size + 1.
 */
size_t fiosPathNormalizeStyle(char *pOut, const char *pPath, size_t size, int style)
{
	char *pEnd = pOut + size;
	char *pDst = pOut;
	char *pBase = pOut;		/* what a ".." cannot remove */
	char *pRoot = pOut;		/* the first byte after the device */
	const char *pSrc = pPath;
	int deviceMax = 0;
	bool hasDevice = false;
	bool absolute = true;
	bool allowRelative = false;	/* a path without a leading separator is relative */
	bool relative = false;		/* no separator at the root */
	bool isHost = false;
	int caseMask = -1;
	char sep = '/';
	char prev, c;

	switch (style) {
	case PATH_STYLE_DEFAULT:
		style = PATH_STYLE_PSP2;
		/* fall through */
	case PATH_STYLE_PSP2:
		deviceMax = 16;
		break;
	case PATH_STYLE_WINDOWS:
		deviceMax = 1;
		allowRelative = true;
		sep = '\\';
		caseMask = ~0x20;
		break;
	}
	if (pPath == NULL || *pPath == 0)
		goto fail;
	if (deviceMax > 0) {
		const char *p = pPath;
		int length = 0;
		int n;

		for (;;) {
			c = *p;
			if (isSeparator(c) || c == 0)
				break;
			if (length > 0 && c == ':') {
				hasDevice = true;
				break;
			}
			if (style == PATH_STYLE_WINDOWS && !isLetter(c))
				break;
			if (style == PATH_STYLE_PSP2 && !isLetter(c) && !isDigit(c))
				break;
			length++;
			p++;
			if (length > deviceMax)
				break;
		}
		if (style == PATH_STYLE_PSP2 && hasDevice && length == 5 && pPath[0] == 'h' &&
		    pPath[1] == 'o' && pPath[2] == 's' && pPath[3] == 't') {
			allowRelative = true;
			isHost = true;
			relative = true;
		}
		if (hasDevice) {
			for (n = 0; n <= length; n++) {
				c = *pSrc++;
				if (c != ':')
					c &= caseMask;
				*pDst++ = c;
				if (pDst >= pEnd)
					goto fail;
			}
			pBase = pDst;
		}
		if (isHost) {
			if (style == PATH_STYLE_PSP2 && pSrc[0] == '\\' && pSrc[1] == '\\') {
				*pDst++ = '\\';
				if (pDst >= pEnd)
					goto fail;
				*pDst++ = pSrc[1];
				pSrc += 2;
				pBase = pDst;
				if (pDst >= pEnd)
					goto fail;
			}
			if (isSeparator(*pSrc))
				while (isSeparator(*++pSrc))
					;
		}
		pRoot = pDst;
		if (isHost && isLetter(pSrc[0]) && pSrc[1] == ':') {
			*pDst++ = pSrc[0] & ~0x20;
			if (pDst >= pEnd)
				goto fail;
			*pDst++ = pSrc[1];
			pSrc += 2;
			pBase += 2;
			pRoot += 2;
			if (pDst >= pEnd)
				goto fail;
			allowRelative = true;
			relative = false;
		}
	}

	c = *pSrc;
	while (c == '.' && isSeparator(pSrc[1])) {
		if (!hasDevice || allowRelative)
			absolute = false;
		pSrc += 2;
		c = *pSrc;
		if (isSeparator(c))
			while (isSeparator(c = *++pSrc))
				;
	}
	if (!isSeparator(c) && (!hasDevice || allowRelative))
		absolute = false;
	if (style == PATH_STYLE_WINDOWS && pSrc == pPath && pPath[0] == '\\' && pPath[1] == '\\') {
		*pDst++ = *pSrc++;
		absolute = true;
		pBase++;
		pRoot++;
		if (pDst >= pEnd)
			goto fail;
	} else if (!absolute) {
		relative = true;
	}

	prev = sep;
	do {
		if (isSeparator(prev))
			prev = sep;
		if (prev == sep) {
			c = *pSrc;
			if (!isSeparator(c) && c != 0) {
				if (c == '.' && (isSeparator(pSrc[1]) || pSrc[1] == 0)) {
					pSrc++;
				} else if (c == '.' && pSrc[1] == '.' &&
					   (isSeparator(pSrc[2]) || pSrc[2] == 0)) {
					pSrc += 2;
					if (pDst == pBase) {
						if (!absolute) {
							if (pRoot != pBase) {
								*pDst++ = sep;
								if (pDst >= pEnd)
									goto fail;
							}
							*pDst++ = '.';
							if (pDst >= pEnd)
								goto fail;
							*pDst++ = '.';
							pBase = pDst;
						}
					} else {
						do
							pDst--;
						while (pDst > pBase && *pDst != sep);
					}
				} else if (!relative || pDst != pRoot) {
					*pDst++ = sep;
				}
			}
		} else {
			*pDst++ = prev;
		}
		if (pDst >= pEnd)
			goto fail;
		prev = *pSrc++;
	} while (prev != 0);
	if (pDst == pRoot) {
		*pDst++ = absolute && !relative ? sep : '.';
		if (pDst >= pEnd)
			goto fail;
	}
	*pDst = 0;
	return pDst - pOut;

fail:
	*pOut = 0;
	return size + 1;
}

__attribute__((noinline))
size_t sceFiosPathNormalize(char *pBuffer, const char *pPath, size_t bufferSize)
{
	return fiosPathNormalizeStyle(pBuffer, pPath, bufferSize, PATH_STYLE_PSP2);
}
