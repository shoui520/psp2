/*
 * The one dialog the plugin has open at a time: the error, the processing dialog, the question
 * before an installed title is removed, and the last notice. The file's name is ours: no Sony
 * path names it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <common/main_thread_call_list.h>
#include <widget/core/handler.h>
#include "../cdlg_main/common_dialog_main.h"
#include "np_trophy_setup_dialog_plugin.h"

using namespace paf;

static TrophySetupDialogController s_dialogController;

static void dialogProcessed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void errorDialogClosed(SceInt32 value, SceInt32 button, void *arg);
static void dialogControllerUpdate(void *userdata);

/* lets the open dialog go */
static inline void closeDialog(TrophySetupDialogController *controller)
{
	if (controller->m_dialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(controller->m_dialogId);
		controller->m_dialogId = -1;
	}
}

static inline void showDimmer(TrophySetupDialogController *controller)
{
	if (!controller->m_dimmerShown)
		SceCommonDialogMain_877BB562();
}

TrophySetupDialogController *TrophySetupDialogController::GetInstance()
{
	return &s_dialogController;
}

int TrophySetupDialogController::Close(bool hideDimmer)
{
	if (hideDimmer && m_dimmerShown)
		SceCommonDialogMain_FDED47DB();
	closeDialog(this);
	m_state = TROPHY_SETUP_DIALOG_STATE_NONE;
	return 0;
}

void TrophySetupDialogController::ShowError(SceInt32 error)
{
	closeDialog(this);
	m_state = TROPHY_SETUP_DIALOG_STATE_NONE;
	showDimmer(this);
	m_dialogId = OpenErrorDialog(error, errorDialogClosed, this);
}

void TrophySetupDialogController::ShowRemovalQuestion(const wstring &title,
						      intrusive_ptr<graph::Surface> &icon,
						      CommonGuiDialogFunction func, void *arg)
{
	closeDialog(this);
	m_state = TROPHY_SETUP_DIALOG_STATE_NONE;
	showDimmer(this);
	if (icon.get() != NULL)
		m_dialogId = OpenRemovalQuestionDialogWithIcon(title, icon, func, arg);
	else
		m_dialogId = OpenRemovalQuestionDialog(title, func, arg);
}

void TrophySetupDialogController::ShowExitNotice()
{
	closeDialog(this);
	m_state = TROPHY_SETUP_DIALOG_STATE_NONE;
	showDimmer(this);
	m_dialogId = OpenExitNoticeDialog();
}

int TrophySetupDialogController::ShowProcessing()
{
	int dialogId;

	closeDialog(this);
	m_state = TROPHY_SETUP_DIALOG_STATE_NONE;
	showDimmer(this);
	dialogId = m_dialogId = OpenProcessingDialog(dialogProcessed, this);
	m_state = TROPHY_SETUP_DIALOG_STATE_PROCESSING;
	return dialogId;
}

int TrophySetupDialogController::Hide()
{
	if (m_dimmerShown)
		SceCommonDialogMain_FDED47DB();
	closeDialog(this);
	m_state = TROPHY_SETUP_DIALOG_STATE_NONE;
	return 0;
}

bool TrophySetupDialogController::IsProcessing() const
{
	return m_dialogId >= 0 && (m_state == TROPHY_SETUP_DIALOG_STATE_PROCESSING ||
				   m_state == TROPHY_SETUP_DIALOG_STATE_PROCESSED);
}

void TrophySetupDialogController::CloseAfterProcessing(bool hideDimmer, void (*func)(void))
{
	m_hideDimmer = hideDimmer;
	m_closedFunc = func;
	common::MainThreadCallList::Register(dialogControllerUpdate, this);
}

TrophySetupDialogController::TrophySetupDialogController()
	: m_dialogId(-1), m_dimmerShown(false), m_state(TROPHY_SETUP_DIALOG_STATE_NONE)
{
}

TrophySetupDialogController::~TrophySetupDialogController()
{
}

/* on the main thread: once the processing dialog has shown long enough, closes it and calls
   the function */
static void dialogControllerUpdate(void *userdata)
{
	TrophySetupDialogController *controller = (TrophySetupDialogController *)userdata;

	/* Sony's test is a range: past processing, up to processed */
	if (controller->m_dialogId < 0 || controller->m_state <= TROPHY_SETUP_DIALOG_STATE_PROCESSING ||
	    controller->m_state > TROPHY_SETUP_DIALOG_STATE_PROCESSED)
		return;
	common::MainThreadCallList::Unregister(dialogControllerUpdate, controller);
	controller->Close(controller->m_hideDimmer);
	if (controller->m_closedFunc != NULL)
		controller->m_closedFunc();
}

/* the processing dialog's timer */
static void dialogProcessed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	((TrophySetupDialogController *)userdata)->m_state = TROPHY_SETUP_DIALOG_STATE_PROCESSED;
}

static void errorDialogClosed(SceInt32 value, SceInt32 button, void *arg)
{
	TrophySetupDialogController::GetInstance()->ShowExitNotice();
}
