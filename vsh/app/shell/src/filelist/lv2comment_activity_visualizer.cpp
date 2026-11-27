/*
 * lv2comment_activity_visualizer.cpp: level-2 activity comment visualizer,
 * factory, and segment busy footer (0x8135B29C..0x8135D7EF).
 */
#include "filelist_internal.h"
#include <common/char_encoding.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/w_button_base.h>
#include <widget/w_busyindicator.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <std/string.h>

namespace filelist {

namespace {

static const char s_lv2CommentVisualizerTypeName[] = "lv2comment_activity_visualizer";

/* PAF widget, template, and texture resource IDs for comment rows */
enum {
	TEMPLATE_LV2_COMMENT_CARD       = -1797516029,
	WIDGET_LV2_COMMENT_CARD_ROOT    =  1105919704,
	WIDGET_LV2_COMMENT_AVATAR_BTN   = -1275049805,
	WIDGET_LV2_COMMENT_OPTION_BTN   =   146511389,
	WIDGET_LV2_COMMENT_BODY_PLANE   = -1424815565,
	TEMPLATE_COMMENT_BUSY_OVERLAY   =   213765718,
	WIDGET_COMMENT_BUSY_INDICATOR   =  -515153258,
	TEMPLATE_COMMENT_POST_FLASH     =  1498982009,
	WIDGET_COMMENT_POST_FLASH       =  -800904207,
	TEX_COMMENT_PANEL_BASE          = -1066268994,
	TEX_COMMENT_PANEL_MASK          =  -797768180,
	TEMPLATE_BUSY_FOOTER            =  -258258900,
	WIDGET_BUSY_FOOTER_ROOT         = -2046896109,
	WIDGET_BUSY_FOOTER_INDICATOR    = -1361887340,
	WIDGET_BUSY_FOOTER_TEXT         =  1316990501
};

enum {
	COMMENT_DRAWOBJ_PANEL  = 0,
	COMMENT_DRAWOBJ_UNREAD = 1,
	COMMENT_DRAWOBJ_BODY   = 2,
	COMMENT_DRAWOBJ_TIME   = 3
};

enum {
	EVENT_WIDGET_SIZE_CHANGED  = 0x00100004,
	EVENT_CACHE_IMAGE_READY    = 0x00100005,
	EVENT_BUTTON_DECIDE        = 0x10000008,
	ANIM_CHANNEL_META_ALPHA    = 0x00010003,
	ANIM_CHANNEL_SIZE          = 0x00010004
};

} /* namespace */

/* -------------------------------------------------------------------------- */
/* Lv2CommentActivityVisualizer                                               */
/* -------------------------------------------------------------------------- */

/* 0x8135B29C */
bool Lv2CommentActivityVisualizer::IsInherit(const char *type) const
{
	if (type == s_lv2CommentVisualizerTypeName)
		return true;
	return BaseActivityVisualizer::IsInherit(type);
}

/* 0x8135B2B4 */
const char *Lv2CommentActivityVisualizer::GetTypeName() const
{
	return s_lv2CommentVisualizerTypeName;
}

const char *Lv2CommentActivityVisualizer::TypeName()
{
	return s_lv2CommentVisualizerTypeName;
}

/* 0x8135B2C0 / 0x8135B39E */
Lv2CommentActivityVisualizer::Lv2CommentActivityVisualizer(
	paf::ui::Widget *parent, paf::Plugin *plugin,
	paf::cache::CacheManager *cacheManager,
	const paf::common::SharedPtr<Live2Iterator> &iterator,
	SceInt64 cellId,
	const paf::intrusive_ptr<Live2Record> &parentRecord)
	: BaseActivityVisualizer(parent, plugin, cacheManager, iterator, cellId),
	  m_parentRecord(parentRecord)
{
	RequestRecordUpdate();
}

/* 0x8135B3BA / 0x8135B462 / 0x8135B472 */
Lv2CommentActivityVisualizer::~Lv2CommentActivityVisualizer()
{
}

/* 0x8135B47A */
void Lv2CommentActivityVisualizer::AnimateNewComment(paf::ui::Widget *commentWidget)
{
	commentWidget->SetMetaAlpha(0.0f, NULL, ANIM_CHANNEL_META_ALPHA, 0, 0.0f, NULL, NULL);
	commentWidget->SetMetaAlpha(1.0f, new paf::Timer(184.0f), ANIM_CHANNEL_META_ALPHA, 0,
				    200.0f, NULL, NULL);

	paf::ui::Widget *parentWidget = commentWidget->m_parent;
	paf::Plugin *commonPlugin = paf::Framework::Instance()->GetCommonResourcePlugin();
	paf::Plugin::TemplateOpenParam openParam;
	commonPlugin->TemplateOpen(parentWidget, paf::IDParam(TEMPLATE_COMMENT_POST_FLASH),
				   openParam);

	paf::ui::Widget *flashWidget =
		parentWidget->FindChild(paf::IDParam(WIDGET_COMMENT_POST_FLASH), 0);
	if (flashWidget != NULL) {
		PafVec4 startSize(30.0f, 30.0f, 0.0f, 0.0f);
		flashWidget->SetSize(startSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);

		PafVec4 baseSize = *commentWidget->GetSize(0);
		PafVec4 endSize(baseSize.extract_x() - 30.0f,
				baseSize.extract_y() - 10.0f,
				baseSize.extract_z(),
				baseSize.extract_w());
		flashWidget->SetSize(endSize, new paf::Timer(233.0f), ANIM_CHANNEL_SIZE, 0,
				     0.0f, NULL, NULL);
		flashWidget->SetMetaAlpha(0.0f, NULL, ANIM_CHANNEL_META_ALPHA, 0,
					  0.0f, NULL, NULL);
		flashWidget->SetMetaAlpha(0.5f, new paf::Timer(100.0f), ANIM_CHANNEL_META_ALPHA, 0,
					  0.0f, NULL, NULL);
		flashWidget->SetMetaAlpha(0.0f, new paf::Timer(184.0f), ANIM_CHANNEL_META_ALPHA, 0,
					  100.0f, NULL, NULL);
		ScePafWidget_5A9793B0(flashWidget, 284.0f);
	}
}

/* 0x8135B7FA */
void Lv2CommentActivityVisualizer::OnCommentSizeReady(int eventId, paf::ui::Handler *handler,
						      paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	paf::ui::Widget *bodyWidget = static_cast<paf::ui::Widget *>(handler);
	bool shouldAnimate = (userdata != NULL);

	paf::graph::DrawObj *bodyObj = bodyWidget->GetDrawObj(COMMENT_DRAWOBJ_BODY);
	paf::graph::DrawObj *timeObj = bodyWidget->GetDrawObj(COMMENT_DRAWOBJ_TIME);
	paf::graph::DrawObj *unreadObj = bodyWidget->GetDrawObj(COMMENT_DRAWOBJ_UNREAD);
	paf::ui::ListItem *listItem = paf::ui::ListItem::GetListItem(bodyWidget);

	float bodyHeight = bodyObj->m_size.extract_y();
	PafVec4 currentSize = *bodyWidget->GetSize(0);
	float cellHeight = bodyHeight + 30.0f;
	if (listItem == NULL)
		return;

	if (cellHeight < 110.0f)
		cellHeight = 110.0f;

	PafVec4 newSize(currentSize.extract_x(), cellHeight,
			currentSize.extract_z(), currentSize.extract_w());
	listItem->SetSize(newSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);

	paf::ui::ListView *listView = listItem->GetListView();
	int32_t segIdx = listItem->GetSegmentIndex();
	int32_t cellIdx = listItem->GetCellIndex();
	listView->SetCellSize(segIdx, cellIdx, newSize);

	PafVec3 bodyPos(bodyObj->m_pos.extract_x(),
			(cellHeight * 0.5f) - (bodyHeight * 0.5f + 10.0f),
			bodyObj->m_pos.extract_z());
	bodyObj->SetPos(bodyPos);

	PafVec3 timePos(timeObj->m_pos.extract_x(),
			(cellHeight * 0.5f) - 29.0f,
			timeObj->m_pos.extract_z());
	timeObj->SetPos(timePos);

	PafVec3 unreadPos(unreadObj->m_pos.extract_x(),
			  (newSize.extract_y() * 0.5f) - 23.0f,
			  unreadObj->m_pos.extract_z());
	unreadObj->SetPos(unreadPos);

	paf::ui::Widget *cardWidget = bodyWidget->m_parent;
	cardWidget->Show(0xff, new paf::Timer(50.0f), NULL, 0.0f);
	if (shouldAnimate && cardWidget != NULL)
		AnimateNewComment(cardWidget);

	bodyWidget->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED, NULL, NULL);
}

