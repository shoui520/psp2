/*
 * chunk.cpp: the chunk trees the scheduler runs. An op becomes a tree of chunks, its root
 * a CHUNK_ROOT and its leaves the I/O; a chunk is done when its children are. The names in
 * quotes are the ones the profiling messages give ("Allocate", "Link", "Defer", ...); the
 * rest are ours.
 */
#include <algorithm>
#include <sce_atomic.h>
#include "fios2_internal.h"

/* the bytes each chunk type takes, its header included */
const uint8_t s_chunkSizes[] = {
	24, 32, 32, 40, 64, 32, 48, 56, 56, 32, 32, 32,
	48, 56, 56, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	40, 40, 56, 48, 32, 48, 48, 32, 40, 64, 64, 32,
};

Allocator s_chunkAllocator = { "chunk", 0, 0, 0xffffffff, 0, 1 };
uint16_t s_nullChunk = CHUNKID_INVALID;		/* a CHUNK_NULL no op names */

/* a chunk's next child slot: a CHUNK_CHILD list's last link, any other chunk's child[1] */
static inline uint16_t getChildNext(FiosChunk *pChunk)
{
	return pChunk->type == CHUNK_CHILD ? pChunk->links[7] : pChunk->child[1];
}

static inline void setChildNext(FiosChunk *pChunk, uint16_t chunkID)
{
	if (pChunk->type == CHUNK_CHILD)
		pChunk->links[7] = chunkID;
	else
		pChunk->child[1] = chunkID;
}

/* the slot before it */
static inline void setChildLast(FiosChunk *pChunk, uint16_t chunkID)
{
	if (pChunk->type == CHUNK_CHILD)
		pChunk->links[6] = chunkID;
	else
		pChunk->child[0] = chunkID;
}

/* a chunk's next parent slot: a CHUNK_PARENT list's first link, any other chunk's parent */
static inline uint16_t getParentNext(FiosChunk *pChunk)
{
	return pChunk->type == CHUNK_PARENT ? pChunk->links[0] : pChunk->parent;
}

static inline void setParentNext(FiosChunk *pChunk, uint16_t chunkID)
{
	if (pChunk->type == CHUNK_PARENT)
		pChunk->links[0] = chunkID;
	else
		pChunk->parent = chunkID;
}

/* s_chunkMutex for a scope, when lock is true */
class ChunkLock {
public:
	ChunkLock(bool lock) : m_locked(lock)
	{
		if (m_locked)
			s_chunkMutex.Lock();
	}
	~ChunkLock()
	{
		if (m_locked)
			s_chunkMutex.Unlock();
	}

private:
	bool m_locked;
};

uint16_t fiosChunkAllocate(uint8_t type, uint32_t size)
{
	FiosChunk *pChunk;
	uint16_t chunkID;
	int index;

	index = s_chunkAllocator.Allocate((size + 7) >> 3);
	if (index == -1) {
		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s (failed)", "Allocate");
		return CHUNKID_INVALID;
	}
	chunkID = (uint16_t)index;
	pChunk = fiosChunkAt(chunkID);
	pChunk->magic = FIOS_CHUNK_MAGIC;
	pChunk->type = type;
	pChunk->bufferChunk = CHUNKID_INVALID;
	pChunk->child[1] = CHUNKID_INVALID;
	pChunk->child[0] = CHUNKID_INVALID;
	pChunk->parent = CHUNKID_INVALID;
	FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s %s", "Allocate",
		   fiosDebugDumpChunk(chunkID, NULL, 0));
	return chunkID;
}

uint16_t fiosChunkAllocateType(uint8_t type)
{
	return fiosChunkAllocate(type, s_chunkSizes[type]);
}

__attribute__((noinline)) uint16_t fiosChunkTryAllocate(uint8_t type)
{
	if (!s_libraryInitialized)
		return CHUNKID_INVALID;
	return fiosChunkAllocate(type, s_chunkSizes[type]);
}

