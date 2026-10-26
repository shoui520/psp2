/*
 * Timer: timeouts on the timer slots (TimerSlot), which tick every handler registered
 * in them; a one-shot Timer and a PeriodicTimer, calling back when they run out.
 */
#ifndef TIMER_H
#define TIMER_H

#include "stm32f10x_type.h"
#include "Callback.h"

/* what the timer slots tick: its slot, and the function its timer calls */
class TimerSlotHandler {
public:
	typedef Callback<void ()> Func;

	TimerSlotHandler() {}
	virtual void Tick(s32 elapsed) {}

	void Register();
	void Unregister();

	void SetSlot(u32 slot)
	{
		slot_ = slot;
	}

	u32 Slot() const
	{
		return slot_;
	}

protected:
	u32 slot_;
	Func func_;
};

class ITimer {
public:
	ITimer() {}
	virtual void Start(s32 ticks, Callback<void ()>::Func func, void *arg) {}
	virtual void Stop() {}
};

class Timer : public TimerSlotHandler, public ITimer {
public:
	Timer() {}
	virtual void Tick(s32 elapsed);
	virtual void Start(s32 ticks, Callback<void ()>::Func func, void *arg);
	virtual void Stop();
	bool StartChecked(s32 ticks, Callback<void ()>::Func func, void *arg);

private:
	s32 remaining_;
};

class PeriodicTimer : public TimerSlotHandler, public ITimer {
public:
	PeriodicTimer() {}
	virtual void Tick(s32 elapsed);
	virtual void Start(s32 ticks, Callback<void ()>::Func func, void *arg);
	virtual void Stop();

private:
	s32 period_;
	s32 remaining_;
};

#endif