/* 0x8135BAB4 */
void Lv2CommentActivityVisualizer::UpdateCommentTime(paf::graph::TextObj *timeText)
{
	if (timeText == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_COMMENT)
		return;

	Live2CommentRecord *commentRec = static_cast<Live2CommentRecord *>(rec.get());
	paf::wstring formatted =
		FormatActivityTime(static_cast<SceUInt64>(commentRec->m_comment.creationTime));
	timeText->SetString(formatted);
}

/* 0x8135BB30 */
void Lv2CommentActivityVisualizer::UpdateCommentText(paf::ui::Widget *panel,
						     paf::ui::Widget *bodyWidget,
						     bool newlyCreated)
{
	(void)panel;
	if (bodyWidget == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	paf::graph::TextObj *bodyText = GetBodyTextObj(bodyWidget);
	if (bodyText != NULL && rec.get() != NULL && rec->GetRecordKind() == LIVE2_RECORD_COMMENT) {
		Live2CommentRecord *commentRec = static_cast<Live2CommentRecord *>(rec.get());
		paf::string ownerName(commentRec->m_comment.ownerId);
		TruncateAtChar(ownerName, '@');

		paf::string fullText = ownerName + "\n" + commentRec->m_comment.message;
		if (!fullText.empty()) {
			paf::wstring ownerWide;
			paf::common::Utf8ToUtf16(ownerName.c_str(), &ownerWide);
			paf::wstring fullWide;
			paf::common::Utf8ToUtf16(fullText.c_str(), &fullWide);

			bodyText->SetString(fullWide);
			bodyText->SetStyleAttribute(paf::graph::TextStyleAttribute_Bold,
						    0, 0, false);
			bodyText->SetStyleAttribute(paf::graph::TextStyleAttribute_Bold,
						    0, ownerWide.size(), true);
		}
	}

	UpdateCommentTime(GetTimeTextObj(bodyWidget));

	bool shouldAnimate = false;
	if (newlyCreated && GetDisplayMode() == 2 &&
	    m_parentRecord.get() != NULL &&
	    m_parentRecord->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *parentStatus =
			static_cast<Live2StatusRecord *>(m_parentRecord.get());
		ActivityStatusDialog *dialog =
			activityUiFindDialogByStatusRowid(&parentStatus->m_status.rowid);
		if (dialog != NULL && dialog->m_animateNewComment &&
		    dialog->m_commentCount == m_listItem->GetCellIndex() + 1) {
			shouldAnimate = true;
			dialog->SetAnimateNewComment(false);
		}
	}

	if (bodyWidget->m_prepare_count != 0) {
		paf::ui::Widget *cardWidget = bodyWidget->m_parent;
		cardWidget->Hide(0xff, NULL, NULL, 0.0f);
		bodyWidget->AddEventCallback(
			EVENT_WIDGET_SIZE_CHANGED,
			&Lv2CommentActivityVisualizer::OnCommentSizeReady,
			shouldAnimate ? reinterpret_cast<void *>(1) : NULL);
	} else {
		bodyWidget->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED, NULL, NULL);
		OnCommentSizeReady(0, bodyWidget, NULL,
				   shouldAnimate ? reinterpret_cast<void *>(1) : NULL);
	}
}

