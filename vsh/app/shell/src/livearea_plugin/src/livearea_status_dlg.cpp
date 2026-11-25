/*
 * livearea_status_dlg.cpp: livearea_plugin activity status detail dialog
 * (0x811A078C-0x811A1348, sti 0x811A12D6).
 */
#include "livearea_internal.h"

static void liveareaStatusDlgTimerCb(int eventId, int eventArg, int eventData, int dlgId); /* 0x811A122E */

/* 0x811A078C */
LiveAreaStatusDlg::LiveAreaStatusDlg(int *pageKeyPtr, paf::ui::Widget *parent,
				     ActivityRecord **record, int displayMode,
				     paf::common::SharedPtr<int> *feedContext,
				     paf::common::SharedPtr<SceUInt64> *lastReadTime,
				     paf::cache::CacheImageInterface **iconRef)
	: m_pageKey(pageKeyPtr != NULL ? *pageKeyPtr : -1),
	  m_locked(false),
	  m_lockRequested(false),
	  m_lastReadTime(*lastReadTime),
	  m_iconRef(*iconRef),
	  m_ownerContext(),
	  m_titleId(),
	  m_showUnreadBadge(false),
	  m_forceGameIcon(false)
{
	void *subVisMem = operator new(24);
	LiveAreaSubVisualizer *subVis = NULL;
	if (subVisMem != NULL) {
		ActivityRecord *rec = *record;
		if (rec != NULL)
			rec->AddRef();
		subVis = liveareaSubVisualizerCtor(subVisMem, &rec, lastReadTime);
		if (rec != NULL)
			rec->Release();
	}
	paf::common::SharedPtr<LiveAreaSubVisualizer> subVisRef(subVis);
	activityUiStatusDlgBaseCtor(this, parent, g_liveareaPlugin, g_liveareaCache->m_manager,
				    record, displayMode, feedContext, &subVisRef);

	if (m_iconRef != NULL)
		m_iconRef->AddRef();
	g_liveareaMgr->m_topmenuIf->setScrollEnabled(0);
}

/* 0x811A09E8 */
void LiveAreaStatusDlg::UnlockFeed()
{
	LiveAreaPage *page = liveareaPageFindByAppId(m_pageKey);
	if (page != NULL && page->m_feed != NULL && m_locked) {
		page->m_feed->UnlockListView();
		m_locked = false;
	}
}

/* 0x811A0A14 / 0x811A0B4C */
LiveAreaStatusDlg::~LiveAreaStatusDlg()
{
	UnlockFeed();
	g_liveareaMgr->m_topmenuIf->setScrollEnabled(1);
	if (m_iconRef != NULL)
		m_iconRef->Release();
	activityUiStatusDlgBaseDtor(this);
}

/* 0x811A0B5C */
void LiveAreaStatusDlg::SetupIcon()
{
	ActivityRecord *record = m_record;
	if (record != NULL)
		record->AddRef();
	int category = activityUiGetItemCategory(&record);
	if (record != NULL)
		record->Release();
	if (category != 2)
		return;

	paf::IDParam actionContainerId(526555210U);
	paf::ui::Widget *actionContainer = m_contentRoot->FindChild(actionContainerId, 0);
	if (actionContainer == NULL)
		return;
	paf::IDParam iconWidgetId((uint32_t)-1145406561);
	paf::ui::Widget *iconWidget = actionContainer->FindChild(iconWidgetId, 0);
	if (iconWidget == NULL)
		return;

	bool showIcon = m_forceGameIcon;
	if (!showIcon) {
		ActivityRecord *checkRec = m_record;
		if (checkRec != NULL)
			checkRec->AddRef();
		showIcon = (liveareaIsGamePlayUri(&checkRec) != 0);
		if (checkRec != NULL)
			checkRec->Release();
	}

	if (!showIcon) {
		ScePafWidget_4588699E(iconWidget, NULL, 2, 0.0f, NULL);
		return;
	}

	if (m_displayMode == 1 && m_iconRef != NULL) {
		paf::IDParam texId((uint32_t)-375432280);
		paf::intrusive_ptr<paf::graph::Surface> surf =
			paf::Framework::Instance()->GetCommonTexture(texId);
		iconWidget->SetTexture(surf, 0, 0);
	}
}

/* 0x811A0E32 */
int LiveAreaStatusDlg::LockFeed()
{
	LiveAreaPage *page = liveareaPageFindByAppId(m_pageKey);
	if (page != NULL && page->m_feed != NULL) {
		page->m_feed->LockListView();
		m_locked = true;
	}
	m_lockRequested = true;
	return 1;
}

/* 0x811A0E5A */
void LiveAreaStatusDlg::BindContext(paf::common::SharedPtr<int> *ownerContext, const char *titleId)
{
	m_ownerContext = *ownerContext;
	m_titleId = titleId;
}

/* 0x811A0F4E */
bool LiveAreaStatusDlg::IsUnread()
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

/* 0x811A0F98 */
bool LiveAreaStatusDlg::GetShowUnreadBadge()
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
int LiveAreaStatusDlg::TriggerSysUpdate()
{
	liveareaFeedHandleSysUpdateErr((int)0x80550D12);
	return 1;
}

/* 0x811A0FBE */
const paf::IDParam &LiveAreaStatusDlg::GetDialogTemplateId()
{
	static paf::IDParam s_templateId((uint32_t)-337631049);
	return s_templateId;
}

/* 0x811A105A */
const paf::IDParam &LiveAreaStatusDlg::GetDialogRootId()
{
	static paf::IDParam s_rootId(1483096796U);
	return s_rootId;
}

/* 0x811A10F6 */
const paf::IDParam &LiveAreaStatusDlg::GetContentRootId()
{
	static paf::IDParam s_contentId(617383319U);
	return s_contentId;
}

/* 0x811A1192 */
const paf::IDParam &LiveAreaStatusDlg::GetSubListId()
{
	static paf::IDParam s_subListId(1389538884U);
	return s_subListId;
}

/* 0x811A122E */
static void liveareaStatusDlgTimerCb(int eventId, int eventArg, int eventData, int dlgId)
{
	(void)eventId;
	(void)eventArg;
	(void)eventData;
	LiveAreaStatusDlg *dlg = (LiveAreaStatusDlg *)activityUiFindDialogById(dlgId);
	if (dlg == NULL)
		return;
	activityUiRefreshContext(&dlg->m_ownerContext);
	LiveAreaPage *page = liveareaPageFindByTitleId(dlg->m_titleId.c_str());
	if (page == NULL || !page->m_isActive || page->m_feed == NULL)
		return;

	page->ScrollToContent();
	if (page->m_pageKey != dlg->m_pageKey) {
		dlg->m_pageKey = page->m_pageKey;
		page->m_feed->SetDialogId(dlg->m_dialogId);
		if (dlg->m_lockRequested)
			dlg->LockFeed();
	}
	paf::ui::Widget *rootWidget = page->m_rootWidget;
	if (rootWidget->GetWorldMatrix().w().extract_x() == 0.0f &&
	    page->m_inner.IsIntroTimerDone() != 0) {
		if (rootWidget != dlg->m_dialogRoot)
			dlg->m_dialogRoot->SetParent(rootWidget, NULL, 0);
	}
}
