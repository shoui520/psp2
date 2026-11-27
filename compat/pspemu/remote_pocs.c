/*
 * remote_pocs.c: ScePspemu PocketStation host thread, Kermit channel 8
 * bridge, MCX.BIN AES-128-CBC/SHA-256 storage, and Adhoc matching peer
 * communication (0x81022730, 0x81028eb4..0x8102a37b).
 */
#include "pocs_internal.h"

#include <libaes.h>
#include <libsha256.h>
#include <stdio.h>
#include <string.h>

#define POCS_KERMIT_CHANNEL             8U
#define POCS_KERMIT_CMD_IS_ACTIVE       0x0F02
#define POCS_KERMIT_CMD_SPI_EXCHANGE    0x0F03
#define POCS_KERMIT_CMD_IS_ENABLED      0x0F04
#define POCS_KERMIT_CMD_MEMC_WRITE      0x0F05
#define POCS_KERMIT_CMD_IS_INSERTED     0x0F06

#define POCS_VMP_MAGIC                  0x564D5000U /* "\0PMV" */
#define POCS_CPU_RESET_PC               0x04000000U
#define POCS_FRAME_CYCLES               133389
#define POCS_BOOT_STEP_CYCLES           266513
#define POCS_ADHOC_IDLE_FRAMES          18000
#define POCS_ADHOC_POOL_SIZE            0x20000U
#define POCS_ADHOC_MAX_PEERS            16

#define POCS_ERR_IO                     ((int)0x80010005)
#define POCS_ERR_INVALID_ARG            ((int)0x80010016)

#define POCS_DIALOG_SDK_VERSION         0x03650000U
#define POCS_DIALOG_MAGIC_BASE          0xC0D1A109U

typedef struct PocsKermitRequest {
	uint32_t headerWords[4];
	uint32_t firstArg;
	uint32_t secondArg;
	uint32_t thirdArg;
} PocsKermitRequest;

typedef struct PocsMcxFileBuf {
	uint8_t vmpImage[POCS_VMP_IMAGE_SIZE];
	uint8_t sha256Digest[SCE_SHA256_DIGEST_SIZE];
} PocsMcxFileBuf;

_Static_assert(sizeof(PocsMcxFileBuf) == POCS_MCX_FILE_SIZE, "PocsMcxFileBuf size");

typedef struct PocsAesCbcWork {
	SceAesContext aesCtx;
	uint8_t       iv[32];
} PocsAesCbcWork;

typedef struct PocsNetCtlPeerRaw {
	uint32_t ipAddr;
	uint8_t  macAndMeta[52];
	char     nickname[20];
	uint8_t  tail[4];
} PocsNetCtlPeerRaw;

_Static_assert(sizeof(PocsNetCtlPeerRaw) == 80U, "PocsNetCtlPeerRaw size");

typedef struct PocsAdhocPeerInfo {
	uint32_t ipAddr;
	char     nickname[20];
} PocsAdhocPeerInfo;

_Static_assert(sizeof(PocsAdhocPeerInfo) == 24U, "PocsAdhocPeerInfo size");

typedef struct PocsNetCheckDialogParam {
	uint32_t sdkVersion;
	uint8_t  commonParamHeader[0x48];
	uint32_t commonMagic;
	int32_t  mode;
	char     npCommId[12];
	void    *ps3ConnectParam;
	void    *groupName;
	uint32_t timeoutUs;
	int8_t   defaultAgeRestriction;
	uint8_t  tail[115];
} PocsNetCheckDialogParam;

_Static_assert(offsetof(PocsNetCheckDialogParam, commonMagic) == 0x4CU, "commonMagic offset");
_Static_assert(offsetof(PocsNetCheckDialogParam, mode) == 0x50U, "mode offset");
_Static_assert(offsetof(PocsNetCheckDialogParam, npCommId) == 0x54U, "npCommId offset");
_Static_assert(offsetof(PocsNetCheckDialogParam, defaultAgeRestriction) == 0x6CU, "defaultAgeRestriction offset");
_Static_assert(sizeof(PocsNetCheckDialogParam) == 0xE0U, "PocsNetCheckDialogParam size");

typedef struct PocsNetCheckDialogResult {
	int32_t result;
	uint8_t tail[0x80];
} PocsNetCheckDialogResult;

_Static_assert(sizeof(PocsNetCheckDialogResult) == 0x84U, "PocsNetCheckDialogResult size");

typedef struct PocsAdhocSessionState {
	uint32_t ipAddr;
	uint8_t  reservedMeta[52];
	char     nickname[16];
	char     nicknameTerm;
	uint8_t  tail[7];
} PocsAdhocSessionState;

_Static_assert(sizeof(PocsAdhocSessionState) == 0x50U, "PocsAdhocSessionState size");

/* PocketStation machine state shared with pocs_cpu, pocs_periph, pocs_rtc_audio, pocs_gxm */
PocsMachineState g_pocsState;

static const uint8_t g_pocsBootButtonSequence[4] = { 0x10U, 0x00U, 0x10U, 0x00U };

static const uint8_t g_pocsMcxAesKey[16] = {
	0x81U, 0xD9U, 0xCCU, 0xE9U, 0x71U, 0xA9U, 0x49U, 0x9BU,
	0x04U, 0xADU, 0xDCU, 0x48U, 0x30U, 0x7FU, 0x07U, 0x92U
};

static const uint8_t g_pocsMcxAesIv[16] = {
	0x13U, 0xC2U, 0xE7U, 0x69U, 0x4BU, 0xECU, 0x69U, 0x6DU,
	0x52U, 0xCFU, 0x00U, 0x09U, 0x2AU, 0xC1U, 0xF2U, 0x72U
};

static const char g_pocsJpTitlePrefixes[4][5] = {
	"SCPS", "SCZS", "SLPM", "SLPS"
};

