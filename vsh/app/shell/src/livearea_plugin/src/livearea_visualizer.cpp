/*
 * livearea_visualizer.cpp: livearea_plugin activity list item & visualizer factories
 * (0x811A1EF4-0x811A2910, sti 0x811A289E). The file and class names are ours (PLACEHOLDER).
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
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <graphics/surface/surface.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
struct LiveAreaMgr;
extern LiveAreaMgr *g_liveareaMgr;				/* 0x815670A8 */
struct LiveAreaCacheMgr;
extern LiveAreaCacheMgr *g_liveareaCache;			/* 0x81567080 */

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
}

extern void *liveareaPageFindByListView(void *lv);		/* 0x8119C0FA */
extern void liveareaFeedSetDialogId(void *feed, int id);	/* 0x8119E790 */
extern int liveareaIsGamePlayUri(paf::common::RefObj **item);	/* 0x8119F9EA */
extern void *liveareaStatusDlgCtor(void *mem, int *appIdPtr, paf::ui::Widget *parent,
				   paf::common::RefObj **item, int type, void *ctxA,
				   void *tsSp, void **iconRef);	/* 0x811A078C */
extern void liveareaStatusDlgShow(void *dlg);			/* 0x811A0B5C */
extern void liveareaStatusDlgSetFlagE8(void *dlg, bool v);	/* 0x811A0F9E */
extern void liveareaStatusDlgSetFlagE9(void *dlg, bool v);	/* 0x811A0FA6 */
extern void liveareaStatusDlgSub31DA(void *dlg, void *item);	/* 0x813531DA */

extern void liveareaBaseItemCtor(void *self, int itemType, paf::Plugin *p, void *cache, void *ctx); /* 0x8134DA7E */
extern int liveareaGetItemType(void *self);			/* 0x812FEA62 */
extern void liveareaGetItemCtx(void *outSp, void *self);	/* 0x8134D860 */
extern int liveareaSub349C32(void);				/* 0x81349C32 */
extern void *liveareaGetIconShader(void);			/* 0x8139303C */
extern void liveareaBaseVisCtor(void *self, paf::Plugin *p, void *cache); /* 0x813508E8 */
extern void liveareaBaseSubItemCtor(void *self, int itemType, paf::Plugin *p, void *cache, void *ctx); /* 0x8135B39E */
extern void liveareaBaseSubVisCtor(void *self, paf::Plugin *p, void *cache, paf::common::RefObj **item); /* 0x8135D0DC */

class LiveAreaFeedItem : public paf::common::RefObj {
public:
	LiveAreaFeedItem(int itemType, paf::common::SharedPtr<int> *ctxSp, int flags, int width, int height,
			 paf::common::SharedPtr<SceUInt64> *tsSp, void **iconRef); /* 0x811A1EF4 */
	virtual ~LiveAreaFeedItem() {}

	void OnClick();						/* 0x811A2016 */
	int SetupIcon(paf::ui::Widget *w);			/* 0x811A2196 */
	bool IsUnread();					/* 0x811A2386 */
	bool HasComment();					/* 0x811A23CE */
	bool GetFlag78();					/* 0x811A2406 */
	void Noop();						/* 0x811A240C */

	unsigned char m_base[100];				/* +0x08 */
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x6C */
	void *m_iconRef;					/* +0x74 */
	bool m_flag78;						/* +0x78 */
	bool m_flag79;						/* +0x79 */
	unsigned char m_pad7A[6];
};

class LiveAreaFeedVisualizer {
public:
	LiveAreaFeedVisualizer(paf::common::SharedPtr<SceUInt64> *tsSp, void **iconRef); /* 0x811A240E */
	virtual ~LiveAreaFeedVisualizer();			/* 0x811A2476 / 0x811A24F0 */

	void CreateItem(paf::common::RefObj **out, int itemType, paf::common::SharedPtr<int> *ctxSp, int width, int height); /* 0x811A2500 */
	void SetFlag1(bool v);					/* 0x811A25B0 */
	bool GetFlag1();					/* 0x811A25B4 */
	void SetFlag2(bool v);					/* 0x811A25B8 */

	unsigned char m_base[8];				/* +0x04 */
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x0C */
	void *m_iconRef;					/* +0x14 */
	bool m_flag1;						/* +0x18 */
	bool m_flag2;						/* +0x19 */
	unsigned char m_pad1A[2];
};

class LiveAreaSubItem : public paf::common::RefObj {
public:
	LiveAreaSubItem(int itemType, paf::common::SharedPtr<int> *ctxSp, int flags, int width, int height,
			paf::common::RefObj **item, paf::common::SharedPtr<SceUInt64> *tsSp); /* 0x811A25BC */
	virtual ~LiveAreaSubItem() {}

	bool IsUnread();					/* 0x811A26E4 */

	unsigned char m_base[100];				/* +0x08 */
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x6C */
	unsigned char m_pad74[4];
};

