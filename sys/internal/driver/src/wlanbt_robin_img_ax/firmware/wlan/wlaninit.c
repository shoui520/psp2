/*
 * The WLAN part's start-up (0x1b896..0x1beb4): the hardware information, the
 * buffer pools, the stages the initialisation runs through (each registers
 * itself again so that the chain can run once more after a reset) and the
 * shutdown that stops the chain's work. The names are inferred from use.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/btrom.h"
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/ie.h"
#include "robin/join.h"
#include "robin/mac.h"
#include "robin/baslot.h"
#include "robin/macrate.h"
#include "robin/mactx.h"
#include "robin/mdev.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/rfbus.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/slot.h"
#include "robin/soc.h"
#include "robin/stream.h"
#include "robin/ccm.h"
#include "robin/defrag.h"
#include "robin/swcrypto.h"
#include "robin/subscribe.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/threadx.h"
#include "robin/timer.h"
#include "robin/txring.h"
#include "robin/txstats.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"
#include "robin/apps.h"
#include "robin/assocAp.h"
#include "robin/hostcmd.h"

/* The hardware's description the host reads (HostCmd_CMD_GET_HW_SPEC, RF_TX_POWER). */
struct WlanHwInfo {
	uint8_t reserved0[5];
	uint8_t version[2];		/* the hardware version, from the SoC */
	uint8_t reserved7[4];
	uint8_t txPowerLevel[2];	/* HostCmd_CMD_RF_TX_POWER's current level */
	uint8_t txPowerSteps[16];	/* the levels it can be set to, 16-bit each */
};
extern struct WlanHwInfo wlanHwInfo;							/* 0x040088ac */
extern uint8_t wlanDtcmState[0xef];							/* 0x040087ec: wlanHwInfo's block */
extern uint8_t *wlanAntennaProfile;							/* 0x040088ca, unaligned */
extern uint8_t wlanPrimaryEvents;							/* 0x04008828 */
extern uint8_t wlanSavedMgmtPriority;							/* 0x040088da */
extern uint8_t wlanAntennaProfileDefault[];						/* SQRAM 0xc0031b40 */

/* The state of the transmit notification the stages configure. */
extern uint32_t *macTxNotifyState;							/* 0x54bd4 */
extern uint8_t wlanBssLocalDescriptor[];						/* 0x040089ac */

/* The buffer pools (descriptor, then the configuration osa_BlockPoolCreate() reads). */
#define POOL(name, kind, size, count)								\
	struct OsaBufferPool name __attribute__((section(".dtcm.rw"))) = { { { kind, 0, 0 } } };	\
	static struct OsaPoolConfig name##Config __attribute__((section(".dtcm.rw"))) = { 0, size, count }

POOL(wlanBlockPool28, 1, 0x7f4, 28);							/* 0x04000db8 */
POOL(wlanPacketBufferPool, 1, 0x7ec, 4);						/* 0x04000dcc */
POOL(wlanMgmtFramePool, 1, 0x33c, 4);							/* 0x04000de0 */
POOL(wlanBlockPool7, 2, 0x1e0, 3);							/* 0x04000df4 */
POOL(wlanHciPool, 1, 0x7ec, 1);								/* 0x04000e10 */
POOL(hostCmdResponsePool, 1, 0x7ec, 1);						/* 0x04000e24 */
POOL(wlanTxFramePool, 3, 0x1dc, 7);							/* 0x04000e38 */
POOL(wlanMgmtTaskPool, 1, 0x7ec, 1);							/* 0x04000e5c */
POOL(wlanEncrPool, 1, 0x210, 4);							/* 0x04000e70 */
POOL(wlanReplyFramePool, 1, 0x7ec, 3);							/* 0x04000e84 */
POOL(wlanEventPool, 1, 0x3c, 10);							/* 0x04000eac */

