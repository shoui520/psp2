/*
 * ScePocsDialogPlugin (vs0:sys/external/libcdlg_pocketstation.suprx):
 * plugin entry, work memory layout, and callback declarations.
 */
#ifndef POCKETSTATION_DIALOG_PLUGIN_H
#define POCKETSTATION_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <plugin.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

#define POCKETSTATION_DIALOG_ERROR_NO_WORK_MEMORY	(-2146399358)	/* 0x80108B82 */
#define POCKETSTATION_DIALOG_STATE_RUNNING		1
#define POCKETSTATION_DIALOG_STATE_RESUME		7

struct PocketStationDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceUInt32 openMode;
	SceUInt32 activeFlags;
	SceUInt32 backgroundIndex;
	SceUInt32 correspondentCount;
	SceUInt8 dialogPayload[80];
};

struct PocketStationDialogResult {
	SceInt32 result;
	SceUInt32 selectedAction;
	SceUInt32 selectedIndex;
};

struct CommonDialogPocketStationParam {
	SceInt32 abortRequested;
	SceInt32 abortResult;
	SceInt32 state;
	PocketStationDialogParam param;
	PocketStationDialogResult result;
	char apiName[64];
};

typedef void (*PocketStationCloseCallback)(SceInt32 result);

extern paf::Plugin *g_pocketStationPlugin;

#endif /* POCKETSTATION_DIALOG_PLUGIN_H */
