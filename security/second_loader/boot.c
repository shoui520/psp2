/*
 * boot.c: second_loader main boot sequence, hardware state verification,
 * SNVS OS version anti-rollback check, banner printing, and Keyring lock down
 * (0x8003e0..0x801161).
 *
 * Function and variable names here are ours (descriptive), except where noted
 * from SKBL or prototype strings.
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80bc20..0x80bd1b)
 * ============================================================================ */

static const char s_bootFailedMsg[] = " b1: b004fa16\n";
static const char s_sceConfidentialBanner[] = "\nSCE CONFIDENTIAL\n";
static const char s_copyrightBanner[] = "Copyright(C) 2016 Sony Computer Entertainment Inc. ";
static const char s_allRightsReservedBanner[] = "All Rights Reserved.\n";
static const char s_sdkPrefix[] = "[SDK]: ";
static const char s_dotStr[] = ".";
static const char s_newlineStr[] = "\n";
static const char s_releaseSuffixStr[] = "R, ";
static const char s_commaSpaceStr[] = ", ";
static const char s_blPrefix[] = "[BL]: ";
static const char s_revisionPrefix[] = "revision=";
static const char s_slashStr[] = "/";
static const char s_datePrefix[] = "date=";
static const char s_buildDateStr[] = "2017/02/23 19:40:26";

static const KeyringLockRange s_bootKeyringLockRanges[8] = {
	{ 0x0000u, 0x007fu },
	{ 0x0140u, 0x017fu },
	{ 0x0200u, 0x0203u },
	{ 0x0206u, 0x020du },
	{ 0x0344u, 0x0353u },
	{ 0x0400u, 0x047fu },
	{ 0x0502u, 0x057fu },
	{ 0x0700u, 0x077fu },
};

/* ============================================================================
 * .data (0x80c684..0x80c69f) and .bss (0x80cd40..0x80cd53)
 * ============================================================================ */

/* SVN revision of Sony's security repository (matches security-revision in index.dat) */
uint32_t g_bootldrRevision = 6862u;
static uint32_t s_reservedData688[2] = { 0u, 0u };

uint32_t __stack_chk_guard = 0u;
int g_bootExitStatus = -1;
uint32_t g_isColdBoot = 0u;
static uint32_t s_bannerPrinted = 0u;

static uint32_t s_idStorageInitialized;
static uint32_t s_cachedBootldrRevision;
static uint32_t s_socRevisionCached;
static uint32_t s_socRevisionValue;
static uint32_t s_sd0PowerEnabled;

/* ============================================================================
 * Functions (0x8003e0..0x801161)
 * ============================================================================ */

/* 0x8003e0 */
void cmepSyncm(void)
{
	__asm__ volatile("syncm");
}

/* 0x8003e4 */
void earlyInitHook(void)
{
}

/* 0x8003e6 */
void noopHook(void)
{
}

/* 0x8003e8 */
void keyringReadSlot(unsigned int slot, uint32_t *dst)
{
	volatile uint32_t *src = &KEYRING_SLOTS[slot].words[0];

	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	dst[4] = src[4];
	dst[5] = src[5];
	dst[6] = src[6];
	dst[7] = src[7];
}

