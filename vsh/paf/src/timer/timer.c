#include "timer.h"
#include "../std/math.h"
#include "../std/new.h"
#include "../std/string.h"

extern int ScePafStdc_D608246A(void);

TimerListenerList *s_default_list;

const TimerListenerClass timerListenerClass = {
	0, NULL, {
		TimerListenerDestroy, TimerListenerDelete,
		TimerListenerOnTick, TimerListenerOnFinish
	}
};

void TimerListenerOnTick(TimerListener *self, void *data1, void *data2)
{
	(void)self;
	(void)data1;
	(void)data2;
}

void TimerListenerOnFinish(TimerListener *self, void *data1, void *data2)
{
	(void)self;
	(void)data1;
	(void)data2;
}

TimerListener *TimerListenerDestroy(TimerListener *self)
{
	Timer *timer = self->m_timer;

	self->vtable = &timerListenerClass.methods;
	if (timer)
		timer->vtable->Delete(timer);
	self->m_timer = NULL;
	return self;
}

void TimerListenerDelete(TimerListener *self)
{
	TimerListenerDestroy(self);
	PafDelete(self);
}

TimerListener *TimerListenerDestroyBase(TimerListener *self)
{
	return TimerListenerDestroy(self);
}

void TimerListenerResetTimer(TimerListener *self)
{
	Timer *timer = self->m_timer;

	if (!timer)
		return;
	if (timer->m_is_3d) {
		Timer3D *timer3d = (Timer3D *)timer;

		TimerResetTimer(timer);
		timer3d->base.m_mapped_y = timer->m_mapped_x;
		timer3d->m_mapped_z = timer->m_mapped_x;
	} else if (timer->m_is_2d) {
		Timer2D *timer2d = (Timer2D *)timer;

		TimerResetTimer(timer);
		timer2d->m_mapped_y = timer->m_mapped_x;
	} else {
		TimerResetTimer(timer);
	}
}

ListenerParam *ListenerParamConstruct(ListenerParam *self, TimerListener *listener,
				      unsigned char interval, void *data1, void *data2)
{
	self->listener = listener;
	self->data1 = data1;
	self->data2 = data2;
	self->interval = interval;
	return self;
}

float TimerListenerDoTick(TimerListener *self, void *data1, void *data2)
{
	Timer *timer = self->m_timer;
	float ret;

	if (!timer)
		return 0.0f;
	if (timer->m_is_3d)
		ret = Timer3DDoTick((Timer3D *)timer);
	else if (timer->m_is_2d)
		ret = Timer2DDoTick((Timer2D *)timer);
	else
		ret = TimerDoTick(timer);
	self->vtable->OnTick(self, data1, data2);
	timer = self->m_timer;
	if (timer->m_state == STATE_HOLD ||
	    (timer->m_state == STATE_FINISHED && timer->m_repeat == REPEAT_NONE))
		self->vtable->OnFinish(self, data1, data2);
	return ret;
}

static void ListenerParamListPushBack(ListenerParamList *list, const ListenerParam *param)
{
	ListenerParamNode *node;

	if (!list->root_node)
		return;
	node = PafNew(sizeof(*node));
	if (!node)
		ScePafStdc_D608246A();
	if (node) {
		ListenerParam *value = PafPlacementNew(sizeof(*value), &node->val);
		if (value)
			*value = *param;
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

static TimerListenerList *TimerListenerListInitialize(TimerListenerList *self)
{
	ListenerParamNode *root;

	self->m_list.count = 0;
	root = PafNew(sizeof(*root));
	if (!root)
		ScePafStdc_D608246A();
	self->m_list.root_node = root;
	if (root) {
		root->prev = root;
		root->next = root;
	}
	self->m_iterator = NULL;
	_ZN3paf6thread6RMutexC1EPKci(&self->m_list_mutex, "ScePafTimerListenerListMutex", 0);
	self->m_iterator = self->m_list.root_node;
	self->m_iterator_updated = 0;
	return self;
}

TimerListenerList *TimerListenerListConstruct(TimerListenerList *self)
{
	return TimerListenerListInitialize(self);
}

TimerListenerList *TimerListenerListConstructBase(TimerListenerList *self)
{
	return TimerListenerListInitialize(self);
}

void TimerListenerListInit(void)
{
	TimerListenerList *list = PafNew(sizeof(*list));

	if (list)
		TimerListenerListConstruct(list);
	s_default_list = list;
}

void TimerListenerListExit(void)
{
	TimerListenerList *list = s_default_list;
	ListenerParamNode *root;

	if (!list)
		return;
	_ZN3paf6thread6RMutexD1Ev(&list->m_list_mutex);
	root = list->m_list.root_node;
	if (root) {
		ListenerParamNode *node = root->next;

		while (node != root) {
			ListenerParamNode *next = node->next;

			PafDelete(node);
			root = list->m_list.root_node;
			node = next;
		}
		PafDelete(root);
	}
	list->m_list.root_node = (ListenerParamNode *)(uintptr_t)-1;
	list->m_list.count = (unsigned int)-1;
	PafDelete(list);
}

void TimerListenerListUpdate(TimerListenerList *self)
{
	ListenerParamNode *root;
	ListenerParamNode *node;

	_ZN3paf6thread6RMutex4LockEv(&self->m_list_mutex);
	root = self->m_list.root_node;
	node = root ? root->next : NULL;
	self->m_iterator = node;
	while (node != root) {
		TimerListenerDoTick(node->val.listener, node->val.data1, node->val.data2);
		if (self->m_iterator_updated) {
			self->m_iterator_updated = 0;
			root = self->m_list.root_node;
			node = self->m_iterator;
		} else {
			if (node->val.interval) {
				TimerListener *listener = node->val.listener;
				Timer *timer = listener->m_timer;

				if (!timer || timer->m_state == STATE_HOLD ||
				    (timer->m_state == STATE_FINISHED && timer->m_repeat == REPEAT_NONE))
					TimerListenerResetTimer(listener);
			}
			node = self->m_iterator;
			root = self->m_list.root_node;
			if (node) {
				node = node->next;
				self->m_iterator = node;
			}
		}
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_list_mutex);
}

void ScePafMisc_C565FD29(TimerListenerList *self, const ListenerParam *param)
{
	_ZN3paf6thread6RMutex4LockEv(&self->m_list_mutex);
	ListenerParamListPushBack(&self->m_list, param);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_list_mutex);
}

void ScePafMisc_A4554B25(TimerListenerList *self, TimerListener *listener)
{
	ListenerParamNode *root;
	ListenerParamNode *node;

	_ZN3paf6thread6RMutex4LockEv(&self->m_list_mutex);
	root = self->m_list.root_node;
	node = root ? root->next : NULL;
	while (node != root) {
		if (node->val.listener == listener) {
			int is_iterator = node == self->m_iterator;
			ListenerParamNode *prev = node->prev;
			ListenerParamNode *next = node->next;

			next->prev = prev;
			prev->next = node->next;
			if (is_iterator) {
				PafDelete(node);
				self->m_iterator = next;
				--self->m_list.count;
				self->m_iterator_updated = 1;
			} else {
				PafDelete(node);
				--self->m_list.count;
			}
			break;
		}
		node = node ? node->next : NULL;
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_list_mutex);
}
