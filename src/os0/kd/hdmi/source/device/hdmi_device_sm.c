/*
 * SceHdmi device layer: the connection state machine (Sony's
 * device/hdmi_device_sm.c). The "SceHdmiDeviceSm" thread follows the sink
 * from hot plug through the EDID, power on, the video mode and the PLL lock
 * to HDCP authentication, driven by the events the interrupt handlers and
 * the sceHdmiDevice* commands set. Internal names are ours unless a comment
 * says otherwise.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr.h>
#include "fw.h"
#include "sce/debug.h"
#include "sce/dtrace.h"
#include "../hdmi_internal.h"

int sceKernelDelayThread(SceUInt32 usec);
int sceKernelExitThread(int status);

#define HDMI_DBG_AUTH	0x00000020	/* "[hdmiAUTH] " */
#define HDMI_DBG_INFO	0x40000000	/* "[HDMI    ] " */

/* The state machine's event flag */
#define HDMI_SM_EVENT_UNPLUG	0x0001	/* the sink went away (or must be read again) */
#define HDMI_SM_EVENT_PLUG	0x0002	/* hot plug */
#define HDMI_SM_EVENT_VIDEO	0x0004	/* a video mode was set */
#define HDMI_SM_EVENT_POWER_ON	0x0008	/* the sink's receiver is powered (rx sense) */
#define HDMI_SM_EVENT_REAUTH	0x0010	/* never set */
#define HDMI_SM_EVENT_AUTH_ERR	0x0020	/* the transmitter's Ri check failed */
#define HDMI_SM_EVENT_STOP	0x0040	/* _sceHdmiDeviceSmStopService */
#define HDMI_SM_EVENT_REAUTH2	0x0400	/* never set */
#define HDMI_SM_EVENT_NONE	0x0800	/* never set */
#define HDMI_SM_EVENT_PACKET	0x2000	/* never set */
#define HDMI_SM_EVENT_START	0x4000	/* _sceHdmiDeviceSmRestart */
#define HDMI_SM_EVENT_EXIT	0x8000

/* The DDC event flag's bits the authentication waits for */
#define HDMI_DDC_EVENT_ABORT	0x01
#define HDMI_DDC_EVENT_HDCP	0x08	/* authentication done */
#define HDMI_DDC_EVENT_ERROR	0x10
#define HDMI_DDC_EVENT_BKSV	0x04	/* the receiver's KSV is in */
#define HDMI_DDC_EVENT_EDID	0x1c	/* what a restart clears */

/* HdmiDevice.state */
#define HDMI_SM_STATE_STOPPED		0
#define HDMI_SM_STATE_UNPLUGGED		1
#define HDMI_SM_STATE_PLUGGED		2
#define HDMI_SM_STATE_WAIT_POWER_ON	3
#define HDMI_SM_STATE_WAIT_RESOLUTION	4
#define HDMI_SM_STATE_WAIT_PLL_LOCK	5
#define HDMI_SM_STATE_AUTH		6
#define HDMI_SM_STATE_AUTHED		7
#define HDMI_SM_STATE_NO_HDCP		8
#define HDMI_SM_STATE_RUNNING		9
#define HDMI_SM_STATE_RUNNING_NO_HDCP	10
#define HDMI_SM_STATE_EXIT		12
#define HDMI_SM_STATE_KEEP		(-1)	/* no change */

/* HdmiDevice.authState: the HDCP authentication steps */
#define HDMI_AUTH_IDLE		0
#define HDMI_AUTH_STEP1		1	/* read R0 */
#define HDMI_AUTH_STEP2		2
#define HDMI_AUTH_REPEATER	3	/* read the repeater's KSV list */
#define HDMI_AUTH_STEP4		4	/* wait for the end */

/* hdmiDeviceSmAuthenticate */
#define HDMI_AUTH_DONE		0
#define HDMI_AUTH_FAILED	1
#define HDMI_AUTH_CONTINUE	2
#define HDMI_AUTH_SKIPPED	3

/* the interrupts hdmiDeviceEnableIntr/DisableIntr take */
#define HDMI_INTR_MASK_HPD2	2
#define HDMI_INTR_MASK_AUTH	6

#define HDMI_SM_CALLBACK_COUNT	6

/* What the state machine's callbacks are told */
#define HDMI_SM_CALLBACK_DISCONNECT	0
#define HDMI_SM_CALLBACK_EDID		1
#define HDMI_SM_CALLBACK_CONNECT	2
#define HDMI_SM_CALLBACK_RECONNECT	3
#define HDMI_SM_CALLBACK_AUTH_FAILED	4

/* the thread's option parameter: size and attributes only */
typedef struct HdmiSmThreadOptParam {
	SceSize size;
	SceUInt32 attr;
} HdmiSmThreadOptParam;

