#include "mp4_internal.h"

static Mp4ListNode<Mp4Atom *> *mp4AtomListGetAt(Mp4ListNode<Mp4Atom *> *node, uint32_t curIdx, uint32_t targetIdx)
{
	while (curIdx != targetIdx) {
		if (curIdx > targetIdx) {
			node = node->prev;
			curIdx--;
		} else {
			node = node->next;
			curIdx++;
		}
	}
	return node;
}

static Mp4ListNode<Mp4Atom *> *mp4AtomListFindPrev(Mp4ListNode<Mp4Atom *> *head, Mp4ListNode<Mp4Atom *> *target)
{
	Mp4ListNode<Mp4Atom *> *cur = head;
	while (cur != target)
		cur = cur->next;
	return cur->prev;
}

static Mp4Atom *mp4AtomListElem(const Mp4List<Mp4Atom *> *list, uint32_t index)
{
	if (index >= list->count)
		return list->sentinel->value;
	if (index >= (list->count >> 1))
		return mp4AtomListGetAt(list->tail, list->count - 1, index)->value;
	return mp4AtomListGetAt(list->head, 0, index)->value;
}

static void mp4AtomListPushBack(Mp4List<Mp4Atom *> *list, Mp4Atom *atom)
{
	sceKernelLockLwMutex(&list->mutex, 1, NULL);
	Mp4ListNode<Mp4Atom *> *node = (Mp4ListNode<Mp4Atom *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4Atom *>));
	if (node != NULL) {
		node->prev = NULL;
		node->next = NULL;
		sceClibMemcpy_safe(&node->value, &atom, sizeof(atom));
	}
	if (list->tail != NULL) {
		list->tail->next = node;
		node->prev = list->tail;
	} else {
		list->head = node;
	}
	list->tail = node;
	list->count++;
	list->totalBytes += sizeof(Mp4ListNode<Mp4Atom *>);
	sceKernelUnlockLwMutex(&list->mutex, 1);
}

Mp4Atom::Mp4Atom(Mp4DataReader *inReader, int64_t offset, int64_t size, uint32_t fourcc, int headerSize, SceAvPlayerMemAllocator *inAllocator)
{
	SceKernelLwMutexOptParam opt;
	Mp4Atom *nullAtom = NULL;

	this->allocator = inAllocator;
	this->reader = inReader;
	this->atomOffset = offset;
	this->atomSize = size;
	this->atomHeaderSize = headerSize;
	this->atomFourcc = fourcc;

	this->children.head = NULL;
	this->children.tail = NULL;
	this->children.count = 0;
	this->children.totalBytes = 0;
	this->children.isShallowCopy = 0;
	this->children.beginIter = NULL;
	this->children.endIter = NULL;

	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	sceKernelCreateLwMutex(&this->children.mutex, "0Demux_Stl_Handler_Mutex ", 0, 0, &opt);

	Mp4ListNode<Mp4Atom *> *sentinel = (Mp4ListNode<Mp4Atom *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4Atom *>));
	if (sentinel != NULL) {
		sentinel->prev = NULL;
		sentinel->next = NULL;
		sceClibMemcpy_safe(&sentinel->value, &nullAtom, sizeof(nullAtom));
	}
	this->children.sentinel = sentinel;
	this->mappedData = NULL;
	this->mapRefCount = 0;
	this->parsedOffset = 0;
}

Mp4Atom::~Mp4Atom()
{
	for (uint32_t i = 0; i < this->children.count; i++) {
		Mp4Atom *child = mp4AtomListElem(&this->children, i);
		if (child != NULL)
			delete child;
	}

	while (this->children.count != 0) {
		sceKernelLockLwMutex(&this->children.mutex, 1, NULL);
		if (this->children.count != 0 && this->children.tail != NULL) {
			Mp4ListNode<Mp4Atom *> *tail = this->children.tail;
			Mp4ListNode<Mp4Atom *> *prev = mp4AtomListFindPrev(tail, tail);
			if (prev != NULL)
				prev->next = NULL;
			mp4MspaceFree(tail);
			this->children.tail = prev;
			this->children.count--;
			this->children.totalBytes -= sizeof(Mp4ListNode<Mp4Atom *>);
		}
		sceKernelUnlockLwMutex(&this->children.mutex, 1);
	}

	if (this->children.isShallowCopy == 0) {
		if (this->children.beginIter != NULL)
			delete this->children.beginIter;
		if (this->children.endIter != NULL)
			delete this->children.endIter;
		if (this->children.sentinel != NULL)
			mp4MspaceFree(this->children.sentinel);
	}

	sceKernelDeleteLwMutex(&this->children.mutex);
}

int Mp4Atom::readData(int64_t relOffset, uint32_t length, void *dst)
{
	if (this->isLoaded() != 0) {
		uint8_t *buf = this->mapBuffer();
		if (buf == NULL)
			return MP4_ERR_NO_MEMORY;
		sceClibMemcpy_safe(dst, buf + (uint32_t)relOffset, length);
		this->unmapBuffer();
		return MP4_OK;
	}

	for (uint32_t retry = 0; ; retry++) {
		if (this->reader == NULL)
			return MP4_ERR_NO_MEMORY;
		int err = this->reader->read(this->atomOffset + relOffset, length, (uint8_t *)dst);
		if (err != MP4_ERR_BUFFER_UNDERRUN || retry > 100)
			return err;
		sceKernelDelayThread(900);
	}
}

int Mp4Atom::getChildCount()
{
	if (this->children.count == 0)
		this->parseChildren(0);
	return (int)this->children.count;
}

