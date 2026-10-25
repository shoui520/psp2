/*
 * The sleep factor: 32 flags that tell the host why the system sleeps or wakes, read with
 * command 0x0003. A change of a flag sets sleepFactorStatus.changed, which the host sees in
 * the first byte of every frame's answer (sleepFactorSnapshot, with the link's own flags);
 * the host then reads the flags as they were at that frame (sleepFactorGet), and the change
 * is cleared once nothing changed since (sleepFactorAcknowledge). The flags change with the
 * dispatcher disabled, so no task sees half a change.
 */
#include "chip.h"
#include "kernel.h"
#include "sleepfactor.h"

union SleepFactor {
	unsigned long	value;
	struct SfrBits	bits[4];
};

/* the status bits sleepFactorSnapshot returns; bits 0, 1 and 5 are the link's (HostFlags) */
union SleepFactorStatus {
	unsigned char	value;
	struct {
		unsigned char	reserved1:2;
		unsigned char	changed:1;	/* a flag changed since the host read them */
		unsigned char	status3:1;	/* set by the module at 0x28eeb */
		unsigned char	status4:1;	/* set by the module at 0x28eeb */
		unsigned char	reserved2:3;
	} bits;
};

static __sreg union SleepFactor sleepFactor;
static __sreg union SleepFactor sleepFactorRead;	/* the flags the host reads */
static __sreg union SleepFactorStatus sleepFactorStatus;

void sleepFactorInit(void)
{
	sleepFactor.value = 0;
	sleepFactorStatus.value = 0;
}

/* at the start of a frame: the flags the host may read, and the status for its first byte */
unsigned char sleepFactorSnapshot(void)
{
	unsigned char status;

	dis_dsp();
	sleepFactorRead = sleepFactor;
	status = sleepFactorStatus.value;
	ena_dsp();
	return status;
}

/* the host read the flags: the change is cleared if none came since the snapshot */
void sleepFactorAcknowledge(void)
{
	if (sleepFactor.value == sleepFactorRead.value)
		sleepFactorStatus.bits.changed = 0;
}

unsigned long sleepFactorGet(void)
{
	return sleepFactorRead.value;
}

void sleepFactorSet5(unsigned char on)
{
	if (sleepFactor.bits[0].b5 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b5 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet6(unsigned char on)
{
	if (sleepFactor.bits[0].b6 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b6 = on;
		if (on)
			sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet4(unsigned char on)
{
	if (sleepFactor.bits[0].b4 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b4 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet7(unsigned char on)
{
	if (sleepFactor.bits[0].b7 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b7 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet3(unsigned char on)
{
	if (sleepFactor.bits[0].b3 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b3 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet8(unsigned char on)
{
	if (sleepFactor.bits[1].b0 ^ on) {
		dis_dsp();
		sleepFactor.bits[1].b0 = on;
		if (on)
			sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet25(unsigned char on)
{
	if (sleepFactor.bits[3].b1 ^ on) {
		dis_dsp();
		sleepFactor.bits[3].b1 = on;
		if (on)
			sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSetStatus3(unsigned char on)
{
	sleepFactorStatus.bits.status3 = on;
}

void sleepFactorSet0(unsigned char on)
{
	if (sleepFactor.bits[0].b0 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b0 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet1(unsigned char on)
{
	if (sleepFactor.bits[0].b1 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b1 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet2(unsigned char on)
{
	if (sleepFactor.bits[0].b2 ^ on) {
		dis_dsp();
		sleepFactor.bits[0].b2 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet9(unsigned char on)
{
	if (sleepFactor.bits[1].b1 ^ on) {
		dis_dsp();
		sleepFactor.bits[1].b1 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet10(unsigned char on)
{
	if (sleepFactor.bits[1].b2 ^ on) {
		dis_dsp();
		sleepFactor.bits[1].b2 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSetStatus4(unsigned char on)
{
	sleepFactorStatus.bits.status4 = on;
}

void sleepFactorSet16(unsigned char on)
{
	if (sleepFactor.bits[2].b0 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b0 = on;
		if (on)
			sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet17(unsigned char on)
{
	if (sleepFactor.bits[2].b1 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b1 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet18(unsigned char on)
{
	if (sleepFactor.bits[2].b2 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b2 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet15(unsigned char on)
{
	if (sleepFactor.bits[1].b7 ^ on) {
		dis_dsp();
		sleepFactor.bits[1].b7 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet19(unsigned char on)
{
	if (sleepFactor.bits[2].b3 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b3 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet22(unsigned char on)
{
	if (sleepFactor.bits[2].b6 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b6 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet23(unsigned char on)
{
	if (sleepFactor.bits[2].b7 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b7 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet20(unsigned char on)
{
	if (sleepFactor.bits[2].b4 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b4 = on;
		if (on)
			sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorStart(void)
{
}