typedef struct HdmiSmWork {
	SceUID eventFlag;			/* 0x00 */
	SceUID thread;				/* 0x04 */
	SceKernelEventFlagOptParam eventFlagOpt;	/* 0x08 */
	HdmiSmThreadOptParam threadOpt;		/* 0x0c */
} HdmiSmWork;

typedef struct HdmiSmServiceWork {
	SceUID mutex;				/* 0x00: guards HdmiDevice.state */
	SceUInt32 reserved[3];
} HdmiSmServiceWork;

/* .bss */
static HdmiSmWork smWork;
#if FW_SINCE(0x04000000)
void (*stateProbeHook)(const SceDtraceProbe *probe, ...);
#endif
static HdmiSmServiceWork serviceWork;
#if FW_SINCE(0x04000000)
int stateProbeEnabled;
#endif
static HdmiEventCallback callbacks[HDMI_SM_CALLBACK_COUNT];

#if FW_SINCE(0x04000000)
static const SceDtraceProbe stateProbe;
#endif

static int smThread(SceSize args, void *argp);

int hdmiDeviceSmInitialize(void)
{
	int ret, ret2;

	smWork = (HdmiSmWork){ 0 };
	smWork.eventFlag = ksceKernelCreateEventFlag("SceHdmiDeviceSm", SCE_EVENT_WAITMULTIPLE, 0,
						     &smWork.eventFlagOpt);
	smWork.threadOpt.size = sizeof(smWork.threadOpt);
	smWork.thread = ksceKernelCreateThread("SceHdmiDeviceSm", smThread, 0x1e, 0x2000,
					       0x10000000, 0x80000,
					       (SceKernelThreadOptParam *)&smWork.threadOpt);
	ret = smWork.thread < 0 ? SCE_HDMI_ERROR_DEVICE : 0;
	ret2 = ksceKernelStartThread(smWork.thread, 0, NULL);
	if (ret2 == 0)
		ret2 = ret;
	return ret2;
}

/* Did one of the events in pattern come? The events stay set. */
int hdmiDeviceSmPollEvent(SceUInt32 *events, SceUInt32 pattern)
{
	return ksceKernelPollEventFlag(smWork.eventFlag, pattern, SCE_EVENT_WAITOR, events) == 0;
}

/* Wait for one of the events in pattern and clear those that came. */
int hdmiDeviceSmWaitEvent(SceUInt32 *events, SceUInt32 pattern)
{
	SceUInt32 cleared;
	int ret;

	HDMI_PRINTF(HDMI_DBG_SEVT, "[hdmiSEVT] ", "WaitSystemEvent Ptn:0x%08X\n", pattern);
	ret = ksceKernelWaitEventFlag(smWork.eventFlag, pattern, SCE_EVENT_WAITOR, events, NULL);
	cleared = pattern & *events;
	ksceKernelClearEventFlag(smWork.eventFlag, ~cleared);
	HDMI_PRINTF(HDMI_DBG_SEVT, "[hdmiSEVT] ", "Event Occure :0x%08X\n", *events);
	HDMI_PRINTF(HDMI_DBG_SEVT, "[hdmiSEVT] ", "Event Clear  :0x%08X\n", cleared);
	return ret;
}

int hdmiDeviceSmSetEvent(SceUInt32 bits)
{
	HDMI_PRINTF(HDMI_DBG_SEVT, "[hdmiSEVT] ", "Event Set :0x%08X\n", bits);
	return ksceKernelSetEventFlag(smWork.eventFlag, bits);
}

int hdmiDeviceSmClearAllEvents(void)
{
	return ksceKernelClearEventFlag(smWork.eventFlag, 0);
}

int hdmiDeviceSmClearEvent(SceUInt32 bits)
{
	return ksceKernelClearEventFlag(smWork.eventFlag, ~bits);
}

/* Stop HDCP: no encryption, no authentication interrupts, start again at step 1. */
void hdmiDeviceSmAuthReset(HdmiDevice *device)
{
	HdmiDeviceHwAuthStop(device);
	HdmiDeviceHwSetCypher(device, 0);
	hdmiDeviceDisableIntr(HDMI_INTR_MASK_AUTH);
	device->authState = HDMI_AUTH_IDLE;
	device->repeater = 0;
	device->aksvValid = 0;
	device->ksvCount = 0;
}

