/*
 * livearea_status_dlg.cpp: livearea_plugin activity status detail dialog
 * (0x811A078C-0x811A1348, sti 0x811A12D6). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <framework.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <widget/core/widget.h>
#include <graphics/surface/surface.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
struct LiveAreaMgr;
extern LiveAreaMgr *g_liveareaMgr;				/* 0x815670A8 */
struct LiveAreaCacheMgr;
extern LiveAreaCacheMgr *g_liveareaCache;			/* 0x81567080 */

extern void *liveareaPageFindByAppId(int appId);		/* 0x8119C0C6 */
extern void *liveareaPageFindByTitleId(const char *titleId);	/* 0x8119C12E */
extern int liveareaPageScrollToContent(void *page);		/* 0x8119BD0E */
extern void liveareaFeedUnlockListView(void *feed);		/* 0x8119E7A4 */
extern void liveareaFeedLockListView(void *feed);		/* 0x8119E7FC */
extern void liveareaFeedSetDialogId(void *feed, int id);	/* 0x8119E790 */
extern int liveareaFeedHandleSysUpdateErr(int err);		/* 0x8119E85A */
extern int liveareaIsGamePlayUri(paf::common::RefObj **item);	/* 0x8119F9EA */
extern int liveareaPageInnerSubAB8F8(void *inner);		/* 0x811AB8F8 */

extern void *liveareaStatusHelperCtor(void *mem, paf::common::RefObj **item, void *tsSp); /* 0x811A272C */
extern void liveareaBaseDlgCtor(void *self, paf::ui::Widget *w, paf::Plugin *p, void *cache,
				paf::common::RefObj **item, int type, void *ctxA, void *helper); /* 0x8135286C */
extern void liveareaBaseDlgDtor(void *self);			/* 0x81352A64 */
extern int liveareaGetItemCategory(paf::common::RefObj **item); /* 0x81349E1C */
extern void *liveareaGetIconShader(void);			/* 0x8139303C */
extern void *liveareaCheckDialogActive(int id);			/* 0x8135B002 */
extern void liveareaSub34C54C(void *ctx);			/* 0x8134C54C */

static void liveareaStatusDlgTimerCb(int eventId, int param1, int param2, int dlgId); /* 0x811A122E */

class LiveAreaStatusDlg {
public:
	LiveAreaStatusDlg(int *appIdPtr, paf::ui::Widget *parent, paf::common::RefObj **item,
			  int type, paf::common::SharedPtr<int> *ctxA,
			  paf::common::SharedPtr<SceUInt64> *tsSp, void **iconRef); /* 0x811A078C */
	virtual ~LiveAreaStatusDlg();				/* 0x811A0A14 / 0x811A0B4C */

	void UnlockFeed();					/* 0x811A09E8 */
	void SetupIcon();					/* 0x811A0B5C */
	int LockFeed();						/* 0x811A0E32 */
	void BindContext(paf::common::SharedPtr<int> *ctxB, const char *title); /* 0x811A0E5A */
	bool IsUnread();					/* 0x811A0F4E */
	bool GetFlagE8();					/* 0x811A0F98 */
	void SetFlagE8(bool v);					/* 0x811A0F9E */
	void Noop();						/* 0x811A0FA4 */
	void SetFlagE9(bool v);					/* 0x811A0FA6 */
	int TriggerSysUpdate();					/* 0x811A0FAC */

	static const paf::IDParam &GetId1();			/* 0x811A0FBE */
	static const paf::IDParam &GetId2();			/* 0x811A105A */
	static const paf::IDParam &GetId3();			/* 0x811A10F6 */
	static const paf::IDParam &GetId4();			/* 0x811A1192 */

	unsigned char m_base[188];				/* +0x04 */
	int m_appId;						/* +0xC0 */
	bool m_locked;						/* +0xC4 */
	bool m_lockRequested;					/* +0xC5 */
	unsigned char m_padC6[2];
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0xC8 */
	void *m_iconRef;					/* +0xD0 */
	paf::common::SharedPtr<int> m_ctxB;			/* +0xD4 */
	paf::string m_title;					/* +0xDC */
	bool m_flagE8;						/* +0xE8 */
	bool m_flagE9;						/* +0xE9 */
	unsigned char m_padEA[2];
};

