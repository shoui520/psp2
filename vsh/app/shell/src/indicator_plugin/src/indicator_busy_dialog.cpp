/*
 * indicator_busy_dialog.cpp: the indicator's busy dialog, the library's layout dialog with the
 * caller's message, opened and closed on the main thread when asked, never in the PM or IDU
 * modes (0x81182D06-0x81183012). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <thread/thread.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../shell.h"
#include "../../livespace_db/livespace_db_internal.h"
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* the page's priority over the notices */
#define INDICATOR_BUSY_DIALOG_PRIORITY	19

/* the dialog open, below 0 when none is (0x81544CC4) */
static int s_busyDialog = -1;

static int indicatorBusyDialogOpenNow(void *message);
static int indicatorBusyDialogCloseNow(void *arg);

/* (0x81182D06) */
int indicatorBusyDialogOpen(const wchar_t *message, bool sync)
{
	int id = -1;

	if (!shellStartupIsPmMode() && shellStartupGetIduMode() != 1) {
		if (sync)
			thread::SyncCall::main_thread_synccall.Call(indicatorBusyDialogOpenNow,
								    (void *)message);
		else
			indicatorBusyDialogOpenNow((void *)message);
		id = s_busyDialog;
	}
	return id;
}

/* (0x81182E4C) */
int indicatorBusyDialogClose(bool sync)
{
	if (s_busyDialog >= 0) {
		if (sync)
			thread::SyncCall::main_thread_synccall.Call(indicatorBusyDialogCloseNow,
								    NULL);
		else
			indicatorBusyDialogCloseNow(NULL);
	}
	return 0;
}

/* (0x81182E82) */
static int indicatorBusyDialogOpenNow(void *message)
{
	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = g_indicatorPlugin;
	param.layout = SceCommonGuiDialog_2A68B4A4;
	param.priority = INDICATOR_BUSY_DIALOG_PRIORITY;
	param.text = (const wchar_t *)message;
	s_busyDialog = SceCommonGuiDialog_CCFCD9A6(&param);
	return 0;
}

/* (0x81182F8A) */
static int indicatorBusyDialogCloseNow(void *arg)
{
	SceCommonGuiDialog_BC311DC0(s_busyDialog);
	return 0;
}
