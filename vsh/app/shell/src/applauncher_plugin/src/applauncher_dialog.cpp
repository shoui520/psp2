/*
 * applauncher_dialog.cpp: what the home's dialogs share (0x810BA6FC-0x810BA824): the handler that
 * passes a dialog's button on to a function, the open handler of its error dialogs, and the texts
 * of the errors. The file's name and the names here are ours (PLACEHOLDER): no record of the 3.6x
 * shell names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

/* an error and the resource ID of its text */
struct ApplauncherErrorText {
	int error;
	SceUInt32 textId;
};

/* the errors with a text of their own (0x815077C4) */
static const ApplauncherErrorText s_errorTexts[] = {
	{ (int)0x80103320, 0x3D12ADF2 },
	{ (int)0x8010112C, 0x338DFC50 },
	{ (int)0x8010112D, 0x4C148EAD },
	{ (int)0x8010112E, 0x888126CA },
	{ (int)0x80103309, 0x26FCB046 },
	{ (int)0x8010330E, 0x96F41016 },
	{ (int)0x8010330F, 0x45CEF25C },
	{ (int)0x8001001C, 0xD0F637B3 },
	{ (int)0x80870006, 0xBB3E6D51 },
};

#define APPLAUNCHER_ERROR_TEXT_COUNT	(sizeof(s_errorTexts) / sizeof(s_errorTexts[0]))

/* (0x810BA6FC) */
ApplauncherDialogHandler::ApplauncherDialogHandler(CommonGuiDialogFunction func, void *arg)
	: m_func(func), m_arg(arg)
{
}

/* (0x810BA710) */
void ApplauncherDialogHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_func != NULL)
		m_func(value, button, m_arg);
}

/* (0x810BA724) */
ApplauncherErrorOpenHandler::ApplauncherErrorOpenHandler()
{
}

/* (0x810BA734) */
SceUInt32 applauncherGetErrorTextId(int error)
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_ERROR_TEXT_COUNT; i++) {
		if (error == s_errorTexts[i].error)
			return s_errorTexts[i].textId;
	}
	return APPLAUNCHER_ERROR_TEXT_GENERIC;
}

/* (0x810BA76A) */
void ApplauncherErrorOpenHandler::operator()(CommonGuiMessageDialogParam *param)
{
	SceUInt32 textId = applauncherGetErrorTextId(param->type);

	if (textId != APPLAUNCHER_ERROR_TEXT_GENERIC) {
		param->defaultButton = 0;
		param->cancelButton = 0;
	}
	param->messageId = textId;
}
