/*
 * lv1status_activity2_visualizer.cpp: level-1 activity status card visualizer,
 * status detail dialog, vote/comment/delete/grief-report job manager, and
 * dialog/file-list registries (0x8134D97C..0x8135B29B).
 */
#include "filelist_internal.h"
#include "../../popup_list.h"
#include <common/char_encoding.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/rect.h>
#include <widget/w_button_base.h>
#include <widget/w_busyindicator.h>
#include <widget/w_text_box.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_scroll_view_base.h>
#include <std/stdio.h>
#include <std/string.h>

namespace filelist {

namespace {

static const char s_lv1StatusVisualizerTypeName[] = "lv1status_activity2_visualizer";

/* PAF widget, template, texture, and string resource IDs (exact signed 32-bit hashes) */
enum {
	TEMPLATE_STATUS_BUSY_OVERLAY     =   213765718,
	WIDGET_STATUS_BUSY_INDICATOR     =  -515153258,
	WIDGET_STATUS_PANEL_BTN          =  -306099657,
	WIDGET_STATUS_AVATAR_BTN         = -1275049805,
	WIDGET_STATUS_ACTION_BTN         = -1145406561,
	TEMPLATE_STATUS_TITLE_CARD       =  -367414514,
	WIDGET_STATUS_TITLE_CARD_ROOT    =  -312608082,
	TEMPLATE_STATUS_USER_CARD        =  1423951551,
	WIDGET_STATUS_USER_CARD_ROOT     =  -259635832,
	TEX_STATUS_PANEL_BASE            = -1371129670,
	TEX_STATUS_PANEL_TITLE_MASK      = -1604668446,
	TEX_STATUS_PANEL_USER_MASK       =  -797768180,
	TEX_COMMENT_ICON_NEW             = -1805030763,
	TEX_COMMENT_ICON_NORMAL          =  1725222766,
	STR_COMMENT_COUNT_FMT            =  1160273227,

	TEMPLATE_STATUS_DIALOG           = -1790666770,
	WIDGET_STATUS_DIALOG_ROOT        =  1471131967,
	WIDGET_STATUS_DIALOG_LIST        =  1407301643,
	WIDGET_STATUS_DIALOG_BG          =   692387201,
	WIDGET_STATUS_DIALOG_PANEL       =  -406624041,
	WIDGET_STATUS_DIALOG_OPTION_BTN  = -1466069370,
	TEMPLATE_STATUS_DIALOG_HEADER    =  2135074743,
	WIDGET_STATUS_DIALOG_HEADER_ROOT =   526555210,
	WIDGET_STATUS_DIALOG_COMMENT_BOX =  -230985351,
	WIDGET_STATUS_DIALOG_COMMENT_BAR =   548899213,

	WIDGET_GOODJOB_HEADER_BOX        =   619555798,
	TEMPLATE_GOODJOB_VOTE_BTN        =  1423544944,
	WIDGET_GOODJOB_VOTE_BTN          =  -113409995,
	WIDGET_GOODJOB_VOTE_LABEL        =  -196625912,
	TEMPLATE_GOODJOB_AREA            =  2057018850,
	WIDGET_GOODJOB_AREA              = -1245339371,
	WIDGET_GOODJOB_ACTION_BTN        =  1284919397,
	WIDGET_GOODJOB_NAMES_TEXT        = -1982060943,

	TEX_GOODJOB_DO                   =   421036993,
	TEX_GOODJOB_CANCEL               =  1130531396,
	STR_GOODJOB_DO                   = -2046813036,
	STR_GOODJOB_CANCEL               = -1319769250,
	STR_GOODJOB_ONE                  =   998224761,
	STR_GOODJOB_TWO                  = -1821078740,
	STR_GOODJOB_MANY                 =   652317232,
	STR_GOODJOB_NAME_FMT             =  -463369791,

	STR_DELETE_STATUS_CONFIRM        = -1369352515,
	STR_DELETE_COMMENT_CONFIRM       = -1529301176,
	STR_TOO_MANY_JOBS                = -1463023626,
	STR_POPUP_DELETE                 =  -243196622,
	STR_POPUP_REPORT                 =  1528059821
};

/* Lv1 status panel DrawObj slot indices */
enum {
	STATUS_DRAWOBJ_PANEL         = 0,
	STATUS_DRAWOBJ_BODY          = 1,
	STATUS_DRAWOBJ_TIME          = 2,
	STATUS_DRAWOBJ_NEWLINE_MARK  = 3,
	STATUS_DRAWOBJ_UNREAD        = 4,
	STATUS_DRAWOBJ_VOTE_ICON     = 5,
	STATUS_DRAWOBJ_VOTE_COUNT    = 6,
	STATUS_DRAWOBJ_COMMENT_ICON  = 7,
	STATUS_DRAWOBJ_COMMENT_COUNT = 8
};

/* PAF event & timer IDs */
enum {
	EVENT_WIDGET_DESTROY       = 0x00100001,
	EVENT_WIDGET_SIZE_CHANGED  = 0x00100004,
	EVENT_CACHE_IMAGE_READY    = 0x00100005,
	EVENT_BUTTON_DECIDE        = 0x10000008,
	EVENT_TEXTBOX_EDIT_BEGIN   = 0x10000007,
	EVENT_TEXTBOX_EDIT_END     = 0x10000008,
	EVENT_TEXTBOX_ENTER        = 0x1000000B,
	EVENT_TEXTBOX_CHANGED      = 0x1000000C,
	TIMER_ID_DIALOG_CLIP       = 0x0123,
	TIMER_ID_POST_COMMENT_ANIM = 0x0124,
	ANIM_CHANNEL_COLOR         = 0x00010002,
	ANIM_CHANNEL_META_ALPHA    = 0x00010003,
	ANIM_CHANNEL_SIZE          = 0x00010004,
	ANIM_CHANNEL_POS           = 0x00010005
};

enum {
	ACTIVITY_ERR_NOT_FOUND = (int)0x80101005,
	ACTIVITY_MAX_PENDING_JOBS = 10,
	ACTIVITY_MAX_GOODJOB_VOTES = 100,
	ACTIVITY_MAX_GOODJOB_NAMES = 15,
	ACTIVITY_COMMENT_MAX_LENGTH = 140
};

bool ContainsRowid(const paf::vector<SceInt64> &vec, SceInt64 rowid)
{
	for (size_t i = 0; i < vec.size(); ++i) {
		if (vec[i] == rowid)
			return true;
	}
	return false;
}

void RemoveRowid(paf::vector<SceInt64> &vec, SceInt64 rowid)
{
	for (size_t i = 0; i < vec.size(); ++i) {
		if (vec[i] == rowid) {
			vec.erase(vec.begin() + i);
			return;
		}
	}
}

/* -------------------------------------------------------------------------- */
/* Global Registries: FileList registry, Dialog manager, Service manager      */
/* -------------------------------------------------------------------------- */

struct ActivityFileListRegistry {
	paf::vector<paf::ui::FileList *> lists;
};

struct ActivityDialogManager {
	ActivityDialogManager() : nextDialogId(1)
	{
	}

	paf::map<int, ActivityStatusDialog *> dialogs;
	int nextDialogId;
};

struct ActivityServiceManager {
	ActivityServiceManager()
		: syncHandle(),
		  postJobQueue(NULL)
	{
	}

	paf::common::SharedPtr<ActivitySyncHandle> syncHandle;
	paf::job::JobQueue *postJobQueue;
	paf::vector<SceInt64> votingRowids;
	paf::vector<SceInt64> cancelingVoteRowids;
	paf::vector<SceInt64> postingCommentRowids;
	paf::vector<SceInt64> deletingCommentRowids;
	paf::vector<SceInt64> deletingStatusRowids;
};

ActivityFileListRegistry *s_fileListRegistry = NULL;
ActivityDialogManager *s_dialogManager = NULL;
ActivityServiceManager *s_serviceManager = NULL;

size_t CountPendingNetworkJobs()
{
	if (s_serviceManager == NULL)
		return 0;
	return s_serviceManager->votingRowids.size() +
	       s_serviceManager->cancelingVoteRowids.size() +
	       s_serviceManager->postingCommentRowids.size() +
	       s_serviceManager->deletingCommentRowids.size() +
	       s_serviceManager->deletingStatusRowids.size();
}

/* 0x813509E4 */
Lv1StatusActivityVisualizer *FindStatusVisualizerByRowid(SceInt64 statusRowid)
{
	if (s_fileListRegistry == NULL)
		return NULL;

	for (size_t listIdx = 0; listIdx < s_fileListRegistry->lists.size(); ++listIdx) {
		paf::ui::FileList *fileList = s_fileListRegistry->lists[listIdx];
		if (fileList == NULL || fileList->m_hold_count > 0)
			continue;

		int32_t generatedCount = fileList->GetGeneratedNum();
		for (int32_t i = 0; i < generatedCount; ++i) {
			paf::ui::listview::ListPos pos = fileList->GetGeneratedPos(i);
			paf::ui::ListItem *item =
				fileList->GetListItem(pos.m_segment, pos.m_cell);
			paf::intrusive_ptr<paf::ui::filelist::Visualizer> vis =
				paf::ui::filelist::GetVisualizer(item);
			if (vis.get() == NULL ||
			    !vis->IsInherit(Lv1StatusActivityVisualizer::TypeName()))
				continue;

			Lv1StatusActivityVisualizer *statusVis =
				static_cast<Lv1StatusActivityVisualizer *>(vis.get());
			paf::intrusive_ptr<Live2Record> rec = statusVis->m_record;
			if (rec.get() != NULL &&
			    rec->GetRecordKind() == LIVE2_RECORD_STATUS) {
				Live2StatusRecord *statusRec =
					static_cast<Live2StatusRecord *>(rec.get());
				if (statusRec->m_status.rowid == statusRowid)
					return statusVis;
			}
		}
	}
	return NULL;
}

/* 0x8135A96E */
Lv2CommentActivityVisualizer *FindCommentVisualizerByRowid(SceInt64 commentRowid)
{
	if (s_dialogManager == NULL)
		return NULL;

	for (paf::map<int, ActivityStatusDialog *>::iterator it = s_dialogManager->dialogs.begin();
	     it != s_dialogManager->dialogs.end(); ++it) {
		ActivityStatusDialog *dialog = it->second;
		if (dialog == NULL || dialog->m_listView == NULL ||
		    dialog->m_listView->m_hold_count > 0)
			continue;

		int32_t generatedCount = dialog->m_listView->GetGeneratedNum();
		for (int32_t i = 0; i < generatedCount; ++i) {
			paf::ui::listview::ListPos pos = dialog->m_listView->GetGeneratedPos(i);
			paf::ui::ListItem *item =
				dialog->m_listView->GetListItem(pos.m_segment, pos.m_cell);
			paf::intrusive_ptr<paf::ui::filelist::Visualizer> vis =
				paf::ui::filelist::GetVisualizer(item);
			if (vis.get() == NULL)
				continue;

			Lv2CommentActivityVisualizer *commentVis =
				static_cast<Lv2CommentActivityVisualizer *>(vis.get());
			paf::intrusive_ptr<Live2Record> rec = commentVis->m_record;
			if (rec.get() != NULL &&
			    rec->GetRecordKind() == LIVE2_RECORD_COMMENT) {
				Live2CommentRecord *commentRec =
					static_cast<Live2CommentRecord *>(rec.get());
				if (commentRec->m_comment.rowid == commentRowid)
					return commentVis;
			}
		}
	}
	return NULL;
}

/* 0x81350E04 */
void OnFileListDestroyed(int eventId, paf::ui::Handler *handler,
			 paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	(void)userdata;
	if (s_fileListRegistry == NULL)
		return;

	paf::ui::FileList *fileList = static_cast<paf::ui::FileList *>(handler);
	for (size_t i = 0; i < s_fileListRegistry->lists.size(); ++i) {
		if (s_fileListRegistry->lists[i] == fileList) {
			s_fileListRegistry->lists.erase(s_fileListRegistry->lists.begin() + i);
			break;
		}
	}
}

/* 0x81350BFA */
void ShowStatusBusyByRowid(SceInt64 statusRowid)
{
	Lv1StatusActivityVisualizer *vis = FindStatusVisualizerByRowid(statusRowid);
	if (vis != NULL)
		vis->ShowBusy();
}

/* 0x81350C18 */
void HideStatusBusyByRowid(SceInt64 statusRowid)
{
	Lv1StatusActivityVisualizer *vis = FindStatusVisualizerByRowid(statusRowid);
	if (vis != NULL)
		vis->HideBusy();
}

/* 0x81350C5A */
void ShowStatusErrorByRowid(SceInt64 statusRowid, int errorCode)
{
	Lv1StatusActivityVisualizer *vis = FindStatusVisualizerByRowid(statusRowid);
	if (vis != NULL)
		vis->ShowDeleteErrorDialog(errorCode);
}

/* -------------------------------------------------------------------------- */
/* CommonGuiDialog Handlers                                                   */
/* -------------------------------------------------------------------------- */

/* 0x8153D4A8: dismiss handler for Lv1StatusActivityVisualizer error dialog */
class StatusCardErrorDialogHandler : public CommonGuiDialogHandler {
public:
	explicit StatusCardErrorDialogHandler(SceInt64 statusRowid)
		: m_statusRowid(statusRowid)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
		Lv1StatusActivityVisualizer *vis = FindStatusVisualizerByRowid(m_statusRowid);
		if (vis != NULL)
			vis->m_errorDialogId = -1;
	}

	SceInt64 m_statusRowid;
};

/* 0x814449C0 / 0x81444A48: error dialog handlers for ActivityStatusDialog */
class StatusDialogErrorHandler : public CommonGuiDialogHandler {
public:
	StatusDialogErrorHandler(int dialogId, bool closeDialogOnDismiss)
		: m_dialogId(dialogId),
		  m_closeOnDismiss(closeDialogOnDismiss)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
		ActivityStatusDialog *dialog = activityUiFindDialogById(m_dialogId);
		if (dialog != NULL) {
			dialog->m_commonDialogId = -1;
			if (m_closeOnDismiss)
				dialog->Close();
		}
	}

	int m_dialogId;
	bool m_closeOnDismiss;
};

/* 0x81444174: too-many-jobs notice handler */
class TooManyJobsDialogHandler : public CommonGuiDialogHandler {
public:
	explicit TooManyJobsDialogHandler(int dialogId)
		: m_dialogId(dialogId)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
		ActivityStatusDialog *dialog = activityUiFindDialogById(m_dialogId);
		if (dialog != NULL)
			dialog->m_commonDialogId = -1;
	}

	int m_dialogId;
};