/* 0x800432 */
int secondLoaderMain(void)
{
	uint8_t keyCopyBuf[32];
	uint32_t dipsw[8];
	uint8_t qaFlags[16];
	BigmacContext bigmacCtx;
	uint32_t kblMinRequiredVer;
	uint32_t mbrTotalSectors;
	uint32_t wakeupFactor;
	uint32_t hwInfo;
	uint32_t baryonVer;
	uint32_t armClockStep;
	uint32_t cmepClockStep;
	int status = 0;
	uint32_t isWarmBoot;
	uint32_t warmBootKblRestored;
	int dramZeroStatus;
	uint32_t hwModel;
	volatile uint32_t *sbootKeyDst;
	unsigned int i;

	(void)s_reservedData688;
	sceGpioInit();
	sceGpioSetPortMode(0, 7, 0);
	sceGpioPortSet(0, 7);
	clearBootFlagsState();
	initBootModulesTable();
	initKeyringBaseSeeds();

	/* Initialize eMMC while polling sceSysconAsyncPollInit asynchronously */
	sdifEmmcSetAsyncCallback(sceSysconAsyncPollInit);
	status = initEmmcStorage();
	if (status != 0)
		goto finish_boot;
	sdifEmmcGetAsyncCallbackStatus(&status);
	if (status != 0)
		goto finish_boot;

	hwInfo = (uint32_t)sceSysconGetHardwareInfo();
	setDebugLogFlags(hwInfo);
	hwModel = hwInfo & 0x00ff0000u;
	if (hwModel == 0x00100000u || hwModel == 0x00310000u ||
	    hwModel == 0x00410000u || hwModel == 0x00510000u ||
	    hwModel == 0x00600000u || hwModel == 0x00700000u ||
	    hwModel == 0x00720000u || hwModel == 0x00800000u ||
	    hwModel == 0x00820000u || hwModel == 0x00900000u) {
		enableDevkitFeatureFlag();
		enableGpioDebugLeds();
	}

	setGpioDebugLedPostcode('A');
	baryonVer = (uint32_t)sceSysconGetBaryonVersion();
	if (baryonVer <= 0x000802ffu) {
		status = (int)0x800f0037;
		debugLogError(2, 'B', (uint32_t)status, 0);
		goto finish_boot;
	}
	writeKeyringSlotBytes(0x508u, &baryonVer, sizeof(baryonVer));

	hwInfo = (uint32_t)sceSysconGetHardwareInfo();
	writeKeyringSlotBytes(0x51bu, &hwInfo, sizeof(hwInfo));
	if ((hwInfo & 0x00ff0000u) <= 0x007fffffu)
		*(volatile uint32_t *)0xe3100180u = 0x10u;
	else
		*(volatile uint32_t *)0xe3100180u = 0x1030u;

	setGpioDebugLedPostcode('C');
	wakeupFactor = 0;
	status = sceSysconGetWakeupFactor(&wakeupFactor);
	if (status != 0) {
		debugLogError(2, 'C', (uint32_t)status, 0);
		goto finish_boot;
	}

	setGpioDebugLedPostcode('D');
	status = verifySoCHardwareState(*(volatile uint32_t *)0xe0020004u,
			*(volatile uint32_t *)0xe0010004u,
			*(volatile uint32_t *)0xe0062020u);
	if (status != 0) {
		debugLogError(2, 'D', (uint32_t)status, 0);
		goto finish_boot;
	}

	setGpioDebugLedPostcode('E');
	isWarmBoot = (wakeupFactor >> 7) & 1u;
	status = ddrcInitLpddr2(1, (int)isWarmBoot);
	if (status != 0) {
		uint32_t ch0Err = ddrcGetChannel0Error() & 0xffu;
		uint32_t ch1Err = ddrcGetChannel1Error() & 0xffu;

		debugLogError(2, 'E', (uint32_t)status, (ch0Err << 8) | ch1Err);
		goto finish_boot;
	}

	if (isWarmBoot != 0) {
		if (isWarmBootMagicValid()) {
			if (restoreWarmBootKblParam() == 0) {
				isWarmBoot = 1u;
				warmBootKblRestored = 1u;
				goto after_kbl_init;
			}
			isWarmBoot = 1u;
		} else {
			isWarmBoot = 0u;
		}
	}
	warmBootKblRestored = 0u;
	clearKblParam();

after_kbl_init:
	if ((*(volatile uint32_t *)0xe0070004u & 1u) != 0) {
		setGpioDebugLedPostcode('R');
		status = (int)0x800f0029;
		debugLogError(2, 'R', (uint32_t)status, 0);
		goto finish_boot;
	}

	blockdevSetSectorBuffer((void *)0x40000500u, 0x1000u);
	setGpioDebugLedPostcode('T');
	mbrTotalSectors = status;
	status = readEmmcMbrTotalSectors(&mbrTotalSectors);
	if (status != 0) {
		debugLogError(2, 'T', (uint32_t)status, 0);
		goto finish_boot;
	}
	if (mbrTotalSectors >= 0x04000001u) {
		status = (int)0x800f0030;
		debugLogError(2, 'T', (uint32_t)status, mbrTotalSectors);
		goto finish_boot;
	}
	writeKeyringSlotBytes(0x50fu, &mbrTotalSectors, sizeof(mbrTotalSectors));

	setGpioDebugLedPostcode('U');
	status = runErnieHandshake0BAndLoadIds();
	if (status != 0)
		goto finish_boot;

	setGpioDebugLedPostcode('_');
	status = pervasiveScaleBusVoltage(6u);
	if (status != 0)
		debugLogError(1, '_', (uint32_t)status, 0);
	pervasiveSetBusClock(6u);
	pervasiveEnableSysClock();

	dramZeroStatus = (int)0x800f0026;
	if (isWarmBoot == 0) {
		bigmacInitContext(&bigmacCtx);
		dramZeroStatus = bigmacMemsetAsync((void *)0x40000000u, 0x00200000u, 0, &bigmacCtx);
	}

	setGpioDebugLedPostcode('V');
	status = loadBootFlagsFromNvs();
	if (status != 0)
		debugLogError(1, 'V', (uint32_t)status, 0);
	if (warmBootKblRestored == 0)
		loadHardwareFlagsFromNvs();
	if (isWarmBoot == 0 && dramZeroStatus == 0)
		bigmacWaitChannel(&bigmacCtx);

	setGpioDebugLedPostcode('M');
	status = initSnvsAndKeyringState((isWarmBoot != 0) ? (baryonVer > 0x01000101u) : 0);
	if (status != 0) {
		initDebugUartAndClock();
		debugLogError(2, 'M', (uint32_t)status, 0);
		goto finish_boot;
	}

	setGpioDebugLedPostcode('Q');
	status = enableSd0Power();
	if (status != 0) {
		initDebugUartAndClock();
		debugLogError(2, 'Q', (uint32_t)status, 0);
		goto finish_boot;
	}

	slMemset(dipsw, 0, sizeof(dipsw));
	if (isDevkitFeatureEnabled()) {
		setGpioDebugLedPostcode('W');
		status = readDipswFromScratchpad(dipsw);
		if (status != 0)
			debugLogError(1, 'W', (uint32_t)status, 0);
		if (!checkDipswBit(dipsw, DIPSW_DEVELOPMENT_MODE) &&
		    !checkDipswBit(dipsw, DIPSW_RELEASE_CHECK_MODE)) {
			setDebugUartDisabled(1u);
		}
	}
	populateSysconKblFields();
	writeKeyringSlotBytes(0x510u, dipsw, sizeof(dipsw));

	slMemset(qaFlags, 0, sizeof(qaFlags));
	initQafSeedsInKeyring();
	if (!checkDipswBit(dipsw, 0xf0u)) {
		setGpioDebugLedPostcode('X');
		status = verifyQafToken(qaFlags);
		if (status != (int)0x800f0002 && status != (int)0x800f0025) {
			if (status != 0) {
				debugLogError(1, 'X', (uint32_t)status, 0);
			} else if (checkDipswBit(dipsw, 0xf1u)) {
				clearBitFlagInArray(qaFlags, 13, 1u);
				clearBitFlagInArray(qaFlags, 14, 1u);
			}
		}
	}
	writeKeyringSlotBytes(0x50au, qaFlags, sizeof(qaFlags));

	setGpioDebugLedPostcode('F');
	status = verifyStepFHook(qaFlags);
	if (status != 0)
		goto finish_boot;

	setGpioDebugLedPostcode('G');
	status = verifyStepGHook();
	if (status != 0) {
		initDebugUartAndClock();
		debugLogError(2, 'G', (uint32_t)status, 0);
		setGpioDebugLedPostcode('O');
		goto finish_boot;
	}

	setGpioDebugLedPostcode('I');
	kblMinRequiredVer = 0;
	if (isWarmBoot == 0 || (wakeupFactor & 0x7fu) == 0x17u) {
		status = loadKernelBootLoaderSelf(&kblMinRequiredVer);
		if (status != 0) {
			initDebugUartAndClock();
			debugLogError(2, 'I', (uint32_t)status, 0);
			goto finish_boot;
		}
	} else {
		copyWarmBootVector();
	}

	setGpioDebugLedPostcode('Y');
	initDebugUartAndClock();
	secondLoaderPrintBannerOnce();

	armClockStep = getTargetArmClockStep();
	cmepClockStep = getTargetCmepClockStep();
	g_isColdBoot = getTargetBootModeFlag();
	sceSysconStartAsyncBootCallback(asyncLoadBootModulesCallback);

	setGpioDebugLedPostcode('N');
	status = verifySnvsOsVersion(kblMinRequiredVer, isWarmBoot);
	if (status != 0) {
		debugLogError(2, 'N', (uint32_t)status, 0);
		goto finish_boot;
	}

	sceSysconWaitAsyncBootCallback(&status);
	if (status != 0) {
		debugLogError(2, 'J', (uint32_t)status, 0);
		goto finish_boot;
	}

	setGpioDebugLedPostcode('^');
	if (isWarmBoot == 0 && kblMinRequiredVer < mbrTotalSectors) {
		debugLogError(2, '^', mbrTotalSectors, kblMinRequiredVer);
		status = (int)0x800f0030;
		goto finish_boot;
	}

	setGpioDebugLedPostcode('Z');
	populateKblParam(dipsw, qaFlags, wakeupFactor, (int)isWarmBoot, mbrTotalSectors);

	setGpioDebugLedPostcode('`');
	status = pervasiveScaleArmVoltage(armClockStep);
	if (status != 0)
		debugLogError(1, 'L', (uint32_t)status, 0);

	setGpioDebugLedPostcode('L');
	status = ddrcPostInitConfigure(armClockStep, g_isColdBoot, 0);
	if (status != 0) {
		debugLogError(2, 'L', (uint32_t)status, 0);
		goto finish_boot;
	}

	setGpioDebugLedPostcode('P');
	sbootKeyDst = (volatile uint32_t *)(uintptr_t)CMEP_SBOOT_KEY_COPY_PA;
	keyringReadSlot(0x602u, (uint32_t *)keyCopyBuf);
	for (i = 0; i < 8; i++)
		sbootKeyDst[i] = ((const uint32_t *)keyCopyBuf)[i];
	keyringReadSlot(0x601u, (uint32_t *)keyCopyBuf);
	for (i = 0; i < 8; i++)
		sbootKeyDst[8 + i] = ((const uint32_t *)keyCopyBuf)[i];
	clearCmepSboot();

	if (cmepClockStep >= 7u) {
		pervasiveScaleBusVoltage(cmepClockStep);
		pervasiveSetBusClock(cmepClockStep);
	} else if (cmepClockStep != 6u) {
		pervasiveSetBusClock(cmepClockStep);
		pervasiveScaleBusVoltage(cmepClockStep);
	}
	status = 0;

finish_boot:
	lockBootKeyringRanges();
	if (status == 0) {
		setArmResetHold(1);
		cmepSyncm();
		setGpioDebugLedPostcode('@');
	} else {
		setArmResetHold(0);
		secondLoaderPrintBannerOnce();
		debugSetNumberBase(NULL, 16u);
		debugPrintNumber(2u);
		debugPrintString(s_bootFailedMsg);
		cmepDelayUs(BIGMAC->ch[0].rng & 0xffffu);
		if (!isGpioDebugLedsEnabled() && isSysconInitialized())
			sceSysconSetPowerMode(0, 0);
	}
	return status;
}

