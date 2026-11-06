/*
 * The glyph cache the PVF library keeps rendered glyphs in: the SDK's libpvf sample font cache
 * (fontcache.c), in paf's revision. A slot keeps its buffers for the next glyph of the same size,
 * and a conflicting or failed slot is only marked empty; its buffers are freed with the cache.
 */
#include <stddef.h>
#include <std/stdlib.h>
#include "fontcache.h"

#define MY_FALSE	(0)
#define MY_TRUE		(!MY_FALSE)

typedef struct {
	unsigned long touchedTime;	/* the cache's time of the last use, 0: empty */
	ScePvfCacheKey key;
	int data0Size;			/* -1: no buffer */
	void *data0;
	int data1Size;			/* -1: no buffer */
	void *data1;
	unsigned int bHasData0;
	unsigned int bHasData1;
} myPvfCache_t_Slot;

typedef struct {
	ScePvf_t_cacheSystemInterface interface;
	myPvfCache_t_initRec initRec;
	unsigned long currentTime;
	int numCacheSlots;
	int locked;
	myPvfCache_t_Slot *pSlots;
} myPvfCache_t_Rec;

static void initAllCacheSlots(myPvfCache_t_Rec *_this_);
static void initCacheSlotN(myPvfCache_t_Rec *_this_, int N);
static void freeAllCacheSlots(myPvfCache_t_Rec *_this_);
static void freeACacheSlot(myPvfCache_t_Rec *_this_, myPvfCache_t_Slot *slot);
static int compareKey(ScePvfCacheKey *keyA, ScePvfCacheKey *keyB);
static ScePvf_t_s32 cb_lockFunc(ScePvf_t_pointer t);
static ScePvf_t_s32 cb_unlockFunc(ScePvf_t_pointer t);
static ScePvf_t_pointer cb_findFunc(ScePvf_t_pointer t, ScePvf_t_u32 hashValue,
				    ScePvf_t_pointer k, ScePvf_t_bool *hit);
static ScePvf_t_s32 cb_writeKeyValueToCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
						ScePvf_t_pointer k);
static ScePvf_t_s32 cb_write0ToCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					 ScePvf_t_pointer data, ScePvf_t_int dataSize);
static ScePvf_t_s32 cb_write1ToCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					 ScePvf_t_pointer data, ScePvf_t_int dataSize);
static ScePvf_t_s32 cb_read0FromCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					  ScePvf_t_pointer dst);
static ScePvf_t_s32 cb_read1FromCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					  ScePvf_t_pointer dst);

void *myPvfCache_new(myPvfCache_t_initRec *pInitRec, int numCacheSlots)
{
	myPvfCache_t_Rec *_new_ = NULL;

	if (!pInitRec)
		return NULL;
	if (!pInitRec->allocFunc || !pInitRec->freeFunc || !pInitRec->memcpyFunc)
		return NULL;
	_new_ = (myPvfCache_t_Rec *)pInitRec->allocFunc(sizeof(myPvfCache_t_Rec));
	if (!_new_)
		return NULL;
	_new_->locked = MY_TRUE;
	_new_->initRec = *pInitRec;
	_new_->numCacheSlots = numCacheSlots;
	_new_->currentTime = 0;
	_new_->pSlots = (myPvfCache_t_Slot *)_new_->initRec.allocFunc(
		sizeof(_new_->pSlots[0]) * _new_->numCacheSlots);
	if (!_new_->pSlots) {
		_new_->initRec.freeFunc(_new_);
		return NULL;
	}
	initAllCacheSlots(_new_);
	_new_->interface.cacheInstance = (ScePvf_t_pointer *)_new_;
	_new_->interface.lockFunc = cb_lockFunc;
	_new_->interface.unlockFunc = cb_unlockFunc;
	_new_->interface.findFunc = cb_findFunc;
	_new_->interface.writeKeyValueToCacheFunc = cb_writeKeyValueToCacheFunc;
	_new_->interface.write0ToCacheFunc = cb_write0ToCacheFunc;
	_new_->interface.write1ToCacheFunc = cb_write1ToCacheFunc;
	_new_->interface.read0FromCacheFunc = cb_read0FromCacheFunc;
	_new_->interface.read1FromCacheFunc = cb_read1FromCacheFunc;
	_new_->locked = MY_FALSE;
	return _new_;
}

ScePvf_t_cacheSystemInterface *myPvfCache_getCacheInterfaceHandle(void *_this_)
{
	if (!_this_)
		return NULL;
	return &((myPvfCache_t_Rec *)_this_)->interface;
}

