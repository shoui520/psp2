/*
 * Host commands (0x18a0e..0x1ae88): the "CB Proc" task takes the host's
 * commands (Marvell's HostCmd_DS_COMMAND: command, size, sequence number
 * with the BSS, result, then the body) from wlanDriverQueue1, runs each and
 * returns the response in a buffer of its own. The names are inferred from
 * use; the command codes are Marvell's (mwifiex fw.h).
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/hostps.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/pssm.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/stapssm.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/threadx.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

#define HOSTCMD_U16(p)		((uint16_t)((p)[0] | (p)[1] << 8))

/* Response codes of the commands whose response tells the AP power save of traffic. */
#define HOSTCMD_RESP_11N_ADDBA_REQ	0x80ce
#define HOSTCMD_RESP_PS_ACTIVITY	27	/* 0x80e9, from HOSTCMD_RESP_11N_ADDBA_REQ */

#define HOSTCMD_RATE_STEPS		16

/* Not converted yet; provisional names. */
void cbProcFilterContext(void);						/* 0x1b6d0 */
unsigned int rfCalibDispatchTable(void);				/* 0x2761a */
void uapSqramFilterEntry(int enable);					/* SQRAM 0xc0009ba0 */
void wlanPsHandleBuffer(unsigned int count);				/* 0x3c124 */
struct WlanConnection *p2pDirectProcessEntry(unsigned int interfaceType);	/* 0x41250 */
uint16_t macMgmtDispatchDescriptor(void);				/* 0x3bc6e */
extern uint32_t wlanPsFilterState;					/* 0x54c58 */
extern uint16_t hostCmdRateDefault;					/* SQRAM 0xc0011ba8 */
extern uint16_t hostCmdRateSteps[HOSTCMD_RATE_STEPS];			/* SQRAM 0xc0011b8a */
extern uint32_t hostCmdRateLimits[4];					/* 0x040089b0 */

/* The command being run: set while its response is outstanding. */
uint8_t hostCmdBusy __attribute__((section(".itcm.rw")));			/* 0x54b8c */
/* The response buffer of the command being run, and its HostCmd_DS_COMMAND. */
OsaBuffer *hostCmdResponse __attribute__((section(".itcm.rw")));		/* 0x54b9c */
/* HostCmd_CMD 0x0101: the value the host set. */
uint16_t hostCmdNodeValue __attribute__((section(".itcm.rw")));		/* 0x54b98 */

/* The default release of a response buffer: the connection's power save phase restarts. */
static int hostCmdResponseRelease(OsaBuffer *buffer)
{
	wlanPsPhaseArm(buffer->context);
	return 0;
}

/* Gives the response of the command being run to the host. */
int hostCmdRespond(struct WlanConnection *connection, int (*release)(OsaBuffer *buffer))
{
	uint8_t *response;
	unsigned int code;

	if (release == NULL)
		release = hostCmdResponseRelease;
	if (!hostCmdBusy)
		return hostCmdBusy;
	cbProcFilterContext();
	if (wlanApPsEnabled()) {
		hostCmdResponse->dataOffset = 32;
		response = (uint8_t *)hostCmdResponse + 32;
		code = HOSTCMD_U16(response) - HOSTCMD_RESP_11N_ADDBA_REQ;
		if (code == 0 || code == 1 || code == 2 || code == HOSTCMD_RESP_PS_ACTIVITY)
			wlanPsActivity(wlanConnectionByRole(2));
	}
	hostCmdResponse->flags |= 1;
	hostCmdResponse->release = release;
	wlanHostCallback(connection->device, hostCmdResponse, 0, 3);
	hostCmdBusy = 0;
	return 0;
}

/* HostCmd_CMD 0x0101: action 1 sets the value; the value is returned. */
uint16_t hostCmdNode(const uint8_t *body)
{
	if (HOSTCMD_U16(body) == 1)
		hostCmdNodeValue = HOSTCMD_U16(body + 2);
	return hostCmdNodeValue;
}

void hostCmdSuspendMacTasks(void)
{
	osaTaskSuspend(macTxTask);
	osaTaskSuspend(macMgmtTask);
	osaTaskSuspend(macTxNotifyTask);
}

/* The first rate step at or above value / 1000, if there is one. */
#define HOSTCMD_RATE_STEP(field, value)						\
	do {									\
		unsigned int limit = (value) / 1000;				\
		unsigned int i;							\
										\
		for (i = 0; i < HOSTCMD_RATE_STEPS; i++) {			\
			if (hostCmdRateSteps[i] >= limit) {			\
				(field)[0] = hostCmdRateSteps[i];		\
				(field)[1] = hostCmdRateSteps[i] >> 8;		\
				break;						\
			}							\
		}								\
	} while (0)

/*
 * HostCmd_CMD 0x0094, get: the rate step of the current rate, then by
 * type 0 the steps of the four limits (size 24), by type 1 the step table
 * (size 48).
 */