void fiosChunkDeallocate(uint16_t chunkID)
{
	FiosChunk *pChunk;
	uint32_t size;

	FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s %s", "Deallocate",
		   fiosDebugDumpChunk(chunkID, NULL, 0));
	if (chunkID == CHUNKID_INVALID)
		return;
	pChunk = fiosChunkAt(chunkID);
	size = s_chunkSizes[pChunk->type];
	if (pChunk->type == CHUNK_BUFFER)
		size += (uint32_t)pChunk->buffer.size;
	fiosChunkAt(chunkID)->magic = 0;
	s_chunkAllocator.Free(chunkID, (size + 7) >> 3);
}

void fiosChunkFree(uint16_t chunkID)
{
	fiosChunkDeallocate(chunkID);
}

/*
 * Move the first child of the chunk's child list into the chunk's last slot, and so on down
 * the lists; a list left with one child is freed and its child takes its place. False when
 * the chunk has no list.
 */
bool pullChild(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	FiosChunk *pList;
	uint16_t listID;
	int i;

	listID = getChildNext(pChunk);
	if (listID == CHUNKID_INVALID)
		return false;
	pList = fiosChunkAt(listID);
	if (pList->type != CHUNK_CHILD)
		return false;
	setChildLast(pChunk, pList->child[0]);
	pList->child[0] = pList->child[1];
	pList->child[1] = pList->links[0];
	for (i = 0; i < 6; i++)
		pList->links[i] = pList->links[i + 1];
	if (!pullChild(listID)) {
		pList->links[6] = pList->links[7];
		pList->links[7] = CHUNKID_INVALID;
	}
	if (pList->child[1] == CHUNKID_INVALID) {
		setChildNext(pChunk, pList->child[0]);
		fiosChunkDeallocate(listID);
	}
	return true;
}

/* "AddChild": a child of a chunk whose two child slots are taken */
bool addChild(uint16_t chunkID, uint16_t childID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	FiosChunk *pList;
	uint16_t listID, newListID;
	uint32_t i;

	listID = getChildNext(pChunk);
	pList = fiosChunkAt(listID);
	if (pList->type != CHUNK_CHILD) {
		newListID = fiosChunkAllocateType(CHUNK_CHILD);
		if (newListID == CHUNKID_INVALID)
			return false;
		pList = fiosChunkAt(newListID);
		for (i = 0; i < 8; i++)
			pList->links[i] = CHUNKID_INVALID;
		pList->child[0] = listID;
		pList->child[1] = childID;
		pList->parent = chunkID;
		setChildNext(pChunk, newListID);
		return true;
	}
	for (i = 0; i < 8; i++) {
		if (pList->links[i] == CHUNKID_INVALID) {
			pList->links[i] = childID;
			return true;
		}
	}
	return addChild(listID, childID);
}

/* "AddParent": a parent of a chunk whose parent slot is taken */
bool addParent(uint16_t chunkID, uint16_t parentID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	FiosChunk *pList;
	uint16_t listID, newListID;
	uint32_t i;

	listID = getParentNext(pChunk);
	pList = fiosChunkAt(listID);
	if (pList->type != CHUNK_PARENT) {
		newListID = fiosChunkAllocateType(CHUNK_PARENT);
		if (newListID == CHUNKID_INVALID)
			return false;
		pList = fiosChunkAt(newListID);
		pList->links[0] = listID;
		pList->links[1] = parentID;
		pList->parent = chunkID;
		setParentNext(pChunk, newListID);
		pList->links[2] = CHUNKID_INVALID;
		pList->links[3] = CHUNKID_INVALID;
		return true;
	}
	for (i = 0; i < 4; i++) {
		if (pList->links[i] == CHUNKID_INVALID) {
			pList->links[i] = parentID;
			return true;
		}
	}
	return addParent(listID, parentID);
}