/* 0x800a5e */
void secondLoaderPrintBannerOnce(void)
{
	if (!s_bannerPrinted) {
		printSdkBanner();
		printBlRevisionBanner();
		printExtraBannerNoop();
		s_bannerPrinted = 1u;
	}
}

/* 0x800a82 */
int isWarmBootMagicValid(void)
{
	return *(volatile uint32_t *)(uintptr_t)BOOT_VECTOR_MAGIC_PA == BOOT_VECTOR_PAGE_MAGIC;
}

/* 0x800a9a */
int copyWarmBootVector(void)
{
	return bigmacMemcpy((void *)(uintptr_t)BOOT_VECTOR_SRAM_PA,
			(const void *)(uintptr_t)BOOT_VECTOR_DRAM_PA, 0x100u);
}

/* 0x800aaa */
void *slMemset(void *dst, int c, size_t n)
{
	uint8_t *p = (uint8_t *)dst;
	uint8_t b = (uint8_t)c;
	size_t i;

	for (i = 0; i < n; i++)
		p[i] = b;
	return dst;
}

/* 0x800b00 */
int isIdStorageInitialized(void)
{
	return (int)s_idStorageInitialized;
}

/* 0x800b06 */
int runErnieHandshake0BAndLoadIds(void)
{
	HandshakePacket txPkt;
	HandshakePacket rxPkt;
	uint8_t verifyScratch[32];
	uint8_t hostRandom[8];
	uint8_t rawPsCode[16];
	uint8_t openPsId[16];
	int status;
	int psCodeErr;
	int openPsIdErr;

	status = ernieHandshakeBuildStartPacket(&txPkt, 0x0bu);
	if (status != 0)
		goto fail_early;
	status = sceSysconSendHandshakePacket(&txPkt, sizeof(txPkt));
	if (status != 0)
		goto fail_early;

	/* Load IdStorage leaf 0x46 (PsCode) while Ernie computes challenge */
	psCodeErr = readIdStoragePsCodeRaw(rawPsCode);
	if (psCodeErr == 0) {
		formatPsCodeFromLeaf(rawPsCode, &KBL_PARAM->pscode);
		writeKeyringSlotBytes(0x509u, rawPsCode, sizeof(rawPsCode));
	}

	status = sceSysconReceiveHandshakePacket(&rxPkt, sizeof(rxPkt));
	openPsIdErr = (int)0x800f0026;
	if (status != 0)
		goto cleanup;
	status = bigmacGenerateRandom(hostRandom, sizeof(hostRandom));
	if (status != 0)
		goto cleanup;
	status = ernieHandshakeProcessChallenge(&txPkt, &rxPkt, 0x0bu, 1u, hostRandom);
	if (status != 0)
		goto cleanup;
	status = sceSysconSendHandshakePacket(&txPkt, sizeof(txPkt));
	if (status != 0)
		goto cleanup;

	/* Load IdStorage leaf 0x47 (OpenPsId) while Ernie verifies response */
	openPsIdErr = readIdStorageOpenPsId(openPsId);
	if (openPsIdErr == 0) {
		slMemcpy(&KBL_PARAM->openpsid, openPsId, sizeof(openPsId));
		writeKeyringSlotBytes(0x50du, openPsId, sizeof(openPsId));
	}

	status = sceSysconReceiveHandshakePacket(&rxPkt, sizeof(rxPkt));
	if (status != 0)
		goto cleanup;
	status = ernieHandshakeVerifyAndFinish(&rxPkt, 0x0bu, 1u, verifyScratch, hostRandom, NULL);
	goto cleanup;

fail_early:
	psCodeErr = (int)0x800f0026;
	openPsIdErr = (int)0x800f0026;

cleanup:
	ernieHandshakeClearKeyMaterial(0x0bu, 1u);
	if (status != 0) {
		debugLogError(2, 'U', (uint32_t)status, 0);
	} else {
		s_idStorageInitialized = 1u;
		if (psCodeErr != 0) {
			setGpioDebugLedPostcode('K');
			debugLogError(2, 'K', (uint32_t)psCodeErr, 0);
			status = psCodeErr;
		} else if (openPsIdErr != 0) {
			setGpioDebugLedPostcode('S');
			debugLogError(2, 'S', (uint32_t)openPsIdErr, 0);
			status = openPsIdErr;
		}
	}
	return status;
}

