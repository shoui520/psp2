/*
 * patch_chk_plugin.h: public plugin entry points and interface table for
 * patch_chk_plugin (0x812AB958-0x812B1B63).
 */
#ifndef SHELL_PATCH_CHK_PLUGIN_H
#define SHELL_PATCH_CHK_PLUGIN_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>

/* Patch check finish result payload passed to PatchChkFinishCallback (8 bytes) */
struct PatchChkResultData {
	SceUInt32 infoFlags;
	SceInt32 updateStatus;
};

typedef void (*PatchChkFinishCallback)(int result, const PatchChkResultData *resultData,
				       void *userData);
typedef void (*PatchChkStateCallback)(int state, void *userData);

/* Parameters passed by value to PatchChkInterface::startCheck (108 = 0x6C bytes) */
struct PatchChkStartParam {
	SceUInt16 checkMode;		/* 0: interactive LiveArea/topmenu, 1: quiet/bg check */
	char titleId[13];
	char currentAppVer[7];
	char contentId[48];
	SceUInt16 reservedPad;
	SceUInt32 currentPsp2SysVer;
	SceUInt32 currentPspSysVer;
	SceInt32 targetPlatform;	/* 0: default, 1: PSP2, 2: PSP */
	char patchTitleId[13];
	SceUInt8 titleIdPad[3];
	SceInt32 presetStatus;		/* -1: run live check, else skip directly to status */
	SceUInt32 extraOption;
};

typedef char PatchChkStartParamSizeCheck[sizeof(PatchChkStartParam) == 0x6C ? 1 : -1];

/* patch_chk_plugin GetInterface(1) function table (0x81545B54) */
struct PatchChkInterface {
	int (*startCheck)(PatchChkStartParam param,
			  PatchChkFinishCallback finishCallback, void *userData);
	int (*setStateCallback)(PatchChkStateCallback callback, void *userData);
	void (*clearStateCallback)(void);
	void (*cancel)(void);
	void (*handleEvent)(int eventId);
};

void patchChkPluginSetParam(paf::Plugin::InitParam2 *param);
void patchChkPluginInit(paf::Plugin *plugin);
void patchChkPluginStart(paf::Plugin *plugin);
void patchChkPluginStop(paf::Plugin *plugin);
void patchChkPluginExit(paf::Plugin *plugin);

#endif
