/* A call executes once its state check reports ready. */
#include "main_thread_call.h"
#include "../std/new.h"

extern void _ZN3paf6thread5SleepEi(unsigned int milliseconds);
extern void _ZN3paf6common18MainThreadCallList8RegisterEPFvPvES2_(Function func,
							    void *userdata);
extern void _ZN3paf6common18MainThreadCallList10UnregisterEPFvPvES2_(Function func,
							      void *userdata);

/*
 * The state check's vtable is exported; its destructors and IsReady are inline (Sony's build
 * emits them with the inline functions at the end of the module), Sleep is its key function.
 */
const StateCheckVTableData StateCheckVTableObject = {
	0, NULL, { StateCheckDestroy, StateCheckDelete, StateCheckIsReady, StateCheckSleep }
};

StateCheck *StateCheckDestroy(StateCheck *self)
{
	self->vtable = &StateCheckVTableObject.functions;
	return self;
}

void StateCheckDelete(StateCheck *self)
{
	self->vtable = &StateCheckVTableObject.functions;
	PafDelete(self);
}

/* a state check is never ready unless a derived check says so */
int StateCheckIsReady(StateCheck *self)
{
	return 0;
}

void StateCheckSleep(StateCheck *self)
{
	_ZN3paf6thread5SleepEi(10);
}

MainThreadCall *MainThreadCallConstruct(MainThreadCall *self, StateCheck *check,
				      Function func, void *userdata)
{
	self->m_check = check;
	self->m_func = func;
	self->m_userdata = userdata;
	_ZN3paf6common18MainThreadCallList8RegisterEPFvPvES2_(MainThreadCallRun, self);
	return self;
}

MainThreadCall *MainThreadCallConstructBase(MainThreadCall *self, StateCheck *check,
					  Function func, void *userdata)
{
	self->m_check = check;
	self->m_func = func;
	self->m_userdata = userdata;
	_ZN3paf6common18MainThreadCallList8RegisterEPFvPvES2_(MainThreadCallRun, self);
	return self;
}

MainThreadCall *MainThreadCallDestroy(MainThreadCall *self)
{
	_ZN3paf6common18MainThreadCallList10UnregisterEPFvPvES2_(MainThreadCallRun, self);
	return self;
}

MainThreadCall *MainThreadCallDestroyBase(MainThreadCall *self)
{
	_ZN3paf6common18MainThreadCallList10UnregisterEPFvPvES2_(MainThreadCallRun, self);
	return self;
}

void MainThreadCallRun(void *userdata)
{
	MainThreadCall *self = userdata;

	if (self->m_check->vtable->IsReady(self->m_check)) {
		_ZN3paf6common18MainThreadCallList10UnregisterEPFvPvES2_(MainThreadCallRun, self);
		self->m_func(self->m_userdata);
	}
}