/* take a child out of the chunk's child lists */
bool removeChild(uint16_t chunkID, uint16_t childID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	FiosChunk *pList;
	uint16_t listID;
	bool found = false;
	uint32_t i = 0;

	listID = getChildNext(pChunk);
	if (listID == CHUNKID_INVALID)
		return found;
	pList = fiosChunkAt(listID);
	if (pList->type != CHUNK_CHILD)
		return found;
	if (pList->child[0] == childID) {
		pList->child[0] = pList->child[1];
		pList->child[1] = pList->links[0];
		found = true;
	} else if (pList->child[1] == childID) {
		pList->child[1] = pList->links[0];
		found = true;
	} else {
		for (i = 0; i < 8; i++) {
			if (pList->links[i] == childID) {
				found = true;
				break;
			}
		}
	}
	if (found) {
		if (i < 7) {
			for (; i < 6; i++)
				pList->links[i] = pList->links[i + 1];
			if (!pullChild(listID)) {
				pList->links[i] = pList->links[i + 1];
				pList->links[i + 1] = CHUNKID_INVALID;
			}
		} else if (i < 8) {
			pList->links[i] = CHUNKID_INVALID;
		}
		if (pList->child[1] == CHUNKID_INVALID) {
			setChildNext(pChunk, pList->child[0]);
			fiosChunkDeallocate(listID);
		}
	}
	if (!found)
		found = removeChild(listID, childID);
	return found;
}

/* take a child out of a chunk's children */
static inline void detachChild(uint16_t parentID, uint16_t childID)
{
	FiosChunk *pParent = fiosChunkAt(parentID);

	if (pParent->child[0] == childID) {
		if (!pullChild(parentID)) {
			pParent->child[0] = pParent->child[1];
			pParent->child[1] = CHUNKID_INVALID;
		}
	} else if (pParent->child[1] == childID) {
		pParent->child[1] = CHUNKID_INVALID;
	} else {
		removeChild(parentID, childID);
	}
}

bool fiosChunkLink(uint16_t parentID, uint16_t childID)
{
	FiosChunk *pParent, *pChild;
	bool linked = false;

	FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s parent %d, newChild %d", "Link",
		   parentID, childID);
	if (parentID == CHUNKID_INVALID || childID == CHUNKID_INVALID)
		return linked;
	pParent = fiosChunkAt(parentID);
	if (pParent->child[0] == CHUNKID_INVALID) {
		pParent->child[0] = childID;
	} else if (pParent->child[1] == CHUNKID_INVALID) {
		pParent->child[1] = childID;
	} else if (!addChild(parentID, childID)) {
		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s failed (parent %d, newChild %d)",
			   "AddChild", parentID, childID);
		return linked;
	}
	pChild = fiosChunkAt(childID);
	if (pChild->parent == CHUNKID_INVALID) {
		pChild->parent = parentID;
		linked = true;
	} else if (addParent(childID, parentID)) {
		linked = true;
	} else {
		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s failed (child %d, newParent %d)",
			   "AddParent", childID, parentID);
		detachChild(parentID, childID);
	}
	return linked;
}

__attribute__((noinline)) uint16_t fiosChunkAllocateChild(uint16_t parentID, uint8_t type)
{
	uint16_t chunkID = CHUNKID_INVALID;

	FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s parent %d, type %d", "AllocateChild",
		   parentID, type);
	if (parentID != CHUNKID_INVALID) {
		chunkID = fiosChunkAllocate(type, s_chunkSizes[type]);
		if (chunkID != CHUNKID_INVALID && !fiosChunkLink(parentID, chunkID)) {
			fiosChunkDeallocate(chunkID);
			chunkID = CHUNKID_INVALID;
		}
	}
	return chunkID;
}

uint16_t fiosChunkAllocateChildLocked(uint16_t parentID, uint8_t type)
{
	uint16_t chunkID;

	s_chunkMutex.Lock();
	chunkID = fiosChunkAllocateChild(parentID, type);
	s_chunkMutex.Unlock();
	return chunkID;
}

/* a CHUNK_BUFFER of size bytes under the parent, completed: its data */
void *fiosChunkAllocateBuffer(uint16_t parentID, uint32_t size)
{
	FiosChunk *pChunk;
	uint16_t chunkID;
	void *pData = NULL;

	if (parentID == CHUNKID_INVALID)
		return pData;
	chunkID = fiosChunkAllocate(CHUNK_BUFFER, size + 32);
	if (chunkID == CHUNKID_INVALID)
		return pData;
	pChunk = fiosChunkAt(chunkID);
	pChunk->buffer.size = size;
	pChunk->scheduled = 1;
	pChunk->completed = 1;
	pData = pChunk->buffer.data;
	if (!fiosChunkLink(parentID, chunkID)) {
		fiosChunkDeallocate(chunkID);
		return NULL;
	}
	return pData;
}