/* 0x81444E14: confirmation handler for deleting a status */
class DeleteStatusConfirmHandler : public CommonGuiDialogHandler {
public:
	explicit DeleteStatusConfirmHandler(int dialogId)
		: m_dialogId(dialogId)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		ActivityStatusDialog *dialog = activityUiFindDialogById(m_dialogId);
		if (dialog == NULL)
			return;
		dialog->m_commonDialogId = -1;
		if (button == 3)
			dialog->StartDeleteStatus();
	}

	int m_dialogId;
};

/* 0x81444A90: confirmation handler for deleting a comment */
class DeleteCommentConfirmHandler : public CommonGuiDialogHandler {
public:
	DeleteCommentConfirmHandler(int dialogId,
				    const paf::intrusive_ptr<Live2Record> &commentRecord)
		: m_dialogId(dialogId),
		  m_commentRecord(commentRecord)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		ActivityStatusDialog *dialog = activityUiFindDialogById(m_dialogId);
		if (dialog == NULL)
			return;
		dialog->m_commonDialogId = -1;
		if (button == 3)
			dialog->StartDeleteComment(m_commentRecord);
	}

	int m_dialogId;
	paf::intrusive_ptr<Live2Record> m_commentRecord;
};

/* -------------------------------------------------------------------------- */
/* Background Jobs (GetVoteInfo, VoteGood, CancelGood, PostComment, etc.)     */
/* -------------------------------------------------------------------------- */

/* 0x81355D50 / 0x8135AB08 / 0x8135ABC4 / 0x8135AC9E */
class GetVoteInfoJob : public paf::common::SlotJobItem {
public:
	GetVoteInfoJob(SceInt64 statusRowid, const paf::string &titleId,
		       const paf::string &ownerId, int dialogId, int queryType)
		: paf::common::SlotJobItem("GetVoteInfoJob", NULL),
		  m_statusRowid(statusRowid),
		  m_ownerId(ownerId),
		  m_titleId(titleId),
		  m_dialogId(dialogId),
		  m_queryType(queryType),
		  m_goodJob()
	{
		m_goodJob.mainRowid = 0;
		m_goodJob.isVoted = false;
		m_goodJob.reserved = 0;
		m_goodJob.numOfVotes = 0;
	}

	virtual ~GetVoteInfoJob()
	{
	}

	virtual int32_t Run()
	{
		void *queryHandle = NULL;
		SceActivityDb_6C98B745(&queryHandle);

		ActivityDbQueryCondition condition;
		condition.type = m_queryType;
		int err = SceActivityDb_F65C0C21(&queryHandle, &condition);
		if (err == 0)
			err = SceActivityDb_8D4B7F73(&queryHandle, &m_statusRowid, &m_goodJob);

		SceActivityDb_6E198FFA(&queryHandle);
		return err;
	}

	virtual void Finish(int32_t err)
	{
		paf::common::SlotJobItem::Finish(err);
		if (IsCanceled())
			return;

		ActivityStatusDialog *dialog = activityUiFindDialogById(m_dialogId);
		if (dialog != NULL && err == 0)
			dialog->OnVoteInfoReady(err, m_goodJob);
	}

	SceInt64 m_statusRowid;
	paf::string m_ownerId;
	paf::string m_titleId;
	int m_dialogId;
	int m_queryType;
	ActivityDbGoodJob m_goodJob;
};

/* 0x814441E8 / 0x81444208 */
class VoteGoodJob : public paf::job::JobItem {
public:
	VoteGoodJob(const paf::common::SharedPtr<ActivitySyncHandle> &syncHandle,
		    SceInt64 statusRowid)
		: paf::job::JobItem("VoteGoodJob", NULL),
		  m_syncHandle(syncHandle),
		  m_statusRowid(statusRowid)
	{
	}

	virtual ~VoteGoodJob()
	{
	}

	virtual int32_t Run()
	{
		activityUiInvokeNetAcquire();
		return SceActivityDb_2929CAD1(m_syncHandle.get(), &m_statusRowid);
	}

	virtual void Finish(int32_t err)
	{
		activityUiInvokeNetRelease();
		if (s_serviceManager != NULL)
			RemoveRowid(s_serviceManager->votingRowids, m_statusRowid);

		ActivityStatusDialog *dialog = activityUiFindDialogByStatusRowid(&m_statusRowid);
		if (dialog != NULL)
			dialog->OnVoteFinished(err, true);
	}

	paf::common::SharedPtr<ActivitySyncHandle> m_syncHandle;
	SceInt64 m_statusRowid;
};

/* 0x81444358 / 0x81444378 */
class CancelGoodJob : public paf::job::JobItem {
public:
	CancelGoodJob(const paf::common::SharedPtr<ActivitySyncHandle> &syncHandle,
		      SceInt64 statusRowid)
		: paf::job::JobItem("CancelGoodJob", NULL),
		  m_syncHandle(syncHandle),
		  m_statusRowid(statusRowid)
	{
	}

	virtual ~CancelGoodJob()
	{
	}

	virtual int32_t Run()
	{
		activityUiInvokeNetAcquire();
		return SceActivityDb_DF874DD2(m_syncHandle.get(), &m_statusRowid);
	}

	virtual void Finish(int32_t err)
	{
		activityUiInvokeNetRelease();
		if (s_serviceManager != NULL)
			RemoveRowid(s_serviceManager->cancelingVoteRowids, m_statusRowid);

		ActivityStatusDialog *dialog = activityUiFindDialogByStatusRowid(&m_statusRowid);
		if (dialog != NULL)
			dialog->OnVoteFinished(err, false);
	}

	paf::common::SharedPtr<ActivitySyncHandle> m_syncHandle;
	SceInt64 m_statusRowid;
};

/* 0x814444CC / 0x81444580 */
class PostCommentJob : public paf::job::JobItem {
public:
	PostCommentJob(const paf::common::SharedPtr<ActivitySyncHandle> &syncHandle,
		       SceInt64 statusRowid, const paf::string &message)
		: paf::job::JobItem("PostCommentJob", NULL),
		  m_syncHandle(syncHandle),
		  m_statusRowid(statusRowid),
		  m_message(message)
	{
	}

	virtual ~PostCommentJob()
	{
	}

	virtual int32_t Run()
	{
		activityUiInvokeNetAcquire();
		ActivityDbComment comment;
		int err = SceActivityDb_C715B8BC(&comment, &m_message);
		if (err == 0)
			err = SceActivityDb_3F501885(m_syncHandle.get(), &m_statusRowid, &comment);
		return err;
	}

	virtual void Finish(int32_t err)
	{
		activityUiInvokeNetRelease();
		if (s_serviceManager != NULL)
			RemoveRowid(s_serviceManager->postingCommentRowids, m_statusRowid);

		ActivityStatusDialog *dialog = activityUiFindDialogByStatusRowid(&m_statusRowid);
		if (dialog != NULL)
			dialog->OnPostCommentFinished(err);
	}

	paf::common::SharedPtr<ActivitySyncHandle> m_syncHandle;
	SceInt64 m_statusRowid;
	paf::string m_message;
};

/* 0x814446D4 / 0x814446F4 */
class DelStatusJob : public paf::job::JobItem {
public:
	DelStatusJob(const paf::common::SharedPtr<ActivitySyncHandle> &syncHandle,
		     SceInt64 statusRowid)
		: paf::job::JobItem("DelStatusJob", NULL),
		  m_syncHandle(syncHandle),
		  m_statusRowid(statusRowid)
	{
	}

	virtual ~DelStatusJob()
	{
	}

	virtual int32_t Run()
	{
		activityUiInvokeNetAcquire();
		return SceActivityDb_5FC8EE9A(m_syncHandle.get(), &m_statusRowid);
	}

	virtual void Finish(int32_t err)
	{
		activityUiInvokeNetRelease();
		if (s_serviceManager != NULL)
			RemoveRowid(s_serviceManager->deletingStatusRowids, m_statusRowid);

		if (err != 0) {
			HideStatusBusyByRowid(m_statusRowid);
			ShowStatusErrorByRowid(m_statusRowid, err);
		}
	}

	paf::common::SharedPtr<ActivitySyncHandle> m_syncHandle;
	SceInt64 m_statusRowid;
};

/* 0x81444848 / 0x81444868 */
class DeleteCommentJob : public paf::job::JobItem {
public:
	DeleteCommentJob(const paf::common::SharedPtr<ActivitySyncHandle> &syncHandle,
			 SceInt64 statusRowid, SceInt64 commentRowid)
		: paf::job::JobItem("DeleteCommentJob", NULL),
		  m_syncHandle(syncHandle),
		  m_statusRowid(statusRowid),
		  m_commentRowid(commentRowid)
	{
	}

	virtual ~DeleteCommentJob()
	{
	}

	virtual int32_t Run()
	{
		activityUiInvokeNetAcquire();
		return SceActivityDb_2A7ABB45(m_syncHandle.get(), &m_statusRowid, &m_commentRowid);
	}

	virtual void Finish(int32_t err)
	{
		activityUiInvokeNetRelease();
		if (s_serviceManager != NULL)
			RemoveRowid(s_serviceManager->deletingCommentRowids, m_commentRowid);

		Lv2CommentActivityVisualizer *commentVis =
			FindCommentVisualizerByRowid(m_commentRowid);
		if (commentVis != NULL)
			commentVis->OnDeleteFinished(err);

		ActivityStatusDialog *dialog = activityUiFindDialogByStatusRowid(&m_statusRowid);
		if (dialog != NULL)
			dialog->OnDeleteCommentFinished(err, m_commentRowid);
	}

	paf::common::SharedPtr<ActivitySyncHandle> m_syncHandle;
	SceInt64 m_statusRowid;
	SceInt64 m_commentRowid;
};

/* 0x81357B2E / 0x8135ACF4 / 0x8135AF8C */
class GriefReportJob : public paf::common::SlotJobItem {
public:
	GriefReportJob(const paf::intrusive_ptr<Live2Record> &statusRecord,
		       const paf::intrusive_ptr<Live2Record> &commentRecord,
		       const paf::common::SharedPtr<Live2AccessorProxy> &accessorProxy)
		: paf::common::SlotJobItem("GriefReportJob", NULL),
		  m_statusRecord(statusRecord),
		  m_commentRecord(commentRecord),
		  m_accessorProxy(accessorProxy)
	{
	}

	virtual ~GriefReportJob()
	{
	}

	virtual int32_t Run()
	{
		if (m_statusRecord.get() == NULL ||
		    m_statusRecord->GetRecordKind() != LIVE2_RECORD_STATUS)
			return -1;
		if (m_commentRecord.get() == NULL ||
		    m_commentRecord->GetRecordKind() != LIVE2_RECORD_COMMENT)
			return -1;

		Live2StatusRecord *statusRec =
			static_cast<Live2StatusRecord *>(m_statusRecord.get());
		Live2CommentRecord *commentRec =
			static_cast<Live2CommentRecord *>(m_commentRecord.get());

		paf::vector<SceInt64> statusRowids;
		statusRowids.push_back(statusRec->m_status.rowid);
		paf::vector<ActivityDbStatus> statuses;
		int err = SceActivityDb_74F96707(m_accessorProxy->m_queryHandle,
						 &statusRowids, &statuses);
		if (err != 0)
			return err;

		paf::vector<SceInt64> commentRowids;
		err = SceActivityDb_572A6463(m_accessorProxy->m_queryHandle,
					     &statusRec->m_status.rowid, 0, 20, &commentRowids);
		if (err != 0)
			return err;

		paf::vector<ActivityDbComment> comments;
		err = SceActivityDb_D512F568(m_accessorProxy->m_queryHandle,
					     &commentRowids, &comments);
		if (err != 0)
			return err;

		err = activityGriefReportBegin(&statusRec->m_status, &commentRec->m_comment);
		if (err != 0)
			return err;

		for (size_t i = 0; i < comments.size(); ++i) {
			err = activityGriefReportAppendComment(&comments[i]);
			if (err != 0) {
				activityGriefReportEnd(false);
				return err;
			}
		}

		return activityGriefReportEnd(true);
	}

	virtual void Finish(int32_t err)
	{
		paf::common::SlotJobItem::Finish(err);
		if (m_commentRecord.get() != NULL &&
		    m_commentRecord->GetRecordKind() == LIVE2_RECORD_COMMENT) {
			Live2CommentRecord *commentRec =
				static_cast<Live2CommentRecord *>(m_commentRecord.get());
			Lv2CommentActivityVisualizer *commentVis =
				FindCommentVisualizerByRowid(commentRec->m_comment.rowid);
			if (commentVis != NULL)
				commentVis->OnGriefReportFinished();
		}

		if (err != 0 && m_statusRecord.get() != NULL &&
		    m_statusRecord->GetRecordKind() == LIVE2_RECORD_STATUS) {
			Live2StatusRecord *statusRec =
				static_cast<Live2StatusRecord *>(m_statusRecord.get());
			ActivityStatusDialog *dialog =
				activityUiFindDialogByStatusRowid(&statusRec->m_status.rowid);
			if (dialog != NULL)
				dialog->ShowErrorDialog(err, false);
		}
	}

	paf::intrusive_ptr<Live2Record> m_statusRecord;
	paf::intrusive_ptr<Live2Record> m_commentRecord;
	paf::common::SharedPtr<Live2AccessorProxy> m_accessorProxy;
};

/* -------------------------------------------------------------------------- */
/* ActivityStatusDialog static callbacks                                      */
/* -------------------------------------------------------------------------- */

/* 0x81358DC2 */
void OnDetailSyncCallback(int errorCode, int dialogId)
{
	activityUiInvokeNetRelease();
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnDetailSyncFinished(errorCode);
}

/* 0x81358E0C */
void OnCommentCountCallback(int count, int dialogId)
{
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnCommentCountChanged(count);
}

/* 0x81358E42 */
void OnCommentBoxEnterCallback(int eventId, paf::ui::Handler *handler,
			       paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnSendComment();
}

/* 0x81358E60 */
void OnCommentBoxBeginEditCallback(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnCommentBoxBeginEdit(static_cast<paf::ui::Widget *>(handler));
}

/* 0x81358E82 */
void OnCommentBoxEndEditCallback(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnCommentBoxEndEdit(static_cast<paf::ui::Widget *>(handler));
}

/* 0x81359BF8 */
void OnCommentBoxTextChangedCallback(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->RefreshCommentBoxState(false);
}

/* 0x813589C4 thunk */
void OnVoteGoodJobCallback(int eventId, paf::ui::Handler *handler,
			   paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnVoteGoodJob();
}