int myPvfCache_expireAll(void *_this_, int bCompulsion)
{
	if (!bCompulsion) {
		/* not while libpvf holds the cache */
		if (((myPvfCache_t_Rec *)_this_)->locked)
			return -1;
		((myPvfCache_t_Rec *)_this_)->locked = MY_TRUE;
	}
	freeAllCacheSlots((myPvfCache_t_Rec *)_this_);
	if (!bCompulsion)
		((myPvfCache_t_Rec *)_this_)->locked = MY_FALSE;
	return 0;
}

void myPvfCache_delete(void *_this_)
{
	if (!_this_)
		return;
	freeAllCacheSlots((myPvfCache_t_Rec *)_this_);
	((myPvfCache_t_Rec *)_this_)->initRec.freeFunc(((myPvfCache_t_Rec *)_this_)->pSlots);
	((myPvfCache_t_Rec *)_this_)->initRec.freeFunc(_this_);
}

static void initAllCacheSlots(myPvfCache_t_Rec *_this_)
{
	int i;

	if (!_this_)
		return;
	if (!_this_->pSlots)
		return;
	for (i = 0; i < _this_->numCacheSlots; i++)
		initCacheSlotN(_this_, i);
}

static void initCacheSlotN(myPvfCache_t_Rec *_this_, int N)
{
	myPvfCache_t_Slot *pSlot;

	if (!_this_)
		return;
	if (!_this_->pSlots)
		return;
	pSlot = &_this_->pSlots[N];
	pSlot->touchedTime = 0;
	pSlot->data0Size = -1;
	pSlot->data0 = NULL;
	pSlot->data1Size = -1;
	pSlot->data1 = NULL;
	pSlot->key.keyValue0 = 0;
	pSlot->key.keyValue1 = 0;
	pSlot->key.keyValue2 = 0.0f;
	pSlot->key.keyValue3 = 0.0f;
	pSlot->key.keyValue6 = 0.0f;
	pSlot->key.keyValue7 = 0.0f;
	pSlot->key.keyValue8 = 0.0f;
	pSlot->key.keyValue4 = 0.0f;
	pSlot->key.keyValue5 = 0.0f;
	pSlot->bHasData0 = 0;
	pSlot->bHasData1 = 0;
}

static void freeAllCacheSlots(myPvfCache_t_Rec *_this_)
{
	int i;

	if (!_this_)
		return;
	if (!_this_->pSlots)
		return;
	for (i = 0; i < _this_->numCacheSlots; i++)
		freeACacheSlot(_this_, &_this_->pSlots[i]);
}

static void freeACacheSlot(myPvfCache_t_Rec *_this_, myPvfCache_t_Slot *slot)
{
	if (!_this_)
		return;
	if (!slot)
		return;
	if (slot->data0) {
		_this_->initRec.freeFunc(slot->data0);
		slot->data0 = NULL;
	}
	slot->data0Size = -1;
	slot->bHasData0 = 0;
	if (slot->data1) {
		_this_->initRec.freeFunc(slot->data1);
		slot->data1 = NULL;
	}
	slot->bHasData1 = 0;
	slot->data1Size = -1;
	slot->touchedTime = 0;
}

static int compareKey(ScePvfCacheKey *keyA, ScePvfCacheKey *keyB)
{
	if (keyA->keyValue0 != keyB->keyValue0)
		return 1;
	if (keyA->keyValue1 != keyB->keyValue1)
		return 1;
	if (keyA->keyValue2 != keyB->keyValue2)
		return 1;
	if (keyA->keyValue3 != keyB->keyValue3)
		return 1;
	if (keyA->keyValue6 != keyB->keyValue6)
		return 1;
	if (keyA->keyValue7 != keyB->keyValue7)
		return 1;
	if (keyA->keyValue8 != keyB->keyValue8)
		return 1;
	if (keyA->keyValue4 != keyB->keyValue4)
		return 1;
	if (keyA->keyValue5 != keyB->keyValue5)
		return 1;
	return 0;
}

static ScePvf_t_s32 cb_lockFunc(ScePvf_t_pointer t)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;

	if (_this_->locked) {
		sce_paf_printf_dummy();
		return -1;
	}
	_this_->locked = MY_TRUE;
	return 0;
}

static ScePvf_t_s32 cb_unlockFunc(ScePvf_t_pointer t)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;

	if (!_this_->locked) {
		sce_paf_printf_dummy();
		return -1;
	}
	_this_->locked = MY_FALSE;
	return 0;
}