/* Sony's name (its messages). Wait for the receiver's KSV. */
static inline int _authWaitBksv(SceUInt32 timeout)
{
	SceUInt32 events;
	int ret;

	hdmiDeviceEnableIntr(HDMI_INTR_MASK_AUTH);
	ret = hdmiDeviceDdcWaitEvent(&events, HDMI_DDC_EVENT_ERROR | HDMI_DDC_EVENT_BKSV |
				     HDMI_DDC_EVENT_ABORT, &timeout);
	if (ret != 0) {
		HDMI_ERR_PRINTF("[hdmiERR ] _authWaitBksv Event ERROR !!!:0x%08X\n", ret);
		hdmiDeviceDisableIntr(HDMI_INTR_MASK_AUTH);
		return -1;
	}
	if (events & (HDMI_DDC_EVENT_ERROR | HDMI_DDC_EVENT_ABORT)) {
		HDMI_ERR_PRINTF("[hdmiERR ] _authWaitBksv Receive DDC-ERROR or Shutdown:0x%08X\n",
				events);
		sceKernelDelayThread(500000);
		hdmiDeviceDdcClearEvent(HDMI_DDC_EVENT_ERROR | HDMI_DDC_EVENT_ABORT);
		hdmiDeviceDisableIntr(HDMI_INTR_MASK_AUTH);
		return -1;
	}
	hdmiDeviceDisableIntr(HDMI_INTR_MASK_AUTH);
	return 0;
}

/* Sony's name (its messages). Wait for the end of the authentication. */
static inline int _authWaitHdcpDone(void)
{
	SceUInt32 events;
	SceUInt32 timeout = 5100000;
	int ret;

	ret = hdmiDeviceDdcWaitEvent(&events, HDMI_DDC_EVENT_ERROR | HDMI_DDC_EVENT_HDCP |
				     HDMI_DDC_EVENT_ABORT, &timeout);
	if (ret != 0) {
		HDMI_ERR_PRINTF("[hdmiERR ] _authWaitHdcpDone Event ERROR!!!:0x%08X\n", ret);
		return -1;
	}
	if (events & (HDMI_DDC_EVENT_ERROR | HDMI_DDC_EVENT_ABORT)) {
		HDMI_ERR_PRINTF("[hdmiERR ] _authWaitHdcpDone Receive DDC-ERROR or Shutdown:0x%08X\n",
				events);
		sceKernelDelayThread(100000);
		return -1;
	}
	return 0;
}

/* One step of the HDCP authentication (device->authState). */
int hdmiDeviceSmAuthenticate(HdmiDevice *device)
{
	SceUInt8 bcaps = 0, count = 0;
	SceUInt8 bstatus[2] = { 0, 0 };
	int result;
	int ret;

	switch (device->authState) {
	case HDMI_AUTH_IDLE:
		device->authState = HDMI_AUTH_STEP1;
		/* fall through */
	case HDMI_AUTH_STEP1:
		HDMI_FLAG_PRINTF(HDMI_DBG_INFO, "[HDMI    ] Step1 R0 Read waittime : %d\n", 0);
		if (HdmiDeviceHwAuthStart(device) != 0)
			goto failed;
		if (_authWaitBksv(500000) != 0)
			goto failed;
		HdmiDeviceHwReadAksv(device, device->aksv);
		HdmiDeviceHwReadBksv(device, device->ksvList);
		HdmiDeviceHwCheckRepeater(device);
		device->ksvCount = 1;
		if (device->repeater) {
			device->authState = HDMI_AUTH_REPEATER;
			return HDMI_AUTH_CONTINUE;
		}
		device->authState = HDMI_AUTH_STEP4;
		break;
	case HDMI_AUTH_STEP2:
		HDMI_PRINTF(HDMI_DBG_AUTH, "[hdmiAUTH] ", "Step 2-1 is through.\n");
		device->authState = HDMI_AUTH_REPEATER;
		return HDMI_AUTH_CONTINUE;
	case HDMI_AUTH_REPEATER:
		if (_authWaitBksv(7200000) != 0)
			return HDMI_AUTH_FAILED;
		ret = HdmiDeviceHwReadBcaps(device, &bcaps);
		if (ret != 0) {
			HDMI_ERR_PRINTF("[hdmiERR ] HdmiDeviceHwReadBcaps Error:0x%08X\n", ret);
			return HDMI_AUTH_FAILED;
		}
		ret = HdmiDeviceHwReadBstatus(device, bstatus);
		if (ret != 0 || bstatus[0] > 13 || (bstatus[1] & 0x08)) {
			HDMI_ERR_PRINTF("[hdmiERR ] HdmiDeviceHwReadBstatus Error:0x%08X\n", ret);
			device->ksvCount = 0;
			return HDMI_AUTH_FAILED;
		}
		if (bstatus[0] != 0) {
			ret = HdmiDeviceHwReadBksvCount(device, &count);
			if (count != bstatus[0] || ret != 0) {
				HDMI_ERR_PRINTF("[hdmiERR ] readBKSVCount Error:0x%08X Count:0x%02X(Bstatus) %02X(Reg)\n",
						ret, bstatus[0], count);
				return HDMI_AUTH_FAILED;
			}
			ret = HdmiDeviceHwReadKSVListFromFIFO(device, count * 5,
							      device->ksvList + device->ksvCount * 5);
			if (ret != 0) {
				device->ksvCount = 0;
				return HDMI_AUTH_FAILED;
			}
			device->ksvCount += count;
		}
		break;
	case HDMI_AUTH_STEP4:
		break;
	default:
		return HDMI_AUTH_FAILED;
	}
	if (_authWaitHdcpDone() != 0)
		goto failed;
	ret = HdmiDeviceHwSetAvMute(device, 0);
	if (HdmiDeviceHwSetCypher(device, 1) < 0)
		result = HDMI_AUTH_FAILED;
	else
		result = ret < 0;
	if (hdmiDeviceEnableIntr(HDMI_INTR_MASK_HPD2) < 0) {
		HdmiDeviceHwSetCypher(device, 0);
		result = HDMI_AUTH_FAILED;
	}
	return result;
failed:
	device->authState = HDMI_AUTH_IDLE;
	device->ksvCount = 0;
	return HDMI_AUTH_FAILED;
}

