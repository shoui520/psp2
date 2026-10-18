/*
 * SceHdmi device layer (Sony's device/hdmi_device.c): the transmitter's
 * interrupt thread and its handlers, module start and resume, and the
 * sceHdmiDevice* commands the upper layer calls ("[hdmiCmd ] ..."), with the
 * HdmiDeviceHw* layer that forwards them to the AD80244 driver.
 * Internal names are ours unless a comment says otherwise.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/lowio/gpio.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "../hdmi_internal.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>

/* GPIO 0 port 13: the transmitter's interrupt line, sub interrupt 13 of 0xf9 */
#define HDMI_INTR_CODE		0xf9
#define HDMI_INTR_SUBCODE	13
#define HDMI_INTR_GPIO_PORT	13

/* intrWork.eventFlag */
#define HDMI_INTR_EVENT_INTR	0x000001	/* the transmitter raised its interrupt */
#define HDMI_INTR_EVENT_EXIT	0x100000

/* hdmiDeviceEnableIntr/hdmiDeviceDisableIntr: which interrupts */
#define HDMI_INTR_MASK_HPD		1
#define HDMI_INTR_MASK_HPD2		2
#define HDMI_INTR_MASK_EDID		4
#define HDMI_INTR_MASK_EDID2		5
#define HDMI_INTR_MASK_AUTH		6
#define HDMI_INTR_MASK_CEC		0x80fe

/* edidMode, hdcpMode */
#define HDMI_MODE_DEFAULT	0xff
#define HDMI_EDID_MODE_ON	0x80
#define HDMI_HDCP_MODE_OFF	0x01

/* The interrupt thread's state (0x34 bytes). */
typedef struct HdmiIntrWork {
	SceUID eventFlag;			/* 0x00 */
	SceUID thread;				/* 0x04 */
	SceKernelEventFlagOptParam eventFlagOpt;	/* 0x08 */
	HdmiThreadOptParam threadOpt;		/* 0x0c */
	SceUID mutex;				/* 0x1c */
	SceUInt32 reserved;			/* 0x20 */
	int plugStatus;				/* 0x24: 0: unplugged, 1: plugged, 2: powered */
	int powered;				/* 0x28: the transmitter is powered up */
	HdmiIntrRegs mask;			/* 0x2c: the interrupt enables last written */
} HdmiIntrWork;

/* An interrupt handler and the status bits it handles. */
typedef struct HdmiIntrHandler {
	SceUInt32 mask;
	int (*handler)(const HdmiIntrRegs *regs);
} HdmiIntrHandler;

/* .bss: the buffers pDevice points to */
static SceUInt8 aksv[5];
static SceUInt8 ksvList[0x43];
static SceUInt8 edid[0x200];

/* .data */
SceUInt8 edidMode = HDMI_MODE_DEFAULT;
SceUInt8 hdcpMode = HDMI_HDCP_MODE_OFF;
int smWaitTime = 500000;		/* hdmi_device_sm.c's power on delay */
static SceUInt8 physicalAddress[2] = { 0xff, 0xff };
HdmiDevice pDevice = {
	.chipVersion = &hdmiChipVersion,
	.aksv = aksv,
	.ksvList = ksvList,
	.firstConnect = 1,
	.edid = edid,
	.physicalAddress = physicalAddress,
	.packetCtrl = 0x700e,
	.reserved4 = { 1, 1, 1 },
};

/* .bss */
static HdmiIntrWork intrWork;
static int hpdState;
static int rxSenseState;
int hdmiChipVersion;
#if FW_DEBUG
SceUInt32 hdmiDebugFlags;
#endif

static int intrCecFlag(const HdmiIntrRegs *regs);
static int intrBksvFlag(const HdmiIntrRegs *regs);
static int intrEdidReady(const HdmiIntrRegs *regs);
static int intrAuthed(const HdmiIntrRegs *regs);
static int intrHpdrsen(const HdmiIntrRegs *regs);
static int intrAutoriErr(const HdmiIntrRegs *regs);

static const HdmiIntrHandler intrHandlers[6] = {
	{ 0x800000, intrAutoriErr },
	{ 0x00c000, intrHpdrsen },
	{ 0x000200, intrAuthed },
	{ 0x000400, intrEdidReady },
	{ 0x400000, intrBksvFlag },
	{ 0x3f00ff, intrCecFlag },
};

