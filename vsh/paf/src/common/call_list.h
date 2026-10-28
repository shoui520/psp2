/* C representation of paf::common::CallList and its list<CallItem> nodes. */
#ifndef PAF_COMMON_CALL_LIST_H
#define PAF_COMMON_CALL_LIST_H

#include "../thread/mutex.h"
#include <stddef.h>

typedef void (*Function)(void *userdata);

typedef struct CallItem {
	Function m_func;
	void *m_data;
} CallItem;

typedef struct CallItemNode {
	struct CallItemNode *prev;
	struct CallItemNode *next;
	CallItem val;
} CallItemNode;

typedef struct CallItemList {
	CallItemNode *root_node;
	unsigned int count;
	/* Each empty C++ allocator still occupies a byte in the list object. */
	unsigned char alloc_value;
	unsigned char alloc_node;
} CallItemList;

typedef struct CallList {
	CallItemList m_list;
	CallItemNode *m_iterator;
	unsigned char m_iterator_updated;
	RMutex m_item_mtx;
} CallList;

typedef char CallItemNodeSizeCheck[sizeof(CallItemNode) == 16 ? 1 : -1];
typedef char CallListSizeCheck[sizeof(CallList) == 56 ? 1 : -1];
typedef char CallListMutexOffsetCheck[offsetof(CallList, m_item_mtx) == 24 ? 1 : -1];

/* C spellings of Sony's C++ ABI entry points. */
#define CallListRegister _ZN3paf6common8CallList8RegisterEPFvPvES2_
#define CallListUnregister _ZN3paf6common8CallList10UnregisterEPFvPvES2_
#define CallListGetNum _ZN3paf6common8CallList6GetNumEv
#define CallListUpdate _ZN3paf6common8CallList6UpdateEPNS_6thread5MutexE
#define CallListConstruct _ZN3paf6common8CallListC1Ev
#define CallListConstructBase _ZN3paf6common8CallListC2Ev

void CallListRegister(CallList *self, Function entry, void *userdata);
void CallListUnregister(CallList *self, Function entry, void *userdata);
unsigned int CallListGetNum(CallList *self);
void CallListUpdate(CallList *self, Mutex *extMtx);
CallList *CallListConstruct(CallList *self);
CallList *CallListConstructBase(CallList *self);

void CallListPushBack(CallItemList *list, const CallItem *item);

#endif
