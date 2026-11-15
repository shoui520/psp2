/*
 * The MAC's interrupt (SoC line 10, 0x1bfe0..0x1cc6c): the events the MAC
 * raises are read once from MAC_510_EVENTS, handled and acknowledged one by
 * one (a bit is acknowledged by writing a word with that bit clear), and the
 * tasks are told. Then the radio's sleep and wake-up (the host sleep exit and
 * entry, the MAC restart) and the connection slots (the rings and templates a
 * connection takes). The names are inferred from use; the event bits are this
 * firmware's own.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/apps.h"
#include "robin/autotx.h"
#include "robin/buffer.h"
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/dmaring.h"
#include "robin/hostcmd.h"
#include "robin/ie.h"
#include "robin/ibss.h"
#include "robin/mac.h"
#include "robin/baslot.h"
#include "robin/macrate.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/phy.h"
#include "robin/pspoll.h"
#include "robin/psctl.h"
#include "robin/rf.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/stapssm.h"
#include "robin/subscribe.h"
#include "robin/sync.h"
#include "robin/eventpower.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wlanrx.h"
#include "robin/wlantask.h"
#include "robin/macentry.h"

/* The MAC's event word and the registers the handler looks at. */
#define MAC_510_EVENTS		MAC_REG(0x510)	/* pending events; a bit is acknowledged by writing 0 to it */
#define MAC_540_STATUS		MAC_REG(0x540)	/* bit 31: an operation timed out */
#define MAC_544_CONTROL		MAC_REG(0x544)
#define MAC_610_CIPHER		MAC_REG(0x610)
#define MAC_61C_PROFILE		MAC_REG(0x61c)
#define MAC_620_EVENT		MAC_REG(0x620)
#define MAC_648_CONTEXT		MAC_REG(0x648)
#define MAC_838_ACTIVE_FRAME	MAC_REG(0x838)
#define MAC_8E4_ACTIVE_NODE	MAC_REG(0x8e4)
#define MAC_8FC_ACTIVE_SLOT	MAC_REG(0x8fc)

/* MAC_510_EVENTS bits. */
#define MAC_EVENT_QUEUE_A	0x80000000u	/* a frame is waiting: post it, or wake the management task */
#define MAC_EVENT_NODE_B	0x10000000u	/* a beacon interval of the AP ended */
#define MAC_EVENT_BEACON	0x08000000u	/* the beacon template of a BSS needs its state */
#define MAC_EVENT_RECEIVE	0x04000000u	/* a frame is in the receive ring */
#define MAC_EVENT_RECEIVE_DONE	0x02000000u	/* acknowledged with a received frame */
#define MAC_EVENT_ACK_ONLY_24	0x01000000u
#define MAC_EVENT_IBSS_PS	0x00100000u	/* an IBSS exists: its power save window */
#define MAC_EVENT_ACK_ONLY_17	0x00020000u
#define MAC_EVENT_TX_NOTIFY	0x00008000u	/* the transmit notification task has work */
#define MAC_EVENT_QUEUE_B	0x00000800u	/* a frame is waiting (the second queue) */
#define MAC_EVENT_DATA_PENDING	0x00000080u	/* the AP has data pending for us */
#define MAC_EVENT_COUNT_20	0x00000020u
#define MAC_EVENT_COUNT_10	0x00000010u
#define MAC_EVENT_COUNT_08	0x00000008u
#define MAC_EVENT_COUNT_04	0x00000004u
#define MAC_EVENT_TX_DONE	0x00000002u
#define MAC_EVENT_ACK_ONLY_00	0x00000001u

/* The events of the management task's queue (macMgmtEventFlags) the handler posts. */
#define MGMT_EVENT_QUEUE_B	0x00000001
#define MGMT_EVENT_RECEIVE	0x00000002
#define MGMT_EVENT_QUEUE_A	0x00000200