/* "SceHdmiIntr": wakes the interrupt thread. */
static int intrHandler(void *subintrArg, void *registerArg, unsigned char priority)
{
	ksceKernelSetEventFlag(intrWork.eventFlag, HDMI_INTR_EVENT_INTR);
	return -2;
}

static int intrCecFlag(const HdmiIntrRegs *regs)
{
	HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "***  intr_cec_flag. ***\n");
	cecDevIntrHandler(regs->reg[HDMI_INTR_STATUS2] << 16 | regs->reg[HDMI_INTR_STATUS1] << 8 |
			  regs->reg[HDMI_INTR_CEC]);
	return 0;
}

static int intrBksvFlag(const HdmiIntrRegs *regs)
{
	HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "***  intr_bksv_flag. ***\n");
	hdmiDeviceDdcSetEvent(4);
	return 0;
}

static int intrEdidReady(const HdmiIntrRegs *regs)
{
	HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "***  intr_edid_ready. ***\n");
	hdmiDeviceDdcSetEvent(2);
	return 0;
}

static int intrAuthed(const HdmiIntrRegs *regs)
{
	HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "***  intr_authed. ***\n");
	hdmiDeviceDdcSetEvent(8);
	return 0;
}

static int intrHpdrsen(const HdmiIntrRegs *regs)
{
	SceUInt8 status;
	int last, hpd, rxSense, ret;

	HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "***  intr_hpdrsen. ***\n");
	last = intrWork.plugStatus;
	ret = HdmiDeviceAD80244ReadPlgStat(&status);
	if (ret != 0) {
		HDMI_ERR_PRINTF("[hdmiERR ] CheckPluging ERROR !: 0x%08X\n", ret);
	} else {
		intrWork.plugStatus = status & 0x40;
		if (status & 0x40) {
			if (status & 0x20)
				intrWork.plugStatus = 2;
			else
				intrWork.plugStatus = 1;
		}
		status &= 0x60;
		hpd = (status & 0x40) != 0;
		if (hpd != hpdState) {
			hpdState = hpd;
			if (hpd)
				cecDevNotifyHpd(1);
			else
				cecDevNotifyHpd(0);
		}
		rxSense = (status >> 5) & 1;
		if (rxSense != rxSenseState) {
			rxSenseState = rxSense;
			if (rxSense)
				cecDevNotifyRxSense(1);
			else
				cecDevNotifyRxSense(0);
		}
	}
	if (intrWork.plugStatus == last)
		return 0;
	if (intrWork.plugStatus == 1) {
		HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "%s Ch Pluged\n", "[HdmiINTR] ");
		hdmiDeviceSmSetEvent(2);
	} else {
		if (intrWork.plugStatus == 0) {
			HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "%s Ch UnPluged\n", "[HdmiINTR] ");
		} else if (intrWork.plugStatus == 2) {
			HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "%s Ch Power On\n", "[HdmiINTR] ");
			hdmiDeviceSmSetEvent(8);
			return 0;
		} else {
			HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", "%s Ch Unknown plg event\n",
				    "[HdmiINTR] ");
			HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ",
				    "%s Ch Set Un Plug Event to fix pDevice situation\n",
				    "[HdmiINTR] ");
		}
		hdmiDeviceSmSetEvent(1);
	}
	return 0;
}

static int intrAutoriErr(const HdmiIntrRegs *regs)
{
	SceUInt8 value = 0;

	sceHdmiDeviceI2cRead(1, 0x7a, 0xc8, &value, 1);
	hdmiDeviceSmSetEvent(0x20);
	hdmiDeviceDdcSetEvent(0x10);
	HdmiDeviceHwAuthStop(&pDevice);
	return 0;
}

/* Sony's name (debug records). */
static inline void lock(void)
{
	SceUInt32 timeout = 0xffffffff;

	if (ksceKernelLockMutex(intrWork.mutex, 1, &timeout) != 0)
		SCE_DBG_ASSERT_LEVEL_FUNC(0, 0, 152, "lock");
}

static inline void unlock(void)
{
	ksceKernelUnlockMutex(intrWork.mutex, 1);
}

