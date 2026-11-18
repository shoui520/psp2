/*
 * sysupchk.h: the shell's loader of the system update check (sysupchk_plugin): its parameters,
 * passed to the plugin as arguments ("check_mode", "dialog_mode", ...), and the plugin's
 * InitParam. The names are ours (no record names them). PLACEHOLDER: the file that defines
 * these (0x813623D8..0x81362AA0) is not written yet.
 */
#ifndef _SHELL_SYSUPCHK_H
#define _SHELL_SYSUPCHK_H

#include <plugin.h>
#include <common_gui_dialog/common_gui_dialog.h>

/* the update check's parameters, 76 bytes (0x813623D8) */
class ShellSysupchkParam {
public:
	ShellSysupchkParam();

	paf::string caller;			/* the plugin that loads the check */
	void (*callback)(int result, void *arg);	/* told the check's result */
	void *reserved;
	void *arg;
	SceInt32 checkMode;
	SceInt32 dialogMode;
	SceInt32 launchMode;
	SceInt32 launchApiType;
	SceInt32 acceptedFlag;
	SceUInt32 requiredVersion;
	paf::string requiredVersionString;
	SceInt32 npCacheOpt;
	SceInt32 npDownloadOpt;
	SceInt32 npCheckLevel;
	CommonGuiLayoutDialogParam *msgDlgParam;	/* its "msg_dlg_param_draw_priority" */
};

/* sysupchk_plugin's InitParam: its name, resource and entry points (0x81362410) */
class ShellSysupchkInitParam : public paf::Plugin::InitParam {
public:
	ShellSysupchkInitParam();

	/* PLACEHOLDER: Sony's copy is the COMDAT at 0x814091D0 (every caller calls it) */
	~ShellSysupchkInitParam();

	/* the parameters as the plugin's arguments, the callback as its notify callback
	   (0x81362476) */
	int SetParam(const ShellSysupchkParam &param);

	/* the plugin loaded with Plugin::LoadAsync (0x81362AA0) */
	int Load(paf::Plugin::LoadFinishCallback callback, int option);
};

/* what the update check asks the shell, and the shell's own answers (0x81547534, 0x8154753C) */
struct ShellSysupchkInterface;
extern ShellSysupchkInterface *shellSysupchkInterface;
extern ShellSysupchkInterface shellSysupchkDefaultInterface;

#endif