/* Control blocks (DTCM) and memory of the pools. */
extern uint8_t wlanBlockPool28Control[], wlanBlockPool28Memory[];			/* 0x04004ea0, SQRAM 0xc001b600 */
extern uint8_t wlanPacketBufferPoolControl[], wlanPacketBufferPoolMemory[];		/* 0x04004f10, SQRAM 0xc000fb20 */
extern uint8_t wlanReplyFramePoolControl[], wlanReplyFramePoolMemory[];		/* 0x04004f7c, SQRAM 0xc002dfe0 */
extern uint8_t wlanMgmtTaskPoolControl[], wlanMgmtTaskPoolMemory[];			/* 0x04000f04, SQRAM 0xc002d7e4 */
extern uint8_t wlanMgmtFramePoolControl[], wlanMgmtFramePoolMemory[];			/* 0x04004f20, SQRAM 0xc0029520 */
extern uint8_t wlanBlockPool7Control[], wlanBlockPool7Memory[];			/* 0x04004f30, SQRAM 0xc002a220 */
extern uint8_t wlanEncrPoolControl[], wlanEncrPoolMemory[];				/* 0x04004f6c, 0x0005db94 */
extern uint8_t wlanHciPoolControl[], wlanHciPoolMemory[];				/* 0x04000efc, SQRAM 0xc002b72c */
extern uint8_t hostCmdResponsePoolControl[], hostCmdResponsePoolMemory[];		/* 0x04000f00, SQRAM 0xc002bf1c */
extern uint8_t wlanTxFramePoolControl[], wlanTxFramePoolMemory[];			/* 0x04004f4c, SQRAM 0xc002c720 */
extern uint8_t wlanEventPoolControl[], wlanEventPoolMemory[];				/* 0x04004fb8, SQRAM 0xc002f7b0 */

/* The state of the frames the management layer builds (0x04001070), its entry and the buffer. */

/* The ROM reads the pool the encryption routines take their buffers from. */
struct OsaBufferPool *ramHook_encrPoolConfig __attribute__((section(".itcm.rw")));	/* 0x54ef4 */

/* The post-initialisation stages, in the order bufferPoolsInit() runs them; ended by NULL. */
static int wlanInitStageHwInfo(void);
static int wlanInitStageRadio(void);
static int wlanInitStageDevice(void);
static int wlanTablesInit(void);
static int wlanTxNotifyIdle(void);
static int (*wlanPostInitStages[16])(void) __attribute__((section(".itcm.rw"))) = {
	wlanInitStageHwInfo,
	wlanInitStageRadio,
	wlanInitStageDevice,
	cbProcPostInitCreateTasks,
	cbProcPostInitStartTasks,
};

/* One stage of the chain each (0x54bf0, 0x54bf8, 0x54c00, and the idle hook after them). */
static struct WlanInitStage wlanHwInfoStage __attribute__((section(".itcm.rw")));
static struct WlanInitStage wlanRadioStage __attribute__((section(".itcm.rw")));
static struct WlanInitStage wlanTablesStage __attribute__((section(".itcm.rw")));
static struct OsaIdleHook wlanTxNotifyIdleHook __attribute__((section(".itcm.rw")));	/* 0x54be8 */
static uint32_t macTxNotifySetupDone __attribute__((section(".itcm.rw")));		/* 0x54bd0 */
static struct OsaSemaphore *macTxNotifySemaphore __attribute__((section(".itcm.rw")));	/* 0x54bd8 */
static struct SysTimerSlotNode *wlanSlotTimer;						/* SQRAM 0xc0011ae0 */
struct OsaEventFlags *wlanHostSleepFlags __attribute__((section(".itcm.rw")));	/* 0x54bdc */
static struct { uint32_t words[7]; } macTxNotifySemaphoreObject __attribute__((section(".itcm.bss")));
static struct { uint32_t words[8]; } wlanHostSleepFlagsObject __attribute__((section(".itcm.bss")));