void *fiosChunkAllocateBufferLocked(uint16_t parentID, uint32_t size)
{
	void *pData;

	s_chunkMutex.Lock();
	pData = fiosChunkAllocateBuffer(parentID, size);
	s_chunkMutex.Unlock();
	return pData;
}

/* a CHUNK_CHILD list's child at index */
uint16_t fiosChunkGetListChild(uint16_t listID, int index)
{
	FiosChunk *pList;
	uint16_t nextID;
	int count, i;

	if (listID == CHUNKID_INVALID)
		return CHUNKID_INVALID;
	pList = fiosChunkAt(listID);
	if (pList->type != CHUNK_CHILD)
		return CHUNKID_INVALID;
	if (index == 0)
		return pList->child[0];
	if (index == 1)
		return pList->child[1];
	nextID = pList->links[7];
	count = 8;
	index -= 2;
	if (nextID != CHUNKID_INVALID && fiosChunkAt(nextID)->type == CHUNK_CHILD)
		count = 7;
	for (i = 0; i < count; i++) {
		if (pList->links[i] == CHUNKID_INVALID)
			break;
		if (i == index)
			return pList->links[i];
	}
	return fiosChunkGetListChild(nextID, index - i);
}

uint16_t fiosChunkGetChild(uint16_t chunkID, int index)
{
	FiosChunk *pChunk;
	uint16_t childID;

	if (chunkID == CHUNKID_INVALID)
		return CHUNKID_INVALID;
	pChunk = fiosChunkAt(chunkID);
	if (index == 0)
		return pChunk->child[0];
	childID = pChunk->child[1];
	if (childID != CHUNKID_INVALID && fiosChunkAt(childID)->type == CHUNK_CHILD)
		return fiosChunkGetListChild(childID, index - 1);
	if (index == 1)
		return childID;
	return CHUNKID_INVALID;
}

/* a CHUNK_PARENT list's parent at index */
uint16_t fiosChunkGetListParent(uint16_t listID, int index)
{
	FiosChunk *pList;
	uint16_t nextID;
	int i;

	if (listID == CHUNKID_INVALID)
		return CHUNKID_INVALID;
	pList = fiosChunkAt(listID);
	if (pList->type != CHUNK_PARENT)
		return CHUNKID_INVALID;
	nextID = pList->links[0];
	if (fiosChunkAt(nextID)->type == CHUNK_PARENT)
		index++;
	for (i = 0; i < 4; i++) {
		if (pList->links[i] == CHUNKID_INVALID)
			break;
		if (i == index)
			return pList->links[i];
	}
	return fiosChunkGetListParent(nextID, index - i);
}

uint16_t fiosChunkGetParent(uint16_t chunkID, int index)
{
	uint16_t parentID;

	if (chunkID == CHUNKID_INVALID)
		return CHUNKID_INVALID;
	parentID = fiosChunkAt(chunkID)->parent;
	if (parentID == CHUNKID_INVALID)
		return CHUNKID_INVALID;
	if (index != 0 || fiosChunkAt(parentID)->type == CHUNK_PARENT)
		return fiosChunkGetListParent(parentID, index);
	return parentID;
}

/* take a parent out of a CHUNK_PARENT list */
bool removeParent(uint16_t listID, uint16_t parentID)
{
	uint8_t *pStorage = (uint8_t *)g_FiosParams.chunkStorage.pPtr;
	FiosChunk *pList, *pOwner, *pNext;
	uint16_t ownerID;
	uint32_t i;

	if (listID == CHUNKID_INVALID)
		return false;
	pList = (FiosChunk *)(pStorage + listID * 8);
	if (pList->type != CHUNK_PARENT)
		return false;
	for (i = 0; i < 4; i++) {
		if (pList->links[i] == parentID) {
			for (; i < 3; i++)
				pList->links[i] = pList->links[i + 1];
			pList->links[3] = CHUNKID_INVALID;
			if (pList->links[1] == CHUNKID_INVALID) {
				ownerID = pList->parent;
				pOwner = (FiosChunk *)(pStorage + ownerID * 8);
				setParentNext(pOwner, pList->links[0]);
				pNext = (FiosChunk *)(pStorage + pList->links[0] * 8);
				if (pNext->type == CHUNK_PARENT)
					pNext->parent = ownerID;
				fiosChunkDeallocate(listID);
			}
			return true;
		}
	}
	return removeParent(pList->links[0], parentID);
}