static ScePvf_t_pointer cb_findFunc(ScePvf_t_pointer t, ScePvf_t_u32 hashValue,
				    ScePvf_t_pointer k, ScePvf_t_bool *hit)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;
	ScePvfCacheKey *key = (ScePvfCacheKey *)k;
	myPvfCache_t_Slot *slot;

	if (!hit)
		return NULL;
	*hit = SCE_PVF_FALSE;
	if (!_this_)
		return NULL;
	if (!key)
		return NULL;
	slot = &_this_->pSlots[hashValue % _this_->numCacheSlots];
	/* nothing cached in it yet */
	if (slot->touchedTime == 0)
		return slot;
	/* another glyph has the slot: it is taken over, keeping its buffers */
	if (compareKey(key, &slot->key) != 0) {
		slot->bHasData0 = 0;
		slot->bHasData1 = 0;
		slot->touchedTime = 0;
		return slot;
	}
	slot->touchedTime = ++_this_->currentTime;
	*hit = SCE_PVF_TRUE;
	return slot;
}

static ScePvf_t_s32 cb_writeKeyValueToCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
						ScePvf_t_pointer k)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;
	myPvfCache_t_Slot *slot = (myPvfCache_t_Slot *)s;
	ScePvfCacheKey *key = (ScePvfCacheKey *)k;

	if (!_this_)
		return -1;
	if (!slot)
		return -1;
	if (!key)
		return -1;
	slot->key = *key;
	slot->touchedTime = ++_this_->currentTime;
	return 0;
}

static ScePvf_t_s32 cb_write0ToCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					 ScePvf_t_pointer data, ScePvf_t_int dataSize)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;
	myPvfCache_t_Slot *slot = (myPvfCache_t_Slot *)s;

	if (!_this_)
		return -1;
	if (!slot)
		return -1;
	/* a buffer of the same size is reused */
	if (slot->data0Size != dataSize) {
		if (slot->data0)
			_this_->initRec.freeFunc(slot->data0);
		slot->data0 = _this_->initRec.allocFunc(dataSize);
		if (!slot->data0) {
			slot->data0Size = -1;
			slot->bHasData0 = 0;
			slot->bHasData1 = 0;
			slot->touchedTime = 0;
			return -1;
		}
	}
	slot->bHasData0 = 1;
	_this_->initRec.memcpyFunc((unsigned char *)slot->data0, (unsigned char *)data,
				   (int)dataSize);
	slot->data0Size = dataSize;
	slot->touchedTime = ++_this_->currentTime;
	return 0;
}

static ScePvf_t_s32 cb_write1ToCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					 ScePvf_t_pointer data, ScePvf_t_int dataSize)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;
	myPvfCache_t_Slot *slot = (myPvfCache_t_Slot *)s;

	if (!_this_)
		return -1;
	if (!slot)
		return -1;
	/* a buffer of the same size is reused */
	if (slot->data1Size != dataSize) {
		if (slot->data1)
			_this_->initRec.freeFunc(slot->data1);
		slot->data1 = _this_->initRec.allocFunc(dataSize);
		if (!slot->data1) {
			slot->data1Size = -1;
			slot->bHasData0 = 0;
			slot->bHasData1 = 0;
			slot->touchedTime = 0;
			return -1;
		}
	}
	slot->bHasData1 = 1;
	_this_->initRec.memcpyFunc((unsigned char *)slot->data1, (unsigned char *)data,
				   (int)dataSize);
	slot->data1Size = dataSize;
	slot->touchedTime = ++_this_->currentTime;
	return 0;
}

static ScePvf_t_s32 cb_read0FromCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					  ScePvf_t_pointer dst)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;
	myPvfCache_t_Slot *slot = (myPvfCache_t_Slot *)s;

	if (!_this_)
		return -1;
	if (!slot)
		return -1;
	if (!slot->data0)
		return -1;
	if (!slot->bHasData0)
		return -1;
	_this_->initRec.memcpyFunc((unsigned char *)dst, (unsigned char *)slot->data0,
				   (int)slot->data0Size);
	slot->touchedTime = ++_this_->currentTime;
	return 0;
}

static ScePvf_t_s32 cb_read1FromCacheFunc(ScePvf_t_pointer t, ScePvf_t_pointer s,
					  ScePvf_t_pointer dst)
{
	myPvfCache_t_Rec *_this_ = (myPvfCache_t_Rec *)t;
	myPvfCache_t_Slot *slot = (myPvfCache_t_Slot *)s;

	if (!_this_)
		return -1;
	if (!slot)
		return -1;
	if (!slot->data1)
		return -1;
	if (!slot->bHasData1)
		return -1;
	_this_->initRec.memcpyFunc((unsigned char *)dst, (unsigned char *)slot->data1,
				   (int)slot->data1Size);
	slot->touchedTime = ++_this_->currentTime;
	return 0;
}
