/*
 * SceNpProfile2DialogPlugin (vs0:sys/external/libcdlg_npprofile2.suprx):
 * NP profile dialog launcher and friend_profile_plugin host.
 */
#ifndef NPPROFILE2_DIALOG_PLUGIN_H
#define NPPROFILE2_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <np/np_common.h>
#include <np_profile_dialog.h>
#include <plugin.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

#define NPPROFILE2_ERROR_HTTP_POOL		(-2146431228)	/* 0x80100F04 */
#define NPPROFILE2_ERROR_SSL_POOL		(-2146431227)	/* 0x80100F05 */
#define NPPROFILE2_ERROR_NOT_SIGNED_IN		(-2141913086)	/* 0x80550002 */

struct CommonDialogNpProfileParam {
	SceNpProfileDialogParam param;
	SceNpProfileDialogResult result;
	SceCommonDialogColor bgColor;
	char apiName[64];
};

#endif /* NPPROFILE2_DIALOG_PLUGIN_H */
