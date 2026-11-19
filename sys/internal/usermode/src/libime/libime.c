/*
 * SceIme (vs0:sys/external/libime.suprx): sceImeOpen and its calls, over SceImeVsh, which talks to
 * the IME service over IPMI. Built by Sony with SNC. No debug record names its files; the names
 * are ours.
 */

#include <stddef.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include "libime.h"

/* IPMI::Client::getBufferSize(const Config *) and IPMI::Client::create(Client **, const Config *, void *, void *) */
extern int _ZN4IPMI6Client13getBufferSizeEPKNS0_6ConfigE(const ImeIpmiConfig *pConfig);
extern int _ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_(ImeIpmiClient **ppClient, const ImeIpmiConfig *pConfig,
	void *pUserData, void *pMemory);
#define SceIpmi_4E255C31 _ZN4IPMI6Client13getBufferSizeEPKNS0_6ConfigE
#define SceIpmi_B282B430 _ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_
extern int sceAppMgrIsGameProgram(void);
extern int sceAppMgrGetStatusByName(int pid, void *pStatus);
extern int sceCtrlSetButtonIntercept(int intercept);
extern int sceCtrlGetButtonIntercept(int *pIntercept);
extern int sceHidKeyboardSetIntercept(int intercept);
extern int SceSvcCtrl_683998F2(void);
extern int SceSvcCtrl_B21BE499(void);

void SceImeVsh_D3FE7CD8(void) __attribute__((noinline));
void SceImeVsh_03D0D46F(void) __attribute__((noinline));
int SceImeVsh_269FFDCF(ImeVshHandle *pHandle) __attribute__((noinline));
int SceImeVsh_2765866F(ImeVshHandle *pHandle, const ImeVshParam *pParam) __attribute__((noinline));
int SceImeVsh_3DA671DD(int a0, int a1, const short *pData) __attribute__((noinline));
int SceImeVsh_443E963A(ImeVshHandle **ppHandle, const ImeVshParam *pParam) __attribute__((noinline));
int SceImeVsh_4D7F8416(int a0, int a1, int a2, int a3) __attribute__((noinline));
int SceImeVsh_684C4574(ImeVshHandle *pHandle, SceUInt32 index) __attribute__((noinline));
int SceImeVsh_7EC113FA(int a0, int a1, void *pOut) __attribute__((noinline));
int SceImeVsh_8BE1872C(ImeVshHandle *pHandle) __attribute__((noinline));
int SceImeVsh_9AEEC1BD(ImeVshHandle *pHandle, const SceWChar16 *pText, SceUInt32 caretIndex) __attribute__((noinline));
int SceImeVsh_A41DB14B(int a0, int a1, int a2, void *pOut) __attribute__((noinline));
int SceImeVsh_BE92A5B3(void) __attribute__((noinline));
int SceImeVsh_DDBE0490(ImeVshHandle *pHandle) __attribute__((noinline));
int SceImeVsh_F27C99D3(ImeVshHandle *pHandle, const SceUInt32 *pCaret) __attribute__((noinline));
int SceImeVsh_F4A4865B(ImeVshHandle *pHandle) __attribute__((noinline));
int SceImeVsh_FD1A16DF(void) __attribute__((noinline));

/* the application's permission to use the layer (the status it reports is read, not named) */
#define IME_VSH_ALLOWED(status) \
	(sceAppMgrIsGameProgram() != 0 || (sceAppMgrGetStatusByName(0, &(status)), (status).value != 0))

/* the library's state */
SceUInt8 g_initialized __attribute__((aligned(4)));
SceUInt8 g_vshOpened __attribute__((aligned(4)));
SceUInt8 g_vshMutexCreated __attribute__((aligned(4)));
SceKernelLwMutexWork g_mutex;
ImeState g_ime;
SceKernelLwMutexWork g_vshMutex;
SceUInt8 g_ipmiMemory[0x200];

static void ImeVshCreateMutex(void) __attribute__((noinline));
static void ImeVshDeleteMutex(void) __attribute__((noinline));
static int ImeCheckOpenParam(ImeVshParam *pOut, const SceImeParam *pParam) __attribute__((noinline));
static void ImeUpdatePreedit(void *arg, const ImeVshCall *pCall) __attribute__((noinline));
static void ImeProcessEvent(void *arg, const ImeVshCall *pCall) __attribute__((noinline));
static void ImeHandleEvent(void *arg, const ImeVshCall *pCall) __attribute__((noinline));
static SceUInt32 ImeTextLength(const SceWChar16 *pText, SceUInt32 max) __attribute__((noinline));
static inline SceUInt32 ImeTextLengthInline(const SceWChar16 *pText, SceUInt32 max) __attribute__((always_inline));
static SceUInt32 ImeInsertText(SceWChar16 *pText, SceUInt32 capacity, SceUInt32 index, const SceWChar16 *pInsert) __attribute__((noinline));
static int ImeCopyText(SceWChar16 *pDst, SceUInt32 capacity, const SceWChar16 *pSrc, SceUInt32 max) __attribute__((noinline));
static int ImeRemoveText(SceWChar16 *pText, SceUInt32 capacity, SceUInt32 index, SceUInt32 count) __attribute__((noinline));
static int ImeVshCheckParam(ImeOpenRequest *pOut, const ImeVshParam *pParam) __attribute__((noinline));
static int ImeVshConnect(ImeIpmiClient **ppClient, int type, int budget) __attribute__((noinline));
static void ImeVshDispatch(ImeVshHandle *pHandle, ImeVshEvent *pEvent) __attribute__((noinline));
static int ImeVshSetPreeditGeometry(ImeVshHandle *pHandle, const SceUInt32 *pGeometry) __attribute__((noinline));
static int ImeVshInvoke(int type, int method, const void *pIn, int inSize, int *pResult, void *pOut,
	SceSize *pOutSize, SceSize outMax) __attribute__((noinline));

int module_start(void)
{
	SceImeVsh_D3FE7CD8();
	return 0;
}

int module_stop(void)
{
	SceImeVsh_03D0D46F();
	return 0;
}

void SceImeVsh_D3FE7CD8(void)
{
	if (g_initialized == 0) {
		ImeVshCreateMutex();
		sceClibMemset(&g_ime, 0, sizeof(g_ime));
		sceKernelCreateLwMutex(&g_mutex, "SceImeMutex", 0x2002, 0, NULL);
		g_initialized = 1;
	}
}

void SceImeVsh_03D0D46F(void)
{
	if (g_initialized != 0) {
		sceKernelDeleteLwMutex(&g_mutex);
		ImeVshDeleteMutex();
		g_initialized = 0;
	}
}

