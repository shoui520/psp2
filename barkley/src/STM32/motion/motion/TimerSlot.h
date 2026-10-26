/*
 * TimerSlot: the SysTick interrupt, which ticks every handler registered in its slots
 * by the time elapsed since the last tick.
 */
#ifndef TIMERSLOT_H
#define TIMERSLOT_H

#include "Timer.h"

class TimerSlot {
public:
	TimerSlot(TimerSlotHandler **slots, u32 count);
	void Init(u8 preemption, u8 sub, u32 reload, s32 elapsed);
	void Start();
	void Resume();
	bool Add(TimerSlotHandler *handler);
	bool Remove(TimerSlotHandler *handler);

private:
	static void IsrCallback(void *arg);
	void Isr();

	TimerSlotHandler **slots_;
	u32 count_;
	s32 elapsed_;		/* the time of a tick, given to each handler */
};

#endif
