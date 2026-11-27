/*
 * impose_shell_event.cpp: impose_plugin's dialogs for the shell's events (0x8115013C-
 * 0x81150574): a dialog asking on event 4 (taken back on event 7) and one telling on event 6,
 * whose buttons go on with SceShellUtil_636544FB; the dialog of an application quit by its
 * name; and events 12 to 16 passed on to SceShellUtil_BB54D049. The file's and the names are
 * ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <shell_svc/shell_util.h>
#include "../../../shell.h"
#include "impose_shell_event.h"

using namespace paf;

/* the shell's events (PLACEHOLDER names) */
#define SHELL_EVENT_ASK			4
#define SHELL_EVENT_NOTICE		6
#define SHELL_EVENT_ASK_CANCEL		7
#define SHELL_EVENT_BB54D049_5A		12
#define SHELL_EVENT_BB54D049_5B		13
#define SHELL_EVENT_BB54D049_4A		14
#define SHELL_EVENT_BB54D049_4B		15
#define SHELL_EVENT_BB54D049_3		16

/* the dialogs' texts (PLACEHOLDER names) */
#define SHELL_EVENT_MSG_ASK		0x12C27FEB
#define SHELL_EVENT_MSG_NOTICE		0xB2D21358
#define SHELL_EVENT_MSG_QUIT		0x417EA976

/* the dialog buttons (PLACEHOLDER names) */
#define SHELL_EVENT_BUTTON_YES		4
#define SHELL_EVENT_BUTTON_NO		5

extern "C" {
/* the shell's event callbacks: registered and taken off (the shell's exports, PLACEHOLDER
   names) */
int SceShell_548A3111(int event, void (*callback)(int event, int *data), void *arg);
int SceShell_F197032C(int event, void (*callback)(int event, int *data), void *arg);
}

int shellStateWatcherOpenDialog(CommonGuiLayoutDialogParam *param, bool playSound, int level);
int shellStateWatcherCloseDialog(int dialogId);

/* (0x81564E94) */
static ImposeShellEvent *s_shellEvent;

/* (0x8115013C) */
ImposeShellEvent::ImposeShellEvent(Plugin *plugin)
	: m_plugin(plugin), m_askDialog(-1), m_noticeDialog(-1), m_quitDialog(-1)
{
	s_shellEvent = this;
}

/* (0x81150154) */
ImposeShellEvent::~ImposeShellEvent()
{
	s_shellEvent = NULL;
}

static void imposeShellEventOnEvent(int event, int *data);

/* (0x81150164) */
void ImposeShellEvent::Start()
{
	if (!shellStartupIsPmMode()) {
		SceShell_548A3111(SHELL_EVENT_ASK, imposeShellEventOnEvent, NULL);
		SceShell_548A3111(SHELL_EVENT_NOTICE, imposeShellEventOnEvent, NULL);
		SceShell_548A3111(SHELL_EVENT_ASK_CANCEL, imposeShellEventOnEvent, NULL);
	}
	SceShell_548A3111(SHELL_EVENT_BB54D049_5A, imposeShellEventOnEvent, NULL);
	SceShell_548A3111(SHELL_EVENT_BB54D049_5B, imposeShellEventOnEvent, NULL);
	SceShell_548A3111(SHELL_EVENT_BB54D049_4A, imposeShellEventOnEvent, NULL);
	SceShell_548A3111(SHELL_EVENT_BB54D049_4B, imposeShellEventOnEvent, NULL);
	SceShell_548A3111(SHELL_EVENT_BB54D049_3, imposeShellEventOnEvent, NULL);
}

/* (0x811501D0) */
void ImposeShellEvent::Stop()
{
	SceShell_F197032C(SHELL_EVENT_BB54D049_3, imposeShellEventOnEvent, NULL);
	SceShell_F197032C(SHELL_EVENT_BB54D049_4B, imposeShellEventOnEvent, NULL);
	SceShell_F197032C(SHELL_EVENT_BB54D049_4A, imposeShellEventOnEvent, NULL);
	SceShell_F197032C(SHELL_EVENT_BB54D049_5B, imposeShellEventOnEvent, NULL);
	SceShell_F197032C(SHELL_EVENT_BB54D049_5A, imposeShellEventOnEvent, NULL);
	if (!shellStartupIsPmMode()) {
		SceShell_F197032C(SHELL_EVENT_ASK_CANCEL, imposeShellEventOnEvent, NULL);
		SceShell_F197032C(SHELL_EVENT_NOTICE, imposeShellEventOnEvent, NULL);
		SceShell_F197032C(SHELL_EVENT_ASK, imposeShellEventOnEvent, NULL);
	}
}

