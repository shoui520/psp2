/*
 * livearea_more_ctrl.cpp: livearea_plugin "load more" footer controller
 * (0x811A1348-0x811A1EF4, sti 0x811A1E82). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <common/ref_obj.h>
#include <widget/core/widget.h>
#include <widget/w_list_view.h>
#include <widget/w_busyindicator.h>
#include <timer/timer.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */

extern void liveareaListSrcSetSeg(paf::common::RefObj *src, int idx, paf::common::RefObj **cell, void *param); /* 0x8134A8D2 */
extern void liveareaListSrcRemoveSeg(paf::common::RefObj *src, int idx); /* 0x8134AAA2 */

class LiveAreaMoreCtrl {
public:
	typedef void (*MoreCallback)(void *);

	LiveAreaMoreCtrl(paf::ui::ListView *lv, int segIdx, paf::Plugin *plugin,
			 MoreCallback cb, void *arg);		/* 0x811A1348 */
	virtual ~LiveAreaMoreCtrl();				/* 0x811A13C4 / 0x811A13EE */

	virtual void SetState(int state, paf::ui::Widget *hdr);	/* 0x811A141E */
	virtual void Enable();					/* 0x811A1AF2 */
	virtual void Disable();					/* 0x811A1B76 */
	virtual void Show();					/* 0x811A1BA2 */
	virtual void Hide();					/* 0x811A1C7C */
	virtual void OnResult(bool hasErr);			/* 0x811A1D10 */
	virtual paf::IDParam GetTemplateId();			/* 0x811A1D2A */
	virtual paf::IDParam GetHeaderId();			/* 0x811A1D6E */
	virtual bool IsScrolledToBottom();			/* 0x811A1DB2 */
	virtual void OnScroll();				/* 0x811A1E4E */

	static void OnScrollEvent(int a1, paf::ui::Handler *h, paf::ui::Event *ev, void *arg); /* 0x811A1E76 */

	paf::ui::ListView *m_listView;				/* +0x04 */
	int m_segIdx;						/* +0x08 */
	paf::Plugin *m_plugin;					/* +0x0C */
	MoreCallback m_cb;					/* +0x10 */
	void *m_arg;						/* +0x14 */
	bool m_flag18;						/* +0x18 */
	bool m_hasErr;						/* +0x19 */
	unsigned char m_pad1A[2];
	int m_state;						/* +0x1C */
};

/* 0x811A1348 */
LiveAreaMoreCtrl::LiveAreaMoreCtrl(paf::ui::ListView *lv, int segIdx, paf::Plugin *plugin,
				   MoreCallback cb, void *arg)
	: m_listView(lv),
	  m_segIdx(segIdx),
	  m_plugin(plugin),
	  m_cb(cb),
	  m_arg(arg),
	  m_flag18(false),
	  m_hasErr(false),
	  m_state(0)
{
}

/* 0x811A13C4 / 0x811A13EE */
LiveAreaMoreCtrl::~LiveAreaMoreCtrl()
{
	m_listView->DeleteEventCallback(268435457, OnScrollEvent, this);
}

/* 0x811A141E */
void LiveAreaMoreCtrl::SetState(int state, paf::ui::Widget *hdr)
{
	if (state == m_state)
		return;
	m_state = state;
	if (hdr == NULL)
		hdr = m_listView->GetSegmentHeader(m_segIdx);

	if (state == 1) {
		if (hdr != NULL) {
			paf::IDParam busyId((uint32_t)-1734585631);
			paf::ui::BusyIndicator *busy = (paf::ui::BusyIndicator *)hdr->FindChild(busyId, 0);
			paf::IDParam id2(1750994960U);
			paf::ui::Widget *w2 = hdr->FindChild(id2, 0);
			paf::IDParam id3(612899877U);
			paf::ui::Widget *w3 = hdr->FindChild(id3, 0);
			if (busy != NULL)
				busy->Stop();
			if (w2 != NULL) {
				paf::Timer *t = new paf::Timer();
				t->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_4588699E(w2, t, 2, 0.0f, NULL);
			}
			if (w3 != NULL) {
				paf::Timer *t = new paf::Timer();
				t->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_FF5BC957(w3, t, 2, 0.0f, NULL);
			}
		}
	} else if (state == 2) {
		if (hdr != NULL) {
			paf::IDParam busyId((uint32_t)-1734585631);
			paf::ui::BusyIndicator *busy = (paf::ui::BusyIndicator *)hdr->FindChild(busyId, 0);
			paf::IDParam id2(1750994960U);
			paf::ui::Widget *w2 = hdr->FindChild(id2, 0);
			paf::IDParam id3(612899877U);
			paf::ui::Widget *w3 = hdr->FindChild(id3, 0);
			if (busy != NULL)
				busy->Start();
			if (w2 != NULL) {
				paf::Timer *t = new paf::Timer();
				t->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_FF5BC957(w2, t, 2, 0.0f, NULL);
			}
			if (w3 != NULL) {
				paf::Timer *t = new paf::Timer();
				t->InitTimer(0.0f, 0, -1, 0, NULL);
				ScePafWidget_4588699E(w3, t, 2, 0.0f, NULL);
			}
		}
		if (m_cb != NULL)
			m_cb(m_arg);
	}
}

