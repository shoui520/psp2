/*
 * Timer: a timer counts down by the time each tick of its slot gives; a one-shot
 * timer leaves its slot when it runs out, a periodic one starts its next period.
 */

#include "System.h"
#include "Timer.h"

bool TimerSlotHandler::Register()
{
	return AddTimer(this);
}

bool TimerSlotHandler::Unregister()
{
	return RemoveTimer(this);
}

void Timer::Start(s32 ticks, Callback<void ()>::Func func, void *arg)
{
	func_.Set(func, arg);
	remaining_ = ticks;
	Register();
}

/* as Start: false when every slot is taken */
bool Timer::StartChecked(s32 ticks, Callback<void ()>::Func func, void *arg)
{
	func_.Set(func, arg);
	remaining_ = ticks;
	return Register();
}

void Timer::Stop()
{
	Unregister();
}

void Timer::Tick(s32 elapsed)
{
	remaining_ -= elapsed;
	if (remaining_ < 1) {
		Unregister();
		func_();
	}
}

void PeriodicTimer::Start(s32 ticks, Callback<void ()>::Func func, void *arg)
{
	func_.Set(func, arg);
	period_ = ticks;
	remaining_ = ticks;
	Register();
}

void PeriodicTimer::Stop()
{
	Unregister();
}

void PeriodicTimer::Tick(s32 elapsed)
{
	remaining_ -= elapsed;
	if (remaining_ < 1) {
		remaining_ += period_;
		func_();
	}
}