void fiosChunkUnlink(uint16_t parentID, uint16_t childID)
{
	FiosChunk *pChild;
	uint16_t firstID;

	FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s parent %d, child %d", "Unlink",
		   parentID, childID);
	detachChild(parentID, childID);
	pChild = fiosChunkAt(childID);
	firstID = pChild->parent;
	if (firstID == parentID)
		pChild->parent = CHUNKID_INVALID;
	else
		removeParent(firstID, parentID);
}

/* unlink the chunk from a parent; free it and its subtree when it has no parent left */
void fiosChunkRelease(uint16_t chunkID, uint16_t parentID)
{
	uint16_t childID;

	if (chunkID == CHUNKID_INVALID)
		return;
	fiosChunkUnlink(parentID, chunkID);
	if (fiosChunkGetParent(chunkID, 0) != CHUNKID_INVALID)
		return;
	while ((childID = fiosChunkGetChild(chunkID, 0)) != CHUNKID_INVALID)
		fiosChunkRelease(childID, chunkID);
	fiosChunkDeallocate(chunkID);
}

/* unlink the chunk from every parent and free it and its subtree */
void fiosChunkDelete(uint16_t chunkID)
{
	uint16_t parentID, childID;

	if (chunkID == CHUNKID_INVALID)
		return;
	while ((parentID = fiosChunkGetParent(chunkID, 0)) != CHUNKID_INVALID)
		fiosChunkUnlink(parentID, chunkID);
	while ((childID = fiosChunkGetChild(chunkID, 0)) != CHUNKID_INVALID)
		fiosChunkRelease(childID, chunkID);
	fiosChunkDeallocate(chunkID);
}

void fiosChunkDeleteLocked(uint16_t chunkID)
{
	s_chunkMutex.Lock();
	fiosChunkDelete(chunkID);
	s_chunkMutex.Unlock();
}

void fiosChunkReleaseLocked(uint16_t chunkID, uint16_t parentID)
{
	s_chunkMutex.Lock();
	fiosChunkRelease(chunkID, parentID);
	s_chunkMutex.Unlock();
}

bool fiosChunkLinkLocked(uint16_t parentID, uint16_t childID)
{
	bool linked;

	s_chunkMutex.Lock();
	linked = fiosChunkLink(parentID, childID);
	s_chunkMutex.Unlock();
	return linked;
}

void fiosChunkUnlinkLocked(uint16_t parentID, uint16_t childID)
{
	s_chunkMutex.Lock();
	fiosChunkUnlink(parentID, childID);
	s_chunkMutex.Unlock();
}

/* "Replace": move the chunk's parents and children to another */
void fiosChunkReplace(uint16_t oldChunkID, uint16_t newChunkID)
{
	uint16_t parentID, childID;

	FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s oldChunk %d, newChunk %d", "Replace",
		   oldChunkID, newChunkID);
	if (oldChunkID == CHUNKID_INVALID || newChunkID == CHUNKID_INVALID)
		return;
	while ((parentID = fiosChunkGetParent(oldChunkID, 0)) != CHUNKID_INVALID) {
		fiosChunkUnlink(parentID, oldChunkID);
		fiosChunkLink(parentID, newChunkID);
	}
	while ((childID = fiosChunkGetChild(oldChunkID, 0)) != CHUNKID_INVALID) {
		fiosChunkUnlink(oldChunkID, childID);
		fiosChunkLink(newChunkID, childID);
	}
}

void fiosChunkReplaceLocked(uint16_t oldChunkID, uint16_t newChunkID)
{
	s_chunkMutex.Lock();
	fiosChunkReplace(oldChunkID, newChunkID);
	s_chunkMutex.Unlock();
}

/*
 * The chunk takes the earliest deadline and the highest priority of the one given and its
 * parents' (but the null chunk and the parent it was reached from), and passes them down.
 */