/* 0x800c7c */
void printSdkBanner(void)
{
	debugPrintString(s_sceConfidentialBanner);
	debugPrintString(s_copyrightBanner);
	debugPrintString(s_allRightsReservedBanner);
	debugPrintString(s_sdkPrefix);
	debugSetNumberBase(NULL, 16u);
	debugPrintNumber(4u);
	debugPrintString(s_dotStr);
	debugPrintNumber(0u);
	debugPrintNumber(0u);
	debugPrintNumber(0u);
	debugPrintString(s_newlineStr);
}

/* 0x800cda */
void printBlRevisionBanner(void)
{
	uint32_t baryonVer;
	uint32_t bootTimestamp;

	s_cachedBootldrRevision = g_bootldrRevision;
	baryonVer = (uint32_t)sceSysconGetBaryonVersion();
	bootTimestamp = sceSysconGetBaryonTimestamp();
	debugPrintString(s_blPrefix);
	debugPrintString(s_revisionPrefix);
	debugSetNumberBase(NULL, 10u);
	debugPrintNumber(s_cachedBootldrRevision);
	debugPrintString(s_slashStr);
	debugSetNumberBase(NULL, 16u);
	debugPrintNumber(baryonVer >> 24);
	debugPrintString(s_dotStr);
	debugPrintNumber((baryonVer >> 16) & 0xffu);
	debugPrintString(s_dotStr);
	debugPrintNumber((baryonVer >> 8) & 0xffu);
	debugPrintString(s_dotStr);
	debugPrintNumber(baryonVer & 0xffu);
	if ((bootTimestamp & 0x40u) != 0)
		debugPrintString(s_releaseSuffixStr);
	else
		debugPrintString(s_commaSpaceStr);
	debugPrintString(s_datePrefix);
	debugPrintString(s_buildDateStr);
	debugPrintString(s_newlineStr);
}

