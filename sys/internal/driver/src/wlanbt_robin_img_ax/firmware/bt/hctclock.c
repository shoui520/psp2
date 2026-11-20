/*
 * Clock requirements of the Bluetooth controller during coexistence: the
 * rates it asks for, the RF blocks they keep on, and the coexistence tick
 * that refreshes them. Sony's code and data for these functions form one
 * object, ahead of the host controller transport (bt/hct.c).
 *
 * The filename and all role names are inferred; no source name survives.
 */
#include <stddef.h>
#include "robin/btbuffer.h"
#include "robin/btcontext.h"
#include "robin/btpool.h"
#include "robin/btrom.h"
#include "robin/clock.h"
#include "robin/eventpower.h"
#include "robin/hct.h"
#include "robin/mdev.h"
#include "robin/osa.h"
#include "robin/power.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"
#include "robin/wlan.h"

/*
 * The clock rates the Bluetooth side needs during coexistence (0x56e40), set
 * by hctClockRequirementSet(). A policy bit says the chip's rate meets one
 * of them; while it does, the RF block that goes with it stays on.
 */
#define HCT_CLOCK_REFERENCE_MET	0x01	/* rfBlock0 */
#define HCT_CLOCK_BASE_MET	0x02	/* rfBlock1 */
#define HCT_CLOCK_SLOWEST_MET	0x04	/* rfBlock4 */
#define HCT_CLOCK_SCHEDULING	0x08	/* hctEnableCoexScheduling() */

/* hctClockRequirementSet()'s selectors; bit 31 applies the requirements at once. */
enum {
	HCT_CLOCK_NONE = 0,
	HCT_CLOCK_REFERENCE = 1,
	HCT_CLOCK_CORE = 2,
	HCT_CLOCK_AUX = 3,
	HCT_CLOCK_BASE = 4,
	HCT_CLOCK_MODE = 5,
	HCT_CLOCK_BUS = 6,
	HCT_CLOCK_VOTE = 7,
};
#define HCT_CLOCK_APPLY		0x80000000

struct HctClockRequirements {
	uint32_t referenceRate;		/* 32000 while mode is above 1 */
	uint32_t coreRate;
	uint32_t auxRate;
	uint32_t busRate;
	uint32_t voteRate;
	uint8_t mode;
	uint8_t policy;			/* HCT_CLOCK_* */
	uint8_t reserved[2];
};

static struct HctClockRequirements hctClock __attribute__((section(".itcm.bss")));	/* 0x56e40 */

/* Coexistence tick state. */
uint8_t hctCoexTickCount __attribute__((section(".itcm.rw"))) = 0;
uint8_t hctCoexDirty __attribute__((section(".itcm.rw"))) = 0;

/*
 * 0x0001fdc4: drops the policy bit of a requirement the chip's rates no
 * longer meet. 1: a bit was dropped, so the caller checks again.
 */
int hctClockCheckRequirements(void)
{
	int changed = 0;
	uint32_t slowest;

	if (hctClock.mode > 1)
		hctClock.referenceRate = 32000;
	if ((hctClock.policy & HCT_CLOCK_REFERENCE_MET) &&
	    clockReferenceRate() < hctClock.referenceRate) {
		changed = 1;
		hctClock.policy &= ~HCT_CLOCK_REFERENCE_MET;
	}
	if (!(hctClock.policy & HCT_CLOCK_SLOWEST_MET))
		return changed;
	slowest = clockSlowestRateGet();
	if (hctClock.coreRate > slowest || hctClock.busRate > slowest ||
	    (!rfEventTransitionActive() &&
	     (clockRequestedCore(3) > slowest || clockRequestedBus(3) > slowest)) ||
	    clockRequestedCore(5) > slowest || clockRequestedBus(5) > slowest ||
	    clockRequestedCore(1) > slowest || clockRequestedBus(1) > slowest ||
	    clockRequestedCore(2) > slowest || clockRequestedBus(2) > slowest ||
	    !(hctClock.policy & HCT_CLOCK_REFERENCE_MET) ||
	    !(hctClock.policy & HCT_CLOCK_BASE_MET)) {
		changed = 1;
		hctClock.policy &= ~HCT_CLOCK_SLOWEST_MET;
	}
	return changed;
}

