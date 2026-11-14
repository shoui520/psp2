/*
 * 802.11g protection timers, the element update of the beacon and probe
 * response templates, and the reset of a template's transmit request
 * (0x5fb6..0x64ac). The filename is inferred; no source filename survives.
 */
#include "robin/protection.h"
#include "robin/ie.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/threadx.h"

static ROBIN_PATCH_SLOT(wlanTemplateSetIeOverride);

static inline int32_t timerRemaining(const WlanProtectionTimer *timer)
{
	return (int32_t)__aeabi_uread4(timer->remaining);
}

static inline void timerSet(WlanProtectionTimer *timer, int32_t timeout, unsigned int now)
{
	__aeabi_uwrite4(timeout, timer->remaining);
	__aeabi_uwrite4(now, timer->last);
}

/* Counts a running timer down by the time since it was last set or aged. */
static inline int32_t timerAge(WlanProtectionTimer *timer, unsigned int now)
{
	int elapsed = wlanTimeDelta(now, __aeabi_uread4(timer->last));
	int32_t remaining = timerRemaining(timer) - elapsed;

	__aeabi_uwrite4(remaining, timer->remaining);
	__aeabi_uwrite4(now, timer->last);
	return remaining;
}

static inline void timerAgeRunning(WlanProtection *protection, unsigned int index,
				   unsigned int now)
{
	WlanProtectionTimer *timer = &protection->timers[index];

	if (timerRemaining(timer) > 0)
		timerAge(timer, now);
}

/* Long preambles while a Barker or a long-preamble station was seen; protection likewise. */
void wlanProtectionUpdateFlags(WlanProtection *protection)
{
	uint8_t flags = protection->flags;

	if (timerRemaining(&protection->timers[WLAN_PROTECTION_BARKER]) > 0 ||
	    timerRemaining(&protection->timers[WLAN_PROTECTION_NO_SHORT_PREAMBLE]) > 0)
		protection->flags = flags | WLAN_PROTECTION_LONG_PREAMBLE;
	else
		protection->flags = flags & ~WLAN_PROTECTION_LONG_PREAMBLE;
	if (timerRemaining(&protection->timers[WLAN_PROTECTION_NON_ERP]) > 0 ||
	    timerRemaining(&protection->timers[WLAN_PROTECTION_USE_ERP]) > 0 ||
	    timerRemaining(&protection->timers[WLAN_PROTECTION_DSSS_ONLY]) > 0)
		protection->flags |= WLAN_PROTECTION_USE_PROTECTION;
	else
		protection->flags &= ~WLAN_PROTECTION_USE_PROTECTION;
}

/*
 * Ages timers 3 and 6-11, which the caller runs only in its mode 1 (the
 * code that sets them is not converted yet); the last stops at -1, not 0.
 */
void wlanProtectionAgeMode1(WlanProtection *protection, unsigned int now)
{
	WlanProtectionTimer *last = &protection->timers[WLAN_PROTECTION_TIMER11];

	timerAgeRunning(protection, WLAN_PROTECTION_TIMER6, now);
	timerAgeRunning(protection, WLAN_PROTECTION_TIMER7, now);
	timerAgeRunning(protection, WLAN_PROTECTION_TIMER8, now);
	timerAgeRunning(protection, WLAN_PROTECTION_TIMER9, now);
	timerAgeRunning(protection, WLAN_PROTECTION_TIMER3, now);
	timerAgeRunning(protection, WLAN_PROTECTION_TIMER10, now);
	if (timerRemaining(last) > 0 && timerAge(last, now) == 0)
		__aeabi_uwrite4(-1, last->remaining);
}

/* Ages the timers the beacons of the BSS and its neighbours set. */
void wlanProtectionAge(WlanProtection *protection, unsigned int now)
{
	timerAgeRunning(protection, WLAN_PROTECTION_NON_ERP, now);
	timerAgeRunning(protection, WLAN_PROTECTION_USE_ERP, now);
	timerAgeRunning(protection, WLAN_PROTECTION_BARKER, now);
	timerAgeRunning(protection, WLAN_PROTECTION_DSSS_ONLY, now);
	timerAgeRunning(protection, WLAN_PROTECTION_NO_SHORT_PREAMBLE, now);
}

