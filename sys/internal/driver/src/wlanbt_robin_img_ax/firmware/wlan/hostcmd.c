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
#include "robin/btrom.h"
#include "robin/calibration.h"
#include "robin/coex.h"
#include "robin/ibss.h"
#include "robin/join.h"
#include "robin/apps.h"
#include "robin/mac.h"
#include "robin/mdev.h"
#include "robin/mef.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/rfbus.h"
#include "robin/signal.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/stream.h"
#include "robin/subscribe.h"
#include "robin/txstats.h"
#include "robin/mactx.h"

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

/*
 * The host command loop. The handlers that are not converted yet keep
 * provisional names, hostCmd<code> (Marvell's command code in hex), with
 * their address.
 */
struct WlanTxRateStats;
struct WlanDriverQueue;

int hostCmdUsesRole(const uint8_t *command);					/* 0x2c940 */
void hostCmd0003Fill(uint8_t *body, int caps, unsigned int primary, unsigned int secondary,
		     int extra);							/* 0x2cf42 */
int hostCmd0003Caps(void);							/* 0x1b820 */
unsigned int hostCmd0003Version(void);						/* 0x27de0 */
int hostCmd0006(struct WlanConnection *connection, int bgScan, uint8_t *body, int length); /* 0x370f2 */
void hostCmd000a(uint8_t *body, unsigned int channel, int psState, unsigned int radioOn); /* 0x2cba4 */
void hostCmd0010(uint8_t *body, uint16_t *count);				/* 0x2cad0 */
void hostCmd0016Oid17(void);							/* 0x3b55c */
void hostCmd0019Build(void);							/* 0x23e90 */
unsigned int hostCmd0019Parse(void);						/* 0x23e3e */
int hostCmd001cResolve(void);							/* 0x37966 */
void hostCmd001eChannel(uint8_t *channel);					/* 0x356c6 */
int hostCmd0020(uint8_t *body, unsigned int action, uint16_t *result, uint8_t *antenna); /* 0x2ce5e */
unsigned int hostCmd0020Set(unsigned int secondary, unsigned int antenna);	/* 0x262c0 */
void hostCmd0020Clear(unsigned int value);					/* SQRAM 0xc000a3f6 */
void hostCmd0020Setup(unsigned int value);					/* SQRAM 0xc000a3d4 */
void hostCmd0024(struct WlanConnection *connection, uint8_t *body);		/* 0x341aa */
void hostCmd002bStart(struct WlanConnection *connection, uint8_t *command);	/* 0x365aa */
void hostCmd002cJoin(struct WlanConnection *connection, uint8_t *command);	/* 0x3675e */
void hostCmd0036(uint8_t *body, int authenticator, uint8_t *state);		/* 0x2cc7e */
void hostCmd003c(int offset, int frequency);					/* 0x2e9cc */
int hostCmd004d(struct WlanConnection *connection, uint8_t *body);		/* 0x1b74e */
void hostCmd004eSet(uint8_t *body, unsigned int length);			/* SQRAM 0xc0009bea */
unsigned int hostCmd004eGet(uint8_t *body);					/* SQRAM 0xc0009c50 */
unsigned int hostCmd005bGet(struct WlanConnection *connection, uint8_t *data);	/* 0x3009a */
int hostCmd005bSet(struct WlanConnection *connection, uint8_t *data, unsigned int length); /* 0x30144 */
unsigned int hostCmd005d(uint8_t *body);					/* 0x418d4 */
void hostCmd005e(struct WlanConnection *connection, uint8_t *data, uint8_t *end); /* 0x3eb24 */
unsigned int hostCmd005eGet(struct WlanConnection *connection, uint8_t *data);	/* 0x3ebe2 */
int hostCmd006bSet(struct WlanConnection *connection, unsigned int action, uint8_t *data,
		   unsigned int length);					/* 0x378b0 */
unsigned int hostCmd006bGet(uint8_t *body, unsigned int flush);			/* 0x378f0 */
int hostCmd006e(struct WlanConnection *connection, char *body, int length);	/* 0x41af8 */
void hostCmd006f(uint8_t *body, int length);					/* 0x41838 */
unsigned int hostCmd0070(uint8_t *body);					/* 0x1e826 */
unsigned int hostCmd0071(struct WlanConnection *connection, char *body);	/* 0x1e782 */
void hostCmd0074Reset(void);							/* 0x311d0 */
void hostCmd0074Arm(void);							/* 0x311ee */
void hostCmd0078Parse(uint32_t *status, uint8_t *body, uint32_t *type, uint32_t *extra); /* SQRAM 0xc00078d6 */
void hostCmd0078Lock(void);							/* 0x26c40 */
void hostCmd0078Unlock(void);							/* 0x26c4c */
extern void (*hostCmd0078Handlers[])(uint32_t *type, uint32_t *failed);		/* SQRAM 0xc0011f34 */
void hostCmd007f(struct WlanConnection *connection, uint32_t *rate, uint32_t *flags); /* 0x335b0 */
unsigned int hostCmd0081(uint8_t *body);					/* 0x1e83c */
unsigned int hostCmd0082Set(uint8_t *body);					/* 0x2fcc2 */
unsigned int hostCmd0082Get(uint8_t *body);					/* 0x2fd28 */
void hostCmd0083(unsigned int bssType, unsigned int bssNumber, unsigned int value); /* SQRAM 0xc0007406 */
void hostCmd0084(unsigned int action, uint8_t *data, unsigned int size);	/* 0x4237e */
void hostCmd008a(uint8_t *body, uint16_t *beaconInterval, uint16_t *wakeBeacons,
		 uint16_t *wakeDtims, uint16_t *listenBeacons);				/* 0x2cdfc */
int hostCmd008fWrite(unsigned int length, uint8_t *data);			/* 0x33084 */
void hostCmd008fConfigure(uint8_t *data, unsigned int length);			/* 0x283cc */
int hostCmd008fApply(void);							/* 0x31224 */
unsigned int hostCmd008fResolve(uint8_t *body);					/* 0x26fdc */
void hostCmd0096Enable(void);							/* 0x26c58 */
void hostCmd0096Disable(void);							/* 0x26c5a */
unsigned int hostCmd0096Get(void);						/* 0x26c5c */
void hostCmd0096Track(void);							/* 0x31222 */
int hostCmd00a9Shutdown(void);							/* 0x1be44 */
void hostCmd00a9Reset(void);							/* 0x24590 */
void pmicRegWrite(uint8_t reg, uint8_t value);					/* 0x3121c */
uint8_t pmicRegRead(uint8_t reg);						/* 0x3121e */
int hostCmd00af(struct WlanConnection *connection);				/* 0x38608 */
int hostCmd00b0Set(struct WlanConnection *connection, uint8_t *command);	/* 0x37eb8 */
int hostCmd00b0Get(struct WlanConnection *connection, uint8_t *command);	/* 0x382b8 */
int hostCmd00b1(struct WlanConnection *connection);				/* 0x3876e */
int hostCmd00b2(struct WlanConnection *connection);				/* 0x3861c */
int hostCmd00b3(struct WlanConnection *connection, uint8_t *command);		/* 0x38808 */
int hostCmd00b5(struct WlanConnection *connection, uint8_t *command);		/* 0x38564 */
void hostCmd00c5(unsigned int action, uint8_t *data, unsigned int *length);	/* 0x3ed9e */
unsigned int hostCmd00c7(struct WlanConnection *connection, uint8_t *body);	/* 0x34e58 */
void hostCmd00cdSet(struct WlanConnection *connection, uint8_t *body);		/* 0x3089c */
void hostCmd00cdGet(uint8_t *body);						/* 0x308d4 */
int hostCmd00ce(uint8_t *body, unsigned int length);				/* 0x3058e */
void hostCmd00cf(uint8_t *body, unsigned int length);				/* 0x306ca */
int hostCmd00d0(uint8_t *body, unsigned int length);				/* 0x30728 */
int hostCmd00d1Set(struct WlanConnection *connection, uint8_t *body);		/* 0x3b5d8 */
unsigned int hostCmd00d1Get(uint8_t *body);					/* 0x3b6a2 */
int hostCmd00d6Parse(uint8_t *data, unsigned int length, uint8_t **rates0, uint8_t **rates1,
		     uint8_t **rates2);						/* 0x3f35a */
