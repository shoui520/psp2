/*
 * The sleep factor: 32 flags that tell the host why the system sleeps or wakes, read with
 * command 0x0003. A change of a flag sets sleepFactorStatus.changed, which the host sees in
 * the first byte of every frame's answer (sleepFactorSnapshot, with the link's own flags);
 * the host then reads the flags as they were at that frame (sleepFactorGet), and the change
 * is cleared once nothing changed since (sleepFactorAcknowledge). The flags change with the
 * dispatcher disabled, so no task sees half a change.
 */
#include "ernie.h"
#include "chip.h"
#include "config.h"
#include "kernel.h"
#include "sleepfactor.h"
#include "board.h"

/* not static: the updater's downloaderStart takes them over from the application */
__sreg union SleepFactor sleepFactor;
__sreg union SleepFactor sleepFactorRead;	/* the flags the host reads */
__sreg union SleepFactorStatus sleepFactorStatus;

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
#if SOURCE_0103
		sleepFactorStatus.bits.changed = 1;	/* both ways on USS-1001 */
#else
		if (on)
			sleepFactorStatus.bits.changed = 1;
#endif
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

/* 0x00090206 has none */
#if FW_SINCE(0x00090804)
void sleepFactorSet10(unsigned char on)
{
	if (sleepFactor.bits[1].b2 ^ on) {
		dis_dsp();
		sleepFactor.bits[1].b2 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}
#endif

#if defined(ERNIE_UPDATER) || !FW_SINCE(0x01000104)
/* the updater's, and before 0x01000104 the application's, which nothing calls */
void sleepFactorSet24(unsigned char on)
{
	if (sleepFactor.bits[3].b0 ^ on) {
		dis_dsp();
		sleepFactor.bits[3].b0 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}
#endif

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

#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
void sleepFactorSet15(unsigned char on)
{
	if (sleepFactor.bits[1].b7 ^ on) {
		dis_dsp();
		sleepFactor.bits[1].b7 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}
#endif

#if defined(ERNIE_UPDATER) || FW_SINCE(0x00090206)	/* 0x00090009 has no charge factor 5 */
void sleepFactorSet19(unsigned char on)
{
	if (sleepFactor.bits[2].b3 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b3 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}
#endif

/* the updater has not the last three, 0x00090804 not 22 and 23 */
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x00090903)
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

#if FW_SINCE(0x01000104)
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

#if SOURCE_0103
/* the slim's: analog input 0x0010 (analog.c) */
void sleepFactorSet14(unsigned char on)
{
	if (sleepFactor.bits[1].b6 ^ on) {
		dis_dsp();
		sleepFactor.bits[1].b6 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

/* the slim's: nothing sets it */
void sleepFactorSet21(unsigned char on)
{
	if (sleepFactor.bits[2].b5 ^ on) {
		dis_dsp();
		sleepFactor.bits[2].b5 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

#if BOARD_DOL1002
/* DOL-1002's three more */
void sleepFactorSet27(unsigned char on)
{
	if (sleepFactor.bits[3].b3 ^ on) {
		dis_dsp();
		sleepFactor.bits[3].b3 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet29(unsigned char on)
{
	if (sleepFactor.bits[3].b5 ^ on) {
		dis_dsp();
		sleepFactor.bits[3].b5 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}

void sleepFactorSet28(unsigned char on)
{
	if (sleepFactor.bits[3].b4 ^ on) {
		dis_dsp();
		sleepFactor.bits[3].b4 = on;
		sleepFactorStatus.bits.changed = 1;
		ena_dsp();
	}
}
#endif
#endif
#endif
#endif

#if FW_SINCE(0x0100060B)
/* host command 0x0015 and serial command 0x0168: what the settings say of the off state, a
   code for its flags, the flags (bit 3 transition 0x18 blocked, bits 0 and 1 the settings'
   offInfoBit0 and offInfoBit1, bit 2 always) and the settings' offInfo2 and offInfo3 */
void sleepFactorGetOffInfo(unsigned char *info)
{
	unsigned char flags = 0;

	flags |= configGet()->offTransitionBlock == 0x5aa5 ? 0x08 : 0;
#if BOARD_OFF_INFO_BITS
	flags |= configGet()->offInfoBit0 != 0 ? 0x01 : 0;
	flags |= configGet()->offInfoBit1 != 0 ? 0x02 : 0;
	flags |= 0x04;
#else
	flags |= configGet()->offInfo2 != 0 ? 0x01 : 0;
	flags |= configGet()->offInfo3 != 0 ? 0x02 : 0;
#endif
	switch (flags) {
	case 3:
	case 4:
	case 7:
		info[0] = 0x70;
		break;
	case 8:
	case 12:
		info[0] = 0x00;
		break;
	case 9:
	case 13:
		info[0] = 0x20;
		break;
	default:
		info[0] = 0x80;
		break;
	}
	info[1] = flags;
	info[2] = configGet()->offInfo2;
	info[3] = configGet()->offInfo3;
}
#endif