/* Sony's name. Start the state machine again from its unplugged state. */
int _sceHdmiDeviceSmRestart(HdmiDevice *device)
{
	int ret;

	hdmiDeviceSmClearAllEvents();
	ret = hdmiDeviceSmSetEvent(HDMI_SM_EVENT_START);
	if (device->state != HDMI_SM_STATE_UNPLUGGED)
		hdmiDeviceSmSetEvent(HDMI_SM_EVENT_UNPLUG);
	return ret;
}

/* Sony's name. */
int _sceHdmiDeviceSmStopService(HdmiDevice *device, int wait)
{
	int ret;

	ret = hdmiDeviceSmClearAllEvents();
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(190);
		return ret;
	}
	if (wait == 0)
		return hdmiDeviceSmSetEvent(HDMI_SM_EVENT_STOP);
	hdmiDeviceSmLock();
	device->request = 0;
	hdmiDeviceSmUnlock();
	hdmiDeviceSmAuthReset(device);
	return ret;
}

/* Everything off: no output, no EDID, unpowered. */
static inline void _smPowerDown(HdmiDevice *device)
{
	hdmiDeviceSetPowered(0);
	HdmiDeviceHwSetAvMute(device, 1);
	hdmiDeviceSmStopPackets(device);
	hdmiDeviceSmAuthReset(device);
	HdmiDeviceHwSetTMDSPower(device, 0);
	hdmiDeviceSmClearEvent(HDMI_SM_EVENT_UNPLUG);
	hdmiDeviceSmClearEvent(HDMI_SM_EVENT_AUTH_ERR);
	hdmiDeviceDdcClearEvent(HDMI_DDC_EVENT_EDID);
	hdmiDeviceEdidClear(device);
	hdmiDeviceEdidSetDviMode(device);
	hdmiDeviceSmLock();
	device->request = 0;
	hdmiDeviceSmUnlock();
}

/* The sink went away: power down and tell the upper layer. */
static inline int _smDisconnect(HdmiDevice *device)
{
	_smPowerDown(device);
	if (callbacks[HDMI_SM_CALLBACK_DISCONNECT] != NULL)
		callbacks[HDMI_SM_CALLBACK_DISCONNECT]();
	device->firstConnect = 1;
	device->aksvValid = 0;
	device->ksvCount = 0;
	return HDMI_SM_STATE_UNPLUGGED;
}

/* Stop the output (TMDS, packets, HDCP), keeping the EDID. */
static inline int _smStopOutput(HdmiDevice *device, int state)
{
	HdmiDeviceHwSetAvMute(device, 1);
	hdmiDeviceSmStopPackets(device);
	hdmiDeviceSmAuthReset(device);
	HdmiDeviceHwSetTMDSPower(device, 0);
	hdmiDeviceSmClearEvent(HDMI_SM_EVENT_AUTH_ERR);
	hdmiDeviceDdcClearEvent(HDMI_DDC_EVENT_EDID);
	device->aksvValid = 0;
	device->ksvCount = 0;
	return state;
}

static inline void _smPowerOn(HdmiDevice *device)
{
	int powered;

	hdmiDeviceGetPowered(&powered);
	if (!powered) {
		HdmiDeviceHwPowerOn(device);
		hdmiDeviceSetPowered(1);
	}
}

/* The checks every connected state starts with. */
static inline void _smClearPlugEvents(void)
{
	hdmiDeviceSmClearEvent(HDMI_SM_EVENT_POWER_ON);
	hdmiDeviceSmClearEvent(HDMI_SM_EVENT_UNPLUG);
	hdmiDeviceSmClearEvent(HDMI_SM_EVENT_PLUG);
}