/* 0x8135BFB0 */
void Lv2CommentActivityVisualizer::UpdateCommentPanel(paf::ui::Widget *panel,
						      paf::ui::Widget *bodyWidget)
{
	if (panel == NULL || bodyWidget == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_COMMENT)
		return;

	Live2CommentRecord *commentRec = static_cast<Live2CommentRecord *>(rec.get());
	paf::string panelUrl(commentRec->m_comment.announcePanelUrl);
	SceUInt32 bgColor = commentRec->m_comment.announcePanelBgColor;
	int ownerType = commentRec->m_comment.ownerType;

	paf::graph::DrawObj *existingObj = bodyWidget->GetDrawObj(COMMENT_DRAWOBJ_PANEL);
	if (existingObj == NULL ||
	    existingObj->GetDrawObjType() != ActivityDrawObj::DrawObjType()) {
		paf::common::SharedPtr<paf::graph::DrawObj> newObj(new ActivityDrawObj());
		bodyWidget->SwapDrawObj(newObj, COMMENT_DRAWOBJ_PANEL);
	}

	ActivityDrawObj *panelObj =
		static_cast<ActivityDrawObj *>(bodyWidget->GetDrawObj(COMMENT_DRAWOBJ_PANEL));
	if (panelObj != NULL) {
		panelObj->SetMode(0);
		panelObj->SetVariant(1);
		SceUInt32 swappedColor = 0;
		SwapColorEndian(bgColor, &swappedColor);
		panelObj->m_color = swappedColor;

		if (ownerType != ACTIVITY_DB_OWNER_TITLE &&
		    (ownerType == ACTIVITY_DB_OWNER_SELF || ownerType == ACTIVITY_DB_OWNER_USER)) {
			paf::intrusive_ptr<paf::graph::Surface> baseTex =
				paf::Framework::Instance()->GetCommonTexture(
					paf::IDParam(TEX_COMMENT_PANEL_BASE));
			bodyWidget->SetTexture(baseTex, COMMENT_DRAWOBJ_PANEL, 0);

			if (!panelUrl.empty()) {
				paf::intrusive_ptr<paf::graph::Surface> maskTex =
					paf::Framework::Instance()->GetCommonTexture(
						paf::IDParam(TEX_COMMENT_PANEL_MASK));
				bodyWidget->SetTexture(maskTex, COMMENT_DRAWOBJ_PANEL, 1);

				paf::intrusive_ptr<paf::cache::CacheImageInterface> imageCache;
				CreateActivityImageCache(panelUrl, imageCache, m_cacheManager);
				bodyWidget->AddEventCallback(
					EVENT_CACHE_IMAGE_READY,
					&Lv1StatusActivityVisualizer::OnPanelImageLoaded,
					reinterpret_cast<void *>(1));
				bodyWidget->SetDrawObjMaskBit(&imageCache,
							      COMMENT_DRAWOBJ_PANEL, 2);
			}
		}
	}

	paf::ui::Widget *avatarWidget =
		panel->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_AVATAR_BTN), 0);
	if (avatarWidget != NULL)
		ApplyAvatarIcon(avatarWidget);
}

