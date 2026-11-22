/*
 * indicator_error_dialog.cpp: the indicator's error dialog, opened over the notices with the
 * error's text or the caller's message, its button told to the caller (0x8117EF86-0x8117F184).
 * The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <common_gui_dialog/common_gui_dialog.h>
#include "indicator.h"

using namespace paf;

/* the page's priority over the notices */
#define INDICATOR_ERROR_DIALOG_PRIORITY	19

/* the dialog open, -1 once it has closed (0x81544CB0) */
static int s_errorDialog = -1;

/* the dialog closed and its button told to the caller; the dialog forgotten as the library
   deletes the handler with it */
class IndicatorErrorDialogHandler : public CommonGuiDialogHandler {
public:
	IndicatorErrorDialogHandler(IndicatorDialogCB callback, void *arg)
		: m_callback(callback), m_arg(arg)
	{
	}

	virtual ~IndicatorErrorDialogHandler()
	{
		s_errorDialog = -1;
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

/* (0x8117EF86) */
int indicatorErrorDialogOpen(int error, const wchar_t *message, IndicatorDialogCB callback,
			     void *arg)
{
	Plugin *plugin = g_indicatorPlugin;
	int id = -1;

	if (plugin != NULL) {
		CommonGuiMessageDialogParam param;

		param.resourcePlugin = plugin;
		param.type = error;
		param.message = message;
		param.priority = INDICATOR_ERROR_DIALOG_PRIORITY;
		param.handler = new IndicatorErrorDialogHandler(callback, arg);
		id = SceCommonGuiDialog_20B141F1(&param);
		s_errorDialog = id;
	}
	return id;
}