void wlanProtectionClear(WlanProtection *protection)
{
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_DSSS_ONLY].remaining);
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_NO_SHORT_PREAMBLE].remaining);
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_NON_ERP].remaining);
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_USE_ERP].remaining);
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_BARKER].remaining);
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_TIMER3].remaining);
	protection->flags &= ~(WLAN_PROTECTION_USE_PROTECTION | WLAN_PROTECTION_LONG_PREAMBLE);
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_TIMER10].remaining);
	__aeabi_uwrite4(0, protection->timers[WLAN_PROTECTION_TIMER11].remaining);
}

/*
 * A beacon or probe response was seen: its rate bitmap, capability and ERP
 * element (any may be missing) restart the timers they ask for.
 */
void wlanProtectionObserve(WlanProtection *protection, const uint8_t *capability,
			   const uint8_t *erp, unsigned int rates, unsigned int now,
			   int32_t timeout)
{
	if (rates <= 0xf)
		timerSet(&protection->timers[WLAN_PROTECTION_DSSS_ONLY], timeout, now);
	if (capability && !(*capability & WLAN_CAPABILITY_SHORT_PREAMBLE))
		timerSet(&protection->timers[WLAN_PROTECTION_NO_SHORT_PREAMBLE], timeout, now);
	if (!erp)
		return;
	if (erp[2] & WLAN_ERP_NON_ERP_PRESENT) {
		timerSet(&protection->timers[WLAN_PROTECTION_NON_ERP], timeout, now);
		timerSet(&protection->timers[WLAN_PROTECTION_USE_ERP], timeout, now);
	} else if (erp[2] & WLAN_ERP_USE_PROTECTION) {
		timerSet(&protection->timers[WLAN_PROTECTION_USE_ERP], timeout, now);
	}
	if (erp[2] & WLAN_ERP_BARKER_PREAMBLE)
		timerSet(&protection->timers[WLAN_PROTECTION_BARKER], timeout, now);
}

/* A rate bitmap (wlanRateBitmapAll) with the four DSSS/CCK rates at most. */
int wlanRatesDsssOnly(unsigned int rates)
{
	return rates >> 4 == 0;
}

/*
 * Replaces an element of a beacon (kind WLAN_FRAME_BEACON) or probe response
 * template and moves the template's offsets, length and end by the change.
 */
void wlanTemplateSetIe(int kind, WlanWirelessFrame *frame, struct WlanMacTxRequest *request,
		       const uint8_t *ie)
{
	uint8_t removed = 0;
	uint8_t added = 0;
	unsigned int length;
	UINT posture;

	if (wlanTemplateSetIeOverride(kind, frame, request, ie))
		return;
	wlanSetIe(frame, ie,
		  kind == WLAN_FRAME_BEACON ? wlanBeaconTemplateEnd : wlanProbeTemplateEnd,
		  &removed, &added);
	posture = tx_interrupt_control(TX_INT_DISABLE);
	if (ie[0] < IE_CF_PARAMETER_SET)
		request->cfOffset = request->cfOffset - removed + added;
	if (ie[0] < IE_TIM)
		request->timOffset = request->timOffset - removed + added;
	length = (request->templateLength[0] | request->templateLength[1] << 8) - removed + added;
	request->templateLength[0] = length;
	request->templateLength[1] = length >> 8;
	if (kind == WLAN_FRAME_BEACON)
		wlanBeaconTemplateEnd = wlanBeaconTemplateEnd - removed + added;
	else
		wlanProbeTemplateEnd = wlanProbeTemplateEnd - removed + added;
	tx_interrupt_control(posture);
}

/* A template's transmit request before its fields are filled in. */
void wlanTxRequestReset(struct WlanMacTxRequest *request)
{
	request->mode = 1;
	request->reserved1[0] = 0;
	request->reserved1[1] = 0;
	request->reserved1[2] = 0;
	request->hwStatus = 0;
	request->hwRetries = 0;
	request->hwFlags = 0;
	__aeabi_uwrite4(0, &request->reserved16[0]);
	__aeabi_uwrite4(0, &request->reserved16[4]);
	request->reserved16[8] = 0;
	request->reserved16[9] = 0;
	request->reserved16[10] = 0;
	request->reserved16[11] = 0;
	request->type = 0;
	request->info = 0;
}