/* 0x811A1AF2 */
void LiveAreaMoreCtrl::Enable()
{
	paf::common::RefObj *segSrc = *(paf::common::RefObj **)((char *)m_listView + 1252);
	if (segSrc != NULL) {
		segSrc->AddRef();
		paf::common::RefObj *nullCell = NULL;
		unsigned char param[44];
		sce_paf_memset(param, 0, sizeof(param));
		param[40] = 1;
		liveareaListSrcSetSeg(segSrc, m_segIdx, &nullCell, param);
		if (nullCell != NULL)
			nullCell->Release();
		SetState(0, NULL);
		segSrc->Release();
	}
}

/* 0x811A1B76 */
void LiveAreaMoreCtrl::Disable()
{
	paf::common::RefObj *segSrc = *(paf::common::RefObj **)((char *)m_listView + 1252);
	if (segSrc != NULL) {
		segSrc->AddRef();
		liveareaListSrcRemoveSeg(segSrc, m_segIdx);
		SetState(0, NULL);
		segSrc->Release();
	}
}

/* 0x811A1BA2 */
void LiveAreaMoreCtrl::Show()
{
	m_hasErr = false;
	if (m_state == 1)
		return;
	if (m_state == 2) {
		SetState(1, NULL);
		return;
	}
	paf::IDParam tmplId = GetTemplateId();
	paf::Plugin::TemplateOpenParam op;
	g_liveareaPlugin->TemplateOpen(m_listView, tmplId, op);
	paf::IDParam hdrId = GetHeaderId();
	paf::ui::Widget *child = m_listView->FindChild(hdrId, 0);
	m_listView->SetSegmentHeader(m_segIdx, child, false, false);
	SetState(1, child);
}

/* 0x811A1C7C */
void LiveAreaMoreCtrl::Hide()
{
	m_listView->SetSegmentHeader(m_segIdx, NULL, false, false);
	SetState(0, NULL);
}

/* 0x811A1D10 */
void LiveAreaMoreCtrl::OnResult(bool hasErr)
{
	m_hasErr = hasErr;
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
	paf::ui::Widget *hdr = m_listView->GetSegmentHeader(m_segIdx);
	if (hdr == NULL)
		return false;
	paf::math::v2 focus = m_listView->GetViewFocus();
	float scrollY = -((const float *)&focus)[1];
	float contentH = *(const float *)((const char *)m_listView + 724);
	float viewH = ((const float *)m_listView->GetSize(0))[1];
	return scrollY > (contentH - 10.0f) - (viewH * 0.5f);
}

/* 0x811A1E4E */
void LiveAreaMoreCtrl::OnScroll()
{
	if (m_listView->GetSegmentHeader(m_segIdx) != NULL && IsScrolledToBottom())
		SetState(2, NULL);
}

/* 0x811A1E76 */
void LiveAreaMoreCtrl::OnScrollEvent(int a1, paf::ui::Handler *h, paf::ui::Event *ev, void *arg)
{
	(void)a1;
	(void)h;
	(void)ev;
	((LiveAreaMoreCtrl *)arg)->OnScroll();
}