/* Not converted yet; provisional names. */
void *macTxNotifyConfigurePacket(int enable);						/* 0x3258e */
void sysChipIdRead(void);								/* 0x26ab4 */
void clockHostApplyTarget(void);							/* 0x26b48 */
void macTxNotifyProcessTable(uint32_t descriptor, uint32_t first, uint32_t second);	/* 0x31450 */
void macTxNotifySetupPacket(void);							/* 0x32c16 */
void uapSqramVerifyEntry(void);								/* SQRAM 0xc0009b34 */
void hostCmd0074Reset(void);								/* 0x311d0 */
void hostCmd0074Arm(void);								/* 0x311ee */
void macTxNotifyFormatState(void);							/* 0x2ff78 */
void wlanPsReleaseHeader(void);								/* 0x3e280 */
void uapSqramClearTable(void);						/* SQRAM 0xc000a3f6 */
void cbProcConfigureFrame(void);							/* 0x1e9f2 */
void macMgmtAdjustHeader(void);								/* 0x3b93a */
void macTxNotifyParseState(void);							/* 0x2fbcc */
void macTxNotifyComputeFrame(void);							/* 0x3355c */
void wlanPsApplyDescriptor(void);							/* 0x3e9f0 */
void macMgmtResetFrame(void);								/* 0x3abf6 */
int uapSqramCheckState(void);							/* SQRAM 0xc00073a6 */
void macMgmtRecordEntry(void);								/* 0x3719a */
void macMgmtSyncTable(void);								/* 0x37944 */
int wpaSuppVerifyDescriptor(uint32_t index, uint32_t argument);			/* 0x2c1ba */
void uapSqramParseState(void);							/* SQRAM 0xc00074d4 */
void macTxNotifyFormatContext(void);							/* 0x309ac */
void wlanPsComputeFrame(void);								/* 0x3d510 */
void p2pDirectAcquireContext(void);							/* 0x40df6 */
int macTxNotifyPrepareState(void);							/* 0x2faec */
void uapSqramSelectEntry(void);								/* SQRAM 0xc0009b36 */
void macTxNotifyProcessState(void);							/* 0x2f94a */
void wlanPsTrackDescriptor(void);							/* 0x3eb14 */
void macTxNotifyApplyEntry(void);							/* 0x31156 */
void p2pSqramConfigureEntry(void);							/* SQRAM 0xc000e23a */
void uapSqramProcessContext(void);							/* SQRAM 0xc0008204 */
void p2pDirectCheckState(void);								/* 0x3f34c */
void p2pDirectResetTable(void);								/* 0x42188 */
void uapSqramTrackContext(void);							/* SQRAM 0xc0008754 */
void cbProcDispatchFrame(void);								/* 0x1eb4e */
void macTxNotifyAcquireContext(void);							/* 0x30c0e */
void wlanPsAcquireFrame(void);								/* 0x3d896 */
void wlanPsBuildFrame(void);								/* 0x3d4ea */
void macMgmtSyncEntry(void);								/* 0x36a1c */
void wlanPsApplyPacket(void);								/* 0x3d06c */
void macMgmtHandleHeader(void);								/* 0x3b120 */
void macMgmtValidateHeader(void);							/* 0x3b1e2 */
void wlanPsSyncDescriptor(void);							/* 0x3e9d4 */
void uapSqramConvertContext(void);							/* SQRAM 0xc00089b6 */
void uapSqramSelectContext(void);							/* SQRAM 0xc00089ee */
void uapSqramVerifyContext(void);							/* SQRAM 0xc00089e6 */
void wlanPsConvertDescriptor(void);							/* 0x3ed8a */
void macTxNotifyManagePacket(void *queue);						/* 0x32a6a */
int cbProcProcessBuffer(void);								/* 0x1d52e */
extern uint16_t wlanMgmtFrameSize;							/* 0x54b8e */

/* The idle hook of the transmit notification. */
static int wlanTxNotifyIdle(void)
{
	int result = macTxNotifyPrepareState();

	uapSqramSelectEntry();
	return result;
}

/* Clears the hardware's block and describes the hardware: version, TX power levels, antennas. */
static void wlanHwInfoInit(void)
{
	unsigned int version;
	unsigned int i;

	__aeabi_memclr(wlanDtcmState, sizeof(wlanDtcmState));
	wlanHwInfo.reserved7[0] = 3;
	__aeabi_uwrite4((int)wlanAntennaProfileDefault, &wlanAntennaProfile);
	wlanHwInfo.reserved7[2] = 8;
	wlanHwInfo.reserved7[3] = 0;
	wlanHwInfo.txPowerLevel[0] = 12;
	wlanHwInfo.txPowerLevel[1] = 0;
	for (i = 0; i < 8; i++) {
		wlanHwInfo.txPowerSteps[2 * i] = 6 + 2 * i;
		wlanHwInfo.txPowerSteps[2 * i + 1] = 0;
	}
	wlanPrimaryEvents |= 1;
	wlanProtection.flags |= 0x20;
	wlanMacSlotConfigure(1);
	version = REG16(0x80002018);
	wlanHwInfo.version[0] = version;
	wlanHwInfo.version[1] = version >> 8;
	wlanSavedMgmtPriority = 0xff;
}