int hostCmd00d6Apply(struct WlanConnection *connection, uint8_t *rates0, uint8_t *rates1,
		     uint8_t *rates2);						/* 0x3ce42 */
unsigned int hostCmd00d6Get(struct WlanConnection *connection, uint8_t *data, uint32_t rate); /* 0x3cea6 */
int hostCmd00dd(struct WlanConnection *connection, uint8_t *body, unsigned int length); /* SQRAM 0xc0009a76 */
int hostCmd00e0(unsigned int action, uint8_t *data, unsigned int length);	/* 0x3f06e */
unsigned int hostCmd00e4(struct WlanConnection *connection, uint8_t *data);	/* 0x3f2c0 */
int hostCmd00e4Check(struct WlanConnection *connection, uint8_t *data, unsigned int length); /* 0x3f1f6 */
int hostCmd00e9(struct WlanConnection *connection, uint8_t *body, unsigned int length); /* 0x307d2 */
int hostCmd00ea(struct WlanConnection *connection, uint8_t *command);		/* SQRAM 0xc000d086 */
void hostCmd00ed(void);								/* 0x2691a */
void hostCmd00edEnd(void);							/* 0x26906 */
void hostCmd00edSet(unsigned int reg, unsigned int value);			/* 0x267d2 */
unsigned int hostCmd00edGet(unsigned int reg);					/* 0x267ee */
void hostCmd00f4(struct WlanConnection *connection, uint8_t *command);		/* SQRAM 0xc000d06c */
int hostCmd00f5(struct WlanConnection *connection, unsigned int action, uint8_t *data,
		unsigned int *length);						/* 0x3f002 */
int hostCmd00fb(struct WlanConnection *connection, unsigned int action, uint8_t *data,
		unsigned int *length);						/* 0x3b338 */
void hostCmd0102(unsigned int enable);						/* 0x32538 */
unsigned int hostCmd01ff(void);							/* 0x3e88e */
void wlanConnectionSetBssMode(struct WlanConnection *connection, unsigned int mode,
			      uint8_t *body0, uint8_t *body1);				/* 0x1d934 */
struct WlanConnection *wlanConnectionConfigure(int interfaceType, int bssType, int bssNumber,
					       const uint8_t *local, const uint8_t *peer,
					       int policy, void *driver);		/* 0x1d604 */
int hostCmdMisc(uint16_t *result, uint8_t *body, struct WlanConnection *connection); /* 0x2ccb6 */
int hostCmdRateStepsSet(uint8_t *body);						/* 0x1b6e4 */
void wlanTxBufSizeSet(uint16_t *size);						/* 0x1bb40 */
void wlanPsFormatBuffer(struct WlanConnection *connection, unsigned int rate);	/* 0x3c7b8 */
void wlanPsReleaseBuffer(void);							/* 0x3c9ba */
void wlanPsQueryPacket(void);							/* 0x3d006 */
unsigned int wlanRateSelect(uint8_t *rateControl);				/* 0x2be18 */
int wlanPsFormatPacket(struct WlanConnection *connection);			/* 0x3d0e4 */
void wlanHostWakeGpioSet(unsigned int gpio, int enable);			/* 0x28142 */
void osaFormatVersionBanner(unsigned int selector, char *text);			/* 0x27c36 */
void macMgmtValidateTable(int reason);						/* 0x3726c */
void macMgmtScheduleTable(int value);						/* 0x3796e */
void macMgmtNotifyTable(void);							/* 0x3797a */
void macMgmtApplyTable(void);							/* 0x3795a */
void macMgmtPrepareDescriptor(uint16_t atimWindow);				/* 0x3bc5a */
void wlanPsAdjustTable(struct WlanConnection *connection);			/* 0x3c0aa */
void macMgmtNotifyEntry(struct WlanConnection *connection);			/* 0x36a78 */
struct WlanPsPeerState *wlanConnectionPsState(struct WlanConnection *connection); /* 0x1d1f6 */
int wlanP2pConnection(struct WlanConnection *connection);			/* 0x41228 */
struct WlanConnection *p2pDirectProcessEntry(unsigned int interfaceType);	/* 0x41250 */
extern uint8_t hostCmd0096Tracking;						/* 0x54f9c */

extern struct WlanDriverQueue wlanDriverQueue1;
extern struct WlanPsHostConfig wlanPsHostConfig;				/* 0x568be */
extern uint16_t wlanRxFrameTypeMask;
unsigned int bbpReadVersionRegister(unsigned int reg);				/* 0xc308 */
extern struct OsaBufferPool hostCmdResponsePool;				/* 0x04000e24 */
extern uint8_t wlanPrimaryEvents;						/* 0x04008828, bit 0: radio on */
extern uint8_t *wlanAntennaProfile;						/* 0x040088ca, unaligned */
extern uint8_t wlanIbssJoinState[4];						/* 0x040088ce, unaligned */
extern uint8_t wlanRssiInfoMarks[8];						/* 0x040088d2 */
extern struct WlanConnection *wlanIbssConnection;				/* 0x000596d0 */
extern uint8_t wlanTxBufferCount;						/* 0x04000c9c */
extern uint16_t wlanAmsduSize;							/* 0x04001004 */
extern uint16_t wlanAmsduEnabled;						/* 0x04000ff8 */
extern uint32_t wlanTxRateDefault;						/* 0x0400100c */
extern struct WlanConnection *wlanAssociateConnection;				/* 0x04004e9c */
extern uint8_t wlanAfcEnabled;							/* 0x00054f18 */
extern uint8_t wlanAfcAuto;							/* 0x00054f1e */
extern uint8_t wlanAfcThreshold;						/* 0x00054f1f */
extern int16_t wlanAfcTimingOffset;						/* 0x00054f1a */
extern int16_t wlanAfcCarrierOffset;						/* 0x00054f1c */
extern uint8_t wlanAfcSamples;							/* 0x00054f20 */
extern uint8_t wlanScanRequested;						/* 0x00054c80 */
extern uint32_t wlanPsWakeAdvance;						/* 0x56260 */
extern uint16_t sleepHostWakeCount;						/* 0x5622e */
extern uint32_t sleepHostWakeMargin;						/* 0x56234 */
extern uint32_t sleepHostClockParameter;					/* 0x5623c */
extern uint16_t wlanPsAltModeActive;						/* 0x562a0 */
extern uint16_t wlanPsAltModeRequest;						/* 0x562a2 */

struct WlanHwInfo {
	uint8_t reserved0[5];
	uint8_t version[2];		/* HostCmd_DS_GET_HW_SPEC's hardware version */
	uint8_t reserved7[4];
	uint8_t txPowerLevel[2];	/* HostCmd_CMD_802_11_RF_TX_POWER's current level */
};
extern struct WlanHwInfo wlanHwInfo;						/* 0x040088ac */

uint8_t hostCmdLocked __attribute__((section(".itcm.rw")));			/* 0x54b8d */
uint16_t wlanMgmtFrameSize __attribute__((section(".itcm.rw"))) = 1860;	/* 0x54b8e */
uint16_t wlanMulticastCount __attribute__((section(".itcm.rw")));		/* 0x54b90 */
static uint16_t hostCmdFragLimited __attribute__((section(".itcm.rw")));	/* 0x54b92 */
uint16_t wlanIbssAtimWindow __attribute__((section(".itcm.rw")));		/* 0x54b94 */
uint16_t hostCmdResponseLimit __attribute__((section(".itcm.rw")));		/* 0x54b96 */
uint32_t hostCmdMacAwake __attribute__((section(".itcm.rw")));			/* 0x54ba4 */
uint8_t *hostCmdResponseData __attribute__((section(".itcm.rw")));		/* 0x54bbc */

#define PUT16(p, v)	do { (p)[0] = (uint8_t)(v); (p)[1] = (uint8_t)((v) >> 8); } while (0)
#define MAC_644_STATE	MAC_REG(0x644)

#define SDIO_IOCTL_SLEEP_STATS		9
#define SDIO_IOCTL_HOST_EVENT_FILTER	23
#define SDIO_IOCTL_HOST_EVENT_QUERY	24
#define SDIO_IOCTL_BLOCK_SIZE_QUERY	0x4c
#define SDIO_IOCTL_HOST_SLEEP_CONFIG	30
#define SDIO_IOCTL_WAKE_GPIO_SET	0x60
#define SDIO_IOCTL_WAKE_GPIO_ARM	0x61
#define SDIO_IOCTL_HOST_PARAM_SET	28
#define SDIO_IOCTL_HOST_PARAM_GET	29
#define SDIO_IOCTL_HOST_EVENT_DONE	0x0b