/* 0x8135C388 */
void Lv2CommentActivityVisualizer::StartCommentBusyOnButton(paf::ui::Widget *optionButton)
{
	if (optionButton->FindChild(paf::IDParam(WIDGET_COMMENT_BUSY_INDICATOR), 0) != NULL)
		return;

	paf::Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(optionButton, paf::IDParam(TEMPLATE_COMMENT_BUSY_OVERLAY),
			       openParam);
	paf::ui::BusyIndicator *indicator = static_cast<paf::ui::BusyIndicator *>(
		optionButton->FindChild(paf::IDParam(WIDGET_COMMENT_BUSY_INDICATOR), 0));
	if (indicator != NULL)
		indicator->Start();
}

/* 0x8135C4E4 */
void Lv2CommentActivityVisualizer::ShowCommentBusy()
{
	if (m_listItem == NULL)
		return;

	paf::ui::ButtonBase *optionBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_OPTION_BTN), 0));
	if (optionBtn != NULL) {
		optionBtn->Disable(false);
		StartCommentBusyOnButton(optionBtn);
	}
}

/* 0x8135C586 */
void Lv2CommentActivityVisualizer::StopCommentBusyOnButton(paf::ui::Widget *optionButton)
{
	paf::ui::BusyIndicator *indicator = static_cast<paf::ui::BusyIndicator *>(
		optionButton->FindChild(paf::IDParam(WIDGET_COMMENT_BUSY_INDICATOR), 0));
	if (indicator != NULL) {
		indicator->Stop();
		ScePafWidget_5A9793B0(indicator, 0.0f);
		indicator->m_name = paf::IDParam("");
	}
}

