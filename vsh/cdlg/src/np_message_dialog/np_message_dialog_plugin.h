/*
 * SceNpMessageDialogPlugin (vs0:sys/external/libcdlg_np_message.suprx):
 * plugin entry, work memory layout, and sub-plugin loader declarations.
 */
#ifndef NP_MESSAGE_DIALOG_PLUGIN_H
#define NP_MESSAGE_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <np/np_message.h>
#include <np_message_dialog.h>
#include <plugin.h>
#include <job/job.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

#define NP_MESSAGE_DIALOG_ERROR_NO_WORK_MEMORY	(-2146431454)	/* 0x80100E22 */

/*
 * Shared work memory layout matching vsh/cdlg/src/cdlg_mgr/np_message_dialog.cpp (2756 bytes).
 */
struct CommonDialogNpMessageParam {
	SceNpMessageDialogParam param;
	SceNpMessageDialogResult result;
	SceUInt8 reservedResult[4];
	SceNpId sendNpIds[SCE_NP_MESSAGE_DIALOG_MAX_RECIPIENTS];
	SceUInt8 pluginState[1612];
	SceInt32 drawPriority;
	SceCommonDialogColor bgColor;
	char apiName[64];
};

enum NpMessageSubPluginKind {
	NP_MSG_SUBPLUGIN_IMPL = 1,
	NP_MSG_SUBPLUGIN_FRIEND_SELECT = 2,
	NP_MSG_SUBPLUGIN_CONTACTS = 5,
	NP_MSG_SUBPLUGIN_PARTY_MEMBERS = 6
};

typedef void (*NpMessagePluginLoadCallback)(SceInt32 kind, SceInt32 result, void *userData);

class ScePsmJobPluginLoad : public paf::job::JobItem {
public:
	explicit ScePsmJobPluginLoad(SceInt32 kind);
	virtual ~ScePsmJobPluginLoad();

	void SetCallback(NpMessagePluginLoadCallback callback, void *userData);
	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	SceInt32 m_kind;
	NpMessagePluginLoadCallback m_callback;
	void *m_userData;
};

#endif /* NP_MESSAGE_DIALOG_PLUGIN_H */
