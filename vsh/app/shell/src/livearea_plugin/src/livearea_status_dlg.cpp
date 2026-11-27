/*
 * livearea_status_dlg.cpp: livearea_plugin activity status detail dialog
 * (0x811A078C-0x811A1348, sti 0x811A12D6).
 */
#include "livearea_internal.h"

#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

static void liveareaStatusDlgTimerCb(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata); /* 0x811A122E */

/* 0x811A078C */
LiveAreaStatusDlg::LiveAreaStatusDlg(
	int *pageKeyPtr, paf::ui::Widget *parent,
	const paf::intrusive_ptr<filelist::Live2Record> &record,
	int displayMode,
	const paf::common::SharedPtr<filelist::Live2AccessorProxy> &accessorProxy,
	const paf::common::SharedPtr<SceUInt64> &lastReadTime,
	const paf::intrusive_ptr<paf::cache::CacheImageInterface> &iconRef)
	: filelist::ActivityStatusDialog(
		  parent, g_liveareaPlugin, g_liveareaCache->m_manager,
		  record, displayMode, accessorProxy,
		  paf::common::SharedPtr<filelist::Live2VisualizerFactory>(
			  new LiveAreaSubVisualizer(record, lastReadTime))),
	  m_pageKey(pageKeyPtr != NULL ? *pageKeyPtr : -1),
	  m_locked(false),
	  m_lockRequested(false),
	  m_lastReadTime(lastReadTime),
	  m_iconRef(iconRef),
	  m_ownerAccessor(),
	  m_titleId(),
	  m_showUnreadBadge(false),
	  m_forceGameIcon(false)
{
	g_liveareaMgr->m_topmenuIf->setScrollEnabled(0);
}

/* 0x811A09E8 */
void LiveAreaStatusDlg::OnDialogClosed()
{
	LiveAreaPage *page = liveareaPageFindByAppId(m_pageKey);
	if (page != NULL && page->m_feed != NULL && m_locked) {
		page->m_feed->UnlockListView();
		m_locked = false;
	}
#if !defined(FW_TYPE_CEX)
	if (page != NULL && page->m_isActive)
		ScePafAutoTestTty_9E10E873(1, "/livespace/livearea_%s/activity",
					   page->m_titleId.c_str());
#endif
}

/* 0x811A0A14 / 0x811A0B4C */
LiveAreaStatusDlg::~LiveAreaStatusDlg()
{
	OnDialogClosed();
	g_liveareaMgr->m_topmenuIf->setScrollEnabled(1);
}

/* 0x811A0B5C */
void LiveAreaStatusDlg::SetupIcon()
{
	int category = filelist::BaseActivityVisualizer::ClassifyUriCategory(m_record);
	if (category != filelist::ACTIVITY_URI_CATEGORY_GAME)
		return;

	paf::IDParam actionContainerId(526555210U);
	paf::ui::Widget *actionContainer = m_listView->FindChild(actionContainerId, 0);
	if (actionContainer == NULL)
		return;
	paf::IDParam iconWidgetId((uint32_t)-1145406561);
	paf::ui::Widget *iconWidget = actionContainer->FindChild(iconWidgetId, 0);
	if (iconWidget == NULL)
		return;

	bool showIcon = m_forceGameIcon;
	if (!showIcon)
		showIcon = (liveareaIsGamePlayUri(m_record) != 0);

	if (!showIcon) {
		ScePafWidget_4588699E(iconWidget, NULL, 2, 0.0f, NULL);
		return;
	}

	if (m_displayMode == 1 && m_iconRef.get() != NULL) {
		paf::IDParam texId((uint32_t)-375432280);
		paf::intrusive_ptr<paf::graph::Surface> surf =
			paf::Framework::Instance()->GetCommonTexture(texId);
		iconWidget->SetTexture(surf, 0, 0);
	}
}