/* 0x8135C684 / 0x8135CCA0 */
void Lv2CommentActivityVisualizer::OnRecordUpdated()
{
	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_COMMENT)
		return;

	Live2CommentRecord *commentRec = static_cast<Live2CommentRecord *>(rec.get());
	SetDisplayMode(commentRec->m_comment.ownerType);
	UpdateCategory();

	int displayMode = GetDisplayMode();
	if (displayMode != ACTIVITY_DB_OWNER_SELF && displayMode != ACTIVITY_DB_OWNER_USER) {
		paf::ui::Widget *existing =
			m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_CARD_ROOT), 0);
		if (existing != NULL)
			existing->DestroyWidget();
		return;
	}

	bool newlyCreated = false;
	paf::ui::Widget *cardWidget =
		m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_CARD_ROOT), 0);
	if (cardWidget == NULL) {
		OpenTemplate(m_listItem, paf::IDParam(TEMPLATE_LV2_COMMENT_CARD));
		cardWidget = m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_CARD_ROOT), 0);
		if (cardWidget == NULL)
			return;
		cardWidget->SetParent(cardWidget->m_parent, NULL, 6);
		newlyCreated = true;
	}

	paf::ui::Widget *avatarBtn =
		cardWidget->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_AVATAR_BTN), 0);
	PafVec4 itemSize = *m_listItem->GetSize(0);
	PafVec4 cardSize = *cardWidget->GetSize(0);
	PafVec4 mergedSize(itemSize.extract_x(), cardSize.extract_y(),
			   itemSize.extract_z(), itemSize.extract_w());
	m_listItem->SetSize(mergedSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);

	if (avatarBtn != NULL) {
		avatarBtn->AddEventCallback(
			EVENT_BUTTON_DECIDE,
			&Lv1StatusActivityVisualizer::AvatarDecideThunk, this);
	}

	paf::ui::Widget *optionBtn =
		cardWidget->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_OPTION_BTN), 0);
	if (optionBtn != NULL) {
		int optionMode = 0;
		if (displayMode == ACTIVITY_DB_OWNER_SELF) {
			optionMode = 1;
		} else if (displayMode == ACTIVITY_DB_OWNER_USER &&
			   m_parentRecord.get() != NULL &&
			   m_parentRecord->GetRecordKind() == LIVE2_RECORD_STATUS) {
			Live2StatusRecord *parentStatus =
				static_cast<Live2StatusRecord *>(m_parentRecord.get());
			if (parentStatus->m_status.ownerType == ACTIVITY_DB_OWNER_SELF)
				optionMode = 2;
		}

		StopCommentBusyOnButton(optionBtn);
		if (optionMode == 1) {
			optionBtn->AddEventCallback(
				EVENT_BUTTON_DECIDE,
				&Lv2CommentActivityVisualizer::DeleteOptionDecideThunk, this);
			if (activityUiIsCommentDeleting(&commentRec->m_comment.rowid))
				ShowCommentBusy();
		} else if (optionMode == 2) {
			optionBtn->AddEventCallback(
				EVENT_BUTTON_DECIDE,
				&Lv1StatusActivityVisualizer::ActionIconDecideThunk, this);
		} else {
			optionBtn->AddEventCallback(EVENT_BUTTON_DECIDE, NULL, NULL);
			optionBtn->Hide(0xff, NULL, NULL, 0.0f);
		}
	}

	paf::ui::Widget *bodyWidget =
		cardWidget->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_BODY_PLANE), 0);
	if (bodyWidget != NULL) {
		UpdateCommentPanel(cardWidget, bodyWidget);
		UpdateCommentText(cardWidget, bodyWidget, newlyCreated);
		UpdateUnreadBadge();
	} else {
		cardWidget->Hide(0xff, NULL, NULL, 0.0f);
	}
}

