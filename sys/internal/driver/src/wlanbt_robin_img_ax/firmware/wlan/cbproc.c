/*
 * The "CB Proc" control task (0x1ae88..): it takes the events other tasks
 * post with wlanPostConnectionEvent() (a connection and an event mask, in a
 * buffer of wlanEventPool) and runs what they ask: the host's commands, the
 * answers of the management task to the SME's commands, power save and host
 * sleep. The names are inferred from use; the event masks are this
 * firmware's own.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/led.h"
#include "robin/ratesel.h"
#include "robin/buffer.h"
#include "robin/txhold.h"
#include "robin/txpower.h"
#include "robin/conn.h"
#include "robin/coex.h"
#include "robin/esa.h"
#include "robin/hostcmd.h"
#include "robin/hostps.h"
#include "robin/icverr.h"
#include "robin/join.h"
#include "robin/keyMgmtAp.h"
#include "robin/mactx.h"
#include "robin/mdev.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/pssm.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/session.h"
#include "robin/runtime.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/subscribe.h"
#include "robin/sync.h"
#include "robin/eventpower.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/threadx.h"
#include "robin/btrom.h"
#include "robin/wlantask.h"
#include "robin/stamachine.h"

/* The events of the control task's queue (wlanPostConnectionEvent()). */
#define CBPROC_EVENT_SLEEP_TIMEOUT	0x00000001	/* the host sleep timeout of the power save ran out */
#define CBPROC_EVENT_HOST_COMMAND	0x00000002	/* the host's commands wait in wlanDriverQueue1 */
#define CBPROC_EVENT_PS_HEADER		0x00000008
#define CBPROC_EVENT_SME_RESPONSE	0x00000080	/* an answer waits in cbProcSmeQueue */
#define CBPROC_EVENT_MGMT_STATUS	WLAN_EVENT_HOST_PS_PEER
#define CBPROC_EVENT_HOST_AWAKE		WLAN_EVENT_HOST_PS_AWAKE
#define CBPROC_EVENT_HOST_SLEEP		WLAN_EVENT_HOST_PS_SLEEP
#define CBPROC_EVENT_PS_BUFFER		0x00004000
#define CBPROC_EVENT_PS_TABLE_3		0x00008000
#define CBPROC_EVENT_PS_TABLE_5		0x00020000
#define CBPROC_EVENT_PS_VERIFY		0x00200000
#define CBPROC_EVENT_SLEEP_REFUSED	0x02000000
#define CBPROC_EVENT_PENDING_EVENTS	0x04000000	/* wlanPendingEvents are due */

#define SDIO_IOCTL_HOST_SLEEP_CONFIG	30
#define CBPROC_PENDING_EVENT_BITS	48

/* Marvell's HostCmd_DS_COMMAND responses of the commands the management task answers. */
#define HOSTCMD_RESP_SCAN			0x8006
#define HOSTCMD_RESP_ASSOCIATE			0x8012
#define HOSTCMD_RESP_DEAUTHENTICATE		0x8024
#define HOSTCMD_RESP_AD_HOC_STOP		0x8025
#define HOSTCMD_RESP_AD_HOC_START		0x802b
#define HOSTCMD_RESP_AD_HOC_JOIN		0x802c
#define HOSTCMD_RESP_8040			0x8040
#define HOSTCMD_RESP_8060			0x8060
#define HOSTCMD_RESP_8061			0x8061
#define HOSTCMD_RESP_8062			0x8062
#define HOSTCMD_RESP_806E			0x806e
#define HOSTCMD_RESP_80AF			0x80af
#define HOSTCMD_RESP_80B1			0x80b1
#define HOSTCMD_RESP_80B2			0x80b2
#define HOSTCMD_RESP_80B5			0x80b5
#define HOSTCMD_RESP_80C3			0x80c3
#define HOSTCMD_RESP_80CE			0x80ce
#define HOSTCMD_RESP_80D0			0x80d0
#define HOSTCMD_RESP_80E9			0x80e9
#define HOSTCMD_RESP_80FE			0x80fe

#define CBPROC_U16(p)		((uint16_t)((p)[0] | (p)[1] << 8))

/* The host's commands the task found nothing to run for. */
static uint32_t cbProcCommandRetries __attribute__((section(".itcm.rw")));		/* 0x54ba0 */

