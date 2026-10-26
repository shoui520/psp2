/*
 * TimerSlot: a handler takes the first free slot and is ticked from the SysTick
 * interrupt until it is removed.
 */

#include <stdio.h>

extern "C" {
#include "stm32f10x_lib.h"
}
#include "TimerSlot.h"
#include "stm32f10x_it.h"

TimerSlot::TimerSlot(TimerSlotHandler **slots, u32 count)
{
	slots_ = slots;
	count_ = count;
	for (u32 i = 0; i < count; i++)
		slots[i] = 0;
}

/* SysTick's interrupt on, every reload counts; the counter is started by Start */
void TimerSlot::Init(u8 preemption, u8 sub, u32 reload, s32 elapsed)
{
	elapsed_ = elapsed;
	RegisterIrqHandler(15, IsrCallback, this, preemption, sub);	/* SysTick */
	SysTick_SetReload(reload);
	SysTick_ITConfig(ENABLE);
}

void TimerSlot::Start()
{
	SysTick_CounterCmd(SysTick_Counter_Enable);
}

#if MOTION_IMAGE != 0
/* the interrupt on again, after the STOP mode */
void TimerSlot::Resume()
{
	SysTick_ITConfig(ENABLE);
}
#endif

bool TimerSlot::Add(TimerSlotHandler *handler)
{
	u32 i;

	for (i = 0; i < count_; i++) {
		if (slots_[i] == 0) {
			slots_[i] = handler;
			handler->SetSlot(i);
			return true;
		}
	}
#if MOTION_IMAGE != 0
	printf("TimerSlot full.\n");
#endif
	return false;
}

bool TimerSlot::Remove(TimerSlotHandler *handler)
{
	slots_[handler->Slot()] = 0;
	return true;
}

void TimerSlot::Isr()
{
	for (u32 i = 0; i < count_; i++) {
		if (slots_[i] != 0)
			slots_[i]->Tick(elapsed_);
	}
}

void TimerSlot::IsrCallback(void *arg)
{
	static_cast<TimerSlot *>(arg)->Isr();
}