void hostCmdRateStepsGet(uint8_t *body, uint16_t *size)
{
	unsigned int current = rfCalibDispatchTable() / 1000;
	unsigned int i;

	body[2] = hostCmdRateDefault;
	body[3] = hostCmdRateDefault >> 8;
	for (i = 0; i < HOSTCMD_RATE_STEPS; i++) {
		if (hostCmdRateSteps[i] >= current) {
			body[2] = hostCmdRateSteps[i];
			body[3] = hostCmdRateSteps[i] >> 8;
			break;
		}
	}
	if (HOSTCMD_U16(body + 4) == 0) {
		body[6] = 8;
		body[7] = 0;
		body[8] = hostCmdRateDefault;
		body[9] = hostCmdRateDefault >> 8;
		body[10] = hostCmdRateDefault;
		body[11] = hostCmdRateDefault >> 8;
		body[12] = hostCmdRateDefault;
		body[13] = hostCmdRateDefault >> 8;
		body[14] = hostCmdRateDefault;
		body[15] = hostCmdRateDefault >> 8;
		HOSTCMD_RATE_STEP(body + 8, hostCmdRateLimits[0]);
		HOSTCMD_RATE_STEP(body + 10, hostCmdRateLimits[1]);
		HOSTCMD_RATE_STEP(body + 12, hostCmdRateLimits[2]);
		HOSTCMD_RATE_STEP(body + 14, hostCmdRateLimits[3]);
		*size = 24;
	}
	if (HOSTCMD_U16(body + 4) == 1) {
		body[6] = 32;
		body[7] = 0;
		__aeabi_memcpy(body + 8, hostCmdRateSteps, sizeof(hostCmdRateSteps));
		*size = 48;
	}
}

/* The host changed the keys or rates of a connection. */
int hostCmdConnectionChanged(void)
{
	wlanAnyConnectionHostFlag8();
	return romLibScheduleState();
}

/* Leaves host power save mode; 0 when it was on. */
int wlanPsHostModeExit(void)
{
	int result = 1;

	wlanPsCancelHostModeTimer();
	if (wlanPsHostMode == 1) {
		wlanPsHostMode = 0;
		wlanPsPostEvent(2);
		result = 0;
		if (wlanConnectionExists(1))
			uapSqramFilterEntry(wlanPsFilterState != 0 ? 2 : 5);
	}
	return result;
}

/*
 * Enters host power save mode with the parameters of HostCmd_CMD_802_11_PS_MODE_ENH
 * (hold seconds, DTIM policy, null window, buffered frames); 0 when it was entered.
 */
int wlanPsHostModeEnter(const uint8_t *params)
{
	int result = 1;
	struct WlanConnection *connection;
	struct WlanConnection *client;
	unsigned int policy;
	UINT posture;

	connection = wlanConnectionByRole(wlanHasApRole() ? 2 : 0);
	client = p2pDirectProcessEntry(2);
	if (client == NULL)
		client = wlanConnectionFirst(2);
	if (!((wlanRoleOtherOnly() && ((wlanSessionId & 0x10) || (connection->flags2 & 0x10)) &&
	       ((policy = HOSTCMD_U16(params + 4)) <= 5 || policy >= 0xfffd)) ||
	      (wlanRoleIbssOnly() && (macMgmtDispatchDescriptor() != 0 || !(connection->flags2 & 0x10))) ||
	      (wlanHasApRole() && ((wlanSessionId >> 8) & 3) != 0)))
		return result;

	posture = sysIrqDisable();
	if (HOSTCMD_U16(params + 4) != 0)
		wlanPsDtimPolicy = HOSTCMD_U16(params + 4);
	if (HOSTCMD_U16(params + 6) != 0)
		wlanPsNullWindow = HOSTCMD_U16(params + 6);
	if (HOSTCMD_U16(params + 2) != 0)
		wlanPsHoldSeconds = HOSTCMD_U16(params + 2);
	if (wlanPsHostMode == 0 &&
	    ((wlanRoleOtherOnly() && (connection->flags2 & 0x10) && (wlanSessionId & 0x10) &&
	      (client == NULL || client->state != 7)) ||
	     (wlanHasApRole() && ((wlanSessionId >> 8) & 3) != 0 && connection->state == 7) ||
	     (wlanRoleIbssOnly() && (connection->flags2 & 0x10)))) {
		wlanPsHostMode = 1;
		wlanPsPostEvent(1);
		result = 0;
	}
	sysIrqRestore(posture);
	if (HOSTCMD_U16(params + 10) != 0)
		wlanPsHandleBuffer((uint8_t)HOSTCMD_U16(params + 10));
	return result;
}

/* Commands that need the radio: refused while it is off. */
int hostCmdNeedsRadio(unsigned int command)
{
	return command == 0x0006 || command == 0x00e6 || command == 0x006b || command == 0x002b ||
	       command == 0x002c || command == 0x0050 || command == 0x00e7 || command == 0x0024 ||
	       command == 0x00b1;
}
