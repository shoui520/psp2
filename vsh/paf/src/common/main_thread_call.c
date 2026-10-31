/* A call executes once its state check reports ready. */
#include "main_thread_call.h"

extern void _ZN3paf6thread5SleepEi(unsigned int milliseconds);
extern void _ZN3paf6common18MainThreadCallList8RegisterEPFvPvES2_(Function func,
							    void *userdata);
extern void _ZN3paf6common18MainThreadCallList10UnregisterEPFvPvES2_(Function func,
							      void *userdata);

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