/* the application's SceImeParam, checked, as the layer's own (64 bytes) */
static int ImeCheckOpenParam(ImeVshParam *pOut, const SceImeParam *pParam)
{
	SceKernelSystemSwVersion version;
	SceUInt32 sdkVersion;
	SceUInt32 firmware;
	SceUInt32 maxType;
	SceUInt32 maxLength;
	SceUInt32 length;
	SceUInt32 i;
	const SceUInt8 *pReserved;
	const SceWChar16 *pInitial;
	SceUInt64 unsupported;

	if (pParam == NULL)
		return SCE_IME_ERROR_INVALID_PARAM;
	version.size = 40;
	if (sceKernelGetSystemSwVersion(&version) < 0) {
		version.version = 0xFFFFFFFF;
		version.versionExtra = 0xFFFFFFFF;
	}
	sdkVersion = pParam->sdkVersion;
	firmware = (sdkVersion > version.version) ? version.version : sdkVersion;
	if (pParam->inputMethod != 0)
		return SCE_IME_ERROR_INVALID_INPUT_METHOD;
	unsupported = (pParam->supportedLanguages ^ 0xFFFFF) & pParam->supportedLanguages;
	if (unsupported != 0)
		return SCE_IME_ERROR_INVALID_SUPPORTED_LANGUAGES;
	if (pParam->languagesForced > 1)
		return SCE_IME_ERROR_INVALID_PARAM;
	maxType = (firmware <= 0x2100000) ? 3 : 5;
	if (maxType < pParam->type)
		return SCE_IME_ERROR_INVALID_TYPE;
	if ((pParam->option & (pParam->option ^ 6)) != 0)
		return SCE_IME_ERROR_INVALID_OPTION;
	if ((SceUInt32)pParam->work == 0 || ((int)pParam->work % 4) != 0)
		return SCE_IME_ERROR_INVALID_WORK;
	if (pParam->handler == NULL)
		return SCE_IME_ERROR_INVALID_HANDLER;
	maxLength = (sdkVersion <= 0x2000000) ? 512 : 2048;
	length = pParam->maxTextLength;
	if (length == 0 || length > maxLength)
		return SCE_IME_ERROR_INVALID_MAX_TEXT_LENGTH;
	if (pParam->inputTextBuffer == NULL)
		return SCE_IME_ERROR_INVALID_INPUT_TEXT_BUFFER;
	if (pParam->enterLabel > 3)
		return SCE_IME_ERROR_INVALID_ENTER_LABEL;
	pReserved = pParam->reserved;
	for (i = 0; i < 7; i++) {
		if (pReserved[i] != 0)
			return SCE_IME_ERROR_INVALID_RESERVED;
	}
	if ((SceUInt32)pParam->inputTextBuffer <= (SceUInt32)pParam->work
	    && (SceUInt32)pParam->inputTextBuffer + 2 * length + 61 >= (SceUInt32)pParam->work)
		return SCE_IME_ERROR_INVALID_PARAM;
	if ((SceUInt32)pParam->inputTextBuffer >= (SceUInt32)pParam->work
	    && (SceUInt32)pParam->inputTextBuffer <= (SceUInt32)pParam->work + 20479)
		return SCE_IME_ERROR_INVALID_PARAM;
	sceClibMemset(pParam->work, 0, IME_WORK_SIZE);
	pOut->size = 64;
	pOut->option = pParam->option;
	pOut->type = pParam->type;
	pOut->supportedLanguages = pParam->supportedLanguages;
	pOut->work = pParam->work;
	pOut->handler = ImeHandleEvent;
	pOut->filter = pParam->filter;
	pOut->arg = pParam->arg;
	pOut->languagesForced = pParam->languagesForced;
	pOut->enterLabel = pParam->enterLabel;
	pOut->sdkVersion = firmware;
	g_ime.sdkVersion = firmware;
	g_ime.composeState = 0;
	g_ime.pArg = pParam->arg;
	g_ime.caretIndex = 0;
	g_ime.pWork = (SceWChar16 *)((SceUInt8 *)pParam->work + 10240);
	g_ime.preeditIndex = 0;
	g_ime.filter = pParam->filter;
	g_ime.maxTextLength = pParam->maxTextLength;
	g_ime.textBufferLength = pParam->maxTextLength + 30;
	g_ime.pText = pParam->inputTextBuffer;
	sceClibMemset(pParam->inputTextBuffer, 0, 2 * pParam->maxTextLength + 62);
	g_ime.pShadowText = (SceWChar16 *)((SceUInt8 *)pParam->work + 10240);
	sceClibMemset(g_ime.pShadowText, 0, 2 * g_ime.textBufferLength + 2);
	pInitial = pParam->initialText;
	if (pInitial != NULL) {
		length = pParam->maxTextLength;
		for (i = 0; i < length; i++) {
			if (*pInitial == 10 || *pInitial == 13 || (*pInitial >= 0xD800 && *pInitial <= 0xDFFF))
				return SCE_IME_ERROR_INVALID_TEXT;
			if (*pInitial == 0)
				break;
			pInitial++;
		}
		ImeCopyText(g_ime.pText, g_ime.maxTextLength, pParam->initialText, g_ime.maxTextLength);
		ImeCopyText(g_ime.pShadowText, g_ime.maxTextLength, pParam->initialText, g_ime.maxTextLength);
	}
	g_ime.handler = pParam->handler;
	return 0;
}

int sceImeOpen(const SceImeParam *pParam)
{
	ImeVshParam param;
	int ret;

	if (sceKernelLockLwMutex(&g_mutex, 1, NULL) >= 0) {
		if (g_ime.opened != 0) {
			ret = SCE_IME_ERROR_ALREADY_OPENED;
		} else {
			ret = 0;
			if (SceSvcCtrl_683998F2() < 0)
				ret = SCE_IME_ERROR_ALREADY_OPENED;
			if (ret < 0
			    || (sceClibMemset(&param, 0, sizeof(param)), (ret = ImeCheckOpenParam(&param, pParam)) < 0)
			    || (ret = SceImeVsh_443E963A(&g_ime.pHandle, &param)) < 0) {
				SceSvcCtrl_B21BE499();
			} else {
				g_ime.opened = 1;
				g_ime.firstEvent = 0;
			}
		}
	} else {
		ret = SCE_IME_ERROR_INTERNAL;
	}
	sceKernelUnlockLwMutex(&g_mutex, 1);
	return ret;
}

int sceImeUpdate(void)
{
	int ret;

	if (sceKernelLockLwMutex(&g_mutex, 1, NULL) >= 0) {
		if (g_ime.opened != 0) {
			g_ime.updated = 0;
			ret = SceImeVsh_F4A4865B(g_ime.pHandle);
			if (ret >= 0) {
				if (g_ime.updated != 0)
					SceImeVsh_9AEEC1BD(g_ime.pHandle, g_ime.pText, g_ime.caretIndex);
			} else {
				ret = SCE_IME_ERROR_INTERNAL;
			}
		} else {
			ret = SCE_IME_ERROR_NOT_OPENED;
		}
	} else {
		ret = SCE_IME_ERROR_INTERNAL;
	}
	sceKernelUnlockLwMutex(&g_mutex, 1);
	return ret;
}

int sceImeSetText(const SceWChar16 *pText, SceUInt32 length)
{
	const SceWChar16 *p;
	SceUInt32 i;
	int ret;

	if (sceKernelLockLwMutex(&g_mutex, 1, NULL) >= 0) {
		if (pText != NULL) {
			if (g_ime.opened != 0) {
				if (length != 0) {
					p = pText;
					i = 0;
					while (1) {
						if (*p == 10 || *p == 13 || (*p >= 0xD800 && *p <= 0xDFFF)) {
							ret = SCE_IME_ERROR_INVALID_TEXT;
							break;
						}
						if (*p != 0) {
							i++;
							p++;
							if (i < length)
								continue;
						}
						goto set;
					}
				} else {
set:
					ImeCopyText(g_ime.pText, g_ime.maxTextLength, pText, length);
					ImeCopyText(g_ime.pShadowText, g_ime.maxTextLength, g_ime.pText, length);
					g_ime.preeditIndex = ImeTextLength(g_ime.pText, g_ime.maxTextLength);
					g_ime.caretIndex = g_ime.preeditIndex;
					g_ime.composeState = 0;
					ret = SceImeVsh_269FFDCF(g_ime.pHandle);
					g_ime.preeditLength = 0;
					SceImeVsh_9AEEC1BD(g_ime.pHandle, g_ime.pText, g_ime.caretIndex);
				}
			} else {
				ret = SCE_IME_ERROR_NOT_OPENED;
			}
		} else {
			ret = SCE_IME_ERROR_INVALID_POINTER;
		}
	} else {
		ret = SCE_IME_ERROR_INTERNAL;
	}
	sceKernelUnlockLwMutex(&g_mutex, 1);
	return ret;
}

