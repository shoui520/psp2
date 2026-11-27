/*
 * livearea_visualizer.cpp: livearea_plugin activity list item & visualizer factories
 * (0x811A1EF4-0x811A2910, sti 0x811A289E).
 */
#include <widget/w_list_item.h>
#include "livearea_internal.h"

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
}

class LiveAreaFeedItem : public ActivityFeedItemBase {
public:
	LiveAreaFeedItem(int itemType, paf::common::SharedPtr<int> *feedContext,
			 int flags, int width, int height,
			 paf::common::SharedPtr<SceUInt64> *lastReadTime,
			 paf::cache::CacheImageInterface **iconRef); /* 0x811A1EF4 */
	virtual ~LiveAreaFeedItem() {}

	void OnClick();						/* 0x811A2016 */
	int SetupIcon(paf::ui::Widget *widget);			/* 0x811A2196 */
	bool IsUnread();					/* 0x811A2386 */
	bool HasComment();					/* 0x811A23CE */
	bool GetShowUnreadBadge();				/* 0x811A2406 */
	void OnItemUpdated();					/* 0x811A240C */

	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x6C */
	paf::cache::CacheImageInterface *m_iconRef;		/* +0x74 */
	bool m_showUnreadBadge;					/* +0x78 */
	bool m_forceGameIcon;					/* +0x79 */
	unsigned char itemFlagPadding[6];
};

class LiveAreaFeedVisualizer : public ActivityFeedVisualizerBase {
public:
	LiveAreaFeedVisualizer(paf::common::SharedPtr<SceUInt64> *lastReadTime,
			       paf::cache::CacheImageInterface **iconRef); /* 0x811A240E */
	virtual ~LiveAreaFeedVisualizer();			/* 0x811A2476 / 0x811A24F0 */

	void CreateItem(paf::common::RefObj **outItem, int itemType,
			paf::common::SharedPtr<int> *feedContext,
			int width, int height);			/* 0x811A2500 */
	void SetShowUnreadBadge(bool show);			/* 0x811A25B0 */
	bool GetShowUnreadBadge();				/* 0x811A25B4 */
	void SetForceGameIcon(bool force);			/* 0x811A25B8 */

	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x0C */
	paf::cache::CacheImageInterface *m_iconRef;		/* +0x14 */
	bool m_showUnreadBadge;					/* +0x18 */
	bool m_forceGameIcon;					/* +0x19 */
	unsigned char visFlagPadding[2];
};

class LiveAreaSubItem : public ActivitySubItemBase {
public:
	LiveAreaSubItem(int itemType, paf::common::SharedPtr<int> *feedContext,
			int flags, int width, int height, ActivityRecord **parentRecord,
			paf::common::SharedPtr<SceUInt64> *lastReadTime); /* 0x811A25BC */
	virtual ~LiveAreaSubItem() {}

	bool IsUnread();					/* 0x811A26E4 */

	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x6C */
	unsigned char subItemPadding[4];
};

class LiveAreaSubVisualizer : public ActivitySubVisualizerBase {
public:
	LiveAreaSubVisualizer(ActivityRecord **parentRecord,
			      paf::common::SharedPtr<SceUInt64> *lastReadTime); /* 0x811A272C */
	virtual ~LiveAreaSubVisualizer();			/* 0x811A27BE / 0x811A2842 */

	void CreateItem(paf::common::RefObj **outItem, int itemType,
			paf::common::SharedPtr<int> *feedContext,
			int width, int height);			/* 0x811A2852 */

	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x10 */
};

/* 0x811A1EF4 */
LiveAreaFeedItem::LiveAreaFeedItem(int itemType, paf::common::SharedPtr<int> *feedContext,
				   int flags, int width, int height,
				   paf::common::SharedPtr<SceUInt64> *lastReadTime,
				   paf::cache::CacheImageInterface **iconRef)
	: m_lastReadTime(*lastReadTime),
	  m_iconRef(*iconRef),
	  m_showUnreadBadge(false),
	  m_forceGameIcon(false)
{
	(void)flags;
	(void)width;
	(void)height;
	paf::common::SharedPtr<int> contextCopy(*feedContext);
	activityUiItemBaseCtor(this, itemType, g_liveareaPlugin,
			       g_liveareaCache->m_manager, &contextCopy);
	if (m_iconRef != NULL)
		m_iconRef->AddRef();
}

