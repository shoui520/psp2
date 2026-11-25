/*
 * SceShell mtp: USB error and busy dialogs
 * (0x811CA094 - 0x811CA5A2)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <framework.h>
#include <common_gui_dialog/common_gui_dialog.h>

extern "C" {
extern int sceClibPrintf(const char *fmt, ...);
}

extern int strongDialogOpenMessageDialog(const CommonGuiLayoutDialogParam *param,
	SceUInt32 mode, int flags);				/* 0x8124D61C */
extern void strongDialogClose(int dlgId);			/* 0x8124D69A */

static int s_usbError1DlgId = -1;				/* 0x81544E20 */
static int s_usbError2DlgId = -1;				/* 0x81544E24 */
static int s_usbError3DlgId = -1;				/* 0x81544E28 */
static int s_mtpBusyDlgId = -1;					/* 0x81544E2C */

class MtpUsbErrorDialogListener : public CommonGuiDialogHandler {
public:
	virtual ~MtpUsbErrorDialogListener() {}
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)button;
		strongDialogClose(dialogId);
		if (s_usbError1DlgId == dialogId)
			s_usbError1DlgId = -1;
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

	CommonGuiLayoutDialogParam param;
	paf::IDParam msgId(0x76C70719u);
	const wchar_t *msg = fw->GetCommonString(msgId);
	if (msg != NULL)
		param.text = msg;
	param.notifyClose = true;
	param.handler = new MtpUsbErrorDialogListener();

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

	CommonGuiLayoutDialogParam param;
	paf::IDParam msgId(0xB9650C36u);
	const wchar_t *msg = fw->GetCommonString(msgId);
	if (msg != NULL)
		param.text = msg;

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