static int wlanInitStageHwInfo(void)
{
	wlanHwInfoInit();
	sysChipIdRead();
	macTxNotifyState = macTxNotifyConfigurePacket(1);
	wlanHwInfoStage.run = wlanInitStageHwInfo;
	wlanInitStageRegister(&wlanHwInfoStage);
	return 0;
}

/* The radio: the calibration tables, the baseband and RF, the MAC's defaults and the interrupts. */
static int wlanInitStageRadio(void)
{
	uint32_t posture;

	wlanCalibrationTableUpdate(NULL, 0, 0, 0);
	clockHostApplyTarget();
	macTxNotifyProcessTable(__aeabi_uread4(wlanBssLocalDescriptor), macTxNotifyState[9],
				macTxNotifyState[13]);
	if (macTxNotifySetupDone == 0) {
		macTxNotifySetupDone = 1;
		macTxNotifySetupPacket();
	}
	bbpInit();
	rfInitStage(0);
	uapSqramVerifyEntry();
	hostCmd0074Reset();
	hostCmd0074Arm();
	MAC_REG(0x514) = 0x9e12a8bf;
	MAC_REG(0x544) |= 0x80;
	MAC_REG(0x544) |= 0x80000000;
	(void)MAC_REG(0x510);
	MAC_REG(0x510) = 0;
	MAC_REG(0x548) = 0;
	(void)MAC_REG(0x540);
	MAC_REG(0x540) = 0;
	posture = sysIrqDisable();
	wlanIrqUnregisterAll();
	wlanIrqRegisterAll();
	wlanIrqEnableHostLines();
	sysIrqRestore(posture);
	romLibScheduleState();
	wlanMacWrite474(4);
	wlanMacSetE80Bits();
	wlanRadioStage.run = wlanInitStageRadio;
	wlanInitStageRegister(&wlanRadioStage);
	return 0;
}

/* The tables and queues of the MAC and the management layer. */
static int wlanTablesInit(void)
{
	struct WlanTaskHandles handles = {
		&macMgmtEventFlags, &macMgmtSmeQueue, &cbProcSmeQueue, &timerCbQueue,
	};
	uint32_t actual;
	int result;

	wlanTablesStage.run = wlanTablesInit;
	wlanInitStageRegister(&wlanTablesStage);
	wlanHostEventQueueInit();
	macTxNotifyFormatState();
	wlanPsReleaseHeader();
	macMgmtUpdateContext(0, 6);
	macMgmtCheckContext();
	uapSqramClearTable();
	cbProcConfigureFrame();
	macMgmtAdjustHeader();
	wlanTaskHandlesInit(&handles);
	wlanMefConfigClear();
	macTxNotifyParseState();
	wlanSubscribeEventsClearAll();
	macTxNotifyComputeFrame();
	wlanPsApplyDescriptor();
	wlanMefDisable();
	wlanCryptoRingsInit(&wlanCryptoTxRing, &wlanCryptoRxRing);
	wlanCryptoRxRing.scratch = &wlanCryptoScratch;
	wlanCustomIeReset();
	macMgmtResetFrame();
	wlanApPsInit();
	result = uapSqramCheckState();
	osaEventFlagsWait(wlanHostSleepFlags, 1, 8, &actual, 0);
	return result;
}

/* The device: the timers and semaphores of the MAC, the idle hook of the transmit notification. */
static int wlanInitStageDevice(void)
{
	wlanHostSleepFlags = (struct OsaEventFlags *)&wlanHostSleepFlagsObject;
	osaEventFlagsInit(&wlanHostSleepFlags);
	wlanTablesInit();
	wlanSlotInit();
	macMgmtRecordEntry();
	wlanJoinInit();
	macMgmtSyncTable();
	wlanSlotTimer = sysTimerSlotAcquire(wpaSuppVerifyDescriptor, 0);
	if (wlanSlotTimer != NULL)
		sysTimerStart(wlanSlotTimer->index, 1, 1);
	macTxNotifySemaphore = (struct OsaSemaphore *)&macTxNotifySemaphoreObject;
	osaSemaphoreInit(&macTxNotifySemaphore, 1, 12);
	wlanInitializeDevice();
	uapSqramParseState();
	macTxNotifyFormatContext();
	wlanWindowPoolInit(wlanTxWindowIds);
	wlanPsComputeFrame();
	wlanTxNotifyIdleHook.function = wlanTxNotifyIdle;
	osaAddIdleHook(&wlanTxNotifyIdleHook);
	p2pDirectAcquireContext();
	return 0;
}

