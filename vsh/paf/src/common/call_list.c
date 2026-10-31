/* Callback dispatch, including removal of the callback being dispatched. */
#include "call_list.h"

/* operator new/delete and the allocation-failure handler in ScePafStdc. */
extern void *ScePafStdc_FC5CD359(size_t size);
extern void ScePafStdc_1B77082E(void *ptr);
extern int ScePafStdc_D608246A(void);

/* SNC's placement-new overload: the size is ignored, the address is returned. */
#define PafPlacementNew _ZnwjPv
extern void *PafPlacementNew(size_t size, void *where);

void CallListRegister(CallList *self, Function entry, void *userdata)
{
	CallItem item = { entry, userdata };

	CallListPushBack(&self->m_list, &item);
}

void CallListUnregister(CallList *self, Function entry, void *userdata)
{
	CallItemNode *root = self->m_list.root_node;
	CallItemNode *item = root ? root->next : NULL;

	if (item == root)
		return;
	for (;;) {
		if (item->val.m_func == entry && item->val.m_data == userdata)
			break;
		if (item)
			item = item->next;
		if (item == root)
			return;
	}

	if (item == self->m_iterator) {
		if (!self->m_iterator_updated)
			_ZN3paf6thread6RMutex4LockEv(&self->m_item_mtx);
		CallItemNode *next = item->next;
		next->prev = item->prev;
		item->prev->next = item->next;
		ScePafStdc_1B77082E(item);
		--self->m_list.count;
		self->m_iterator = next;
		if (!self->m_iterator_updated) {
			_ZN3paf6thread6RMutex6UnlockEv(&self->m_item_mtx);
			self->m_iterator_updated = 1;
		}
	} else {
		item->next->prev = item->prev;
		item->prev->next = item->next;
		ScePafStdc_1B77082E(item);
		--self->m_list.count;
	}
}

unsigned int CallListGetNum(CallList *self)
{
	return self->m_list.count;
}

void CallListUpdate(CallList *self, Mutex *extMtx)
{
	CallItemNode *item = self->m_list.root_node;

	if (item)
		item = item->next;
	self->m_iterator = item;
	while (item != self->m_list.root_node) {
		_ZN3paf6thread6RMutex4LockEv(&self->m_item_mtx);
		if (extMtx)
			_ZN3paf6thread5Mutex6UnlockEv(extMtx);
		item->val.m_func(item->val.m_data);
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_item_mtx);
		if (extMtx)
			_ZN3paf6thread5Mutex4LockEv(extMtx);
		item = self->m_iterator;
		if (self->m_iterator_updated) {
			self->m_iterator_updated = 0;
		} else if (item) {
			item = item->next;
			self->m_iterator = item;
		}
	}
}

static inline __attribute__((always_inline)) CallList *CallListInitialize(CallList *self)
{
	self->m_list.count = 0;
	CallItemNode *root = ScePafStdc_FC5CD359(sizeof(*root));
	if (!root)
		ScePafStdc_D608246A();
	self->m_list.root_node = root;
	if (root) {
		root->prev = root;
		root->next = root;
		root = self->m_list.root_node;
	}
	self->m_iterator = root;
	self->m_iterator_updated = 0;
	_ZN3paf6thread6RMutexC1EPKci(&self->m_item_mtx, "ScePafCallListItem", 0);
	return self;
}

CallList *CallListConstruct(CallList *self)
{
	return CallListInitialize(self);
}

CallList *CallListConstructBase(CallList *self)
{
	return CallListInitialize(self);
}

void CallListPushBack(CallItemList *list, const CallItem *item)
{
	if (!list->root_node)
		return;

	CallItemNode *node = ScePafStdc_FC5CD359(sizeof(*node));
	if (!node)
		ScePafStdc_D608246A();
	if (node) {
		CallItem *val = PafPlacementNew(sizeof(*val), &node->val);
		if (val) {
			val->m_func = item->m_func;
			val->m_data = item->m_data;
		}
	} else {
		node = list->root_node;
	}
	if (node != list->root_node) {
		node->prev = list->root_node->prev;
		node->next = list->root_node;
		list->root_node->prev->next = node;
		list->root_node->prev = node;
		++list->count;
	}
}
