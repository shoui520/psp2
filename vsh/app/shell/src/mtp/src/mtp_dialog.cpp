/*
 * SceShell mtp: USB error and busy dialogs
 * (0x811CA094 - 0x811CA5A2)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <framework.h>

extern "C" {
extern int sceClibPrintf(const char *fmt, ...);
extern void SceCommonGuiDialog_696DB9C9(void *param);
}

extern int strongDialogOpenMessageDialog(const void *param, SceUInt32 mode, int flags); /* 0x8124D61C */
extern void strongDialogClose(int dlgId);			/* 0x8124D69A */

static int s_usbError1DlgId = -1;				/* 0x81544E20 */
static int s_usbError2DlgId = -1;				/* 0x81544E24 */
static int s_usbError3DlgId = -1;				/* 0x81544E28 */
static int s_mtpBusyDlgId = -1;					/* 0x81544E2C */

struct MtpUsbErrorDialogListener {
	virtual ~MtpUsbErrorDialogListener() {}
};

struct MtpMessageDialogParam {
	SceUInt8 commonHeader[72];				/* +0 */
	const void *style1;					/* +72 */
	const void *style2;					/* +76 */
	const void *style3;					/* +80 */
	paf::wstring title;					/* +84 */
	paf::wstring message;					/* +96 */
	const void *extra1;					/* +108 */
	const void *extra2;					/* +112 */
	const void *extra3;					/* +116 */
	const void *extra4;					/* +120 */
	float scale;						/* +128 */
	paf::graph::Surface *iconSurface;			/* +132 */
	void *widgetRef;					/* +136 */
	paf::graph::Surface *bgSurface;				/* +140 */
	MtpUsbErrorDialogListener *listener;			/* +144 */
	void *secondaryListener;				/* +148 */
	void *tertiaryListener;					/* +152 */

	MtpMessageDialogParam()
		: style1(NULL), style2(NULL), style3(NULL),
		  extra1(NULL), extra2(NULL), extra3(NULL), extra4(NULL),
		  scale(1.0f), iconSurface(NULL), widgetRef(NULL), bgSurface(NULL),
		  listener(NULL), secondaryListener(NULL), tertiaryListener(NULL)
	{
		SceCommonGuiDialog_696DB9C9(commonHeader);
		commonHeader[68] = 0;
	}

	~MtpMessageDialogParam()
	{
		if (bgSurface != NULL)
			bgSurface->Release();
		if (widgetRef != NULL) {
			typedef void (*ReleaseFn)(void *);
			(*(ReleaseFn **)(*(void **)widgetRef))[7](widgetRef);
		}
		if (iconSurface != NULL)
			iconSurface->Release();
	}
};

/* 0x811CA094 */
void mtpCloseUsbErrorDialogs(void)
{
	if (s_usbError1DlgId != -1) {
		strongDialogClose(s_usbError1DlgId);
		s_usbError1DlgId = -1;
	}
	if (s_usbError2DlgId != -1) {
		strongDialogClose(s_usbError2DlgId);
		s_usbError2DlgId = -1;
	}
	if (s_usbError3DlgId != -1) {
		strongDialogClose(s_usbError3DlgId);
		s_usbError3DlgId = -1;
	}
}

/* 0x811CA0FE */
int openUsbError1Dialog(void)
{
	paf::Framework *fw = paf::Framework::Instance();
	if (fw == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): framework not found\n",
			"DIALOG", "openUsbError1Dialog", 195);
		return -1;
	}

	MtpMessageDialogParam param;
	paf::IDParam msgId(0x76C70719u);
	const wchar_t *msg = fw->GetCommonString(msgId);
	if (msg != NULL)
		param.message = msg;
	param.commonHeader[53] = 1;
	param.listener = new MtpUsbErrorDialogListener();

	int dlgId = strongDialogOpenMessageDialog(&param, 1, 1);
	s_usbError1DlgId = dlgId;
	if (dlgId < 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): StrongDialogOpenMessageDialog failed: 0x%x",
			"DIALOG", "openUsbError1Dialog", 216, dlgId);
		return -1;
	}
	return 0;
}

/* 0x811CA30E */
int mtpShowUsbError1Dialog(void)
{
	mtpCloseUsbErrorDialogs();
	return openUsbError1Dialog();
}

/* 0x811CA37C */
void mtpCloseBusyDialog(void)
{
	if (s_mtpBusyDlgId != -1) {
		strongDialogClose(s_mtpBusyDlgId);
		s_mtpBusyDlgId = -1;
	}
}

/* 0x811CA3A2 */
void mtpOpenBusyDialog(void)
{
	mtpCloseBusyDialog();
	paf::Framework *fw = paf::Framework::Instance();
	if (fw == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): framework not found\n",
			"DIALOG", "OpenMtpBusyDialog", 259);
		return;
	}

	MtpMessageDialogParam param;
	paf::IDParam msgId(0xB9650C36u);
	const wchar_t *msg = fw->GetCommonString(msgId);
	if (msg != NULL)
		param.message = msg;

	int dlgId = strongDialogOpenMessageDialog(&param, 1, 1);
	s_mtpBusyDlgId = dlgId;
	if (dlgId < 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): StrongDialogOpenMessageDialog failed: 0x%x\n",
			"DIALOG", "OpenMtpBusyDialog", 267, dlgId);
	}
}

/* 0x811CA570 */
int mtpIsUsbError2Open(void)
{
	return (s_usbError2DlgId != -1);
}

/* 0x811CA588 */
int mtpIsUsbError1Open(void)
{
	return (s_usbError1DlgId != -1);
}

/* 0x811CA5A0 */
void mtpDialogNoop(void)
{
}