/* Not converted yet; provisional names. */
void uapSqramFilterEntry(int enable);								/* SQRAM 0xc0009ba0 */
uint32_t wlanPsResetDescriptor(struct WlanConnection *connection, const uint8_t *tim);		/* 0x3e98a */
void romDmaRingSetPointer(uint16_t pointer, unsigned int ring);					/* 0x03f3abf7 */
void clockHostSetMax(void);								/* 0x26ae0 */
int macTxNotifyUpdatePacket(void);								/* 0x32584 */
void macMgmtVerifyHeader(void);									/* 0x3b904 */
int wlanStaSmLinked(const struct WlanConnection *connection);					/* 0x35faa */
struct WlanPsPeerState *wlanConnectionPsState(struct WlanConnection *connection);		/* 0x1d1f6 */
void p2pDirectUpdateEntry(struct WlanConnection *connection, int kind);			/* 0x41268 */
void wlanPsResolvePacket(struct WlanConnection *connection, struct WlanRxStatus *status);	/* 0x3d08c */
void wlanPsAdjustFrame(struct WlanConnection *connection, const uint8_t *body);			/* 0x3db1a */
void wlanPsRecordFrame(void);									/* 0x3dc46 */
int macMgmtAcquireContext(struct WlanConnection *connection, const uint8_t *body,
			  unsigned int length);							/* 0x35ac2 */
void macMgmtConfigureState(struct WlanConnection *connection);					/* 0x347e8 */
void wlanApOlbcSelect(struct WlanConnection *connection, uint8_t *olbcActive,
			uint8_t *erpInfo);							/* 0x2b680 */
void macMgmtAcquirePacket(struct WlanConnection *connection, uint8_t *olbcCount,
			  uint8_t *erpInfo);							/* 0x3a204 */
void macMgmtClearPacket(struct WlanConnection *connection);					/* 0x3a2fa */
void macMgmtCheckDescriptor(void);								/* 0x3bc28 */
void wlanPsConvertTable(uint32_t activeFrame);							/* 0x3c054 */
void macMgmtFormatContext(void);								/* 0x35936 */
void clockHostApplyTarget(void);								/* 0x26b48 */
void wlanAmpTxPacketsAdjust(void);								/* 0x1e84a */
int wlanPsSleepBlocked(void);									/* 0x2ea5c */
extern uint8_t wlanIbssJoinState[4];								/* 0x040088ce, unaligned */
extern struct OsaEventFlags *macMgmtEventFlags;							/* 0x55138 */
extern struct OsaEventFlags *macTxNotifyEventFlags;						/* 0x04000180 */

void wlanMacSleepDone(void);									/* 0x1caa6 */
void wlanMacReset(void);									/* 0x1ca28 */
extern uint8_t wlanPrimaryEvents;								/* 0x04008828, bit 0: radio on */

/* The ring (entry) of the last beacon event, and what the handler counts. */
uint8_t wlanMacRing __attribute__((section(".itcm.rw")));				/* 0x54c30 */
uint32_t wlanMacTxDoneCount __attribute__((section(".itcm.rw")));			/* 0x54c34 */
uint32_t wlanMacAbortCount __attribute__((section(".itcm.rw")));			/* 0x54c38 */
uint32_t wlanMacCount04 __attribute__((section(".itcm.rw")));				/* 0x54c3c */
uint32_t wlanMacCount08 __attribute__((section(".itcm.rw")));				/* 0x54c40 */
uint32_t wlanMacCount10 __attribute__((section(".itcm.rw")));				/* 0x54c44 */
uint32_t wlanMacCount20 __attribute__((section(".itcm.rw")));				/* 0x54c48 */
uint32_t wlanMacFlushCount __attribute__((section(".itcm.rw")));			/* 0x54c5c */
/* The IBSS power save window: its limit (-1: none), and whether a PS buffer was posted. */
uint32_t wlanIbssPsLimit __attribute__((section(".itcm.rw")));				/* 0x54bc0 */
uint32_t wlanIbssPsBuffered __attribute__((section(".itcm.rw")));			/* 0x593cc */
uint32_t wlanIbssPsActive __attribute__((section(".itcm.rw")));				/* 0x593d4 */
/* Set while the host sleep is on (0), the power save waits to sleep again (1). */
uint32_t wlanPsSleepTimeout __attribute__((section(".itcm.rw")));				/* 0x54c4c */
uint32_t wlanPsMgmtWakeDue __attribute__((section(".itcm.rw")));				/* 0x54c50 */
uint32_t wlanPsHostWakeDue __attribute__((section(".itcm.rw")));				/* 0x54c54 */
uint32_t wlanPsFilterState __attribute__((section(".itcm.rw")));				/* 0x54c58 */

