/*
 * The "MAC Tx Notify" task (macTxNotifyTaskMain()): what happens to a frame
 * the MAC has finished sending. The transmit completion ring is read frame
 * by frame; each is counted, the power-save, coexistence and subscribed-event
 * state learns of it, and the frame is completed towards the host or freed.
 * wpaSuppScheduleParam() is what it does with a management frame of an access
 * point. The file name is inferred.
 */
#include <stddef.h>
#include "robin/ratesel.h"
#include "robin/apps.h"
#include "robin/txhold.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/coex.h"
#include "robin/esa.h"
#include "robin/htba.h"
#include "robin/keyMgmtAp.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/pssm.h"
#include "robin/psctl.h"
#include "robin/pspoll.h"
#include "robin/rom.h"
#include "robin/slot.h"
#include "robin/stapssm.h"
#include "robin/subscribe.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/txring.h"
#include "robin/txstats.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

_Static_assert(offsetof(struct WlanConnectionInfo, sleep) + 4 == 1148, "sleep mode");

/* The MAC's transmit completion ring: where the firmware is and where the hardware is (inferred). */
#define MAC_TX_STATUS_HEAD	MAC_REG(0x010)
#define MAC_TX_STATUS_TAIL	MAC_REG(0x014)
#define MAC_510_EVENTS		MAC_REG(0x510)
#define MAC_EVENT_TX_NOTIFY	0x00008000u	/* the transmit notification task has work */

/* Frame control: the type (bits 2-3) and the subtype of the first byte. */
#define FC0_TYPE(fc)		(((unsigned int)(fc) << 28) >> 30)
#define FC0_SUBTYPE(fc)		((unsigned int)(fc) >> 4)
#define FC_TYPE_MANAGEMENT	0
#define FC_TYPE_CONTROL		1
#define FC_TYPE_DATA		2
#define FC1_POWER_MANAGEMENT	0x10

/* What the frame was queued for (TX_DATA_TAG): the host's data, a null frame of the power save machine, ... */
#define TX_TAG_NULL		0x10
#define TX_TAG_NULL_IEEE	0x11
#define TX_TAG_PROBE		0x12
#define TX_TAG_PROBE_RETRY	0x13
#define TX_TAG_NULL_RETRY	0x14
#define TX_TAG_NULL_QOS		0x15
#define TX_TAG_NULL_EXIT	0x16
#define TX_TAG_AP_NULL		20
#define TX_TAG_REPLY		48
#define TX_TAG_MGMT		90

/* Not converted yet; provisional names. */
void p2pDirectPrepareTable(struct WlanConnection *connection, struct WlanTxStatus *status); /* 0x41f6a */
void p2pSqramPrepareContext(OsaBuffer *buffer, int acknowledged);			/* SQRAM 0xc000cac0 */
void macMgmtConvertPacket(struct WlanConnection *connection);			/* inferred */
extern uint32_t wlanPsPollSkips;

/* What the task counts, and when the Bluetooth radio was last given the air for a probe. */
uint32_t macTxResetMarker __attribute__((section(".dtcm.bss")));	/* 0x04000170: completions with no frame */
uint32_t wlanTxNotifyRearms __attribute__((section(".dtcm.bss")));	/* 0x04000174: rounds that found the ring not empty */
uint32_t wlanPsPollSleeps __attribute__((section(".dtcm.bss")));	/* 0x04000178: sleeps after the PS-Poll was sent */
uint8_t wlanTxDoneDescriptor __attribute__((section(".dtcm.bss")));	/* 0x040088b7: what wlanPsSchedulePacket() said of the last frame */
uint8_t wlanTxDoneQueue __attribute__((section(".dtcm.bss")));		/* 0x040088b8 */
uint64_t wlanCoexProbeTime __attribute__((section(".dtcm.bss")));	/* 0x04004e70 */

/*
 * What an access point does with a management frame it sent, by subtype: a re-authentication's
 * connection state is converted (5), an authentication's buffer acquired (11), a deauthentication
 * frees what was kept for the station and may have to wait for the buffer (12), an association
 * response starts the handshake for a station that needs one (1 and 3). Returns 0 when the frame
 * is not to be freed.
 */