/* Not converted yet; provisional names. */
void wlanPsApplyHeader(void);								/* 0x3e0c0 */
void macMgmtValidateTable(int reason);							/* 0x3726c */
void wlanAdHocResponseBssid(uint8_t *response, const uint8_t *command);			/* 0x2a8b0 */
unsigned int macMgmtClearEntry(uint8_t *response, const uint8_t *scan);			/* 0x36e08 */
void wlanMacFlush(void);									/* 0x1c840 */
extern uint8_t wlanScanRequested;							/* 0x54c80 */
extern uint16_t wlanIbssAtimWindow;							/* 0x54b94 */
extern uint8_t *hostCmdResponseData;						/* 0x54bbc */
extern uint8_t hostCmdBusy;								/* 0x54b8c */
extern OsaBuffer *hostCmdResponse;							/* 0x54b9c */
extern uint32_t hostCmdMacAwake;							/* 0x54ba4 */
extern struct WlanDriverQueue wlanDriverQueue1;
extern uint32_t wlanPsSavedDtimPolicy;							/* 0x54b58 */

/*
 * The host wakes up (host sleep ends): tell it so, in the phase where the
 * countdown to its next sleep runs, and go back to the normal DTIM policy.
 */
void wlanPsHostAwakeAnnounce(struct WlanConnection *connection)
{
	if (wlanConnectionPhase != 0) {
		wlanConnectionPhase = 2;
		wlanHostEventSendRelease(connection, WLAN_HOST_EVENT_PS_AWAKE, wlanPsHostSessionStart);
	}
	macMgmtValidateTable(8);
	wlanPsDtimPolicy = wlanPsSavedDtimPolicy;
}

/*
 * A MIC failure was found: in the mode where the host runs the
 * countermeasures (mode 2) the control task takes the frame's header,
 * otherwise the host is told of the error (a group or a unicast key's) and
 * the supplicant starts its countermeasures.
 */
void wlanMicErrorReport(struct WlanConnection *connection, int group)
{
	if (connection->mode == 2) {
		ApMicCounterMeasureInvoke(connection);
		return;
	}
	wlanHostEventSendPlain(connection, group != 0 ? WLAN_HOST_EVENT_MIC_ERR_MULTICAST :
						     WLAN_HOST_EVENT_MIC_ERR_UNICAST);
	if (supplicantIsEnabled())
		supplicantMICCounterMeasureInvoke(connection, group == 0);
}

/*
 * The controlled port opens (a key was installed): while the DTIM policy
 * waits for the host (0xffff) the host is told it is awake, or, with no
 * session and no phase, that the deep sleep ended; the event itself is then
 * posted to the control task.
 */
void wlanPortOpenNotify(struct WlanConnection *connection, uint32_t event)
{
	if (wlanPsDtimPolicy == 0xffff) {
		if ((wlanSessionId | wlanConnectionPhase) != 0) {
			if (wlanConnectionPhase != 0 && wlanConnectionPhase != 6)
				wlanConnectionPhase = 2;
			wlanHostEventSendRelease(connection, WLAN_HOST_EVENT_PS_AWAKE, wlanPsHostSessionStart);
			wlanPsDtimPolicy = wlanPsSavedDtimPolicy;
		} else {
			wlanHostEventSendPlain(connection, WLAN_HOST_EVENT_DEEP_SLEEP_AWAKE);
		}
	}
	wlanPostConnectionEvent(connection, event);
}

/*
 * The power save machines ask for the response of a command that set the
 * power save mode (0x21, 0xe4) to be sent without waiting for them.
 */
void wlanPsHostCmdRespond(struct WlanConnection *connection)
{
	uint8_t *data = hostCmdResponseData;

	if (hostCmdCurrentCode() == 0x21 && CBPROC_U16(data) != 0x34) {
		hostCmdRespond(connection, NULL);
		return;
	}
	if (hostCmdCurrentCode() == 0xe4 && CBPROC_U16(data) != 5)
		hostCmdRespond(connection, NULL);
}

/*
 * The management task's answers to the commands the SME sent: each is given
 * the sequence number and the BSS of the host command being run and goes to
 * the host as that command's response.
 */