/* "SceHdmiDeviceIntr": runs the handlers of the interrupts that are raised. */
static int intrThread(SceSize args, void *argp)
{
	SceUInt32 bits;
	HdmiIntrRegs regs;
	SceUInt32 raised;
	int i, ret;

	HDMI_ITRS_PRINTF("[hdmiITRS]  *** Interrupt Task Wake Up ***\n");
	do {
		for (;;) {
			ksceKernelWaitEventFlag(intrWork.eventFlag,
						HDMI_INTR_EVENT_INTR | HDMI_INTR_EVENT_EXIT,
						SCE_KERNEL_EVF_WAITMODE_OR, &bits, NULL);
			ksceKernelClearEventFlag(intrWork.eventFlag, ~bits);
			if (bits & HDMI_INTR_EVENT_EXIT) {
				sceKernelExitThread(0);
				return 0;
			}
			if (bits & HDMI_INTR_EVENT_INTR)
				break;
		}
		lock();
		regs = (HdmiIntrRegs){ 0 };
		ret = HdmiDeviceAD80244GetIntrStatus(&regs);
		if (ret != 0) {
			HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ",
				    "*** Int Reg Read Error(0x%08x). ***\n", ret);
		} else {
			regs.reg[HDMI_INTR_STATUS1] &= regs.reg[HDMI_INTR_ENABLE1];
			regs.reg[HDMI_INTR_STATUS2] =
				(regs.reg[HDMI_INTR_STATUS2] & regs.reg[HDMI_INTR_ENABLE2]) & ~0x40;
			ret = HdmiDeviceAD80244ClearIntrStatus(&regs);
			if (ret != 0) {
				HDMI_ERR_PRINTF("[hdmiERR ]  Ch *** Int Reg Write Error(0x%08x). ***\n",
						ret);
			} else {
				raised = (regs.reg[HDMI_INTR_ENABLE2] & regs.reg[HDMI_INTR_STATUS2]) << 16 |
					 regs.reg[HDMI_INTR_STATUS1] << 8 | regs.reg[HDMI_INTR_CEC];
				for (i = 0; i < 6; i++) {
					if (!(raised & intrHandlers[i].mask))
						continue;
					unlock();
					if (intrHandlers[i].handler != NULL)
						intrHandlers[i].handler(&regs);
					lock();
				}
			}
		}
		unlock();
	} while (ksceKernelEnableSubIntr(HDMI_INTR_CODE, HDMI_INTR_SUBCODE) >= 0);
	return 0;
}

int hdmiDeviceSetHdcpMode(SceUInt8 mode)
{
	hdcpMode = mode;
	return 0;
}

int hdmiDeviceSetEdidMode(SceUInt8 mode)
{
	edidMode = mode;
	return 0;
}

int hdmiDeviceIsHdcpOff(void)
{
	if (hdcpMode == HDMI_MODE_DEFAULT)
		return 0;
	return hdcpMode & 1;
}

int hdmiDeviceIsEdidOn(void)
{
	return edidMode != HDMI_MODE_DEFAULT && (edidMode & 0x80);
}

int hdmiDeviceIsEdidForced(void)
{
	if (edidMode == HDMI_MODE_DEFAULT)
		return 0;
	return (edidMode >> 6) & 1;
}

int hdmiDeviceIsEdidNotForced(void)
{
	if (edidMode == HDMI_MODE_DEFAULT)
		return 0;
	return ((edidMode ^ 0x40) >> 6) & 1;
}