class LiveAreaSubVisualizer {
public:
	LiveAreaSubVisualizer(paf::common::RefObj **item, paf::common::SharedPtr<SceUInt64> *tsSp); /* 0x811A272C */
	virtual ~LiveAreaSubVisualizer();			/* 0x811A27BE / 0x811A2842 */

	void CreateItem(paf::common::RefObj **out, int itemType, paf::common::SharedPtr<int> *ctxSp, int width, int height); /* 0x811A2852 */

	unsigned char m_base[8];				/* +0x04 */
	paf::common::RefObj *m_item;				/* +0x0C */
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x10 */
};

/* 0x811A1EF4 */
LiveAreaFeedItem::LiveAreaFeedItem(int itemType, paf::common::SharedPtr<int> *ctxSp, int flags, int width, int height,
				   paf::common::SharedPtr<SceUInt64> *tsSp, void **iconRef)
	: m_lastReadTime(*tsSp),
	  m_iconRef(*iconRef),
	  m_flag78(false),
	  m_flag79(false)
{
	(void)flags;
	(void)width;
	(void)height;
	paf::common::SharedPtr<int> sp(*ctxSp);
	liveareaBaseItemCtor(this, itemType, g_liveareaPlugin, *(void **)g_liveareaCache, &sp);
	if (m_iconRef != NULL) {
		typedef void (*AddRefFn)(void *);
		(*(AddRefFn **)(*(void **)m_iconRef))[6](m_iconRef);
	}
}

/* 0x811A2016 */
void LiveAreaFeedItem::OnClick()
{
	paf::ui::ListItem *li = *(paf::ui::ListItem **)((char *)this + 8);
	paf::ui::ListView *lv = li->GetListView();
	int *page = (int *)liveareaPageFindByListView(lv);
	if (page == NULL)
		return;

	if (*((int *)g_liveareaMgr + 4) == 2) {
		sceAppMgrLaunchAppByName(0x20000, "NPXS10016", "signin");
		return;
	}

	void *dlg = NULL;
	void *mem = operator new(240);
	if (mem != NULL) {
		paf::ui::Widget *parent = *(paf::ui::Widget **)((char *)li->GetListView() + 360);
		paf::common::RefObj *item = *(paf::common::RefObj **)((char *)this + 68);
		if (item != NULL)
			item->AddRef();
		int type = liveareaGetItemType(this);
		paf::common::SharedPtr<int> ctx;
		liveareaGetItemCtx(&ctx, this);
		void *icon = m_iconRef;
		if (icon != NULL) {
			typedef void (*AddRefFn)(void *);
			(*(AddRefFn **)(*(void **)icon))[6](icon);
		}
		dlg = liveareaStatusDlgCtor(mem, page, parent, &item, type, &ctx, &m_lastReadTime, &icon);
		if (icon != NULL) {
			typedef void (*ReleaseFn)(void *);
			(*(ReleaseFn **)(*(void **)icon))[7](icon);
		}
		if (item != NULL)
			item->Release();
	}
	liveareaStatusDlgSetFlagE8(dlg, m_flag78);
	liveareaStatusDlgSetFlagE9(dlg, m_flag79);
	liveareaStatusDlgSub31DA(dlg, li);
	liveareaStatusDlgShow(dlg);
	ScePafWidget_AC0C81D5(*(paf::ui::Widget **)((char *)li->GetListView() + 360), 0.0f);
	void *feed = *((void **)page + 62);
	if (feed != NULL)
		liveareaFeedSetDialogId(feed, *((int *)dlg + 1));
}

/* 0x811A2196 */
int LiveAreaFeedItem::SetupIcon(paf::ui::Widget *w)
{
	if (liveareaSub349C32() != 2)
		return 0;
	if (!m_flag79) {
		paf::common::RefObj *item = *(paf::common::RefObj **)((char *)this + 68);
		if (item != NULL)
			item->AddRef();
		bool isPlay = (liveareaIsGamePlayUri(&item) != 0);
		if (item != NULL)
			item->Release();
		if (!isPlay) {
			ScePafWidget_4588699E(w, NULL, 2, 0.0f, NULL);
			return 1;
		}
	}
	if (liveareaGetItemType(this) != 1)
		return 0;
	paf::IDParam texId((uint32_t)-375432280);
	paf::intrusive_ptr<paf::graph::Surface> surf =
		paf::Framework::Instance()->GetCommonTexture(texId);
	w->SetTexture(surf, 0, 0);
	return 1;
}