/* 0x81358BE0 thunk */
void OnCancelGoodJobCallback(int eventId, paf::ui::Handler *handler,
			     paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnCancelGoodJob();
}

/* 0x8135886E */
void OnHeaderOptionDecideCallback(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OpenStatusOptionMenu(static_cast<paf::ui::Widget *>(handler));
}

/* 0x8135A92A */
void OnDialogAvatarDecideCallback(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	ActivityStatusDialog *dialog = static_cast<ActivityStatusDialog *>(userdata);
	if (dialog != NULL)
		dialog->OnAvatarDecide();
}

/* 0x8135A93E */
void OnDialogActionDecideCallback(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	ActivityStatusDialog *dialog = static_cast<ActivityStatusDialog *>(userdata);
	if (dialog != NULL)
		dialog->OnStatusActionDecide();
}

/* 0x81354396 */
void OnGoodJobButtonSizeChanged(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	(void)userdata;
	paf::ui::Widget *btn = static_cast<paf::ui::Widget *>(handler);
	if (btn == NULL)
		return;
	btn->Show(0xff, NULL, NULL, 0.0f);
	btn->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED, NULL, NULL);
}

/* 0x81359F7E */
void OnStatusOptionMenuSelect(int index, int value, void *userdata)
{
	(void)index;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL && value == 0)
		dialog->ShowDeleteStatusConfirmDialog();
}

/* 0x81359FA0 */
void OnDeleteCommentMenuSelect(int index, int value, void *userdata)
{
	(void)index;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL && value == 0 && dialog->m_selectedCommentVis != NULL)
		dialog->ShowDeleteCommentConfirmDialog(dialog->m_selectedCommentVis->m_record);
}

/* 0x81359FBC */
void OnDeleteOrReportCommentMenuSelect(int index, int value, void *userdata)
{
	(void)index;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog == NULL || dialog->m_selectedCommentVis == NULL)
		return;

	if (value == 0)
		dialog->ShowDeleteCommentConfirmDialog(dialog->m_selectedCommentVis->m_record);
	else if (value == 1)
		dialog->StartGriefReport(dialog->m_selectedCommentVis->m_record);
}

/* 0x81359FE6 */
void OnPopupMenuClosed(int result, void *userdata)
{
	(void)result;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL) {
		dialog->m_popupMenuOpen = false;
		dialog->m_selectedCommentVis = NULL;
	}
}

/* 0x8135A716 */
void OnDialogClipTimer(int eventId, paf::ui::Handler *handler,
		       paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	(void)userdata;
	paf::ui::Widget *dialogRoot = static_cast<paf::ui::Widget *>(handler);
	paf::ui::Widget *listWidget =
		dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_LIST), 0);
	if (listWidget == NULL)
		return;

	paf::math::rect clipRect = listWidget->CalcClipRect();
	if (listWidget->m_layout_rect == NULL)
		listWidget->m_layout_rect = new paf::math::rect();
	if (listWidget->m_layout_rect != NULL)
		*listWidget->m_layout_rect = clipRect;
}

/* 0x8135A828 */
void OnDialogOpenTransitionDone(paf::Timer *timer, paf::ui::AnimListener *listener,
				void *widgetArg, void *userdataArg, void *extraArg)
{
	(void)timer;
	(void)listener;
	(void)extraArg;
	paf::ui::Widget *dialogRoot = static_cast<paf::ui::Widget *>(widgetArg);
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdataArg));

	paf::ui::Widget *listWidget =
		dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_LIST), 0);
	dialogRoot->KillIntervalEvent(TIMER_ID_DIALOG_CLIP);
	ScePafWidget_A96A0B4A(dialogRoot, -1);

	if (listWidget != NULL) {
		listWidget->m_clip_mode = 0;
		if (listWidget->m_layout_rect != NULL) {
			delete listWidget->m_layout_rect;
			listWidget->m_layout_rect = NULL;
		}
	}

	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->SetInputEnabled(true);
}

/* 0x8135A908 */
void OnDialogCloseTransitionDone(paf::Timer *timer, paf::ui::AnimListener *listener,
				 void *widgetArg, void *userdataArg, void *extraArg)
{
	(void)timer;
	(void)listener;
	(void)widgetArg;
	(void)extraArg;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdataArg));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		delete dialog;
}

/* 0x8135A952 */
void OnPostCommentAnimTimer(int eventId, paf::ui::Handler *handler,
			    paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->UpdateDialogLayout();
}

/* 0x8135B25A */
void OnDialogRootDeleteCallback(paf::ui::Widget *widget, void *userdata)
{
	(void)widget;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnBackButtonDecide();
}

/* 0x8135B27A */
void OnShowFocusAfterAnim(paf::Timer *timer, paf::ui::AnimListener *listener,
			  void *widgetArg, void *userdataArg, void *extraArg)
{
	(void)timer;
	(void)listener;
	(void)extraArg;
	paf::ui::Widget *widget = static_cast<paf::ui::Widget *>(widgetArg);
	if (userdataArg != NULL && widget != NULL) {
		widget->SetFocus(0);
		paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
		env->ShowFocus(-1);
	}
}

} /* namespace */

/* -------------------------------------------------------------------------- */
/* Lv1StatusActivityVisualizer                                                */
/* -------------------------------------------------------------------------- */

/* 0x8134D97C */
bool Lv1StatusActivityVisualizer::IsInherit(const char *type) const
{
	if (type == s_lv1StatusVisualizerTypeName)
		return true;
	return BaseActivityVisualizer::IsInherit(type);
}

/* 0x8134D994 */
const char *Lv1StatusActivityVisualizer::GetTypeName() const
{
	return s_lv1StatusVisualizerTypeName;
}

const char *Lv1StatusActivityVisualizer::TypeName()
{
	return s_lv1StatusVisualizerTypeName;
}

/* 0x8134D9A0 */
Lv1StatusActivityVisualizer::Lv1StatusActivityVisualizer(
	paf::ui::Widget *parent, paf::Plugin *plugin,
	paf::cache::CacheManager *cacheManager,
	const paf::common::SharedPtr<Live2Iterator> &iterator,
	SceInt64 cellId)
	: BaseActivityVisualizer(parent, plugin, cacheManager, iterator, cellId),
	  m_errorDialogId(-1)
{
	RequestRecordUpdate();
}

/* 0x8134DA98 */
Lv1StatusActivityVisualizer::~Lv1StatusActivityVisualizer()
{
	CloseErrorDialog();
}

/* 0x8134DB5A */
void Lv1StatusActivityVisualizer::StopBusyIndicator(paf::ui::Widget *panel)
{
	if (panel == NULL)
		return;

	paf::ui::BusyIndicator *indicator = static_cast<paf::ui::BusyIndicator *>(
		panel->FindChild(paf::IDParam(WIDGET_STATUS_BUSY_INDICATOR), 0));
	if (indicator != NULL) {
		indicator->Stop();
		ScePafWidget_5A9793B0(indicator, 0.0f);
		indicator->m_name = paf::IDParam("");
	}
}

/* 0x8134DC58 */
void Lv1StatusActivityVisualizer::HideBusy()
{
	if (m_listItem == NULL)
		return;

	paf::ui::ButtonBase *panelBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_PANEL_BTN), 0));
	paf::ui::ButtonBase *avatarBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_AVATAR_BTN), 0));
	paf::ui::ButtonBase *actionBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_ACTION_BTN), 0));

	if (panelBtn != NULL) {
		panelBtn->Enable(false);
		StopBusyIndicator(panelBtn);
		PafVec4 fullColor(1.0f, 1.0f, 1.0f, 1.0f);
		panelBtn->SetColor(fullColor, NULL, ANIM_CHANNEL_COLOR, 0, 0.0f, NULL, NULL);
	}
	if (avatarBtn != NULL)
		avatarBtn->Enable(false);
	if (actionBtn != NULL)
		actionBtn->Enable(false);
	ScePafWidget_A96A0B4A(m_listItem, -1);
}

/* 0x8134DE6A */
void Lv1StatusActivityVisualizer::ShowBusy()
{
	if (m_listItem == NULL)
		return;

	paf::ui::ButtonBase *panelBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_PANEL_BTN), 0));
	paf::ui::ButtonBase *avatarBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_AVATAR_BTN), 0));
	paf::ui::ButtonBase *actionBtn = static_cast<paf::ui::ButtonBase *>(
		m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_ACTION_BTN), 0));

	if (panelBtn != NULL) {
		panelBtn->Disable(false);
		if (panelBtn->FindChild(paf::IDParam(WIDGET_STATUS_BUSY_INDICATOR), 0) == NULL) {
			paf::Plugin::TemplateOpenParam openParam;
			m_plugin->TemplateOpen(panelBtn,
					       paf::IDParam(TEMPLATE_STATUS_BUSY_OVERLAY),
					       openParam);
			paf::ui::BusyIndicator *indicator = static_cast<paf::ui::BusyIndicator *>(
				panelBtn->FindChild(paf::IDParam(WIDGET_STATUS_BUSY_INDICATOR), 0));
			if (indicator != NULL)
				indicator->Start();
		}
		PafVec4 dimColor(0.5f, 0.5f, 0.5f, 1.0f);
		panelBtn->SetColor(dimColor, NULL, ANIM_CHANNEL_COLOR, 0, 0.0f, NULL, NULL);
	}
	if (avatarBtn != NULL)
		avatarBtn->Disable(false);
	if (actionBtn != NULL)
		actionBtn->Disable(false);
	ScePafWidget_A96A0B4A(m_listItem, 1);
}

/* 0x8134E09C */
void Lv1StatusActivityVisualizer::LayoutCountsCallback(int eventId, paf::ui::Handler *handler,
						       paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	(void)userdata;
	paf::ui::Widget *panel = static_cast<paf::ui::Widget *>(handler);
	if (panel == NULL)
		return;

	paf::graph::DrawObj *commentIcon = panel->GetDrawObj(STATUS_DRAWOBJ_COMMENT_ICON);
	paf::graph::DrawObj *commentText = panel->GetDrawObj(STATUS_DRAWOBJ_COMMENT_COUNT);
	paf::graph::DrawObj *voteIcon = panel->GetDrawObj(STATUS_DRAWOBJ_VOTE_ICON);
	paf::graph::DrawObj *voteText = panel->GetDrawObj(STATUS_DRAWOBJ_VOTE_COUNT);

	PafVec4 panelSize = *panel->GetSize(0);
	float rightEdge = panelSize.extract_x() * 0.5f - 24.0f;

	if (commentText != NULL && commentIcon != NULL && commentText->m_flags.m_visible) {
		float textX = rightEdge - commentText->m_size.extract_x() * 0.5f;
		commentText->SetPos(PafVec3(textX,
					    commentText->m_pos.extract_y(),
					    commentText->m_pos.extract_z()));

		float iconX = textX - commentText->m_size.extract_x() * 0.5f -
			      commentIcon->m_size.extract_x() * 0.5f - 6.0f;
		commentIcon->SetPos(PafVec3(iconX,
					    commentIcon->m_pos.extract_y(),
					    commentIcon->m_pos.extract_z()));
		rightEdge = iconX - commentIcon->m_size.extract_x() * 0.5f - 16.0f;
	}

	if (voteText != NULL && voteIcon != NULL && voteText->m_flags.m_visible) {
		float textX = rightEdge - voteText->m_size.extract_x() * 0.5f;
		voteText->SetPos(PafVec3(textX,
					 voteText->m_pos.extract_y(),
					 voteText->m_pos.extract_z()));

		float iconX = textX - voteText->m_size.extract_x() * 0.5f -
			      voteIcon->m_size.extract_x() * 0.5f - 6.0f;
		voteIcon->SetPos(PafVec3(iconX,
					 voteIcon->m_pos.extract_y(),
					 voteIcon->m_pos.extract_z()));
	}

	panel->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED, NULL, NULL);
}

/* 0x8134E768 */
void Lv1StatusActivityVisualizer::UpdateVoteAndCommentCounts(paf::ui::Widget *panel)
{
	if (panel == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(rec.get());
	int commentCount = statusRec->m_status.numOfComments;
	int voteCount = GetVoteCount(rec);

	paf::graph::DrawObj *commentIcon = panel->GetDrawObj(STATUS_DRAWOBJ_COMMENT_ICON);
	paf::graph::TextObj *commentText = GetCommentCountTextObj(panel);
	paf::graph::DrawObj *voteIcon = panel->GetDrawObj(STATUS_DRAWOBJ_VOTE_ICON);
	paf::graph::TextObj *voteText = GetVoteCountTextObj(panel);

	if (commentText != NULL && commentIcon != NULL) {
		if (commentCount > 0) {
			wchar_t numBuf[32];
			sce_paf_swprintf(numBuf, 32, L"%d", commentCount);
			const wchar_t *fmt =
				m_plugin->GetString(paf::IDParam(STR_COMMENT_COUNT_FMT));
			paf::wstring formatted = shellFormatText(fmt, numBuf);
			commentText->SetString(formatted);
			commentIcon->SetVisible(true);
			commentText->SetVisible(true);
		} else {
			commentIcon->SetVisible(false);
			commentText->SetVisible(false);
		}
	}

	if (voteText != NULL && voteIcon != NULL) {
		if (voteCount > 0) {
			wchar_t numBuf[32];
			sce_paf_swprintf(numBuf, 32, L"%d", voteCount);
			voteText->SetString(paf::wstring(numBuf));
			voteIcon->SetVisible(true);
			voteText->SetVisible(true);
		} else {
			voteIcon->SetVisible(false);
			voteText->SetVisible(false);
		}
	}

	if (panel->m_prepare_count != 0) {
		panel->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED,
					&Lv1StatusActivityVisualizer::LayoutCountsCallback, NULL);
	} else {
		LayoutCountsCallback(0, panel, NULL, NULL);
	}
}