/* 0x0001fe7a: switches the RF blocks the policy bits keep on, and asks for the rates. */
int hctClockApply(void)
{
	int state = sysIrqDisable();

	while (hctClockCheckRequirements())
		;
	if ((hctClock.policy & HCT_CLOCK_REFERENCE_MET) && !rfBlock0Get())
		rfBlock0Set(1);
	if ((hctClock.policy & HCT_CLOCK_BASE_MET) && !rfBlock1Get())
		rfBlock1Set(1);
	if (hctClock.policy & HCT_CLOCK_SLOWEST_MET) {
		if (!rfBlock4Get())
			rfBlock4Set(1);
	}
	if (!(hctClock.policy & HCT_CLOCK_SLOWEST_MET)) {
		if (rfBlock4Get())
			rfBlock4Set(0);
	}
	if (!(hctClock.policy & HCT_CLOCK_BASE_MET) && rfBlock1Get())
		rfBlock1Set(0);
	if (!(hctClock.policy & HCT_CLOCK_REFERENCE_MET) && rfBlock0Get())
		rfBlock0Set(0);
	clockRequestCore(hctClock.coreRate, 0);
	clockRequestAux(hctClock.auxRate, 0);
	if (!(hctClock.policy & HCT_CLOCK_REFERENCE_MET))
		clockSelectSource(hctClock.referenceRate == 32000);
	clockRequestBus(hctClock.busRate, 0);
	clockRequestVote(hctClock.voteRate, 0);
	hctCoexDirty = 0;
	sysIrqRestore(state);
	return 0;
}

/* Sets the policy bit of a requirement the rate meets. */
static inline void hctClockSlowestMet(uint32_t value)
{
	if (clockSlowestRateGet() >= value)
		hctClock.policy |= HCT_CLOCK_SLOWEST_MET;
	else
		hctClock.policy &= ~HCT_CLOCK_SLOWEST_MET;
}

/*
 * 0x0001ff4e: sets one clock requirement (HCT_CLOCK_*), and applies them all
 * when HCT_CLOCK_APPLY is set. 1: coexistence scheduling is off.
 */
int hctClockRequirementSet(int selector, uint32_t value)
{
	struct HctClockRequirements previous;
	int apply = 0;

	__aeabi_memcpy4(&previous, &hctClock, sizeof(previous));
	if (!(hctClock.policy & HCT_CLOCK_SCHEDULING))
		return 1;
	if (selector < 0) {
		selector &= ~HCT_CLOCK_APPLY;
		apply = 1;
	}
	switch (selector) {
	case HCT_CLOCK_REFERENCE:
		if (clockReferenceRate() >= value)
			hctClock.policy |= HCT_CLOCK_REFERENCE_MET;
		else
			hctClock.policy &= ~HCT_CLOCK_REFERENCE_MET;
		hctClock.referenceRate = value;
		break;
	case HCT_CLOCK_CORE:
		hctClockSlowestMet(value);
		hctClock.coreRate = value;
		break;
	case HCT_CLOCK_AUX:
		hctClockSlowestMet(value);
		hctClock.auxRate = value;
		break;
	case HCT_CLOCK_BUS:
		hctClockSlowestMet(value);
		hctClock.busRate = value;
		break;
	case HCT_CLOCK_VOTE:
		hctClockSlowestMet(value);
		hctClock.voteRate = value;
		break;
	case HCT_CLOCK_BASE:
		if (clockBaseRate() >= value)
			hctClock.policy |= HCT_CLOCK_BASE_MET;
		else
			hctClock.policy &= ~HCT_CLOCK_BASE_MET;
		break;
	case HCT_CLOCK_MODE:
		hctClock.mode = value;
		break;
	default:
		break;
	}
	if (memcmp(&previous, &hctClock, sizeof(previous)) != 0)
		hctCoexDirty = 1;
	if (apply)
		hctClockApply();
	return 0;
}

