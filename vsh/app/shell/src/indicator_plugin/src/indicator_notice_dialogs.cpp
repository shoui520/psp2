/*
 * indicator_notice_dialogs.cpp: the notices' dialogs kept open by their IDs, each forgotten as
 * its widget ends, let go by its ID or all at once as the notices close
 * (0x8116D76A-0x8116DA80). The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/map>
#include <thread/thread.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../impose_plugin/src/impose.h"
#include "indicator.h"

using namespace paf;

/* a dialog kept: its ID and the common dialog's (16 bytes) */
class IndicatorNoticeDialog {
public:
	IndicatorNoticeDialog(SceInt64 id, int dialogId) : m_id(id), m_dialogId(dialogId)
	{
	}

	SceInt64 m_id;
	int m_dialogId;
};

/* the dialogs kept, by their IDs (12 bytes) */
class IndicatorNoticeDialogs {
public:
	void Add(SceInt64 id, int dialogId);
	void Close(SceInt64 id);
	void OnEnd(IndicatorNoticeDialog *dialog);

	map<SceInt64, int> m_dialogs;
};

/* the common dialog's widget told to end (as SceCommonGuiDialog_8947FC23 takes it) */
#define INDICATOR_NOTICE_DIALOG_WIDGET	4

/* (0x8156697C) */
static IndicatorNoticeDialogs *s_noticeDialogs;

/* the dialog kept by its ID, and forgotten by OnEnd once its widget ends; a dialog without its
   widget is not kept (0x8116D76A) */
void IndicatorNoticeDialogs::Add(SceInt64 id, int dialogId)
{
	thread::RMutexLock lock(&thread::RMutex::main_thread_mutex);
	ui::Widget *widget = SceCommonGuiDialog_8947FC23(dialogId, INDICATOR_NOTICE_DIALOG_WIDGET);

	if (widget != NULL) {
		m_dialogs[id] = dialogId;

		IndicatorNoticeDialog *dialog = new IndicatorNoticeDialog(id, dialogId);

		widget->AddEventListener(ui::Handler::CB_STATE_TERM,
					 new ImposePsMenuArgListener<IndicatorNoticeDialogs,
								     IndicatorNoticeDialog *>(
						 this, &IndicatorNoticeDialogs::OnEnd, dialog));
	}
}

/* the dialog kept by the ID let go and forgotten (0x8116D840) */
void IndicatorNoticeDialogs::Close(SceInt64 id)
{
	thread::RMutex::main_thread_mutex.Lock();

	map<SceInt64, int>::iterator it = m_dialogs.find(id);

	if (it != m_dialogs.end()) {
		SceCommonGuiDialog_BC311DC0(it->second);
		m_dialogs.erase(it);
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* a dialog's widget ended: the dialog forgotten if it is still the one kept by its ID
   (0x8116D8D4) */
void IndicatorNoticeDialogs::OnEnd(IndicatorNoticeDialog *dialog)
{
	thread::RMutex::main_thread_mutex.Lock();
	if (dialog == NULL) {
		thread::RMutex::main_thread_mutex.Unlock();
		return;
	}

	map<SceInt64, int>::iterator it = m_dialogs.find(dialog->m_id);

	if (it != m_dialogs.end() && it->second == dialog->m_dialogId)
		m_dialogs.erase(it);
	delete dialog;
	thread::RMutex::main_thread_mutex.Unlock();
}

/* (0x8116D988) */
void indicatorNoticeDialogsCreate(void)
{
	s_noticeDialogs = new IndicatorNoticeDialogs;
}

/* (0x8116D9A8) */
void indicatorNoticeDialogsDestroy(void)
{
	if (s_noticeDialogs != NULL)
		delete s_noticeDialogs;
	s_noticeDialogs = NULL;
}

/* (0x8116D9E0) */
void indicatorNoticeDialogAdd(SceInt64 id, int dialogId)
{
	s_noticeDialogs->Add(id, dialogId);
}

/* (0x8116DA00) */
void indicatorNoticeDialogClose(SceInt64 id)
{
	s_noticeDialogs->Close(id);
}

/* every dialog kept forgotten, then let go (0x8116DA18) */
void indicatorNoticeDialogCloseAll(void)
{
	IndicatorNoticeDialogs *dialogs = s_noticeDialogs;

	thread::RMutex::main_thread_mutex.Lock();
	while (dialogs->m_dialogs.size() != 0) {
		map<SceInt64, int>::iterator it = dialogs->m_dialogs.begin();

		if (it != dialogs->m_dialogs.end()) {
			int dialogId = it->second;

			dialogs->m_dialogs.erase(it);
			SceCommonGuiDialog_BC311DC0(dialogId);
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();
}