/*
 * The MAC's events, read once and handled in an order of their own; the
 * management task is told what it has to do (post) after the frame bits.
 */
void wlanMacEvents(int line, uint32_t pending)
{
	uint32_t events;
	uint32_t post = 0;
	struct WlanConnection *connection;
	int verify;
	int received;
	int shouldNotify;
	uint8_t *raw;
	uint8_t *header;
	uint8_t *body;
	uint16_t bodyLength;

	clockHostSetMax();
	events = MAC_510_EVENTS;
	if ((int32_t)MAC_540_STATUS < 0) {
		sysDelayUs(20);
		if (MAC_8FC_ACTIVE_SLOT != 0) {
			wlanMacAbortCount++;
			macTxPostEvent(0x40);
			MAC_544_CONTROL = (MAC_544_CONTROL << 1) >> 1;
		} else {
			MAC_540_STATUS = 0x7fffffff;
		}
	}
	wlanPsHostWakeDue |= (events & 0x8880) | wlanHostSleepPending;
	if (wlanPsEnabled() && wlanPsHostWakeDue == 0) {
		wlanPsHostWakeDue |= mdevIoctl(wlanSdioDev, 0x1b, 0);
		if (wlanPsHostWakeDue == 0)
			wlanPsHostWakeDue |= mdevIoctl(wlanSdioDev, 8, 0);
	}
	romLibAcquireContext();
	if (events & MAC_EVENT_ACK_ONLY_24)
		MAC_510_EVENTS = ~MAC_EVENT_ACK_ONLY_24;
	if (events & MAC_EVENT_DATA_PENDING) {
		wlanPsDataPending++;
		if (wlanPsAsleep())
			macTxPostEvent(4);
		wlanPsPollReplied();
	}
	if (events & MAC_EVENT_ACK_ONLY_17)
		MAC_510_EVENTS = ~MAC_EVENT_ACK_ONLY_17;
	if (events & MAC_EVENT_QUEUE_A) {
		if (macTxNotifyUpdatePacket() != 0)
			macTxPostEvent(4);
		else
			post = MGMT_EVENT_QUEUE_A;
		MAC_510_EVENTS = 0x7fffffff;
	}
	if (events & MAC_EVENT_QUEUE_B) {
		if (macTxNotifyUpdatePacket() != 0)
			macTxPostEvent(4);
		else
			post |= MGMT_EVENT_QUEUE_B;
		MAC_510_EVENTS = ~MAC_EVENT_QUEUE_B;
	}

	if (events & MAC_EVENT_RECEIVE) {
		if (macTxNotifyUpdatePacket() != 0) {
			macTxPostEvent(4);
			MAC_510_EVENTS = ~MAC_EVENT_RECEIVE;
			events &= ~MAC_EVENT_RECEIVE;
		} else {
			verify = 0;
			connection = NULL;
			received = wlanDmaRingPeekReady(&raw, &header, &body, &bodyLength);
			if (received) {
				post |= MGMT_EVENT_RECEIVE;
				events &= ~MAC_EVENT_RECEIVE_DONE;
				MAC_510_EVENTS = ~MAC_EVENT_RECEIVE_DONE;
				connection = wlanConnectionFindByLocal(WLAN_IF_IBSS, header + 18);
				if (connection == NULL || !(connection->stationFlags & 0x80)) {
					connection = wlanConnectionFindByPeer(header + 12);
					if (connection == NULL) {
						received = 0;
						wlanIbssBeaconMatched((const WlanWirelessFrame *)header, body);
					}
				}
			}
			if (wlanRoleIbssOnly()) {
				MAC_61C_PROFILE = 0;
				MAC_REG(0x640) = 32;
			}
			if (received) {
				if (wlanIbssPsEnabled()) {
					if (wlanIbssPsBuffered == 0)
						wlanPostConnectionEvent(connection, 0x4000);
					macMgmtVerifyHeader();
				}
				if (wlanStaSmLinked(connection)) {
					if (connection->interfaceType == WLAN_IF_IBSS) {
						if (header != NULL &&
						    wlanConnectionHasLocalAddress(connection, header + 18)) {
							__aeabi_uwrite4(0, wlanIbssJoinState);
							wlanIbssPsActive = 0;
							if (wlanPsFilterState == 0 && wlanIbssPsLimit != (uint32_t)-1) {
								wlanPostConnectionEvent(connection, 1024);
								wlanPsFilterState = 1;
								uapSqramFilterEntry(2);
							}
						}
					} else {
						verify = wlanPsVerifyFrame(0);
						if (wlanHasApRole()) {
							struct WlanPsSleep *sleep =
								&wlanConnectionInfo(wlanConnectionByRole(2))->sleep;

							sleep->tbttReference = romGetTimerTimestamp();
							sleep->tsfReference = *(const uint64_t *)body;
							sleep->dtimCount =
								wlanIeFind(5, body + 12, bodyLength - 12)[2];
						}
						wlanLinkQualityEndPeriod();
					}
					shouldNotify = (uint8_t)wlanStaPsShouldNotify();
					{
						struct WlanRxStatus *status = (struct WlanRxStatus *)raw;
						uint8_t legacyRate = status->legacyRate;
						uint8_t rssi = status->rssi;
						uint8_t noise = status->noise;

						wlanRxConvertStatus(status);
						wlanPsResolvePacket(connection, status);
						status->legacyRate = legacyRate;
						status->rssi = rssi;
						status->noise = noise;
					}
					if (wlanRoleOtherOnly() && wlanPsEnabled() && verify != 0 && !shouldNotify &&
					    !wlanHasApRole() && (wlanHostSleepArmed() || wlanPsSessionOpen())) {
						if (wlanPsSleepTimeout == 1 ||
						    (wlanSessionId != 0 && wlanPsSleepSequence != 0)) {
							const uint8_t *tim = wlanIeFind(5, body + 12, bodyLength - 12);
							uint32_t flags;

							if ((unsigned int)tim[1] - 4 >= 0xfb || tim[3] == 0)
								tim = NULL;
							flags = wlanPsResetDescriptor(connection, tim);
							if ((wlanHostSleepActive != NULL &&
							     ((__aeabi_uread4(wlanHostSleepActive) | 2) & flags) != 0) ||
							    mdevIoctl(wlanSdioDev, 0x1b, 0) != 0 ||
							    wlanSubscribeEventsPending(connection) ||
							    (__aeabi_uread4(&wlanMefClassMask) & flags) != 0 ||
							    wlanPsSleepSequence == 0 || (flags & 0x800b) != 0 ||
							    macMgmtAcquireContext(connection, body, bodyLength) == 0) {
								wlanPsMgmtWakeDue = 1;
							} else {
								post &= ~MGMT_EVENT_RECEIVE;
								wlanPsAdjustFrame(connection, body);
								wlanPsRecordFrame();
								romDmaRingSetPointer(wlanDmaRingRearm(5), 5);
								wlanPostConnectionEvent(connection, 1);
							}
						}
						wlanPsHostWakeDue = 0;
					} else if (wlanPsSleepTimeout == 1 ||
						   (wlanSessionId != 0 && wlanPsSleepSequence != 0)) {
						wlanPsMgmtWakeDue = 1;
					}
					if (wlanAutoTxWanted(connection) && shouldNotify) {
						if (!wlanPsEnabled() || wlanCoexUseP2pMode() || wlanCoexUseStationMode())
							macTxPostEvent(8);
						else
							wlanPsMgmtWakeDue = 1;
					}
				}
				if (wlanPsMgmtWakeDue == 1) {
					wlanPsSleepTimeout = 0;
					wlanPsSleepSequence = 0;
					post &= ~MGMT_EVENT_RECEIVE;
					wlanRaiseEvent(3);
				}
			}
			MAC_510_EVENTS = ~MAC_EVENT_RECEIVE;
		}
	}

	if (events & MAC_EVENT_BEACON) {
		connection = NULL;
		if (MAC_500_SLEEP_CONTROL & 2) {
			connection = wlanConnectionFindByTypeNumber(WLAN_IF_IBSS, 0);
		} else if (MAC_500_SLEEP_CONTROL & 1) {
			connection = wlanConnectionSlotGet(wlanMacRing);
			if (!wlanConnectionBssStarted(connection))
				connection = NULL;
		}
		if (connection != NULL) {
			const WlanWirelessFrame *beacon;

			beacon = (const WlanWirelessFrame *)__aeabi_uread4(connection->beaconRequest->frame);
			macMgmtConfigureState(connection);
			if ((beacon->body[10] & 1) && connection->mode == 2 &&
			    wlanConnectionBssStarted(connection) == 1) {
				struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

				if (info->phyMode != 0) {
					wlanApOlbcSelect(connection, &info->olbcActive, &info->erpInfo);
					if (info->htEnabled != 0) {
						macMgmtAcquirePacket(connection, &info->olbcCount, &info->erpInfo);
						if (info->htCapability[2] & 2)
							macMgmtClearPacket(connection);
					}
				}
			}
			if (beacon->body[10] & 2) {
				uint32_t activeFrame;

				if (wlanRoleIbssOnly()) {
					MAC_61C_PROFILE = 0;
					MAC_REG(0x640) = 32;
				}
				macMgmtCheckDescriptor();
				activeFrame = MAC_838_ACTIVE_FRAME;
				wlanPsConvertTable(activeFrame);
				wlanIbssRefreshBeacons(connection);
				if (wlanIbssSmActive(connection) && wlanIbssPsLimit != (uint32_t)-1) {
					uint32_t elapsed = __aeabi_uwrite4(__aeabi_uread4(wlanIbssJoinState) + activeFrame,
									   wlanIbssJoinState);

					if (elapsed == 52 && wlanPsFilterState != 0)
						wlanIbssPsActive = 1;
					if (elapsed >= wlanIbssPsLimit && wlanPsFilterState != 0) {
						__aeabi_uwrite4(0, wlanIbssJoinState);
						wlanPostConnectionEvent(connection, 512);
						wlanPsFilterState = 0;
						macMgmtFormatContext();
						wlanIbssPsActive = 0;
					}
				}
			}
		}
		if (wlanApPsEnabled()) {
			struct WlanConnectionInfo *info = wlanConnectionInfo(wlanConnectionByRole(2));
			struct WlanPsSleep *sleep = &info->sleep;

			wlanRaiseEvent(7);
			if (sleep->mode == WLAN_PS_SLEEP_DTIM && (MAC_610_CIPHER == 0 || MAC_620_EVENT == 1) &&
			    sleep->recentActivity == 0) {
				if (sleep->control & 1) {
					if (info->stationCount == 0)
						wlanRaiseEvent(0x12);
				} else if (macTxHardwareIdle()) {
					wlanRaiseEvent(0x12);
				}
			}
		}
		MAC_510_EVENTS = ~MAC_EVENT_BEACON;
	}
	if (events & MAC_EVENT_RECEIVE_DONE)
		MAC_510_EVENTS = ~MAC_EVENT_RECEIVE_DONE;
	if (post != 0)
		osaEventFlagsSet(macMgmtEventFlags, post, 7);

	if (events & MAC_EVENT_NODE_B) {
		if (wlanRoleIbssOnly()) {
			wlanPostConnectionEvent(NULL, 0x20000);
			wlanIbssPsBuffered = 0;
		}
		if (MAC_500_SLEEP_CONTROL & 1) {
			wlanMacRing = MAC_2E8_RING;
			connection = wlanConnectionSlotGet(wlanMacRing);
			if (connection != NULL) {
				if (++connection->beaconsSinceDtim != connection->beaconsPerDtim ||
				    (connection->beaconsSinceDtim = 0, connection->dtimHold != 0)) {
					MAC_2F4_RINGS = (MAC_2F4_RINGS & ~(1u << wlanMacRing)) | 0x100;
				} else {
					MAC_2F4_RINGS |= (1u << connection->groupRing) | 0x100;
					if (connection->mode == 2) {
						MAC_648_CONTEXT = connection->groupRing;
						if (MAC_610_CIPHER == 1 || MAC_620_EVENT == 1)
							wlanApPsSendGroup(connection, 1);
					}
				}
			}
			/* The original reads the connection's type even when it found none. */
			if (connection->interfaceType == WLAN_IF_AP) {
				struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

				if (ps->p2pEntryPending != 0) {
					p2pDirectUpdateEntry(connection, 8);
					ps->p2pEntryPending = 0;
				}
			}
			if (wlanApPsEnabled())
				wlanRaiseEvent(6);
		}
		MAC_510_EVENTS = ~MAC_EVENT_NODE_B;
	}
	if (events & MAC_EVENT_IBSS_PS) {
		if (wlanConnectionExists(WLAN_IF_IBSS)) {
			wlanIbssPsBuffered = 1;
			wlanPostConnectionEvent(NULL, 0x4000);
		}
		MAC_510_EVENTS = ~MAC_EVENT_IBSS_PS;
	}
	if (events & MAC_EVENT_ACK_ONLY_00)
		MAC_510_EVENTS = ~MAC_EVENT_ACK_ONLY_00;
	if (events & MAC_EVENT_TX_DONE) {
		wlanMacTxDoneCount++;
		macTxPostEvent(0x10);
		MAC_510_EVENTS = ~MAC_EVENT_TX_DONE;
		events &= ~MAC_EVENT_TX_NOTIFY;
		MAC_510_EVENTS = ~MAC_EVENT_TX_NOTIFY;
	}
	if (events & MAC_EVENT_TX_NOTIFY)
		osaEventFlagsSet(macTxNotifyEventFlags, 1, 7);
	if (events & MAC_EVENT_COUNT_04) {
		wlanMacCount04++;
		MAC_510_EVENTS = ~MAC_EVENT_COUNT_04;
	}
	if (events & MAC_EVENT_COUNT_08) {
		wlanMacCount08++;
		MAC_510_EVENTS = ~MAC_EVENT_COUNT_08;
	}
	if (events & MAC_EVENT_COUNT_10) {
		wlanMacCount10++;
		MAC_510_EVENTS = ~MAC_EVENT_COUNT_10;
	}
	if (events & MAC_EVENT_COUNT_20) {
		wlanMacCount20++;
		MAC_510_EVENTS = ~MAC_EVENT_COUNT_20;
	}
	if (wlanPsSleepTimeout == 0)
		clockHostApplyTarget();
	SOC_IRQ_ENABLE_SET = SOC_IRQ_ENABLE_SET | (1u << 10);
}