/* Sony's name. */
int _sceHdmiDeviceModuleStart(void)
{
	SceUInt8 chipId[5];
	SceUInt8 length;
	int ret;

	if (HdmiDeviceAD80244SetCecAddress(&pDevice) < 0) {
		SCE_DBG_ASSERT_FAIL(223);
		return 2;
	}
	if (HdmiDeviceHwGetChipId(&pDevice, chipId, &length) < 0) {
		SCE_DBG_ASSERT_FAIL(230);
		return 2;
	}
	if (HdmiDeviceAD80244SetFixedRegisters(&pDevice) < 0) {
		SCE_DBG_ASSERT_FAIL(237);
		return 2;
	}
	intrWork = (HdmiIntrWork){ 0 };
	ret = ksceKernelCreateMutex("SceHdmiDeviceIntr", 0x2002, 0, NULL);
	if (ret < 0)
		goto error;
	intrWork.mutex = ret;
	intrWork.eventFlag = ksceKernelCreateEventFlag("SceHdmiDeviceIntr",
						       SCE_KERNEL_EVF_ATTR_MULTI, 0,
						       &intrWork.eventFlagOpt);
	intrWork.threadOpt.size = 8;
	ret = ksceKernelCreateThread("SceHdmiDeviceIntr", intrThread, 0x19, 0x2000, 0x10000000,
				     0x80000, (SceKernelThreadOptParam *)&intrWork.threadOpt);
	if (ret < 0)
		goto error;
	intrWork.thread = ret;
	if (ksceKernelStartThread(ret, 0, NULL) < 0)
		goto error;
	if (ksceGpioSetPortMode(0, HDMI_INTR_GPIO_PORT, 1) < 0)
		goto error;
	if (ksceGpioPortRead(0, HDMI_INTR_GPIO_PORT) < 0)
		goto error;
	if (ksceGpioSetIntrMode(0, HDMI_INTR_GPIO_PORT, 0) < 0)
		goto error;
	if (ksceKernelRegisterSubIntrHandler(HDMI_INTR_CODE, HDMI_INTR_SUBCODE, "SceHdmiIntr",
					     intrHandler, NULL) < 0)
		goto error;
	ret = cecInitialize();
	if (ret == SCE_HDMI_ERROR_CEC_UNSUPPORTED || ret >= 0) {
		if (ksceKernelEnableSubIntr(HDMI_INTR_CODE, HDMI_INTR_SUBCODE) < 0) {
			SCE_DBG_ASSERT_FAIL(261);
			return 2;
		}
		if (hdmiDeviceEdidInitialize(&pDevice) < 0) {
			SCE_DBG_ASSERT_FAIL(267);
			return 2;
		}
		if (hdmiDeviceDdcInitialize(&pDevice) < 0) {
			SCE_DBG_ASSERT_FAIL(273);
			return 2;
		}
		if (hdmiDeviceStatusInitialize(&pDevice) < 0) {
			SCE_DBG_ASSERT_FAIL(279);
			return 2;
		}
		if (hdmiDeviceSmInitialize() < 0) {
			SCE_DBG_ASSERT_FAIL(286);
			return 2;
		}
		if (_sceHdmiDeviceSmStartService(&pDevice) >= 0) {
			if (_sceHdmiDeviceSmRestart(&pDevice) >= 0)
				return 0;
			SCE_DBG_ASSERT_FAIL(299);
		}
	}
	return 2;

error:
	SCE_DBG_ASSERT_FAIL(244);
	return 2;
}

int _sceHdmiDeviceSuspend(void)
{
	pDevice.request = 0;
	if (hdmiDeviceDdcAbort() >= 0 && _sceHdmiDeviceSmStopService(&pDevice, 0) >= 0)
		return cecDevSuspend();
	return 0;
}

int _sceHdmiDeviceSuspendDone(void)
{
	if (pDevice.state == 0)
		return cecDevSuspendDone();
	return -1;
}

/* Sony's name. */
int _sceHdmiDeviceResume(void)
{
	int ret;

	intrWork.plugStatus = 0;
	ret = HdmiDeviceAD80244SetCecAddress(&pDevice);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(534);
		return ret;
	}
	ret = HdmiDeviceHwReset(&pDevice);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(541);
		return ret;
	}
	ret = ksceGpioSetPortMode(0, HDMI_INTR_GPIO_PORT, 1);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(549);
		return ret;
	}
	ret = ksceGpioSetIntrMode(0, HDMI_INTR_GPIO_PORT, 0);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(556);
		return ret;
	}
	ret = ksceKernelEnableSubIntr(HDMI_INTR_CODE, HDMI_INTR_SUBCODE);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(563);
		return ret;
	}
	ret = cecDevResume();
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(569);
		return ret;
	}
	ret = _sceHdmiDeviceSmRestart(&pDevice);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(576);
		return ret;
	}
	hdmiDeviceDdcClearAbort(&pDevice);
	return 0;
}

static inline void _setIntrMask(SceUInt8 *bits, SceUInt32 type)
{
	switch (type) {
	case HDMI_INTR_MASK_HPD:
		bits[HDMI_INTR_ENABLE1] = 0x06;
		bits[HDMI_INTR_ENABLE2] = 0x80;
		break;
	case HDMI_INTR_MASK_HPD2:
		bits[HDMI_INTR_ENABLE1] = 0x06;
		bits[HDMI_INTR_ENABLE2] = 0x80;
		break;
	case HDMI_INTR_MASK_EDID:
	case HDMI_INTR_MASK_EDID2:
		break;
	case HDMI_INTR_MASK_AUTH:
		bits[HDMI_INTR_ENABLE2] = 0x40;
		break;
	case HDMI_INTR_MASK_CEC:
		bits[HDMI_INTR_ENABLE2] = 0x3f;
		break;
	default:
		HDMI_PRINTF(HDMI_DBG_INTR, "[hdmiINTR] ", " Unknown Mask Ptn\n");
		break;
	}
}