static inline void cbProcRespondSme(void)
{
	OsaBuffer *smeBuffer;
	uint8_t *command;
	uint8_t *response;
	struct WlanConnection *connection;
	unsigned int size;
	unsigned int code;

	for (;;) {
		if (osaMsgQueueReceive(cbProcSmeQueue, (uintptr_t *)&smeBuffer, 4, 0) == 15)
			return;
		response = (uint8_t *)hostCmdResponse + hostCmdResponse->dataOffset;
		connection = smeBuffer->context;
		command = (uint8_t *)smeBuffer + smeBuffer->dataOffset;
		command[4] = response[4];
		command[5] = (connection->bssType << 4) | (connection->bssNumber & 0xf);
		code = CBPROC_U16(command);

		switch (code) {
		case HOSTCMD_RESP_SCAN:
			sysIssuePrimaryBbpCommand((uint32_t)response, (uint32_t)command, 8);
			response[2] = 8;
			response[3] = 0;
			size = macMgmtClearEntry(response + 8, (const uint8_t *)__aeabi_uread4(command + 9));
			*(uint16_t *)(response + 2) = size + CBPROC_U16(response + 2);
			if (connection->interfaceType == 3 && connection->mode != 0 &&
			    connection->localName.length != 4)
				connection->localName.length = 2;
			wlanScanRequested = 0;
			hostCmdRespond(connection, NULL);
			break;
		case HOSTCMD_RESP_ASSOCIATE:
		case HOSTCMD_RESP_AD_HOC_STOP:
			sysIssuePrimaryBbpCommand((uint32_t)response, (uint32_t)command, 8);
			response[2] = 8;
			response[3] = 0;
			size = wlanJoinGetResponse(response + 8) + CBPROC_U16(response + 2);
			response[2] = size;
			response[3] = size >> 8;
			hostCmdRespond(connection, wlanJoinReported);
			break;
		case HOSTCMD_RESP_AD_HOC_START:
		case HOSTCMD_RESP_AD_HOC_JOIN:
			wlanAnyConnectionHostFlag8();
			romLibScheduleState();
			sysIssuePrimaryBbpCommand((uint32_t)response, (uint32_t)command, CBPROC_U16(command + 2));
			wlanAdHocResponseBssid(response, command);
			if (command[19] == 0 || command[8] == 0) {
				macMgmtPrepareDescriptor(wlanIbssAtimWindow);
				if (wlanIbssAtimWindow != 0)
					wlanPsClearTable(connection, 0);
			}
			hostCmdRespond(connection, NULL);
			break;
		case HOSTCMD_RESP_80AF:
			if (wlanConnectionsIdle()) {
				wlanTxPowerSetHostLimit(255, 0);
				macMgmtSyncHeader(connection);
			}
			/* fall through */
		case HOSTCMD_RESP_DEAUTHENTICATE:
		case HOSTCMD_RESP_8040:
		case HOSTCMD_RESP_8060:
		case HOSTCMD_RESP_8061:
		case HOSTCMD_RESP_8062:
		case HOSTCMD_RESP_806E:
		case HOSTCMD_RESP_80B1:
		case HOSTCMD_RESP_80B2:
		case HOSTCMD_RESP_80C3:
		case HOSTCMD_RESP_80CE:
		case HOSTCMD_RESP_80E9:
		case HOSTCMD_RESP_80FE:
			sysIssuePrimaryBbpCommand((uint32_t)response, (uint32_t)command, CBPROC_U16(command + 2));
			hostCmdRespond(connection, NULL);
			break;
		case HOSTCMD_RESP_80B5:
			response[0] = 0xb5;
			response[1] = 0x80;
			response[6] = command[6];
			response[7] = command[7];
			hostCmdRespond(connection, NULL);
			break;
		case HOSTCMD_RESP_80D0:
			sysIssuePrimaryBbpCommand((uint32_t)response, (uint32_t)command, 8);
			sysIssuePrimaryBbpCommand((uint32_t)(response + 8), (uint32_t)(command + 8), 12);
			hostCmdRespond(connection, NULL);
			break;
		default:
			break;
		}
		osa_BufferFree(smeBuffer);
	}
}

/* The events of the subscribed kinds that are due (the bits of wlanPendingEvents). */
static void cbProcPostPendingEvents(void)
{
	uint32_t posture = sysIrqDisable();
	uint32_t high = wlanPendingEvents[1];
	uint32_t low = wlanPendingEvents[0];
	unsigned int bit;

	wlanPendingEvents[0] = 0;
	wlanPendingEvents[1] = 0;
	sysIrqRestore(posture);
	for (bit = 0; bit < CBPROC_PENDING_EVENT_BITS; bit = (uint8_t)(bit + 1)) {
		if ((low | high) == 0)
			break;
		if (low & 1)
			wlanPsPostEvent(bit);
		low = (low >> 1) | (high << 31);
		high >>= 1;
	}
}