/* The commands that must not run while a connection event is outstanding. */
static int hostCmdNeedsLock(unsigned int command)
{
	return command == 43 || command == 44 || command == 62 || command == 64 || command == 111 ||
	       command == 169 || command == 170 || command == 175 || command == 177 ||
	       command == 178 || command == 181 || command == 206 || command == 207 ||
	       command == 208 || command == 217;
}

/* The byte at antenna profile + 3: bits 0-1 the primary, bits 2-3 the secondary antenna setup. */
static inline unsigned int hostCmdAntennaProfile(void)
{
	return ((const uint8_t *)__aeabi_uread4(&wlanAntennaProfile))[3];
}

/*
 * The CB Proc task's loop over the host's commands (wlanDriverQueue1): runs
 * each and answers it in a buffer of its own. A response's result is 0
 * (success), 1 (the command failed) or 2 (not supported).
 */
int hostCmdProcess(void)
{
	hostCmdResponseLimit = 944;
	for (;;) {
		OsaBuffer *peek;
		OsaBuffer *buffer;
		uint8_t *hdr;
		uint8_t *body;
		struct WlanConnection *connection;
		int (*release)(OsaBuffer *buffer);
		unsigned int rawCommand;
		unsigned int command;
		unsigned int bodyLength;
		unsigned int lock;
		unsigned int action;
		unsigned int a;
		unsigned int b;
		unsigned int c;
		uint16_t result;
		int respond;
		int freeCommand;

		if (hostCmdBusy || osa_MsgQueuePeek(&wlanDriverQueue1) == NULL)
			return 1;
		peek = osa_MsgQueuePeek(&wlanDriverQueue1);
		hdr = (uint8_t *)peek + peek->dataOffset;
		rawCommand = HOSTCMD_U16(hdr);
		command = rawCommand & 0x7fff;
		body = hdr + 8;
		bodyLength = HOSTCMD_U16(hdr + 2) - 8;
		lock = hostCmdNeedsLock(rawCommand);
		if (lock && !wlanConnectEventTryWait())
			return 0;
		hostCmdLocked = lock;
		respond = 1;
		freeCommand = 1;
		release = NULL;
		buffer = osa_MsgQueueRecv(&wlanDriverQueue1);
		hdr = (uint8_t *)buffer + buffer->dataOffset;
		if (!hostCmdUsesRole(hdr) || wlanRoleMaskIs(0)) {
			connection = wlanConnectionFind(hdr[5] >> 4, hdr[5] & 15);
		} else {
			connection = wlanConnectionByRole(wlanHasApRole() ? 2 : 0);
		}
		if (connection == NULL) {
			unsigned int type;

			if (command == 247)
				type = body[0];
			else if ((hdr[5] >> 4) == 1)
				type = 2;
			else if ((hdr[5] >> 4) == 2)
				type = 3;
			else
				type = 0;
			connection = wlanConnectionConfigure(type, hdr[5] >> 4, hdr[5] & 15, NULL, NULL, 0,
							     buffer->context);
		}
		buffer->context = connection;
		while ((hostCmdResponse = osa_BufferAlloc(&hostCmdResponsePool, 0x76c, 0)) == NULL)
			osaTaskSleep(1);
		hostCmdResponse->dataOffset = 32;
		hostCmdResponseData = (uint8_t *)hostCmdResponse + 32;
		hostCmdResponse->context = connection;
		sysIssuePrimaryBbpCommand((uint32_t)hostCmdResponseData, (uint32_t)hdr, HOSTCMD_U16(hdr + 2));
		hostCmdBusy = 1;
		if (!hostCmdMacAwake && command != 170) {
			sleepMacResume();
			hostCmdMacAwake = 1;
			SOC_PENDING_EVENT_CONTROL |= 4;
		}
		wlanHostSleepCancel();
		result = 0;

		/* The radio is off: the commands that need it are refused. */
		if (!(wlanPrimaryEvents & 1) && hostCmdNeedsRadio(command)) {
			hdr[0] = command;
			hdr[1] = (command >> 8) | 0x80;
			PUT16(hdr + 6, 2);
			sysIssuePrimaryBbpCommand((uint32_t)hostCmdResponseData, (uint32_t)hdr,
						  HOSTCMD_U16(hdr + 2));
			osa_BufferFree(buffer);
			hostCmdRespond(connection, NULL);
			return 1;
		}
		wlanPsCancelPendingTimer();
		action = HOSTCMD_U16(body);

		switch (command) {
		case 3:		/* HostCmd_CMD_GET_HW_SPEC */
		{
			unsigned int profile = hostCmdAntennaProfile();

			__aeabi_memcpy(body + 8, connection->interfaceAddress, 6);
			if ((profile & 3) != ((profile >> 2) & 3) || (profile & 2)) {
				PUT16(body + 16, macGetEventPrimaryPathState() == 7 ? 3 : 2);
			} else {
				PUT16(body + 16, 1);
			}
			body[6] = 64;
			body[7] = 0;
			body[14] = ((const uint8_t *)__aeabi_uread4(&wlanAntennaProfile))[32];
			body[15] = ((const uint8_t *)__aeabi_uread4(&wlanAntennaProfile))[33];
			__aeabi_uwrite4(hostCmd0003Version(), body + 18);
			hostCmd0003Fill(body, hostCmd0003Caps(), hostCmdAntennaProfile() & 3,
					(hostCmdAntennaProfile() >> 2) & 3, 0);
			body[2] = wlanHwInfo.version[0];
			body[3] = wlanHwInfo.version[1];
			body[0] = 2;
			body[1] = 0;
			body[43] = wlanTxBufferCount;
			body[44] = 0;
			goto done;
		}

		case 6:		/* HostCmd_CMD_802_11_SCAN */
			wlanScanRequested = 1;
			if (connection->interfaceType == 3 && connection->mode != 0 &&
			    connection->localName.length != 4)
				connection->localName.length = 1;
			if (!hostCmd0006(connection, 0, body, (int16_t)bodyLength)) {
				wlanScanRequested = 0;
				goto failed;
			}
			goto filtered;

		case 10:	/* HostCmd_CMD_802_11_GET_STAT */
			hostCmd000a(body, rfGetChannel(), wlanPsFormatPacket(connection), wlanPrimaryEvents & 1);
			goto done;

		case 11:	/* HostCmd_CMD_802_11_GET_LOG */
			hdr[2] = 76;
			hdr[3] = 0;
			wlanTxStatsGetLog((HostCmd_DS_802_11_GET_LOG *)body);
			goto done;

		case 16:	/* HostCmd_CMD_MAC_MULTICAST_ADR */
			hostCmd0010(body, &wlanMulticastCount);
			goto done;

		case 18:	/* HostCmd_CMD_802_11_AUTHENTICATE */
		case 80:	/* HostCmd_CMD_802_11_ASSOCIATE */
			wlanConnectionSetBssMode(connection, 0, body, body);
			wlanAssociateConnection = connection;
			if (!wlanJoinAssociate(connection, (HostCmd_DS_802_11_ASSOCIATE *)body, (uint16_t)bodyLength))
				goto failed;
			goto filtered;

		case 22:	/* HostCmd_CMD_802_11_SNMP_MIB */
		{
			struct WlanConnection *other;
			unsigned int oid = HOSTCMD_U16(body + 2);

			if (action == 1) {
				if (oid == 9) {
					connection->flags2 = (connection->flags2 & ~1) | (body[6] & 1);
					wlanTxPowerUpdateConnection(connection, wlanOperatingChannel(), 1);
				} else if (oid == 17) {
					wlanLocalState->mibOid17 = body[6];
					if (body[6] != 0)
						hostCmd0016Oid17();
				}
				other = connection;
				do {
					oid = HOSTCMD_U16(body + 2);
					if (oid == 5) {
						other->rtsThreshold[0] = body[6];
						other->rtsThreshold[1] = body[7];
					} else if (oid == 6) {
						other->shortRetryLimit = body[6];
					} else if (oid == 7) {
						other->longRetryLimit = body[6];
					} else if (oid == 8 && (hostCmdFragLimited == 0 || HOSTCMD_U16(body + 6) < 0x2ee)) {
						other->fragThreshold[0] = body[6];
						other->fragThreshold[1] = body[7];
					}
					other = wlanConnectionNextByBss(other, other->bssType, other->bssNumber);
				} while (other != NULL);
				goto done;
			}
			if (action != 0)
				goto done;
			oid = HOSTCMD_U16(body + 2);
			if (oid == 8) {
				body[6] = connection->fragThreshold[0];
				body[7] = connection->fragThreshold[1];
			} else if (oid > 8) {
				if (oid == 9)
					body[6] = connection->flags2 & 1;
				else if (oid == 17)
					body[6] = wlanLocalState->mibOid17;
				else
					goto done;
			} else if (oid == 5) {
				body[6] = connection->rtsThreshold[0];
				body[7] = connection->rtsThreshold[1];
			} else if (oid == 6) {
				body[6] = connection->shortRetryLimit;
			} else if (oid == 7) {
				body[6] = connection->longRetryLimit;
			}
			goto done;
		}


		case 25:	/* HostCmd_CMD_MAC_REG_ACCESS */
		{
			unsigned int reg = HOSTCMD_U16(body + 2);
			uint32_t value;

			if (action == 1) {
				value = __aeabi_uread4(body + 4);
				if (reg == 0xc000) {
					if (value != 0) {
						wlanCoexConfig.btConfig = 4;
						wlanCoexConfig.period[1] = 40000;
						wlanCoexConfig.reserve[1].word = 25000;
						wlanCoexConfig.flags[1] = 0;
					} else {
						wlanCoexConfig.btConfig = 2;
						wlanCoexConfig.period[0] = 20000;
						wlanCoexConfig.reserve[0].word = 10000;
						wlanCoexConfig.flags[0] = 0;
					}
				} else if (reg == 0xc001) {
					hostCmd0019Build();
				} else if (reg == 0xd000) {
					wlanCoexSetBtLinkLimit(value);
				} else {
					REG32(0x80000000 + reg) = value;
				}
			} else {
				if (reg == 0xc000)
					value = wlanCoexConfig.btConfig >> 1;
				else if (reg == 0xc001)
					value = hostCmd0019Parse();
				else if (reg == 0xd000)
					value = wlanCoexBtLinkLimit();
				else
					value = REG32(0x80000000 + reg);
				__aeabi_uwrite4(value, body + 4);
			}
			goto done;
		}

		case 26:	/* HostCmd_CMD_BBP_REG_ACCESS */
			if (action == 1)
				bbpWriteRegister(HOSTCMD_U16(body + 2), body[4]);
			else
				body[4] = bbpReadVersionRegister(HOSTCMD_U16(body + 2));
			goto done;

		case 27:	/* HostCmd_CMD_RF_REG_ACCESS */
			rfBusAcquire();
			if (action == 1)
				rfBusWrite(HOSTCMD_U16(body + 2), body[4]);
			else
				body[4] = rfBusRead(HOSTCMD_U16(body + 2));
			rfBusRelease();
			goto done;

		case 28:	/* HostCmd_CMD_802_11_RADIO_CONTROL */
			if (action != 1) {
				PUT16(body + 2, wlanPrimaryEvents & 7);
				goto done;
			}
			if (HOSTCMD_U16(body + 2) != 0)
				wlanPrimaryEvents |= 1;
			else
				wlanPrimaryEvents &= ~1;
			if (wlanPrimaryEvents & 1) {
				sleepMacResume();
			} else if (!wlanNoConnectionActive() || hostCmd001cResolve() != 0) {
				wlanPrimaryEvents |= 1;
				result = 1;
			} else {
				macTxWaitIdle();
				sleepMacHalt();
			}
			goto done;

		case 30:	/* HostCmd_CMD_802_11_RF_TX_POWER */
			if (action == 1)
				wlanTxPowerSetHostLimit(HOSTCMD_U16(body + 2), 1);
			if (connection->flags2 & 0x10) {
				uint8_t channel[2];

				hostCmd001eChannel(channel);
				body[2] = wlanHwInfo.txPowerLevel[0];
				body[3] = wlanHwInfo.txPowerLevel[1];
				body[5] = wlanTxPowerMinimum(channel[1], channel[0] & 3);
				body[4] = wlanTxPowerAllowed(channel[1], channel[0] & 3, 1);
			} else {
				body[2] = 0;
				body[3] = 0;
				body[5] = wlanTxPowerMin(1, 0);
				body[4] = wlanTxPowerMax(1, 0);
			}
			PUT16(hdr + 2, 14);
			goto done;

		case 31:	/* HostCmd_CMD_RSSI_INFO */
			hdr[2] = 16;
			hdr[3] = 0;
			PUT16(body + 2, wlanClampNoise(connection->dataNoiseFloor));
			PUT16(body, wlanClampSnr(connection->dataSnr));
			PUT16(body + 6, wlanClampNoise(wlanNoiseFloor(connection)));
			PUT16(body + 4, wlanClampSnr(wlanDataSnr(connection)));
			if ((connection->mode != 3 && connection->mode != 1) || (connection->flags2 & 0x10))
				goto done;
			goto failed;

		case 32:	/* HostCmd_CMD_802_11_ANTENNA */
			if (hostCmd0020(body, action, &result, &wlanRadioConfig->secondaryAntenna))
				goto done;
			switch (action) {
			case 1:
				if (HOSTCMD_U16(body + 2) == 0xffff) {
					if (!(((hostCmdAntennaProfile() >> 2) & 3) & 2))
						goto failed;
					wlanAntennaSelectSecondary();
					a = wlanRadioConfig->secondaryAntenna;
				} else {
					a = (uint8_t)(body[2] - 1);
					if (wlanRadioConfig->primaryDiversity != 0)
						goto failed;
					b = (hostCmdAntennaProfile() >> 2) & 3;
					if (!(b & 2) && b != a)
						goto failed;
					wlanRadioConfig->secondaryAntenna = a;
					wlanRadioConfig->secondaryDiversity = 0;
				}
				hostCmd0020Set(0, a);
				goto done;
			case 2:
				if (HOSTCMD_U16(body + 2) == 0xffff) {
					if (!(hostCmdAntennaProfile() & 2))
						goto failed;
					if (wlanRadioConfig->secondaryDiversity == 0)
						goto failed;
					hostCmd0020Clear(wlanRadioConfig->secondaryDiversity);
					wlanRadioConfig->primaryDiversity = 1;
				} else {
					a = (uint8_t)(body[2] - 1);
					b = hostCmdAntennaProfile();
					if (!(b & 2) && (b & 3) != a)
						goto failed;
					wlanRadioConfig->primaryAntenna = a;
					hostCmd0020Setup(hostCmd0020Set(1, a));
					wlanRadioConfig->primaryDiversity = 0;
				}
				goto done;
			case 3:
				if (HOSTCMD_U16(body + 2) == 0xffff) {
					b = hostCmdAntennaProfile();
					if (!(((b >> 2) & 3) & 2))
						goto failed;
					if (!(b & 2))
						goto failed;
					hostCmd0020Clear(b << 30);
					wlanRadioConfig->secondaryDiversity = 1;
					wlanRadioConfig->primaryDiversity = 1;
					goto done;
				}
				a = (uint8_t)(body[2] - 1);
				b = hostCmdAntennaProfile();
				if ((b & 2) || (b & 3) == a) {
					c = (b >> 2) & 3;
					if ((c & 2) || c == a) {
						wlanRadioConfig->primaryAntenna = a;
						hostCmd0020Set(1, a);
						wlanRadioConfig->primaryDiversity = 0;
						hostCmd0020Setup((uintptr_t)wlanRadioConfig + 192);
						wlanRadioConfig->secondaryAntenna = a;
						hostCmd0020Set(0, a);
						wlanRadioConfig->secondaryDiversity = 0;
						goto done;
					}
				}
				goto failed;
			default:
				goto done;
			}

		case 36:	/* HostCmd_CMD_802_11_DEAUTHENTICATE */
		case 38:
			hostCmd0024(connection, body);
			goto filtered;

		case 40:	/* HostCmd_CMD_802_11_KEY_ENCRYPT? */
			connection->hostFlags = body[0];
			connection->txOptions = body[1];
			wlanBaTablesChangedHandler();
			hostCmdConnectionChanged();
			goto done;

		case 43:	/* HostCmd_CMD_802_11_AD_HOC_START */
			uapSqramFilterEntry(5);
			__aeabi_uwrite4(0, wlanIbssJoinState);
			wlanIbssConnection = connection;
			wlanPsFilterState = 0;
			wlanIbssAtimWindow = HOSTCMD_U16(hdr + 46);
			hostCmd002bStart(connection, hdr);
			goto quiet;

		case 44:	/* HostCmd_CMD_802_11_AD_HOC_JOIN */
			uapSqramFilterEntry(2);
			__aeabi_uwrite4(0, wlanIbssJoinState);
			wlanIbssConnection = connection;
			wlanPsFilterState = 0;
			wlanIbssAtimWindow = HOSTCMD_U16(hdr + 75);
			hostCmd002cJoin(connection, hdr);
			goto quiet;

		case 46:
			__aeabi_uwrite4(0, body);
			goto done;

		case 54:
			hostCmd0036(body, 0, connection->snmpBssParams[1] + 28);
			goto done;
		case 55:
			hostCmd0036(body, 1, connection->snmpBssParams[0] + 28);
			goto done;

		case 60:	/* automatic frequency control */
			wlanAfcEnabled = body[0];
			if (action != 0) {
				wlanAfcAuto = body[2];
				wlanAfcThreshold = body[4];
			} else {
				wlanAfcTimingOffset = HOSTCMD_U16(body + 2);
				wlanAfcCarrierOffset = HOSTCMD_U16(body + 4);
				hostCmd003c(1000 * (int16_t)HOSTCMD_U16(body + 2) / 954,
					    1000 * (int16_t)HOSTCMD_U16(body + 4) / 1102);
			}
			goto done;
		case 61:
		{
			int divisor = wlanAfcSamples != 0 ? wlanAfcSamples : 1;

			PUT16(body + 2, (954 * wlanAfcTimingOffset / 1000) / divisor);
			PUT16(body + 4, (1102 * wlanAfcCarrierOffset / 1000) / divisor);
			PUT16(body, wlanAfcEnabled);
			goto done;
		}

		case 64:	/* HostCmd_CMD_802_11_AD_HOC_STOP */
			if (!wlanIbssSmActive(connection))
				goto done;
			macMgmtPrepareDescriptor(0);
			wlanPsAdjustTable(connection);
			wlanPsCancelHostModeTimer();
			wlanPsSleepSequence = 0;
			wlanPsHostActive = 0;
			wlanPsHostModeExit();
			*(volatile uint8_t *)&MAC_644_STATE &= ~1;
			macMgmtNotifyEntry(connection);
			__aeabi_uwrite4(0, wlanIbssJoinState);
			wlanPsFilterState = 0;
			connection->txOptions &= ~0x20;
			connection->stationFlags &= ~0x80;
			connection->flags &= ~4;
			goto quiet;

		case 69:
			if (wlanMefConfig.wakeSignalled) {
				wlanHostSleepActive = &wlanHostSleepSettings;
				wlanMefConfig.activated = 1;
			}
			goto done;

		case 77:
			if (hostCmd004d(connection, body) == 0)
				goto done;
			goto failed;

		case 78:
			if (action == 1) {
				hostCmd004eSet(body, (uint16_t)(HOSTCMD_U16(hdr + 2) - 8));
				goto done;
			}
			if (action == 0) {
				PUT16(hdr + 2, hostCmd004eGet(body) + 12);
				goto done;
			}
			goto unsupported;

		case 91:	/* HostCmd_CMD_802_11_ADDTS? */
			if (action == 0) {
				PUT16(hdr + 2, hostCmd005bGet(connection, body + 2) + HOSTCMD_U16(hdr + 2));
				goto done;
			}
			if (action != 1)
				goto failed;
			if (hostCmd005bSet(connection, body + 2, bodyLength) != 0)
				goto done;
			goto failed;

		case 93:
		{
			unsigned int size = hostCmd005d(body);

			if (size == 0)
				goto failed;
			PUT16(hdr + 2, size + 8);
			goto done;
		}

		case 94:
			if (action == 1) {
				hostCmd005e(connection, body + 2, hdr + HOSTCMD_U16(hdr + 2));
				wlanBaTablesChangedHandler();
				if (connection->snmpBssParams[1] + 28 != NULL && (connection->snmpBssParams[1][38] & 1)) {
					hostCmdConnectionChanged();
				}
				if (wlanSessionId != 0 && wlanRoleOtherOnly() && (connection->flags & 1))
					wlanPsStartHostModeTimer(connection);
				goto done;
			}
			if (action == 0) {
				PUT16(hdr + 2, hostCmd005eGet(connection, body + 2) + 10);
				goto done;
			}
			goto unsupported;

		case 102:	/* host sleep parameters */
			if (action != 1) {
				PUT16(body + 2, sleepHostWakeMargin);
				PUT16(body + 4, wlanPsWakeAdvance);
				body[8] = sleepCalibrationHeld + 1;
				body[9] = sleepKeepWakeTiming + 1;
				body[6] = sleepWakeLatency;
				body[7] = sleepWakeLatency >> 8;
				PUT16(body + 10, (uint16_t)(sleepCalibrationTime * 1000 / sleepCalibrationSamples /
							    sleepHostWakeCount));
				goto done;
			}
			a = HOSTCMD_U16(body + 2);
			if (a != 0 && sleepHostWakeMargin != a) {
				sleepHostWakeMargin = a;
				sleepCalibrationInterval = 1000000 / a;
				sleepHostClockParameter = 1000000 / (a + 100);
			}
			if (HOSTCMD_U16(body + 4) != 0)
				wlanPsWakeAdvance = HOSTCMD_U16(body + 4);
			if (body[8] != 0)
				sleepCalibrationHeld = body[8] - 1;
			if (body[9] != 0 && sleepKeepWakeTiming != body[9] - 1)
				sleepKeepWakeTiming = body[9] - 1;
			if (HOSTCMD_U16(body + 6) != 0)
				sleepWakeLatency = HOSTCMD_U16(body + 6);
			if (HOSTCMD_U16(body + 10) != 0)
				sleepHostWakeCount = HOSTCMD_U16(body + 10);
			result = wlanPsRecalibrate();
			goto done;

		case 103:
			if (action == 1) {
				wlanPsWakeBeacons = HOSTCMD_U16(body + 2);
				wlanPsBeaconInterval = 0;
			}
			PUT16(body + 2, wlanPsWakeBeacons);
			goto done;

		case 104:
			if (action != 1) {
				PUT16(body + 2, wlanPsNullSuppressed ? 255 : wlanPsAltModeActive);
				goto done;
			}
			a = HOSTCMD_U16(body + 2);
			if (a == 255) {
				wlanPsNullSuppressed = 1;
				a = 10;
			} else {
				wlanPsNullSuppressed = 0;
			}
			wlanPsAltModeRequest = a;
			if ((wlanPsAltModeRequest == 0) == (wlanPsAltModeActive == 0))
				wlanPsAltModeActive = wlanPsAltModeRequest;
			else
				wlanPsPostEvent(46);
			goto done;

		case 107:
			if (body[0] != 1)
				goto unsupported;
			if (hostCmd006bSet(connection, body[1], body + 2, bodyLength - 2) + 1 != 0)
				goto done;
			goto failed;

		case 108:
			hdr[2] = 8;
			hdr[3] = 0;
			PUT16(hdr + 2, hostCmd006bGet(body, body[0]) + HOSTCMD_U16(hdr + 2));
			goto done;

		case 109:
		{
			UINT posture = sysIrqDisable();

			wlanCalibrationTableUpdate(body + 6, HOSTCMD_U16(body + 2), HOSTCMD_U16(body + 4), action);
			sysIrqRestore(posture);
			goto done;
		}

		case 110:
			if (hostCmd006e(connection, (char *)body, bodyLength) != 1)
				goto done;
			goto quiet;
		case 111:
			hostCmd006f(body, bodyLength);
			goto done;
		case 112:
		case 113:
		case 129:
		{
			unsigned int size;

			if (command == 112)
				size = hostCmd0070(body);
			else if (command == 113)
				size = hostCmd0071(connection, (char *)body);
			else
				size = hostCmd0081(body);
			if (size == 0)
				goto failed;
			PUT16(hdr + 2, size + 8);
			goto done;
		}

		case 116:
			if (action != 1) {
				PUT16(body + 2, sleepStats.mode);
				goto done;
			}
			a = HOSTCMD_U16(body + 2);
			if (a == 0)
				goto unsupported;
			if (a == 1 || a == 2) {
				sleepStats.mode = body[2];
				hostCmd0074Reset();
				hostCmd0074Arm();
				mdevIoctl(wlanSdioDev, SDIO_IOCTL_SLEEP_STATS, 0);
				wlanMefConfig.hostState = 0;
				goto done;
			}
			goto failed;

		case 117:	/* HostCmd_CMD_802_11_SUBSCRIBE_EVENT */
			if (action == 0) {
				PUT16(hdr + 2, wlanSubscribeEventsGet(connection, body) + 12);
				goto done;
			}
			wlanSubscribeEventsConfigure(connection, body, (int16_t)bodyLength, action);
			goto done;

		case 120:
		{
			uint32_t status = 0;
			uint32_t type;
			uint32_t extra = 0;
			uint32_t failedFlag = 0;
			unsigned int subCommand;
			unsigned int socEvents;
			void (*handler)(uint32_t *type, uint32_t *failedFlag);

			hostCmd0078Parse(&status, body, &type, &extra);
			if (status != 0)
				goto failed;
			socEvents = (SOC_EVENT_CONTROL << 22) >> 30;
			if (socEvents == 0)
				hostCmd0078Lock();
			subCommand = HOSTCMD_U16(body + 2);
			handler = hostCmd0078Handlers[12 * (uint8_t)type + 1 + 2 * subCommand];
			if (handler != NULL)
				handler(&type, &failedFlag);
			else
				failedFlag = 1;
			if (socEvents == 0)
				hostCmd0078Unlock();
			if (failedFlag != 0)
				goto failed;
			if ((uint8_t)extra != 0)
				PUT16(hdr + 2, HOSTCMD_U16(hdr + 2) + (uint8_t)extra);
			if (subCommand != 4)
				goto done;
			if (HOSTCMD_U16(body) != 0)
				PUT16(hdr + 2, HOSTCMD_U16(hdr + 2) + 8);
			else
				PUT16(hdr + 2, HOSTCMD_U16(hdr + 2) - 8);
			goto done;
		}

		case 127:
		{
			uint32_t rate;
			uint32_t flags;

			hostCmd007f(connection, &rate, &flags);
			hdr[2] = 10;
			hdr[3] = 0;
			body[0] = rate;
			body[1] = flags;
			goto done;
		}

		case 130:
			if (action == 1) {
				if (bodyLength == 2)
					result = hostCmd0082Set(NULL);
				else
					result = hostCmd0082Set(body);
			} else if (action == 0) {
				result = hostCmd0082Get(body);
				a = HOSTCMD_U16(body + 4);
				PUT16(hdr + 2, a != 0 ? a + 14 : 2);
				PUT16(body, 0);
				goto done;
			} else {
				result = 1;
			}
			PUT16(hdr + 2, 2);
			goto done;

		case 131:
			if (action == 1) {
				if (connection->interfaceType == 1)
					hostCmd0083(connection->bssType, connection->bssNumber, HOSTCMD_U16(body + 2));
				goto done;
			}
			if (connection->interfaceType == 1) {
				__aeabi_memcpy(body, wlanIbssGetInfo(), 16);
				hdr[2] = 24;
				hdr[3] = 0;
				PUT16(body + 2, connection->stationFlags & 1);
				goto done;
			}
			goto failed;

		case 132:
			if (connection->interfaceType == 1)
				hostCmd0084(action, body + 2, HOSTCMD_U16(hdr + 2));
			goto done;

		case 136:
			if (action != 1) {
				mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_EVENT_QUERY, (uintptr_t)body);
				goto done;
			}
			if (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_EVENT_FILTER, (uintptr_t)body) == 0)
				goto done;
			goto failed;

		case 138:
			hostCmd008a(body, &wlanPsBeaconInterval, &wlanPsWakeBeacons, &wlanPsWakeDtims,
				    &wlanPsListenBeacons);
			goto done;

		case 141:
			wlanTxStatsTakeRates((struct WlanTxRateStats *)body);
			PUT16(hdr + 2, 628);
			goto done;

		case 143:	/* calibration table */
			switch (HOSTCMD_U16(body + 2)) {
			case 1:
				if (action != 1)
					goto unsupported;
				if (hostCmd008fWrite(HOSTCMD_U16(body + 4), body + 6) == 1)
					goto failed;
				sysIrqRestore(sysIrqDisable());
				goto done;
			case 2:
			{
				uint16_t length;
				UINT posture;

				if (action == 0) {
					result = wlanCalibrationTableQuery(body + 6, &length);
					PUT16(body + 4, length);
					PUT16(hdr, HOSTCMD_U16(hdr) | 0x8000);
					PUT16(hdr + 6, result);
					PUT16(hdr + 2, length + 14);
					goto done;
				}
				posture = sysIrqDisable();
				wlanCalibrationTableUpdate(body + 6, 0xffff, HOSTCMD_U16(body + 4), action);
				sysIrqRestore(posture);
				goto done;
			}
			case 3:
				if (action != 1)
					goto unsupported;
				hostCmd008fConfigure(body + 6, HOSTCMD_U16(body + 4));
				result = hostCmd008fApply();
				goto done;
			case 4:
				goto unsupported;
			case 5:
				if (action == 1) {
					if (hostCmd008fResolve(body) != 0)
						goto done;
					goto failed;
				}
				if (action != 0)
					goto unsupported;
				a = hostCmd008fResolve(body);
				if (a == 0)
					goto failed;
				PUT16(hdr + 2, a + 14);
				goto done;
			default:
				goto done;
			}

		case 147:
			if (action == 1) {
				uint16_t value[4];

				if (bodyLength == 8) {
					value[0] = 0;
					value[2] = HOSTCMD_U16(body + 6);
					value[3] = 1;
				} else {
					value[0] = HOSTCMD_U16(body + 2);
					value[3] = 0;
				}
				value[1] = HOSTCMD_U16(body + 4);
				if (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PARAM_SET, (uintptr_t)value) == 0)
					goto done;
				goto failed;
			} else {
				uint16_t value[4];

				mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PARAM_GET, (uintptr_t)value);
				PUT16(body + 2, value[0]);
				PUT16(body + 4, value[1]);
				if (value[3] == 1)
					PUT16(body + 6, value[2]);
			}
			goto done;

		case 148:	/* HostCmd_CMD_802_11_TX_RATE_QUERY? */
		{
			uint16_t size;

			if (action == 1) {
				if (hostCmdRateStepsSet(body) == 0)
					goto unsupported;
				goto done;
			}
			size = HOSTCMD_U16(hdr + 2);
			hostCmdRateStepsGet(body, &size);
			PUT16(hdr + 2, size);
			goto done;
		}

		case 150:
			if (action == 1) {
				if (HOSTCMD_U16(body + 2) != 0) {
					if (hostCmd0096Tracking != 0)
						hostCmd0096Track();
					hostCmd0096Enable();
				} else {
					hostCmd0096Disable();
				}
				goto done;
			}
			PUT16(body + 2, hostCmd0096Get());
			goto done;

		case 151:
			if (body[0] < 100)
				osaFormatVersionBanner(body[0], (char *)body + 1);
			PUT16(hdr + 2, strlen((char *)body + 1) + 10);
			goto done;

		case 154:
			if (wlanMefConfigure(connection, body) == 0)
				result = 1;
			goto done;

		case 164:	/* the averaging windows of the RSSI */
			if (action == 1) {
				uint16_t beaconShift = connection->beaconRssiShift;
				uint16_t dataShift = connection->dataRssiShift;

				a = HOSTCMD_U16(body + 2);
				if (a != 0)
					connection->beaconRssiShift = wlanSignalAverageShift(a);
				a = HOSTCMD_U16(body + 4);
				if (a != 0)
					connection->dataRssiShift = wlanSignalAverageShift(a);
				hdr[2] = 38;
				hdr[3] = 0;
				if (connection->beaconRssiShift != beaconShift)
					wlanSignalBeaconReset(connection);
				if (connection->dataRssiShift != dataShift)
					wlanSignalDataReset(connection);
			} else {
				int rssi;
				int noise;

				PUT16(body + 2, 1 << connection->beaconRssiShift);
				PUT16(body + 4, 1 << connection->dataRssiShift);
				hdr[2] = 38;
				hdr[3] = 0;
				__aeabi_memcpy(body + 22, wlanRssiInfoMarks, 8);
				PUT16(body + 8, connection->noise);
				PUT16(body + 6, connection->beaconRssi);
				rssi = wlanBeaconRssi(connection);
				PUT16(body + 10, rssi);
				if (rssi == -98)
					PUT16(body + 10, connection->beaconRssiLast);
				PUT16(body + 16, connection->dataNoiseFloor);
				PUT16(body + 14, connection->dataRssi);
				PUT16(body + 18, wlanDataRssi(connection));
				noise = wlanNoiseFloor(connection);
				PUT16(body + 12, noise);
				PUT16(body + 20, noise);
				if (noise == -98) {
					PUT16(body + 12, connection->noiseLast);
					PUT16(body + 20, connection->noiseLast);
				}
			}
			goto done;

		case 169:	/* the host resets the firmware */
		{
			uint8_t *response;

			sysForceEventDispatch = 0;
			mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_EVENT_DONE, 0);
			osa_BufferFree(buffer);
			osa_BufferFree(hostCmdResponse);
			if (hostCmd00a9Shutdown() != 0)
				result = 1;
			wlanSessionId = 0;
			wlanPsSleepSequence = 0;
			wlanPsHostActive = 0;
			hostCmd00a9Reset();
			while ((hostCmdResponse = osa_BufferAlloc(&hostCmdResponsePool, 0x7d0, 0)) == NULL)
				osaTaskSleep(1);
			hostCmdResponse->dataOffset = 32;
			hostCmdResponse->context = connection;
			response = (uint8_t *)hostCmdResponse + 32;
			hostCmdResponseData = response;
			response[0] = command;
			response[1] = (command >> 8) | 0x80;
			PUT16(response + 2, bodyLength + 8);
			PUT16(response + 6, result);
			response[4] = hdr[4];
			response[5] = hdr[5];
			hostCmdRespond(connection, NULL);
			continue;
		}

		case 170:	/* the host puts the firmware to sleep */
		{
			struct WlanConnection *other;

			wlanPsPhaseReset();
			other = wlanConnectionFirst(3);
			if (other != NULL && other->mode != 0 && other->localName.length != 0) {
				other->localName.length = 2;
				macMgmtValidateTable(0);
				macMgmtScheduleTable(0);
				macMgmtNotifyTable();
			}
			macMgmtScheduleTable(0);
			macMgmtApplyTable();
			hdr[0] = command;
			hdr[1] = (command >> 8) | 0x80;
			PUT16(hdr + 6, result);
			sysIssuePrimaryBbpCommand((uint32_t)hostCmdResponseData, (uint32_t)hdr, HOSTCMD_U16(hdr + 2));
			osa_BufferFree(buffer);
			hostCmdRespond(connection, NULL);
			respond = 0;
			freeCommand = 0;
			while (osa_BufferClone(&hostCmdResponsePool) != 0)
				;
			hostCmdSuspendMacTasks();
			sleepMacHalt();
			hostCmdMacAwake = 0;
			SOC_PENDING_EVENT_CONTROL &= ~4u;
			sysForceEventDispatch = 1;
			goto done;
		}

		case 173:
			if (action == 1)
				pmicRegWrite(body[2], body[4]);
			else
				body[4] = pmicRegRead(body[2]);
			goto done;

		case 174:
			osaFormatVersionBanner(0, (char *)body);
			PUT16(hdr + 2, strlen((char *)body) + 8);
			goto done;

		case 175:
			result = hostCmd00af(connection);
			if (result != 0)
				goto done;
			goto quiet;

		case 176:
			if (HOSTCMD_U16(body) == 1)
				result = hostCmd00b0Set(connection, hdr);
			else
				result = hostCmd00b0Get(connection, hdr);
			goto done;

		case 177:
			a = hostCmd00b1(connection);
			if (a != 0) {
				if (a == 1)
					goto unsupported;
				if (a == 2)
					goto done;
				if (a != 5)
					goto failed;
				cbProcFilterContext();
			}
			respond = 0;
			goto done;

		case 178:
			a = hostCmd00b2(connection);
			if (a == 0) {
				respond = 0;
				goto done;
			}
			if (a != 2)
				goto failed;
			goto done;

		case 179:
			result = hostCmd00b3(connection, hdr);
			goto done;

		case 181:
			result = hostCmd00b5(connection, hdr);
			if (result != 0)
				goto done;
			goto quiet;

		case 197:
		{
			unsigned int length = bodyLength - 4;

			hostCmd00c5(action, body + 4, &length);
			PUT16(hdr + 2, length + 12);
			goto done;
		}

		case 199:
			hdr[2] = 8;
			hdr[3] = 0;
			PUT16(hdr + 2, hostCmd00c7(connection, body) + HOSTCMD_U16(hdr + 2));
			goto done;

		case 205:
			if (action == 1)
				hostCmd00cdSet(connection, body);
			else
				hostCmd00cdGet(body);
			goto done;

		case 206:
			if (hostCmd00ce(body, bodyLength) != 1) {
				PUT16(hdr + 2, 24);
				goto done;
			}
			goto quiet;

		case 207:
			hostCmd00cf(body, bodyLength);
			goto done;

		case 208:
			if (hostCmd00d0(body, bodyLength) != 1)
				goto failed;
			goto quiet;

		case 209:
			if (action == 1 && hostCmd00d1Set(connection, body) == 0)
				result = 1;
			PUT16(hdr + 2, hostCmd00d1Get(body) + 16);
			goto done;

		case 214:
		{
			uint8_t *rates0;
			uint8_t *rates1;
			uint8_t *rates2;
			struct WlanConnection *other;

			if (action == 1) {
				result = hostCmd00d6Parse(body + 4, (uint16_t)(bodyLength - 4), &rates0, &rates1, &rates2);
				if (result == 1)
					goto done;
				for (other = connection; other != NULL;
				     other = wlanConnectionNextByBss(other, other->bssType, other->bssNumber)) {
					if (other->flags2 & 0x10) {
						a = hostCmd00d6Apply(other, rates0, rates1, rates2);
						if (a != 0)
							result = a;
					} else {
						result = 1;
					}
				}
			}
			PUT16(hdr + 2, hostCmd00d6Get(connection, body + 4, wlanTxRateDefault) + 12);
			goto done;
		}

		case 217:
			if (action == 1) {
				wlanTxBufSizeSet((uint16_t *)(body + 2));
				wlanPsQueryPacket();
				wlanPsFormatBuffer(connection, wlanRateSelect(connection->rateControl));
				wlanPsReleaseBuffer();
				wlanTxBaArmTimeouts((uintptr_t)connection, wlanTxBaSessions);
				body[4] = wlanTxBufferCount;
				body[5] = 0;
			} else if (action == 0) {
				if (wlanMgmtFrameSize >= 7000)
					PUT16(body + 2, 7440);
				else if (wlanMgmtFrameSize >= 3000)
					PUT16(body + 2, 3720);
				else if (wlanMgmtFrameSize >= 1000)
					PUT16(body + 2, 1860);
			}
			a = HOSTCMD_U16(body + 2);
			b = (uint16_t)mdevIoctl(wlanSdioDev, SDIO_IOCTL_BLOCK_SIZE_QUERY, 0);
			PUT16(body + 2, a / b * b);
			goto done;

		case 221:
			result = hostCmd00dd(connection, body, bodyLength);
			goto done;

		case 223:
			if (action == 1) {
				wlanAmsduSize = HOSTCMD_U16(body + 2);
				wlanPsQueryPacket();
				wlanPsFormatBuffer(connection, wlanRateSelect(connection->rateControl));
			}
			PUT16(body + 2, wlanAmsduSize);
			PUT16(body + 4, wlanAmsduEnabled);
			goto done;

		case 224:
			result = hostCmd00e0(action, body + 4, (uint16_t)(bodyLength - 4));
			goto done;

		case 228:	/* HostCmd_CMD_802_11_PS_MODE_ENH */
			switch (action) {
			case 0:
				PUT16(body + 2, wlanSessionId);
				PUT16(hdr + 2, hostCmd00e4(connection, body + 4) + 12);
				goto done;
			case 5:
				release = wlanPsHostSleepReply;
				if (wlanMefConfig.wakeSignalled && wlanPsAltModeActive == 0) {
					wlanHostSleepActive = &wlanHostSleepSettings;
					wlanMefConfig.activated = 1;
					mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_SLEEP_CONFIG, 0);
				}
				goto done;
			case 254:
				a = HOSTCMD_U16(body + 2) & wlanSessionId;
				if (a & 1) {
					wlanPsPhaseReset();
					wlanSessionId &= ~1;
				}
				if ((a & 0x310) == 0)
					goto done;
				hdr[1] |= 0x80;
				PUT16(hdr + 6, result);
				sysIssuePrimaryBbpCommand((uint32_t)hostCmdResponseData, (uint32_t)hdr,
							  HOSTCMD_U16(hdr + 2));
				a = wlanPsEndSession(connection, a);
				freeCommand = 0;
				osa_BufferFree(buffer);
				if (wlanP2pConnection(connection))
					wlanConnectionPsState(connection)->triggerPending = 1;
				if (a == 0)
					goto done;
				respond = wlanPsHostModeExit();
				goto done;
			case 255:
			{
				struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
				struct WlanConnection *client;
				uint8_t params[12];

				a = HOSTCMD_U16(body + 2);
				if (((a >> 8) & 3) != 0 && ((wlanSessionId >> 8) & 3) != 0 &&
				    (a & 0x300) != (wlanSessionId & 0x300))
					goto failed;
				if (hostCmd00e4Check(connection, body + 4, (uint16_t)(HOSTCMD_U16(hdr + 2) - 12)) == 1)
					goto failed;
				wlanSessionId |= a;
				if (((a >> 8) & 3) != 0)
					info->sleep.mode = body[3];
				if (wlanSessionId & 1)
					wlanConnectionPhase = wlanConnectionsIdle() ? 2 : 6;
				if ((a & 0x310) == 0)
					goto done;
				hdr[1] |= 0x80;
				PUT16(hdr + 6, result);
				sysIssuePrimaryBbpCommand((uint32_t)hostCmdResponseData, (uint32_t)hdr,
							  HOSTCMD_U16(hdr + 2));
				client = p2pDirectProcessEntry(2);
				if (client == NULL)
					client = wlanConnectionFirst(2);
				if ((connection->interfaceType == 2 || connection->interfaceType == 3) &&
				    client != NULL && client->state == 7)
					wlanConnectionActivate(connection);
				__aeabi_memcpy(params + 2, wlanPsHostConfig.holdSeconds, 10);
				freeCommand = 0;
				osa_BufferFree(buffer);
				wlanPsCancelHostModeTimer();
				respond = wlanPsHostModeEnter(params);
				goto done;
			}
			default:
				goto done;
			}

		case 229:
			if (action == 1) {
				if (!wlanCoexUseP2pMode())
					wlanHostSleepEndOnWake = 1;
			} else {
				if (action != 2 || !wlanMefConfig.wakeSignalled || wlanPsAltModeActive != 0)
					goto done;
				wlanHostSleepActive = &wlanHostSleepSettings;
				wlanMefConfig.activated = 1;
			}
			if (action == 1) {
				if (__aeabi_uread4(body + 2) == 0xffffffff) {
					__aeabi_uwrite4(-1, &wlanHostSleepSettings);
					wlanMefConfig.wakeSignalled = 0;
					wlanHostSleepEndOnWake = 0;
				} else {
					__aeabi_memcpy(&wlanHostSleepSettings, body + 2, 6);
					if (wlanHostSleepEndOnWake != 0)
						wlanMefConfigParse(body + 8, (int16_t)(bodyLength - 8));
					else
						wlanMefConfigParse(body + 6, (int16_t)(bodyLength - 6));
					wlanMefConfig.wakeSignalled = 1;
					if (wlanHostSleepSettings.gpio != -1 && wlanHostSleepSettings.gpio != -2) {
						mdevIoctl(wlanSdioDev, SDIO_IOCTL_WAKE_GPIO_SET, wlanHostSleepSettings.gpio);
						mdevIoctl(wlanSdioDev, SDIO_IOCTL_WAKE_GPIO_ARM, 0);
						wlanHostWakeGpioSet((uint8_t)wlanHostSleepSettings.gpio, 1);
					}
				}
			}
			goto done;

		case 233:
			if (hostCmd00e9(connection, body, bodyLength) != 1)
				goto failed;
			goto quiet;

		case 234:
		case 235:
		case 236:
			result = hostCmd00ea(connection, hdr);
			goto done;

		case 237:
			hostCmd00ed();
			if (action == 1)
				hostCmd00edSet(HOSTCMD_U16(body + 2), body[4]);
			else
				body[4] = hostCmd00edGet(HOSTCMD_U16(body + 2));
			hostCmd00edEnd();
			goto done;

		case 242:
		{
			uint16_t size = HOSTCMD_U16(hdr + 2);
			int status = wlanCustomIeCommand(connection, action, body + 2, &size);

			PUT16(hdr + 2, size);
			if (status == 0)
				goto done;
			goto failed;
		}

		case 244:
			if (hdr[19] == 9)
				hostCmd00f4(connection, hdr);
			goto done;

		case 245:
		{
			unsigned int length = bodyLength - 2;

			result = hostCmd00f5(connection, action, body + 2, &length);
			PUT16(hdr + 2, (int16_t)length + 10);
			goto done;
		}

		case 247:
			wlanConnectionSetBssMode(connection, body[0], NULL, NULL);
			goto done;

		case 251:
		{
			unsigned int length;

			if (action == 1) {
				length = bodyLength - 4;
				if (hostCmd00fb(connection, action, body + 4, &length) != 0)
					goto done;
				goto failed;
			}
			if (action != 0)
				goto failed;
			if (hostCmd00fb(connection, action, body + 4, &length) == 0)
				result = 1;
			PUT16(hdr + 2, length + 12);
			goto done;
		}

		case 257:
			if (action == 0)
				PUT16(body + 2, hostCmdNode(body));
			hdr[2] = 12;
			hdr[3] = 0;
			goto done;

		case 258:
			if (action == 1) {
				if (HOSTCMD_U16(body + 2) != 1) {
					if (wlanConnectionHasPrimary()) {
						MAC_300_MODE &= ~8u;
						osaTaskSleep(10);
						wlanRxFrameTypeMask = 0;
						wlanConnectionSetPrimary(NULL);
						hostCmd0102(0);
						MAC_300_MODE |= 8;
					}
					goto done;
				}
				if (HOSTCMD_U16(body + 6) != 0x12a)
					goto failed;
				MAC_300_MODE &= ~8u;
				osaTaskSleep(10);
				wlanRxFrameTypeMask = HOSTCMD_U16(body + 4);
				macMgmtUpdateContext(body[10], body[11]);
				macMgmtCheckContext();
				wlanConnectionSetPrimary(connection);
				hostCmd0102(1);
				MAC_300_MODE |= 8;
				goto done;
			}
			if (action == 0) {
				uint8_t band;
				uint32_t extra;

				body[6] = 42;
				body[7] = 1;
				body[8] = 2;
				body[9] = 0;
				macMgmtPrepareContext(&band, &extra);
				body[10] = band;
				body[11] = extra;
				PUT16(body + 2, wlanConnectionHasPrimary() != 0);
				PUT16(body + 4, wlanRxFrameTypeMask);
				goto done;
			}
			goto failed;

		case 259:
			wlanApPsBufferCommand(connection, (struct WlanApPsBufferConfig *)body);
			PUT16(hdr + 2, 12);
			goto done;

		case 292:
			if (bbpSetAdjustment(action, body[2]) != 1)
				goto done;
			goto unsupported;

		case 511:
			hdr[2] = 8;
			hdr[3] = 0;
			PUT16(hdr + 2, hostCmd01ff() + HOSTCMD_U16(hdr + 2));
			goto done;

		default:
			if ((hostCmdMisc(&result, body, connection) | result) == 0)
				result = 2;
			goto done;
		}

	failed:
		result = 1;
		goto done;
	unsupported:
		result = 2;
		goto done;
	filtered:
		respond = 0;
		cbProcFilterContext();
		goto finish;
	quiet:
		respond = 0;
		goto finish;
	done:
		if (respond) {
			hdr[1] |= 0x80;
			PUT16(hdr + 6, result);
			sysIssuePrimaryBbpCommand((uint32_t)hostCmdResponseData, (uint32_t)hdr,
						  HOSTCMD_U16(hdr + 2));
		}
	finish:
		if (freeCommand)
			osa_BufferFree(buffer);
		if (respond)
			hostCmdRespond(connection, release);
	}
}