int hdmiDeviceEnableIntr(SceUInt32 type)
{
	SceUInt8 bits[6];
	HdmiIntrRegs mask;
	int i, ret;

	bits[0] = 0;
	bits[1] = 0;
	bits[2] = 0;
	bits[3] = 0;
	bits[4] = 0;
	bits[5] = 0;
	_setIntrMask(bits, type);
	lock();
	ret = sceHdmiDeviceAD80244GetIntrMask(&mask);
	if (ret == 0) {
		for (i = 0; i < 6; i++)
			mask.reg[i] |= bits[i];
		ret = sceHdmiDeviceAD80244SetIntrMask(&mask);
		if (ret == 0)
			intrWork.mask = mask;
	}
	unlock();
	return ret;
}

int hdmiDeviceDisableIntr(SceUInt32 type)
{
	SceUInt8 bits[6];
	HdmiIntrRegs mask;
	int i, ret;

	bits[0] = 0;
	bits[1] = 0;
	bits[2] = 0;
	bits[3] = 0;
	bits[4] = 0;
	bits[5] = 0;
	_setIntrMask(bits, type);
	lock();
	ret = sceHdmiDeviceAD80244GetIntrMask(&mask);
	if (ret == 0) {
		for (i = 0; i < 6; i++)
			mask.reg[i] &= ~bits[i];
		ret = sceHdmiDeviceAD80244SetIntrMask(&mask);
		if (ret == 0)
			intrWork.mask = mask;
	}
	unlock();
	return ret;
}

int hdmiDeviceGetPowered(int *powered)
{
	if (powered == NULL)
		return -1;
	*powered = intrWork.powered;
	return 0;
}

int hdmiDeviceSetPowered(int powered)
{
	intrWork.powered = powered;
	return 0;
}

/* "[hdmiCmd ] ..." */
int sceHdmiDeviceSuspend(void)
{
	return _sceHdmiDeviceSuspend();
}

int sceHdmiDeviceSuspendDone(void)
{
	return _sceHdmiDeviceSuspendDone();
}

int sceHdmiDeviceResume(void)
{
	return _sceHdmiDeviceResume();
}

int sceHdmiDeviceGetChipId(SceUInt8 *id, SceUInt8 *length)
{
	int ret;

	if (id == NULL || length == NULL)
		return -1;
	ret = HdmiDeviceHwGetChipId(&pDevice, id, length);
	if (ret != 0)
		HDMI_ERR_PRINTF("[hdmiERR ] [HdmiDev] Get ChipId Failed. HDMI Tx Not Found:0x%08X\n",
				ret);
	HDMI_CMD_PRINTF("[hdmiCmd ] Get ChipId :0x[%02x %02x %02x %02x %02x]\n",
			id[0], id[1], id[2], id[3], id[4]);
	return ret;
}

int sceHdmiDeviceGetEdid(void *edid, int *blocks)
{
	int ret;

	if (edid == NULL || blocks == NULL)
		return -1;
	ret = 0;
	if (pDevice.edidBlockCount == 0) {
		HDMI_ERR_PRINTF("[hdmiERR ] [HdmiDev] EDID is empty. Please Check HDMI Status(HPD etc...).\n");
		ret = -1;
	} else {
		memcpy(edid, pDevice.edid, pDevice.edidBlockCount * 128);
		*blocks = pDevice.edidBlockCount;
	}
	HDMI_CMD_PRINTF("[hdmiCmd ] Get Edid :Block[%d]\n", pDevice.edidBlockCount);
	return ret;
}

int sceHdmiDeviceSetEdidMode(int on)
{
	HDMI_CMD_PRINTF("[hdmiCmd ] Set Edid Mode :[%s]\n", on ? "Edid On" : "Edid Pass");
	hdmiDeviceSetEdidMode(on == 1 ? HDMI_EDID_MODE_ON : HDMI_MODE_DEFAULT);
	hdmiDeviceSmLock();
	if (pDevice.request != 0)
		pDevice.request = 5;
	HdmiDeviceHwRestartEdid(&pDevice);
	hdmiDeviceSmSetEvent(1);
	hdmiDeviceSmUnlock();
	return 0;
}