/* 0x811A2016 */
void LiveAreaFeedItem::OnClick()
{
	paf::ui::ListItem *listItem = m_listItem;
	paf::ui::ListView *listView = listItem->GetListView();
	LiveAreaPage *page = liveareaPageFindByListView(listView);
	if (page == NULL)
		return;

	if (g_liveareaMgr->m_netState == 2) {
		sceAppMgrLaunchAppByName(0x20000, "NPXS10016", "signin");
		return;
	}

	paf::ui::Widget *parent = listView->m_parent;
	ActivityRecord *record = m_record;
	if (record != NULL)
		record->AddRef();
	int displayMode = activityUiGetItemDisplayMode(this);
	paf::common::SharedPtr<int> feedContext;
	activityUiGetItemContext(&feedContext, this);
	paf::cache::CacheImageInterface *iconRef = m_iconRef;
	if (iconRef != NULL)
		iconRef->AddRef();

	LiveAreaStatusDlg *dlg = new LiveAreaStatusDlg(&page->m_pageKey, parent, &record,
						       displayMode, &feedContext,
						       &m_lastReadTime, &iconRef);
	if (iconRef != NULL)
		iconRef->Release();
	if (record != NULL)
		record->Release();

	dlg->SetShowUnreadBadge(m_showUnreadBadge);
	dlg->SetForceGameIcon(m_forceGameIcon);
	activityUiStatusDlgOpen(dlg, listItem);
	dlg->SetupIcon();
	ScePafWidget_AC0C81D5(parent, 0.0f);
	if (page->m_feed != NULL)
		page->m_feed->SetDialogId(dlg->m_dialogId);
}

extern int activityUiGetItemCategoryForItem(ActivityFeedItemBase *self);

/* 0x811A2196 */
int LiveAreaFeedItem::SetupIcon(paf::ui::Widget *widget)
{
	if (activityUiGetItemCategoryForItem(this) != 2)
		return 0;
	if (!m_forceGameIcon) {
		ActivityRecord *record = m_record;
		if (record != NULL)
			record->AddRef();
		bool isPlay = (liveareaIsGamePlayUri(&record) != 0);
		if (record != NULL)
			record->Release();
		if (!isPlay) {
			ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
			return 1;
		}
	}
	if (activityUiGetItemDisplayMode(this) != 1)
		return 0;
	paf::IDParam texId((uint32_t)-375432280);
	paf::intrusive_ptr<paf::graph::Surface> surf =
		paf::Framework::Instance()->GetCommonTexture(texId);
	widget->SetTexture(surf, 0, 0);
	return 1;
}

/* 0x811A2386 */
bool LiveAreaFeedItem::IsUnread()
{
	ActivityRecord *record = m_record;
	if (record != NULL)
		record->AddRef();
	bool unread = false;
	if (record->GetRecordKind() == 1)
		unread = (*m_lastReadTime.get() < record->m_updateTimestamp);
	if (record != NULL)
		record->Release();
	return unread;
}

/* 0x811A23CE */
bool LiveAreaFeedItem::HasComment()
{
	ActivityRecord *record = m_record;
	if (record != NULL)
		record->AddRef();
	bool hasComment = false;
	if (record->GetRecordKind() == 1)
		hasComment = (record->m_commentCount != 0);
	if (record != NULL)
		record->Release();
	return hasComment;
}

/* 0x811A2406 */
bool LiveAreaFeedItem::GetShowUnreadBadge()
{
	return m_showUnreadBadge;
}

/* 0x811A240C */
void LiveAreaFeedItem::OnItemUpdated()
{
}

/* 0x811A240E */
LiveAreaFeedVisualizer::LiveAreaFeedVisualizer(paf::common::SharedPtr<SceUInt64> *lastReadTime,
					       paf::cache::CacheImageInterface **iconRef)
	: m_lastReadTime(*lastReadTime),
	  m_iconRef(*iconRef),
	  m_showUnreadBadge(false),
	  m_forceGameIcon(false)
{
	activityUiFeedVisualizerBaseCtor(this, g_liveareaPlugin, g_liveareaCache->m_manager);
	if (m_iconRef != NULL)
		m_iconRef->AddRef();
}

/* 0x811A2476 / 0x811A24F0 */
LiveAreaFeedVisualizer::~LiveAreaFeedVisualizer()
{
	if (m_iconRef != NULL)
		m_iconRef->Release();
}