/*
 * The size of the frames the transmit pool carries (HostCmd_CMD_TX_BUF_SIZE): the pool is
 * emptied of them, deleted and made again with blocks of the size, and the transmit
 * quotas follow its block count.
 */
void wlanTxBufSizeSet(uint16_t *size)
{
	uint32_t request[2];
	unsigned int count = 0;
	unsigned int quota;

	switch (*size) {
	case 0x1000:
		*size = 3720;
		count = 14;
		break;
	case 0x2000:
		*size = 7440;
		count = 7;
		break;
	case 0x800:
		*size = 1860;
		count = 28;
		break;
	default:
		break;
	}
	wlanMgmtFrameSize = *size + 176;
	while (osa_BufferClone(&wlanBlockPool28) !=
		   osa_PoolQuery(&wlanBlockPool28, 8) + osa_PoolQuery(&wlanBlockPool28, 16) ||
	       osa_BufferClone(&wlanBlockPool7) !=
		   osa_PoolQuery(&wlanBlockPool7, 8) + osa_PoolQuery(&wlanBlockPool7, 16) ||
	       osa_BufferClone(&wlanReplyFramePool) !=
		   osa_PoolQuery(&wlanReplyFramePool, 8) + osa_PoolQuery(&wlanReplyFramePool, 16))
		osaTaskSleep(1);
	mdevIoctl(wlanSdioDev, 0x41, 0);
	macTxNotifyProcessState();
	osa_BlockPoolDelete(&wlanBlockPool28, osaDeletePoolEntry);
	wlanBlockPool28Config.blockSize = wlanMgmtFrameSize;
	wlanBlockPool28Config.blockCount = count;
	if (count == 28) {
		quota = 28;
	} else {
		quota = 1;
		if (count > 3)
			quota = count >> 1;
		quota = (uint8_t)quota;
	}
	wlanTxQuotaSet(0, 4, quota);
	osa_BlockPoolCreate(&wlanBlockPool28, &wlanBlockPool28Config, wlanBlockPool28Control,
			    wlanBlockPool28Memory);
	request[0] = 4;
	mdevIoctl(wlanSdioDev, 0x42, (uintptr_t)request);
}

/* Creates the buffer pools and runs the post-initialisation stages; 1 if one of them failed. */
int bufferPoolsInit(void)
{
	int failed = 0;
	unsigned int i;

	macTxNotifyManagePacket(wlanPacketBufferPoolMemory);
	wlanInitStageHead = NULL;
	wlanInitStageTail = NULL;
	osa_BlockPoolCreate(&wlanBlockPool28, &wlanBlockPool28Config, wlanBlockPool28Control,
			    wlanBlockPool28Memory);
	osa_BlockPoolCreate(&wlanPacketBufferPool, &wlanPacketBufferPoolConfig,
			    wlanPacketBufferPoolControl, wlanPacketBufferPoolMemory);
	osa_BlockPoolCreate(&wlanReplyFramePool, &wlanReplyFramePoolConfig, wlanReplyFramePoolControl,
			    wlanReplyFramePoolMemory);
	osa_BlockPoolCreate(&wlanMgmtTaskPool, &wlanMgmtTaskPoolConfig, wlanMgmtTaskPoolControl,
			    wlanMgmtTaskPoolMemory);
	osa_BlockPoolCreate(&wlanMgmtFramePool, &wlanMgmtFramePoolConfig, wlanMgmtFramePoolControl,
			    wlanMgmtFramePoolMemory);
	osa_BlockPoolCreate(&wlanBlockPool7, &wlanBlockPool7Config, wlanBlockPool7Control,
			    wlanBlockPool7Memory);
	osa_BlockPoolCreate(&wlanEncrPool, &wlanEncrPoolConfig, wlanEncrPoolControl,
			    wlanEncrPoolMemory);
	osa_BlockPoolCreate(&wlanHciPool, &wlanHciPoolConfig, wlanHciPoolControl, wlanHciPoolMemory);
	osa_BlockPoolCreate(&hostCmdResponsePool, &hostCmdResponsePoolConfig, hostCmdResponsePoolControl,
			    hostCmdResponsePoolMemory);
	osa_BlockPoolCreate(&wlanTxFramePool, &wlanTxFramePoolConfig, wlanTxFramePoolControl,
			    wlanTxFramePoolMemory);
	if (osa_BlockPoolCreate(&wlanEventPool, &wlanEventPoolConfig, wlanEventPoolControl,
				wlanEventPoolMemory) != 0)
		for (;;)
			;
	if (cbProcProcessBuffer() != 0)
		for (;;)
			;
	wlanDriverInit();
	for (i = 0; wlanPostInitStages[i] != NULL; i++) {
		if (wlanPostInitStages[i]() != 0)
			failed = 1;
	}
	return failed;
}