/* 0x811A078C */
LiveAreaStatusDlg::LiveAreaStatusDlg(int *appIdPtr, paf::ui::Widget *parent,
				     paf::common::RefObj **item, int type,
				     paf::common::SharedPtr<int> *ctxA,
				     paf::common::SharedPtr<SceUInt64> *tsSp, void **iconRef)
	: m_appId(appIdPtr != NULL ? *appIdPtr : -1),
	  m_locked(false),
	  m_lockRequested(false),
	  m_lastReadTime(*tsSp),
	  m_iconRef(*iconRef),
	  m_ctxB(),
	  m_title(),
	  m_flagE8(false),
	  m_flagE9(false)
{
	void *helperMem = operator new(24);
	void *helperObj = NULL;
	if (helperMem != NULL) {
		paf::common::RefObj *it = *item;
		if (it != NULL)
			it->AddRef();
		helperObj = liveareaStatusHelperCtor(helperMem, &it, tsSp);
		if (it != NULL)
			it->Release();
	}
	paf::common::SharedPtr<int> helperSp((int *)helperObj);
	void *cacheMgr = *(void **)g_liveareaCache;
	liveareaBaseDlgCtor(this, parent, g_liveareaPlugin, cacheMgr, item, type, ctxA, &helperSp);

	if (m_iconRef != NULL) {
		typedef void (*AddRefFn)(void *);
		(*(AddRefFn **)(*(void **)m_iconRef))[6](m_iconRef);
	}
	typedef void (*TopmenuFn216)(int);
	void *topmenu = *((void **)g_liveareaMgr + 1);
	(*(TopmenuFn216 *)((char *)topmenu + 216))(0);
}

/* 0x811A09E8 */
void LiveAreaStatusDlg::UnlockFeed()
{
	void *page = liveareaPageFindByAppId(m_appId);
	if (page != NULL) {
		void *feed = *((void **)page + 62);
		if (feed != NULL && m_locked) {
			liveareaFeedUnlockListView(feed);
			m_locked = false;
		}
	}
}

/* 0x811A0A14 / 0x811A0B4C */
LiveAreaStatusDlg::~LiveAreaStatusDlg()
{
	UnlockFeed();
	typedef void (*TopmenuFn216)(int);
	void *topmenu = *((void **)g_liveareaMgr + 1);
	(*(TopmenuFn216 *)((char *)topmenu + 216))(1);
	if (m_iconRef != NULL) {
		typedef void (*ReleaseFn)(void *);
		(*(ReleaseFn **)(*(void **)m_iconRef))[7](m_iconRef);
	}
	liveareaBaseDlgDtor(this);
}

/* 0x811A0B5C */
void LiveAreaStatusDlg::SetupIcon()
{
	paf::common::RefObj *item = *(paf::common::RefObj **)((char *)this + 36);
	if (item != NULL)
		item->AddRef();
	int cat = liveareaGetItemCategory(&item);
	if (item != NULL)
		item->Release();
	if (cat != 2)
		return;

	paf::ui::Widget *root = *(paf::ui::Widget **)((char *)this + 48);
	paf::IDParam id1(526555210U);
	paf::ui::Widget *c1 = root->FindChild(id1, 0);
	if (c1 == NULL)
		return;
	paf::IDParam id2((uint32_t)-1145406561);
	paf::ui::Widget *c2 = c1->FindChild(id2, 0);
	if (c2 == NULL)
		return;

	bool show = m_flagE9;
	if (!show) {
		paf::common::RefObj *it2 = *(paf::common::RefObj **)((char *)this + 36);
		if (it2 != NULL)
			it2->AddRef();
		show = (liveareaIsGamePlayUri(&it2) != 0);
		if (it2 != NULL)
			it2->Release();
	}

	if (!show) {
		ScePafWidget_4588699E(c2, NULL, 2, 0.0f, NULL);
		return;
	}

	if (*(int *)((char *)this + 40) == 1 && m_iconRef != NULL) {
		paf::IDParam texId((uint32_t)-375432280);
		paf::intrusive_ptr<paf::graph::Surface> surf =
			paf::Framework::Instance()->GetCommonTexture(texId);
		c2->SetTexture(surf, 0, 0);
	}
}