static PocsMcxFileBuf g_pocsMcxSaveBuf;
static SceUID g_pocsSaveEventFlag;
static uint8_t g_pocsAdhocPool[POCS_ADHOC_POOL_SIZE];
static int (*g_pocsAdhocAcceptCallback)(void);
static int g_pocsAdhocConnected;
static PocsNetCtlPeerRaw g_pocsNetCtlPeers[POCS_ADHOC_MAX_PEERS];
static int g_pocsAdhocState;
static int g_pocsFirstBootDialogState;
static uint16_t g_pocsSerialWideStr[10];
static uint16_t g_pocsSerialFormatted;
static SceKernelLwMutexWork g_pocsMainMutex;
static uint8_t g_pocsMcxImage[POCS_VMP_IMAGE_SIZE];
static uint8_t g_pocsGxmFramebufferState[0x88];
static int g_pocsStandaloneConfirmState;
static char g_pocsLocalNickname[20];
static int g_pocsAdhocRequestPending;
static int g_pocsAdhocIdleCounter;
static int g_pocsAdhocErrorCode;
static char g_pocsMcxPath[0x104];
static PocsAdhocSessionState g_pocsPeerSession;
static int g_pocsPeerDisconnectNotify;
static int g_pocsAdhocMatchingId = -1;
static int g_pocsEmulationActive;

extern int swprintf(uint16_t *dst, size_t maxLen, const uint16_t *fmt, ...);

static int pocsSaveMcxFile(int skipDiskWrite);
static int pocsSignAndSaveMcx(int skipDiskWrite);
static SceUID pocsLoadMcxFile(void);
static int pocsResetAllHardware(void);

/* 0x81022730 */
void remotePocsKermitThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	for (;;) {
		PspemuKermitRequest *req = NULL;
		int cmd = remotefsWaitAndGetRequest(POCS_KERMIT_CHANNEL, &req);
		uint64_t reply;

		switch (cmd) {
		case POCS_KERMIT_CMD_IS_ACTIVE:
			reply = (uint64_t)(int64_t)remotePocsIsActive();
			break;
		case POCS_KERMIT_CMD_SPI_EXCHANGE: {
			int portMode = (int)req->args[0];
			uint8_t txByte = (uint8_t)req->args[1];
			if (portMode == 1)
				pocsSpiExchangeWrapper(0, 0x81U);
			reply = (uint64_t)(uint32_t)pocsSpiExchangeWrapper(portMode, txByte);
			break;
		}
		case POCS_KERMIT_CMD_IS_ENABLED:
			reply = (uint64_t)(int64_t)pspemuIsPocketStationEnabled();
			break;
		case POCS_KERMIT_CMD_MEMC_WRITE:
			pocsHandleGuestMemcWrite((uint32_t)req->args[0]);
			reply = 0ULL;
			break;
		case POCS_KERMIT_CMD_IS_INSERTED:
			reply = (uint64_t)(int64_t)remotePocsHasMountedMedia();
			break;
		default:
			pspemuRecordTitleListFlags(16);
			reply = 0xFFFFFFFFULL;
			break;
		}

		remotefsReturnValue(POCS_KERMIT_CHANNEL, req, (int64_t)reply);
	}
}

/* 0x81028eb4 */
int pocsCpuReset(void)
{
	uint32_t nibble;

	memset(&g_pocsState, 0, sizeof(g_pocsState));
	g_pocsState.schedHead.next = &g_pocsState.schedHead;
	g_pocsState.schedHead.prev = &g_pocsState.schedHead;

	for (nibble = 0U; nibble < 16U; nibble++) {
		uint32_t mask = 0U;
		if ((nibble & 1U) != 0U)
			mask |= 0x000000FFU;
		if ((nibble & 2U) != 0U)
			mask |= 0x0000FF00U;
		if ((nibble & 4U) != 0U)
			mask |= 0x00FF0000U;
		if ((nibble & 8U) != 0U)
			mask |= 0xFF000000U;
		g_pocsState.nibbleMaskTable[nibble] = mask;
	}

	g_pocsBiosRom[POCS_ROM_OFF_RESET_PATCH_A] = 0U;
	g_pocsBiosRom[POCS_ROM_OFF_RESET_PATCH_B] = 0U;
	g_pocsState.cpuPc = POCS_CPU_RESET_PC;
	return 0;
}

/* 0x81028f48 */
int pocsCpuRaiseUndef(void)
{
	return pocsCpuExceptionUndef();
}

/* 0x81028f50 */
PocsSchedEvent *pocsSchedAddEvent(PocsSchedEvent *event, int32_t delayCycles)
{
	int32_t remaining = g_pocsState.cpuCyclesLeft;
	int32_t targetCycle = pocsGetCurrentCycle() + delayCycles;
	PocsSchedEvent *cursor;

	for (cursor = &g_pocsState.schedHead; ; cursor = cursor->next) {
		PocsSchedEvent *nextEv = cursor->next;
		if (nextEv == &g_pocsState.schedHead || (nextEv->targetCycle - targetCycle) > 0)
			break;
	}

	event->next = cursor->next;
	event->prev = cursor;
	cursor->next->prev = event;
	cursor->next = event;

	if (cursor == &g_pocsState.schedHead && remaining > 0) {
		g_pocsState.schedTargetCycle = targetCycle;
		g_pocsState.cpuCyclesLeft = pocsUnscaleCpuCycles(delayCycles);
	}

	event->targetCycle = targetCycle;
	return event;
}

