/*
 * remote_osk.c: Kermit On-Screen Keyboard (OSK) server (channel 13)
 * backed by SceImeDialog for ScePspemu.
 */
#include "host_common.h"

#include <string.h>

#define OSK_CHANNEL_ID               13
#define OSK_CMD_ABORT                268
#define OSK_CMD_OPEN                 269
#define OSK_CMD_GET_STATUS           270

#define OSK_MAX_TEXT_CHARS           512
#define OSK_SDK_VERSION_365          PSPEMU_SDK_VERSION
#define OSK_COMMON_DIALOG_MAGIC_BASE 0xC0D1A109U

typedef struct PspOskData {
	uint32_t charMode;
	uint32_t extOption;
	uint32_t language;
	uint32_t lines;
	uint32_t inputTypes;
	uint32_t maxLines;
	uint32_t outTextOffset;
	uint32_t descGuestAddr;
	uint32_t inTextGuestAddr;
	uint32_t outTextLength;
	uint32_t outTextGuestAddr;
	int32_t  result;
	uint32_t outTextLimit;
} PspOskData;

typedef struct PspOskDialogParam {
	uint32_t size;
	int32_t  language;
	uint32_t buttonSwap;
	uint32_t graphicsThread;
	uint32_t accessThread;
	uint32_t fontThread;
	uint32_t soundThread;
	int32_t  result;
	uint32_t reservedWords[4];
	uint32_t dataCount;
	uint32_t dataGuestAddr;
	uint32_t localStatus;
	uint32_t padWord;
} PspOskDialogParam;

typedef struct OskLangBitEntry {
	int      slotIndex;
	uint32_t imeLangBit;
} OskLangBitEntry;

static const OskLangBitEntry g_oskLangBitTable[12] = {
	{  0, 0x00002000U },
	{  1, 0x00002000U },
	{  2, 0x00000400U },
	{  3, 0x00004000U },
	{  4, 0x00002000U },
	{  5, 0x00000000U },
	{  6, 0x00000000U },
	{  7, 0x00002000U },
	{  8, 0x00000000U },
	{  9, 0x00000000U },
	{ 10, 0x00000400U },
	{ 12, 0x00000000U },
};

static const uint32_t g_oskEuroLangBits[7] = {
	0x00000004U,
	0x00000010U,
	0x00000008U,
	0x00000002U,
	0x00000020U,
	0x00000040U,
	0x00000200U,
};

SceKernelLwMutexWork g_oskMutex;
SceUInt64            g_oskLastAbortTime;
int                  g_oskOpenPending;
uint16_t             g_oskInitialText[OSK_MAX_TEXT_CHARS + 1];
uint16_t             g_oskInputBuffer[OSK_MAX_TEXT_CHARS + 1];
int                  g_oskStatus;
int                  g_oskLanguage;
int                  g_oskAbortState;
PspOskDialogParam   *g_oskActiveParam;
int                  g_oskLangMask[13];
int                  g_oskKanaOnly;

