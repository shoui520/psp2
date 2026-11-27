/*
 * livearea_visualizer.cpp: livearea_plugin activity list item & visualizer factories
 * (0x811A1EF4-0x811A2910, sti 0x811A289E).
 */
#include <widget/w_list_item.h>
#include "livearea_internal.h"

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
}

class LiveAreaFeedItem : public filelist::Lv1StatusActivityVisualizer {
public:
	LiveAreaFeedItem(paf::ui::Widget *parent,
			 const paf::common::SharedPtr<filelist::Live2Iterator> &iterator,
			 SceInt64 cellId,
			 const paf::common::SharedPtr<SceUInt64> &lastReadTime,
			 const paf::intrusive_ptr<paf::cache::CacheImageInterface> &iconRef); /* 0x811A1EF4 */
	virtual ~LiveAreaFeedItem() {}

	virtual bool IsUnread();				/* 0x811A2386 */
	virtual void OnPanelDecide(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event);	/* 0x811A2016 */
	virtual bool CustomizeActionIcon(paf::ui::Widget *iconWidget); /* 0x811A2196 */
	virtual bool HasNewComment();				/* 0x811A23CE */
	virtual bool ShouldShowTitleName();			/* 0x811A2406 */

	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x6C */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_iconRef; /* +0x74 */
	bool m_showUnreadBadge;					/* +0x78 */
	bool m_forceGameIcon;					/* +0x79 */
};

class LiveAreaSubItem : public filelist::Lv2CommentActivityVisualizer {
public:
	LiveAreaSubItem(paf::ui::Widget *parent,
			const paf::common::SharedPtr<filelist::Live2Iterator> &iterator,
			SceInt64 cellId,
			const paf::intrusive_ptr<filelist::Live2Record> &parentRecord,
			const paf::common::SharedPtr<SceUInt64> &lastReadTime); /* 0x811A25BC */
	virtual ~LiveAreaSubItem() {}

	virtual bool IsUnread();				/* 0x811A26E4 */

	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x6C */
};

/* 0x811A1EF4 */
LiveAreaFeedItem::LiveAreaFeedItem(paf::ui::Widget *parent,
				   const paf::common::SharedPtr<filelist::Live2Iterator> &iterator,
				   SceInt64 cellId,
				   const paf::common::SharedPtr<SceUInt64> &lastReadTime,
				   const paf::intrusive_ptr<paf::cache::CacheImageInterface> &iconRef)
	: filelist::Lv1StatusActivityVisualizer(parent, g_liveareaPlugin,
						g_liveareaCache->m_manager,
						iterator, cellId),
	  m_lastReadTime(lastReadTime),
	  m_iconRef(iconRef),
	  m_showUnreadBadge(false),
	  m_forceGameIcon(false)
{
}

/* 0x811A2016 */
void LiveAreaFeedItem::OnPanelDecide(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event)
{
	(void)eventId;
	(void)handler;
	(void)event;
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
	int displayMode = GetDisplayMode();
	paf::common::SharedPtr<filelist::Live2AccessorProxy> accessorProxy = GetAccessorProxy();

	LiveAreaStatusDlg *dlg = new LiveAreaStatusDlg(&page->m_pageKey, parent, m_record,
						       displayMode, accessorProxy,
						       m_lastReadTime, m_iconRef);
	dlg->SetShowUnreadBadge(m_showUnreadBadge);
	dlg->SetForceGameIcon(m_forceGameIcon);
	dlg->Open(listItem);
	dlg->SetupIcon();
	ScePafWidget_AC0C81D5(parent, 0.0f);
	if (page->m_feed != NULL)
		page->m_feed->SetDialogId(dlg->m_dialogId);
}

/* 0x811A2196 */
bool LiveAreaFeedItem::CustomizeActionIcon(paf::ui::Widget *iconWidget)
{
	if (GetCategory() != filelist::ACTIVITY_URI_CATEGORY_GAME)
		return false;
	if (!m_forceGameIcon) {
		if (liveareaIsGamePlayUri(m_record) == 0) {
			ScePafWidget_4588699E(iconWidget, NULL, 2, 0.0f, NULL);
			return true;
		}
	}
	if (GetDisplayMode() != 1)
		return false;
	paf::IDParam texId((uint32_t)-375432280);
	paf::intrusive_ptr<paf::graph::Surface> surf =
		paf::Framework::Instance()->GetCommonTexture(texId);
	iconWidget->SetTexture(surf, 0, 0);
	return true;
}

