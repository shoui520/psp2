/*
 * The error dialog, with trophysetup_dialog_plugin's text for the error. The file's name is
 * ours: no Sony path names it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <np/np_error.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <std/string>
#include <std/memory>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_text_box.h>
#include <graphics/text/text_layout.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "np_trophy_setup_dialog_plugin.h"
#include "dialogs.h"

using namespace paf;

/* what the error dialog calls as it opens: nothing */
class TrophySetupOpenHandler : public CommonGuiMessageDialogOpenHandler {
public:
	virtual void operator()(CommonGuiMessageDialogParam *param)
	{
	}
};

/* the text an error is shown with, or 0 for the library's own */
static __attribute__((noinline)) SceUInt32 GetErrorMessageId(SceUInt32 error)
{
	if (((error >> 16) & 0xFFF) == 0x014 ||
	    error == 0x80551688 || error == 0x8055168A || error == 0x8055168C ||
	    error == 0x805516B4 || error == SCE_NP_TROPHY_ERROR_BROKEN_DATA ||
	    error == 0x805516CF || error == 0x805516B7 || error == 0x805516B5 ||
	    error == 0x805516B6 || error == 0x805516CE || error == 0x80551696 ||
	    error == 0x805516CC || error == SCE_NP_TROPHY_ERROR_INVALID_TROPHY_CONF_FORMAT)
		return TROPHY_SETUP_MSG_ERROR;
	if (error == SCE_NP_TROPHY_ERROR_INSUFFICIENT_MC_SPACE)
		return TROPHY_SETUP_MSG_NO_SPACE;
	return 0;
}

int OpenErrorDialog(SceInt32 error, CommonGuiDialogFunction func, void *arg)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);
	SceUInt32 messageId;
	int dialogId;

	if (plugin == NULL)
		return -1;
	CommonGuiMessageDialogParam param;

	param.type = error;
	param.option.flags = 8;
	messageId = GetErrorMessageId(error);
	if (messageId != 0)
		param.message = plugin->GetString(IDParam(s_empty_name, messageId));
	dialogId = SceCommonGuiDialog_A6422883(plugin, &param, new TrophySetupDialogHandler(func, arg),
					       new TrophySetupOpenHandler);
	if (dialogId >= 0) {
		math::v4 block(0.0f, 0.0f, 0.0f, 0.0f);

		SceCommonGuiDialog_206623FF(dialogId, block, block);
	}
	return dialogId;
}