int sceHdmiDeviceSetHdcpMode(int on)
{
	HDMI_CMD_PRINTF("[hdmiCmd ] Set HDCP Mode :[%s]\n", on ? "HDCP On" : "HDCP Off");
	hdmiDeviceSetHdcpMode(on == 1 ? HDMI_MODE_DEFAULT : HDMI_HDCP_MODE_OFF);
	hdmiDeviceSmLock();
	if (pDevice.request != 0)
		pDevice.request = 5;
	HdmiDeviceHwRestartEdid(&pDevice);
	hdmiDeviceSmSetEvent(1);
	hdmiDeviceSmUnlock();
	return 0;
}

int sceHdmiDeviceGetHdcpAksv(SceUInt8 *aksv, int fromChip)
{
	int ret;

	if (aksv == NULL)
		return -1;
	if (fromChip != 0) {
		ret = HdmiDeviceHwReadAksv(&pDevice, aksv);
		if (ret == 0 && aksv[0] == 0 && aksv[1] == 0 && aksv[2] == 0 && aksv[3] == 0)
			ret = aksv[4] != 0 ? 0 : -1;
	} else if (hdmiDeviceIsHdcpOff()) {
		HDMI_ERR_PRINTF("[hdmiERR ] [HdmiDev]Hdmi Device is HDCP Off Mode.\n");
		ret = -1;
	} else if (pDevice.state < 7) {
		HDMI_ERR_PRINTF("[hdmiERR ] [HdmiDev]Not finish HDCP Authentication or Not Ready Aksv.\n");
		ret = -1;
	} else if (!pDevice.aksvValid) {
		HDMI_ERR_PRINTF("[hdmiERR ] [HdmiDev]Not Ready Aksv.\n");
		ret = -1;
	} else {
		*(SceUInt32 *)aksv = *(const SceUInt32 *)pDevice.aksv;
		aksv[4] = pDevice.aksv[4];
		ret = 0;
	}
	HDMI_CMD_PRINTF("[hdmiCmd ] Get HDCP Aksv :0x[%02x %02x %02x %02x %02x]\n",
			aksv[0], aksv[1], aksv[2], aksv[3], aksv[4]);
	return ret;
}

int sceHdmiDeviceSetVideoConfig(const HdmiDeviceVideoConfig *config)
{
	int ret;

	hdmiDeviceSmLock();
	HDMI_CMD_PRINTF("[hdmiCmd ] Set Video Config :0x[%02x %02x %02x %02x %02x %02x]\n",
			config->vic, config->inputCsc, config->outputCsc, config->colorDepth,
			config->aspectRatio, config->scanInfo);
	ret = HdmiDeviceHwSetVideoConfig(&pDevice, config);
	pDevice.request = 1;
	hdmiDeviceSmSetEvent(4);
	hdmiDeviceSmUnlock();
	return ret;
}

int sceHdmiDeviceSetAudioConfig(const HdmiDeviceAudioConfig *config)
{
	int ret;

	hdmiDeviceSmLock();
	ret = HdmiDeviceHwSetAudioConfig(&pDevice, config);
	pDevice.audioConfigured = 1;
	hdmiDeviceSmUnlock();
	return ret;
}

int sceHdmiDeviceSetInfoFrame(const HdmiInfoFrame *frame)
{
	int ret;

	hdmiDeviceSmLock();
	ret = hdmiDeviceSmSetPacketData(&pDevice, frame->type, frame->packet, frame->size);
	hdmiDeviceSmUnlock();
	return ret;
}

int sceHdmiDeviceSetPacketCtrl(SceUInt32 ctrl)
{
	HDMI_CMD_PRINTF("[hdmiCmd ] Set Packet Ctrl:[0x%x]\n", ctrl);
	hdmiDeviceSmLock();
	pDevice.packetCtrl = ctrl;
	hdmiDeviceSmUnlock();
	return hdmiDeviceSmSetPacketCtrl(&pDevice);
}

int sceHdmiDeviceSetTmdsCtrl(int on)
{
	HDMI_CMD_PRINTF("[hdmiCmd ] Set TMDS Ctrl:[%s]\n", on ? "On" : "Off");
	if (on) {
		hdmiDeviceSmLock();
		pDevice.request = 1;
		hdmiDeviceSmSetEvent(4);
		hdmiDeviceSmUnlock();
	}
	return HdmiDeviceHwSetTMDSPower(&pDevice, on != 0);
}