int sceImeSetCaret(const SceImeCaret *pCaret)
{
	SceUInt32 geometry[3];
	SceUInt32 index;
	int ret;

	if (sceKernelLockLwMutex(&g_mutex, 1, NULL) >= 0) {
		if (g_ime.opened != 0) {
			if (pCaret != NULL) {
				index = pCaret->index;
				if (index <= ImeTextLength(g_ime.pText, g_ime.textBufferLength)) {
					index = pCaret->index;
					if (g_ime.caretIndex != index) {
						if (g_ime.preeditLength != 0) {
							g_ime.caretIndex = pCaret->index;
							if (g_ime.preeditIndex > index || index > g_ime.preeditIndex + g_ime.preeditLength)
								SceImeVsh_8BE1872C(g_ime.pHandle);
							else
								SceImeVsh_684C4574(g_ime.pHandle, index - g_ime.preeditIndex);
						} else {
							g_ime.caretIndex = index;
							g_ime.preeditIndex = index;
							SceImeVsh_9AEEC1BD(g_ime.pHandle, g_ime.pText, index);
						}
					}
					geometry[0] = pCaret->x;
					geometry[1] = pCaret->y;
					geometry[2] = pCaret->height;
					SceImeVsh_F27C99D3(g_ime.pHandle, geometry);
					ret = 0;
				} else {
					ret = SCE_IME_ERROR_INVALID_PARAM;
				}
			} else {
				ret = SCE_IME_ERROR_INVALID_PARAM;
			}
		} else {
			ret = SCE_IME_ERROR_NOT_OPENED;
		}
	} else {
		ret = SCE_IME_ERROR_INTERNAL;
	}
	sceKernelUnlockLwMutex(&g_mutex, 1);
	return ret;
}

int sceImeSetPreeditGeometry(const SceImePreeditGeometry *pPreedit)
{
	int ret;

	if (sceKernelLockLwMutex(&g_mutex, 1, NULL) >= 0) {
		if (g_ime.opened != 0) {
			if (pPreedit != NULL) {
				if (pPreedit->x >= 0x3C0)
					ret = SCE_IME_ERROR_INVALID_PARAM;
				else if (pPreedit->y < 0x220)
					ret = ImeVshSetPreeditGeometry(g_ime.pHandle, (const SceUInt32 *)pPreedit);
				else
					ret = SCE_IME_ERROR_INVALID_PARAM;
			} else {
				ret = SCE_IME_ERROR_INVALID_PARAM;
			}
		} else {
			ret = SCE_IME_ERROR_NOT_OPENED;
		}
	} else {
		ret = SCE_IME_ERROR_INTERNAL;
	}
	sceKernelUnlockLwMutex(&g_mutex, 1);
	return ret;
}

int sceImeClose(void)
{
	int ret;
	int vshRet;

	ret = 0;
	if (sceKernelLockLwMutex(&g_mutex, 1, NULL) >= 0) {
		if (g_ime.opened != 0) {
			vshRet = SceImeVsh_DDBE0490(g_ime.pHandle);
			if (vshRet >= 0 || vshRet == SCE_IME_ERROR_TOO_MANY_REQUESTS) {
				g_ime.opened = 0;
				g_ime.pText = NULL;
				SceSvcCtrl_B21BE499();
			} else {
				ret = SCE_IME_ERROR_INTERNAL;
			}
		} else {
			ret = SCE_IME_ERROR_NOT_OPENED;
		}
	} else {
		ret = SCE_IME_ERROR_INTERNAL;
	}
	sceKernelUnlockLwMutex(&g_mutex, 1);
	return ret;
}

/* the preedit the service reports: the text before it is put back, and the preedit inserted */
static void ImeUpdatePreedit(void *arg, const ImeVshCall *pCall)
{
	SceImeEvent event;
	const SceWChar16 *pPreedit;
	SceUInt32 length;
	SceUInt32 index;
	SceUInt32 caret;

	pPreedit = (const SceWChar16 *)pCall->param.value[0];
	ImeCopyText(g_ime.pText, g_ime.maxTextLength, g_ime.pShadowText, g_ime.maxTextLength);
	ImeInsertText(g_ime.pText, g_ime.textBufferLength, g_ime.preeditIndex, pPreedit);
	g_ime.caretIndex = g_ime.preeditIndex + pCall->param.value[1];
	length = ImeTextLength((const SceWChar16 *)pCall->param.value[0], IME_PREEDIT_LENGTH);
	g_ime.preeditLength = length;
	index = g_ime.preeditIndex;
	caret = g_ime.caretIndex;
	sceClibMemset(&event, 0, sizeof(event));
	event.id = SCE_IME_EVENT_UPDATE_TEXT;
	event.param.text.preeditIndex = index;
	event.param.text.preeditLength = length;
	event.param.text.caretIndex = caret;
	event.param.text.str = g_ime.pText;
	event.param.text.editIndex = 0;
	event.param.text.editLengthChange = 0;
	g_ime.handler(arg, &event);
	g_ime.composeState = g_ime.preeditLength != 0;
}