/* 0x800d9e */
void printExtraBannerNoop(void)
{
}

/* 0x800da0 */
uint32_t getPervasiveSocRevision(void)
{
	if (!s_socRevisionCached) {
		s_socRevisionValue = PERVASIVE_MISC->revision;
		s_socRevisionCached = 1u;
	}
	return s_socRevisionValue;
}

/* 0x800dbc */
int verifySoCHardwareState(uint32_t sbootReg4, uint32_t crySboot, uint32_t secReg2020)
{
	uint32_t krStatus;

	if (PERVASIVE_RESET->arm != 0x0001000fu)
		return (int)0x800f0010;
	if ((getPervasiveSocRevision() & 0x00ffffffu) < 0x42u)
		return (int)0x800f0037;

	*(volatile uint32_t *)0xe0030028u = 0x501u;
	krStatus = *(volatile uint32_t *)0xe003002cu;
	if ((krStatus & 3u) != 3u || (krStatus & 4u) != 0)
		return (int)0x800f0029;
	if ((sbootReg4 & 0x8000001fu) != 0)
		return (int)0x800f0028;
	if ((crySboot & 5u) != 5u || (crySboot & 8u) != 0)
		return (int)0x800f002f;
	if ((secReg2020 & 7u) != 0)
		return (int)0x800f0033;
	return 0;
}