/* 0x81028fe0 */
PocsSchedEvent *pocsSchedRemoveEvent(PocsSchedEvent *event)
{
	PocsSchedEvent *prevEv = event->prev;
	PocsSchedEvent *nextEv = event->next;

	prevEv->next = nextEv;
	nextEv->prev = prevEv;

	if (prevEv == &g_pocsState.schedHead && g_pocsState.cpuCyclesLeft > 0) {
		int32_t nextTarget = nextEv->targetCycle;
		int32_t delta = nextTarget - g_pocsState.schedTargetCycle;
		g_pocsState.schedTargetCycle = nextTarget;
		g_pocsState.cpuCyclesLeft += pocsUnscaleCpuCycles(delta);
	}

	event->prev = NULL;
	return event;
}

/* 0x81029040 */
static int pocsSwapVmcBuffer(void)
{
	uint8_t *vmcBuf = (uint8_t *)popsHostGetVmcBuffer(0U);
	uint8_t sectorTmp[128];
	uint32_t offset;

	if (vmcBuf == NULL)
		return POCS_ERR_INVALID_ARG;

	sceCompatCache(PSPEMU_COMPAT_CACHE_INVALIDATE, vmcBuf, POCS_VMP_IMAGE_SIZE);
	for (offset = 0U; offset < POCS_VMP_IMAGE_SIZE; offset += sizeof(sectorTmp)) {
		memcpy(sectorTmp, &vmcBuf[offset], sizeof(sectorTmp));
		memcpy(&vmcBuf[offset], &g_pocsMcxImage[offset], sizeof(sectorTmp));
		memcpy(&g_pocsMcxImage[offset], sectorTmp, sizeof(sectorTmp));
	}
	sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, vmcBuf, POCS_VMP_IMAGE_SIZE);

	if (g_pocsState.vmcInserted == 1U)
		vmcBuf = g_pocsMcxImage;
	g_pocsState.vmcInserted ^= 1U;
	g_pocsState.flashDataPtr = vmcBuf + POCS_VMP_HEADER_SIZE;

	vmcLoadCardCache(0U);
	vmcLoadCardCache(3U);
	return 0;
}

/* 0x81029108 */
static int pocsAdhocClearPeer(void)
{
	int ret;

	g_pocsAdhocConnected = 0;
	g_pocsAdhocRequestPending = 0;
	ret = pocsIrdaCancelSession();
	g_pocsAdhocIdleCounter = 0;
	g_pocsPeerDisconnectNotify = 1;
	return ret;
}

/* 0x8102913c */
static void pocsAdhocMatchingCallback(int matchingId, int eventType, SceNetInAddr *peerAddr,
                                      SceSize optLen, void *optData)
{
	if (matchingId != g_pocsAdhocMatchingId)
		return;

	switch (eventType) {
	case 2:
		if (g_pocsAdhocAcceptCallback != NULL) {
			if (g_pocsAdhocAcceptCallback() != 0) {
				sceNetAdhocMatchingCancelTarget(g_pocsAdhocMatchingId, peerAddr);
			} else {
				g_pocsAdhocErrorCode = -6;
				g_pocsPeerSession.ipAddr = peerAddr->s_addr;
				if (optData != NULL) {
					strncpy(g_pocsPeerSession.nickname, (const char *)optData, 16U);
					g_pocsPeerSession.nicknameTerm = '\0';
				}
				g_pocsAdhocRequestPending = 1;
			}
		}
		break;
	case 3:
		if (g_pocsAdhocConnected != 0)
			pocsAdhocClearPeer();
		g_pocsAdhocState = 1;
		g_pocsAdhocErrorCode = 0;
		break;
	case 4:
	case 5:
		g_pocsAdhocState = 1;
		g_pocsPeerSession.ipAddr = 0U;
		g_pocsAdhocErrorCode = 0;
		break;
	case 6:
		g_pocsPeerSession.ipAddr = peerAddr->s_addr;
		if (optData != NULL) {
			strncpy(g_pocsPeerSession.nickname, (const char *)optData, 16U);
			g_pocsPeerSession.nicknameTerm = '\0';
		}
		break;
	case 7:
		g_pocsAdhocConnected = 1;
		g_pocsAdhocState = 0;
		pocsIrdaSignalConnected();
		pspemuSettingsAbortPocsDialog();
		g_pocsAdhocErrorCode = 0;
		break;
	case 8:
		if (g_pocsAdhocConnected != 0)
			pocsAdhocClearPeer();
		g_pocsAdhocState = -3;
		g_pocsAdhocErrorCode = 0;
		break;
	case 9:
		if (g_pocsAdhocConnected != 0)
			pocsAdhocClearPeer();
		g_pocsAdhocState = -1;
		g_pocsAdhocErrorCode = 0;
		break;
	case 10:
		break;
	case 11:
		if (pocsPokejimaIsActive() != 0) {
			pocsPokejimaReceivePacket(optLen, optData);
		} else {
			pocsIrdaReceivePacket(optLen, (const uint8_t *)optData);
			g_pocsAdhocIdleCounter = POCS_ADHOC_IDLE_FRAMES;
		}
		break;
	case 12:
		pocsIrdaSignalConnected();
		break;
	default:
		break;
	}
}