int wpaSuppScheduleParam(struct WlanConnection *connection, const uint8_t *frame, const uint8_t *data)
{
	int release = 1;
	unsigned int subtype = FC0_SUBTYPE(frame[2]);

	if (subtype == 5) {
		if (connection->interfaceType == 2) {
			macMgmtConvertPacket(connection);
			return 0;
		}
	} else if (subtype > 5) {
		if (subtype == 11) {
			if ((frame[37] << 8 | frame[36]) != 0)
				macMgmtAcquireBuffer(connection);
		} else if (subtype == 12) {
			unsigned int tag;

			macMgmtManageBuffer(connection, frame + 6);
			tag = data[109];
			if (tag == 51 || tag == 52) {
				unsigned int kind = 32;

				if (tag != 51)
					kind = 34;
				while (macMgmtBuildBuffer(connection, kind) == 1)
					osaTaskSleep(1);
			}
		}
	} else if (subtype == 1 || subtype == 3) {
		struct WlanConnectionRole *role = (struct WlanConnectionRole *)((uint8_t *)wlanConnectionInfoSlot(connection) + 12);

		if (wlanInterfaceNeedsSecurity(connection) != 0 && (connection->flags2 & WLAN_CONN2_ASSOCIATED) &&
		    (role->keyMgmt.staSecType.wpa || role->keyMgmt.staSecType.wpa2))
			GenerateApEapolMsg(connection, 1);
	}
	return release;
}