int sceHdmiDeviceSetAvMute(int mute)
{
	HDMI_CMD_PRINTF("[hdmiCmd ] Set AvMute :[%s]\n", mute ? "On" : "Off");
	return HdmiDeviceHwSetAvMute(&pDevice, mute != 0);
}

int sceHdmiDeviceSetVideoMute(int mute)
{
	HDMI_CMD_PRINTF("[hdmiCmd ] Set VideoMute :[%s]\n", mute ? "On" : "Off");
	return HdmiDeviceHwSetVideoMute(&pDevice, mute != 0);
}

int sceHdmiDeviceSetAudioMute(int mute)
{
	int ret;

	HDMI_CMD_PRINTF("[hdmiCmd ] Set AudioMute :[%s]\n", mute ? "On" : "Off");
	hdmiDeviceSmLock();
	ret = HdmiDeviceHwSetAudioMute(&pDevice, mute);
	hdmiDeviceSmUnlock();
	return ret;
}

int sceHdmiDeviceRegisterEventCallback(SceUInt32 event, HdmiEventCallback callback)
{
	HDMI_CMD_PRINTF("[hdmiCmd ] Set RegisterEventCallback\n");
	return _sceHdmiDeviceRegisterEventCallback(event, callback);
}

int sceHdmiDeviceCheckStatus(void)
{
	return hdmiDeviceIsEdidOn();
}

/* HdmiDeviceHw*: the transmitter's operations (Sony's prefix) */
int HdmiDeviceHwReset(HdmiDevice *device)
{
	return HdmiDeviceAD80244Reset(device);
}

int HdmiDeviceHwGetChipId(HdmiDevice *device, SceUInt8 *id, SceUInt8 *length)
{
	return HdmiDeviceAD80244GetChipId(device, id, length);
}

int HdmiDeviceHwGetPlgStat(HdmiDevice *device)
{
	return sceHdmiDeviceAD80244GetPlgStat(device);
}

int HdmiDeviceHwGetPllStat(HdmiDevice *device)
{
	return sceHdmiDeviceAD80244GetPllStat(device);
}

/* Sony's name. */
int HdmiDeviceHwSetFrameDirect(HdmiDevice *device, SceUInt32 frame, const SceUInt8 *data,
			       SceUInt32 size)
{
	return HdmiDeviceAD80244SetFrameDirect(device, frame, data, size);
}

/* Sony's name. */
int HdmiDeviceHwSetFrame(HdmiDevice *device, SceUInt32 frames)
{
	return HdmiDeviceAD80244SetFrame(device, frames);
}

int HdmiDeviceHwSetPacketCtrl(HdmiDevice *device, SceUInt32 frames)
{
	return HdmiDeviceAD80244SetPacketCtrl(device, frames);
}

int HdmiDeviceHwSetAvMute(HdmiDevice *device, int mute)
{
	return HdmiDeviceAD80244SetAvMute(device, mute);
}

int HdmiDeviceHwSetVideoMute(HdmiDevice *device, int mute)
{
	return HdmiDeviceAD80244SetVideoMute(device, mute);
}

int HdmiDeviceHwSetAudioMute(HdmiDevice *device, int mute)
{
	return HdmiDeviceAD80244SetAudioMute(device, mute);
}

int HdmiDeviceHwSetHDMIMode(HdmiDevice *device, int hdmi)
{
	return HdmiDeviceAD80244SetHDMIMode(device, hdmi);
}

int HdmiDeviceHwCheckRepeater(HdmiDevice *device)
{
	return HdmiDeviceAD80244CheckRepeater(device);
}

int HdmiDeviceHwAuthStart(HdmiDevice *device)
{
	return HdmiDeviceAD80244AuthCtrl(device, 1);
}

int HdmiDeviceHwAuthStop(HdmiDevice *device)
{
	return HdmiDeviceAD80244AuthCtrl(device, 0);
}

/* Sony's name. */
int HdmiDeviceHwSetCypher(HdmiDevice *device, int on)
{
	return HdmiDeviceAD80244SetCypher(device, on);
}

int HdmiDeviceHwReadAksv(HdmiDevice *device, SceUInt8 *aksv)
{
	return HdmiDeviceAD80244ReadAksv(device, aksv);
}

int HdmiDeviceHwReadBksv(HdmiDevice *device, SceUInt8 *bksv)
{
	return HdmiDeviceAD80244ReadBksv(device, bksv);
}