/* 0x811A2386 */
bool LiveAreaFeedItem::IsUnread()
{
	paf::common::RefObj *item = *(paf::common::RefObj **)((char *)this + 68);
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

/* 0x811A23CE */
bool LiveAreaFeedItem::HasComment()
{
	paf::common::RefObj *item = *(paf::common::RefObj **)((char *)this + 68);
	if (item != NULL)
		item->AddRef();
	typedef int (*GetTypeFn)(paf::common::RefObj *);
	bool res = false;
	if ((*(GetTypeFn **)(*(void **)item))[2](item) == 1)
		res = (*((int *)item + 30) != 0);
	if (item != NULL)
		item->Release();
	return res;
}

/* 0x811A2406 */
bool LiveAreaFeedItem::GetFlag78()
{
	return m_flag78;
}

/* 0x811A240C */
void LiveAreaFeedItem::Noop()
{
}

/* 0x811A240E */
LiveAreaFeedVisualizer::LiveAreaFeedVisualizer(paf::common::SharedPtr<SceUInt64> *tsSp, void **iconRef)
	: m_lastReadTime(*tsSp),
	  m_iconRef(*iconRef),
	  m_flag1(false),
	  m_flag2(false)
{
	liveareaBaseVisCtor(this, g_liveareaPlugin, *(void **)g_liveareaCache);
	if (m_iconRef != NULL) {
		typedef void (*AddRefFn)(void *);
		(*(AddRefFn **)(*(void **)m_iconRef))[6](m_iconRef);
	}
}

/* 0x811A2476 / 0x811A24F0 */
LiveAreaFeedVisualizer::~LiveAreaFeedVisualizer()
{
	if (m_iconRef != NULL) {
		typedef void (*ReleaseFn)(void *);
		(*(ReleaseFn **)(*(void **)m_iconRef))[7](m_iconRef);
	}
}

/* 0x811A2500 */
void LiveAreaFeedVisualizer::CreateItem(paf::common::RefObj **out, int itemType,
					paf::common::SharedPtr<int> *ctxSp, int width, int height)
{
	void *icon = m_iconRef;
	if (icon != NULL) {
		typedef void (*AddRefFn)(void *);
		(*(AddRefFn **)(*(void **)icon))[6](icon);
	}
	LiveAreaFeedItem *item = new LiveAreaFeedItem(itemType, ctxSp, 0, width, height, &m_lastReadTime, &icon);
	*out = item;
	if (item != NULL)
		item->AddRef();
	if (icon != NULL) {
		typedef void (*ReleaseFn)(void *);
		(*(ReleaseFn **)(*(void **)icon))[7](icon);
	}
	item->m_flag78 = m_flag1;
	item->m_flag79 = m_flag2;
}

/* 0x811A25B0 */
void LiveAreaFeedVisualizer::SetFlag1(bool v)
{
	m_flag1 = v;
}

/* 0x811A25B4 */
bool LiveAreaFeedVisualizer::GetFlag1()
{
	return m_flag1;
}

/* 0x811A25B8 */
void LiveAreaFeedVisualizer::SetFlag2(bool v)
{
	m_flag2 = v;
}

/* 0x811A25BC */
LiveAreaSubItem::LiveAreaSubItem(int itemType, paf::common::SharedPtr<int> *ctxSp, int flags, int width, int height,
				 paf::common::RefObj **item, paf::common::SharedPtr<SceUInt64> *tsSp)
	: m_lastReadTime(*tsSp)
{
	(void)flags;
	(void)width;
	(void)height;
	paf::common::SharedPtr<int> sp(*ctxSp);
	paf::common::RefObj *it = *item;
	if (it != NULL)
		it->AddRef();
	liveareaBaseSubItemCtor(this, itemType, g_liveareaPlugin, *(void **)g_liveareaCache, &sp);
	if (it != NULL)
		it->Release();
}

/* 0x811A26E4 */
bool LiveAreaSubItem::IsUnread()
{
	paf::common::RefObj *item = *(paf::common::RefObj **)((char *)this + 68);
	if (item != NULL)
		item->AddRef();
	typedef int (*GetTypeFn)(paf::common::RefObj *);
	bool unread = false;
	if ((*(GetTypeFn **)(*(void **)item))[2](item) == 2)
		unread = (*m_lastReadTime.get() < *((SceUInt64 *)item + 6));
	if (item != NULL)
		item->Release();
	return unread;
}

/* 0x811A272C */
LiveAreaSubVisualizer::LiveAreaSubVisualizer(paf::common::RefObj **item, paf::common::SharedPtr<SceUInt64> *tsSp)
	: m_item(NULL),
	  m_lastReadTime(*tsSp)
{
	paf::common::RefObj *it = *item;
	if (it != NULL)
		it->AddRef();
	liveareaBaseSubVisCtor(this, g_liveareaPlugin, *(void **)g_liveareaCache, &it);
	if (it != NULL)
		it->Release();
}

/* 0x811A27BE / 0x811A2842 */
LiveAreaSubVisualizer::~LiveAreaSubVisualizer()
{
	if (m_item != NULL)
		m_item->Release();
}

/* 0x811A2852 */
void LiveAreaSubVisualizer::CreateItem(paf::common::RefObj **out, int itemType,
				       paf::common::SharedPtr<int> *ctxSp, int width, int height)
{
	LiveAreaSubItem *item = new LiveAreaSubItem(itemType, ctxSp, 0, width, height, &m_item, &m_lastReadTime);
	*out = item;
	if (item != NULL)
		item->AddRef();
}
