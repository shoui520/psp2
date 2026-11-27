/*
 * livearea_more_ctrl.cpp: livearea_plugin "load more" footer controller
 * (0x811A1348-0x811A1EF4, sti 0x811A1E82).
 */
#include <std/string.h>
#include <widget/w_busyindicator.h>
#include <timer/timer.h>
#include "livearea_internal.h"

/* 0x811A1348 */
LiveAreaMoreCtrl::LiveAreaMoreCtrl(paf::ui::FileList *fileList, int segmentIndex,
				   paf::Plugin *plugin, MoreCallback callback, void *userdata)
	: m_fileList(fileList),
	  m_segmentIndex(segmentIndex),
	  m_plugin(plugin),
	  m_callback(callback),
	  m_userdata(userdata),
	  m_isScrollHooked(false),
	  m_hasError(false),
	  m_state(0)
{
}

/* 0x811A13C4 / 0x811A13EE */
LiveAreaMoreCtrl::~LiveAreaMoreCtrl()
{
	m_fileList->DeleteEventCallback(268435457, OnScrollEvent, this);
}

/* 0x811A141E */
void LiveAreaMoreCtrl::SetState(int state, paf::ui::Widget *header)
{
	if (state == m_state)
		return;
	m_state = state;
	if (header == NULL)
		header = m_fileList->GetSegmentHeader(m_segmentIndex);

	if (state == 1) {
		if (header != NULL) {
			paf::IDParam busyId((uint32_t)-1734585631);
			paf::ui::BusyIndicator *busy = (paf::ui::BusyIndicator *)header->FindChild(busyId, 0);
			paf::IDParam loadingLabelId(1750994960U);
			paf::ui::Widget *loadingLabel = header->FindChild(loadingLabelId, 0);
			paf::IDParam idleLabelId(612899877U);
			paf::ui::Widget *idleLabel = header->FindChild(idleLabelId, 0);
			if (busy != NULL)
				busy->Stop();
			if (loadingLabel != NULL) {
				paf::Timer *fadeTimer = new paf::Timer();
				fadeTimer->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_4588699E(loadingLabel, fadeTimer, 2, 0.0f, NULL);
			}
			if (idleLabel != NULL) {
				paf::Timer *fadeTimer = new paf::Timer();
				fadeTimer->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_FF5BC957(idleLabel, fadeTimer, 2, 0.0f, NULL);
			}
		}
	} else if (state == 2) {
		if (header != NULL) {
			paf::IDParam busyId((uint32_t)-1734585631);
			paf::ui::BusyIndicator *busy = (paf::ui::BusyIndicator *)header->FindChild(busyId, 0);
			paf::IDParam loadingLabelId(1750994960U);
			paf::ui::Widget *loadingLabel = header->FindChild(loadingLabelId, 0);
			paf::IDParam idleLabelId(612899877U);
			paf::ui::Widget *idleLabel = header->FindChild(idleLabelId, 0);
			if (busy != NULL)
				busy->Start();
			if (loadingLabel != NULL) {
				paf::Timer *fadeTimer = new paf::Timer();
				fadeTimer->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_FF5BC957(loadingLabel, fadeTimer, 2, 0.0f, NULL);
			}
			if (idleLabel != NULL) {
				paf::Timer *fadeTimer = new paf::Timer();
				fadeTimer->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_4588699E(idleLabel, fadeTimer, 2, 0.0f, NULL);
			}
		}
		if (m_callback != NULL)
			m_callback(m_userdata);
	}
}

/* 0x811A1AF2 */
void LiveAreaMoreCtrl::Enable()
{
	paf::intrusive_ptr<filelist::BasicSegmentSource> segSrc(
		static_cast<filelist::BasicSegmentSource *>(m_fileList->m_segment_source.get()));
	if (segSrc.get() != NULL) {
		paf::intrusive_ptr<paf::ui::filelist::CellSource> nullCell;
		paf::ui::filelist::SegmentListAccessor::OptionParam option;
		sce_paf_memset(&option, 0, sizeof(option));
		option.m_joined = true;
		segSrc->InsertSegment(m_segmentIndex, nullCell, option);
		SetState(0, NULL);
	}
}

/* 0x811A1B76 */
void LiveAreaMoreCtrl::Disable()
{
	paf::intrusive_ptr<filelist::BasicSegmentSource> segSrc(
		static_cast<filelist::BasicSegmentSource *>(m_fileList->m_segment_source.get()));
	if (segSrc.get() != NULL) {
		segSrc->RemoveSegment(m_segmentIndex);
		SetState(0, NULL);
	}
}

/* 0x811A1BA2 */
void LiveAreaMoreCtrl::Show()
{
	m_hasError = false;
	if (m_state == 1)
		return;
	if (m_state == 2) {
		SetState(1, NULL);
		return;
	}
	paf::IDParam tmplId = GetTemplateId();
	paf::Plugin::TemplateOpenParam openParam;
	g_liveareaPlugin->TemplateOpen(m_fileList, tmplId, openParam);
	paf::IDParam headerId = GetHeaderId();
	paf::ui::Widget *child = m_fileList->FindChild(headerId, 0);
	m_fileList->SetSegmentHeader(m_segmentIndex, child, false, false);
	SetState(1, child);
}

/* 0x811A1C7C */
void LiveAreaMoreCtrl::Hide()
{
	m_fileList->SetSegmentHeader(m_segmentIndex, NULL, false, false);
	SetState(0, NULL);
}

/* 0x811A1CA2 */
void liveareaMoreCtrlSetCount(LiveAreaMoreCtrl *ctrl, int count)
{
	bool hooked = ctrl->m_isScrollHooked;
	if (count < 3) {
		if (hooked) {
			ctrl->m_isScrollHooked = false;
			ctrl->m_fileList->ScePafWidget_7E9DC5B1(
				ctrl->m_segmentIndex, paf::math::v2(0.0f, 0.0f));
		}
	} else {
		if (!hooked) {
			ctrl->m_isScrollHooked = true;
			ctrl->m_fileList->ScePafWidget_7E9DC5B1(
				ctrl->m_segmentIndex, paf::math::v2(0.0f, 12.0f));
		}
	}
}

/* 0x811A1D10 */
void LiveAreaMoreCtrl::OnResult(bool hasError)
{
	m_hasError = hasError;
	SetState(1, NULL);
}

/* 0x811A1D2A */
paf::IDParam LiveAreaMoreCtrl::GetTemplateId()
{
	return paf::IDParam((uint32_t)-213022780);
}

/* 0x811A1D6E */
paf::IDParam LiveAreaMoreCtrl::GetHeaderId()
{
	return paf::IDParam((uint32_t)-428048466);
}

/* 0x811A1DB2 */
bool LiveAreaMoreCtrl::IsScrolledToBottom()
{
	paf::ui::Widget *header = m_fileList->GetSegmentHeader(m_segmentIndex);
	if (header == NULL)
		return false;
	float scrollY = -m_fileList->GetViewFocus().extract_y();
	float contentHeight = m_fileList->m_view_size.extract_y();
	float viewHeight = m_fileList->GetSize(0)->extract_y();
	return scrollY > (contentHeight - 10.0f) - (viewHeight * 0.5f);
}

/* 0x811A1E4E */
void LiveAreaMoreCtrl::OnScroll()
{
	if (m_fileList->GetSegmentHeader(m_segmentIndex) != NULL && IsScrolledToBottom())
		SetState(2, NULL);
}

/* 0x811A1E76 */
void LiveAreaMoreCtrl::OnScrollEvent(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	((LiveAreaMoreCtrl *)userdata)->OnScroll();
}