static inline int _smStopped(HdmiDevice *device)
{
	SceUInt32 events;

	hdmiDeviceSmLock();
	device->request = 0;
	hdmiDeviceSmUnlock();
	hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_START | HDMI_SM_EVENT_EXIT);
	if (events & HDMI_SM_EVENT_START) {
		HdmiDeviceHwReset(device);
		return _smDisconnect(device);
	}
	if (events & HDMI_SM_EVENT_EXIT)
		return HDMI_SM_STATE_EXIT;
	return HDMI_SM_STATE_KEEP;
}

static inline int _smWaitPlug(HdmiDevice *device)
{
	SceUInt32 events;

	cecDevPoll();
	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusGetPlgStat(device) == 1 || hdmiDeviceStatusGetPlgStat(device) == 2)
		return HDMI_SM_STATE_PLUGGED;
	hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_STOP | HDMI_SM_EVENT_POWER_ON |
			      HDMI_SM_EVENT_PLUG);
	if (events & HDMI_SM_EVENT_STOP)
		return HDMI_SM_STATE_STOPPED;
	if (events & (HDMI_SM_EVENT_POWER_ON | HDMI_SM_EVENT_PLUG))
		return HDMI_SM_STATE_PLUGGED;
	return HDMI_SM_STATE_KEEP;
}

/* Plugged: read the EDID. */
static inline int _smReadEdid(HdmiDevice *device)
{
	SceUInt8 address[2];

	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusIsUnplugged(device) == 1 || hdmiDeviceStatusGetPlgStat(device) == 0)
		return _smDisconnect(device);
	_smPowerOn(device);
	hdmiDeviceEnableIntr(1);
	if (hdmiDeviceEdidRead(device) != 0) {
		HdmiDeviceAD80244ClearBksvFlag(3);
		return _smDisconnect(device);
	}
	hdmiDeviceEdidCheckSinkType(device);
	if (callbacks[HDMI_SM_CALLBACK_EDID] != NULL)
		callbacks[HDMI_SM_CALLBACK_EDID]();
	if (hdmiDeviceEdidGetPhysicalAddress(device, address) == 0)
		cecDevSetPhysicalAddress(address);
	return HDMI_SM_STATE_WAIT_POWER_ON;
}

/* Sony's name (debug records). */
static inline int _smWaitPowerOn(HdmiDevice *device)
{
	SceUInt32 events;
	int plg;

	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusIsChanged(device) == 1)
		return _smDisconnect(device);
	_smPowerOn(device);
	_smClearPlugEvents();
	plg = hdmiDeviceStatusGetPlgStat(device);
	if (plg != 1) {
		if (plg == 0)
			return _smDisconnect(device);
		if (plg != 2)
			SCE_DBG_ASSERT_FAIL(540);
	}
	hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_STOP | HDMI_SM_EVENT_POWER_ON |
			      HDMI_SM_EVENT_UNPLUG);
	if (events & HDMI_SM_EVENT_STOP)
		return HDMI_SM_STATE_STOPPED;
	if (events & HDMI_SM_EVENT_UNPLUG)
		return _smDisconnect(device);
	if (events & HDMI_SM_EVENT_POWER_ON)
		return HDMI_SM_STATE_WAIT_RESOLUTION;
	return HDMI_SM_STATE_KEEP;
}

/* Sony's name (debug records). */
static inline int _smWaitResolutionSet(HdmiDevice *device)
{
	SceUInt32 events;
	int plg;

	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusIsChanged(device) == 1 || hdmiDeviceStatusIsUnplugged(device) == 1)
		return _smDisconnect(device);
	_smClearPlugEvents();
	plg = hdmiDeviceStatusGetPlgStat(device);
	if (plg == 1)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
	if (plg == 0)
		return _smDisconnect(device);
	if (plg != 2)
		SCE_DBG_ASSERT_FAIL(614);
	if (hdmiDeviceStatusHasRequest(device) == 1) {
		if (hdmiDeviceStatusIsModeRequested(device) == 0)
			device->request = 1;
		return HDMI_SM_STATE_WAIT_PLL_LOCK;
	}
	hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_STOP | HDMI_SM_EVENT_VIDEO |
			      HDMI_SM_EVENT_PLUG | HDMI_SM_EVENT_UNPLUG);
	if (events & HDMI_SM_EVENT_STOP)
		return HDMI_SM_STATE_STOPPED;
	if (events & HDMI_SM_EVENT_UNPLUG)
		return _smDisconnect(device);
	if (events & HDMI_SM_EVENT_PLUG)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
	if ((events & HDMI_SM_EVENT_VIDEO) && hdmiDeviceStatusHasRequest(device) != 0)
		return HDMI_SM_STATE_WAIT_PLL_LOCK;
	return HDMI_SM_STATE_KEEP;
}