/* an event of the service, turned into the application's */
static void ImeProcessEvent(void *arg, const ImeVshCall *pCall)
{
	SceImeEvent event;
	SceImeEvent preedit;
	SceUInt32 id;
	SceUInt32 caret;
	SceUInt32 oldCaret;
	int length;
	int inserted;

	sceClibMemset(&event, 0, sizeof(event));
	id = pCall->id;
	caret = g_ime.caretIndex;
	if (id > 0x106) {
		if (id <= 0x201 && id > 0x1FF) {
			if (id <= 0x200) {
				if (g_ime.caretIndex != 0) {
					g_ime.caretIndex--;
					event.id = SCE_IME_EVENT_UPDATE_CARET;
					event.param.caretIndex = caret - 1;
					g_ime.handler(arg, &event);
					g_ime.updated = 1;
				}
			} else if (caret < ImeTextLength(g_ime.pText, g_ime.maxTextLength)) {
				g_ime.caretIndex++;
				event.id = SCE_IME_EVENT_UPDATE_CARET;
				event.param.caretIndex = g_ime.caretIndex;
				g_ime.handler(arg, &event);
				g_ime.updated = 1;
			}
		}
	} else if (id > 3) {
		if (id <= 5) {
			if (g_ime.preeditLength != 0) {
				sceClibMemset(&preedit, 0, sizeof(preedit));
				preedit.param.text.caretIndex = caret;
				preedit.param.text.str = g_ime.pText;
				preedit.id = SCE_IME_EVENT_UPDATE_TEXT;
				preedit.param.text.preeditIndex = 0;
				preedit.param.text.preeditLength = 0;
				g_ime.handler(arg, &preedit);
				g_ime.preeditLength = 0;
				id = pCall->id;
			}
			event.id = id;
			g_ime.handler(arg, &event);
		} else if (id > 0xFF) {
			switch (id) {
			case IME_VSH_EVENT_TEXT:
				g_ime.preeditIndex = g_ime.caretIndex;
				g_ime.composeState = 1;
				ImeUpdatePreedit(arg, pCall);
				break;
			case IME_VSH_EVENT_INSERT:
			case IME_VSH_EVENT_INSERT_SPACE:
				inserted = ImeInsertText(g_ime.pText, g_ime.maxTextLength, g_ime.caretIndex,
					(const SceWChar16 *)pCall->param.value[0]);
				oldCaret = g_ime.caretIndex;
				ImeCopyText(g_ime.pShadowText, g_ime.maxTextLength, g_ime.pText, g_ime.maxTextLength);
				g_ime.caretIndex += inserted;
				g_ime.preeditIndex = g_ime.caretIndex;
				sceClibMemset(&event, 0, sizeof(event));
				event.param.text.caretIndex = g_ime.caretIndex;
				event.param.text.str = g_ime.pText;
				event.id = SCE_IME_EVENT_UPDATE_TEXT;
				event.param.text.preeditIndex = 0;
				event.param.text.preeditLength = 0;
				event.param.text.editIndex = oldCaret;
				event.param.text.editLengthChange = inserted;
				g_ime.handler(arg, &event);
				g_ime.updated = 1;
				break;
			case IME_VSH_EVENT_BACKSPACE:
				if (g_ime.caretIndex != 0) {
					ImeRemoveText(g_ime.pShadowText, g_ime.maxTextLength, g_ime.caretIndex - 1, 1);
					ImeCopyText(g_ime.pText, g_ime.maxTextLength, g_ime.pShadowText, g_ime.maxTextLength);
					g_ime.caretIndex--;
					g_ime.preeditIndex = g_ime.caretIndex;
					sceClibMemset(&event, 0, sizeof(event));
					event.param.text.caretIndex = g_ime.caretIndex;
					event.param.text.str = g_ime.pText;
					event.id = SCE_IME_EVENT_UPDATE_TEXT;
					event.param.text.preeditIndex = 0;
					event.param.text.preeditLength = 0;
					event.param.text.editIndex = g_ime.caretIndex;
					event.param.text.editLengthChange = -1;
					g_ime.handler(arg, &event);
					g_ime.updated = 1;
				}
				break;
			case IME_VSH_EVENT_CLEAR_BEFORE:
				if (g_ime.caretIndex != 0) {
					ImeRemoveText(g_ime.pShadowText, g_ime.maxTextLength, 0, g_ime.caretIndex);
					ImeCopyText(g_ime.pText, g_ime.maxTextLength, g_ime.pShadowText, g_ime.maxTextLength);
					g_ime.caretIndex = 0;
					g_ime.preeditIndex = 0;
					sceClibMemset(&event, 0, sizeof(event));
					event.param.text.caretIndex = 0;
					event.param.text.str = g_ime.pText;
					event.id = SCE_IME_EVENT_UPDATE_TEXT;
					event.param.text.preeditIndex = 0;
					event.param.text.preeditLength = 0;
					event.param.text.editIndex = caret;
					event.param.text.editLengthChange = caret;
					g_ime.handler(arg, &event);
					g_ime.updated = 1;
				}
				break;
			case IME_VSH_EVENT_CLEAR:
				length = ImeTextLength(g_ime.pText, g_ime.maxTextLength);
				inserted = ImeTextLength(g_ime.pShadowText, g_ime.maxTextLength);
				if (length != 0) {
					oldCaret = g_ime.caretIndex;
					ImeRemoveText(g_ime.pText, length, 0, length);
					ImeRemoveText(g_ime.pShadowText, inserted, 0, inserted);
					g_ime.caretIndex = 0;
					g_ime.preeditIndex = 0;
					g_ime.preeditLength = 0;
					sceClibMemset(&event, 0, sizeof(event));
					event.param.text.str = g_ime.pText;
					event.param.text.preeditIndex = 0;
					event.id = SCE_IME_EVENT_UPDATE_TEXT;
					event.param.text.preeditLength = 0;
					event.param.text.caretIndex = 0;
					event.param.text.editIndex = oldCaret;
					event.param.text.editLengthChange = -length;
					g_ime.handler(arg, &event);
					g_ime.composeState = 0;
					g_ime.updated = 1;
				}
				break;
			case IME_VSH_EVENT_REPLACE:
				length = ImeTextLength(g_ime.pShadowText, g_ime.maxTextLength);
				inserted = ImeTextLength((const SceWChar16 *)pCall->param.value[0], IME_PREEDIT_LENGTH);
				if (length != 0 && length + inserted != g_ime.caretIndex) {
					ImeRemoveText(g_ime.pShadowText, g_ime.maxTextLength, g_ime.preeditIndex, 1);
					ImeCopyText(g_ime.pText, g_ime.maxTextLength, g_ime.pShadowText, g_ime.maxTextLength);
					if (inserted != 0)
						ImeInsertText(g_ime.pText, g_ime.textBufferLength, g_ime.preeditIndex,
							(const SceWChar16 *)pCall->param.value[0]);
					oldCaret = g_ime.caretIndex;
					g_ime.preeditIndex = g_ime.caretIndex - inserted;
					sceClibMemset(&event, 0, sizeof(event));
					event.param.text.caretIndex = oldCaret;
					event.param.text.str = g_ime.pText;
					event.id = SCE_IME_EVENT_UPDATE_TEXT;
					event.param.text.preeditIndex = g_ime.preeditIndex;
					event.param.text.preeditLength = g_ime.preeditLength;
					event.param.text.editIndex = oldCaret;
					event.param.text.editLengthChange = -1;
					g_ime.handler(arg, &event);
					g_ime.updated = 1;
				}
				break;
			default:
				__builtin_trap();
			}
		}
	} else if (id != 0) {
		if (id > 2 && g_ime.sdkVersion >= 0x3100000) {
			event.id = pCall->id;
			event.param.rect.x = pCall->param.value[0];
			event.param.rect.y = pCall->param.value[1];
			event.param.rect.width = pCall->param.value[2];
			event.param.rect.height = pCall->param.value[3];
			g_ime.handler(arg, &event);
		}
	} else if (g_ime.firstEvent == 0) {
		event.id = SCE_IME_EVENT_OPEN;
		event.param.rect.x = pCall->param.value[0];
		event.param.rect.y = pCall->param.value[1];
		event.param.rect.width = pCall->param.value[2];
		event.param.rect.height = pCall->param.value[3];
		g_ime.handler(arg, &event);
		g_ime.preeditIndex = ImeTextLength(g_ime.pText, g_ime.maxTextLength);
		g_ime.caretIndex = g_ime.preeditIndex;
		sceClibMemset(&event, 0, sizeof(event));
		event.param.text.caretIndex = g_ime.preeditIndex;
		event.param.text.str = g_ime.pText;
		event.id = SCE_IME_EVENT_UPDATE_TEXT;
		event.param.text.preeditIndex = 0;
		event.param.text.preeditLength = 0;
		event.param.text.editIndex = 0;
		event.param.text.editLengthChange = 0;
		g_ime.handler(arg, &event);
		g_ime.updated = 1;
		g_ime.firstEvent = 1;
	}
}

