/*
 * The Bluetooth link's state as the coexistence code sees it (0x243ec..0x245d4):
 * whether a link exists and carries traffic, the profile word the host gave,
 * and the activity the Bluetooth controller reports. The functions are small
 * accessors; wlan/coex.c and wlan/pspoll.c decide what to do with the answers.
 *
 * Marvell published no source; names are inferred from use.
 */
#include <stddef.h>
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/eventpower.h"
#include "robin/hct.h"
#include "robin/pspoll.h"
#include "robin/sys.h"

/* Link states of btCoex.linkState. */
#define BT_COEX_LINK_NONE	0
#define BT_COEX_LINK_STARTING	1	/* the descriptor exists, waiting for traffic */
#define BT_COEX_LINK_ACTIVE	2
#define BT_COEX_LINK_IDLE	3	/* had traffic, none any more */

#define BT_COEX_LINK_TIMEOUT_US		3000000
#define BT_COEX_SLOT_US			625

static struct {
	uint8_t linkCapacity;		/* 2 when a capacity was given, else 4 */
	uint8_t linkState;
	struct BtCoexProfile profile;
	uint32_t activity;		/* the controller has activity */
	uint32_t scheduled;		/* a transfer is scheduled */
	uint32_t hostReset;		/* set by host command 0x00a9, never cleared here */
	uint32_t linkLoaded;		/* the link carries load */
	uint32_t hctEnabled;
	uint32_t linkTimer;		/* osaCreateTimer() handle of the start timeout */
} btCoex __attribute__((section(".itcm.rw"))) = {
	.scheduled = 1,
};

/* 0x243ec: follows the link controller's descriptor and the load through the link states. */
void btCoexUpdateLink(void)
{
	UINT posture = sysIrqDisable();

	if (btLmpLcUpdateDescriptor() != 0) {
		if (btCoex.linkState == BT_COEX_LINK_NONE) {
			btCoex.linkState = BT_COEX_LINK_STARTING;
			osaCreateTimer(btCoexLinkTimerExpired, 0, BT_COEX_LINK_TIMEOUT_US,
				       &btCoex.linkTimer, 0);
		}
	} else {
		btCoex.linkState = BT_COEX_LINK_NONE;
	}
	switch (btCoex.linkState) {
	case BT_COEX_LINK_STARTING:
	case BT_COEX_LINK_IDLE:
		if (btCoex.linkLoaded == 1)
			btCoex.linkState = BT_COEX_LINK_ACTIVE;
		break;
	case BT_COEX_LINK_ACTIVE:
		if (btCoex.linkLoaded == 0)
			btCoex.linkState = BT_COEX_LINK_IDLE;
		break;
	}
	sysIrqRestore(posture);
}

/* 0x2444a: the link did not carry load in time. */
void btCoexLinkTimerExpired(uint32_t timer, uint32_t argument)
{
	if (timer == btCoex.linkTimer && btCoex.linkState == BT_COEX_LINK_STARTING &&
	    btCoex.linkLoaded == 0) {
		btCoex.linkLoaded = 1;
		btCoexUpdateLink();
	}
}

/* 0x24466 */
void btCoexSetProfile(struct BtCoexProfile profile)
{
	struct BtCoexProfile previous = btCoex.profile;

	btCoex.profile = profile;
	btCoexUpdateLink();
	btCoexProfileChanged(previous, btCoex.profile);
}

/* 0x24486 */
void btCoexActivityStarted(void)
{
	btCoex.activity = 1;
	wlanCoexBtActivityChanged(1, btCoex.scheduled);
}

/* 0x24496 */
void btCoexActivityStopped(void)
{
	btCoex.activity = 0;
	wlanCoexBtActivityChanged(0, btCoex.scheduled);
}

/* 0x244a6 */
void btCoexSetScheduled(uint32_t scheduled)
{
	if (btCoex.scheduled != scheduled) {
		btCoex.scheduled = scheduled;
		wlanCoexBtActivityChanged(btCoex.activity, scheduled);
	}
}

/*
 * 0x244bc: a load above 100 means the link is busy; the start timeout no longer matters. The
 * caller also passes whether the link streams, which is not used.
 */
void btCoexSetLoad(uint32_t load, uint32_t streaming)
{
	if (load > 100) {
		btCoex.linkLoaded = 1;
		osaCancelTimer(btCoex.linkTimer);
		btCoex.linkTimer = 0;
	} else {
		btCoex.linkLoaded = 0;
	}
	btCoexUpdateLink();
}

/* 0x244dc */
void btCoexSetLinkCapacity(uint32_t first, uint32_t second)
{
	btCoex.linkCapacity = (first | second) != 0 ? 2 : 4;
}

/* 0x244ec: the polling interval and window of the profile, in microseconds. */
void btCoexPollTiming(uint32_t *interval, uint32_t *window)
{
	struct BtCoexProfile profile = btCoex.profile;

	*interval = profile.interval * BT_COEX_SLOT_US;
	*window = profile.window * BT_COEX_SLOT_US;
}

/* 0x2450a */
uint32_t btCoexProfileBit1(void)
{
	return btCoex.profile.slaveOnly;
}

/* 0x24516 */
int btCoexProfilePollable(void)
{
	struct BtCoexProfile profile = btCoex.profile;
	int pollable = 1;

	if (!profile.polling || profile.noPoll) {
		if (!profile.slaveOnly)
			pollable = 0;
	}
	return pollable;
}

/* 0x2452e */
uint32_t btCoexProfileBit3(void)
{
	return btCoex.profile.windowed;
}

/* 0x2453a */
int btCoexLinkIsActive(void)
{
	btCoexUpdateLink();
	return btCoex.linkState == BT_COEX_LINK_ACTIVE;
}

/* 0x2454e */
int btCoexLinkIsStarting(void)
{
	btCoexUpdateLink();
	return btCoex.linkState == BT_COEX_LINK_STARTING;
}

/* 0x24560 */
uint32_t btCoexActivityStart(void)
{
	return btCoex.activity;
}

/* 0x24566: no transfer is scheduled, after the link controller's context was brought up to date. */
int btCoexRefreshActivityIdle(void)
{
	btLmpLcSyncContext();
	return btCoex.scheduled == 0;
}

/* 0x2457a */
int btCoexActivityIdle(void)
{
	return btCoex.scheduled == 0;
}

/* 0x24588 */
int btCoexWlanFlagged(void)
{
	return wlanAnyConnectionFlags2Bit4();
}

/* 0x24590 */
void hostCmd00a9Reset(void)
{
	btCoex.hostReset = 1;
}

/* 0x24598 */
uint32_t btCoexParam(void)
{
	return btCoex.hostReset;
}

/* 0x2459e */
unsigned int btCoexActiveLinks(void)
{
	return btLmpLcResetEntry();
}

/* 0x245a6 */
unsigned int btCoexLinkCapacity(void)
{
	return btCoex.linkCapacity;
}

/* 0x245ac: the HCI transport is switched on or off. */
void btSdioSetEnabled(int enabled)
{
	if ((uint32_t)enabled != btCoex.hctEnabled)
		hctSetEnabled(enabled);
	btCoex.hctEnabled = enabled;
}