/* 0x8135CCB2 */
void Lv2CommentActivityVisualizer::Stop()
{
	Live2Visualizer::Stop();
	if (m_parentRecord.get() != NULL &&
	    m_parentRecord->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *parentStatus =
			static_cast<Live2StatusRecord *>(m_parentRecord.get());
		ActivityStatusDialog *dialog =
			activityUiFindDialogByStatusRowid(&parentStatus->m_status.rowid);
		if (dialog != NULL)
			dialog->ClosePopupIfSelectedComment(this);
	}
}

/* 0x8135CCE2 */
void Lv2CommentActivityVisualizer::OnDeleteFinished(int errorCode)
{
	if (errorCode == 0 || m_listItem == NULL)
		return;

	paf::ui::ButtonBase *optionBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_OPTION_BTN), 0));
	if (optionBtn != NULL) {
		optionBtn->Enable(false);
		StopCommentBusyOnButton(optionBtn);
	}
}

/* 0x8135CD88 */
void Lv2CommentActivityVisualizer::OnGriefReportFinished()
{
	if (m_listItem == NULL)
		return;

	paf::ui::ButtonBase *optionBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_OPTION_BTN), 0));
	if (optionBtn != NULL) {
		optionBtn->Enable(false);
		StopCommentBusyOnButton(optionBtn);
	}
}

/* 0x8135CE2A */
paf::graph::TextObj *Lv2CommentActivityVisualizer::GetTimeTextObj(paf::ui::Widget *panel)
{
	paf::graph::DrawObj *obj = panel->GetDrawObj(COMMENT_DRAWOBJ_TIME);
	if (obj != NULL && obj->IsInherit(paf::graph::TextObj::DrawObjType()))
		return static_cast<paf::graph::TextObj *>(obj);
	return NULL;
}

/* Comment body TextObj (slot 2) */
paf::graph::TextObj *Lv2CommentActivityVisualizer::GetBodyTextObj(paf::ui::Widget *panel)
{
	paf::graph::DrawObj *obj = panel->GetDrawObj(COMMENT_DRAWOBJ_BODY);
	if (obj != NULL && obj->IsInherit(paf::graph::TextObj::DrawObjType()))
		return static_cast<paf::graph::TextObj *>(obj);
	return NULL;
}

/* 0x8135CE72 */
paf::ui::Widget *Lv2CommentActivityVisualizer::GetSubWidget(int kind)
{
	if (kind == 0)
		return m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_BODY_PLANE), 0);
	if (kind == 1)
		return m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_AVATAR_BTN), 0);
	if (kind == 2)
		return m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_OPTION_BTN), 0);
	return NULL;
}

/* 0x8135CFDA */
void Lv2CommentActivityVisualizer::OnDeleteOptionDecide(int eventId, paf::ui::Handler *handler,
							paf::ui::Event *event)
{
	(void)eventId;
	(void)event;
	if (m_parentRecord.get() != NULL &&
	    m_parentRecord->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *parentStatus =
			static_cast<Live2StatusRecord *>(m_parentRecord.get());
		activityUiOpenDeleteCommentMenu(&parentStatus->m_status.rowid,
						static_cast<paf::ui::Widget *>(handler), this);
	}
}