/* 0x800e74 */
int verifySnvsOsVersion(uint32_t kblMinRequiredVer, uint32_t isWarmBoot)
{
	uint32_t efuseWords[8];
	uint8_t rawSnvsSector[32];
	uint32_t osVerTable[8];
	uint32_t factoryIdx;
	uint32_t curBankIdx;
	uint32_t factoryVer;
	uint32_t currentVer = 0;
	int status;

	if (!isSnvsInitialized())
		return (int)0x800f0026;

	slMemset(osVerTable, 0, sizeof(osVerTable));
	slMemset(rawSnvsSector, 0, sizeof(rawSnvsSector));
	status = readSnvsSectorData(1, osVerTable, rawSnvsSector, 1);
	if (status != 0)
		return status;

	curBankIdx = 0;
	factoryIdx = 0;
	status = readEmmcPartitionActiveBank(1, &curBankIdx);
	if (status != 0)
		return status;
	if (curBankIdx >= 2u)
		return (int)0x800f0024;
	status = readEmmcPartitionFactoryIndex(1, &factoryIdx);
	if (status != 0)
		return status;
	if (factoryIdx >= 4u)
		return (int)0x800f0024;

	currentVer = osVerTable[curBankIdx];
	factoryVer = osVerTable[factoryIdx];
	if (factoryVer == 0xdeadbeefu)
		factoryVer = 0x04000000u;
	if (currentVer == 0xdeadbeefu) {
		currentVer = 0x04000000u;
	} else if (currentVer != 0x04000000u) {
		volatile uint32_t *efuseRegs = (volatile uint32_t *)0xe0062160u;
		unsigned int i;

		slMemset(efuseWords, 0, sizeof(efuseWords));
		for (i = 0; i < 8u; i++)
			efuseWords[i] = efuseRegs[i];
		if ((efuseWords[1] & 1u) != 0) {
			currentVer = 0x04000000u;
			factoryVer = 0x04000000u;
		}
	}

	if (currentVer != 0x04000000u) {
		debugLogError(0, 'N', currentVer, 1u);
		return (int)0x800f0037;
	}
	if (isWarmBoot == 0 && kblMinRequiredVer != 0x04000000u) {
		debugLogError(0, 'N', kblMinRequiredVer, 2u);
		return (int)0x800f003a;
	}

	writeKeyringSlotBytes(0x50eu, &currentVer, sizeof(currentVer));
	writeKeyringSlotBytes(0x518u, &factoryVer, sizeof(factoryVer));
	return 0;
}