/*
 * The hooks the supplicant's functions call through, set to the AP, power
 * save and management routines (the SQRAM ones and the ROM's host event
 * send).
 */
struct WlanSessionOps {
	void (*uapConvert)(void);
	void (*uapSelect)(void);
	void (*uapVerify)(void);
	int (*hostEventSend)(struct WlanConnection *connection, uint16_t event);
	void (*mgmtPrepare)(uint8_t *band, uint32_t *extra);
	void (*psConvert)(void);
};

struct WlanSessionOps wlanSessionOps __attribute__((section(".itcm.bss")));	/* 0x54ea8 */

void wlanSessionOpsInit(void)
{
	wlanSessionOps.uapConvert = uapSqramConvertContext;
	wlanSessionOps.uapSelect = uapSqramSelectContext;
	wlanSessionOps.uapVerify = uapSqramVerifyContext;
	wlanSessionOps.hostEventSend = wlanHostEventSendPlain;
	wlanSessionOps.mgmtPrepare = macMgmtPrepareContext;
	wlanSessionOps.psConvert = wlanPsConvertDescriptor;
}

/* The WLAN part's own tables, hooks and callbacks. */
int wlanInit(void)
{
	ramHook_encrPoolConfig = &wlanEncrPool;
	wlanPsTrackDescriptor();
	macTxNotifyApplyEntry();
	wlanCcmSetup();
	wlanRomServicesInit();
	wlanBaTablesInit();
	p2pSqramConfigureEntry();
	uapSqramProcessContext();
	p2pDirectCheckState();
	p2pDirectResetTable();
	uapSqramTrackContext();
	wlanIeInit();
	wlanIeBufferInit();
	wlanMefInit();
	wlanTxRingOpsInit();
	cbProcDispatchFrame();
	macTxNotifyAcquireContext();
	wlanSessionOpsInit();
	wlanPsAcquireFrame();
	wlanPsBuildFrame();
	macMgmtSyncEntry();
	wlanTxHooksInit();
	wlanPsApplyPacket();
	AssocSrvAp_InstallHooks();
	macMgmtHandleHeader();
	macMgmtValidateHeader();
	wlanApInstallHooks();
	wlanInitializeReceiveErrorHandler();
	wlanPsSyncDescriptor();
	rfBusEnable();
	return 0;
}

/*
 * HostCmd_CMD 0x00a9: stops the SDIO side and the timers, runs the chain of stages (a stage that
 * returns 1 stops it) and restarts them; the SoC's pending control keeps its bits.
 */
int hostCmd00a9Shutdown(void)
{
	struct WlanInitStage *stage;
	int result = 0;
	uint32_t saved = REG32(0x80002060) & 0x30003000;

	REG32(0x80002060) &= ~0x10000000u;
	REG32(0x80002060) |= 0x20003000u;
	wlanDisconnectSdioDevice();
	osaPauseTimers();
	for (stage = wlanInitStageHead; stage != NULL; stage = stage->next) {
		if (stage->run() == 1) {
			result = 1;
			break;
		}
	}
	wlanTxStatsReset();
	wlanConnectSdioDevice();
	osaResumeTimers();
	osaTaskResume(macTxTask);
	osaTaskResume(macMgmtTask);
	osaTaskResume(macTxNotifyTask);
	REG32(0x80002060) = (REG32(0x80002060) & 0xcfffcfff) | saved;
	return result;
}

/* The end of the firmware's data, 16-byte aligned: where the heap may not reach. */
extern char wlanDataEnd;								/* 0x57e9a */

unsigned int robinQueryHeapLimit(void)
{
	return (uintptr_t)&wlanDataEnd & 0xfffffff0;
}