/* Line 10: the MAC has something to report, unless the coexistence mask keeps it. */
int wlanMacIrq(int line, uint32_t pending)
{
	if (!sysSleepVotesAllowed(16)) {
		SOC_IRQ_ENABLE_CLEAR = 1u << 10;
		wlanMacEvents(line, pending);
	}
	return 1;
}

/* Brings the MAC back after the radio was retuned (reason 0: the host changed the channel). */
void wlanMacResume(unsigned int reason)
{
	UINT posture;

	romLibResolveState();
	wlanMacE80Bit2Clear();
	sysDelayUs(20);
	if (reason == 0)
		wlanDmaRingsDrain();
	posture = sysIrqDisable();
	wlanMacDmaRestart();
	wlanDmaRingClearPending();
	wlanMacEventMasksSet();
	if (reason == 0)
		romLibApplyState();
	wlanMacE80Bit2Set();
	sysIrqRestore(posture);
}

/* Resets the MAC's state (what the radio's retune left behind), counted and logged. */
void wlanMacFlush(void)
{
	UINT posture;

	wlanMacFlushCount++;
	sysLogStatistic(0x12, wlanMacTxDoneCount, 0);
	posture = sysIrqDisable();
	wlanMacReset();
	sysIrqRestore(posture);
}

/* The host's sleep is over: the power save state and the event the wake-up raises. */
void wlanMacSleepExit(void)
{
	sleepExit();
	btSdioSetEnabled(0);
	if (wlanRoleOtherOnly() && wlanPsEnabled() && (wlanHostSleepArmed() || wlanPsSessionOpen()) &&
	    !wlanApPsEnabled()) {
		if (wlanHostSleepArmed())
			wlanPsSleepTimeout = 0;
		wlanPsSleepSequence = 0;
		wlanPsHostWakeDue = 1;
	}
	wlanPsSetPowerState(2);
	hostCmd0074Arm();
	mdevIoctl(wlanSdioDev, 25, 0);
	if (wlanHostSleepArmed()) {
		wlanMefConfig.hostState = WLAN_HOST_LEAVING;
		wlanHostSleepCancel();
		if (wlanHostSleepEndOnWake != 0) {
			wlanHostSleepEndOnWake = 0;
			wlanMefConfig.hostState = WLAN_HOST_AWAKE;
			wlanMefConfig.wakeSignalled = 0;
		} else {
			wlanHostSleepEnter();
		}
	}
	if (wlanPsDtimPolicy == 0xffff) {
		unsigned int role = 0;

		if (wlanHasApRole())
			role = 2;
		wlanPostConnectionEvent(wlanConnectionByRole(role), 0x1000);
	} else {
		wlanRaiseEvent(wlanHasApRole() ? 4 : 3);
	}
	wlanMacSleepDone();
}