/* 0x801038 */
int enableSd0Power(void)
{
	int status;

	if (!isSnvsSd0FlagEnabled())
		return 0;
	status = sdifInitController(1);
	if (status == 0)
		s_sd0PowerEnabled = 1u;
	return status;
}

/* 0x80105c */
int readSd0SectorsStub(void *dst, uint32_t sectorCount, uint32_t startSector)
{
	(void)dst;
	(void)sectorCount;
	(void)startSector;
	return (int)0x800f0016;
}

/* 0x801066 */
int getZeroStatusStub(void)
{
	return 0;
}

/* 0x80106a */
int isSd0PowerEnabled(void)
{
	return (int)s_sd0PowerEnabled;
}

/* 0x801070 */
int initEmmcStorage(void)
{
	unsigned int retry;
	int status = 0;

	for (retry = 0; retry < 60u; retry++) {
		status = sdifInitEmmcCard(0);
		if (status == 0) {
			sdifSetAdmaDescriptorBuffer((void *)0x40000400u, 0x100u);
			return 0;
		}
	}
	return status;
}

/* 0x8010b0 */
int readEmmcSectors(void *dst, uint32_t size, uint32_t startSector)
{
	if (sdifReadSectorsAligned(dst, size, startSector) == 0)
		return 0;
	return (int)0x800f0005;
}

/* 0x801100 */
static void lockKeyringRangeTable(void)
{
	KeyringLockRange ranges[8];
	unsigned int i;
	uint32_t slot;

	slMemcpy(ranges, s_bootKeyringLockRanges, sizeof(ranges));
	for (i = 0; i < 8u; i++) {
		for (slot = ranges[i].startSlot; slot <= ranges[i].endSlot; slot++)
			KEYRING_CTRL->status = (slot & 0xffffu) | 0x1c1f0000u;
	}
}

/* 0x80114e */
static void lockCmepSboot(void)
{
	*(volatile uint32_t *)(uintptr_t)CMEP_SBOOT_MEM_BASE = 0x00030003u;
	*(volatile uint32_t *)(uintptr_t)(CMEP_SBOOT_MEM_BASE + 4u) = 0u;
}

/* 0x8010ce */
void lockBootKeyringRanges(void)
{
	lockKeyringRangeTable();
	lockCmepSboot();
}

/* 0x8010e2 */
void setArmResetHold(int releaseArm)
{
	uint32_t val = CMEP_MAILBOX_REG(cry2Arm[0]);

	if (releaseArm)
		val |= 0x4u;
	else
		val |= 0x8u;
	CMEP_MAILBOX_REG(cry2Arm[0]) = val | 0x600u;
}
