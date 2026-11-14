/*
 * 802.11g protection (0x5fb6..0x64ac): what the beacons and probe responses
 * of the BSS and its neighbours say about stations that need protection or
 * long preambles, kept as timers that run out a few seconds after the last
 * frame that asked for it. See wlan/protection.c.
 */
#ifndef ROBIN_PROTECTION_H
#define ROBIN_PROTECTION_H

#include <stdint.h>

#include "robin/txring.h"
#include "robin/wlan.h"

/* WlanProtection.flags */
#define WLAN_PROTECTION_USE_PROTECTION	0x10	/* RTS/CTS or CTS-to-self before OFDM frames */
#define WLAN_PROTECTION_LONG_PREAMBLE	0x20	/* no short preamble */
#define WLAN_PROTECTION_DSSS_BSS	0x40	/* the IBSS's own rates are DSSS/CCK only */

/* WlanProtection.timers */
enum {
	WLAN_PROTECTION_NON_ERP,	/* an ERP element said Non-ERP Present */
	WLAN_PROTECTION_USE_ERP,	/* an ERP element said Use Protection */
	WLAN_PROTECTION_BARKER,		/* an ERP element said Barker Preamble Mode */
	WLAN_PROTECTION_TIMER3,		/* set by code not converted yet */
	WLAN_PROTECTION_DSSS_ONLY,	/* a station with DSSS/CCK rates only */
	WLAN_PROTECTION_NO_SHORT_PREAMBLE, /* a station without the Short Preamble capability */
	WLAN_PROTECTION_NON_GREENFIELD,	/* an HT element said Non-Greenfield STAs Present */
	WLAN_PROTECTION_TIMER7,		/* timers 7-11: set by code not converted yet */
	WLAN_PROTECTION_TIMER8,
	WLAN_PROTECTION_TIMER9,
	WLAN_PROTECTION_TIMER10,
	WLAN_PROTECTION_TIMER11,	/* never runs out by reaching 0 exactly */
	WLAN_PROTECTION_TIMERS
};

#define WLAN_PROTECTION_TIMEOUT		5000000	/* microseconds */

/* Short Preamble bit of the capability information's first byte */
#define WLAN_CAPABILITY_SHORT_PREAMBLE	0x20

typedef struct __attribute__((packed)) WlanProtectionTimer {
	uint8_t remaining[4];		/* signed microseconds; > 0 while running */
	uint8_t last[4];		/* the time it was last set or aged */
} WlanProtectionTimer;

/* At 0x04008844, in the BSS state (byte aligned). */
typedef struct __attribute__((packed)) WlanProtection {
	uint8_t flags;			/* WLAN_PROTECTION_* */
	uint8_t reserved1;
	WlanProtectionTimer timers[WLAN_PROTECTION_TIMERS];
} WlanProtection;

_Static_assert(sizeof(WlanProtection) == 98, "protection state");

/* Not converted yet: in the BSS state at 0x0400882c. */
extern WlanProtection wlanProtection;			/* 0x04008844 */

void wlanProtectionUpdateFlags(WlanProtection *protection);
void wlanProtectionAgeMode1(WlanProtection *protection, unsigned int now);
void wlanProtectionAge(WlanProtection *protection, unsigned int now);
void wlanProtectionClear(WlanProtection *protection);
void wlanProtectionObserve(WlanProtection *protection, const uint8_t *capability,
			   const uint8_t *erp, unsigned int rates, unsigned int now,
			   int32_t timeout);
int wlanRatesDsssOnly(unsigned int rates);
void wlanTemplateSetIe(int kind, WlanWirelessFrame *frame, struct WlanMacTxRequest *request,
		       const uint8_t *ie);
void wlanTxRequestReset(struct WlanMacTxRequest *request);

#endif /* ROBIN_PROTECTION_H */
