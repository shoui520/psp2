/*
 * What the plugin's dialog files share: the resource's strings and templates the dialogs show,
 * the dialogs' widgets, and the handler that calls a function. Sony built the dialogs in four
 * files (each keeps its own copy of the plugin's name); the names are ours: no Sony path names
 * them.
 */
#ifndef TROPHYSETUP_DIALOGS_H
#define TROPHYSETUP_DIALOGS_H

/* the strings and templates of trophysetup_dialog_plugin's resource the dialogs show */
#define TROPHY_SETUP_MSG_ERROR			0x0176210C
#define TROPHY_SETUP_MSG_NO_SPACE		0xE2A2DDCC
#define TROPHY_SETUP_MSG_PROCESSING		0xD1512F9B
#define TROPHY_SETUP_MSG_EXIT			0xC67DC466
#define TROPHY_SETUP_MSG_REMOVAL_QUESTION	0x4B303AAB
#define TROPHY_SETUP_MSG_REMOVAL_NOTE		0xC667793A
#define TROPHY_SETUP_QUESTION_FOCUS		0xE0D9F918
#define TROPHY_SETUP_TEMPLATE_TITLE		0xEEC81AFE
#define TROPHY_SETUP_TITLE_ICON			0xA359C587
#define TROPHY_SETUP_TITLE_NAME			0x0EC240C5
#define TROPHY_SETUP_TEMPLATE_NOTE		0x956D0FC2
#define TROPHY_SETUP_NOTE_TEXT			0x145CAB23

/* the dialogs' widgets (SceCommonGuiDialog_8947FC23) */
#define TROPHY_SETUP_DIALOG_WIDGET_TEXT		1
#define TROPHY_SETUP_DIALOG_WIDGET_ANIMATION	4
#define TROPHY_SETUP_DIALOG_WIDGET_CONTENTS	10
#define TROPHY_SETUP_DIALOG_WIDGET_REMOVED	12

/* the processing dialog shows at least this long, in milliseconds */
#define TROPHY_SETUP_PROCESSING_TIME		2000.0f

/* a handler that calls a function */
class TrophySetupDialogHandler : public CommonGuiDialogHandler {
public:
	TrophySetupDialogHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		m_func(value, button, m_arg);
	}

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

#endif