/* 0x8134ECA6 */
void Lv1StatusActivityVisualizer::UpdateTimeText(paf::graph::TextObj *timeText)
{
	if (timeText == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(rec.get());
	paf::wstring timeStr =
		FormatActivityTime(static_cast<SceUInt64>(statusRec->m_status.creationTime));

	if (ShouldShowTitleName() && !statusRec->m_status.titleName.empty()) {
		paf::wstring titleWide;
		paf::common::Utf8ToUtf16(statusRec->m_status.titleName.c_str(), &titleWide);
		paf::wstring combined = titleWide + L"  " + timeStr;
		timeText->SetString(combined);
	} else {
		timeText->SetString(timeStr);
	}
}

/* 0x8134EE3C */
void Lv1StatusActivityVisualizer::UpdateStatusText(paf::ui::Widget *listItem,
						   paf::ui::Widget *panel)
{
	if (listItem == NULL || panel == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(rec.get());
	PafVec4 itemSize = *listItem->GetSize(0);

	paf::graph::TextObj *bodyText = GetBodyTextObj(panel);
	if (bodyText != NULL) {
		bodyText->SetSize(PafVec3(itemSize.extract_x() - 192.0f,
					  bodyText->m_size.extract_y(),
					  bodyText->m_size.extract_z()));

		paf::string ownerName(statusRec->m_status.ownerId);
		TruncateAtChar(ownerName, '@');

		paf::string message(statusRec->m_status.message);
		bool hasNewline = (message.find('\n') != paf::string::npos);
		paf::graph::DrawObj *newlineMark = GetNewlineMarkObj(panel);
		if (newlineMark != NULL)
			newlineMark->SetVisible(hasNewline);

		if (hasNewline) {
			paf::string stripped;
			char chBuf[2];
			chBuf[1] = '\0';
			for (size_t i = 0; i < message.size(); ++i) {
				if (message[i] != '\n') {
					chBuf[0] = message[i];
					stripped += chBuf;
				}
			}
			message = stripped;
		}

		paf::string combined = ownerName + " " + message;
		if (!combined.empty()) {
			paf::wstring ownerWide;
			paf::common::Utf8ToUtf16(ownerName.c_str(), &ownerWide);
			paf::wstring combinedWide;
			paf::common::Utf8ToUtf16(combined.c_str(), &combinedWide);

			bodyText->SetString(combinedWide);
			bodyText->SetStyleAttribute(paf::graph::TextStyleAttribute_Bold,
						    0, 0, false);
			bodyText->SetStyleAttribute(paf::graph::TextStyleAttribute_Bold,
						    0, ownerWide.size(), true);
		}
	}

	paf::graph::TextObj *timeText = GetTimeTextObj(panel);
	if (timeText != NULL) {
		timeText->SetSize(PafVec3(itemSize.extract_x() - 40.0f,
					  timeText->m_size.extract_y(),
					  timeText->m_size.extract_z()));
		UpdateTimeText(timeText);
	}
}

/* 0x8134F2C4 */
void Lv1StatusActivityVisualizer::UpdatePanel(paf::ui::Widget *listItem,
					      paf::ui::Widget *panel)
{
	if (listItem == NULL || panel == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(rec.get());
	paf::string panelUrl(statusRec->m_status.announcePanelUrl);
	SceUInt32 bgColor = statusRec->m_status.announcePanelBgColor;
	int ownerType = statusRec->m_status.ownerType;

	paf::graph::DrawObj *existingObj = panel->GetDrawObj(STATUS_DRAWOBJ_PANEL);
	if (existingObj == NULL ||
	    existingObj->GetDrawObjType() != ActivityDrawObj::DrawObjType()) {
		paf::common::SharedPtr<paf::graph::DrawObj> newObj(new ActivityDrawObj());
		panel->SwapDrawObj(newObj, STATUS_DRAWOBJ_PANEL);
	}

	ActivityDrawObj *panelObj =
		static_cast<ActivityDrawObj *>(panel->GetDrawObj(STATUS_DRAWOBJ_PANEL));
	if (panelObj != NULL) {
		panelObj->SetMode(0);
		SceUInt32 swappedColor = 0;
		SwapColorEndian(bgColor, &swappedColor);
		panelObj->m_color = swappedColor;

		paf::intrusive_ptr<paf::graph::Surface> baseTex =
			paf::Framework::Instance()->GetCommonTexture(
				paf::IDParam(TEX_STATUS_PANEL_BASE));
		panel->SetTexture(baseTex, STATUS_DRAWOBJ_PANEL, 0);

		if (!panelUrl.empty()) {
			int maskId = (ownerType == ACTIVITY_DB_OWNER_TITLE)
					     ? TEX_STATUS_PANEL_TITLE_MASK
					     : TEX_STATUS_PANEL_USER_MASK;
			paf::intrusive_ptr<paf::graph::Surface> maskTex =
				paf::Framework::Instance()->GetCommonTexture(
					paf::IDParam(maskId));
			panel->SetTexture(maskTex, STATUS_DRAWOBJ_PANEL, 1);

			paf::intrusive_ptr<paf::cache::CacheImageInterface> imageCache;
			CreateActivityImageCache(panelUrl, imageCache, m_cacheManager);
			panel->AddEventCallback(
				EVENT_CACHE_IMAGE_READY,
				&Lv1StatusActivityVisualizer::OnPanelImageLoaded,
				reinterpret_cast<void *>(1));
			panel->SetDrawObjMaskBit(&imageCache, STATUS_DRAWOBJ_PANEL, 2);
		}
	}

	PafVec4 itemSize = *listItem->GetSize(0);
	PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
	paf::ui::Widget *avatarBtn =
		listItem->FindChild(paf::IDParam(WIDGET_STATUS_AVATAR_BTN), 0);
	if (avatarBtn != NULL) {
		PafVec4 curPos = avatarBtn->GetPos(0, NULL, zeroOffset);
		PafVec4 avatarPos(-(itemSize.extract_x() * 0.5f) + 20.0f,
				  curPos.extract_y(), curPos.extract_z(), curPos.extract_w());
		avatarBtn->SetPos(avatarPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
		ApplyAvatarIcon(avatarBtn);
	}

	paf::ui::Widget *actionBtn =
		listItem->FindChild(paf::IDParam(WIDGET_STATUS_ACTION_BTN), 0);
	if (actionBtn != NULL) {
		PafVec4 curPos = actionBtn->GetPos(0, NULL, zeroOffset);
		PafVec4 actionPos((itemSize.extract_x() * 0.5f) - 10.0f,
				  curPos.extract_y(), curPos.extract_z(), curPos.extract_w());
		actionBtn->SetPos(actionPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
		if (!CustomizeActionIcon(actionBtn))
			ApplyActionIcon(actionBtn);
	}
}

/* 0x81350306 */
void Lv1StatusActivityVisualizer::OnRecordUpdated()
{
	UpdateCard();
}

/* 0x8134FAAC */
void Lv1StatusActivityVisualizer::UpdateCard()
{
	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(rec.get());
	SetDisplayMode(statusRec->m_status.ownerType);
	UpdateCategory();

	PafVec4 preferredSize = GetPreferredSize();
	m_listItem->SetSize(preferredSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);

	int displayMode = GetDisplayMode();
	int templateId = (displayMode == ACTIVITY_DB_OWNER_TITLE)
				 ? TEMPLATE_STATUS_TITLE_CARD
				 : TEMPLATE_STATUS_USER_CARD;
	int rootWidgetId = (displayMode == ACTIVITY_DB_OWNER_TITLE)
				   ? WIDGET_STATUS_TITLE_CARD_ROOT
				   : WIDGET_STATUS_USER_CARD_ROOT;
	int otherRootId = (displayMode == ACTIVITY_DB_OWNER_TITLE)
				  ? WIDGET_STATUS_USER_CARD_ROOT
				  : WIDGET_STATUS_TITLE_CARD_ROOT;

	paf::ui::Widget *staleCard = m_listItem->FindChild(paf::IDParam(otherRootId), 0);
	if (staleCard != NULL)
		staleCard->DestroyWidget();

	paf::ui::Widget *cardWidget = m_listItem->FindChild(paf::IDParam(rootWidgetId), 0);
	if (cardWidget == NULL) {
		OpenTemplate(m_listItem, paf::IDParam(templateId));
		cardWidget = m_listItem->FindChild(paf::IDParam(rootWidgetId), 0);
		if (cardWidget == NULL)
			return;
	}

	paf::ui::Widget *avatarBtn =
		cardWidget->FindChild(paf::IDParam(WIDGET_STATUS_AVATAR_BTN), 0);
	if (avatarBtn != NULL) {
		avatarBtn->AddEventCallback(
			EVENT_BUTTON_DECIDE,
			&Lv1StatusActivityVisualizer::AvatarDecideThunk, this);
	}

	paf::ui::Widget *panelBtn =
		cardWidget->FindChild(paf::IDParam(WIDGET_STATUS_PANEL_BTN), 0);
	if (panelBtn != NULL) {
		panelBtn->AddEventCallback(
			EVENT_BUTTON_DECIDE,
			&Lv1StatusActivityVisualizer::PanelDecideThunk, this);
	}

	paf::ui::Widget *actionBtn =
		cardWidget->FindChild(paf::IDParam(WIDGET_STATUS_ACTION_BTN), 0);
	if (actionBtn != NULL) {
		actionBtn->AddEventCallback(
			EVENT_BUTTON_DECIDE,
			&Lv1StatusActivityVisualizer::ActionIconDecideThunk, this);
	}

	if (panelBtn != NULL) {
		UpdatePanel(cardWidget, panelBtn);
		UpdateStatusText(cardWidget, panelBtn);
		UpdateVoteAndCommentCounts(panelBtn);
		UpdateBadges();

		if (activityUiIsStatusDeleting(&statusRec->m_status.rowid))
			ShowBusy();
		else
			HideBusy();
	}
}

void Lv1StatusActivityVisualizer::Stop()
{
	Live2Visualizer::Stop();
	CloseErrorDialog();
}

/* 0x8134F9F2 */
paf::graph::TextObj *Lv1StatusActivityVisualizer::GetBodyTextObj(paf::ui::Widget *panel)
{
	paf::graph::DrawObj *obj = panel->GetDrawObj(STATUS_DRAWOBJ_BODY);
	if (obj != NULL && obj->IsInherit(paf::graph::TextObj::DrawObjType()))
		return static_cast<paf::graph::TextObj *>(obj);
	return NULL;
}

paf::graph::TextObj *Lv1StatusActivityVisualizer::GetTimeTextObj(paf::ui::Widget *panel)
{
	paf::graph::DrawObj *obj = panel->GetDrawObj(STATUS_DRAWOBJ_TIME);
	if (obj != NULL && obj->IsInherit(paf::graph::TextObj::DrawObjType()))
		return static_cast<paf::graph::TextObj *>(obj);
	return NULL;
}

paf::graph::DrawObj *Lv1StatusActivityVisualizer::GetNewlineMarkObj(paf::ui::Widget *panel)
{
	return panel->GetDrawObj(STATUS_DRAWOBJ_NEWLINE_MARK);
}

paf::graph::TextObj *Lv1StatusActivityVisualizer::GetVoteCountTextObj(paf::ui::Widget *panel)
{
	paf::graph::DrawObj *obj = panel->GetDrawObj(STATUS_DRAWOBJ_VOTE_COUNT);
	if (obj != NULL && obj->IsInherit(paf::graph::TextObj::DrawObjType()))
		return static_cast<paf::graph::TextObj *>(obj);
	return NULL;
}

paf::graph::TextObj *Lv1StatusActivityVisualizer::GetCommentCountTextObj(paf::ui::Widget *panel)
{
	paf::graph::DrawObj *obj = panel->GetDrawObj(STATUS_DRAWOBJ_COMMENT_COUNT);
	if (obj != NULL && obj->IsInherit(paf::graph::TextObj::DrawObjType()))
		return static_cast<paf::graph::TextObj *>(obj);
	return NULL;
}

void Lv1StatusActivityVisualizer::OnActionIconDecide(int eventId, paf::ui::Handler *handler,
						     paf::ui::Event *event)
{
	(void)eventId;
	(void)handler;
	(void)event;
	LaunchActionUri(m_record, GetCategory());
}

void Lv1StatusActivityVisualizer::OnPanelDecide(int eventId, paf::ui::Handler *handler,
						paf::ui::Event *event)
{
	(void)eventId;
	(void)handler;
	(void)event;
}

bool Lv1StatusActivityVisualizer::CustomizeActionIcon(paf::ui::Widget *iconWidget)
{
	(void)iconWidget;
	return false;
}

PafVec4 Lv1StatusActivityVisualizer::GetPreferredSize()
{
	return PafVec4(832.0f, 144.0f, 0.0f, 0.0f);
}

bool Lv1StatusActivityVisualizer::HasNewComment()
{
	return false;
}

bool Lv1StatusActivityVisualizer::ShouldShowTitleName()
{
	return true;
}

/* 0x81350658 */
void Lv1StatusActivityVisualizer::UpdateBadges()
{
	paf::ui::Widget *panel =
		m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_PANEL_BTN), 0);
	if (panel == NULL)
		return;

	paf::graph::DrawObj *unreadObj = panel->GetDrawObj(STATUS_DRAWOBJ_UNREAD);
	if (unreadObj != NULL)
		unreadObj->SetVisible(IsUnread());

	int texId = HasNewComment() ? TEX_COMMENT_ICON_NEW : TEX_COMMENT_ICON_NORMAL;
	paf::intrusive_ptr<paf::graph::Surface> commentTex =
		paf::Framework::Instance()->GetCommonTexture(paf::IDParam(texId));
	panel->SetTexture(commentTex, STATUS_DRAWOBJ_COMMENT_ICON, 0);
}

/* 0x8135039E */
paf::ui::Widget *Lv1StatusActivityVisualizer::GetSubWidget(int kind)
{
	if (kind == 0)
		return m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_PANEL_BTN), 0);
	if (kind == 1)
		return m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_AVATAR_BTN), 0);
	if (kind == 2)
		return m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_ACTION_BTN), 0);
	return NULL;
}

/* 0x81350506 */
paf::ui::Widget *Lv1StatusActivityVisualizer::GetCardRootWidget()
{
	int displayMode = GetDisplayMode();
	if (displayMode == ACTIVITY_DB_OWNER_TITLE)
		return m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_TITLE_CARD_ROOT), 0);
	if (displayMode == ACTIVITY_DB_OWNER_USER || displayMode == 0)
		return m_listItem->FindChild(paf::IDParam(WIDGET_STATUS_USER_CARD_ROOT), 0);
	return NULL;
}

/* 0x81350C5A body */
void Lv1StatusActivityVisualizer::ShowDeleteErrorDialog(int errorCode)
{
	CloseErrorDialog();

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;
	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(rec.get());

	CommonGuiMessageDialogParam param;
	param.type = errorCode;
	param.priority = activityUiGetDialogPriority();
	param.handler = new StatusCardErrorDialogHandler(statusRec->m_status.rowid);
	m_errorDialogId = SceCommonGuiDialog_20B141F1(&param);
}

void Lv1StatusActivityVisualizer::CloseErrorDialog()
{
	if (m_errorDialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_errorDialogId);
		m_errorDialogId = -1;
	}
}

