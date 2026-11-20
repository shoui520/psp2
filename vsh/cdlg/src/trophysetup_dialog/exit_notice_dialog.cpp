/*
 * The last notice before the dialog exits. The file's name is ours: no Sony path names it.
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

int OpenExitNoticeDialog(void)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);

	if (plugin == NULL)
		return -1;
	CommonGuiLayoutDialogParam param;

	param.text = plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_EXIT));
	param.layout = SceCommonGuiDialog_52A53037;
	return SceCommonGuiDialog_CCFCD9A6(&param);
}