/* the dialogs' buttons (PLACEHOLDER name) */
class ImposeShellEventHandler : public CommonGuiDialogHandler {
public:
	virtual void operator()(SceInt32 value, SceInt32 button);
};

/* (0x8115023C) */
void ImposeShellEvent::OpenDialog(SceInt32 *id, SceInt32 dimmer, CommonGuiDialogLayout layout,
				  SceUInt32 textId)
{
	if (*id < 0) {
		CommonGuiLayoutDialogParam param;

		param.dimmer = dimmer;
		param.layout = layout;
		param.text = m_plugin->GetString(IDParam(textId));

		ImposeShellEventHandler *handler = new ImposeShellEventHandler;

		if (handler != NULL) {
			param.handler = handler;
			*id = shellStateWatcherOpenDialog(&param, true, 0);
		}
	}
}

/* (0x811503CE) */
void ImposeShellEvent::QuitApp(const char *name)
{
	SceAppMgrAppStatus status;

	if (m_noticeDialog < 0 && sceAppMgrGetStatusByName(name, &status) == 0) {
		OpenDialog(&m_quitDialog, 1, SceCommonGuiDialog_B04C8C1A, SHELL_EVENT_MSG_QUIT);
		sceAppMgrQuitApp(status.appId);
	}
}

/* the dialog of the ID closed, if it is open */
static inline void imposeShellEventCloseDialog(SceInt32 *id)
{
	if (*id >= 0) {
		shellStateWatcherCloseDialog(*id);
		*id = -1;
	}
}

/* (0x81150434) */
__attribute__((noinline)) void ImposeShellEvent::OnDialogButton(SceInt32 id, SceInt32 button)
{
	if (id == m_askDialog) {
		switch (button) {
		case SHELL_EVENT_BUTTON_YES:
			SceShellUtil_636544FB(0);
			break;
		case SHELL_EVENT_BUTTON_NO:
			imposeShellEventCloseDialog(&m_askDialog);
			break;
		}
	} else if (id == m_noticeDialog) {
		SceShellUtil_636544FB(0);
	} else if (id == m_quitDialog) {
		imposeShellEventCloseDialog(&m_quitDialog);
	}
}

/* (0x8115048E) */
void ImposeShellEvent::OnEvent(int event, int *data)
{
	switch (event) {
	case SHELL_EVENT_ASK:
		if (m_noticeDialog < 0)
			OpenDialog(&m_askDialog, 1, SceCommonGuiDialog_6CD802FC, SHELL_EVENT_MSG_ASK);
		break;
	case SHELL_EVENT_NOTICE:
		OpenDialog(&m_noticeDialog, 2, SceCommonGuiDialog_B04C8C1A, SHELL_EVENT_MSG_NOTICE);
		break;
	case SHELL_EVENT_ASK_CANCEL:
		imposeShellEventCloseDialog(&m_askDialog);
		break;
	case SHELL_EVENT_BB54D049_5A:
	case SHELL_EVENT_BB54D049_5B:
		SceShellUtil_BB54D049(5, *data, 0);
		break;
	case SHELL_EVENT_BB54D049_4A:
	case SHELL_EVENT_BB54D049_4B:
		SceShellUtil_BB54D049(4, *data, 0);
		break;
	case SHELL_EVENT_BB54D049_3:
		SceShellUtil_BB54D049(3, *data, 0);
		break;
	}
}

/* (0x81150548) */
static void imposeShellEventOnEvent(int event, int *data)
{
	s_shellEvent->OnEvent(event, data);
}

/* (0x81150560) */
void ImposeShellEventHandler::operator()(SceInt32 value, SceInt32 button)
{
	s_shellEvent->OnDialogButton(value, button);
}