/* 0x813508A8 */
void Lv1StatusActivityVisualizer::AvatarDecideThunk(int eventId, paf::ui::Handler *handler,
						    paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	BaseActivityVisualizer *self = static_cast<BaseActivityVisualizer *>(userdata);
	self->LaunchAuthorProfile();
}

/* 0x81350894 */
void Lv1StatusActivityVisualizer::ActionIconDecideThunk(int eventId, paf::ui::Handler *handler,
							paf::ui::Event *event, void *userdata)
{
	Lv1StatusActivityVisualizer *self = static_cast<Lv1StatusActivityVisualizer *>(userdata);
	self->OnActionIconDecide(eventId, handler, event);
}

/* 0x813508BC */
void Lv1StatusActivityVisualizer::PanelDecideThunk(int eventId, paf::ui::Handler *handler,
						   paf::ui::Event *event, void *userdata)
{
	Lv1StatusActivityVisualizer *self = static_cast<Lv1StatusActivityVisualizer *>(userdata);
	self->OnPanelDecide(eventId, handler, event);
}

/* 0x813508D0 */
void Lv1StatusActivityVisualizer::OnPanelImageLoaded(int eventId, paf::ui::Handler *handler,
						     paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	paf::ui::Widget *widget = static_cast<paf::ui::Widget *>(handler);
	ActivityDrawObj *panelObj =
		static_cast<ActivityDrawObj *>(widget->GetDrawObj(STATUS_DRAWOBJ_PANEL));
	if (panelObj != NULL)
		panelObj->SetMode(static_cast<int>(reinterpret_cast<intptr_t>(userdata)));
}

/* -------------------------------------------------------------------------- */
/* Lv1StatusVisualizerFactory                                                 */
/* -------------------------------------------------------------------------- */

/* 0x813508E8 */
Lv1StatusVisualizerFactory::Lv1StatusVisualizerFactory(paf::Plugin *plugin,
						       paf::cache::CacheManager *cacheManager)
	: m_plugin(plugin),
	  m_cacheManager(cacheManager)
{
}

Lv1StatusVisualizerFactory::~Lv1StatusVisualizerFactory()
{
}

/* 0x813508FC */
paf::intrusive_ptr<paf::ui::filelist::Visualizer> Lv1StatusVisualizerFactory::CreateVisualizer(
	paf::ui::Widget *parent,
	const paf::common::SharedPtr<Live2Iterator> &iterator,
	int index, SceInt64 cellId)
{
	(void)index;
	paf::intrusive_ptr<paf::ui::filelist::Visualizer> vis(
		new Lv1StatusActivityVisualizer(parent, m_plugin, m_cacheManager,
						iterator, cellId));
	return vis;
}

PafVec4 Lv1StatusVisualizerFactory::GetVisualSize()
{
	return PafVec4(832.0f, 144.0f, 0.0f, 0.0f);
}

/* -------------------------------------------------------------------------- */
/* ActivityStatusDialog                                                       */
/* -------------------------------------------------------------------------- */

/* 0x8135236C / 0x8135286C */
ActivityStatusDialog::ActivityStatusDialog(
	paf::ui::Widget *parent, paf::Plugin *plugin,
	paf::cache::CacheManager *cacheManager,
	const paf::intrusive_ptr<Live2Record> &record,
	int displayMode,
	const paf::common::SharedPtr<Live2AccessorProxy> &accessorProxy,
	const paf::common::SharedPtr<Live2VisualizerFactory> &commentFactory)
	: m_dialogId(0),
	  m_plugin(plugin),
	  m_busyFooter(1, plugin),
	  m_cacheManager(cacheManager),
	  m_record(record),
	  m_displayMode(displayMode),
	  m_dialogRoot(NULL),
	  m_listView(NULL),
	  m_jobSlots(),
	  m_syncHandle(),
	  m_voteInfo(),
	  m_goodJobY(0.0f),
	  m_commonDialogId(-1),
	  m_popupMenuOpen(false),
	  m_syncingDetail(false),
	  m_postingComment(false),
	  m_inputDisabled(false),
	  m_animateNewComment(false),
	  m_commentCount(0),
	  m_selectedCommentVis(NULL),
	  m_postAnimWidget(NULL),
	  m_postAnimTime(0.0f),
	  m_postAnimStartPos(0.0f, 0.0f, 0.0f, 0.0f),
	  m_postAnimStartSize(0.0f, 0.0f, 0.0f, 0.0f),
	  m_anchorSize(832.0f, 144.0f, 0.0f, 0.0f)
{
	if (s_dialogManager != NULL) {
		m_dialogId = s_dialogManager->nextDialogId++;
		s_dialogManager->dialogs[m_dialogId] = this;
	}

	m_voteInfo.mainRowid = 0;
	m_voteInfo.isVoted = false;
	m_voteInfo.reserved = 0;
	m_voteInfo.numOfVotes = 0;

	if (m_record.get() != NULL && m_record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *statusRec =
			static_cast<Live2StatusRecord *>(m_record.get());
		ActivityDetailSyncCallback cb;
		cb.func = &OnDetailSyncCallback;
		cb.dialogId = m_dialogId;
		activityUiInvokeNetAcquire();
		m_syncingDetail = true;
		SceActivityDb_FEB3A02F(&m_syncHandle, &statusRec->m_status.rowid,
				       &statusRec->m_status.titleId, &cb);
	}

	paf::ui::Widget *staleDialog =
		parent->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_ROOT), 0);
	if (staleDialog != NULL)
		staleDialog->m_name = paf::IDParam("");

	paf::Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(parent, paf::IDParam(TEMPLATE_STATUS_DIALOG), openParam);
	m_dialogRoot = parent->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_ROOT), 0);
	if (m_dialogRoot == NULL)
		return;

	m_listView = static_cast<paf::ui::FileList *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_LIST), 0));
	if (m_listView == NULL)
		return;

	m_busyFooter.SetListView(m_listView);
	paf::intrusive_ptr<paf::ui::filelist::SegmentSource> segSource(new BasicSegmentSource());
	m_listView->SetSegmentSource(segSource);
	m_listView->SetScrollType(paf::ui::ListView::SCROLL_TYPE_VERTICAL);

	SetupCommentList(accessorProxy, commentFactory);
	SetupPanel();
	m_dialogRoot->AddDeleteCB(&OnDialogRootDeleteCallback,
				  reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId)));
}

/* 0x813528EC / 0x81352A64 / 0x81352A6C */
ActivityStatusDialog::~ActivityStatusDialog()
{
	if (s_dialogManager != NULL) {
		paf::map<int, ActivityStatusDialog *>::iterator it =
			s_dialogManager->dialogs.find(m_dialogId);
		if (it != s_dialogManager->dialogs.end())
			s_dialogManager->dialogs.erase(it);
	}

	if (m_popupMenuOpen) {
		shellPopupListClose(0);
		m_popupMenuOpen = false;
	}
	if (m_commonDialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_commonDialogId);
		m_commonDialogId = -1;
	}
	if (m_syncingDetail) {
		SceActivityDb_5B44F019(&m_syncHandle);
		activityUiInvokeNetRelease();
		m_syncingDetail = false;
	}

	m_jobSlots.CancelAll();
}

/* 0x81350E74 */
void ActivityStatusDialog::SetupPanel()
{
	if (m_dialogRoot == NULL || m_record.get() == NULL ||
	    m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	paf::ui::Widget *bgWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_BG), 0);
	if (bgWidget == NULL)
		return;

	paf::graph::DrawObj *existingObj = bgWidget->GetDrawObj(STATUS_DRAWOBJ_PANEL);
	if (existingObj == NULL ||
	    existingObj->GetDrawObjType() != ActivityDrawObj::DrawObjType()) {
		paf::common::SharedPtr<paf::graph::DrawObj> newObj(new ActivityDrawObj());
		bgWidget->SwapDrawObj(newObj, STATUS_DRAWOBJ_PANEL);
	}

	ActivityDrawObj *panelObj =
		static_cast<ActivityDrawObj *>(bgWidget->GetDrawObj(STATUS_DRAWOBJ_PANEL));
	if (panelObj == NULL)
		return;

	panelObj->SetMode(0);
	SceUInt32 swappedColor = 0;
	BaseActivityVisualizer::SwapColorEndian(statusRec->m_status.announcePanelBgColor,
						&swappedColor);
	panelObj->m_color = swappedColor;

	paf::intrusive_ptr<paf::graph::Surface> baseTex =
		paf::Framework::Instance()->GetCommonTexture(
			paf::IDParam(TEX_STATUS_PANEL_BASE));
	bgWidget->SetTexture(baseTex, STATUS_DRAWOBJ_PANEL, 0);

	const paf::string &panelUrl = statusRec->m_status.announcePanelUrl;
	if (!panelUrl.empty()) {
		int maskId = (statusRec->m_status.ownerType == ACTIVITY_DB_OWNER_TITLE)
				     ? TEX_STATUS_PANEL_TITLE_MASK
				     : TEX_STATUS_PANEL_USER_MASK;
		paf::intrusive_ptr<paf::graph::Surface> maskTex =
			paf::Framework::Instance()->GetCommonTexture(paf::IDParam(maskId));
		bgWidget->SetTexture(maskTex, STATUS_DRAWOBJ_PANEL, 1);

		paf::intrusive_ptr<paf::cache::CacheImageInterface> imageCache;
		BaseActivityVisualizer::CreateActivityImageCache(panelUrl, imageCache,
								 m_cacheManager);
		bgWidget->AddEventCallback(
			EVENT_CACHE_IMAGE_READY,
			&Lv1StatusActivityVisualizer::OnPanelImageLoaded,
			reinterpret_cast<void *>(1));
		bgWidget->SetDrawObjMaskBit(&imageCache, STATUS_DRAWOBJ_PANEL, 2);
	}
}

/* 0x81351B64 */
void ActivityStatusDialog::SetupHeader(paf::ui::Widget *headerWidget)
{
	if (headerWidget == NULL || m_record.get() == NULL ||
	    m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	int category = BaseActivityVisualizer::ClassifyUriCategory(m_record);

	paf::ui::Widget *avatarBtn =
		headerWidget->FindChild(paf::IDParam(WIDGET_STATUS_AVATAR_BTN), 0);
	if (avatarBtn != NULL) {
		BaseActivityVisualizer::ApplyAvatarIconForRecord(m_record, avatarBtn,
								 m_cacheManager);
		avatarBtn->AddEventCallback(EVENT_BUTTON_DECIDE,
					    &OnDialogAvatarDecideCallback, this);
	}

	paf::ui::Widget *actionBtn =
		headerWidget->FindChild(paf::IDParam(WIDGET_STATUS_ACTION_BTN), 0);
	if (actionBtn != NULL) {
		if (!CustomizeStatusActionIcon(actionBtn))
			BaseActivityVisualizer::ApplyActionIconForRecord(category, m_record,
									 actionBtn, m_cacheManager);
		actionBtn->AddEventCallback(EVENT_BUTTON_DECIDE,
					    &OnDialogActionDecideCallback, this);
	}

	paf::ui::Widget *optionBtn =
		headerWidget->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_OPTION_BTN), 0);
	if (optionBtn != NULL) {
		if (statusRec->m_status.ownerType == ACTIVITY_DB_OWNER_SELF) {
			optionBtn->AddEventCallback(
				EVENT_BUTTON_DECIDE,
				&OnHeaderOptionDecideCallback,
				reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId)));
		} else {
			optionBtn->Hide(0xff, NULL, NULL, 0.0f);
		}
	}

	paf::graph::TextObj *bodyText =
		static_cast<paf::graph::TextObj *>(headerWidget->GetDrawObj(STATUS_DRAWOBJ_BODY));
	if (bodyText != NULL) {
		paf::string ownerName(statusRec->m_status.ownerId);
		BaseActivityVisualizer::TruncateAtChar(ownerName, '@');
		paf::string combined = ownerName + "\n" + statusRec->m_status.message;
		if (!combined.empty()) {
			paf::wstring ownerWide;
			paf::common::Utf8ToUtf16(ownerName.c_str(), &ownerWide);
			paf::wstring combinedWide;
			paf::common::Utf8ToUtf16(combined.c_str(), &combinedWide);
			bodyText->SetString(combinedWide);
			bodyText->SetStyleAttribute(paf::graph::TextStyleAttribute_Bold,
						    0, 0, false);
			bodyText->SetStyleAttribute(paf::graph::TextStyleAttribute_Bold,
						    0, ownerWide.size(), true);
		}
	}
}

/* 0x81352078 */
void ActivityStatusDialog::SetupCommentList(
	const paf::common::SharedPtr<Live2AccessorProxy> &accessorProxy,
	const paf::common::SharedPtr<Live2VisualizerFactory> &commentFactory)
{
	if (m_listView == NULL || m_record.get() == NULL)
		return;

	paf::intrusive_ptr<BasicSegmentSource> segSource(
		static_cast<BasicSegmentSource *>(m_listView->m_segment_source.get()));
	if (segSource.get() == NULL)
		return;

	paf::Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(m_listView, paf::IDParam(TEMPLATE_STATUS_DIALOG_HEADER), openParam);
	paf::ui::Widget *headerWidget =
		m_listView->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_HEADER_ROOT), 0);
	SetupHeader(headerWidget);

	paf::ui::filelist::SegmentListAccessor::OptionParam option;
	option.m_layout_type = paf::ui::ListView::LAYOUT_TYPE_LIST;
	option.m_header = headerWidget;
	option.m_header_size = PafVec4(0.0f, 0.0f, 0.0f, 0.0f);
	option.m_header_floating = false;
	option.m_header_disappearance = false;
	option.m_column_num = 0;
	option.m_min_size = PafVec2(0.0f, 0.0f);
	option.m_joined = true;

	paf::common::SharedPtr<Live2AccessorFactory> commentAccFactory(
		new Live2CommentAccessorFactory(accessorProxy, m_record->m_rowid, 128));
	paf::intrusive_ptr<Live2CellSource> cellSource(
		new Live2CellSource(commentFactory, commentAccFactory));
	cellSource->SetRefreshMode(2);
	cellSource->SetCountCallback(&OnCommentCountCallback, m_dialogId);

	paf::intrusive_ptr<paf::ui::filelist::CellSource> baseCellSource(cellSource.get());
	segSource->InsertSegment(0, baseCellSource, option);

	bool isPosting = false;
	if (m_record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *statusRec =
			static_cast<Live2StatusRecord *>(m_record.get());
		isPosting = activityUiIsStatusPostingComment(&statusRec->m_status.rowid);
	}
	m_busyFooter.AttachSegment(isPosting);
	m_listView->BlockSync();
	m_listView->UnblockSync();
	if (isPosting)
		StartPostCommentAnimation(NULL);
}

