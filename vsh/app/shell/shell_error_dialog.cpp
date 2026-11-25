/*
 * shell_error_dialog.cpp: the shell's fatal error dialog (0x810FB388-0x810FB5D4), opened once,
 * whose OK writes the error's kind to the NVS and has the system restart: the start-up error
 * (start.cpp), the accessibility registry's and the database recovery's. No record or string
 * names the file, its functions or its handler: the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string>
#include <kernel/modulemgr.h>
#include <vshbridge/vshbridge.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <shell_svc/shell_util.h>

#include "shell.h"

using namespace paf;

/* the error's kinds as the NVS keeps them, by the dialog's kind */
#define SHELL_ERROR_NVS_AREA		4
#define SHELL_ERROR_NVS_NONE		0xFF

/* (0x8124D646) opens an error dialog; PLACEHOLDER name */
int shellErrorDialogOpenParam(CommonGuiMessageDialogParam *param, bool wait, int flags);

/* what the dialog's OK does: the NVS keeps the error's kind, and the system restarts */
class ShellErrorDialogHandler : public CommonGuiDialogHandler {
public:
	ShellErrorDialogHandler(int type) : m_type(type)
	{
	}

	/* (0x81414CF4) */
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		SceUInt8 kind;
		int ret;

		sce_paf_printf_dummy();
		kind = SHELL_ERROR_NVS_NONE;
		switch (m_type) {
		case 2:
			kind = 3;
			break;
		case 3:
			kind = 5;
			break;
		case 4:
			kind = 9;
			break;
		case 5:
			kind = 17;
			break;
		case 1:
			kind = 1;
			break;
		}
		sce_paf_printf_dummy();
		ret = vshSblSsSetNvsData(SHELL_ERROR_NVS_AREA, &kind, sizeof(kind));
		if (ret < 0)
			sce_paf_printf_dummy();
		sce_paf_printf_dummy();
		ret = SceShellUtil_636544FB(0);
		if (ret < 0)
			sce_paf_printf_dummy();
	}

	int m_type;
};

/* (0x8154477C) the dialog is open: it opens once */
static bool s_opened = false;

/* (0x81559668) */
static wstring s_message;

/* (0x810FB388) */
__attribute__((noinline)) void shellErrorDialogOpen(int type, CommonGuiMessageDialogParam *param, bool loadModule)
{
	if (loadModule)
		sceKernelLoadStartModule("vs0:sys/external/common_gui_dialog.suprx", 0, NULL, 0,
					 NULL, NULL);

	if (param == NULL) {
		sce_paf_printf_dummy();
		return;
	}
	if (s_opened) {
		sce_paf_printf_dummy();
		return;
	}
	s_opened = true;
	sce_paf_printf_dummy();

	param->buttonCount = 1;
	param->dimmer = 2;
	param->useLayout = true;
	param->layout = SceCommonGuiDialog_B04C8C1A;
	param->handler = new ShellErrorDialogHandler(type);
	if (param->title.format.size() == 0 && param->messageId == 0)
		param->messageId = 0xF91B07B7;
	if (type == 0) {
		sce_paf_printf_dummy();
		param->modal = false;
		param->messageId = 0x65E8C179;
	}
	shellErrorDialogOpenParam(param, true, 1);
	sce_paf_printf_dummy();
}

/* (0x810FB45E) */
void shellErrorDialogShow(int type, int error, const wstring *message, bool loadModule)
{
	if (loadModule)
		sceKernelLoadStartModule("vs0:sys/external/common_gui_dialog.suprx", 0, NULL, 0,
					 NULL, NULL);

	CommonGuiMessageDialogParam param;

	param.type = error;
	param.title.format = *message;
	if (error != 0)
		param.defaultButton = 1;
	shellErrorDialogOpen(type, &param, false);
}