/* 0x81029318 */
static int pocsSaveMcxFile(int skipDiskWrite)
{
	PocsAesCbcWork aesWork;
	uint8_t xoredBlock[32];
	uint32_t blockBytes;
	uint32_t offset;
	SceUID fd;
	SceSSize written;
	int ret;

	sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
	memcpy(g_pocsState.flashDataPtr - POCS_VMP_HEADER_SIZE, g_pocsMcxSaveBuf.vmpImage,
	       POCS_VMP_HEADER_SIZE);
	sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK,
	               g_pocsState.flashDataPtr - POCS_VMP_HEADER_SIZE,
	               POCS_VMP_HEADER_SIZE);
	sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);

	if (skipDiskWrite != 0)
		return 0;

	/* Scramble first 12 bytes of header before computing SHA-256 and AES-CBC */
	{
		uint32_t prng = (sceKernelGetProcessTimeLow() ^ 0U) | 1U;
		uint32_t i;
		for (i = 0U; i < 12U; i++) {
			prng *= 253U;
			g_pocsMcxSaveBuf.vmpImage[i] = (uint8_t)(prng >> 16);
		}
	}

	sceSha256Digest(g_pocsMcxSaveBuf.vmpImage, POCS_VMP_IMAGE_SIZE, g_pocsMcxSaveBuf.sha256Digest);

	ret = sceAesScheduleKey(&aesWork.aesCtx, 128U, 128U, g_pocsMcxAesKey);
	if (ret < 0)
		return ret;

	blockBytes = aesWork.aesCtx.blockWords * 4U;
	memcpy(aesWork.iv, g_pocsMcxAesIv, blockBytes);

	for (offset = 0U; offset < POCS_MCX_FILE_SIZE; offset += 16U) {
		uint8_t *blk = &g_pocsMcxSaveBuf.vmpImage[offset];
		uint32_t i;
		for (i = 0U; i < blockBytes; i++)
			xoredBlock[i] = aesWork.iv[i] ^ blk[i];
		sceAesEncrypt(&aesWork.aesCtx, xoredBlock, blk);
		blockBytes = aesWork.aesCtx.blockWords * 4U;
		memcpy(aesWork.iv, blk, blockBytes);
	}

	pspemuPowerLock();
	fd = sceIoOpen(g_pocsMcxPath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606);
	if (fd < 0) {
		pspemuPowerUnlock();
		return fd;
	}

	written = sceIoWrite(fd, &g_pocsMcxSaveBuf, POCS_MCX_FILE_SIZE);
	sceIoClose(fd);
	sceKernelAtomicCompareAndSet8(&g_pocsState.saveCountdown, 0, -1);
	pspemuPowerUnlock();

	return (written == (SceSSize)POCS_MCX_FILE_SIZE) ? 0 : POCS_ERR_IO;
}

/* 0x810294c8 */
static int pocsSignAndSaveMcx(int skipDiskWrite)
{
	int ret;

	sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
	sceDmacMemcpy(g_pocsMcxSaveBuf.vmpImage, g_pocsState.flashDataPtr - POCS_VMP_HEADER_SIZE,
	              POCS_VMP_IMAGE_SIZE);
	sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);

	ret = vmpSignHeader(g_pocsMcxSaveBuf.vmpImage);
	if (ret >= 0)
		return pocsSaveMcxFile(skipDiskWrite);
	return ret;
}

/* 0x81029524 */
static void __attribute__((noreturn)) pocsMemcSaveThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	for (;;) {
		sceKernelWaitEventFlag(g_pocsSaveEventFlag, 1U, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
		                       NULL, NULL);
		pocsSignAndSaveMcx(0);
	}
}