/* 0x8135CFE4 */
void Lv2CommentActivityVisualizer::OnDeleteOrReportOptionDecide(int eventId,
								paf::ui::Handler *handler,
								paf::ui::Event *event)
{
	(void)eventId;
	(void)event;
	if (m_parentRecord.get() != NULL &&
	    m_parentRecord->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *parentStatus =
			static_cast<Live2StatusRecord *>(m_parentRecord.get());
		activityUiOpenDeleteOrReportCommentMenu(&parentStatus->m_status.rowid,
							static_cast<paf::ui::Widget *>(handler),
							this);
	}
}

/* 0x8135CFEE */
void Lv2CommentActivityVisualizer::UpdateUnreadBadge()
{
	paf::ui::Widget *bodyWidget =
		m_listItem->FindChild(paf::IDParam(WIDGET_LV2_COMMENT_BODY_PLANE), 0);
	if (bodyWidget != NULL) {
		paf::graph::DrawObj *unreadObj = bodyWidget->GetDrawObj(COMMENT_DRAWOBJ_UNREAD);
		bool visible = IsUnread();
		unreadObj->SetVisible(visible);
	}
}

/* 0x8135D0C8 */
void Lv2CommentActivityVisualizer::DeleteOptionDecideThunk(int eventId, paf::ui::Handler *handler,
							   paf::ui::Event *event, void *userdata)
{
	Lv2CommentActivityVisualizer *self = static_cast<Lv2CommentActivityVisualizer *>(userdata);
	self->OnDeleteOptionDecide(eventId, handler, event);
}

/* -------------------------------------------------------------------------- */
/* Lv2CommentVisualizerFactory                                                */
/* -------------------------------------------------------------------------- */

/* 0x8135D0DC */
Lv2CommentVisualizerFactory::Lv2CommentVisualizerFactory(
	paf::Plugin *plugin, paf::cache::CacheManager *cacheManager,
	const paf::intrusive_ptr<Live2Record> &parentRecord)
	: m_plugin(plugin),
	  m_cacheManager(cacheManager),
	  m_parentRecord(parentRecord)
{
}

Lv2CommentVisualizerFactory::~Lv2CommentVisualizerFactory()
{
}

/* 0x8135D100 */
paf::intrusive_ptr<paf::ui::filelist::Visualizer> Lv2CommentVisualizerFactory::CreateVisualizer(
	paf::ui::Widget *parent,
	const paf::common::SharedPtr<Live2Iterator> &iterator,
	int index, SceInt64 cellId)
{
	(void)index;
	paf::intrusive_ptr<paf::ui::filelist::Visualizer> vis(
		new Lv2CommentActivityVisualizer(parent, m_plugin, m_cacheManager,
						 iterator, cellId, m_parentRecord));
	return vis;
}

/* 0x8135D20E */
PafVec4 Lv2CommentVisualizerFactory::GetVisualSize()
{
	return PafVec4(832.0f, 110.0f, 0.0f, 0.0f);
}

/* -------------------------------------------------------------------------- */
/* ActivityBusyFooter                                                         */
/* -------------------------------------------------------------------------- */

/* 0x8135D224 */
ActivityBusyFooter::ActivityBusyFooter(int segmentIndex, paf::Plugin *plugin)
	: m_segmentIndex(segmentIndex),
	  m_plugin(plugin),
	  m_listView(NULL),
	  m_state(0)
{
}

/* 0x8135D240 / 0x8135D24E */
ActivityBusyFooter::~ActivityBusyFooter()
{
}

/* 0x8135D264 */
void ActivityBusyFooter::SetState(int state, paf::ui::Widget *header)
{
	if (m_listView == NULL || state == m_state)
		return;

	m_state = state;
	if (header == NULL)
		header = m_listView->GetSegmentHeader(m_segmentIndex);

	if (m_state == 1 && header != NULL)
		header->Hide(0xff, NULL, NULL, 0.0f);
}

