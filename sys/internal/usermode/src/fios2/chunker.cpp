/*
 * chunker.cpp: an op made into a chunk tree ("Chunker::CreateInitialChunks"): its root,
 * the chunks its command needs, the I/O filters' changes, then its large reads and writes
 * cut into chunks of g_FiosParams.maxChunk. The function names but CreateInitialChunks are
 * ours.
 */
#include "fios2_internal.h"

/*
 * "ChunkTree": cut the reads and writes of the tree that are larger than
 * g_FiosParams.maxChunk into chunks of that size, each linked to the parents and children of
 * the first; a read stops at the end of an open file.
 */
int Chunker::ChunkTree(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	uint16_t *pChildren = NULL;
	int ret = SCE_FIOS_OK;
	int count = 0;
	int i;

	while (fiosChunkGetChild(chunkID, count) != CHUNKID_INVALID)
		count++;
	if (count > 0) {
		uint16_t children[count];

		pChildren = children;
		for (i = 0; i < count; i++)
			pChildren[i] = fiosChunkGetChild(chunkID, i);
	}
	if ((pChunk->type == CHUNK_FH_READ || pChunk->type == CHUNK_FH_WRITE) && !pChunk->whole) {
		SceFiosSize maxChunk = g_FiosParams.maxChunk;

		if (pChunk->reqCount > maxChunk) {
			SceFiosSize remaining, size;
			SceFiosSize *pActCount;
			SceFiosOffset offset;
			SceFiosHandle h;
			uint8_t *pBuf;
			FiosFH *pFH;

			FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
				   "%-15s chunking read (chunk %d) of %lld bytes", "ChunkTree", chunkID,
				   pChunk->reqCount);
			pActCount = pChunk->pActCount;
			pBuf = pChunk->pBuf + maxChunk;
			h = pChunk->h;
			remaining = pChunk->reqCount - maxChunk;
			offset = pChunk->offset + maxChunk;
			pFH = (FiosFH *)s_pFHTable->Lookup(h);
			pChunk->reqCount = maxChunk;
			while (remaining > 0) {
				FiosChunk *pNew;
				uint16_t newID, relativeID;
				int j;

				if (pChunk->type == CHUNK_FH_READ && pFH->isOpen && offset >= pFH->size)
					break;
				newID = fiosChunkAllocateType(pChunk->type);
				if (newID == CHUNKID_INVALID)
					return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
				size = maxChunk > remaining ? remaining : maxChunk;
				pNew = fiosChunkAt(newID);
				pNew->h = h;
				pNew->reqCount = size;
				pNew->pActCount = pActCount;
				pNew->pBuf = pBuf;
				pNew->offset = offset;
				offset += size;
				remaining -= size;
				pBuf += size;
				j = 0;
				while ((relativeID = fiosChunkGetParent(chunkID, j++)) != CHUNKID_INVALID) {
					if (!fiosChunkLink(relativeID, newID)) {
						if (j == 1)
							fiosChunkDeallocate(newID);
						return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
					}
				}
				j = 0;
				while ((relativeID = fiosChunkGetChild(chunkID, j++)) != CHUNKID_INVALID)
					if (!fiosChunkLink(newID, relativeID))
						return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
			}
		}
	}
	for (i = 0; i < count; i++) {
		ret = ChunkTree(pChildren[i]);
		if (ret != SCE_FIOS_OK)
			return ret;
	}
	return ret;
}