/* 0x81029548 */
static SceUID pocsLoadMcxFile(void)
{
	PocsAesCbcWork aesWork;
	uint8_t decryptedBlock[32];
	SceUID fd;
	SceSSize bytesRead;
	int ret;

	fd = sceIoOpen(g_pocsMcxPath, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;

	bytesRead = sceIoRead(fd, &g_pocsMcxSaveBuf, POCS_MCX_FILE_SIZE);
	sceIoClose(fd);
	if (bytesRead != (SceSSize)POCS_MCX_FILE_SIZE)
		return POCS_ERR_IO;

	ret = sceAesScheduleKey(&aesWork.aesCtx, 128U, 128U, g_pocsMcxAesKey);
	if (ret < 0)
		return ret;

	{
		uint32_t blockBytes = aesWork.aesCtx.blockWords * 4U;
		uint32_t offset;
		memcpy(aesWork.iv, g_pocsMcxAesIv, blockBytes);

		for (offset = 0U; offset < POCS_MCX_FILE_SIZE; offset += 16U) {
			uint8_t *blk = &g_pocsMcxSaveBuf.vmpImage[offset];
			uint32_t i;
			sceAesDecrypt(&aesWork.aesCtx, blk, decryptedBlock);
			blockBytes = aesWork.aesCtx.blockWords * 4U;
			for (i = 0U; i < blockBytes; i++) {
				uint8_t cipherByte = blk[i];
				blk[i] = aesWork.iv[i] ^ decryptedBlock[i];
				aesWork.iv[i] = cipherByte;
			}
		}
	}

	sceSha256Digest(g_pocsMcxSaveBuf.vmpImage, POCS_VMP_IMAGE_SIZE, decryptedBlock);
	if (sceClibMemcmpConstTime(decryptedBlock, g_pocsMcxSaveBuf.sha256Digest,
	                           SCE_SHA256_DIGEST_SIZE) != 0)
		return POCS_ERR_IO;

	{
		uint32_t *hdrWords = (uint32_t *)g_pocsMcxSaveBuf.vmpImage;
		hdrWords[0] = POCS_VMP_MAGIC;
		hdrWords[1] = POCS_VMP_HEADER_SIZE;
		hdrWords[2] = 0U;
	}

	memcpy(g_pocsMcxImage, g_pocsMcxSaveBuf.vmpImage, POCS_VMP_IMAGE_SIZE);
	return 0;
}

/* 0x81029688 */
int pocsLockMainMutex(void)
{
	return sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
}

/* 0x8102969c */
int pocsUnlockMainMutex(void)
{
	return sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
}

/* 0x810296b0 */
int pocsSpiExchangeWrapper(int portMode, uint8_t txByte)
{
	return pocsSpiExchange(portMode, txByte);
}

/* 0x810296b8 */
static int pocsResetAllHardware(void)
{
	pocsCpuReset();
	pocsSysReset();
	pocsIntcReset();
	pocsTimerReset();
	pocsClockReset();
	pocsRtcReset();
	pocsIrdaReset();
	pocsLcdReset();
	pocsGpioReset();
	pocsAudioReset();
	pocsBatteryReset();
	return pocsSpiReset();
}

/* 0x810296ec */
uint32_t remotePocsStartCore(void)
{
	uint32_t pc = g_pocsState.cpuPc;
	uint8_t savedSuppress = g_pocsState.inputSuppress;
	int step;

	g_pocsState.inputSuppress = 0U;
	for (step = 0; step < 4; step++) {
		uint16_t btn = g_pocsBootButtonSequence[step];
		pc = pocsCpuRunCycles(pc, POCS_BOOT_STEP_CYCLES);
		g_pocsState.cpuPc = pc;
		g_pocsState.intcLatch = btn;
		g_pocsState.intcRawStatus = btn;
	}
	g_pocsState.inputSuppress = savedSuppress;
	g_pocsState.lcdDirtyFlags = 0U;
	return pc;
}

/* 0x81029744 */
int remotePocsWaitSaveIdle(void)
{
	int ret = 0;
	while (g_pocsState.saveCountdown >= 0)
		ret = sceDisplayWaitVblankStartCB();
	return ret;
}

/* 0x81029764 */
void remotePocsNotifyVmcDirty(void)
{
	uint32_t *biosStatus = (uint32_t *)&g_pocsState.internalRam[POCS_RAM_OFF_BIOS_STATUS];
	*biosStatus |= 0x10000000U;
}

/* 0x8102977c */
void pocsHandleGuestMemcWrite(uint32_t writeCmd)
{
	uint32_t sectorIndex = (writeCmd >> 8) & 0x3FFU;
	uint8_t sectorStatus = (uint8_t)writeCmd;

	g_pocsState.internalRam[POCS_RAM_OFF_MEMC_SECTOR] = sectorStatus;
	if (sectorIndex != 63U) {
		uint8_t *vmcBase = (uint8_t *)popsHostGetVmcBuffer(0U);
		uint32_t *biosStatus = (uint32_t *)&g_pocsState.internalRam[POCS_RAM_OFF_BIOS_STATUS];

		sceCompatCache(PSPEMU_COMPAT_CACHE_INVALIDATE,
		               vmcBase + (sectorIndex << 7) + POCS_VMP_HEADER_SIZE, 128U);
		*biosStatus |= 0x10000000U;
		if (sectorStatus == 0U)
			sceKernelAtomicSet8(&g_pocsState.saveCountdown, 60);
	}
}

/* 0x810297c8 */
int remotePocsConsumeDirtyFlag(void)
{
	uint32_t biosStatus;

	if (g_pocsState.vmcInserted == 0U)
		return 0;
	memcpy(&biosStatus, &g_pocsState.internalRam[POCS_RAM_OFF_BIOS_STATUS], sizeof(biosStatus));
	return ((biosStatus ^ 0x200U) & 0x200U) != 0U;
}

/* 0x810297e4 */
int remotePocsGetInsertedSlot(void)
{
	return (int)g_pocsState.internalRam[POCS_RAM_OFF_KERNEL_RUNNING];
}

/* 0x810297f4 */
int remotePocsSetEmulationActive(int active)
{
	sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
	if (g_pocsEmulationActive == 0 && active == 1)
		pocsSyncRtcFromHost();
	g_pocsEmulationActive = active;
	return sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
}

/* 0x81029830 */
void remotePocsNotifyDisplayReset(void)
{
	g_pocsFirstBootDialogState = 1;
}

/* 0x81029840 */
int remotePocsStopBackgroundTasks(void)
{
	if (sceNetCheckDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
		sceNetCheckDialogAbort();
	if (g_pocsFirstBootDialogState == 2)
		SceCommonDialog_A68A44EA();
	if (g_pocsStandaloneConfirmState == 2)
		SceCommonDialog_7030E4A6();
	if (scePhotoImportDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
		scePhotoImportDialogAbort();
	return 0;
}

/* 0x810298a4 */
void *pocsGetActiveFlashData(void)
{
	if (g_pocsState.vmcInserted != 0U)
		return (uint8_t *)popsHostGetVmcBuffer(0U) + POCS_VMP_HEADER_SIZE;
	return g_pocsMcxImage + POCS_VMP_HEADER_SIZE;
}

/* 0x810298c8 */
int remotePocsHasActiveSession(void)
{
	if (popsHostGetPocsAutoBoot() != 1)
		return 0;
	if (g_pocsState.standaloneActive != 0U)
		return 0;
	if (g_pocsState.vmcInserted == 1U)
		return (g_pocsState.gpioData & 0x10U) != 0U;
	return ((g_pocsState.gpioData ^ 0x10U) & 0x10U) != 0U;
}

/* 0x8102990c */
int pocsRenderCurrentDisplay(void)
{
	int channel = popsHostGetPocsDisplayChannel();
	int ret = pocsGxmRequestRender(0, channel, 0);
	if (ret >= 0)
		return pocsGxmWaitFinish();
	return ret;
}

/* 0x81029928 */
int remotePocsSetPeerRequestCallback(int (*callback)(void))
{
	g_pocsAdhocAcceptCallback = callback;
	return 0;
}

/* 0x81029938 */
int remotePocsGetAdhocError(int index)
{
	return (index != 0) ? -1 : g_pocsAdhocErrorCode;
}

/* 0x8102994c */
int remotePocsDisconnectAdhoc(int stopMatching)
{
	if (pspemuCompatHasFlag(0x80000000U) != 0 && g_pocsAdhocMatchingId >= 0) {
		if (g_pocsAdhocConnected != 0) {
			SceNetInAddr addr;
			addr.s_addr = g_pocsPeerSession.ipAddr;
			sceNetAdhocMatchingCancelTarget(g_pocsAdhocMatchingId, &addr);
			if (g_pocsAdhocConnected != 0)
				pocsAdhocClearPeer();
		}
		if (stopMatching != 0) {
			sceNetAdhocMatchingStop(g_pocsAdhocMatchingId);
			if (SceCommonDialog_500229F0() == 1)
				SceCommonDialog_63B08CA3();
		}
	}
	return 0;
}

/* 0x810299ac */
int remotePocsRunConnectDialog(int32_t *outResult)
{
	PocsNetCheckDialogParam param;
	PocsNetCheckDialogResult dlgResult;
	int compatGroup;
	int ret;

	g_pocsAdhocErrorCode = 0;
	g_pocsAdhocAcceptCallback = NULL;
	if (g_pocsAdhocConnected == 1) {
		*outResult = 0;
		return 0;
	}

	pspemuMenuSetImeDeadline(sceKernelGetProcessTimeWide());
	memset(&param, 0, sizeof(param));
	sceClibMemset(&param, 0, sizeof(param));
	param.sdkVersion = POCS_DIALOG_SDK_VERSION;
	param.commonMagic = POCS_DIALOG_MAGIC_BASE + (uint32_t)(uintptr_t)&param.commonParamHeader;
	param.defaultAgeRestriction = -1;
	param.mode = 1;

	compatGroup = pspemuCompatGetParam(29);
	if (compatGroup == -1 || ((uint32_t)(compatGroup >> 2) > 99999U)) {
		const char *titleId = pspemuCompatGetTitleId();
		memcpy(param.npCommId, titleId, 9U);
	} else {
		snprintf(param.npCommId, 10U, "%s%05d",
		         g_pocsJpTitlePrefixes[compatGroup & 3], compatGroup >> 2);
	}
	param.npCommId[9] = '\0';
	param.npCommId[10] = '\0';

	ret = sceNetCheckDialogInit(&param);
	if (ret < 0)
		return ret;

	for (;;) {
		int status = sceNetCheckDialogGetStatus();
		if (status == SCE_COMMON_DIALOG_STATUS_NONE)
			return 0;

		if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
			ret = sceNetCheckDialogGetResult(&dlgResult);
			if (ret < 0)
				return ret;
			ret = sceNetCheckDialogTerm();
			if (ret < 0)
				return ret;

			*outResult = dlgResult.result;
			if (dlgResult.result == 0) {
				memset(&g_pocsPeerSession, 0, sizeof(g_pocsPeerSession));
				return sceNetAdhocMatchingStart(g_pocsAdhocMatchingId, 0x10000100,
				                                0x4000, 0, 18, g_pocsLocalNickname);
			}
		}

		pspemuMenuPollDialog(1);
		sceDisplayWaitVblankStart();
	}
}

/* 0x81029b00 */
int remotePocsGetAdhocPeerList(void *outPeers, unsigned int *inOutCount)
{
	PocsAdhocPeerInfo *dstPeers = (PocsAdhocPeerInfo *)outPeers;
	int ret = sceNetCtlAdhocGetPeerList(inOutCount, NULL);

	if (ret < 0) {
		*inOutCount = 0U;
		return ret;
	}

	if ((int)*inOutCount > POCS_ADHOC_MAX_PEERS) {
		*inOutCount = POCS_ADHOC_MAX_PEERS;
	} else if ((int)*inOutCount <= 0) {
		*inOutCount = 0U;
		return 0;
	}

	ret = sceNetCtlAdhocGetPeerList(inOutCount, (SceNetCtlAdhocPeerInfo *)(void *)g_pocsNetCtlPeers);
	if (ret >= 0) {
		int i;
		for (i = 0; i < (int)*inOutCount; i++) {
			dstPeers[i].ipAddr = g_pocsNetCtlPeers[i].ipAddr;
			strncpy(dstPeers[i].nickname, g_pocsNetCtlPeers[i].nickname, 20U);
		}
		ret = g_pocsAdhocRequestPending;
		if (g_pocsAdhocRequestPending != 0) {
			g_pocsAdhocRequestPending = 0;
			return 3;
		}
	}
	return ret;
}

/* 0x81029b88 */
int remotePocsSelectAdhocTarget(uint32_t peerIp)
{
	SceNetInAddr addr;
	g_pocsPeerSession.ipAddr = peerIp;
	g_pocsAdhocState = 2;
	pocsIrdaSetRole(0);
	addr.s_addr = g_pocsPeerSession.ipAddr;
	return sceNetAdhocMatchingSelectTarget(g_pocsAdhocMatchingId, &addr, 18, g_pocsLocalNickname);
}

/* 0x81029bc4 */
int remotePocsAcceptAdhocTarget(void)
{
	SceNetInAddr addr;
	g_pocsAdhocState = 2;
	if (g_pocsPeerSession.ipAddr == 0U)
		return -6;
	pocsIrdaSetRole(1);
	addr.s_addr = g_pocsPeerSession.ipAddr;
	return sceNetAdhocMatchingSelectTarget(g_pocsAdhocMatchingId, &addr, 18, g_pocsLocalNickname);
}

/* 0x81029c08 */
int remotePocsCancelAdhocTarget(void)
{
	SceNetInAddr addr;
	g_pocsAdhocErrorCode = 0;
	if (g_pocsPeerSession.ipAddr == 0U)
		return -6;
	addr.s_addr = g_pocsPeerSession.ipAddr;
	return sceNetAdhocMatchingCancelTarget(g_pocsAdhocMatchingId, &addr);
}

/* 0x81029c38 */
static int pocsMainThread(SceSize args, void *argp)
{
	SceUID saveThid;

	(void)args;
	(void)argp;

	if (pocsGxmInit(g_pocsGxmFramebufferState) < 0 ||
	    vmcManagerRegisterSlot(3U, g_pocsMcxImage, POCS_VMP_IMAGE_SIZE) < 0)
		pspemuTriggerFatalError(0);

	g_pocsState.flashDataPtr = g_pocsMcxImage + POCS_VMP_HEADER_SIZE;
	if (pocsLoadMcxFile() < 0) {
		if (vmcManagerFormatSlot(3U) < 0)
			pspemuTriggerFatalError(0);
		pocsSignAndSaveMcx(1);
	}

	if (vmcManagerMountFile(3U, -1, g_pocsMcxPath) < 0)
		pspemuTriggerFatalError(0);

	pocsResetAllHardware();

	if (pspemuCompatHasFlag(0x80000000U) != 0) {
		if (sceNetAdhocMatchingInit(POCS_ADHOC_POOL_SIZE, g_pocsAdhocPool) < 0)
			pspemuTriggerFatalError(0);
		g_pocsAdhocMatchingId = sceNetAdhocMatchingCreate(
			SCE_NET_ADHOC_MATCHING_MODE_P2P, POCS_ADHOC_MAX_PEERS,
			0x4E20U, 0x4000, 1000000U, 5000000U, 10, 1000000U,
			pocsAdhocMatchingCallback);
		if (g_pocsAdhocMatchingId < 0 ||
		    sceAppUtilSystemParamGetString(3U, (SceChar8 *)g_pocsLocalNickname, 17U) < 0)
			pspemuTriggerFatalError(0);
	} else {
		sceNetTerm();
	}

	g_pocsState.saveCountdown = -1;
	g_pocsSaveEventFlag = sceKernelCreateEventFlag("ScePocketStationSaveRequest", 0, 0, NULL);
	if (g_pocsSaveEventFlag < 0)
		pspemuTriggerFatalError(0);

	saveThid = sceKernelCreateThread("ScePocketStationMemcSave",
	                                 (SceKernelThreadEntry)pocsMemcSaveThread,
	                                 127, 0x2000U, 0, 0x40000, NULL);
	if (saveThid < 0 || sceKernelStartThread(saveThid, 0, NULL) < 0)
		pspemuTriggerFatalError(0);

	for (;;) {
		if (pspemuIsPocketStationInstalled() != 0) {
			sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);

			if (sceKernelAtomicDecIfPositive8(&g_pocsState.saveCountdown) == 1)
				sceKernelSetEventFlag(g_pocsSaveEventFlag, 1U);

			if (g_pocsPeerDisconnectNotify != 0) {
				g_pocsPeerDisconnectNotify = 0;
				pspemuBgDialogNotifyPocsEvent((int)(uintptr_t)g_pocsPeerSession.nickname);
			}

			if (g_pocsFirstBootDialogState == 1) {
				if (g_pocsState.standaloneActive != 0U) {
					g_pocsFirstBootDialogState = 0;
				} else {
					g_pocsFirstBootDialogState = 2;
					g_pocsFirstBootDialogState = popsHostHandlePocsFirstBoot();
				}
			}

			if (g_pocsState.spiActive != 0 ||
			    (g_pocsEmulationActive != 0 && popsHostIsOverlayActive() == SCE_FALSE)) {
				if (g_pocsState.rtcNeedsSync != 0U) {
					pocsSyncRtcFromHost();
					g_pocsState.rtcNeedsSync = 0U;
				}

				if (g_pocsState.internalRam[POCS_RAM_OFF_KERNEL_RUNNING] == 0U) {
					uint8_t appState = g_pocsState.internalRam[POCS_RAM_OFF_APP_STATE];
					if (appState == 2U || appState == 3U) {
						int bgType = popsHostGetPocsBgType();
						int maxChannels = pocsGxmGetChannelCount(bgType);
						int curChannel;

						g_pocsState.internalRam[POCS_RAM_OFF_APP_STATE] = 1U;
						if (maxChannels > 2 && popsHostGetPocsDisplaySlot() <= 15)
							maxChannels = 2;
						curChannel = popsHostGetPocsDisplayChannel();
						popsHostSetPocsDisplayChannel((uint8_t)((curChannel + 1) % maxChannels));
					}
				}

				if (g_pocsStandaloneConfirmState != 0) {
					g_pocsStandaloneConfirmState = 2;
					g_pocsStandaloneConfirmState = pspemuConfirmPocketStationFirstBoot();
					sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
				} else {
					if (pocsInputPollAndUpdate(g_pocsGxmFramebufferState) != 0 &&
					    g_pocsState.standaloneActive != 0U &&
					    (g_pocsState.gpioData & 0x10U) == 0U) {
						int bgType = popsHostGetPocsBgType();
						int channel = popsHostGetPocsDisplayChannel();
						pocsGxmRequestRender(bgType, channel, 1);
					} else if (remotePocsHasActiveSession() != 0) {
						int channel = popsHostGetPocsDisplayChannel();
						pocsGxmRequestRender(3, channel, 1);
					}

					pocsIrdaBeginFrame(pocsGetCurrentCycle());
					g_pocsState.cpuPc = pocsCpuRunCycles(g_pocsState.cpuPc, POCS_FRAME_CYCLES);
					pocsAudioFlush();
					pocsIrdaEndFrame(pocsGetCurrentCycle());

					if (g_pocsAdhocIdleCounter > 0) {
						g_pocsAdhocIdleCounter--;
						if (g_pocsAdhocIdleCounter == 0) {
							remotePocsCancelAdhocTarget();
							if (g_pocsAdhocConnected != 0)
								pocsAdhocClearPeer();
							g_pocsAdhocState = 1;
							g_pocsAdhocErrorCode = 0;
						}
					}

					sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
				}
			} else {
				g_pocsState.rtcNeedsSync = 1U;
				sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
			}
		}

		sceDisplayWaitVblankStartCB();
	}
}

/* 0x8102a028 */
int remotePocsStopAdhocIfIdle(void)
{
	if (g_pocsAdhocConnected == 1)
		return 0;
	return sceNetAdhocMatchingStop(g_pocsAdhocMatchingId);
}

/* 0x8102a04c */
int remotePocsGetAdhocStatus(void)
{
	return g_pocsAdhocState;
}

/* 0x8102a058 */
int remotePocsGetPeerInfo(void *outPeer)
{
	PocsAdhocPeerInfo *dst = (PocsAdhocPeerInfo *)outPeer;
	dst->ipAddr = g_pocsPeerSession.ipAddr;
	strncpy(dst->nickname, g_pocsPeerSession.nickname, 20U);
	return 0;
}

/* 0x8102a074 */
int remotePocsGetConnectedPeerInfo(void *outPeer)
{
	PocsAdhocPeerInfo *dst = (PocsAdhocPeerInfo *)outPeer;
	if (g_pocsAdhocConnected == 0)
		return -5;
	dst->ipAddr = g_pocsPeerSession.ipAddr;
	strncpy(dst->nickname, g_pocsPeerSession.nickname, 20U);
	return 0;
}

/* 0x8102a0a0 */
int pocsAdhocSendData(int dataLen, void *data)
{
	int ret = pocsIrdaWaitSendReady();
	if (ret == 0) {
		SceNetInAddr addr;
		g_pocsAdhocIdleCounter = POCS_ADHOC_IDLE_FRAMES;
		addr.s_addr = g_pocsPeerSession.ipAddr;
		return sceNetAdhocMatchingSendData(g_pocsAdhocMatchingId, &addr, dataLen, data);
	}
	return ret;
}

/* 0x8102a0d8 */
int remotePocsMountSaveData(const char *titleId)
{
	SceUID ret;

	pocsPokejimaInit();
	snprintf(g_pocsMcxPath, 0x100U, "ms0:PSP/SAVEDATA/%s/MCX.BIN", titleId);

	ret = pocsSpiInit();
	if (ret < 0)
		return ret;

	ret = pocsIrdaInit();
	if (ret < 0)
		return ret;

	ret = pocsAudioInit();
	if (ret < 0)
		return ret;

	ret = sceKernelCreateLwMutex(&g_pocsMainMutex, "ScePocketStationMain", 0, 0, NULL);
	if (ret < 0)
		return ret;

	ret = sceKernelCreateThread("ScePocketStationMain", pocsMainThread,
	                            126, 0x2000U, 0, 0x40000, NULL);
	if (ret < 0)
		return ret;

	return sceKernelStartThread(ret, 0, NULL);
}

/* 0x8102a16c */
const char *remotePocsGetMcxPath(void)
{
	return g_pocsMcxPath;
}

/* 0x8102a178 */
int remotePocsFlushVmc(void)
{
	void *vmcBuf = popsHostGetVmcBuffer(0U);
	sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, vmcBuf, POCS_VMP_IMAGE_SIZE);
	sceKernelSetEventFlag(g_pocsSaveEventFlag, 1U);
	return 0;
}