/* what the layer calls its handler with: during a preedit only some events reach the application */
static void ImeHandleEvent(void *arg, const ImeVshCall *pCall)
{
	SceImeEvent event;
	SceUInt32 id;
	SceUInt32 index;
	SceUInt32 inserted;
	SceUInt32 length;

	if (g_ime.composeState > 0) {
		if (g_ime.composeState > 1)
			return;
		sceClibMemset(&event, 0, sizeof(event));
		id = pCall->id;
		if (id > 0xFF) {
			if (id <= 0x106) {
				switch (id) {
				case IME_VSH_EVENT_TEXT:
					ImeUpdatePreedit(arg, pCall);
					return;
				case IME_VSH_EVENT_INSERT:
					ImeCopyText(g_ime.pText, g_ime.maxTextLength, g_ime.pShadowText, g_ime.maxTextLength);
					inserted = ImeInsertText(g_ime.pText, g_ime.maxTextLength, g_ime.preeditIndex,
						(const SceWChar16 *)pCall->param.value[0]);
					index = g_ime.preeditIndex;
					if (inserted + g_ime.preeditIndex < g_ime.caretIndex)
						g_ime.caretIndex = inserted + g_ime.preeditIndex;
					ImeCopyText(g_ime.pShadowText, g_ime.maxTextLength, g_ime.pText, g_ime.maxTextLength);
					length = ImeTextLength(g_ime.pText, g_ime.maxTextLength);
					if (g_ime.caretIndex > length)
						g_ime.caretIndex = length;
					g_ime.preeditIndex = index;
					g_ime.preeditLength = 0;
					sceClibMemset(&event, 0, sizeof(event));
					event.param.text.caretIndex = g_ime.caretIndex;
					event.param.text.str = g_ime.pText;
					event.id = SCE_IME_EVENT_UPDATE_TEXT;
					event.param.text.preeditIndex = 0;
					event.param.text.preeditLength = 0;
					event.param.text.editIndex = index;
					event.param.text.editLengthChange = inserted;
					g_ime.handler(arg, &event);
					g_ime.updated = 1;
					g_ime.composeState = 0;
					return;
				case IME_VSH_EVENT_INSERT_SPACE:
				case IME_VSH_EVENT_BACKSPACE:
				case IME_VSH_EVENT_CLEAR_BEFORE:
				case IME_VSH_EVENT_CLEAR:
				case IME_VSH_EVENT_REPLACE:
					break;
				default:
					__builtin_trap();
				}
			}
		} else if (id == 0) {
			return;
		}
		ImeProcessEvent(arg, pCall);
	} else if (g_ime.composeState >= 0) {
		ImeProcessEvent(arg, pCall);
	}
}

static inline SceUInt32 ImeTextLengthInline(const SceWChar16 *pText, SceUInt32 max)
{
	SceUInt32 length;
	const SceWChar16 *p;

	if (pText == NULL)
		return 0;
	length = 0;
	if (max != 0) {
		p = pText;
		while (*p != 0) {
			length++;
			p++;
			if (length >= max) {
				length = max;
				break;
			}
		}
	} else {
		length = max;
	}
	return length;
}

static SceUInt32 ImeTextLength(const SceWChar16 *pText, SceUInt32 max)
{
	SceUInt32 length;
	const SceWChar16 *p;

	if (pText == NULL)
		return 0;
	length = 0;
	if (max != 0) {
		for (p = pText; *p != 0; p++) {
			if (++length >= max) {
				length = max;
				break;
			}
		}
	} else {
		length = max;
	}
	return length;
}

/* the text put into another at an index: the number of characters taken (as many as fit) */
static SceUInt32 ImeInsertText(SceWChar16 *pText, SceUInt32 capacity, SceUInt32 index, const SceWChar16 *pInsert)
{
	SceUInt32 length;
	SceUInt32 count;
	SceUInt32 total;
	SceInt32 i;
	SceUInt32 n;
	const SceWChar16 *pSrc;
	SceWChar16 *pDst;

	length = ImeTextLengthInline(pText, capacity);
	count = ImeTextLengthInline(pInsert, capacity);
	total = length + count;
	if (total > capacity) {
		count = capacity - length;
		total = capacity;
	}
	i = total - 1;
	if (i >= (SceInt32)(index + count)) {
		pSrc = &pText[i - count];
		pDst = &pText[i];
		do {
			*pDst-- = *pSrc--;
			i--;
		} while (i >= (SceInt32)(index + count));
	}
	if (count != 0) {
		pDst = &pText[index];
		pSrc = pInsert;
		for (n = count; n != 0; n--)
			*pDst++ = *pSrc++;
	}
	pText[total] = 0;
	return count;
}

static int ImeCopyText(SceWChar16 *pDst, SceUInt32 capacity, const SceWChar16 *pSrc, SceUInt32 max)
{
	SceUInt32 length;
	SceUInt32 count;
	const SceWChar16 *s;
	SceWChar16 *d;
	SceUInt32 n;

	if (pDst == NULL || pSrc == NULL)
		return IME_TEXT_ERROR_POINTER;
	length = ImeTextLengthInline(pSrc, capacity);
	count = length;
	if (count > max)
		count = max;
	if (count != 0) {
		s = pSrc;
		d = pDst;
		for (n = count; n != 0; n--)
			*d++ = *s++;
	}
	pDst[count] = 0;
	return 0;
}

static int ImeRemoveText(SceWChar16 *pText, SceUInt32 capacity, SceUInt32 index, SceUInt32 count)
{
	SceUInt32 length;
	SceUInt32 n;
	const SceWChar16 *pSrc;
	SceWChar16 *pDst;

	if (count + index > capacity)
		return IME_TEXT_ERROR_RANGE;
	length = ImeTextLengthInline(pText, capacity);
	if (index < length) {
		pSrc = &pText[count + index];
		pDst = &pText[index];
		n = length - index;
		do {
			*pDst++ = *pSrc++;
			n--;
		} while (n != 0);
	}
	pText[length - count] = 0;
	return 0;
}

static void ImeVshCreateMutex(void)
{
	if (g_vshMutexCreated == 0) {
		g_vshOpened = 0;
		sceKernelCreateLwMutex(&g_vshMutex, "SceImeVshMutex", 0x2002, 0, NULL);
		g_vshMutexCreated = 1;
	}
}

static void ImeVshDeleteMutex(void)
{
	if (g_vshMutexCreated != 0) {
		g_vshMutexCreated = 0;
		sceKernelDeleteLwMutex(&g_vshMutex);
	}
}

/* the layer's own check of the 64 byte parameter, and what the service is sent from it */
static int ImeVshCheckParam(ImeOpenRequest *pOut, const ImeVshParam *pParam)
{
	SceUInt32 i;
	SceUInt32 sdkVersion;
	SceUInt64 unsupported;

	if (pParam->size != 64)
		return SCE_IME_ERROR_INVALID_SIZE;
	if (pParam->work == NULL)
		return SCE_IME_ERROR_INVALID_WORK;
	unsupported = pParam->supportedLanguages;
	if (((unsupported ^ 0x300000FFFFFULL) & unsupported) != 0)
		return SCE_IME_ERROR_INVALID_SUPPORTED_LANGUAGES;
	if (pParam->languagesForced > 1)
		return SCE_IME_ERROR_INVALID_PARAM;
	if (pParam->type > 5 && (pParam->type < 0x100 || pParam->type > 0x105))
		return SCE_IME_ERROR_INVALID_TYPE;
	if (pParam->handler == NULL)
		return SCE_IME_ERROR_INVALID_HANDLER;
	if (((SceUInt32)pParam->work & 3) != 0)
		return SCE_IME_ERROR_INVALID_WORK;
	if ((SceUInt32)pParam->enterLabel > 3)
		return SCE_IME_ERROR_INVALID_ENTER_LABEL;
	pOut->languagesForced = pParam->languagesForced;
	pOut->type = pParam->type;
	pOut->supportedLanguages = pParam->supportedLanguages;
	pOut->option = pParam->option;
	pOut->enterLabel = pParam->enterLabel;
	pOut->flag = pParam->flag;
	sdkVersion = pParam->sdkVersion;
	if (sdkVersion == 0) {
		pOut->flags |= 1;
		sdkVersion = 0x3650011;
	}
	pOut->sdkVersion = sdkVersion;
	for (i = 0; i < 2; i++) {
		if (pParam->reserved1[0] != 0)
			return SCE_IME_ERROR_INVALID_RESERVED;
	}
	for (i = 0; i < 12; i++) {
		if (pParam->reserved2[0] != 0)
			return SCE_IME_ERROR_INVALID_RESERVED;
	}
	return 0;
}