/* 0x81352C14 */
void ActivityStatusDialog::SetupControls()
{
	if (m_dialogRoot == NULL)
		return;

	paf::ui::Widget *panelWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_PANEL), 0);
	if (panelWidget != NULL) {
		SceUInt32 bgColor = activityUiGetCachedPanelBgColor();
		SceUInt32 swapped = 0;
		BaseActivityVisualizer::SwapColorEndian(bgColor, &swapped);
		PafVec4 colorVec(static_cast<float>(swapped & 0xFF) / 255.0f,
				 static_cast<float>((swapped >> 8) & 0xFF) / 255.0f,
				 static_cast<float>((swapped >> 16) & 0xFF) / 255.0f,
				 static_cast<float>((swapped >> 24) & 0xFF) / 255.0f);
		panelWidget->SetColor(colorVec, NULL, ANIM_CHANNEL_COLOR, 0, 0.0f, NULL, NULL);
	}

	paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
	if (commentBox != NULL) {
		commentBox->m_max_length = ACTIVITY_COMMENT_MAX_LENGTH;
		void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
		commentBox->AddEventCallback(EVENT_TEXTBOX_ENTER,
					     &OnCommentBoxEnterCallback, idArg);
		commentBox->AddEventCallback(EVENT_TEXTBOX_EDIT_BEGIN,
					     &OnCommentBoxBeginEditCallback, idArg);
		commentBox->AddEventCallback(EVENT_TEXTBOX_EDIT_END,
					     &OnCommentBoxEndEditCallback, idArg);
		commentBox->AddEventCallback(EVENT_TEXTBOX_CHANGED,
					     &OnCommentBoxTextChangedCallback, idArg);
		RefreshCommentBoxState(true);
	}
}

/* 0x81351744 */
void ActivityStatusDialog::RefreshCommentBoxState(bool immediate)
{
	if (m_dialogRoot == NULL)
		return;

	paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
	if (commentBox == NULL)
		return;

	bool canEdit = !m_postingComment && !m_inputDisabled && (m_commentCount < 100);
	commentBox->SetEditMode(canEdit);
	(void)immediate;
}

/* 0x813531DA */
void ActivityStatusDialog::Open(paf::ui::ListItem *anchorItem)
{
	if (m_dialogRoot == NULL)
		return;

	SetupControls();

	if (anchorItem != NULL)
		m_anchorSize = *anchorItem->GetSize(0);

	paf::ui::Widget *headerWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_HEADER_ROOT), 0);
	if (headerWidget != NULL && m_record.get() != NULL &&
	    m_record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *statusRec =
			static_cast<Live2StatusRecord *>(m_record.get());
		paf::graph::TextObj *timeText = static_cast<paf::graph::TextObj *>(
			headerWidget->GetDrawObj(STATUS_DRAWOBJ_TIME));
		if (timeText != NULL) {
			paf::wstring formatted = BaseActivityVisualizer::FormatActivityTime(
				static_cast<SceUInt64>(statusRec->m_status.creationTime));
			if (ShouldShowTitleName() && !statusRec->m_status.titleName.empty()) {
				paf::wstring titleWide;
				paf::common::Utf8ToUtf16(statusRec->m_status.titleName.c_str(),
							 &titleWide);
				formatted = titleWide + L"  " + formatted;
			}
			timeText->SetString(formatted);
		}
	}

	paf::ui::Widget *bgWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_BG), 0);
	paf::ui::Widget *panelWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_PANEL), 0);

	ScePafWidget_A96A0B4A(m_dialogRoot, 1);
	m_dialogRoot->SetIntervalEvent(TIMER_ID_DIALOG_CLIP, 16.0f, &OnDialogClipTimer, NULL);

	if (bgWidget != NULL) {
		ScePafWidget_44B22ACA(bgWidget, paf::common::transition::Type_FadeinFast,
				      0.0f, NULL, NULL);
	}
	if (panelWidget != NULL) {
		ScePafWidget_44B22ACA(panelWidget, paf::common::transition::Type_Popup4,
				      0.0f, NULL, NULL);
	}

	void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
	PafVec4 finalSize = *m_dialogRoot->GetSize(0);
	m_dialogRoot->SetSize(finalSize, new paf::Timer(250.0f), ANIM_CHANNEL_SIZE, 0, 0.0f,
			      &OnDialogOpenTransitionDone, idArg);

	int voteCount = BaseActivityVisualizer::GetVoteCount(m_record);
	UpdateGoodJobArea(voteCount, false);
	RequestVoteInfo();
	if (!ShouldAnimateOpen())
		OnDialogOpened();
}

/* 0x81353E16 */
void ActivityStatusDialog::Close()
{
	OnDialogClosed();
	if (m_dialogRoot == NULL) {
		delete this;
		return;
	}

	if (m_popupMenuOpen) {
		shellPopupListClose(0);
		m_popupMenuOpen = false;
	}

	paf::ui::Widget *bgWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_BG), 0);
	paf::ui::Widget *panelWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_PANEL), 0);

	if (bgWidget != NULL) {
		ScePafWidget_F1176ACC(bgWidget, paf::common::transition::Type_FadeinFast,
				      0.0f, NULL, NULL);
	}
	if (panelWidget != NULL) {
		ScePafWidget_F1176ACC(panelWidget, paf::common::transition::Type_Popup4,
				      0.0f, NULL, NULL);
	}

	void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
	m_dialogRoot->SetSize(m_anchorSize, new paf::Timer(250.0f), ANIM_CHANNEL_SIZE, 0, 0.0f,
			      &OnDialogCloseTransitionDone, idArg);
}

void ActivityStatusDialog::CloseWithReason(int reason)
{
	(void)reason;
	Close();
}

bool ActivityStatusDialog::HandleSysUpdateError(int errorCode)
{
	(void)errorCode;
	return false;
}

int ActivityStatusDialog::GetVoteQueryType()
{
	return 0;
}

/* 0x81357D88 */
void ActivityStatusDialog::OnAvatarDecide()
{
	if (m_record.get() != NULL)
		BaseActivityVisualizer::LaunchProfileForRecord(m_record);
}

/* 0x81357DC2 */
void ActivityStatusDialog::OnStatusActionDecide()
{
	int category = BaseActivityVisualizer::ClassifyUriCategory(m_record);
	BaseActivityVisualizer::LaunchActionUri(m_record, category);
}

/* 0x81357E18 */
void ActivityStatusDialog::OnBackButtonDecide()
{
	Close();
}

/* 0x813585DC */
float ActivityStatusDialog::GetDialogHeight()
{
	return 530.0f;
}

/* 0x813585E8 */
float ActivityStatusDialog::GetDialogCenterY()
{
	return 256.0f;
}

/* 0x813585F2 */
float ActivityStatusDialog::GetListViewHeight()
{
	return 430.0f;
}

bool ActivityStatusDialog::ShouldAnimateOpen()
{
	return true;
}

bool ActivityStatusDialog::ShouldAnimateClose()
{
	return true;
}

bool ActivityStatusDialog::IsUnread()
{
	return false;
}

bool ActivityStatusDialog::ShouldShowTitleName()
{
	return true;
}

bool ActivityStatusDialog::UseWideCommentBox()
{
	return false;
}

bool ActivityStatusDialog::HasBackButton()
{
	return true;
}

bool ActivityStatusDialog::HasOptionButton()
{
	return true;
}

/* 0x813585FE */
const paf::IDParam &ActivityStatusDialog::GetBackButtonTemplateId()
{
	static paf::IDParam s_backTemplateId(1665067179U);
	return s_backTemplateId;
}

/* 0x8135869A */
const paf::IDParam &ActivityStatusDialog::GetOptionButtonTemplateId()
{
	static paf::IDParam s_optionTemplateId(static_cast<uint32_t>(-1229031538));
	return s_optionTemplateId;
}

/* 0x81358736 */
const paf::IDParam &ActivityStatusDialog::GetBackButtonId()
{
	static paf::IDParam s_backButtonId(static_cast<uint32_t>(-677217553));
	return s_backButtonId;
}

/* 0x813587D2 */
const paf::IDParam &ActivityStatusDialog::GetOptionButtonId()
{
	static paf::IDParam s_optionButtonId(2125679975U);
	return s_optionButtonId;
}

void ActivityStatusDialog::OnDialogOpened()
{
}

void ActivityStatusDialog::OnDialogClosed()
{
}

bool ActivityStatusDialog::CustomizeStatusActionIcon(paf::ui::Widget *iconWidget)
{
	(void)iconWidget;
	return false;
}

/* 0x813549C8 */
void ActivityStatusDialog::UpdateGoodJobArea(int voteCount, bool hasVoteInfo)
{
	if (m_dialogRoot == NULL)
		return;

	paf::ui::Widget *headerGroup =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_GOODJOB_HEADER_BOX), 0);
	if (headerGroup == NULL)
		return;

	bool isTitleStatus = (m_displayMode == ACTIVITY_DB_OWNER_TITLE);
	void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));

	if (voteCount > 0 || (hasVoteInfo && m_voteInfo.numOfVotes > 0)) {
		paf::ui::Widget *voteBtn =
			headerGroup->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_BTN), 0);
		if (voteBtn != NULL) {
			ScePafWidget_5A9793B0(voteBtn, 0.0f);
			voteBtn->m_name = paf::IDParam("");
		}

		paf::ui::Widget *areaWidget =
			headerGroup->FindChild(paf::IDParam(WIDGET_GOODJOB_AREA), 0);
		if (areaWidget == NULL) {
			paf::Plugin::TemplateOpenParam openParam;
			m_plugin->TemplateOpen(headerGroup, paf::IDParam(TEMPLATE_GOODJOB_AREA),
					       openParam);
			areaWidget =
				headerGroup->FindChild(paf::IDParam(WIDGET_GOODJOB_AREA), 0);
			if (areaWidget == NULL)
				return;
			areaWidget->Hide(0xff, NULL, NULL, 0.0f);
			areaWidget->Show(0xff, new paf::Timer(150.0f), NULL, 0.0f);
		}

		paf::ui::ButtonBase *actionBtn = static_cast<paf::ui::ButtonBase *>(
			areaWidget->FindChild(paf::IDParam(WIDGET_GOODJOB_ACTION_BTN), 0));
		if (actionBtn != NULL) {
			if (isTitleStatus) {
				actionBtn->Hide(0xff, NULL, NULL, 0.0f);
			} else if (m_voteInfo.isVoted) {
				actionBtn->Enable(false);
				actionBtn->Show(0xff, NULL, NULL, 0.0f);
				paf::intrusive_ptr<paf::graph::Surface> tex =
					paf::Framework::Instance()->GetCommonTexture(
						paf::IDParam(TEX_GOODJOB_CANCEL));
				actionBtn->SetTexture(tex, 0, 0);
				const wchar_t *label =
					paf::Framework::Instance()->GetCommonString(
						paf::IDParam(STR_GOODJOB_CANCEL));
				actionBtn->SetString(paf::wstring(label ? label : L""), 1);
				actionBtn->AddEventCallback(EVENT_BUTTON_DECIDE,
							    &OnCancelGoodJobCallback, idArg);
			} else {
				paf::intrusive_ptr<paf::graph::Surface> tex =
					paf::Framework::Instance()->GetCommonTexture(
						paf::IDParam(TEX_GOODJOB_DO));
				actionBtn->SetTexture(tex, 0, 0);
				const wchar_t *label =
					paf::Framework::Instance()->GetCommonString(
						paf::IDParam(STR_GOODJOB_DO));
				actionBtn->SetString(paf::wstring(label ? label : L""), 1);
				actionBtn->AddEventCallback(EVENT_BUTTON_DECIDE,
							    &OnVoteGoodJobCallback, idArg);

				if (static_cast<unsigned int>(m_voteInfo.numOfVotes) >=
				    ACTIVITY_MAX_GOODJOB_VOTES) {
					actionBtn->Disable(false);
					actionBtn->Hide(0xff, NULL, NULL, 0.0f);
				} else {
					actionBtn->Enable(false);
					actionBtn->Show(0xff, NULL, NULL, 0.0f);
				}
			}

			if (!isTitleStatus &&
			    static_cast<unsigned int>(m_voteInfo.numOfVotes) <
				    ACTIVITY_MAX_GOODJOB_VOTES) {
				if (actionBtn->m_prepare_count != 0) {
					actionBtn->Hide(0xff, NULL, NULL, 0.0f);
					actionBtn->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED,
								    &OnGoodJobButtonSizeChanged,
								    NULL);
				} else {
					actionBtn->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED,
								    NULL, NULL);
					OnGoodJobButtonSizeChanged(0, actionBtn, NULL, NULL);
				}
			}
		}

		paf::ui::Widget *namesWidget =
			areaWidget->FindChild(paf::IDParam(WIDGET_GOODJOB_NAMES_TEXT), 0);
		if (namesWidget == NULL)
			return;

		paf::graph::TextObj *namesText =
			static_cast<paf::graph::TextObj *>(namesWidget->GetDrawObj(1));
		if (namesText == NULL)
			return;

		paf::wstring outText;
		size_t voterCount = m_voteInfo.ownerIds.size();
		if (voterCount == 1) {
			paf::wstring firstVoter;
			paf::common::Utf8ToUtf16(m_voteInfo.ownerIds[0].c_str(), &firstVoter);
			BaseActivityVisualizer::TruncateAtWideChar(firstVoter, L'@');
			const wchar_t *fmt = paf::Framework::Instance()->GetCommonString(
				paf::IDParam(STR_GOODJOB_ONE));
			outText = shellFormatText(fmt, firstVoter.c_str());
		} else if (voterCount == 2) {
			paf::wstring firstVoter;
			paf::wstring secondVoter;
			paf::common::Utf8ToUtf16(m_voteInfo.ownerIds[0].c_str(), &firstVoter);
			paf::common::Utf8ToUtf16(m_voteInfo.ownerIds[1].c_str(), &secondVoter);
			BaseActivityVisualizer::TruncateAtWideChar(firstVoter, L'@');
			BaseActivityVisualizer::TruncateAtWideChar(secondVoter, L'@');
			const wchar_t *fmt = paf::Framework::Instance()->GetCommonString(
				paf::IDParam(STR_GOODJOB_TWO));
			outText = shellFormatText(fmt, firstVoter.c_str(), secondVoter.c_str());
		} else {
			wchar_t numBuf[32];
			sce_paf_swprintf(numBuf, 32, L"%d", m_voteInfo.numOfVotes);
			const wchar_t *fmt = paf::Framework::Instance()->GetCommonString(
				paf::IDParam(STR_GOODJOB_MANY));
			outText = shellFormatText(fmt, numBuf);
			outText += L"\n";

			size_t shownCount = voterCount;
			if (shownCount > ACTIVITY_MAX_GOODJOB_NAMES)
				shownCount = ACTIVITY_MAX_GOODJOB_NAMES;
			for (size_t i = 0; i < shownCount; ++i) {
				paf::wstring voterWide;
				paf::common::Utf8ToUtf16(m_voteInfo.ownerIds[i].c_str(), &voterWide);
				BaseActivityVisualizer::TruncateAtWideChar(voterWide, L'@');
				if (i != 0)
					outText += L", ";
				const wchar_t *nameFmt =
					paf::Framework::Instance()->GetCommonString(
						paf::IDParam(STR_GOODJOB_NAME_FMT));
				paf::wstring formattedName =
					shellFormatText(nameFmt, voterWide.c_str());
				outText += formattedName.c_str();
			}
		}
		namesText->SetString(outText);
	} else {
		paf::ui::Widget *areaWidget =
			headerGroup->FindChild(paf::IDParam(WIDGET_GOODJOB_AREA), 0);
		if (areaWidget != NULL) {
			ScePafWidget_5A9793B0(areaWidget, 0.0f);
			areaWidget->m_name = paf::IDParam("");
		}

		paf::ui::Widget *voteBtn =
			headerGroup->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_BTN), 0);
		if (isTitleStatus) {
			if (voteBtn != NULL)
				voteBtn->DestroyWidget();
			return;
		}

		if (voteBtn == NULL) {
			paf::Plugin::TemplateOpenParam openParam;
			m_plugin->TemplateOpen(headerGroup, paf::IDParam(TEMPLATE_GOODJOB_VOTE_BTN),
					       openParam);
			voteBtn = headerGroup->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_BTN), 0);
			if (voteBtn == NULL)
				return;

			voteBtn->Hide(0xff, NULL, NULL, 0.0f);
			voteBtn->AddEventCallback(EVENT_BUTTON_DECIDE,
						  &OnVoteGoodJobCallback, idArg);

			paf::ui::Widget *labelWidget =
				voteBtn->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_LABEL), 0);
			if (labelWidget != NULL) {
				const wchar_t *label =
					paf::Framework::Instance()->GetCommonString(
						paf::IDParam(STR_GOODJOB_DO));
				labelWidget->SetString(paf::wstring(label ? label : L""));
			}
		}
	}
}