/* 0x8135D2D2 */
void ActivityBusyFooter::AttachSegment(bool showHeader)
{
	if (m_listView == NULL)
		return;

	paf::intrusive_ptr<BasicSegmentSource> segSource(
		static_cast<BasicSegmentSource *>(m_listView->m_segment_source.get()));
	if (segSource.get() == NULL)
		return;

	paf::ui::filelist::SegmentListAccessor::OptionParam option;
	option.m_layout_type = paf::ui::ListView::LAYOUT_TYPE_LIST;
	option.m_header = NULL;
	option.m_header_size = PafVec4(0.0f, 0.0f, 0.0f, 0.0f);
	option.m_header_floating = false;
	option.m_header_disappearance = false;
	option.m_column_num = 0;
	option.m_min_size = PafVec2(0.0f, 0.0f);
	option.m_joined = true;

	paf::ui::Widget *headerWidget = NULL;
	if (showHeader) {
		paf::Plugin::TemplateOpenParam openParam;
		m_plugin->TemplateOpen(m_listView, GetTemplateId(), openParam);
		headerWidget = m_listView->FindChild(GetHeaderId(), 0);
		option.m_header = headerWidget;
	}

	paf::intrusive_ptr<paf::ui::filelist::CellSource> emptySource;
	segSource->InsertSegment(m_segmentIndex, emptySource, option);
	if (showHeader)
		SetState(1, headerWidget);
	else
		SetState(0, NULL);
}

/* 0x8135D3F4 */
void ActivityBusyFooter::DetachSegment()
{
	if (m_listView == NULL)
		return;

	paf::intrusive_ptr<BasicSegmentSource> segSource(
		static_cast<BasicSegmentSource *>(m_listView->m_segment_source.get()));
	if (segSource.get() == NULL)
		return;

	segSource->RemoveSegment(m_segmentIndex);
	SetState(0, NULL);
}

/* 0x8135D422 */
void ActivityBusyFooter::ShowBusyHeader()
{
	if (m_listView == NULL || m_state == 1)
		return;

	paf::Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(m_listView, GetTemplateId(), openParam);
	paf::ui::Widget *headerWidget = m_listView->FindChild(GetHeaderId(), 0);
	m_listView->SetSegmentHeader(m_segmentIndex, headerWidget, false, false);
	SetState(1, headerWidget);
}

/* 0x8135D4E4 */
void ActivityBusyFooter::HideBusyHeader()
{
	if (m_listView == NULL)
		return;

	m_listView->SetSegmentHeader(m_segmentIndex, NULL, false, false);
	SetState(0, NULL);
}

/* 0x8135D50A */
paf::IDParam ActivityBusyFooter::GetTemplateId()
{
	return paf::IDParam(TEMPLATE_BUSY_FOOTER);
}

/* 0x8135D54E */
paf::IDParam ActivityBusyFooter::GetHeaderId()
{
	return paf::IDParam(WIDGET_BUSY_FOOTER_ROOT);
}

/* 0x8135D592 */
void ActivityBusyFooter::StartBusyIndicator()
{
	if (m_listView == NULL || m_state != 1)
		return;

	paf::ui::Widget *headerWidget = m_listView->GetSegmentHeader(m_segmentIndex);
	if (headerWidget == NULL)
		headerWidget = m_listView->FindChild(GetHeaderId(), 0);
	if (headerWidget == NULL)
		return;

	paf::ui::BusyIndicator *indicator = static_cast<paf::ui::BusyIndicator *>(
		headerWidget->FindChild(paf::IDParam(WIDGET_BUSY_FOOTER_INDICATOR), 0));
	headerWidget->Show(0xff, NULL, NULL, 0.0f);
	if (indicator != NULL)
		indicator->Start();

	paf::ui::Widget *textWidget =
		headerWidget->FindChild(paf::IDParam(WIDGET_BUSY_FOOTER_TEXT), 0);
	if (textWidget != NULL) {
		textWidget->Hide(0xff, NULL, NULL, 0.0f);
		textWidget->Show(0xff, new paf::Timer(250.0f), NULL, 0.0f);
	}
}

} /* namespace filelist */