/* The radio restarts: the frames of the sleep are verified, and the host or the access point is woken. */
void wlanMacRestart(void)
{
	if (wlanConnectionByRole(0) != NULL)
		wlanPsVerifyFrame(1);
	if (wlanRoleIbssOnly() && wlanPsEnabled())
		wlanIbssPsBuffered = 0;
	if (wlanRoleOtherOnly() && wlanPsEnabled() && wlanPsDtimPolicy != 0xffff)
		wlanApPsEnabled();
	sleepExit();
	btSdioSetEnabled(0);
	if (wlanConnectionByRole(0) != NULL)
		wlanPsVerifyFrame(2);
	if (wlanRoleOtherOnly() && wlanPsEnabled() && (wlanHostSleepArmed() || wlanPsSessionOpen()) &&
	    !wlanApPsEnabled()) {
		if (wlanPsHostWakeDue != 0 || wlanSubscribeEventsPending(wlanConnectionByRole(0)) ||
		    wlanPsSleepBlocked() != 0 || mdevIoctl(wlanSdioDev, 27, 0) != 0) {
			wlanPsSleepTimeout = 0;
			wlanPsSleepSequence = 0;
			wlanRaiseEvent(3);
		} else {
			if (wlanSessionId != 0)
				wlanPsSleepSequence = 1;
			if (wlanHostSleepArmed())
				wlanPsSleepTimeout = 1;
		}
	} else {
		if (wlanApPsEnabled() && wlanPsHostWakeDue != 0) {
			wlanPsHostWakeDue = 0;
			wlanRaiseEvent(wlanSessionId == 0 ? 4 : 3);
		} else {
			wlanRaiseEvent(3);
		}
	}
	wlanMacSleepDone();
}