void reschedule(uint16_t chunkID, SceFiosTime deadline, SceFiosPriority priority,
		       uint16_t fromID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	uint16_t parentID, childID;
	int i;

	if (!pChunk->completed) {
		SceFiosTime newDeadline = deadline;
		SceFiosPriority newPriority = priority;

		i = 0;
		while ((parentID = fiosChunkGetParent(chunkID, i++)) != CHUNKID_INVALID) {
			FiosChunk *pParent;

			if (parentID == s_nullChunk || parentID == fromID)
				continue;
			pParent = fiosChunkAt(parentID);
			newDeadline = pParent->deadline < deadline ? pParent->deadline : deadline;
			newPriority = priority < pParent->priority ? pParent->priority : priority;
		}
		pChunk->deadline = newDeadline;
		pChunk->priority = newPriority;
	}
	i = 0;
	while ((childID = fiosChunkGetChild(chunkID, i++)) != CHUNKID_INVALID)
		reschedule(childID, deadline, priority, chunkID);
}

void fiosChunkReschedule(uint16_t chunkID, SceFiosTime deadline, SceFiosPriority priority)
{
	s_chunkMutex.Lock();
	reschedule(chunkID, deadline, priority, CHUNKID_INVALID);
	s_chunkMutex.Unlock();
}

uint16_t fiosChunkGetChildLocked(uint16_t chunkID, int index)
{
	uint16_t childID;

	s_chunkMutex.Lock();
	childID = fiosChunkGetChild(chunkID, index);
	s_chunkMutex.Unlock();
	return childID;
}

uint16_t fiosChunkGetParentLocked(uint16_t chunkID, int index)
{
	uint16_t parentID;

	s_chunkMutex.Lock();
	parentID = fiosChunkGetParent(chunkID, index);
	s_chunkMutex.Unlock();
	return parentID;
}

/* the file or directory handle the chunk works on */
SceFiosHandle fiosChunkGetHandle(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);

	switch (pChunk->type) {
	case CHUNK_FH_OPEN:
	case CHUNK_FH_READ:
	case CHUNK_FH_WRITE:
	case CHUNK_FH_BARRIER:
	case CHUNK_FH_SYNC:
	case CHUNK_FH_CLOSE:
	case CHUNK_DH_OPEN:
	case CHUNK_DH_READ:
	case CHUNK_DH_READ_BULK:
	case CHUNK_DH_CLOSE:
	case CHUNK_STAT:
	case CHUNK_PREFETCH:
	case CHUNK_ARCHIVE_MOUNT:
	case CHUNK_ARCHIVE_UNMOUNT:
		return pChunk->h;
	case CHUNK_TRUNCATE:
		return pChunk->truncate.fh;
	default:
		return 0;
	}
}

/*
 * Whether a scheduled chunk (any chunk, with force) can run: its children are complete, and
 * the file or directory it works on is open ("Defer" otherwise); a close waits for the
 * handle's other I/O.
 */
bool fiosChunkIsReady(uint16_t chunkID, bool force)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	bool ready = false;
	uint16_t childID;
	int i;

	if (pChunk->scheduled || force)
		ready = true;
	if (!ready)
		return ready;
	i = 0;
	do {
		childID = fiosChunkGetChild(chunkID, i);
		if (childID == CHUNKID_INVALID) {
			uint8_t type = pChunk->type;

			if ((type > CHUNK_FH_OPEN && type < CHUNK_FH_CLOSE) || type == CHUNK_FH_CLOSE) {
				FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(fiosChunkGetHandle(chunkID));

				if (pFH == NULL || pFH->isOpen) {
					if (pChunk->type == CHUNK_FH_CLOSE && pFH != NULL &&
					    pFH->pendingIO > 0) {
						FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
							   "%-15s %d pending I/Os: %s", "Defer",
							   pFH->pendingIO - 1,
							   fiosDebugDumpChunk(chunkID, NULL, 0));
						ready = false;
					}
				} else {
					FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
						   "%-15s file not open yet: %s", "Defer",
						   fiosDebugDumpChunk(chunkID, NULL, 0));
					ready = false;
				}
			} else if ((type > CHUNK_DH_OPEN && type < CHUNK_DH_CLOSE) ||
				   type == CHUNK_DH_CLOSE) {
				FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(fiosChunkGetHandle(chunkID));

				if (pDH == NULL || pDH->isOpen) {
					if (pChunk->type == CHUNK_DH_CLOSE && pDH != NULL &&
					    pDH->pendingIO > 0) {
						FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
							   "%-15s %d pending I/Os: %s", "Defer",
							   pDH->pendingIO - 1,
							   fiosDebugDumpChunk(chunkID, NULL, 0));
						ready = false;
					}
				} else {
					FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
						   "%-15s dir not open yet: %s", "Defer",
						   fiosDebugDumpChunk(chunkID, NULL, 0));
					ready = false;
				}
			}
			break;
		}
		if (!fiosChunkAt(childID)->completed)
			ready = false;
		i++;
	} while (ready);
	return ready;
}