/* 0x81355DE0 */
void ActivityStatusDialog::RequestVoteInfo()
{
	if (m_record.get() == NULL || m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	paf::common::SharedPtr<paf::job::JobItem> job(
		new GetVoteInfoJob(statusRec->m_status.rowid, statusRec->m_status.titleId,
				   statusRec->m_status.ownerId, m_dialogId,
				   ACTIVITY_DB_QUERY_ALL));
	paf::job::JobQueue *queue =
		paf::ui::filelist::Manager::GetJobQueue(
			paf::ui::filelist::Manager::QUEUE_TYPE_NORMAL);
	m_jobSlots.Enqueue(0, queue, job);
}

/* 0x8135AC9E helper */
void ActivityStatusDialog::OnVoteInfoReady(int errorCode, const ActivityDbGoodJob &goodJob)
{
	if (errorCode != 0)
		return;
	m_voteInfo = goodJob;
	UpdateGoodJobArea(m_voteInfo.numOfVotes, true);
}

/* 0x81355FE6 */
void ActivityStatusDialog::OnVoteFinished(int errorCode, bool isNewVote)
{
	(void)isNewVote;
	if (errorCode != 0)
		ShowErrorDialog(errorCode, errorCode == ACTIVITY_ERR_NOT_FOUND);
	RequestVoteInfo();
}

/* 0x8135689A */
void ActivityStatusDialog::OnPostCommentFinished(int errorCode)
{
	m_postingComment = false;
	m_busyFooter.HideBusyHeader();
	RefreshCommentBoxState(false);

	if (errorCode != 0) {
		ShowErrorDialog(errorCode, errorCode == ACTIVITY_ERR_NOT_FOUND);
		return;
	}

	m_animateNewComment = true;

	if (m_dialogRoot != NULL) {
		paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
			m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
		if (commentBox != NULL)
			commentBox->SetString(paf::wstring());
	}
}

void ActivityStatusDialog::OnDeleteStatusFinished(int errorCode)
{
	if (errorCode != 0)
		ShowErrorDialog(errorCode, errorCode == ACTIVITY_ERR_NOT_FOUND);
}

void ActivityStatusDialog::OnDeleteCommentFinished(int errorCode, SceInt64 commentRowid)
{
	(void)commentRowid;
	if (errorCode != 0)
		ShowErrorDialog(errorCode, errorCode == ACTIVITY_ERR_NOT_FOUND);
}

void ActivityStatusDialog::OnGriefReportFinished(int errorCode, SceInt64 commentRowid)
{
	(void)commentRowid;
	if (errorCode != 0)
		ShowErrorDialog(errorCode, false);
}

/* 0x8135743A */
void ActivityStatusDialog::OnDetailSyncFinished(int errorCode)
{
	m_syncingDetail = false;
	if (errorCode == 0)
		RequestVoteInfo();
}

/* 0x81356B6C */
void ActivityStatusDialog::OnCacheInfoUpdated()
{
	SetupPanel();
	SetupControls();
}

/* 0x813576C6 */
void ActivityStatusDialog::OnCommentCountChanged(int count)
{
	m_commentCount = count;
	RefreshCommentBoxState(false);
}

/* 0x81356B66 */
void ActivityStatusDialog::SetAnimateNewComment(bool animate)
{
	m_animateNewComment = animate;
}

/* 0x81356DD0 */
void ActivityStatusDialog::ClosePopupIfSelectedComment(Lv2CommentActivityVisualizer *visualizer)
{
	if (m_popupMenuOpen && m_selectedCommentVis == visualizer) {
		shellPopupListClose(0);
		m_popupMenuOpen = false;
		m_selectedCommentVis = NULL;
	}
}

/* 0x81356DF2 */
void ActivityStatusDialog::OpenDeleteCommentMenu(paf::ui::Widget *anchorWidget,
						 Lv2CommentActivityVisualizer *visualizer)
{
	if (m_popupMenuOpen)
		return;

	ShellPopupListParam param(0, m_plugin);
	const wchar_t *deleteText = m_plugin->GetString(paf::IDParam(STR_POPUP_DELETE));
	ShellPopupListItem item;
	item.text = (deleteText != NULL) ? deleteText : L"";
	item.value = 0;
	param.items.push_back(item);
	param.anchor = anchorWidget;

	void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
	param.SetSelectCallback(&OnDeleteCommentMenuSelect, idArg);
	param.SetCloseCallback(&OnPopupMenuClosed, idArg);

	m_selectedCommentVis = visualizer;
	m_popupMenuOpen = true;
	shellPopupListOpen(&param, 0, 0);
}

/* 0x813570B2 */
void ActivityStatusDialog::OpenDeleteOrReportCommentMenu(paf::ui::Widget *anchorWidget,
							 Lv2CommentActivityVisualizer *visualizer)
{
	if (m_popupMenuOpen)
		return;

	ShellPopupListParam param(0, m_plugin);
	const wchar_t *deleteText = m_plugin->GetString(paf::IDParam(STR_POPUP_DELETE));
	ShellPopupListItem deleteItem;
	deleteItem.text = (deleteText != NULL) ? deleteText : L"";
	deleteItem.value = 0;
	param.items.push_back(deleteItem);

	const wchar_t *reportText = m_plugin->GetString(paf::IDParam(STR_POPUP_REPORT));
	ShellPopupListItem reportItem;
	reportItem.text = (reportText != NULL) ? reportText : L"";
	reportItem.value = 1;
	param.items.push_back(reportItem);

	param.anchor = anchorWidget;
	void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
	param.SetSelectCallback(&OnDeleteOrReportCommentMenuSelect, idArg);
	param.SetCloseCallback(&OnPopupMenuClosed, idArg);

	m_selectedCommentVis = visualizer;
	m_popupMenuOpen = true;
	shellPopupListOpen(&param, 0, 0);
}

/* 0x81357E20 */
void ActivityStatusDialog::OpenStatusOptionMenu(paf::ui::Widget *anchorWidget)
{
	if (m_popupMenuOpen)
		return;

	ShellPopupListParam param(0, m_plugin);
	const wchar_t *deleteText = m_plugin->GetString(paf::IDParam(STR_POPUP_DELETE));
	ShellPopupListItem item;
	item.text = (deleteText != NULL) ? deleteText : L"";
	item.value = 0;
	param.items.push_back(item);
	param.anchor = anchorWidget;

	void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
	param.SetSelectCallback(&OnStatusOptionMenuSelect, idArg);
	param.SetCloseCallback(&OnPopupMenuClosed, idArg);

	m_popupMenuOpen = true;
	shellPopupListOpen(&param, 0, 0);
}

/* 0x8135750C */
void ActivityStatusDialog::ShowDeleteStatusConfirmDialog()
{
	if (m_commonDialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_commonDialogId);
		m_commonDialogId = -1;
	}

	CommonGuiLayoutDialogParam param;
	param.layout = SceCommonGuiDialog_6CD802FC;
	const wchar_t *msg = m_plugin->GetString(paf::IDParam(STR_DELETE_STATUS_CONFIRM));
	param.text = (msg != NULL) ? msg : L"";
	param.priority = activityUiGetDialogPriority();
	param.handler = new DeleteStatusConfirmHandler(m_dialogId);
	m_commonDialogId = SceCommonGuiDialog_CCFCD9A6(&param);
}

/* 0x8135787C */
void ActivityStatusDialog::ShowDeleteCommentConfirmDialog(
	const paf::intrusive_ptr<Live2Record> &commentRecord)
{
	if (m_commonDialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_commonDialogId);
		m_commonDialogId = -1;
	}

	CommonGuiLayoutDialogParam param;
	param.layout = SceCommonGuiDialog_6CD802FC;
	const wchar_t *msg = m_plugin->GetString(paf::IDParam(STR_DELETE_COMMENT_CONFIRM));
	param.text = (msg != NULL) ? msg : L"";
	param.priority = activityUiGetDialogPriority();
	param.handler = new DeleteCommentConfirmHandler(m_dialogId, commentRecord);
	m_commonDialogId = SceCommonGuiDialog_CCFCD9A6(&param);
}

/* 0x813566FC */
void ActivityStatusDialog::ShowErrorDialog(int errorCode, bool closeStatusDialogOnDismiss)
{
	if (m_commonDialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_commonDialogId);
		m_commonDialogId = -1;
	}

	CommonGuiMessageDialogParam param;
	param.type = errorCode;
	param.priority = activityUiGetDialogPriority();
	param.handler = new StatusDialogErrorHandler(m_dialogId, closeStatusDialogOnDismiss);
	m_commonDialogId = SceCommonGuiDialog_20B141F1(&param);
}

/* 0x813580AC */
void ActivityStatusDialog::ShowTooManyJobsDialog()
{
	if (m_commonDialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_commonDialogId);
		m_commonDialogId = -1;
	}

	CommonGuiLayoutDialogParam param;
	param.layout = SceCommonGuiDialog_AF75CA6D;
	const wchar_t *msg = m_plugin->GetString(paf::IDParam(STR_TOO_MANY_JOBS));
	param.text = (msg != NULL) ? msg : L"";
	param.priority = activityUiGetDialogPriority();
	param.handler = new TooManyJobsDialogHandler(m_dialogId);
	m_commonDialogId = SceCommonGuiDialog_CCFCD9A6(&param);
}