Mp4Atom *Mp4Atom::getChildAt(int index)
{
	if (index < 0 || index >= this->getChildCount())
		return NULL;
	return mp4AtomListElem(&this->children, (uint32_t)index);
}

int Mp4Atom::countChildrenByType(uint32_t fourcc)
{
	int total = this->getChildCount();
	int matches = 0;
	for (int i = 0; i < total; i++) {
		Mp4Atom *child = mp4AtomListElem(&this->children, (uint32_t)i);
		if (child != NULL && child->getFourCC() == fourcc)
			matches++;
	}
	return matches;
}

Mp4Atom *Mp4Atom::findChildByType(uint32_t fourcc, int occurrence)
{
	int total = this->getChildCount();
	int matches = 0;
	for (int i = 0; i < total; i++) {
		Mp4Atom *child = mp4AtomListElem(&this->children, (uint32_t)i);
		if (child != NULL && child->getFourCC() == fourcc) {
			if (matches == occurrence)
				return child;
			matches++;
		}
	}
	return NULL;
}

Mp4Atom *Mp4Atom::findChildByTypeAtOrAfter(uint32_t fourcc, uint32_t minOccurrence)
{
	int total = this->getChildCount();
	uint32_t matches = 0;
	for (int i = 0; i < total; i++) {
		Mp4Atom *child = mp4AtomListElem(&this->children, (uint32_t)i);
		if (child != NULL && child->getFourCC() == fourcc) {
			if (matches >= minOccurrence)
				return child;
			matches++;
		}
	}
	return NULL;
}

int64_t Mp4Atom::getChildOffsetByType(uint32_t fourcc, uint32_t minOccurrence)
{
	Mp4Atom *child = this->findChildByTypeAtOrAfter(fourcc, minOccurrence);
	if (child == NULL)
		return -1;
	return child->getOffset();
}

int Mp4Atom::isLoaded()
{
	if ((int32_t)this->mapRefCount > 0)
		return 1;
	if (this->reader != NULL)
		return this->reader->isStreaming();
	return 0;
}

uint8_t *Mp4Atom::mapBuffer()
{
	if (this->atomSize > 0x7FFFFFFFLL)
		return NULL;

	if (this->mappedData == NULL) {
		if (this->reader != NULL && this->reader->isStreaming() != 0 && this->atomOffset < 0x7FFFFFFFLL)
			return (uint8_t *)((uintptr_t)this->reader->getBufferSize() + (uint32_t)this->atomOffset);

		uint8_t *raw;
		if (this->atomSize <= 1024) {
			raw = (uint8_t *)mp4MspaceAllocZero(1, (SceSize)(this->atomSize + 1));
			if (raw == NULL)
				return NULL;
			raw[0] = MP4_TAG_MSPACE_DE;
		} else {
			raw = (uint8_t *)this->allocator->allocate(this->allocator->objectPointer, 1, (uint32_t)(this->atomSize + 1));
			if (raw == NULL)
				return NULL;
			raw[0] = MP4_TAG_EXT_ALLOC_DD;
		}
		this->mappedData = raw + 1;
		if (this->readData(0, (uint32_t)this->atomSize, this->mappedData) != MP4_OK)
			return NULL;
	}

	mp4AtomicInc32(&this->mapRefCount);
	return this->mappedData;
}

int Mp4Atom::unmapBuffer()
{
	if ((int32_t)this->mapRefCount <= 0)
		return (int)this->mapRefCount;

	mp4AtomicDec32(&this->mapRefCount);
	if (this->mapRefCount == 0 && this->mappedData != NULL) {
		uint8_t *raw = this->mappedData - 1;
		uint8_t tag = raw[0];
		if (tag == MP4_TAG_EXT_ALLOC_DD)
			this->allocator->deallocate(this->allocator->objectPointer, raw);
		else if (tag == MP4_TAG_MSPACE_DE)
			mp4MspaceFree(raw);
		this->mappedData = NULL;
		return 0;
	}
	return (int)this->mapRefCount;
}

void Mp4Atom::parseChildren(int64_t startOffset)
{
	int64_t pos = startOffset + this->atomHeaderSize;
	bool sawMoov = false;
	bool sawMdat = false;
	uint32_t childFourcc = 0;

	while (pos < this->atomSize) {
		uint8_t hdr[8];
		sceClibMemset(hdr, 0, sizeof(hdr));
		if (this->readData(pos, 8, hdr) != MP4_OK)
			break;

		int64_t childSize = (int32_t)mp4ReadBe32(hdr);
		childFourcc = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
		              ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
		int hdrSize = 8;

		if (childSize == 1) {
			uint8_t extSize[8];
			if (this->readData(pos + 8, 8, extSize) != MP4_OK)
				break;
			childSize = (int64_t)mp4ReadBe64(extSize);
			hdrSize = 16;
		} else if (childSize == 0) {
			childSize = this->atomSize - pos;
		}

		if (childFourcc == FOURCC_MOOV)
			sawMoov = true;
		if (childFourcc == FOURCC_MDAT)
			sawMdat = true;

		if (childSize < 0 || pos + childSize > this->atomSize)
			break;

		Mp4Atom *child = new Mp4Atom((Mp4DataReader *)this, pos, childSize, childFourcc, hdrSize, this->allocator);
		if (child == NULL)
			break;
		mp4AtomListPushBack(&this->children, child);

		pos += childSize;
		if (sawMoov && sawMdat)
			break;
		if (childFourcc == FOURCC_MOOF)
			break;
	}

	this->parsedOffset = pos;
}
