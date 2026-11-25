/*
 * SceFriendListDialogPlugin (vs0:sys/external/libcdlg_friendlist.suprx):
 * legacy friend list common dialog plugin.
 */
#ifndef FRIENDLIST_DIALOG_PLUGIN_H
#define FRIENDLIST_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <np/np_common.h>
#include <np_friendlist_dialog.h>
#include <plugin.h>
#include <widget/w_scene.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

struct CommonDialogFriendListParam {
	SceNpFriendListDialogParam param;
	SceNpFriendListDialogResult result;
	SceCommonDialogColor bgColor;
	char apiName[64];
};

class SceFriendListDialogCtrl {
public:
	explicit SceFriendListDialogCtrl(paf::Plugin *plugin);
	~SceFriendListDialogCtrl();

	SceInt32 Start(const CommonDialogFriendListParam *work);
	void Stop();
	void Cancel();

	bool IsActive() const;

private:
	paf::Plugin *m_plugin;
	paf::ui::Scene *m_scene;
	SceNpFriendListDialogParam m_param;
	SceInt32 m_sessionHandle;
	bool m_active;
};

#endif /* FRIENDLIST_DIALOG_PLUGIN_H */