/* The checks of the states from the PLL lock on: -1 when they change nothing. */
static inline int _smCheckConnection(HdmiDevice *device)
{
	int plg;

	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusIsChanged(device) == 1 || hdmiDeviceStatusIsUnplugged(device) == 1)
		return _smDisconnect(device);
	_smClearPlugEvents();
	plg = hdmiDeviceStatusGetPlgStat(device);
	if (plg == 0)
		return _smDisconnect(device);
	if (plg == 1)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
	return HDMI_SM_STATE_KEEP;
}

/* Tell the upper layer the output runs. */
static inline void _smNotifyConnect(HdmiDevice *device)
{
	if (device->firstConnect) {
		if (callbacks[HDMI_SM_CALLBACK_CONNECT] != NULL)
			callbacks[HDMI_SM_CALLBACK_CONNECT]();
		device->firstConnect = 0;
	} else {
		if (callbacks[HDMI_SM_CALLBACK_RECONNECT] != NULL)
			callbacks[HDMI_SM_CALLBACK_RECONNECT]();
	}
}

/* Sony's name (debug records). */
static inline int _smWaitPllLock(HdmiDevice *device)
{
	int plg;

	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusIsChanged(device) == 1 || hdmiDeviceStatusIsUnplugged(device) == 1)
		return _smDisconnect(device);
	_smClearPlugEvents();
	plg = hdmiDeviceStatusGetPlgStat(device);
	if (plg == 1)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
	if (plg == 0)
		return _smDisconnect(device);
	if (plg != 2)
		SCE_DBG_ASSERT_FAIL(687);
	if (hdmiDeviceStatusIsResetRequested(device) == 1)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
	if (hdmiDeviceStatusGetPllStat(device) != 1)
		return HDMI_SM_STATE_KEEP;
	HdmiDeviceHwSetCypher(device, 0);
	hdmiDeviceSmSetPacketCtrl(device);
	HDMI_FLAG_PRINTF(HDMI_DBG_INFO, "[HDMI    ] Time Until TMDS On: %d\n", 200000);
	sceKernelDelayThread(200000);
	HdmiDeviceHwSetTMDSPower(device, 1);
	smWaitTime = 500000;
	if (edidManagerIsSonyD702())
		smWaitTime = 900000;
	sceKernelDelayThread(smWaitTime);
	if (hdmiDeviceIsHdcpOff() == 1) {
		HdmiDeviceHwSetAvMute(device, 0);
		hdmiDeviceSmAuthReset(device);
		_smNotifyConnect(device);
		return HDMI_SM_STATE_NO_HDCP;
	}
	hdmiDeviceSmClearEvent(HDMI_SM_EVENT_AUTH_ERR);
	hdmiDeviceSmAuthReset(device);
	return HDMI_SM_STATE_AUTH;
}

static inline int _smAuth(HdmiDevice *device)
{
	int state;

	state = _smCheckConnection(device);
	if (state != HDMI_SM_STATE_KEEP)
		return state;
	if (hdmiDeviceStatusIsResetRequested(device) != 0)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
	switch (hdmiDeviceSmAuthenticate(device)) {
	case HDMI_AUTH_FAILED:
		if (callbacks[HDMI_SM_CALLBACK_AUTH_FAILED] != NULL)
			callbacks[HDMI_SM_CALLBACK_AUTH_FAILED]();
		hdmiDeviceSmAuthReset(device);
		return HDMI_SM_STATE_KEEP;
	case HDMI_AUTH_DONE:
		_smNotifyConnect(device);
		return HDMI_SM_STATE_AUTHED;
	case HDMI_AUTH_SKIPPED:
		return HDMI_SM_STATE_AUTHED;
	default:
		return HDMI_SM_STATE_KEEP;
	}
}

static inline int _smAuthed(HdmiDevice *device)
{
	SceUInt32 events;
	int state;

	state = _smCheckConnection(device);
	if (state != HDMI_SM_STATE_KEEP)
		return state;
	if (hdmiDeviceStatusIsResetRequested(device) != 0)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
	if (hdmiDeviceStatusIsReady(device) == 1)
		return HDMI_SM_STATE_RUNNING;
	do {
		hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_NONE | HDMI_SM_EVENT_STOP |
				      HDMI_SM_EVENT_REAUTH | HDMI_SM_EVENT_VIDEO |
				      HDMI_SM_EVENT_PLUG | HDMI_SM_EVENT_UNPLUG);
		if (events & HDMI_SM_EVENT_STOP)
			return HDMI_SM_STATE_STOPPED;
		if (events & HDMI_SM_EVENT_UNPLUG)
			return _smDisconnect(device);
		if (events & HDMI_SM_EVENT_PLUG)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
		if (events & HDMI_SM_EVENT_VIDEO)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
		if (events & HDMI_SM_EVENT_REAUTH)
			return HDMI_SM_STATE_RUNNING;
	} while (events == HDMI_SM_EVENT_NONE);
	return HDMI_SM_STATE_KEEP;
}

