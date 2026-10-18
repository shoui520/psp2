#pragma once

#include <stddef.h>
#include <scetypes.h>

struct AppmgrWaitObject {
	SceUID eventFlag;
	SceUInt32 bits;
	struct AppmgrWaitObject *next;
};

_Static_assert(sizeof(struct AppmgrWaitObject) == 12, "AppMgr wait object");
_Static_assert(offsetof(struct AppmgrWaitObject, eventFlag) == 0 &&
	       offsetof(struct AppmgrWaitObject, bits) == 4 &&
	       offsetof(struct AppmgrWaitObject, next) == 8,
	       "AppMgr wait object fields");

int appmgrUtilWaitObjectInitialize(void);
struct AppmgrWaitObject *appmgrUtilGetWaitObject(int index);
void appmgrUtilWaitObjectClear(struct AppmgrWaitObject *waitObject);
void appmgrUtilWaitObjectNotify(struct AppmgrWaitObject *waitObject);
int appmgrUtilWaitObjectWait(struct AppmgrWaitObject *waitObject, SceUInt32 *timeout);