void ActivityStatusDialog::StartDeleteStatus()
{
	if (s_serviceManager == NULL || m_record.get() == NULL ||
	    m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	if (CountPendingNetworkJobs() >= ACTIVITY_MAX_PENDING_JOBS) {
		ShowTooManyJobsDialog();
		return;
	}

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	SceInt64 statusRowid = statusRec->m_status.rowid;
	s_serviceManager->deletingStatusRowids.push_back(statusRowid);
	ShowStatusBusyByRowid(statusRowid);

	paf::common::SharedPtr<paf::job::JobItem> job(
		new DelStatusJob(s_serviceManager->syncHandle, statusRowid));
	s_serviceManager->postJobQueue->Enqueue(job);
	Close();
}

/* 0x81357A46 */
void ActivityStatusDialog::StartDeleteComment(const paf::intrusive_ptr<Live2Record> &commentRecord)
{
	if (s_serviceManager == NULL || m_record.get() == NULL ||
	    m_record->GetRecordKind() != LIVE2_RECORD_STATUS ||
	    commentRecord.get() == NULL ||
	    commentRecord->GetRecordKind() != LIVE2_RECORD_COMMENT)
		return;

	if (CountPendingNetworkJobs() >= ACTIVITY_MAX_PENDING_JOBS) {
		ShowTooManyJobsDialog();
		return;
	}

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	Live2CommentRecord *commentRec = static_cast<Live2CommentRecord *>(commentRecord.get());
	SceInt64 commentRowid = commentRec->m_comment.rowid;
	s_serviceManager->deletingCommentRowids.push_back(commentRowid);

	Lv2CommentActivityVisualizer *commentVis = FindCommentVisualizerByRowid(commentRowid);
	if (commentVis != NULL)
		commentVis->ShowCommentBusy();

	paf::common::SharedPtr<paf::job::JobItem> job(
		new DeleteCommentJob(s_serviceManager->syncHandle,
				     statusRec->m_status.rowid, commentRowid));
	s_serviceManager->postJobQueue->Enqueue(job);
}

/* 0x81357C9C */
void ActivityStatusDialog::StartGriefReport(const paf::intrusive_ptr<Live2Record> &commentRecord)
{
	if (m_listView == NULL || m_record.get() == NULL || commentRecord.get() == NULL)
		return;

	paf::intrusive_ptr<paf::ui::filelist::CellSource> cellSource =
		(m_listView->m_segment_source.get() != NULL)
			? m_listView->m_segment_source->GetCellSource(
				  0, m_listView->m_segment_source->GetID(0))
			: paf::intrusive_ptr<paf::ui::filelist::CellSource>();
	if (cellSource.get() == NULL || !cellSource->IsInherit(Live2CellSource::TypeName()))
		return;

	Live2CellSource *liveSource = static_cast<Live2CellSource *>(cellSource.get());
	if (liveSource->m_iterator.get() == NULL)
		return;

	paf::common::SharedPtr<Live2AccessorProxy> proxy =
		liveSource->m_iterator->GetAccessorProxy();
	if (proxy.get() == NULL)
		return;

	if (commentRecord->GetRecordKind() == LIVE2_RECORD_COMMENT) {
		Live2CommentRecord *commentRec =
			static_cast<Live2CommentRecord *>(commentRecord.get());
		Lv2CommentActivityVisualizer *commentVis =
			FindCommentVisualizerByRowid(commentRec->m_comment.rowid);
		if (commentVis != NULL)
			commentVis->ShowCommentBusy();
	}

	paf::common::SharedPtr<paf::job::JobItem> job(
		new GriefReportJob(m_record, commentRecord, proxy));
	paf::job::JobQueue *queue =
		paf::ui::filelist::Manager::GetJobQueue(
			paf::ui::filelist::Manager::QUEUE_TYPE_NORMAL);
	m_jobSlots.Enqueue(1, queue, job);
}

/* 0x8135825C */
void ActivityStatusDialog::OnSendComment()
{
	if (s_serviceManager == NULL || m_dialogRoot == NULL || m_record.get() == NULL ||
	    m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
	if (commentBox == NULL)
		return;

	paf::wstring wideMsg;
	commentBox->GetString(wideMsg);
	if (wideMsg.empty())
		return;

	if (CountPendingNetworkJobs() >= ACTIVITY_MAX_PENDING_JOBS) {
		ShowTooManyJobsDialog();
		return;
	}

	paf::string utf8Msg;
	paf::common::Utf16ToUtf8(wideMsg, &utf8Msg);

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	SceInt64 statusRowid = statusRec->m_status.rowid;

	m_postingComment = true;
	RefreshCommentBoxState(false);
	StartPostCommentAnimation(commentBox);

	s_serviceManager->postingCommentRowids.push_back(statusRowid);
	paf::common::SharedPtr<paf::job::JobItem> job(
		new PostCommentJob(s_serviceManager->syncHandle, statusRowid, utf8Msg));
	s_serviceManager->postJobQueue->Enqueue(job);
}

/* 0x813589C4 */
void ActivityStatusDialog::OnVoteGoodJob()
{
	if (s_serviceManager == NULL || m_record.get() == NULL ||
	    m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	if (CountPendingNetworkJobs() >= ACTIVITY_MAX_PENDING_JOBS) {
		ShowTooManyJobsDialog();
		return;
	}

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	SceInt64 statusRowid = statusRec->m_status.rowid;
	s_serviceManager->votingRowids.push_back(statusRowid);

	paf::common::SharedPtr<paf::job::JobItem> job(
		new VoteGoodJob(s_serviceManager->syncHandle, statusRowid));
	s_serviceManager->postJobQueue->Enqueue(job);
}

/* 0x81358BE0 */
void ActivityStatusDialog::OnCancelGoodJob()
{
	if (s_serviceManager == NULL || m_record.get() == NULL ||
	    m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	if (CountPendingNetworkJobs() >= ACTIVITY_MAX_PENDING_JOBS) {
		ShowTooManyJobsDialog();
		return;
	}

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	SceInt64 statusRowid = statusRec->m_status.rowid;
	s_serviceManager->cancelingVoteRowids.push_back(statusRowid);

	paf::common::SharedPtr<paf::job::JobItem> job(
		new CancelGoodJob(s_serviceManager->syncHandle, statusRowid));
	s_serviceManager->postJobQueue->Enqueue(job);
}

/* 0x81358ECA */
void ActivityStatusDialog::OnCommentBoxBeginEdit(paf::ui::Widget *textBox)
{
	if (m_dialogRoot == NULL || textBox == NULL)
		return;

	paf::ui::Widget *commentBar =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BAR), 0);
	if (commentBar != NULL) {
		PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
		PafVec4 curPos = commentBar->GetPos(0, NULL, zeroOffset);
		PafVec4 nextPos(curPos.extract_x(), curPos.extract_y() + 16.0f,
				curPos.extract_z(), curPos.extract_w());
		commentBar->SetPos(nextPos, new paf::Timer(200.0f), ANIM_CHANNEL_POS, 0,
				   0.0f, NULL, NULL);
	}
}

/* 0x813594BC */
void ActivityStatusDialog::OnCommentBoxEndEdit(paf::ui::Widget *textBox)
{
	if (m_dialogRoot == NULL || textBox == NULL)
		return;

	paf::ui::Widget *commentBar =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BAR), 0);
	if (commentBar != NULL) {
		PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
		PafVec4 curPos = commentBar->GetPos(0, NULL, zeroOffset);
		PafVec4 nextPos(curPos.extract_x(), curPos.extract_y() - 16.0f,
				curPos.extract_z(), curPos.extract_w());
		commentBar->SetPos(nextPos, new paf::Timer(200.0f), ANIM_CHANNEL_POS, 0,
				   0.0f, &OnShowFocusAfterAnim,
				   reinterpret_cast<void *>(1));
	}
}

void ActivityStatusDialog::SetInputEnabled(bool enabled)
{
	m_inputDisabled = !enabled;
	RefreshCommentBoxState(false);
}

/* 0x81351848 */
void ActivityStatusDialog::StartPostCommentAnimation(paf::ui::Widget *commentWidget)
{
	m_busyFooter.ShowBusyHeader();
	m_busyFooter.StartBusyIndicator();

	if (commentWidget != NULL && m_dialogRoot != NULL) {
		PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
		m_postAnimWidget = commentWidget;
		m_postAnimTime = 0.0f;
		m_postAnimStartPos = commentWidget->GetPos(0, NULL, zeroOffset);
		m_postAnimStartSize = *commentWidget->GetSize(0);
		void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
		m_dialogRoot->SetIntervalEvent(TIMER_ID_POST_COMMENT_ANIM, 16.0f,
					       &OnPostCommentAnimTimer, idArg);
	}
}

/* 0x8135A29A */
void ActivityStatusDialog::UpdateDialogLayout()
{
	if (m_dialogRoot == NULL)
		return;
	m_postAnimTime += 16.0f;
	if (m_postAnimTime >= 250.0f) {
		m_dialogRoot->KillIntervalEvent(TIMER_ID_POST_COMMENT_ANIM);
		m_postAnimWidget = NULL;
	}
}

/* -------------------------------------------------------------------------- */
/* Cross-unit Activity UI Registry & Service APIs                             */
/* -------------------------------------------------------------------------- */

/* 0x813564A2 / 0x81356576 */
ActivityStatusDialog *activityUiFindDialogByStatusRowid(const SceInt64 *statusRowid)
{
	if (s_dialogManager == NULL || statusRowid == NULL)
		return NULL;

	for (paf::map<int, ActivityStatusDialog *>::iterator it = s_dialogManager->dialogs.begin();
	     it != s_dialogManager->dialogs.end(); ++it) {
		ActivityStatusDialog *dialog = it->second;
		if (dialog != NULL && dialog->m_record.get() != NULL &&
		    dialog->m_record->GetRecordKind() == LIVE2_RECORD_STATUS) {
			Live2StatusRecord *statusRec =
				static_cast<Live2StatusRecord *>(dialog->m_record.get());
			if (statusRec->m_status.rowid == *statusRowid)
				return dialog;
		}
	}
	return NULL;
}

/* 0x8135B002 */
ActivityStatusDialog *activityUiFindDialogById(int dialogId)
{
	if (s_dialogManager == NULL)
		return NULL;
	paf::map<int, ActivityStatusDialog *>::iterator it =
		s_dialogManager->dialogs.find(dialogId);
	if (it == s_dialogManager->dialogs.end())
		return NULL;
	return it->second;
}

/* 0x8135B01C */
bool activityUiIsStatusDeleting(const SceInt64 *statusRowid)
{
	if (s_serviceManager == NULL || statusRowid == NULL)
		return false;
	return ContainsRowid(s_serviceManager->deletingStatusRowids, *statusRowid);
}

/* 0x8135AFCE */
bool activityUiIsCommentDeleting(const SceInt64 *commentRowid)
{
	if (s_serviceManager == NULL || commentRowid == NULL)
		return false;
	return ContainsRowid(s_serviceManager->deletingCommentRowids, *commentRowid);
}

bool activityUiIsStatusPostingComment(const SceInt64 *statusRowid)
{
	if (s_serviceManager == NULL || statusRowid == NULL)
		return false;
	return ContainsRowid(s_serviceManager->postingCommentRowids, *statusRowid);
}

/* 0x8135B050 */
void activityUiOpenDeleteCommentMenu(const SceInt64 *statusRowid, paf::ui::Widget *anchorWidget,
				     Lv2CommentActivityVisualizer *visualizer)
{
	ActivityStatusDialog *dialog = activityUiFindDialogByStatusRowid(statusRowid);
	if (dialog != NULL)
		dialog->OpenDeleteCommentMenu(anchorWidget, visualizer);
}

/* 0x8135B07E */
void activityUiOpenDeleteOrReportCommentMenu(const SceInt64 *statusRowid,
					     paf::ui::Widget *anchorWidget,
					     Lv2CommentActivityVisualizer *visualizer)
{
	ActivityStatusDialog *dialog = activityUiFindDialogByStatusRowid(statusRowid);
	if (dialog != NULL)
		dialog->OpenDeleteOrReportCommentMenu(anchorWidget, visualizer);
}

/* 0x81350B40 */
void activityUiInitFileList(paf::ui::FileList *fileList)
{
	if (s_fileListRegistry == NULL || fileList == NULL)
		return;
	s_fileListRegistry->lists.push_back(fileList);
	fileList->AddEventCallback(EVENT_WIDGET_DESTROY, &OnFileListDestroyed, NULL);
}

/* 0x81353DF2 */
void activityUiCloseDialog(int dialogId)
{
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->Close();
}

/* 0x8135B0BA */
void activityUiCreateStatusContexts(paf::common::SharedPtr<Live2AccessorProxy> *outProxy,
				    paf::common::SharedPtr<Live2Accessor> *outAccessor,
				    const ActivityDbQueryCondition *condition)
{
	Live2StatusAccessorFactory factory(5);
	factory.m_condition = *condition;
	paf::common::SharedPtr<Live2Accessor> accessor = factory.CreateAccessor();
	if (outAccessor != NULL)
		*outAccessor = accessor;
	if (outProxy != NULL && accessor.get() != NULL)
		*outProxy = accessor->m_proxy;
}

} /* namespace filelist */

/* -------------------------------------------------------------------------- */
/* Global extern entry points (declared in filelist_internal.h)               */
/* -------------------------------------------------------------------------- */

/* 0x81350AEA */
void activityUiDialogManagerInit(void)
{
	if (filelist::s_fileListRegistry == NULL)
		filelist::s_fileListRegistry = new filelist::ActivityFileListRegistry();
	if (filelist::s_dialogManager == NULL)
		filelist::s_dialogManager = new filelist::ActivityDialogManager();
}

/* 0x81350B0C */
void activityUiDialogManagerTerm(void)
{
	if (filelist::s_dialogManager != NULL) {
		delete filelist::s_dialogManager;
		filelist::s_dialogManager = NULL;
	}
	if (filelist::s_fileListRegistry != NULL) {
		delete filelist::s_fileListRegistry;
		filelist::s_fileListRegistry = NULL;
	}
}

/* 0x8135A13E */
void activityUiServiceInit(void)
{
	if (filelist::s_serviceManager != NULL)
		return;

	filelist::s_serviceManager = new filelist::ActivityServiceManager();
	filelist::s_serviceManager->syncHandle =
		paf::common::SharedPtr<filelist::ActivitySyncHandle>(
			new filelist::ActivitySyncHandle());

	paf::job::JobQueue::Option queueOpt;
	queueOpt.priority = 0x10000105;
	queueOpt.stackSize = 0x10000;
	queueOpt.numThreads = 1;
	filelist::s_serviceManager->postJobQueue =
		new paf::job::JobQueue("SceActivityPostJob", &queueOpt);
}

/* 0x8135A244 */
void activityUiServiceTerm(void)
{
	if (filelist::s_serviceManager == NULL)
		return;

	if (filelist::s_serviceManager->syncHandle.get() != NULL)
		SceActivityDb_5B44F019(filelist::s_serviceManager->syncHandle.get());

	if (filelist::s_serviceManager->postJobQueue != NULL) {
		filelist::s_serviceManager->postJobQueue->WaitEmpty();
		delete filelist::s_serviceManager->postJobQueue;
		filelist::s_serviceManager->postJobQueue = NULL;
	}

	delete filelist::s_serviceManager;
	filelist::s_serviceManager = NULL;
}

/* 0x8135B0AC */
paf::job::JobQueue *activityUiGetNetworkJobQueue(void)
{
	if (filelist::s_serviceManager == NULL)
		return NULL;
	return filelist::s_serviceManager->postJobQueue;
}

/* 0x8135B248 / 0x8135AAA6 */
void activityUiNotifyCacheInfoUpdated(void)
{
	if (filelist::s_dialogManager == NULL)
		return;

	for (paf::map<int, filelist::ActivityStatusDialog *>::iterator it =
		     filelist::s_dialogManager->dialogs.begin();
	     it != filelist::s_dialogManager->dialogs.end(); ++it) {
		if (it->second != NULL)
			it->second->OnCacheInfoUpdated();
	}
}

void activityUiInitFileList(paf::ui::FileList *fileList)
{
	filelist::activityUiInitFileList(fileList);
}

void activityUiCloseDialog(void)
{
	if (filelist::s_dialogManager == NULL)
		return;
	paf::vector<filelist::ActivityStatusDialog *> openDialogs;
	for (paf::map<int, filelist::ActivityStatusDialog *>::iterator it =
		     filelist::s_dialogManager->dialogs.begin();
	     it != filelist::s_dialogManager->dialogs.end(); ++it) {
		if (it->second != NULL)
			openDialogs.push_back(it->second);
	}
	for (size_t idx = 0; idx < openDialogs.size(); ++idx)
		openDialogs[idx]->Close();
}

void activityUiCloseDialogFast(void)
{
	if (filelist::s_dialogManager == NULL)
		return;
	paf::vector<filelist::ActivityStatusDialog *> openDialogs;
	for (paf::map<int, filelist::ActivityStatusDialog *>::iterator it =
		     filelist::s_dialogManager->dialogs.begin();
	     it != filelist::s_dialogManager->dialogs.end(); ++it) {
		if (it->second != NULL)
			openDialogs.push_back(it->second);
	}
	for (size_t idx = 0; idx < openDialogs.size(); ++idx)
		openDialogs[idx]->CloseWithReason(1);
}

int activityUiIsDialogActive(void)
{
	if (filelist::s_dialogManager == NULL)
		return 0;
	return (filelist::s_dialogManager->dialogs.size() == 0) ? 0 : 1;
}