static inline int _smNoHdcp(HdmiDevice *device)
{
	SceUInt32 events;

	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusIsChanged(device) == 1)
		return _smDisconnect(device);
	_smClearPlugEvents();
	switch (hdmiDeviceStatusGetPlgStat(device)) {
	case 0:
		return _smDisconnect(device);
	case 1:
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
	}
	if (hdmiDeviceStatusIsResetRequested(device) == 1)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
	if (hdmiDeviceStatusIsReady(device) == 1)
		return HDMI_SM_STATE_RUNNING_NO_HDCP;
	for (;;) {
		hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_NONE | HDMI_SM_EVENT_STOP |
				      HDMI_SM_EVENT_REAUTH | HDMI_SM_EVENT_VIDEO |
				      HDMI_SM_EVENT_PLUG | HDMI_SM_EVENT_UNPLUG);
		if (events & (HDMI_SM_EVENT_STOP | HDMI_SM_EVENT_UNPLUG))
			break;
		if (events & HDMI_SM_EVENT_PLUG) {
			hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_STOP | HDMI_SM_EVENT_POWER_ON |
					      HDMI_SM_EVENT_UNPLUG);
			if (events & HDMI_SM_EVENT_STOP)
				return HDMI_SM_STATE_STOPPED;
			if (events & HDMI_SM_EVENT_UNPLUG)
				return _smDisconnect(device);
			break;
		}
		if ((events & (HDMI_SM_EVENT_REAUTH | HDMI_SM_EVENT_VIDEO)) ||
		    events != HDMI_SM_EVENT_NONE)
			break;
	}
	return HDMI_SM_STATE_KEEP;
}

static inline int _smRunning(HdmiDevice *device)
{
	SceUInt32 events;

	if (hdmiDeviceStatusIsStopRequested(device) == 1)
		return HDMI_SM_STATE_STOPPED;
	if (hdmiDeviceStatusIsChanged(device) == 1)
		return _smDisconnect(device);
	_smClearPlugEvents();
	switch (hdmiDeviceStatusGetPlgStat(device)) {
	case 0:
		return _smDisconnect(device);
	case 1:
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
	}
	if (hdmiDeviceStatusIsResetRequested(device) == 1)
		return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
	if (hdmiDeviceStatusNeedsReauth(device) == 1)
		return HDMI_SM_STATE_AUTHED;
	do {
		hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_PACKET | HDMI_SM_EVENT_REAUTH2 |
				      HDMI_SM_EVENT_NONE | HDMI_SM_EVENT_STOP |
				      HDMI_SM_EVENT_AUTH_ERR | HDMI_SM_EVENT_POWER_ON |
				      HDMI_SM_EVENT_VIDEO | HDMI_SM_EVENT_PLUG |
				      HDMI_SM_EVENT_UNPLUG);
		if (events & HDMI_SM_EVENT_PACKET)
			hdmiDeviceSmSetPacketCtrl(device);
		if (events & HDMI_SM_EVENT_STOP)
			return HDMI_SM_STATE_STOPPED;
		if (events & HDMI_SM_EVENT_UNPLUG)
			return _smDisconnect(device);
		if (events & HDMI_SM_EVENT_PLUG)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
		if (events & HDMI_SM_EVENT_VIDEO)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
		if (events & HDMI_SM_EVENT_AUTH_ERR)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
		if (events & HDMI_SM_EVENT_REAUTH2)
			return HDMI_SM_STATE_AUTHED;
		if (events & HDMI_SM_EVENT_POWER_ON)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
	} while ((events & ~HDMI_SM_EVENT_PACKET) == HDMI_SM_EVENT_NONE ||
		 events == HDMI_SM_EVENT_PACKET);
	return HDMI_SM_STATE_KEEP;
}