/* 0x8102a1a4 */
int remotePocsMountMediaFromMenu(void)
{
	int ret;

	if (pspemuIsPocketStationInstalled() == 0)
		return -1;
	if (g_pocsState.vmcInserted != 0U)
		return 0;

	sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
	ret = pocsSwapVmcBuffer();
	if (ret < 0) {
		sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
		return ret;
	}
	g_pocsState.inputSuppress = 1U;
	sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
	return 0;
}

/* 0x8102a208 */
int remotePocsEnterMode(void)
{
	if (g_pocsState.standaloneActive == 0U) {
		remotePocsSetEmulationActive(1);
		sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
		g_pocsAdhocIdleCounter = 0;
		pocsSpiAbortPending();
		g_pocsStandaloneConfirmState = 1;
		g_pocsState.standaloneActive = 1U;
		g_pocsState.inputSuppress = 1U;
		sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
	}
	return 0;
}

/* 0x8102a264 */
int remotePocsExitMode(void)
{
	if (g_pocsState.standaloneActive == 1U) {
		sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
		pocsGxmWaitFinish();
		g_pocsState.inputSuppress = 1U;
		g_pocsState.standaloneActive = 0U;
		sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
	}
	return 0;
}

/* 0x8102a2ac */
int remotePocsUnmountMediaFromMenu(void)
{
	int ret;

	if (g_pocsState.vmcInserted != 1U)
		return 0;

	sceKernelLockLwMutex(&g_pocsMainMutex, 1, NULL);
	pocsSpiAbortPending();
	ret = pocsSwapVmcBuffer();
	if (ret < 0) {
		sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
		return ret;
	}
	g_pocsState.inputSuppress = 1U;
	sceKernelUnlockLwMutex(&g_pocsMainMutex, 1);
	return 0;
}

/* 0x8102a2fc */
int remotePocsHasMountedMedia(void)
{
	return (int)g_pocsState.vmcInserted;
}

/* 0x8102a30c */
int remotePocsIsActive(void)
{
	return (int)g_pocsState.standaloneActive;
}

/* 0x8102a31c */
int remotePocsIsSavePending(void)
{
	return (g_pocsState.saveCountdown >= 0) ? 1 : 0;
}

/* 0x8102a330 */
const uint16_t *remotePocsGetSerialWideStr(void)
{
	static const uint16_t fmt[] = { '%', 'c', '%', '0', '8', 'd', 0 };
	if (g_pocsSerialFormatted == 0U) {
		swprintf(g_pocsSerialWideStr, 10U, fmt,
		         (int)(g_pocsState.serialId >> 24),
		         (int)(g_pocsState.serialId & 0x00FFFFFFU));
		g_pocsSerialFormatted = 1U;
	}
	return g_pocsSerialWideStr;
}
