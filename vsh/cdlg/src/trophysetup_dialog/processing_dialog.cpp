/*
 * The processing dialog, shown at least TROPHY_SETUP_PROCESSING_TIME. The file's name is ours:
 * no Sony path names it.
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

int OpenProcessingDialog(ui::HandlerCB func, void *data)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);
	CommonGuiLayoutDialogParam param;
	ui::Widget *widget;
	int dialogId;

	param.text = plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_PROCESSING));
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.0f;
	param.layout = SceCommonGuiDialog_F8B2D7E1;
	dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0) {
		widget = SceCommonGuiDialog_8947FC23(dialogId, TROPHY_SETUP_DIALOG_WIDGET_REMOVED);
		if (widget != NULL)
			widget->DestroyWidget();
		widget = SceCommonGuiDialog_8947FC23(dialogId, TROPHY_SETUP_DIALOG_WIDGET_ANIMATION);
		if (widget != NULL && func != NULL)
			widget->SetTimerListener(0x01000000 | (dialogId & 0xFFFF), TROPHY_SETUP_PROCESSING_TIME,
						 new ui::EventCBListener(func, data));
	}
	return dialogId;
}