static inline int _smRunningNoHdcp(HdmiDevice *device)
{
	SceUInt32 events;

	do {
		hdmiDeviceSmWaitEvent(&events, HDMI_SM_EVENT_PACKET | HDMI_SM_EVENT_REAUTH2 |
				      HDMI_SM_EVENT_STOP | HDMI_SM_EVENT_VIDEO |
				      HDMI_SM_EVENT_PLUG | HDMI_SM_EVENT_UNPLUG);
		if (events & HDMI_SM_EVENT_PACKET)
			hdmiDeviceSmSetPacketCtrl(device);
		if (events & HDMI_SM_EVENT_STOP)
			return HDMI_SM_STATE_STOPPED;
		if (events & HDMI_SM_EVENT_UNPLUG)
			return _smDisconnect(device);
		if (events & HDMI_SM_EVENT_PLUG)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_POWER_ON);
		if (events & HDMI_SM_EVENT_VIDEO)
			return _smStopOutput(device, HDMI_SM_STATE_WAIT_RESOLUTION);
	} while (!(events & HDMI_SM_EVENT_REAUTH2));
	return HDMI_SM_STATE_NO_HDCP;
}

/* The "SceHdmiDeviceSm" thread. */
static int smThread(SceSize args, void *argp)
{
	SceUInt32 timeout;
	int state;

	for (;;) {
#if FW_SINCE(0x04000000)
		if (stateProbeHook != NULL)
			stateProbeHook(&stateProbe, pDevice.state);
#endif
		switch (pDevice.state) {
		case HDMI_SM_STATE_STOPPED:
			state = _smStopped(&pDevice);
			break;
		case HDMI_SM_STATE_UNPLUGGED:
			state = _smWaitPlug(&pDevice);
			break;
		case HDMI_SM_STATE_PLUGGED:
			state = _smReadEdid(&pDevice);
			break;
		case HDMI_SM_STATE_WAIT_POWER_ON:
			state = _smWaitPowerOn(&pDevice);
			break;
		case HDMI_SM_STATE_WAIT_RESOLUTION:
			state = _smWaitResolutionSet(&pDevice);
			break;
		case HDMI_SM_STATE_WAIT_PLL_LOCK:
			state = _smWaitPllLock(&pDevice);
			break;
		case HDMI_SM_STATE_AUTH:
			state = _smAuth(&pDevice);
			break;
		case HDMI_SM_STATE_AUTHED:
			state = _smAuthed(&pDevice);
			break;
		case HDMI_SM_STATE_NO_HDCP:
			state = _smNoHdcp(&pDevice);
			break;
		case HDMI_SM_STATE_RUNNING:
			state = _smRunning(&pDevice);
			break;
		case HDMI_SM_STATE_RUNNING_NO_HDCP:
			state = _smRunningNoHdcp(&pDevice);
			break;
		case HDMI_SM_STATE_EXIT:
			state = HDMI_SM_STATE_EXIT;
			break;
		default:
			state = HDMI_SM_STATE_UNPLUGGED;
			break;
		}
		if (state == HDMI_SM_STATE_EXIT) {
			sceKernelExitThread(0);
			return 0;
		}
		if (state == HDMI_SM_STATE_KEEP)
			continue;
		timeout = 0xffffffff;
		ksceKernelLockMutex(serviceWork.mutex, 1, &timeout);
		pDevice.state = state;
		ksceKernelUnlockMutex(serviceWork.mutex, 1);
	}
}

/* Sony's name. */
int _sceHdmiDeviceRegisterEventCallback(SceUInt32 event, HdmiEventCallback callback)
{
	if (event >= HDMI_SM_CALLBACK_COUNT) {
		SCE_DBG_ASSERT_FAIL(1077);
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	if (callback == NULL) {
		SCE_DBG_ASSERT_FAIL(1082);
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	callbacks[event] = callback;
	return 0;
}

int _sceHdmiDeviceSmStartService(HdmiDevice *device)
{
	SceUInt32 timeout;
	int ret;

	serviceWork.mutex = 0;
	serviceWork.reserved[0] = 0;
	serviceWork.reserved[1] = 0;
	serviceWork.reserved[2] = 0;
	serviceWork.mutex = ksceKernelCreateMutex("SceHdmiDeviceSm", 0x2002, 0, NULL);
	timeout = 0xffffffff;
	ret = serviceWork.mutex < 0 ? SCE_HDMI_ERROR_DEVICE : 0;
	ksceKernelLockMutex(serviceWork.mutex, 1, &timeout);
	device->state = HDMI_SM_STATE_UNPLUGGED;
	ksceKernelUnlockMutex(serviceWork.mutex, 1);
	_smDisconnect(device);
	return ret;
}

#if FW_SINCE(0x04000000)
/* DTrace probe __hdmi__hdmi_state. */
static const SceDtraceProbe stateProbe = {
	.enabled = &stateProbeEnabled,
	.provider = "__hdmi__",
	.name = "hdmi_state",
	.address = smThread,		/* PLACEHOLDER: Sony's is the probe site */
	.hook = &stateProbeHook,
};

static const SceDtraceProbe *const smProbes[] __attribute__((used)) = { &stateProbe };
#endif

