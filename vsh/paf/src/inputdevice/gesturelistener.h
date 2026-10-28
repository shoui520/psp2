#ifndef PAF_INPUTDEVICE_GESTURELISTENER_H
#define PAF_INPUTDEVICE_GESTURELISTENER_H

#include "../std/map.h"
#include <stdint.h>

typedef struct GestureUserDataEntry {
	void *first;
	uint32_t second;
} GestureUserDataEntry;

typedef struct GestureUserDataNode {
	struct GestureUserDataNode *_Parent;
	struct GestureUserDataNode *_Left;
	struct GestureUserDataNode *_Right;
	GestureUserDataEntry _Myval;
	unsigned char _Color;
	unsigned char _Isnil;
} GestureUserDataNode;

typedef char GestureUserDataNodeSizeCheck[
	sizeof(GestureUserDataNode) == 24 ? 1 : -1];

PafMap *GestureUserDataMapConstruct(PafMap *self, const void *allocator);
PafMap *GestureUserDataMapClear(PafMap *self);
PafMap *GestureUserDataMapDestroy(PafMap *self);
GestureUserDataNode *GestureUserDataMapRebalance(PafMap *self,
					       GestureUserDataNode *node);
GestureUserDataNode *GestureUserDataMapInsert(PafMap *self,
					    const GestureUserDataEntry *value,
					    unsigned char *inserted);
void GestureUserDataInitialize(void);

#define GestureListenerAddUserDataList ScePafMisc_09EF204D
#define GestureListenerIncrUserDataList ScePafMisc_E3359BF6
#define GestureListenerIsValidUserData ScePafMisc_83D0B118
void GestureListenerAddUserDataList(void *data);
void GestureListenerIncrUserDataList(void *data);
unsigned char GestureListenerIsValidUserData(void *data);

#endif