/* Sony's name. */
int HdmiDeviceHwReadBcaps(HdmiDevice *device, SceUInt8 *bcaps)
{
	return HdmiDeviceAD80244ReadBcaps(device, bcaps);
}

/* Sony's name. */
int HdmiDeviceHwReadBstatus(HdmiDevice *device, SceUInt8 *bstatus)
{
	return HdmiDeviceAD80244ReadBstatus(device, bstatus);
}

int HdmiDeviceHwReadBksvCount(HdmiDevice *device, SceUInt8 *count)
{
	return HdmiDeviceAD80244ReadBksvCount(device, count);
}

/* Sony's name. */
int HdmiDeviceHwReadKSVListFromFIFO(HdmiDevice *device, SceUInt8 size, void *buffer)
{
	HdmiDdcRequest request;

	request.address = 0x74;
	request.offset = 0x43;
	request.size = size;
	request.mode = 0x21;
	request.buffer = buffer;
	return sceHdmiDeviceDdcRead(device, &request, 500000, 1);
}

int HdmiDeviceHwSetTMDSPower(HdmiDevice *device, int on)
{
	return HdmiDeviceAD80244SetTMDSPower(device, on);
}

int HdmiDeviceHwSetVideoConfig(HdmiDevice *device, const HdmiDeviceVideoConfig *config)
{
	HdmiVideoTiming timing;
	int vic = config->vic;

	switch (vic) {
	case 1: case 2: case 3: case 6: case 7: case 8: case 9: case 10: case 11:
	case 12: case 13: case 14: case 15: case 17: case 18: case 21: case 22:
	case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30:
	case 0x52:
		timing.syncPolarity = 3;
		break;
	case 4: case 5: case 16: case 19: case 20: case 31: case 32: case 33: case 34:
	case 0x50:
		timing.syncPolarity = 0;
		break;
	case 0x51:
		HDMI_ERR_PRINTF("[hdmiERR ] VIC:%d :WSVGA Polarity is not support yet!!!!!!!!\n",
				0x51);
		vic = config->vic;
		timing.syncPolarity = 0xff;
		break;
	case 0x53:
		timing.syncPolarity = 2;
		break;
	default:
		HDMI_ERR_PRINTF("[hdmiERR ] Unknown Resolution(VIC):%d\n", vic);
		vic = config->vic;
		timing.syncPolarity = 0xff;
		break;
	}
	switch (vic) {
	case 1: case 2: case 3: case 4: case 5: case 16: case 17: case 18: case 19:
	case 20: case 31: case 32: case 33: case 34:
		timing.pixelRepetition = 0;
		break;
	case 6: case 7: case 8: case 9: case 14: case 15: case 21: case 22: case 23:
	case 24: case 29: case 30:
		timing.pixelRepetition = 1;
		break;
	case 10: case 11: case 12: case 13: case 25: case 26: case 27: case 28:
		timing.pixelRepetition = 2;
		break;
	case 0x50: case 0x51: case 0x52: case 0x53:
		HDMI_ERR_PRINTF("[hdmiERR ] Vesa is not Support Now!!! PixelRep = %x\n", 0);
		timing.pixelRepetition = 0;
		break;
	default:
		HDMI_ERR_PRINTF("[hdmiERR ] Unknown Resolution(PixelRep):%d\n", vic);
		timing.pixelRepetition = 0xff;
		break;
	}
	return HdmiDeviceAD80244SetVideoConfig(device, config, &timing);
}

int HdmiDeviceHwSetAudioConfig(HdmiDevice *device, const HdmiDeviceAudioConfig *config)
{
	return HdmiDeviceAD80244SetAudioConfig(device, config);
}

int HdmiDeviceHwPowerOn(HdmiDevice *device)
{
	return HdmiDeviceAD80244PowerOn(device);
}

/* Stops the authentication and has the transmitter read the EDID again. */
int HdmiDeviceHwRestartEdid(HdmiDevice *device)
{
	int ret;

	ret = HdmiDeviceAD80244AuthCtrl(device, 0);
	if (ret >= 0) {
		hdmiDeviceDdcClearEvent(2);
		ret = HdmiDeviceAD80244EdidReread(device);
	}
	return ret;
}

int HdmiDeviceHwSetEdidSegment(HdmiDevice *device, SceUInt32 segment)
{
	return HdmiDeviceAD80244SetEdidSegment(device, segment);
}