static int __attribute__((noinline, noclone)) oskLock(void)
{
	int ret = sceKernelLockLwMutex(&g_oskMutex, 1, NULL);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int __attribute__((noinline, noclone)) oskUnlock(void)
{
	int ret = sceKernelUnlockLwMutex(&g_oskMutex, 1);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int remoteOskThread(SceSize args, void *argp) __attribute__((noreturn));
static int remoteOskThread(SceSize args, void *argp)
{
	PspemuKermitRequest *req;

	(void)args;
	(void)argp;

	for (;;) {
		int64_t retVal;
		int cmd = remotefsWaitAndGetRequest(OSK_CHANNEL_ID, &req);

		switch (cmd) {
		case OSK_CMD_OPEN: {
			PspOskDialogParam *param = (PspOskDialogParam *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 3, sizeof(PspOskDialogParam));
			oskLock();
			g_oskOpenPending = 1;
			g_oskActiveParam = param;
			g_oskStatus = 2;
			oskUnlock();
			retVal = 0;
			break;
		}
		case OSK_CMD_GET_STATUS:
			retVal = (int64_t)(uint32_t)g_oskStatus;
			break;
		case OSK_CMD_ABORT:
			oskLock();
			if (g_oskStatus != 3) {
				g_oskOpenPending = 0;
				g_oskAbortState = 1;
			}
			g_oskStatus = 0;
			oskUnlock();
			retVal = 0;
			break;
		default:
			pspemuRecordTitleListFlags(16U);
			retVal = 0xFFFFFFFFLL;
			break;
		}

		remotefsReturnValue(OSK_CHANNEL_ID, req, retVal);
	}
}

static int __attribute__((noinline, noclone)) oskFilterString(
	uint16_t *outText, uint32_t *outTextLength, const uint16_t *srcText, uint32_t srcTextLength)
{
	uint32_t written = 0U;

	for (uint32_t i = 0; i < srcTextLength; i++) {
		uint16_t ch = oskConvertHiraToKata(srcText[i], g_oskLangMask);
		if (oskIsCharAllowed(ch, g_oskLangMask, g_oskLanguage, g_oskKanaOnly) != 0)
			outText[written++] = ch;
	}

	*outTextLength = written;
	return 1;
}

static int oskImeFilterCallback(
	uint16_t *outText, uint32_t *outTextLength, const uint16_t *srcText, uint32_t srcTextLength)
{
	if (*outTextLength < srcTextLength)
		return 0;
	return oskFilterString(outText, outTextLength, srcText, srcTextLength);
}

static int __attribute__((noinline, noclone)) oskOpenDialog(PspOskDialogParam *param)
{
	PspemuImeDialogParam imeParam;
	OskLangBitEntry langTable[12];

	g_oskActiveParam = param;
	if (pspemuMenuLockDialog(1) != 0)
		return -2;

	sceClibMemset(&imeParam, 0, sizeof(imeParam));
	imeParam.sdkVersion = OSK_SDK_VERSION_365;
	imeParam.commonParamMagic = OSK_COMMON_DIALOG_MAGIC_BASE + (uint32_t)(uintptr_t)&imeParam.commonParamReserved;

	if (param == NULL)
		return -1;

	PspOskData *oskData = (PspOskData *)remotefsTranslateGuestAddr(param->dataGuestAddr, 1, sizeof(PspOskData));
	const uint16_t *inText = (const uint16_t *)remotefsTranslateGuestAddr(oskData->inTextGuestAddr, 1, 1026U);
	const uint16_t *descText = (const uint16_t *)remotefsTranslateGuestAddr(oskData->descGuestAddr, 1, 1026U);

	g_oskKanaOnly = ((oskData->charMode & 1U) == 0U) ? 1 : 0;
	int lang = (oskData->language != 0U) ? ((int)oskData->language - 1) : param->language;
	uint32_t inputTypes = oskData->inputTypes;
	g_oskLanguage = lang;

	for (int i = 0; i < 13; i++)
		g_oskLangMask[i] = 0;

	uint32_t euroEnabled;
	int latinEnabled;

	if (inputTypes == 0U) {
		if (lang == 0) {
			g_oskLangMask[10] = 0;
			g_oskLangMask[2] = 0;
			g_oskLangMask[3] = 0;
			g_oskLangMask[0] = 1;
			g_oskLangMask[1] = 1;
			g_oskLangMask[4] = 1;
			g_oskLangMask[7] = 1;
			g_oskLangMask[6] = 0;
			g_oskLangMask[12] = 0;
		} else {
			g_oskLangMask[0] = 0;
			g_oskLangMask[1] = 0;
			g_oskLangMask[4] = 0;
			g_oskLangMask[7] = 0;
			if (lang == 8) {
				g_oskLangMask[3] = 0;
				g_oskLangMask[10] = 1;
				g_oskLangMask[2] = 1;
				g_oskLangMask[6] = 0;
				g_oskLangMask[12] = 0;
			} else if (lang == 9) {
				g_oskLangMask[10] = 0;
				g_oskLangMask[2] = 0;
				g_oskLangMask[3] = 1;
				g_oskLangMask[6] = 0;
				g_oskLangMask[12] = 0;
			} else {
				g_oskLangMask[10] = 0;
				g_oskLangMask[2] = 0;
				g_oskLangMask[3] = 0;
				if (lang == 1) {
					g_oskLangMask[6] = 1;
					g_oskLangMask[12] = 1;
				} else {
					g_oskLangMask[6] = 0;
					g_oskLangMask[12] = ((unsigned int)(lang - 8) <= 1U || (unsigned int)(lang - 10) <= 1U) ? 0 : 1;
				}
			}
		}
		euroEnabled = 1U;
		latinEnabled = 1;
		g_oskLangMask[5] = 1;
		g_oskLangMask[8] = 1;
		g_oskLangMask[9] = 1;
		g_oskLangMask[11] = 1;
	} else {
		int allowAscii = (inputTypes & 2U) != 0U;
		int allowRomaji = 0;
		if ((inputTypes & 0x200U) != 0U) {
			if (oskNeedsFullwidthLatin(inputTypes))
				allowAscii = 1;
			else
				allowRomaji = 1;
		}

		int allowAsciiDigits = (inputTypes & 1U) != 0U;
		int allowFullDigits = 0;
		if ((inputTypes & 0x100U) != 0U) {
			if (oskNeedsFullwidthDigits(inputTypes))
				allowAsciiDigits = 1;
			else
				allowFullDigits = 1;
		}

		int allowKanji = ((inputTypes & 0x8000U) != 0U) ? 1 : ((inputTypes & 0x1000U) != 0U);
		int allowLatinFlags = (inputTypes & 0xCU) != 0U;
		latinEnabled = allowAscii || allowLatinFlags;
		if ((inputTypes & 0xC00U) != 0U) {
			if (oskNeedsFullwidthLatin(inputTypes))
				latinEnabled = 1;
			else
				allowRomaji = 1;
		}

		int allowRussian = (inputTypes & 0x80000U) != 0U;
		int allowEuroLang = (lang != 0 && (unsigned int)(lang - 8) > 3U);
		int allowEuroBasic = allowRussian || allowLatinFlags;
		int allowEuroExt = allowEuroLang && allowLatinFlags;
		int allowKata = 0;
		int allowChinese = 0;
		int allowKorean = 0;

		if (lang == 0) {
			allowKata = (inputTypes & 0x4000U) != 0U;
		} else {
			allowFullDigits = 0;
			allowKanji = 0;
			if (lang == 8)
				allowChinese = (inputTypes & 0x30000U) != 0U;
			else if (lang == 9)
				allowKorean = (inputTypes & 0x40000U) != 0U;
		}

		if ((allowKata | allowEuroBasic | allowRussian | allowAsciiDigits | latinEnabled |
		     allowRomaji | allowEuroExt | allowChinese | allowKorean | allowKanji | allowFullDigits) == 0) {
			g_oskLangMask[8] = 1;
			euroEnabled = (uint32_t)g_oskLangMask[11];
			latinEnabled = g_oskLangMask[5];
		} else {
			g_oskLangMask[4] = allowRomaji;
			g_oskLangMask[5] = latinEnabled;
			g_oskLangMask[6] = (lang == 1 && latinEnabled != 0) ? 1 : 0;
			g_oskLangMask[10] = allowChinese;
			g_oskLangMask[2] = allowChinese;
			g_oskLangMask[0] = allowKanji;
			g_oskLangMask[7] = allowFullDigits;
			g_oskLangMask[3] = allowKorean;
			g_oskLangMask[1] = allowKata;
			g_oskLangMask[9] = allowRussian;
			g_oskLangMask[11] = allowEuroBasic;
			g_oskLangMask[12] = allowEuroExt;
			g_oskLangMask[8] = allowAsciiDigits;
			euroEnabled = (uint32_t)allowEuroBasic;
		}
	}

	uint32_t imeType = 0U;
	if (g_oskLangMask[8] != 0) {
		int onlyDigits = 1;
		for (int i = 0; i < 13; i++) {
			if (i != 8 && g_oskLangMask[i] != 0) {
				onlyDigits = 0;
				break;
			}
		}
		if (onlyDigits)
			imeType = 2U;
	}

	if (latinEnabled != 0 || g_oskLangMask[9] != 0 || euroEnabled != 0U) {
		int onlyBasicLatin = 1;
		for (int i = 0; i < 13; i++) {
			if (i == 5 || i == 8 || i == 9 || i == 11)
				continue;
			if (g_oskLangMask[i] != 0) {
				onlyBasicLatin = 0;
				break;
			}
		}
		if (onlyBasicLatin)
			imeType = 1U;
	}

	memcpy(langTable, g_oskLangBitTable, sizeof(langTable));
	uint32_t langBits = 0U;
	if (euroEnabled != 0U) {
		uint32_t euroIdx = (uint32_t)(lang - 1);
		if (euroIdx <= 6U)
			langBits = g_oskEuroLangBits[euroIdx];
	}
	for (int i = 0; i < 12; i++) {
		if (g_oskLangMask[langTable[i].slotIndex] != 0)
			langBits |= langTable[i].imeLangBit;
	}

	uint64_t supportedLanguages;
	int languagesForced;
	if (pspemuIsPs1Mode() == 0 && pspemuCompatHasFlag(0x400000U) != 0) {
		languagesForced = 1;
		supportedLanguages = 4ULL;
	} else {
		languagesForced = (langBits != 0U) ? 1 : 0;
		supportedLanguages = (uint64_t)langBits;
	}

	memset(g_oskInitialText, 0, sizeof(g_oskInitialText));
	wcsncpy(g_oskInitialText, inText, OSK_MAX_TEXT_CHARS);
	if (pspemuCompatHasFlag(256U) != 0) {
		uint32_t len = OSK_MAX_TEXT_CHARS;
		oskFilterString(g_oskInitialText, &len, g_oskInitialText, OSK_MAX_TEXT_CHARS);
	}

	imeParam.supportedLanguages = supportedLanguages;
	imeParam.languagesForced = languagesForced;
	imeParam.type = imeType;
	imeParam.option = (oskData->maxLines > 1U) ? 1U : 0U;
	imeParam.filter = oskImeFilterCallback;
	imeParam.dialogMode = 1U;
	imeParam.title = descText;
	imeParam.initialText = g_oskInitialText;
	imeParam.inputTextBuffer = g_oskInputBuffer;

	uint32_t maxLen;
	if (oskData->outTextLimit != 0U) {
		maxLen = oskData->outTextLimit;
		uint32_t cap = oskData->outTextLength - 1U;
		if (maxLen > cap)
			maxLen = cap;
	} else {
		maxLen = oskData->outTextLength - 1U;
	}
	if (maxLen > OSK_MAX_TEXT_CHARS)
		maxLen = OSK_MAX_TEXT_CHARS;
	imeParam.maxTextLength = maxLen;

	int ret = sceImeDialogInit(&imeParam);
	if (ret >= 0) {
		uint64_t delayUs;
		if (g_oskLastAbortTime != 0ULL) {
			SceUInt64 elapsed = sceKernelGetProcessTimeWide() - g_oskLastAbortTime;
			g_oskLastAbortTime = 0ULL;
			delayUs = (elapsed <= 3999999ULL) ? 0ULL : 2000000ULL;
		} else {
			delayUs = 2000000ULL;
		}
		pspemuMenuSetImeDeadline(sceKernelGetProcessTimeWide() + delayUs);
		ret = 0;
	}
	return ret;
}

static int remoteOskUpdateThread(SceSize args, void *argp) __attribute__((noreturn));
static int remoteOskUpdateThread(SceSize args, void *argp)
{
	PspemuImeDialogResult dlgResult;

	(void)args;
	(void)argp;

	for (;;) {
		oskLock();
		int status = sceImeDialogGetStatus();

		switch (status) {
		case SCE_COMMON_DIALOG_STATUS_NONE:
			if (g_oskOpenPending != 0) {
				int openRet = oskOpenDialog(g_oskActiveParam);
				if (openRet != -2) {
					if (openRet < 0)
						g_oskStatus = 0;
					g_oskOpenPending = 0;
				}
			}
			break;
		case SCE_COMMON_DIALOG_STATUS_RUNNING:
			pspemuMenuPollDialog(0);
			break;
		case SCE_COMMON_DIALOG_STATUS_FINISHED:
			memset(&dlgResult, 0, sizeof(dlgResult));
			if (sceImeDialogGetResult(&dlgResult) >= 0) {
				sceImeDialogTerm();
				if (g_oskActiveParam != NULL) {
					PspOskData *oskData = (PspOskData *)remotefsTranslateGuestAddr(
						g_oskActiveParam->dataGuestAddr, 3, sizeof(PspOskData));
					oskPostProcessResult(g_oskInputBuffer, 1026, oskData->inputTypes, (int)oskData->maxLines);

					if (dlgResult.result == 0) {
						g_oskActiveParam->result = 0;
						oskData->result = 2;
						uint16_t *outBuf = (uint16_t *)remotefsTranslateGuestAddr(
							oskData->outTextGuestAddr, 2, 2U * oskData->outTextLength);
						wcsncpy(outBuf, g_oskInputBuffer, oskData->outTextLength);
						remotefsFlushGuestAddr(outBuf, 2U * oskData->outTextLength);
					} else if (dlgResult.result == 1) {
						g_oskActiveParam->result = 1;
						oskData->result = 1;
					} else if (dlgResult.result == 2) {
						int prevAbort = g_oskAbortState;
						g_oskAbortState = 0;
						g_oskActiveParam->result = (prevAbort == 2) ? 1 : 2;
						g_oskLastAbortTime = sceKernelGetProcessTimeWide();
					}

					remotefsFlushGuestAddr(g_oskActiveParam, sizeof(PspOskDialogParam));
					remotefsFlushGuestAddr(oskData, sizeof(PspOskData));
				}
				pspemuMenuClearImeDeadline();
				pspemuSettingsApplyDisplay();
				pspemuMenuUnlockDialog(1);
			}
			g_oskStatus = 3;
			break;
		default:
			break;
		}

		if (g_oskAbortState != 0) {
			if (sceImeDialogAbort() < 0) {
				g_oskAbortState = 0;
				g_oskLastAbortTime = sceKernelGetProcessTimeWide();
			} else {
				pspemuMenuSetImeDeadline(sceKernelGetProcessTimeWide());
			}
		}

		oskUnlock();
		sceKernelDelayThread(33333U);
	}
}

int remoteOskInit(void)
{
	if (sceKernelCreateLwMutex(&g_oskMutex, "ScePspemuOsk", 0, 0, NULL) != 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	SceUID thid = sceKernelCreateThread(
		"ScePspemuRemoteOsk",
		remoteOskThread,
		96,
		0x1000U,
		0,
		0x10000,
		NULL);
	if (thid < 0)
		return thid;

	int ret = sceKernelStartThread(thid, 0, NULL);
	if (ret != 0)
		return ret;

	SceUID updateThid = sceKernelCreateThread(
		"ScePspemuRemoteOskUpdate",
		remoteOskUpdateThread,
		96,
		0x1000U,
		0,
		0x10000,
		NULL);
	if (updateThid < 0)
		return updateThid;
	return sceKernelStartThread(updateThid, 0, NULL);
}

int remoteOskAbort(void)
{
	oskLock();
	g_oskAbortState = 2;
	oskUnlock();

	while (sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_NONE) {
		pspemuMenuPollDialog(0);
		sceKernelDelayThread(1000U);
	}
	return 0;
}