bool fiosChunkIsReadyLocked(uint16_t chunkID, bool force)
{
	bool ready;

	s_chunkMutex.Lock();
	ready = fiosChunkIsReady(chunkID, force);
	s_chunkMutex.Unlock();
	return ready;
}

bool fiosChunkChildrenComplete(uint16_t chunkID)
{
	uint16_t childID;
	int i;

	i = 0;
	while ((childID = fiosChunkGetChild(chunkID, i++)) != CHUNKID_INVALID)
		if (!fiosChunkAt(childID)->completed)
			return false;
	return true;
}

/* the error goes to the ops and callbacks above the chunk that have none (any, with force) */
void fiosChunkSetError(uint16_t chunkID, int err, bool force)
{
	uint16_t parentID;
	int i;

	i = 0;
	while ((parentID = fiosChunkGetParent(chunkID, i++)) != CHUNKID_INVALID) {
		FiosChunk *pParent = fiosChunkAt(parentID);

		if (pParent->type == CHUNK_ROOT) {
			if (pParent->op != 0) {
				FiosOp *pOp = (FiosOp *)s_pOpTable->Lookup(pParent->op);

				if (force || pOp->err == 0)
					pOp->err = err;
			}
		} else if (pParent->type == CHUNK_CALLBACK && (force || pParent->callback.err == 0)) {
			pParent->callback.err = err;
		}
		fiosChunkSetError(parentID, err, force);
	}
}

/* true for a root chunk */
bool fiosChunkMarkComplete(uint16_t chunkID, int err)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);

	pChunk->executing = 0;
	pChunk->completed = 1;
	if (err != 0)
		fiosChunkSetError(chunkID, err, false);
	return pChunk->type == CHUNK_ROOT;
}

bool fiosChunkComplete(uint16_t chunkID, int err)
{
	bool isRoot = false;

	FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s %s", "Complete",
		   fiosDebugDumpChunk(chunkID, NULL, 0));
	if (chunkID != CHUNKID_INVALID &&
	    fiosChunkAt(chunkID)->magic == FIOS_CHUNK_MAGIC)
		isRoot = fiosChunkAt(chunkID)->type == CHUNK_ROOT;
	ChunkLock lock(!isRoot);
	ChunkLock rootLock(isRoot);
	return fiosChunkMarkComplete(chunkID, err);
}

void fiosChunkSetErrorLocked(uint16_t chunkID, int err)
{
	s_chunkMutex.Lock();
	fiosChunkSetError(chunkID, err, false);
	s_chunkMutex.Unlock();
}

/*
 * Ready the chunk and its subtree to run: a read or a write counts its pieces, and the file
 * or directory it works on counts it as pending I/O.
 */