/* 0x811A0E32 */
void LiveAreaStatusDlg::OnDialogOpened()
{
	LiveAreaPage *page = liveareaPageFindByAppId(m_pageKey);
	if (page != NULL && page->m_feed != NULL) {
		page->m_feed->LockListView();
		m_locked = true;
	}
	m_lockRequested = true;
#if !defined(FW_TYPE_CEX)
	if (page != NULL && page->m_isActive && m_record.get() != NULL)
		ScePafAutoTestTty_9E10E873(1, "/livespace/livearea_%s/activity/comment_%lld",
					   page->m_titleId.c_str(),
					   m_record->m_rowid);
#endif
}

/* 0x811A0E5A */
void LiveAreaStatusDlg::BindContext(
	const paf::common::SharedPtr<filelist::Live2Accessor> &ownerAccessor,
	const char *titleId)
{
	m_ownerAccessor = ownerAccessor;
	m_titleId = titleId;
	if (m_dialogRoot != NULL) {
		m_dialogRoot->SetIntervalEvent(
			0x2000123, 16.0f, &liveareaStatusDlgTimerCb,
			reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId)));
	}
}

/* 0x811A0F4E */
bool LiveAreaStatusDlg::IsUnread()
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

/* 0x811A0F98 */
bool LiveAreaStatusDlg::ShouldShowTitleName()
{
	return m_showUnreadBadge;
}

/* 0x811A0F9E */
void LiveAreaStatusDlg::SetShowUnreadBadge(bool show)
{
	m_showUnreadBadge = show;
}

/* 0x811A0FA4 */
void LiveAreaStatusDlg::OnStatusUpdated()
{
}

/* 0x811A0FA6 */
void LiveAreaStatusDlg::SetForceGameIcon(bool force)
{
	m_forceGameIcon = force;
}

/* 0x811A0FAC */
bool LiveAreaStatusDlg::HandleSysUpdateError(int errorCode)
{
	(void)errorCode;
	liveareaFeedHandleSysUpdateErr((int)0x80550D12);
	return true;
}

/* 0x81409E2C */
int LiveAreaStatusDlg::GetVoteQueryType()
{
	return 0;
}

/* 0x811A0FBE */
const paf::IDParam &LiveAreaStatusDlg::GetBackButtonTemplateId()
{
	static paf::IDParam s_templateId((uint32_t)-337631049);
	return s_templateId;
}

/* 0x811A105A */
const paf::IDParam &LiveAreaStatusDlg::GetOptionButtonTemplateId()
{
	static paf::IDParam s_rootId(1483096796U);
	return s_rootId;
}

/* 0x811A10F6 */
const paf::IDParam &LiveAreaStatusDlg::GetBackButtonId()
{
	static paf::IDParam s_contentId(617383319U);
	return s_contentId;
}

/* 0x811A1192 */
const paf::IDParam &LiveAreaStatusDlg::GetOptionButtonId()
{
	static paf::IDParam s_subListId(1389538884U);
	return s_subListId;
}

/* 0x811A122E */
static void liveareaStatusDlgTimerCb(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	int dlgId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	LiveAreaStatusDlg *dlg =
		static_cast<LiveAreaStatusDlg *>(filelist::activityUiFindDialogById(dlgId));
	if (dlg == NULL)
		return;
	if (dlg->m_ownerAccessor.get() != NULL)
		dlg->m_ownerAccessor->MarkDirty();
	LiveAreaPage *page = liveareaPageFindByTitleId(dlg->m_titleId.c_str());
	if (page == NULL || !page->m_isActive || page->m_feed == NULL)
		return;

	page->ScrollToContent();
	if (page->m_pageKey != dlg->m_pageKey) {
		dlg->m_pageKey = page->m_pageKey;
		page->m_feed->SetDialogId(dlg->m_dialogId);
		if (dlg->m_lockRequested)
			dlg->OnDialogOpened();
	}
	paf::ui::Widget *rootWidget = page->m_rootWidget;
	if (rootWidget->GetWorldMatrix().w().extract_x() == 0.0f &&
	    page->m_inner.IsIntroTimerDone() != 0) {
		if (rootWidget != dlg->m_dialogRoot)
			dlg->m_dialogRoot->SetParent(rootWidget, NULL, 0);
	}
}