/* the chunk each op starts with under its root (CHUNK_NULL: made by the op's case) */
static const uint8_t s_opChunks[] = {
	CHUNK_DELETE, CHUNK_DH_CLOSE, CHUNK_DH_OPEN, CHUNK_DH_READ, CHUNK_NULL,
	CHUNK_DIR_CREATE, CHUNK_DIR_DELETE, CHUNK_DIR_EXISTS, CHUNK_EXISTS, CHUNK_FH_CLOSE,
	CHUNK_FH_OPEN, CHUNK_NULL, CHUNK_NULL, CHUNK_NULL, CHUNK_STAT, CHUNK_FH_SYNC,
	CHUNK_TRUNCATE, CHUNK_NULL, CHUNK_FILE_DELETE, CHUNK_FILE_EXISTS, CHUNK_FILE_SIZE,
	CHUNK_NULL, CHUNK_NULL, CHUNK_PREFETCH, CHUNK_PREFETCH, CHUNK_NULL, CHUNK_NULL,
	CHUNK_RENAME, CHUNK_RESOLVE, CHUNK_STAT, CHUNK_TRUNCATE, CHUNK_ARCHIVE_QUERY,
	CHUNK_ARCHIVE_MOUNT, CHUNK_ARCHIVE_UNMOUNT, CHUNK_CHSTAT, CHUNK_DEVCTL, CHUNK_IOCTL,
	CHUNK_SYNC,
};

/* a copy of a string in a CHUNK_BUFFER under the chunk */
static inline char *copyString(uint16_t chunkID, const char *pString)
{
	size_t length = fiosStrlen(pString) + 1;
	char *pCopy = (char *)fiosChunkAllocateBuffer(chunkID, length);

	if (pCopy != NULL)
		fiosStrlcpy(pCopy, pString, length);
	return pCopy;
}