/* The events 0x200 (a peer's report): the status of the management exchange goes to the host. */
static inline void cbProcReportMgmt(struct WlanConnection *connection)
{
	if (connection->mode == 3 || connection->mode == 1) {
		if (wlanMgmtReport.code == 3) {
			wlanHostEventSendCode(connection, wlanMgmtReport.code, wlanMgmtReport.reason);
		} else if (wlanMgmtReport.code == 8 || wlanMgmtReport.code == 9) {
			wlanHostEventSendCodeAddress(connection, wlanMgmtReport.code, wlanMgmtReport.reason,
						     wlanMgmtReport.peer);
			wlanMgmtReport.code = 0;
			return;
		} else {
			wlanHostEventSendCode(connection, 3, 0);
		}
		wlanMgmtReport.code = 0;
	} else {
		ledFirmwareStateSet(5);
		wlanHostEventSendPlain(connection, 17);
	}
}

/* The host wakes (event 0x1000 after wlanHostSleepFlags) or goes to sleep. */
static void cbProcHostAwake(struct WlanConnection *connection)
{
	uint32_t actual;

	osaEventFlagsWait(wlanHostSleepFlags, 1, 8, &actual, 0);
	if (wlanPsDtimPolicy == 0xffff)
		wlanPsHostAwakeAnnounce(connection);
	else
		wlanHostEventSendRelease(connection, WLAN_HOST_EVENT_PS_AWAKE, wlanPsHostSessionStart);
	macTxPostEvent(1);
	wlanPsPhaseArm(connection);
}

/* The host's sleep: the phase, announced to the host with PS_SLEEP. */
static inline void cbProcHostSleep(struct WlanConnection *connection)
{
	if (wlanConnectionPhase == 0 || wlanConnectionPhase == 6) {
		if (wlanSessionId == 0)
			return;
		if (hostCmdBusy || osa_MsgQueuePeek(&wlanDriverQueue1) != NULL ||
		    wlanPsGetOverride() != 0)
			wlanPsConfirmNow();
		else
			wlanHostEventSendRelease(connection, WLAN_HOST_EVENT_PS_SLEEP, wlanPsNoteHostActive);
	} else {
		uint32_t posture = sysIrqDisable();

		if (wlanConnectionPhase == 3) {
			wlanConnectionPhase = 4;
			wlanHostEventSendRelease(connection, WLAN_HOST_EVENT_PS_SLEEP, wlanPsNoteHostActive);
		} else if (wlanConnectionPhase == 1) {
			wlanConnectionPhase = 0;
		}
		sysIrqRestore(posture);
	}
}

/* The host's sleep is about to start: the DTIM policy follows it, the MAC and the radio rest. */
static inline void cbProcHostSleepEnter(struct WlanConnection *connection)
{
	wlanPsSavedDtimPolicy = wlanPsDtimPolicy;
	wlanPsDtimPolicy = 0xffff;
	if (wlanMefConfig.wakeSignalled != 0) {
		wlanHostSleepActive = &wlanHostSleepSettings;
		wlanMefConfig.activated = 1;
		mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_SLEEP_CONFIG, 0);
	}
	hostCmdUnlock();
	wlanMacFlush();
	macMgmtValidateTable(7);
	btSdioSetEnabled(1);
	if (wlanPsIdleSleep() != 0)
		return;
	wlanPsHostAwakeAnnounce(connection);
	wlanPsPhaseArm(connection);
}

/* Events of the control task that are host events: the connection state the host is told of. */
#define CBPROC_EVENT_LINK_SENSED	0x00000400
#define CBPROC_EVENT_HS_ACT_REQ		0x00040000
#define CBPROC_EVENT_WMM_STATUS		0x40000000
#define CBPROC_EVENT_IBSS_COALESCED	0x80000000u
#define CBPROC_EVENT_STATUS_41		0x00000010
#define CBPROC_EVENT_PORT_RELEASE	0x00000040

/* The host event of an event the control task has no case for; 0 when it is none of them. */
int cbProcEventToHost(struct WlanConnection *connection, unsigned int event)
{
	unsigned int hostEvent;

	switch (event) {
	case CBPROC_EVENT_LINK_SENSED:
		hostEvent = WLAN_HOST_EVENT_LINK_SENSED;
		break;
	case CBPROC_EVENT_HS_ACT_REQ:
		hostEvent = WLAN_HOST_EVENT_HS_ACT_REQ;
		break;
	case CBPROC_EVENT_WMM_STATUS:
		hostEvent = WLAN_HOST_EVENT_WMM_STATUS;
		break;
	case CBPROC_EVENT_IBSS_COALESCED:
		hostEvent = WLAN_HOST_EVENT_IBSS_COALESCED;
		break;
	case CBPROC_EVENT_STATUS_41:
		hostEvent = WLAN_HOST_EVENT_EVENT_41;
		break;
	case CBPROC_EVENT_PORT_RELEASE:
		hostEvent = WLAN_HOST_EVENT_PORT_RELEASE;
		break;
	default:
		return 0;
	}
	wlanSessionOps.hostEventSend(connection, hostEvent);
	return 1;
}