/* 0x811A2386 */
bool LiveAreaFeedItem::IsUnread()
{
	if (m_record.get() != NULL &&
	    m_record->GetRecordKind() == filelist::LIVE2_RECORD_STATUS) {
		filelist::Live2StatusRecord *statusRec =
			static_cast<filelist::Live2StatusRecord *>(m_record.get());
		return (*m_lastReadTime.get() <
			static_cast<SceUInt64>(statusRec->m_status.updateTime));
	}
	return false;
}

/* 0x811A23CE */
bool LiveAreaFeedItem::HasNewComment()
{
	if (m_record.get() != NULL &&
	    m_record->GetRecordKind() == filelist::LIVE2_RECORD_STATUS) {
		filelist::Live2StatusRecord *statusRec =
			static_cast<filelist::Live2StatusRecord *>(m_record.get());
		return (statusRec->m_status.numOfNewComments != 0);
	}
	return false;
}

/* 0x811A2406 */
bool LiveAreaFeedItem::ShouldShowTitleName()
{
	return m_showUnreadBadge;
}

/* 0x811A240E */
LiveAreaFeedVisualizer::LiveAreaFeedVisualizer(
	const paf::common::SharedPtr<SceUInt64> &lastReadTime,
	const paf::intrusive_ptr<paf::cache::CacheImageInterface> &iconRef)
	: filelist::Lv1StatusVisualizerFactory(g_liveareaPlugin, g_liveareaCache->m_manager),
	  m_lastReadTime(lastReadTime),
	  m_iconRef(iconRef),
	  m_showUnreadBadge(false),
	  m_forceGameIcon(false)
{
}

/* 0x811A2476 / 0x811A24F0 */
LiveAreaFeedVisualizer::~LiveAreaFeedVisualizer()
{
}

/* 0x811A2500 */
paf::intrusive_ptr<paf::ui::filelist::Visualizer> LiveAreaFeedVisualizer::CreateVisualizer(
	paf::ui::Widget *parent,
	const paf::common::SharedPtr<filelist::Live2Iterator> &iterator,
	int index, SceInt64 cellId)
{
	(void)index;
	LiveAreaFeedItem *item = new LiveAreaFeedItem(parent, iterator, cellId,
						      m_lastReadTime, m_iconRef);
	item->m_showUnreadBadge = m_showUnreadBadge;
	item->m_forceGameIcon = m_forceGameIcon;
	return paf::intrusive_ptr<paf::ui::filelist::Visualizer>(item);
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
LiveAreaSubItem::LiveAreaSubItem(paf::ui::Widget *parent,
				 const paf::common::SharedPtr<filelist::Live2Iterator> &iterator,
				 SceInt64 cellId,
				 const paf::intrusive_ptr<filelist::Live2Record> &parentRecord,
				 const paf::common::SharedPtr<SceUInt64> &lastReadTime)
	: filelist::Lv2CommentActivityVisualizer(parent, g_liveareaPlugin,
						 g_liveareaCache->m_manager,
						 iterator, cellId, parentRecord),
	  m_lastReadTime(lastReadTime)
{
}

/* 0x811A26E4 */
bool LiveAreaSubItem::IsUnread()
{
	if (m_record.get() != NULL &&
	    m_record->GetRecordKind() == filelist::LIVE2_RECORD_COMMENT) {
		filelist::Live2CommentRecord *commentRec =
			static_cast<filelist::Live2CommentRecord *>(m_record.get());
		return (*m_lastReadTime.get() <
			static_cast<SceUInt64>(commentRec->m_comment.creationTime));
	}
	return false;
}

/* 0x811A272C */
LiveAreaSubVisualizer::LiveAreaSubVisualizer(
	const paf::intrusive_ptr<filelist::Live2Record> &parentRecord,
	const paf::common::SharedPtr<SceUInt64> &lastReadTime)
	: filelist::Lv2CommentVisualizerFactory(g_liveareaPlugin, g_liveareaCache->m_manager,
						parentRecord),
	  m_lastReadTime(lastReadTime)
{
}

/* 0x811A27BE / 0x811A2842 */
LiveAreaSubVisualizer::~LiveAreaSubVisualizer()
{
}

/* 0x811A2852 */
paf::intrusive_ptr<paf::ui::filelist::Visualizer> LiveAreaSubVisualizer::CreateVisualizer(
	paf::ui::Widget *parent,
	const paf::common::SharedPtr<filelist::Live2Iterator> &iterator,
	int index, SceInt64 cellId)
{
	(void)index;
	LiveAreaSubItem *item = new LiveAreaSubItem(parent, iterator, cellId,
						    m_parentRecord, m_lastReadTime);
	return paf::intrusive_ptr<paf::ui::filelist::Visualizer>(item);
}
