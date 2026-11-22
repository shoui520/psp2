/*
 * indicator_notice_popups.cpp: the popup lists the notices open, kept by the notice's ID until
 * the list closes, closed by the ID or all at once as the notices close
 * (0x81173878-0x81173AD0). The file's and the names are ours (PLACEHOLDER). Its functions are
 * declared in indicator.h, which it does not include: Sony's file has none of the LSDB keys'
 * static initialisation that header brings.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <thread/thread.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include "../../../popup_list.h"
#include "../../impose_plugin/src/impose_listener.h"

using namespace paf;

/* the IDs of the popup lists open for the notices (12 bytes) */
class IndicatorNoticePopups {
public:
	void Add(SceInt64 id);
	void Close(SceInt64 id);
	void OnClose(SceInt64 id);

	list<SceInt64> m_ids;
};

/* (0x815669BC) */
static IndicatorNoticePopups *s_noticePopups;

/* the ID kept while the popup list open on top lives, forgotten by OnClose as it closes
   (0x81173878) */
void IndicatorNoticePopups::Add(SceInt64 id)
{
	thread::RMutex::main_thread_mutex.Lock();

	ui::Widget *widget = shellPopupListGetWidget(0);

	if (widget != NULL) {
		m_ids.push_back(id);
		widget->AddEventListener(
			ui::Handler::CB_STATE_TERM,
			new ImposePsMenuArgListener<IndicatorNoticePopups, SceInt64>(
				this, &IndicatorNoticePopups::OnClose, id),
			false);
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the popup list kept by the ID closed and forgotten (0x8117392A) */
void IndicatorNoticePopups::Close(SceInt64 id)
{
	thread::RMutex::main_thread_mutex.Lock();

	for (list<SceInt64>::iterator it = m_ids.begin(); it != m_ids.end(); ++it) {
		if (*it == id) {
			m_ids.erase(it);
			shellPopupListClose(0);
			break;
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* a popup list closed: its ID forgotten (0x81173990) */
void IndicatorNoticePopups::OnClose(SceInt64 id)
{
	thread::RMutex::main_thread_mutex.Lock();
	m_ids.remove(id);
	thread::RMutex::main_thread_mutex.Unlock();
}

/* (0x811739D4) */
void indicatorNoticePopupsCreate(void)
{
	s_noticePopups = new IndicatorNoticePopups;
}

/* (0x81173A08) */
void indicatorNoticePopupsDestroy(void)
{
	if (s_noticePopups != NULL)
		delete s_noticePopups;
	s_noticePopups = NULL;
}

/* (0x81173A52) */
void indicatorNoticePopupAdd(SceInt64 id)
{
	s_noticePopups->Add(id);
}

/* (0x81173A6A) */
void indicatorNoticePopupClose(SceInt64 id)
{
	s_noticePopups->Close(id);
}

/* every popup list kept forgotten, the last first, and closed (0x81173A82) */
void indicatorNoticePopupCloseAll(void)
{
	IndicatorNoticePopups *popups = s_noticePopups;

	thread::RMutex::main_thread_mutex.Lock();
	while (popups->m_ids.size() != 0) {
		list<SceInt64>::iterator last = popups->m_ids.end();

		if (last.node != NULL)
			popups->m_ids.erase(--last);
		shellPopupListClose(0);
	}
	thread::RMutex::main_thread_mutex.Unlock();
}