/* the op's root chunk and the chunks under it, CHUNKID_INVALID with the op's err set */
uint16_t Chunker::CreateInitialChunks(FiosOp *pOp)
{
	uint16_t rootID = fiosChunkAllocateType(CHUNK_ROOT);
	uint16_t chunkID = CHUNKID_INVALID;
	uint8_t type = s_opChunks[pOp->what];
	FiosChunk *pChunk;
	FiosFH *pFH;
	FiosDH *pDH;

	if (rootID == CHUNKID_INVALID)
		goto noChunks;
	fiosChunkAt(rootID)->op = pOp->h;
	if (type != CHUNK_NULL) {
		chunkID = fiosChunkAllocateChild(rootID, type);
		if (chunkID == CHUNKID_INVALID)
			goto noChunks;
	}
	switch (pOp->what) {
	case OP_DELETE:
	case OP_DIRECTORY_DELETE:
	case OP_FILE_DELETE:
		fiosChunkAt(chunkID)->pPath = &pOp->path;
		return rootID;
	case OP_DH_CLOSE:
	case OP_FH_CLOSE:
	case OP_FH_SYNC:
	case OP_ARCHIVE_UNMOUNT:
		fiosChunkAt(chunkID)->h = pOp->fh;
		return rootID;
	case OP_DH_OPEN:
		pDH = (FiosDH *)s_pDHTable->Lookup(pOp->fh);
		if (pDH == NULL) {
			pOp->err = SCE_FIOS_ERROR_BAD_DH;
			goto failed;
		}
		pChunk = fiosChunkAt(chunkID);
		pChunk->open.h = pOp->fh;
		pChunk->open.pPath = &pOp->path;
		if (pDH->path.Set(fiosPathGet(&pOp->path), false))
			return rootID;
		break;
	case OP_DH_READ:
		if (s_pDHTable->Lookup(pOp->fh) == NULL) {
			pOp->err = SCE_FIOS_ERROR_BAD_DH;
			goto failed;
		}
		pChunk = fiosChunkAt(chunkID);
		pChunk->h = pOp->fh;
		pChunk->pBuf = (uint8_t *)pOp->buffers[0].pPtr;
		pChunk->reqCount = pOp->buffers[0].length;
		return rootID;
	case OP_DIRECTORY_CREATE:
		pChunk = fiosChunkAt(chunkID);
		pChunk->create.pPath = &pOp->path;
		pChunk->create.nativeMode = pOp->nativeMode;
		return rootID;
	case OP_DIRECTORY_EXISTS:
		pChunk = fiosChunkAt(chunkID);
		pChunk->query.pPath = &pOp->path;
		pChunk->query.pResult = pOp->buffers[0].pPtr;
		return rootID;
	case OP_EXISTS:
		pChunk = fiosChunkAt(chunkID);
		pChunk->query.pPath = &pOp->path;
		pChunk->query.pResult = pOp->buffers[0].pPtr;
		return rootID;
	case OP_FH_OPEN:
		pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
		if (pFH == NULL) {
			pOp->err = SCE_FIOS_ERROR_BAD_FH;
			goto failed;
		}
		pChunk = fiosChunkAt(chunkID);
		pChunk->open.h = pOp->fh;
		pChunk->open.pPath = &pOp->path;
		pChunk->open.nativeMode = pOp->nativeMode;
		if (pFH->path.Set(fiosPathGet(&pOp->path), false))
			return rootID;
		break;
	case OP_FH_PREADV:
	case OP_FH_PWRITEV:
	case OP_FH_READV:
	case OP_FH_WRITEV: {
		uint8_t ioType = CHUNK_FH_WRITE;
		SceFiosOffset offset;
		uint32_t i;

		if (pOp->what == OP_FH_PREADV || pOp->what == OP_FH_READV)
			ioType = CHUNK_FH_READ;
		pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
		if (pFH == NULL) {
			pOp->err = SCE_FIOS_ERROR_BAD_FH;
			goto failed;
		}
		offset = pOp->offset;
		for (i = 0; i < pOp->bufferCount; i++) {
			uint16_t ioID = fiosChunkAllocateChild(rootID, ioType);

			if (ioID == CHUNKID_INVALID)
				goto noChunks;
			pChunk = fiosChunkAt(ioID);
			pChunk->h = pOp->fh;
			pChunk->offset = offset;
			pChunk->pActCount = &pOp->actCount;
			pChunk->reqCount = pOp->buffers[i].length;
			offset += pOp->buffers[i].length;
			pChunk->pBuf = (uint8_t *)pOp->buffers[i].pPtr;
			if (ioType == CHUNK_FH_READ && pFH->isOpen && offset >= pFH->size)
				break;
		}
		return rootID;
	}
	case OP_FH_STAT:
		pChunk = fiosChunkAt(chunkID);
		pChunk->stat.h = pOp->fh;
		pChunk->stat.pStatus = (SceFiosStat *)pOp->buffers[0].pPtr;
		return rootID;
	case OP_FH_TRUNCATE:
		pChunk = fiosChunkAt(chunkID);
		pChunk->truncate.fh = pOp->fh;
		pChunk->truncate.length = pOp->length;
		return rootID;
	case OP_FILE_EXISTS:
		pChunk = fiosChunkAt(chunkID);
		pChunk->query.pPath = &pOp->path;
		pChunk->query.pResult = pOp->buffers[0].pPtr;
		return rootID;
	case OP_FILE_GET_SIZE:
		pChunk = fiosChunkAt(chunkID);
		pChunk->query.pPath = &pOp->path;
		pChunk->query.pResult = pOp->buffers[0].pPtr;
		return rootID;
	case OP_PATH_READ:
	case OP_PATH_WRITE: {
		uint8_t ioType = CHUNK_FH_WRITE;
		uint16_t parentID, openID;
		SceFiosOffset offset;
		size_t length;
		void *pBuf;

		if (pOp->what == OP_PATH_READ)
			ioType = CHUNK_FH_READ;
		parentID = fiosChunkAllocateChild(rootID, CHUNK_FH_CLOSE);
		fiosChunkAt(parentID)->h = pOp->fh;
		length = pOp->buffers[0].length;
		pBuf = pOp->buffers[0].pPtr;
		offset = pOp->offset;
		if (length != 0) {
			parentID = fiosChunkAllocateChild(parentID, ioType);
			if (parentID == CHUNKID_INVALID)
				break;
			pChunk = fiosChunkAt(parentID);
			pChunk->h = pOp->fh;
			pChunk->reqCount = length;
			pChunk->offset = offset;
			pChunk->pActCount = &pOp->actCount;
			pChunk->pBuf = (uint8_t *)pBuf;
		} else {
			pOp->actCount = 0;
		}
		openID = fiosChunkAllocateChild(parentID, CHUNK_FH_OPEN);
		if (openID == CHUNKID_INVALID)
			break;
		pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
		if (pFH == NULL) {
			pOp->err = SCE_FIOS_ERROR_BAD_FH;
			goto failed;
		}
		pChunk = fiosChunkAt(openID);
		pChunk->open.h = pOp->fh;
		pChunk->open.pPath = &pOp->path;
		pChunk->open.nativeMode = -1;
		if (pFH->path.Set(fiosPathGet(&pOp->path), false))
			return rootID;
		break;
	}
	case OP_PREFETCH_FH:
	case OP_PREFETCH_FH_RANGE:
		pChunk = fiosChunkAt(chunkID);
		pChunk->h = pOp->fh;
		pChunk->offset = pOp->offset;
		if (pOp->what == OP_PREFETCH_FH)
			pChunk->reqCount = SCE_FIOS_OFFSET_MAX;
		else
			pChunk->reqCount = pOp->length;
		return rootID;
	case OP_PREFETCH_FILE:
	case OP_PREFETCH_FILE_RANGE: {
		uint16_t closeID, prefetchID, openID;

		closeID = fiosChunkAllocateChild(rootID, CHUNK_FH_CLOSE);
		fiosChunkAt(closeID)->h = pOp->fh;
		prefetchID = fiosChunkAllocateChild(closeID, CHUNK_PREFETCH);
		if (prefetchID == CHUNKID_INVALID)
			break;
		pChunk = fiosChunkAt(prefetchID);
		pChunk->h = pOp->fh;
		pChunk->reqCount = pOp->what == OP_PREFETCH_FILE ? SCE_FIOS_OFFSET_MAX : pOp->length;
		pChunk->offset = pOp->offset;
		pChunk->pActCount = &pOp->actCount;
		openID = fiosChunkAllocateChild(prefetchID, CHUNK_FH_OPEN);
		if (openID == CHUNKID_INVALID)
			break;
		pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
		if (pFH == NULL) {
			pOp->err = SCE_FIOS_ERROR_BAD_FH;
			goto failed;
		}
		pChunk = fiosChunkAt(openID);
		pChunk->open.h = pOp->fh;
		pChunk->open.pPath = &pOp->path;
		pChunk->open.nativeMode = -1;
		if (pFH->path.Set(fiosPathGet(&pOp->path), false))
			return rootID;
		break;
	}
	case OP_RENAME: {
		char *pNewPath = copyString(chunkID, pOp->pPath2);

		if (pNewPath == NULL)
			break;
		pChunk = fiosChunkAt(chunkID);
		pChunk->rename.pNewPath = pNewPath;
		pChunk->rename.pPath = &pOp->path;
		return rootID;
	}
	case OP_RESOLVE:
		pChunk = fiosChunkAt(chunkID);
		pChunk->resolve.pOutTuple = (SceFiosTuple *)pOp->buffers[0].pPtr;
		pChunk->resolve.pPath = &pOp->path;
		pChunk->resolve.offset = pOp->offset;
		pChunk->resolve.size = pOp->length;
		return rootID;
	case OP_STAT:
		pChunk = fiosChunkAt(chunkID);
		pChunk->stat.h = pOp->fh;
		pChunk->stat.pPath = &pOp->path;
		pChunk->stat.pStatus = (SceFiosStat *)pOp->buffers[0].pPtr;
		return rootID;
	case OP_TRUNCATE:
		pChunk = fiosChunkAt(chunkID);
		pChunk->truncate.pPath = &pOp->path;
		pChunk->truncate.length = pOp->length;
		return rootID;
	case OP_ARCHIVE_QUERY:
	case OP_ARCHIVE_MOUNT: {
		char *pMountPoint = NULL;

		if (pOp->pPath2 != NULL) {
			pMountPoint = copyString(chunkID, pOp->pPath2);
			if (pMountPoint == NULL)
				break;
		}
		pChunk = fiosChunkAt(chunkID);
		pChunk->mount.fh = pOp->fh;
		pChunk->mount.pPath = &pOp->path;
		pChunk->mount.pMountBuffer = pOp->buffers[0].pPtr;
		pChunk->mount.mountBufferLength = pOp->buffers[0].length;
		pChunk->mount.pActCount = &pOp->actCount;
		pChunk->mount.pMountPoint = pMountPoint;
		pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
		if (pFH == NULL) {
			pOp->err = SCE_FIOS_ERROR_BAD_FH;
			goto failed;
		}
		if (pFH->path.Set(fiosPathGet(&pOp->path), false))
			return rootID;
		break;
	}
	case OP_CH_STAT:
		pChunk = fiosChunkAt(chunkID);
		pChunk->chstat.pPath = &pOp->path;
		pChunk->chstat.pStatus = (SceFiosStat *)pOp->buffers[0].pPtr;
		pChunk->chstat.cbit = pOp->cbit;
		return rootID;
	case OP_DEVCTL:
		pChunk = fiosChunkAt(chunkID);
		pChunk->ioctl.pPath = &pOp->path;
		pChunk->ioctl.cmd = pOp->cmd;
		pChunk->ioctl.pArg = pOp->buffers[0].pPtr;
		pChunk->ioctl.argLength = pOp->buffers[0].length;
		pChunk->ioctl.pBuffer = pOp->buffers[1].pPtr;
		pChunk->ioctl.bufferLength = pOp->buffers[1].length;
		return rootID;
	case OP_IOCTL:
		pChunk = fiosChunkAt(chunkID);
		pChunk->ioctl.h = pOp->fh;
		pChunk->ioctl.cmd = pOp->cmd;
		pChunk->ioctl.pArg = pOp->buffers[0].pPtr;
		pChunk->ioctl.argLength = pOp->buffers[0].length;
		pChunk->ioctl.pBuffer = pOp->buffers[1].pPtr;
		pChunk->ioctl.bufferLength = pOp->buffers[1].length;
		return rootID;
	case OP_SYNC:
		pChunk = fiosChunkAt(chunkID);
		pChunk->sync.pPath = &pOp->path;
		pChunk->sync.flag = pOp->flag;
		return rootID;
	default:
		sceFiosPrintf("Chunker::CreateInitialChunks received an invalid op! handle = 0x%08lX, "
			      "pOp = 0x%08llX, what = %d\n", pOp->h, (long long)(intptr_t)pOp,
			      pOp->what);
		break;
	}
noChunks:
	pOp->err = SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
failed:
	fiosChunkDelete(rootID);
	return CHUNKID_INVALID;
}