/* The task of the host's commands and the events of power save and host sleep. */
void cbProcTaskMain(ULONG input)
{
	OsaBuffer *buffer;
	uint8_t *message;
	struct WlanConnection *connection;
	unsigned int event;
	unsigned int events;

	sleepCalibrate();
	sleepMacHalt();
	hostCmdMacAwake = 0;
	SOC_PENDING_EVENT_CONTROL &= ~4u;
	for (;;) {
		while (osaMsgQueueReceive(cbProcQueue, (uintptr_t *)&buffer, 4, 0xffffffff) == 15)
			;
		message = (uint8_t *)buffer + buffer->dataOffset;
		connection = (struct WlanConnection *)__aeabi_uread4(message);
		event = __aeabi_uread4(message + 4);
		if (event == 0) {
			osa_BufferFree(buffer);
			continue;
		}

		switch (event) {
		case CBPROC_EVENT_SLEEP_TIMEOUT:
			if (wlanPsSleepTimeout != 0 || (wlanSessionId != 0 && wlanPsSleepSequence != 0)) {
				wlanPsSleepTimeout = 0;
				wlanPsSleepSequence = 0;
				if (wlanPsIdleSleep() == 0) {
					wlanPsSetPowerState(1);
					wlanPsArmIdleWakeTimer();
					wlanRaiseEvent(WLAN_EVENT_PS_AWAKE);
				}
			}
			break;
		case CBPROC_EVENT_HOST_COMMAND:
			if (hostCmdProcess() == 0) {
				cbProcCommandRetries++;
				osaTaskSleep(1);
				wlanPostConnectionEvent(connection, CBPROC_EVENT_HOST_COMMAND);
			}
			break;
		case CBPROC_EVENT_PS_HEADER:
			wlanPsApplyHeader();
			break;
		case CBPROC_EVENT_SME_RESPONSE:
			cbProcRespondSme();
			if (wlanConnectionsIdle())
				wlanPsPhaseResume(connection);
			break;
		case CBPROC_EVENT_MGMT_STATUS:
			cbProcReportMgmt(connection);
			if (wlanConnectionsIdle())
				wlanPsPhaseResume(connection);
			break;
		case CBPROC_EVENT_HOST_AWAKE:
			cbProcHostAwake(connection);
			break;
		case CBPROC_EVENT_HOST_SLEEP:
			cbProcHostSleep(connection);
			break;
		case CBPROC_EVENT_PS_BUFFER:
			wlanPsApplyBuffer();
			wlanPsClearTable(connection, 2);
			break;
		case CBPROC_EVENT_PS_TABLE_3:
			wlanPsClearTable(connection, 3);
			if (macMgmtClearHeader() != 0)
				wlanPsArmWakeTimer();
			else
				wlanPsPostEvent(18);
			break;
		case CBPROC_EVENT_PS_TABLE_5:
			wlanPsClearTable(connection, 5);
			break;
		case WLAN_EVENT_WEP_ICV_ERR_PENDING:
			wlanWepIcvErrReport(connection, &wlanIbssIcvErrList);
			break;
		case CBPROC_EVENT_PS_VERIFY:
			wlanPsVerifyTable();
			break;
		case WLAN_EVENT_HOST_ASLEEP:
			cbProcHostSleepEnter(connection);
			break;
		case CBPROC_EVENT_SLEEP_REFUSED:
			wlanHostEventSendPlain(connection, WLAN_EVENT_SLEEP_REFUSED);
			break;
		case CBPROC_EVENT_PENDING_EVENTS:
			cbProcPostPendingEvents();
			break;
		case WLAN_EVENT_SUBSCRIBED:
			if (wlanSubscribeEventsPending(connection) && wlanPsAsleep() != 0) {
				events = wlanSubscribeEventsTake(connection);
				wlanSubscribeEventsSend(connection, events);
				if (events & 0x800)
					wlanSubscribePreBeaconLostDone();
				if ((int)events < 0)
					wlanHostEventSendData(connection, 66, &wlanRateSelection.amsduBufferSize, 2);
			}
			break;
		default:
			cbProcEventToHost(connection, event);
			break;
		}
		osa_BufferFree(buffer);
	}
}
