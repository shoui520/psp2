#ifndef PAF_INPUTDEVICE_INPUTLISTENER_H
#define PAF_INPUTDEVICE_INPUTLISTENER_H

#include <stdint.h>

typedef struct InputListener InputListener;

typedef struct InputListenerRefCounter {
	InputListener *m_ptr;
	int32_t m_counter;
	int32_t m_weak_counter;
	void (*m_deleter)(InputListener *ptr);
} InputListenerRefCounter;

typedef struct InputListenerSharedPtr {
	InputListener *m_ptr;
	InputListenerRefCounter *m_ref_counter;
} InputListenerSharedPtr;

typedef struct InputListenerNode {
	struct InputListenerNode *prev;
	struct InputListenerNode *next;
	InputListenerSharedPtr val;
} InputListenerNode;

typedef struct InputListenerList {
	InputListenerNode *root_node;
	int32_t count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} InputListenerList;

typedef char InputListenerRefCounterSizeCheck[
	sizeof(InputListenerRefCounter) == 16 ? 1 : -1];
typedef char InputListenerNodeSizeCheck[sizeof(InputListenerNode) == 16 ? 1 : -1];
typedef char InputListenerListSizeCheck[sizeof(InputListenerList) == 12 ? 1 : -1];

InputListenerList *InputListenerListConstruct(InputListenerList *self,
					     const void *allocator);
InputListenerList *InputListenerListDestroy(InputListenerList *self);
void InputListenerListInitialize(void);

#endif