void fiosChunkPrepare(uint16_t chunkID, SceFiosPriority priority, SceFiosTime deadline)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	uint16_t childID;
	uint8_t type;
	int i;

	if (pChunk->prepared)
		return;
	pChunk->prepared = 1;
	pChunk->priority = priority;
	pChunk->deadline = deadline;
	pChunk->split = 0;
	pChunk->pieces = 1;
	type = pChunk->type;
	if (type > CHUNK_FH_OPEN && type < CHUNK_FH_CLOSE) {
		FiosFH *pFH;

		if (type == CHUNK_FH_READ || type == CHUNK_FH_WRITE) {
			pChunk->split = 1;
			pChunk->pieces = (uint32_t)((pChunk->reqCount + FIOS_CHUNK_PIECE_SIZE - 1) /
						    FIOS_CHUNK_PIECE_SIZE);
			if (pChunk->pieces == 0)
				pChunk->pieces = 1;
		}
		pFH = (FiosFH *)s_pFHTable->Lookup(fiosChunkGetHandle(chunkID));
		if (pFH != NULL) {
			int pending = sceAtomicIncrement32Relaxed(&pFH->pendingIO) + 1;

			FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
				   "%-15s Chunk %d incremented m_pendingIO = %d", "FH", chunkID,
				   pending);
		}
	} else if (type > CHUNK_DH_OPEN && type < CHUNK_DH_CLOSE) {
		FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(fiosChunkGetHandle(chunkID));

		if (pDH != NULL) {
			int pending = sceAtomicIncrement32Relaxed(&pDH->pendingIO) + 1;

			FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
				   "%-15s Chunk %d incremented m_pendingIO = %d", "FH", chunkID,
				   pending);
		}
	}
	i = 0;
	while ((childID = fiosChunkGetChild(chunkID, i++)) != CHUNKID_INVALID)
		fiosChunkPrepare(childID, priority, deadline);
}

void fiosChunkPrepareLocked(uint16_t chunkID, SceFiosPriority priority, SceFiosTime deadline)
{
	s_chunkMutex.Lock();
	fiosChunkPrepare(chunkID, priority, deadline);
	s_chunkMutex.Unlock();
}

SceFiosHandle fiosChunkGetHandleLocked(uint16_t chunkID)
{
	SceFiosHandle h;

	s_chunkMutex.Lock();
	h = fiosChunkGetHandle(chunkID);
	s_chunkMutex.Unlock();
	return h;
}

/* the path the chunk works on */
FiosPath *fiosChunkGetPath(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);

	switch (pChunk->type) {
	case CHUNK_FH_OPEN:
	case CHUNK_DH_OPEN:
	case CHUNK_DIR_CREATE:
	case CHUNK_DIR_DELETE:
	case CHUNK_FILE_DELETE:
	case CHUNK_DELETE:
	case CHUNK_RESOLVE:
	case CHUNK_ARCHIVE_MOUNT:
		return pChunk->pPath;
	case CHUNK_DIR_EXISTS:
	case CHUNK_FILE_EXISTS:
	case CHUNK_FILE_SIZE:
	case CHUNK_EXISTS:
	case CHUNK_RENAME:
		return pChunk->query.pPath;
	case CHUNK_STAT:
		return pChunk->stat.pPath;
	case CHUNK_TRUNCATE:
		return pChunk->truncate.pPath;
	default:
		return NULL;
	}
}

FiosPath *fiosChunkGetPathLocked(uint16_t chunkID)
{
	FiosPath *pPath;

	s_chunkMutex.Lock();
	pPath = fiosChunkGetPath(chunkID);
	s_chunkMutex.Unlock();
	return pPath;
}

int fiosChunkInit(SceFiosBuffer storage)
{
	int ret;

	ret = s_chunkAllocator.Init(storage, 0x10000, 8, 8);
	if (ret != SCE_FIOS_OK)
		return ret;
	s_nullChunk = fiosChunkTryAllocate(CHUNK_NULL);
	if (s_nullChunk == CHUNKID_INVALID)
		return SCE_FIOS_ERROR_BAD_SIZE;
	return ret;
}

void fiosChunkTerm()
{
	if (s_nullChunk != CHUNKID_INVALID) {
		fiosChunkFree(s_nullChunk);
		s_nullChunk = CHUNKID_INVALID;
	}
	if (g_FiosParams.profiling & SCE_FIOS_PROFILE_CHUNK) {
		uint32_t highWater = ((s_chunkAllocator.m_count + 7) >> 3) +
				     s_chunkAllocator.m_maxAllocated * 8;

		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
			   "%-15s chunkStorage high water mark = %d bytes (%d%%)", "Terminate",
			   highWater, highWater * 100 / g_FiosParams.chunkStorage.length);
	}
	s_chunkAllocator.Term();
}