/* The task: waits for the MAC to say frames are done and sees to each. */
void macTxNotifyTaskMain(void)
{
	struct WlanTxStatus status;
	unsigned int actual;
	unsigned int sleeps = 0;
	unsigned int event = 0;

	for (;;) {
		do {
			do
				osaEventFlagsWait(macTxNotifyEventFlags, 3, 8, &actual, 0xffffffff);
			while (actual == 0);
		} while (!(actual & 1));
		for (;;) {
			OsaBuffer *buffer;
			struct WlanConnection *connection;
			uint8_t *data, *frame;
			uint32_t posture;
			int removed, release;
			unsigned int oldMetadata;

			buffer = wlanTxCompletionFetch(&status);
			if (buffer == NULL)
				break;
			connection = buffer->context;
			if (connection == NULL) {
				sysIrqDisable();
				for (;;)
					;
			}
			if (wlanPsPollTxStatus(connection, status.data, &status) == 1)
				break;
			data = status.data;
			if (connection->interfaceType == 1 && !(TX_DATA_FRAME(data) ? ((uint8_t *)TX_DATA_FRAME(data))[6] & 1 : 0))
				wlanIbssResetPeerCheck(connection);
			posture = sysIrqDisable();
			removed = data != NULL && wlanTxInFlightRemove(buffer) != 0;
			sysIrqRestore(posture);
			release = 1;
			frame = (uint8_t *)TX_DATA_FRAME(data);
			oldMetadata = buffer->poolMetadata;
			buffer->poolMetadata = (status.retries << 8) | status.result;
			if (removed) {
				if (!(frame[6] & 1) && connection->mode == 2 && FC0_TYPE(frame[2]) == FC_TYPE_DATA) {
					wlanApPsTxDone(connection, data);
					if (status.result == 0)
						wlanConnectionUpdateListenInterval(connection);
				}
				if (wlanApPsEnabled()) {
					unsigned int tag = TX_DATA_TAG(data);

					if (tag != TX_TAG_AP_NULL && tag != 21 && status.result == 0) {
						unsigned int fc = frame[2];

						if (FC0_TYPE(fc) != 0 || FC0_SUBTYPE(fc) != 5)
							wlanPsActivity(wlanConnectionByRole(2));
					}
				}
			}
			if (((struct WlanReplyFrame *)data)->descriptor.control & 0x20000000) {
				if (!removed)
					goto next;
				wlanTxWindowComplete(TX_DATA_RING(data));
				if (wlanPsEnabled()) {
					if (!(frame[3] & FC1_POWER_MANAGEMENT) || connection->interfaceType == 1 || wlanCoexUseP2pSleepMode() ||
					    wlanPsLegacyPollMode()) {
						++wlanPsIeeeFlag;
						wlanPsArmWakeTimer();
					}
				}
				wlanCoexTxCreditReturned();
				if (wlanPsLegacyPollMode() && !wlanCoexUseStationMode() && wlanPsLastTxComplete != 0 &&
				    wlanPsPollSkips < 3)
					wlanPsEnterSleep(connection, 0);
				wlanCompletePacket(buffer, 0);
				goto next;
			}
			if (!removed) {
				++macTxResetMarker;
				goto next;
			}
			wlanSlotRunningEvent((uint32_t)(uintptr_t)&status);
			if ((int)((struct WlanReplyFrame *)data)->descriptor.control < 0)
				wlanPsHostFrameSeen();
			if (FC0_TYPE(frame[2]) == FC_TYPE_DATA) {
				uint16_t scheduled;
				unsigned int tag;
				unsigned int subtype;

				wlanSubscribeCheckFailures(connection, &status);
				wlanTxWindowComplete(TX_DATA_RING(data));
				scheduled = wlanPsSchedulePacket(connection, &status);
				wlanTxDoneQueue = scheduled >> 8;
				wlanTxDoneDescriptor = scheduled;
				wlanTxStatsCount(connection, &status);
				wlanSubscribeCheckLinkRate(connection, &status);
				if (connection->mode == 3 || connection->mode == 1)
					p2pDirectPrepareTable(connection, &status);
				if (wlanPsEnabled()) {
					if (!(frame[3] & FC1_POWER_MANAGEMENT) || connection->interfaceType == 1 ||
					    wlanCoexUseP2pSleepMode() || wlanPsLegacyPollMode()) {
						++wlanPsIeeeFlag;
						wlanPsArmWakeTimer();
					}
				}
				wlanCoexTxCreditReturned();
				if (wlanPsLegacyPollMode() && !wlanCoexUseStationMode() && wlanPsLastTxComplete != 0 &&
				    wlanPsPollSkips < 3)
					wlanPsEnterSleep(connection, 0);
				if (wlanApPsEnabled() && TX_DATA_TAG(data) == TX_TAG_AP_NULL) {
					unsigned int mode = wlanConnectionInfo(connection)->sleep.mode;

					if (status.result != 0) {
						if (mode != WLAN_PS_SLEEP_DTIM)
							event = 30;
					} else if (mode == WLAN_PS_SLEEP_DTIM) {
						event = 18;
					} else {
						event = 29;
					}
				}
				subtype = FC0_SUBTYPE(frame[2]);
				if (subtype == 4 || subtype == 12) {
					switch (TX_DATA_TAG(data)) {
					case TX_TAG_NULL:
					case TX_TAG_NULL_QOS:
						if (status.result != 0)
							event = 16;
						else if (frame[3] & FC1_POWER_MANAGEMENT)
							event = 14;
						else
							event = 13;
						break;
					case TX_TAG_PROBE:
						if (status.result == 0 && wlanCoexBtNeedsSharing() &&
						    (frame[3] & FC1_POWER_MANAGEMENT))
							wlanCoexProbeTime = romGetTimerTimestamp();
						break;
					case TX_TAG_PROBE_RETRY:
						if (status.result != 0)
							event = 40;
						else
							event = 39;
						break;
					default:
						break;
					}
				}
				tag = TX_DATA_TAG(data);
				if (tag != TX_TAG_REPLY) {
					subtype = FC0_SUBTYPE(frame[2]);
					if (subtype != 4 && subtype != 12 && tag != TX_TAG_MGMT) {
						wlanCompletePacket(buffer, (oldMetadata << 16) | status.result);
						goto next;
					}
					osa_BufferFree(buffer);
					goto next;
				}
				wlanReplyFrameFree(buffer);
			} else if (FC0_TYPE(frame[2]) == FC_TYPE_MANAGEMENT) {
				unsigned int subtype = FC0_SUBTYPE(frame[2]);
				int acknowledged = 0;
				int sendAck = 0;

				if (connection->mode == 2) {
					release = wpaSuppScheduleParam(connection, frame, data);
					if (FC0_SUBTYPE(frame[2]) == 13) {
						wlanBaActionSent(buffer, status.result == 0);
						if (FC0_SUBTYPE(frame[2]) == 13 && connection->interfaceType == 3) {
							acknowledged = 1;
							sendAck = 1;
						}
					}
				} else {
					switch (subtype) {
					case 5:
						if (connection->interfaceType == 4) {
							*((uint8_t *)connection + 940) = 0;
							release = 0;
						}
						break;
					case 9:
						release = 0;
						macMgmtFilterHeader();
						break;
					case 13:
						wlanBaActionSent(buffer, status.result == 0);
						if (connection->interfaceType == 3) {
							acknowledged = 1;
							sendAck = 1;
						}
						break;
					default:
						break;
					}
				}
				if (sendAck) {
					if (status.result != 0)
						acknowledged = 0;
					p2pSqramPrepareContext(buffer, acknowledged);
				}
				actual = 0;
				if (release != 0)
					osa_BufferFree(buffer);
			} else if (FC0_TYPE(frame[2]) == FC_TYPE_CONTROL && FC0_SUBTYPE(frame[2]) == 10) {
				uint32_t irq = sysIrqDisable();

				wlanPsPollFrameDone();
				if (wlanPsPoll.sleepAfterTx != 0) {
					++wlanPsPollSleeps;
					wlanPsPoll.sleepAfterTx = 0;
					wlanPsEnterSleep(connection, 0);
				}
				osa_BufferFree(buffer);
				if (status.result == 0 || sleeps >= 10 || wlanPsDisabled()) {
					sleeps = 0;
				} else {
					sleeps = (uint8_t)(sleeps + 1);
					if (!wlanPsTriggerEnabled())
						wlanPsEnterSleep(connection, 0);
				}
				sysIrqRestore(irq);
			}
next:
			if (event != 0) {
				wlanRaiseEvent(event);
				event = 0;
			}
		}
		MAC_510_EVENTS = ~MAC_EVENT_TX_NOTIFY;
		if (MAC_TX_STATUS_HEAD != MAC_TX_STATUS_TAIL) {
			++wlanTxNotifyRearms;
			osaEventFlagsSet(macTxNotifyEventFlags, 1, 7);
		}
	}
}
