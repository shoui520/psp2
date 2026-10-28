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
void GestureUserDataInitialize(void);

#define GestureListenerIsValidUserData ScePafMisc_83D0B118
unsigned char GestureListenerIsValidUserData(void *data);

#endif
