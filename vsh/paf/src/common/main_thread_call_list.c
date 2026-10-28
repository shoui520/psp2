/* The timer-backed paf::common::MainThreadCallList singleton. */
#include "call_list.h"
#include "../timer/timer.h"

typedef struct CallListListener {
	TimerListener base;
	CallList m_list;
	unsigned char m_is_calling;
} CallListListener;

typedef char CallListListenerSizeCheck[sizeof(CallListListener) == 80 ? 1 : -1];

extern void *ScePafStdc_FC5CD359(size_t size);
extern void ScePafStdc_1B77082E(void *ptr);
extern Mutex *ScePafThread_A5D3F1B5(Mutex *self, const char *name, int option);
extern void ScePafThread_AD04A33D(Mutex *self);
extern int ScePafStdc_C95732BB(void *object, void (*destroy)(void *), void *dso);
extern void *__dso_handle;

static Mutex s_giant_lock;
static CallListListener *s_instance;

CallListListener *CallListListenerConstruct(CallListListener *self);
TimerListener *CallListListenerDestroy(TimerListener *base);
void CallListListenerDelete(TimerListener *base);
void CallListListenerOnFinish(TimerListener *base, void *data1, void *data2);
void CallListListenerOnTick(TimerListener *base, void *data1, void *data2);

/* The ARM C++ ABI places offset-to-top and RTTI before the method pointers. */
static const struct {
	int offset_to_top;
	const void *type_info;
	TimerVTable methods;
} timerClass = {
	0, NULL, { ScePafMisc_70E604B9, ScePafMisc_D29258BB }
};

static const struct {
	int offset_to_top;
	const void *type_info;
	TimerListenerVTable methods;
} callListListenerClass = {
	0, NULL, {
		CallListListenerDestroy, CallListListenerDelete,
		CallListListenerOnTick, CallListListenerOnFinish
	}
};

CallListListener *CallListListenerConstruct(CallListListener *self)
{
	Timer *timer = ScePafStdc_FC5CD359(sizeof(*timer));
	if (timer) {
		timer->vtable = &timerClass.methods;
		timer->m_is_2d = 0;
		timer->m_is_3d = 0;
		timer->m_param_x_num = 0;
		timer->m_param_x = NULL;
		timer->m_mapped_x = 0.0f;
		ScePafMisc_06BD5A37(timer, 0.01f, 0, 0, -1, NULL);
	}
	self->base.m_data = NULL;
	self->base.m_timer = timer;
	self->base.vtable = &callListListenerClass.methods;
	CallListConstruct(&self->m_list);
	self->m_is_calling = 0;
	TimerListenerList *list = s_default_list;
	ListenerParam param;
	ScePafMisc_C565FD29(list,
		ScePafMisc_B90DBCA6(&param, &self->base, 1, NULL, NULL));
	return self;
}

#define MainThreadCallListRegister _ZN3paf6common18MainThreadCallList8RegisterEPFvPvES2_
#define MainThreadCallListUnregister _ZN3paf6common18MainThreadCallList10UnregisterEPFvPvES2_

void MainThreadCallListRegister(Function entry, void *userdata)
{
	ScePafThread_4EA3A3D5(&s_giant_lock);
	CallListListener *listener = s_instance;
	if (!listener) {
		listener = ScePafStdc_FC5CD359(sizeof(*listener));
		if (listener)
			listener = CallListListenerConstruct(listener);
		s_instance = listener;
	}
	CallItem item = { entry, userdata };
	CallListPushBack(&listener->m_list.m_list, &item);
	ScePafThread_9100D580(&s_giant_lock);
}

void MainThreadCallListUnregister(Function entry, void *userdata)
{
	CallListListener *toDelete = NULL;

	ScePafThread_4EA3A3D5(&s_giant_lock);
	if (s_instance) {
		CallListUnregister(&s_instance->m_list, entry, userdata);
		CallListListener *listener = s_instance;
		if (!listener->m_is_calling && !listener->m_list.m_list.count) {
			s_instance = NULL;
			toDelete = listener;
		}
	}
	ScePafThread_9100D580(&s_giant_lock);
	if (toDelete)
		toDelete->base.vtable->Delete(&toDelete->base);
}

TimerListener *CallListListenerDestroy(TimerListener *base)
{
	CallListListener *self = (CallListListener *)base;

	base->vtable = &callListListenerClass.methods;
	ScePafMisc_A4554B25(s_default_list, base);
	ScePafThread_8CCECAC9(&self->m_list.m_item_mtx);
	CallItemNode *root = self->m_list.m_list.root_node;
	if (root) {
		CallItemNode *item = root->next;
		while (item != root) {
			CallItemNode *next = item->next;
			ScePafStdc_1B77082E(item);
			root = self->m_list.m_list.root_node;
			item = next;
		}
		ScePafStdc_1B77082E(root);
	}
	self->m_list.m_list.root_node = (CallItemNode *)~0U;
	self->m_list.m_list.count = ~0U;
	ScePafMisc_CE6EB459(base);
	return base;
}

void CallListListenerDelete(TimerListener *base)
{
	CallListListenerDestroy(base);
	ScePafStdc_1B77082E(base);
}

void CallListListenerOnFinish(TimerListener *base, void *data1, void *data2)
{
	CallListListener *self = (CallListListener *)base;

	ScePafThread_4EA3A3D5(&s_giant_lock);
	if (s_instance) {
		self->m_is_calling = 1;
		CallListUpdate(&self->m_list, &s_giant_lock);
		unsigned int count = self->m_list.m_list.count;
		self->m_is_calling = 0;
		if (!count) {
			if (s_instance)
				s_instance->base.vtable->Delete(&s_instance->base);
			s_instance = NULL;
		}
	}
	ScePafThread_9100D580(&s_giant_lock);
}

int CallListListenerInitialize(void)
{
	ScePafThread_A5D3F1B5(&s_giant_lock, "ScePafCallListListener", 0);
	return ScePafStdc_C95732BB(&s_giant_lock,
		(void (*)(void *))ScePafThread_AD04A33D, &__dso_handle);
}

void CallListListenerOnTick(TimerListener *base, void *data1, void *data2)
{
}