/*
 * The op's chunk tree, prepared and under the null chunk for the scheduler; *pDone when the
 * op had nothing left to do once the filters ran (it is completed then).
 */
uint16_t Chunker::ChunkOp(FiosOp *pOp, bool *pDone)
{
	uint16_t rootID;

	*pDone = false;
	rootID = CreateInitialChunks(pOp);
	if (rootID == CHUNKID_INVALID)
		return CHUNKID_INVALID;
	if (s_ioFilterCount > 0)
		rootID = fiosIOFiltersChunkTree(rootID);
	if ((rootID != CHUNKID_INVALID && fiosChunkGetChild(rootID, 0) == CHUNKID_INVALID) ||
	    fiosChunkChildrenComplete(rootID)) {
		executeRoot(fiosChunkAt(rootID));
		fiosChunkDelete(rootID);
		*pDone = true;
		return CHUNKID_INVALID;
	}
	if (rootID == CHUNKID_INVALID)
		return CHUNKID_INVALID;
	if (ChunkTree(rootID) != SCE_FIOS_OK) {
		fiosChunkDelete(rootID);
		pOp->err = SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
		return CHUNKID_INVALID;
	}
	fiosChunkPrepare(rootID, pOp->attr.priority, pOp->attr.deadline);
	s_chunkMutex.Lock();
	if (!fiosChunkLink(s_nullChunk, rootID)) {
		fiosChunkDelete(rootID);
		pOp->err = SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
		s_chunkMutex.Unlock();
		return CHUNKID_INVALID;
	}
	s_chunkMutex.Unlock();
	return rootID;
}