/* 0x811A0E32 */
int LiveAreaStatusDlg::LockFeed()
{
	void *page = liveareaPageFindByAppId(m_appId);
	if (page != NULL && *((void **)page + 62) != NULL) {
		liveareaFeedLockListView(*((void **)page + 62));
		m_locked = true;
	}
	m_lockRequested = true;
	return 1;
}

/* 0x811A0E5A */
void LiveAreaStatusDlg::BindContext(paf::common::SharedPtr<int> *ctxB, const char *title)
{
	m_ctxB = *ctxB;
	m_title = title;
}

/* 0x811A0F4E */
bool LiveAreaStatusDlg::IsUnread()
{
	paf::common::RefObj *item = *(paf::common::RefObj **)((char *)this + 36);
	if (item != NULL)
		item->AddRef();
	typedef int (*GetTypeFn)(paf::common::RefObj *);
	bool unread = false;
	if ((*(GetTypeFn **)(*(void **)item))[2](item) == 1)
		unread = (*m_lastReadTime.get() < *((SceUInt64 *)item + 6));
	if (item != NULL)
		item->Release();
	return unread;
}

/* 0x811A0F98 */
bool LiveAreaStatusDlg::GetFlagE8()
{
	return m_flagE8;
}

/* 0x811A0F9E */
void LiveAreaStatusDlg::SetFlagE8(bool v)
{
	m_flagE8 = v;
}

/* 0x811A0FA4 */
void LiveAreaStatusDlg::Noop()
{
}

/* 0x811A0FA6 */
void LiveAreaStatusDlg::SetFlagE9(bool v)
{
	m_flagE9 = v;
}

/* 0x811A0FAC */
int LiveAreaStatusDlg::TriggerSysUpdate()
{
	liveareaFeedHandleSysUpdateErr((int)0x80550D12);
	return 1;
}

/* 0x811A0FBE */
const paf::IDParam &LiveAreaStatusDlg::GetId1()
{
	static paf::IDParam s_id((uint32_t)-337631049);
	return s_id;
}

/* 0x811A105A */
const paf::IDParam &LiveAreaStatusDlg::GetId2()
{
	static paf::IDParam s_id(1483096796U);
	return s_id;
}

/* 0x811A10F6 */
const paf::IDParam &LiveAreaStatusDlg::GetId3()
{
	static paf::IDParam s_id(617383319U);
	return s_id;
}

/* 0x811A1192 */
const paf::IDParam &LiveAreaStatusDlg::GetId4()
{
	static paf::IDParam s_id(1389538884U);
	return s_id;
}

/* 0x811A122E */
static void liveareaStatusDlgTimerCb(int eventId, int param1, int param2, int dlgId)
{
	(void)eventId;
	(void)param1;
	(void)param2;
	LiveAreaStatusDlg *dlg = (LiveAreaStatusDlg *)liveareaCheckDialogActive(dlgId);
	if (dlg == NULL)
		return;
	liveareaSub34C54C(&dlg->m_ctxB);
	void *page = liveareaPageFindByTitleId(dlg->m_title.c_str());
	if (page == NULL || *((unsigned char *)page + 45) == 0 || *((void **)page + 62) == NULL)
		return;

	liveareaPageScrollToContent(page);
	int pageAppId = *(int *)page;
	if (pageAppId != dlg->m_appId) {
		void *feed = *((void **)page + 62);
		int id = *((int *)dlg + 1);
		dlg->m_appId = pageAppId;
		liveareaFeedSetDialogId(feed, id);
		if (dlg->m_lockRequested) {
			typedef void (*Vfn25)(LiveAreaStatusDlg *);
			(*(Vfn25 **)(*(void **)dlg))[25](dlg);
		}
	}
	paf::ui::Widget *rootW = *((paf::ui::Widget **)page + 4);
	if ((*((unsigned char *)rootW + 387) & 7) != 0)
		rootW->UpdateMatrix();
	if (*(float *)((char *)rootW + 232) == 0.0f && liveareaPageInnerSubAB8F8((char *)page + 48) != 0) {
		paf::ui::Widget *dlgW = *(paf::ui::Widget **)((char *)dlg + 44);
		if (rootW != dlgW)
			dlgW->SetParent(rootW, NULL, 0);
	}
}