/* 0x811A2500 */
void LiveAreaFeedVisualizer::CreateItem(paf::common::RefObj **outItem, int itemType,
					paf::common::SharedPtr<int> *feedContext,
					int width, int height)
{
	paf::cache::CacheImageInterface *iconRef = m_iconRef;
	if (iconRef != NULL)
		iconRef->AddRef();
	LiveAreaFeedItem *item = new LiveAreaFeedItem(itemType, feedContext, 0, width, height,
						      &m_lastReadTime, &iconRef);
	*outItem = item;
	if (item != NULL)
		item->AddRef();
	if (iconRef != NULL)
		iconRef->Release();
	item->m_showUnreadBadge = m_showUnreadBadge;
	item->m_forceGameIcon = m_forceGameIcon;
}

/* 0x811A25B0 */
void LiveAreaFeedVisualizer::SetShowUnreadBadge(bool show)
{
	m_showUnreadBadge = show;
}

/* 0x811A25B4 */
bool LiveAreaFeedVisualizer::GetShowUnreadBadge()
{
	return m_showUnreadBadge;
}

/* 0x811A25B8 */
void LiveAreaFeedVisualizer::SetForceGameIcon(bool force)
{
	m_forceGameIcon = force;
}

/* 0x811A25BC */
LiveAreaSubItem::LiveAreaSubItem(int itemType, paf::common::SharedPtr<int> *feedContext,
				 int flags, int width, int height, ActivityRecord **parentRecord,
				 paf::common::SharedPtr<SceUInt64> *lastReadTime)
	: m_lastReadTime(*lastReadTime)
{
	(void)flags;
	(void)width;
	(void)height;
	paf::common::SharedPtr<int> contextCopy(*feedContext);
	ActivityRecord *parentRec = *parentRecord;
	if (parentRec != NULL)
		parentRec->AddRef();
	activityUiSubItemBaseCtor(this, itemType, g_liveareaPlugin,
				  g_liveareaCache->m_manager, &contextCopy);
	if (parentRec != NULL)
		parentRec->Release();
}

/* 0x811A26E4 */
bool LiveAreaSubItem::IsUnread()
{
	ActivityRecord *record = m_record;
	if (record != NULL)
		record->AddRef();
	bool unread = false;
	if (record->GetRecordKind() == 2)
		unread = (*m_lastReadTime.get() < record->m_updateTimestamp);
	if (record != NULL)
		record->Release();
	return unread;
}

/* 0x811A272C */
LiveAreaSubVisualizer::LiveAreaSubVisualizer(ActivityRecord **parentRecord,
					     paf::common::SharedPtr<SceUInt64> *lastReadTime)
	: m_lastReadTime(*lastReadTime)
{
	m_parentRecord = NULL;
	ActivityRecord *rec = *parentRecord;
	if (rec != NULL)
		rec->AddRef();
	activityUiSubVisualizerBaseCtor(this, g_liveareaPlugin, g_liveareaCache->m_manager, &rec);
	if (rec != NULL)
		rec->Release();
}

/* 0x811A27BE / 0x811A2842 */
LiveAreaSubVisualizer::~LiveAreaSubVisualizer()
{
	if (m_parentRecord != NULL)
		m_parentRecord->Release();
}

/* 0x811A2852 */
void LiveAreaSubVisualizer::CreateItem(paf::common::RefObj **outItem, int itemType,
				       paf::common::SharedPtr<int> *feedContext,
				       int width, int height)
{
	LiveAreaSubItem *item = new LiveAreaSubItem(itemType, feedContext, 0, width, height,
						    &m_parentRecord, &m_lastReadTime);
	*outItem = item;
	if (item != NULL)
		item->AddRef();
}

LiveAreaFeedVisualizer *liveareaFeedVisualizerCtor(void *mem,
						   paf::common::SharedPtr<SceUInt64> *lastReadTime,
						   paf::cache::CacheImageInterface **iconRef)
{
	return new (mem) LiveAreaFeedVisualizer(lastReadTime, iconRef);
}

LiveAreaSubVisualizer *liveareaSubVisualizerCtor(void *mem, ActivityRecord **record,
						 paf::common::SharedPtr<SceUInt64> *lastReadTime)
{
	return new (mem) LiveAreaSubVisualizer(record, lastReadTime);
}