int SceImeVsh_2765866F(ImeVshHandle *pHandle, const ImeVshParam *pParam)
{
	ImeOpenRequest request;
	ImeVshParam param;
	ImeAppStatus status;
	int ret;
	int result;

	result = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	ret = result;
	if (result >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (IME_VSH_ALLOWED(status)) {
					result = 0;
					if (pParam->size == 48) {
						sceClibMemset(&param, 0, sizeof(param));
						sceClibMemcpy_safe(&param, pParam, 48);
						param.size = 64;
						param.flag = 0;
						param.sdkVersion = 0x1030000;
					} else {
						sceClibMemcpy_safe(&param, pParam, sizeof(param));
					}
					ret = ImeVshCheckParam(&request, &param);
					if (ret >= 0) {
						sceClibMemcpy_safe(&pHandle->param, &param, sizeof(param));
						pHandle->pArg = param.arg;
						pHandle->handler = param.handler;
						pHandle->filter = param.filter;
						ret = pHandle->pClient->pVtbl->invokeSyncMethod(pHandle->pClient, IME_CMD_OPEN,
							&request, sizeof(request), &result, NULL, NULL, 0);
						if (ret >= 0) {
							sceClibMemset(pHandle->preedit, 0, sizeof(pHandle->preedit));
							pHandle->skipToOpenEvent = 1;
							ret = result;
						} else {
							result = ret;
						}
					} else {
						result = ret;
					}
				} else {
					ret = 0;
					result = 0;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
				result = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
			result = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

/* connect to the service with a method type and a budget for it */
static int ImeVshConnect(ImeIpmiClient **ppClient, int type, int budget)
{
	ImeIpmiConfig config;
	ImeIpmiClient *pClient;
	int result;
	int ret;
	int error;
	int i;

	config.syncCallCount = 1;
	config.asyncCallCount = 1;
	config.maxSyncCallInSize = 3840;
	config.maxSyncCallOutSize = 3840;
	config.maxAsyncCallInSize = 3840;
	config.maxAsyncCallOutSize = 3840;
	config.eventFlagCount = 1;
	config.msgQueueCount = 0;
	for (i = 0; i < 32; i++)
		config.msgQueueBufferSize[i] = 0;
	config.budget = 0;
	config.openLimit = -1;
	config.serviceName[0] = 0;
	result = 0;
	*ppClient = NULL;
	sceClibMemset(config.serviceName, 0, 16);
	sceClibMemcpy_safe(config.serviceName, "SceImeService", 14);
	config.budget = 2;
	if (budget != 0)
		config.maxAsyncCallOutSize = budget;
	if (SceIpmi_4E255C31(&config) <= 0x200) {
		ret = SceIpmi_B282B430(&pClient, &config, NULL, g_ipmiMemory);
		if (ret == 0) {
			i = type;
			error = pClient->pVtbl->connect(pClient, &i, 4, &result);
			if (error != 0) {
				ret = SCE_IME_ERROR_INTERNAL;
				if (error > IME_IPMI_ERROR_BUSY) {
					if (error == IME_IPMI_ERROR_UNAVAILABLE)
						ret = IME_VSH_ERROR_SERVICE_REFUSED;
				} else if (error > IME_IPMI_ERROR_REFUSED) {
					ret = IME_VSH_ERROR_BUSY;
				}
				pClient->pVtbl->destroy(pClient);
			} else {
				*ppClient = pClient;
				ret = 0;
			}
		}
	} else {
		ret = SCE_IME_ERROR_NO_MEMORY;
	}
	return ret;
}

int SceImeVsh_443E963A(ImeVshHandle **ppHandle, const ImeVshParam *pParam)
{
	ImeOpenRequest request;
	ImeVshParam param;
	ImeAppStatus status;
	ImeVshHandle *pHandle;
	int intercept;
	int ret;

	intercept = 0;
	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			ret = SCE_IME_ERROR_ALREADY_OPENED;
		} else {
			if (pParam->size == 48) {
				sceClibMemset(&param, 0, sizeof(param));
				sceClibMemcpy_safe(&param, pParam, 48);
				param.size = 64;
				param.flag = 0;
				param.sdkVersion = 0x1030000;
			} else {
				sceClibMemcpy_safe(&param, pParam, sizeof(param));
			}
			ret = ImeVshCheckParam(&request, &param);
			if (ret >= 0) {
				pHandle = (ImeVshHandle *)param.work;
				pHandle->pSelf = pHandle;
				sceClibMemset(pHandle, 0, 0x2800);
				sceClibMemcpy_safe(&pHandle->param, &param, sizeof(param));
				if (ImeVshConnect(&pHandle->pClient, 1, 0) >= 0) {
					pHandle->sessionId = 0;
					pHandle->pArg = param.arg;
					pHandle->handler = param.handler;
					pHandle->filter = param.filter;
					pHandle->requestPending = 0;
					pHandle->preeditLength = 0;
					g_vshOpened = 1;
					pHandle->openEventSeen = 0;
					pHandle->skipToOpenEvent = 1;
					pHandle->numEvents = 0;
					pHandle->spacePending = 0;
					pHandle->afterSpace = 0;
					pHandle->dirty = 0;
					sceClibMemset(&pHandle->update, 0, sizeof(pHandle->update));
					if (g_vshOpened != 0) {
						if (pHandle != NULL) {
							if (IME_VSH_ALLOWED(status))
								ret = 0;
							else
								ret = 0;
						} else {
							ret = IME_VSH_ERROR_INVALID_HANDLE;
						}
					} else {
						ret = IME_VSH_ERROR_NOT_OPENED;
					}
					sceCtrlGetButtonIntercept(&intercept);
					if (intercept == 0) {
						pHandle->buttonIntercepted = 1;
						sceCtrlSetButtonIntercept(1);
						sceHidKeyboardSetIntercept(1);
					}
					*ppHandle = pHandle;
				} else {
					ret = SCE_IME_ERROR_CONNECTION_FAILED;
				}
			}
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

/* one event of the service, as the application's handler gets it */
static void ImeVshDispatch(ImeVshHandle *pHandle, ImeVshEvent *pEvent)
{
	ImeVshCall call;
	ImeVshCallParam param;
	SceWChar16 filtered[122];
	SceUInt32 filteredLength;
	SceWChar16 *pEventText;
	const SceWChar16 *pText;
	SceUInt32 i;
	SceUInt32 last;
	int opened;
	int textLength;
	SceImeTextFilter filter;

	opened = 0;
	sceClibMemset(&call, 0, sizeof(call));
	sceClibMemset(&param, 0, sizeof(param));
	call.id = pEvent->id;
	if (call.id > 3) {
		if (pEvent->id > 0xFF) {
			if (pEvent->id <= 0x114) {
				switch (pEvent->id) {
				case IME_VSH_EVENT_TEXT:
					param.value[0] = (SceUInt32)&pEvent->param[0];
					param.value[1] = pEvent->param[16];
					pHandle->preeditLength = param.value[1];
					sceClibMemcpy_safe(pHandle->preedit, pEvent->param, 0x3E);
					break;
				case IME_VSH_EVENT_INSERT:
				case IME_VSH_EVENT_INSERT_SPACE:
					pEventText = (SceWChar16 *)(void *)pEvent->param;
					param.value[0] = (SceUInt32)pEventText;
					filter = pHandle->filter;
					if (filter != NULL) {
						filteredLength = 90;
						textLength = ImeTextLength(pEventText, 30);
						if (filter(filtered, &filteredLength, pEventText, textLength) != 0) {
							if (filteredLength > 90)
								filteredLength = 90;
							sceClibMemcpy_safe(pEventText, filtered, 2 * filteredLength);
							pEventText[filteredLength] = 0;
						}
					}
					if (pEvent->id == IME_VSH_EVENT_INSERT_SPACE) {
						last = 0;
						pText = pEventText;
						for (i = 0; i < 30; i++) {
							if (*pText == 0)
								break;
							last = *pText++;
						}
						pHandle->afterSpace = last == 32;
						pHandle->spacePending = 1;
					}
					sceClibMemset(pHandle->preedit, 0, 0x3E);
					break;
				case IME_VSH_EVENT_BACKSPACE:
				case IME_VSH_EVENT_CLEAR_BEFORE:
					break;
				case IME_VSH_EVENT_CLEAR:
					sceClibMemset(pHandle->preedit, 0, 0x3E);
					break;
				case IME_VSH_EVENT_REPLACE:
					param.value[0] = (SceUInt32)pHandle->preedit;
					param.value[1] = pHandle->preeditLength;
					break;
				case IME_VSH_EVENT_CAPITALIZE:
					if (pHandle->afterSpace == 0)
						return;
					call.id = 0x103;
					break;
				default:
					break;
				}
			}
		} else if (call.id <= 4) {
			*(SceUInt8 *)&param.value[0] = *(const SceUInt8 *)&pEvent->param[0];
		}
	} else {
		if (call.id != 0) {
			if (call.id > 2) {
				param.value[0] = pEvent->param[0];
				param.value[1] = pEvent->param[1];
				param.value[2] = pEvent->param[2];
				param.value[3] = pEvent->param[3];
			}
		} else {
			opened = 1;
			pHandle->openEventSeen = 1;
			param.value[0] = pEvent->param[0];
			param.value[1] = pEvent->param[1];
			param.value[2] = pEvent->param[2];
			param.value[3] = pEvent->param[3];
		}
	}
	call.param = param;
	pHandle->handler(pHandle->pArg, &call);
	if (opened != 0) {
		if (pHandle->preedit[0] != 0) {
			sceClibMemset(&call, 0, sizeof(call));
			call.param.value[0] = (SceUInt32)pHandle->preedit;
			call.id = IME_VSH_EVENT_INSERT;
			pHandle->handler(pHandle->pArg, &call);
		}
		sceClibMemset(pHandle->preedit, 0, 0x3E);
	}
}

int SceImeVsh_F4A4865B(ImeVshHandle *pHandle)
{
	ImeOpenRequest request;
	ImeAppStatus status;
	ImeIpmiClient *pClient;
	ImeVshEvent *pEvent;
	SceSize outSize;
	int result;
	int count;
	int ret;
	int dispatch;

	result = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (g_vshOpened == 0) {
		result = IME_VSH_ERROR_NOT_OPENED;
		goto out;
	}
	if (pHandle == NULL) {
		result = IME_VSH_ERROR_INVALID_HANDLE;
		goto out;
	}
	if (!IME_VSH_ALLOWED(status)) {
		result = 0;
		goto out;
	}
	result = 0;
	sceClibMemset(&request, 0, sizeof(request));
	request.sessionId = pHandle->sessionId;
	ret = ImeVshCheckParam(&request, &pHandle->param);
	if (ret < 0) {
		result = ret;
		goto out;
	}
	pClient = pHandle->pClient;
	if (pHandle->dirty != 0) {
		ret = pClient->pVtbl->invokeSyncMethod(pClient, IME_CMD_UPDATE, &pHandle->update,
			sizeof(pHandle->update), &result, NULL, NULL, 0);
		if (ret < 0) {
			result = ret;
			goto out;
		}
		sceClibMemset(&pHandle->update, 0, sizeof(pHandle->update));
		pHandle->dirty = 0;
		pClient = pHandle->pClient;
	}
	count = 0;
	if (pHandle->requestPending != 0) {
		ret = pClient->pVtbl->tryGetResult(pClient, pHandle->requestId, &count, pHandle->events,
			&outSize, sizeof(pHandle->events));
		if (ret == IME_IPMI_ERROR_NOT_READY)
			goto polled;
		if (ret < 0) {
			count = ret;
			goto polled;
		}
		pHandle->requestPending = 0;
	}
	ret = pClient->pVtbl->invokeAsyncMethod(pClient, IME_CMD_POLL, &request, sizeof(request),
		&pHandle->requestId, NULL);
	if (ret < 0) {
		ret = ret;
		goto failed;
	}
	pHandle->requestPending = 1;
polled:
	ret = count;
failed:
	if (ret < 0) {
		result = SCE_IME_ERROR_CONNECTION_FAILED;
		goto out;
	}
	pHandle->numEvents = ret;
	pEvent = pHandle->events;
	if (ret > 0) {
		while (1) {
			ImeVshEvent *pCurrent;

			if (g_vshOpened == 0)
				goto out;
			pCurrent = pEvent;
			dispatch = pHandle->skipToOpenEvent;
			pHandle->numEvents = --ret;
			pEvent++;
			if (dispatch != 0) {
				if (pCurrent->id != 0)
					goto next;
				pHandle->skipToOpenEvent = 0;
			}
			ImeVshDispatch(pHandle, pCurrent);
			ret = pHandle->numEvents;
next:
			if (ret <= 0)
				goto out;
		}
	}
out:
	ret = result;
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

int SceImeVsh_269FFDCF(ImeVshHandle *pHandle)
{
	ImeAppStatus status;
	int ret;

	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (IME_VSH_ALLOWED(status)) {
					pHandle->dirty = 1;
					pHandle->update.flags |= 1;
					ret = 0;
					pHandle->update.kind = 1;
				} else {
					ret = 0;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

int SceImeVsh_8BE1872C(ImeVshHandle *pHandle)
{
	ImeAppStatus status;
	int ret;

	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (IME_VSH_ALLOWED(status)) {
					pHandle->update.flags |= 1;
					pHandle->dirty = 1;
					ret = 0;
					pHandle->update.kind = 2;
				} else {
					ret = 0;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

int SceImeVsh_F27C99D3(ImeVshHandle *pHandle, const SceUInt32 *pCaret)
{
	ImeAppStatus status;
	int ret;

	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (IME_VSH_ALLOWED(status)) {
					pHandle->update.caretX = pCaret[0];
					ret = 0;
					pHandle->update.caretY = pCaret[1];
					pHandle->update.flags |= 8;
					pHandle->update.caretHeight = pCaret[2];
					pHandle->dirty = 1;
				} else {
					ret = 0;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

static int ImeVshSetPreeditGeometry(ImeVshHandle *pHandle, const SceUInt32 *pGeometry)
{
	ImeAppStatus status;
	int ret;

	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (IME_VSH_ALLOWED(status)) {
					pHandle->update.preeditX = pGeometry[0];
					ret = 0;
					pHandle->update.preeditY = pGeometry[1];
					pHandle->update.flags |= 0x10;
					pHandle->update.preeditHeight = pGeometry[2];
					pHandle->dirty = 1;
				} else {
					ret = 0;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

/* the text and the caret in it: whether the next text starts a sentence (capitalize) */
int SceImeVsh_9AEEC1BD(ImeVshHandle *pHandle, const SceWChar16 *pText, SceUInt32 caretIndex)
{
	ImeAppStatus status;
	const SceWChar16 *p;
	int ret;
	int index;
	int capitalize;
	int c;

	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (IME_VSH_ALLOWED(status)) {
					index = caretIndex - 1;
					ret = 0;
					if (pHandle->spacePending != 0)
						pHandle->spacePending = 0;
					else
						pHandle->afterSpace = 0;
					if (caretIndex != 0 && pText[index] != 32)
						pHandle->afterSpace = 0;
					capitalize = 1;
					if (index >= 0) {
						p = &pText[index];
						do {
							c = *p;
							if (c == 191 || c == 63 || c == 46 || c == 33)
								break;
							if (c != 32) {
								if (c != 13 && c != 10)
									capitalize = 0;
								break;
							}
							index--;
							p--;
						} while (index >= 0);
					}
					pHandle->update.capitalize = capitalize;
					pHandle->update.flags |= 2;
					pHandle->dirty = 1;
				} else {
					ret = 0;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

int SceImeVsh_684C4574(ImeVshHandle *pHandle, SceUInt32 index)
{
	ImeAppStatus status;
	int ret;

	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (IME_VSH_ALLOWED(status)) {
					ret = 0;
					if (pHandle->preeditLength != index) {
						pHandle->update.caretIndex = index;
						pHandle->update.flags |= 4;
						pHandle->dirty = 1;
					}
				} else {
					ret = 0;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

int SceImeVsh_DDBE0490(ImeVshHandle *pHandle)
{
	ImeOpenRequest request;
	ImeIpmiClient *pClient;
	SceSize outSize;
	SceUInt8 reply[8];
	int result;
	int ret;
	int error;

	ret = sceKernelLockLwMutex(&g_vshMutex, 1, NULL);
	result = ret;
	if (ret >= 0) {
		if (g_vshOpened != 0) {
			if (pHandle != NULL) {
				if (pHandle->buttonIntercepted != 0) {
					pHandle->buttonIntercepted = 0;
					sceCtrlSetButtonIntercept(0);
					sceHidKeyboardSetIntercept(0);
				}
				sceClibMemset(&request, 0, sizeof(request));
				request.sessionId = pHandle->sessionId;
				error = pHandle->pClient->pVtbl->invokeSyncMethod(pHandle->pClient, IME_CMD_CLOSE,
					&request, sizeof(request), &result, NULL, NULL, 0);
				if (error >= 0) {
					pClient = pHandle->pClient;
					if (pHandle->requestPending != 0) {
						while ((error = pHandle->pClient->pVtbl->tryGetResult(pHandle->pClient,
							    pHandle->requestId, (int *)reply, pHandle->events, &outSize,
							    sizeof(pHandle->events))) == IME_IPMI_ERROR_NOT_READY)
							sceKernelDelayThread(1000);
						pClient = pHandle->pClient;
						pHandle->requestPending = 0;
					}
					if (pClient->pVtbl->disconnect(pClient) < 0 || pClient->pVtbl->destroy(pClient) < 0)
						result = SCE_IME_ERROR_CONNECTION_FAILED;
					sceClibMemset(pHandle, 0, sizeof(ImeVshHandle));
					ret = result;
					g_vshOpened = 0;
				} else {
					result = error;
					ret = error;
				}
			} else {
				ret = IME_VSH_ERROR_INVALID_HANDLE;
				result = IME_VSH_ERROR_INVALID_HANDLE;
			}
		} else {
			ret = IME_VSH_ERROR_NOT_OPENED;
			result = IME_VSH_ERROR_NOT_OPENED;
		}
	}
	sceKernelUnlockLwMutex(&g_vshMutex, 1);
	return ret;
}

int SceImeVsh_4D7F8416(int a0, int a1, int a2, int a3)
{
	SceUInt32 request[6];
	ImeIpmiClient *pClient;
	int result;
	int ret;
	int error;

	ret = ImeVshConnect(&pClient, 2, 0);
	if (ret >= 0) {
		request[0] = 0;
		request[4] = a2;
		request[5] = a3;
		request[2] = a0;
		request[3] = a1;
		error = pClient->pVtbl->invokeSyncMethod(pClient, 20, request, sizeof(request), &result, NULL, NULL, 0);
		if (error >= 0)
			error = result;
		ret = 0;
		if (error < 0)
			ret = error;
		error = pClient->pVtbl->disconnect(pClient);
		if (error < 0 || (error = pClient->pVtbl->destroy(pClient)) < 0)
			ret = error;
	}
	return ret;
}

int SceImeVsh_A41DB14B(int a0, int a1, int a2, void *pOut)
{
	SceUInt32 request[6];
	ImeIpmiClient *pClient;
	SceSize outSize;
	int result;
	int ret;
	int error;

	ret = ImeVshConnect(&pClient, 2, 0);
	if (ret >= 0) {
		request[4] = a2;
		error = 0;
		request[0] = 1;
		request[2] = a0;
		request[3] = a1;
		request[5] = 0;
		ret = pClient->pVtbl->invokeSyncMethod(pClient, 20, request, sizeof(request), &result, pOut, &outSize, 4);
		if (ret >= 0) {
			if (outSize == 4)
				ret = result;
			else
				ret = IME_VSH_ERROR_BAD_REPLY;
		}
		if (ret < 0)
			error = ret;
		ret = pClient->pVtbl->disconnect(pClient);
		if (ret < 0 || (ret = pClient->pVtbl->destroy(pClient)) < 0)
			error = ret;
		ret = error;
	}
	return ret;
}

/* one call of the service that waits for its reply */
static int ImeVshInvoke(int type, int method, const void *pIn, int inSize, int *pResult, void *pOut,
	SceSize *pOutSize, SceSize outMax)
{
	ImeIpmiClient *pClient;
	int requestId;
	int ret;
	int error;

	ret = ImeVshConnect(&pClient, type, 0x2000);
	if (ret >= 0) {
		ret = pClient->pVtbl->invokeAsyncMethod(pClient, method, pIn, inSize, &requestId, NULL);
		if (ret >= 0) {
			while ((error = pClient->pVtbl->tryGetResult(pClient, requestId, pResult, pOut, pOutSize,
				    outMax)) != 0) {
				if (error != IME_IPMI_ERROR_NOT_READY) {
					ret = error;
					goto done;
				}
				sceKernelDelayThread(1000);
			}
			ret = 0;
done:
			error = pClient->pVtbl->disconnect(pClient);
			if (error < 0 || (error = pClient->pVtbl->destroy(pClient)) < 0)
				ret = error;
		}
	}
	return ret;
}

int SceImeVsh_7EC113FA(int a0, int a1, void *pOut)
{
	int result;
	SceSize outSize[1];
	SceUInt32 request[2];
	int ret;

	if (g_vshOpened == 1)
		return IME_VSH_ERROR_BUSY;
	request[0] = a0;
	request[1] = a1;
	result = 0;
	ret = ImeVshInvoke(3, 33, request, sizeof(request), &result, pOut, outSize, 5200);
	if (ret < 0)
		result = ret;
	return result;
}

int SceImeVsh_3DA671DD(int a0, int a1, const short *pData)
{
	struct {
		SceUInt32 a0;
		SceUInt32 a1;
		short data[52];
	} request;
	int result;
	int ret;
	int i;

	result = 0;
	if (g_vshOpened == 1)
		return IME_VSH_ERROR_BUSY;
	request.a0 = a0;
	request.a1 = a1;
	for (i = 0; i < 52; i++)
		request.data[i] = pData[i];
	ret = ImeVshInvoke(3, 34, &request, sizeof(request), &result, NULL, NULL, 0);
	if (ret < 0)
		return ret;
	return result;
}

int SceImeVsh_FD1A16DF(void)
{
	int result;
	int ret;

	if (g_vshOpened == 1)
		return IME_VSH_ERROR_BUSY;
	result = 0;
	ret = ImeVshInvoke(3, 37, NULL, 0, &result, NULL, NULL, 0);
	if (ret < 0)
		return ret;
	return result;
}

int SceImeVsh_BE92A5B3(void)
{
	int result;
	int ret;

	if (g_vshOpened == 1)
		return IME_VSH_ERROR_BUSY;
	result = 0;
	ret = ImeVshInvoke(3, 38, NULL, 0, &result, NULL, NULL, 0);
	if (ret < 0)
		return ret;
	return result;
}
