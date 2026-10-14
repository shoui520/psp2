/*
 * fw2 runtime: the interval timer, which times each command (VipCommandBlock's
 * statistics) and wakes vipWaitTimer. It counts down; its clock runs only while
 * it is read or timing.
 */
#include "runtime.h"

/* the ticks since vipStartTimer */
uint32_t vipReadTimer(void)
{
	uint32_t count;

	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, VIP_CLOCK_CORE);
	count = vipReadControlBus(VIP_CB_TIMER_COUNT);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, 0);
	return ~count;
}

void vipStartTimer(void)
{
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, VIP_CLOCK_CORE | VIP_CLOCK_SHARED);
	vipWriteControlBus(VIP_CB_TIMER_COUNT, 0xffffffff);
	vipWriteControlBus(VIP_CB_TIMER_CONTROL, 1);
}

void vipStopTimer(void)
{
	vipWriteControlBus(VIP_CB_TIMER_RELOAD, 1);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, 0);
}

void vipInitializeTimer(void)
{
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, VIP_CLOCK_CORE);
	vipWriteControlBus(VIP_CB_TIMER_ENABLE, 0);
	vipWriteControlBus(VIP_CB_TIMER_ENABLE, 1);
	vipWriteControlBus(VIP_CB_TIMER_MODE, 2);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, 0);
}

/* Halt until the timer reaches ticks (its interrupt wakes the core). */
void vipWaitTimer(uint32_t ticks)
{
	mep_di();
	vipWriteControlBus(VIP_CB_TIMER_COMPARE, ticks);
	vipWriteControlBus(VIP_CB_TIMER_INTERRUPT, 1);
	__asm__ volatile ("nop\n\tnop");
	mep_halt();
	mep_ei();
}