/* Re-establishes the MAC's state: channel, calibration, block acknowledgements, transmit windows. */
void wlanMacReset(void)
{
	uint8_t flags[4];

	MAC_2D4_FLAGS |= 0x30;
	MAC_8E4_ACTIVE_NODE |= 2;
	if (!(wlanPrimaryEvents & 1))
		sleepMacResume();
	wlanCurrentChannelFlags(flags);
	rfSleepHook();
	wlanSetChannel(wlanCurrentChannel(), flags, 0);
	wlanAmpTxPacketsAdjust();
	wlanBaHwReset();
	wlanTxInFlightPurge();
	macTxBaResyncSequences();
	wlanTxWindowsClearPending();
	wlanPsPoll.sent = 0;
	wlanPsPoll.sleepAfterTx = 0;
	wlanTxRingClearCounters();
	wlanPsPoll.triggered = 0;
	wlanCoexClearCredits();
	if (!(wlanPrimaryEvents & 1))
		sleepMacHalt();
	MAC_2D4_FLAGS &= ~0x30u;
	MAC_8E4_ACTIVE_NODE &= ~2u;
}

/* The host sleep may start: the RF is locked again and the host is told. */
void wlanMacSleepDone(void)
{
	sleepRfRelock();
	osaEventFlagsSet(wlanHostSleepFlags, 1, 7);
}
