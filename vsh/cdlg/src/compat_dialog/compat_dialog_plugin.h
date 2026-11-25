/*
 * SceCompatDialogPlugin (vs0:sys/external/libcdlg_compat.suprx):
 * PSP / PS1 compatibility settings dialog plugin and menu controller.
 */
#ifndef COMPAT_DIALOG_PLUGIN_H
#define COMPAT_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <plugin.h>
#include <widget/w_scene.h>
#include <widget/core/widget.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

#define COMPAT_DIALOG_ERROR_NO_WORK_MEMORY	(-2146412414)	/* 0x80105882 */
#define COMPAT_DIALOG_STATE_RUNNING		1
#define COMPAT_DIALOG_STATE_RESUME		7

struct CompatDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceUInt8 dialogData[160];
	SceUInt8 reserved[32];
};

struct CompatDialogResult {
	SceInt32 result;
	SceUInt8 dialogData[64];
	SceUInt8 reserved[32];
};

struct CommonDialogCompatParam {
	SceInt32 abortRequested;
	SceInt32 abortResult;
	SceInt32 state;
	CompatDialogParam param;
	CompatDialogResult result;
	char apiName[64];
};

typedef void (*CompatDialogDoneCallback)(SceInt32 result, const CompatDialogResult *outResult);

enum CompatMenuSection {
	COMPAT_MENU_TOP = 0,
	COMPAT_MENU_DISC_SWITCH,
	COMPAT_MENU_CONTROLLER,
	COMPAT_MENU_BUTTON_ASSIGN,
	COMPAT_MENU_TOUCH_SCREEN,
	COMPAT_MENU_BACK_TOUCH_PAD,
	COMPAT_MENU_MEMORY_CARD,
	COMPAT_MENU_SCREEN_MODE,
	COMPAT_MENU_OTHER
};

class SceCompatMenuCtrl {
public:
	SceCompatMenuCtrl();
	~SceCompatMenuCtrl();

	SceInt32 Start(paf::Plugin *plugin, const CompatDialogParam *param,
		       CompatDialogDoneCallback callback);
	SceInt32 HandleAbort(bool aborted);
	void Resume();
	void Close();

	void OpenSection(CompatMenuSection section);
	void ApplyButtonAssign(bool customMode);
	void SwitchDisc(SceInt32 discIndex);

private:
	paf::Plugin *m_plugin;
	paf::ui::Scene *m_bgScene;
	paf::ui::Widget *m_topList;
	CompatDialogParam m_param;
	CompatDialogResult m_result;
	CompatDialogDoneCallback m_callback;
	CompatMenuSection m_section;
	SceInt32 m_activeDisc;
	bool m_customAssign;
	bool m_bilinearFilter;
	bool m_open;
};

#endif /* COMPAT_DIALOG_PLUGIN_H */