/* 0x00020044: chooses the coexistence intervals from the state of the ACL links. */
int hctComputeCoexIntervals(void)
{
	uint32_t slow = 4000;
	uint32_t fast = 4000;
	uint32_t interval;
	uint8_t *link;
	uint8_t *info;
	unsigned int i;

	for (i = 1; i < HCT_ACL_LINKS + 1; i++) {
		link = btHccRomApplyState();
		if (link[120 * i] == 2) {
			info = btLcoRomVerifyState(i);
			if (info[4] == 1 && info[6] == 1)
				interval = link[120 * i + 3] ? 8000 : 4000;
			else
				interval = link[120 * i + 3] ? 32000 : 8000;
			if (slow <= interval)
				slow = interval;
		} else if (link[120 * i] == 1) {
			interval = 16000;
			if (slow > 16000)
				interval = slow;
			slow = interval;
		}
	}
	if (romCoexPrimaryFlag != 0) {
		slow = slow > 32000 ? slow : 32000;
		fast = fast > 16000 ? fast : 16000;
	}
	if (romTargetTable == 2) {
		slow = slow > 16000 ? slow : 16000;
		fast = fast > 16000 ? fast : 16000;
	}
	hctClockRequirementSet(HCT_CLOCK_REFERENCE, 16000);
	hctClockRequirementSet(HCT_CLOCK_AUX, fast);
	hctClockRequirementSet(HCT_CLOCK_CORE, slow);
	return 0;
}

/* 0x00020112: once per coexistence tick, refreshes the scheduling state. */
void hctCoexTick(void)
{
	uint32_t slow;
	uint32_t fast;

	if (hctCoexTickCount < 5 || (SOC_EVENT_STATE & 4) != 0) {
		if (hctCoexTickCount < 5)
			hctCoexTickCount++;
		if ((hctClock.policy & HCT_CLOCK_SCHEDULING) != 0) {
			if (rfBlock3Get() == 0)
				rfBlock3Set(1);
			if (rfBlock2Get() == 0)
				rfBlock2Set(1);
			if (!rfEventTransitionActive())
				rfEventTransitionSet(1);
			hctClockRequirementSet(HCT_CLOCK_BASE, romHctCoexMode);
			hctComputeCoexIntervals();
			slow = 4000;
			fast = 4000;
			if (hctClock.coreRate > 16000) {
				fast = 16000;
				slow = 16000;
			}
			hctClockRequirementSet(HCT_CLOCK_VOTE, slow);
			hctClockRequirementSet(HCT_CLOCK_BUS, fast);
			if (hctCoexDirty != 0)
				hctClockRequirementSet(HCT_CLOCK_APPLY | HCT_CLOCK_NONE, 0);
		}
	}
}

/* 0x000201a2: lowers the coexistence deadline when the programmed one is later. */
uint32_t sysUpdateEventDeadline(void)
{
	uint32_t result = (uint32_t)hctClock.policy << 28;
	uint32_t interval;
	uint32_t parsed;

	if ((hctClock.policy & HCT_CLOCK_SCHEDULING) != 0) {
		interval = 16000;
		parsed = clockRequestedCore(6);
		result = rfBlock4Get();
		if (result != 0) {
			result = clockSlowestRateGet();
			if (result >= 16000)
				result = 16000;
			interval = result;
		}
		if (parsed < interval) {
			clockRequestCore(interval, 0);
			hctCoexDirty = 1;
			return 1;
		}
	}
	return result;
}

/* 0x000201f6 */
uint32_t *hctEnableCoexScheduling(void)
{
	hctClock.policy |= HCT_CLOCK_SCHEDULING;
	return &hctClock.referenceRate;
}

