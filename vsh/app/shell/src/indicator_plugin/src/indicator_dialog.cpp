/*
 * indicator_dialog.cpp: the indicator's question dialog, the library's layout dialog of the
 * caller's layout with its message, its button told to the caller, never in the PM or IDU modes
 * (0x81183012-0x811831D8). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../shell.h"
#include "../../livespace_db/livespace_db_internal.h"
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* the page's priority over the notices */
#define INDICATOR_DIALOG_PRIORITY	19

/* the dialog open, -1 once it has closed (0x81544CC8) */
static int s_dialog = -1;

/* the dialog closed and its button told to the caller; the dialog forgotten as the library
   deletes the handler with it */
class IndicatorDialogHandler : public CommonGuiDialogHandler {
public:
	IndicatorDialogHandler(IndicatorDialogCB callback, void *arg)
		: m_callback(callback), m_arg(arg)
	{
	}

	virtual ~IndicatorDialogHandler()
	{
		s_dialog = -1;
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		SceCommonGuiDialog_BC311DC0(value);
		if (m_callback != NULL)
			m_callback(value, button, m_arg);
	}

	IndicatorDialogCB m_callback;
	void *m_arg;
};

/* (0x81183012) */
int indicatorDialogOpen(const CommonGuiDialogLayout *layout, const wchar_t *message,
			IndicatorDialogCB callback, void *arg)
{
	int id = -1;

	if (shellStartupIsPmMode() || shellStartupGetIduMode() == 1)
		return id;

	Plugin *plugin = g_indicatorPlugin;

	if (plugin != NULL) {
		CommonGuiLayoutDialogParam param;

		param.resourcePlugin = plugin;
		param.layout = *layout;
		param.priority = INDICATOR_DIALOG_PRIORITY;
		param.handler = new IndicatorDialogHandler(callback, arg);
		param.text = message;
		id = SceCommonGuiDialog_CCFCD9A6(&param);
		s_dialog = id;
	}
	return id;
}
