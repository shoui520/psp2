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
	WIDGET_STATUS_DIALOG_SEND_BTN    =   548899213,
	TEMPLATE_POST_COMMENT_OVERLAY    =  1498982009,
	WIDGET_POST_COMMENT_OVERLAY      =  -800904207,
	WIDGET_BUSY_FOOTER_TARGET        = -1361887340,

	TEMPLATE_GOODJOB_HEADER_BOX      =  -571882114,
	WIDGET_GOODJOB_HEADER_BOX        =   619555798,
	TEMPLATE_GOODJOB_VOTE_BTN        =  1423544944,
	WIDGET_GOODJOB_VOTE_BTN          =  -113409995,
	WIDGET_GOODJOB_VOTE_ICON         = -1907206792,
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
	STR_POPUP_REPORT                 =  1528059821,
	MSG_STATUS_NOT_FOUND             =   482989490
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
	EVENT_TEXTBOX_IME_OPEN     = 0x10000009,
	EVENT_TEXTBOX_ENTER        = 0x1000000B,
	EVENT_TEXTBOX_CHANGED      = 0x1000000C,
	TIMER_ID_DIALOG_CLIP       = 0x0123,
	TIMER_ID_POST_COMMENT_ANIM = 0x0000,
	ANIM_CHANNEL_COLOR         = 0x00010001,
	ANIM_CHANNEL_ALPHA         = 0x00010002,
	ANIM_CHANNEL_META_ALPHA    = 0x00010003,
	ANIM_CHANNEL_SIZE          = 0x00010004,
	ANIM_CHANNEL_POS           = 0x00010000,
	OBJ_ANIM_COLOR             = 0x00020300
};

enum {
	ACTIVITY_ERR_NOT_FOUND = (int)0x80558A65,
	ACTIVITY_ERR_SYS_UPDATE = (int)0x80550412,
	ACTIVITY_ERR_POST_CANCELED = (int)0x80101048,
	ACTIVITY_MAX_PENDING_JOBS = 10,
	ACTIVITY_MAX_COMMENTS = 20,
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
	ActivityDialogManager();
	~ActivityDialogManager();

	void UnregisterDialog(int dialogId);
	ActivityStatusDialog *FindById(int dialogId);
	ActivityStatusDialog *FindByStatusRowid(const SceInt64 *statusRowid);
	void NotifyCacheInfoUpdated();

	paf::map<int, ActivityStatusDialog *> dialogs;
	int nextDialogId;
};

/* 0x8135A10C */
ActivityDialogManager::ActivityDialogManager()
	: dialogs(),
	  nextDialogId(0)
{
}

ActivityDialogManager::~ActivityDialogManager()
{
}

/* 0x81352888 */
void ActivityDialogManager::UnregisterDialog(int dialogId)
{
	paf::map<int, ActivityStatusDialog *>::iterator it = dialogs.find(dialogId);
	if (it != dialogs.end())
		dialogs.erase(it);
}

/* 0x81353DB4 */
ActivityStatusDialog *ActivityDialogManager::FindById(int dialogId)
{
	paf::map<int, ActivityStatusDialog *>::iterator it = dialogs.find(dialogId);
	if (it == dialogs.end())
		return NULL;
	return it->second;
}

/* 0x813564A2 */
ActivityStatusDialog *ActivityDialogManager::FindByStatusRowid(const SceInt64 *statusRowid)
{
	if (statusRowid == NULL)
		return NULL;

	for (paf::map<int, ActivityStatusDialog *>::iterator it = dialogs.begin();
	     it != dialogs.end(); ++it) {
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

/* 0x8135AAA6 */
void ActivityDialogManager::NotifyCacheInfoUpdated()
{
	for (paf::map<int, ActivityStatusDialog *>::iterator it = dialogs.begin();
	     it != dialogs.end(); ++it) {
		if (it->second != NULL)
			it->second->OnCacheInfoUpdated();
	}
}

struct ActivityServiceManager {
	ActivityServiceManager();
	~ActivityServiceManager();

	paf::common::SharedPtr<ActivitySyncHandle> syncHandle;
	paf::job::JobQueue *postJobQueue;
	paf::vector<SceInt64> votingRowids;
	paf::vector<SceInt64> cancelingVoteRowids;
	paf::vector<SceInt64> postingCommentRowids;
	paf::vector<SceInt64> deletingCommentRowids;
	paf::vector<SceInt64> deletingStatusRowids;
};

/* 0x8135A01E */
ActivityServiceManager::ActivityServiceManager()
	: syncHandle(new ActivitySyncHandle()),
	  postJobQueue(NULL),
	  votingRowids(),
	  cancelingVoteRowids(),
	  postingCommentRowids(),
	  deletingCommentRowids(),
	  deletingStatusRowids()
{
	paf::job::JobQueue::Option queueOpt;
	int priority = (paf::Framework::Instance()->GetMode() == 1)
			       ? 0x10000105
			       : 0x100000FB;
	queueOpt.priority = priority;
	queueOpt.stackSize = 0x3000;
	queueOpt.numThreads = 1;
	postJobQueue = new paf::job::JobQueue("SceActivityPostJob", &queueOpt);
}

/* 0x8135A17A */
ActivityServiceManager::~ActivityServiceManager()
{
	if (syncHandle.get() != NULL)
		SceActivityDb_5B44F019(syncHandle.get());
	if (postJobQueue != NULL) {
		postJobQueue->CancelAllItems();
		delete postJobQueue;
	}
}

ActivityFileListRegistry *s_fileListRegistry = NULL;
ActivityDialogManager *s_dialogManager = NULL;
ActivityServiceManager *s_serviceManager = NULL;
paf::thread::RMutex s_detailSyncMutex("SceActivityDetailSync", 0);

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

/* 0x81350C36 */
paf::ui::Widget *FindStatusCardWidgetByRowid(const SceInt64 *statusRowid)
{
	if (statusRowid == NULL)
		return NULL;
	Lv1StatusActivityVisualizer *vis = FindStatusVisualizerByRowid(*statusRowid);
	if (vis != NULL)
		return vis->GetListItem();
	return NULL;
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

/* 0x814449C0: not-found error handler (closes dialog on dismiss) */
class StatusNotFoundErrorHandler : public CommonGuiDialogHandler {
public:
	explicit StatusNotFoundErrorHandler(SceInt64 statusRowid)
		: m_statusRowid(statusRowid)
	{
	}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 2) {
			if (s_dialogManager == NULL) {
				SceCommonGuiDialog_BC311DC0(dialogId);
				return;
			}
			ActivityStatusDialog *dialog =
				s_dialogManager->FindByStatusRowid(&m_statusRowid);
			if (dialog == NULL) {
				SceCommonGuiDialog_BC311DC0(dialogId);
				return;
			}
			if (dialogId >= 0) {
				SceCommonGuiDialog_BC311DC0(dialogId);
				if (dialog->m_commonDialogId == dialogId)
					dialog->m_commonDialogId = -1;
			}
			dialog->Close();
		} else if (button == 0 && s_dialogManager != NULL) {
			ActivityStatusDialog *dialog =
				s_dialogManager->FindByStatusRowid(&m_statusRowid);
			if (dialog != NULL) {
				if (dialogId >= 0) {
					SceCommonGuiDialog_BC311DC0(dialogId);
					if (dialog->m_commonDialogId == dialogId)
						dialog->m_commonDialogId = -1;
				}
				dialog->Close();
			}
		}
	}

	SceInt64 m_statusRowid;
};

/* 0x81444A48: error dialog handler for ActivityStatusDialog */
class StatusDialogErrorHandler : public CommonGuiDialogHandler {
public:
	explicit StatusDialogErrorHandler(SceInt64 statusRowid)
		: m_statusRowid(statusRowid)
	{
	}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)button;
		ActivityStatusDialog *dialog = activityUiFindDialogByStatusRowid(&m_statusRowid);
		if (dialog != NULL && dialog->m_commonDialogId == dialogId)
			dialog->m_commonDialogId = -1;
	}

	SceInt64 m_statusRowid;
};

/* 0x81356590 */
int ShowStatusNotFoundErrorDialog(const SceInt64 *statusRowid)
{
	if (statusRowid == NULL)
		return -1;

	CommonGuiMessageDialogParam param;
	param.messageId = MSG_STATUS_NOT_FOUND;
	param.type = ACTIVITY_ERR_NOT_FOUND;
	param.priority = activityUiGetDialogPriority();
	param.handler = new StatusNotFoundErrorHandler(*statusRowid);
	int dialogId = SceCommonGuiDialog_20B141F1(&param);
	if (s_dialogManager != NULL) {
		ActivityStatusDialog *dialog = s_dialogManager->FindByStatusRowid(statusRowid);
		if (dialog != NULL && dialogId >= 0 && dialog->m_commonDialogId != dialogId) {
			if (dialog->m_commonDialogId >= 0)
				SceCommonGuiDialog_BC311DC0(dialog->m_commonDialogId);
			dialog->m_commonDialogId = dialogId;
		}
	}
	return dialogId;
}

/* 0x813566FC */
int ShowStatusDialogError(const SceInt64 *statusRowid, int errorCode, bool suppressDialog)
{
	if (errorCode == 0 || s_dialogManager == NULL || statusRowid == NULL)
		return -1;

	ActivityStatusDialog *dialog = s_dialogManager->FindByStatusRowid(statusRowid);
	if (dialog == NULL)
		return -1;

	if (errorCode == ACTIVITY_ERR_NOT_FOUND)
		return ShowStatusNotFoundErrorDialog(statusRowid);

	if (errorCode == ACTIVITY_ERR_SYS_UPDATE && dialog->HandleSysUpdateError(errorCode))
		return -1;

	if (suppressDialog)
		return -1;

	CommonGuiMessageDialogParam param;
	param.type = errorCode;
	param.priority = activityUiGetDialogPriority();
	param.handler = new StatusDialogErrorHandler(*statusRowid);
	int dialogId = SceCommonGuiDialog_20B141F1(&param);
	if (dialogId >= 0 && dialog->m_commonDialogId != dialogId) {
		if (dialog->m_commonDialogId >= 0)
			SceCommonGuiDialog_BC311DC0(dialog->m_commonDialogId);
		dialog->m_commonDialogId = dialogId;
	}
	return dialogId;
}

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
		       const paf::string &ownerId, int dialogId, int queryType);
	virtual ~GetVoteInfoJob();
	virtual int32_t Run();
	virtual void Finish(int32_t err);

	SceInt64 m_statusRowid;
	paf::string m_ownerId;
	paf::string m_titleId;
	int m_dialogId;
	int m_queryType;
	ActivityDbGoodJob m_goodJob;
};

/* 0x81355D50 */
GetVoteInfoJob::GetVoteInfoJob(SceInt64 statusRowid, const paf::string &titleId,
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

/* 0x8135AB08 */
GetVoteInfoJob::~GetVoteInfoJob()
{
}

/* 0x8135ABC4 */
int32_t GetVoteInfoJob::Run()
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

/* 0x8135AC9E */
void GetVoteInfoJob::Finish(int32_t err)
{
	paf::common::SlotJobItem::Finish(err);
	if (IsCanceled())
		return;

	if (s_dialogManager != NULL) {
		ActivityStatusDialog *dialog = s_dialogManager->FindById(m_dialogId);
		if (dialog != NULL && err == 0) {
			dialog->m_voteInfo = m_goodJob;
			dialog->UpdateGoodJobArea(m_goodJob.numOfVotes, true);
		}
	}
}

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
/* ActivityStatusDialog static callbacks & layouters                          */
/* -------------------------------------------------------------------------- */

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

/* 0x81359EAA */
int32_t DialogUnreadBadgeLayoutCallback(paf::ui::Layouter *layouter, paf::ui::Widget *widget,
					void *firstData, void *secondData, void *thirdData)
{
	(void)layouter;
	(void)firstData;
	(void)secondData;
	(void)thirdData;
	paf::graph::DrawObj *unreadObj = widget->GetDrawObj(1);
	if (unreadObj != NULL) {
		const PafVec4 *size = widget->GetSize(0);
		PafVec3 nextPos(size->extract_x() * -0.5f + 25.0f,
				size->extract_y() * 0.5f - 25.0f,
				unreadObj->m_pos.extract_z());
		unreadObj->SetPos(nextPos);
	}
	return 0;
}

/* 0x81359C4E */
int32_t CommentEditLayoutCallback(paf::ui::Layouter *layouter, paf::ui::Widget *widget,
				  void *firstData, void *secondData, void *thirdData)
{
	(void)firstData;
	(void)secondData;
	(void)thirdData;
	if (!layouter->IsDirty(0xFF))
		return 0;

	paf::ui::Text *textWidget = static_cast<paf::ui::Text *>(widget);
	paf::ui::Widget *commentBar = widget->m_parent;
	paf::ui::Widget *dialogRoot = commentBar->m_parent;

	PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
	PafVec4 curBarPos = commentBar->GetPos(0, NULL, zeroOffset);
	PafVec4 nextBarPos = commentBar->GetPos(0, NULL, zeroOffset);
	PafVec4 textPos = widget->GetPos(0, NULL, zeroOffset);
	(void)widget->GetSize(0);

	float baseY = -135.0f;
	if (dialogRoot != NULL) {
		paf::ui::Widget *listWidget =
			dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_LIST), 0);
		if (listWidget != NULL) {
			PafVec4 listPos = listWidget->GetPos(0, NULL, zeroOffset);
			const PafVec4 *listSize = listWidget->GetSize(0);
			baseY = (listPos.extract_y() - listSize->extract_y()) + 3.0f + 305.0f;
		}
	}

	if (commentBar->FindAnim(ANIM_CHANNEL_POS) == NULL) {
		uint32_t lineCount = 0;
		textWidget->GetLineCount(lineCount);
		if (lineCount != 0) {
			float linePitch = 0.0f;
			textWidget->GetLayoutAttribute(paf::graph::TextLayoutAttribute_LinePitch,
						       linePitch);
			float targetY = (lineCount <= 1)
						? baseY
						: (baseY + static_cast<float>(lineCount - 1) * linePitch);
			nextBarPos.set_y(targetY);
			if (curBarPos.extract_y() != nextBarPos.extract_y()) {
				commentBar->SetPos(nextBarPos, NULL, ANIM_CHANNEL_POS, 0,
						   0.0f, NULL, NULL);
				textPos.set_y(textPos.extract_y() -
					      (nextBarPos.extract_y() - curBarPos.extract_y()));
				widget->SetPos(textPos, NULL, ANIM_CHANNEL_POS, 0,
					       0.0f, NULL, NULL);
			}
		}
	}
	return 0;
}

/* 0x81358DC2 */
void OnDetailSyncCallback(int errorCode, int unusedArg, int dialogId)
{
	(void)unusedArg;
	s_detailSyncMutex.Lock();
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL) {
		dialog->m_syncingDetail = false;
		activityUiInvokeNetRelease();
		if (errorCode != 0 && dialog->m_record.get() != NULL &&
		    dialog->m_record->GetRecordKind() == LIVE2_RECORD_STATUS) {
			Live2StatusRecord *statusRec =
				static_cast<Live2StatusRecord *>(dialog->m_record.get());
			ShowStatusDialogError(&statusRec->m_status.rowid, errorCode, false);
		}
	}
	s_detailSyncMutex.Unlock();
}

/* 0x81358E0C */
void OnCommentCountCallback(int count, int dialogId)
{
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL) {
		if (!dialog->m_postingComment)
			dialog->m_busyFooter.HideBusyHeader();
		dialog->OnCommentCountChanged(count);
		dialog->RequestVoteInfo();
	}
}

/* 0x81358E42 */
void OnBackButtonEventCallback(int eventId, paf::ui::Handler *handler,
			       paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnBackButtonDecide();
}

/* 0x81358E60 */
void OnOptionButtonEventCallback(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OpenStatusOptionMenu(static_cast<paf::ui::Widget *>(handler));
}

/* 0x81358E82 */
void OnSendCommentButtonCallback(int eventId, paf::ui::Handler *handler,
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

/* 0x81358ECA */
void OnCommentBoxBeginEditCallback(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	paf::ui::Text *textBox = static_cast<paf::ui::Text *>(handler);
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	paf::ui::Widget *commentBar = textBox->m_parent;

	float editBaseY = (dialog != NULL) ? dialog->GetEditCommentBarY() : -135.0f;

	textBox->SetLayoutAttribute(paf::graph::TextLayoutAttribute_MaxLineCount, 5);
	uint32_t currentLineCount = 0;
	textBox->GetLineCount(currentLineCount);

	PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
	PafVec4 barPos = commentBar->GetPos(0, NULL, zeroOffset);
	PafVec4 barSize = *commentBar->GetSize(0);
	barSize.set_y(384.0f);
	commentBar->SetSize(barSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);

	uint32_t savedLineCount =
		static_cast<uint32_t>(reinterpret_cast<uintptr_t>(textBox->m_user_data[0]));
	float linePitch = 0.0f;
	textBox->GetLayoutAttribute(paf::graph::TextLayoutAttribute_LinePitch, linePitch);

	float targetBarY = (savedLineCount <= 1)
				   ? editBaseY
				   : (editBaseY + static_cast<float>(savedLineCount - 1) * linePitch);
	barPos.set_y(targetBarY);
	commentBar->SetPos(barPos, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
			   ANIM_CHANNEL_POS, 2, 0.0f, NULL, NULL);

	PafVec4 textPos = textBox->GetPos(0, NULL, zeroOffset);
	textPos.set_y(textPos.extract_y() - (barPos.extract_y() - editBaseY));
	textBox->SetPos(textPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);

	textPos.set_x(-404.0f);
	textBox->SetPos(textPos, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
			ANIM_CHANNEL_POS, 2, 0.0f, NULL, NULL);

	PafVec4 textSize = *textBox->GetSize(0);
	textSize.set_x(640.0f);
	if (savedLineCount > 1)
		textSize.set_y(linePitch * static_cast<float>(savedLineCount) + 16.0f);
	textBox->SetSize(textSize, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
			 ANIM_CHANNEL_SIZE, 2, 0.0f, NULL, NULL);

	paf::ui::Widget *sendBtn =
		commentBar->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0);
	if (sendBtn != NULL) {
		PafVec4 sendPos = sendBtn->GetPos(0, NULL, zeroOffset);
		sendPos.set_x(404.0f);
		sendBtn->SetPos(sendPos, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
				ANIM_CHANNEL_POS, 2, 0.0f, NULL, NULL);
	}

	if (dialog != NULL && dialog->m_listView != NULL) {
		PafVec2 viewFocus = dialog->m_listView->GetViewFocus();
		viewFocus.set_y(viewFocus.extract_y() - 304.0f);
		dialog->m_listView->SetViewFocus(
			viewFocus, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT), false, false);
		PafVec2 pad(0.0f, 304.0f);
		dialog->m_listView->ScePafWidget_5B94F2B4(
			pad, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT));
	}

	textBox->SetLayouter(new paf::ui::Layouter(&CommentEditLayoutCallback, NULL));
}

/* 0x813594BC */
void OnCommentBoxEndEditCallback(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	paf::ui::Text *textBox = static_cast<paf::ui::Text *>(handler);
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	paf::ui::Widget *commentBar = textBox->m_parent;

	PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
	PafVec4 barPos = commentBar->GetPos(0, NULL, zeroOffset);
	PafVec4 barSize = *commentBar->GetSize(0);
	if (dialog != NULL)
		(void)dialog->GetEditCommentBarY();

	textBox->SetLayouter(new paf::ui::Layouter(NULL, NULL));

	PafVec4 textPos = textBox->GetPos(0, NULL, zeroOffset);
	textPos.set_y(-57.0f);
	textBox->SetPos(textPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);

	bool wideBox = (dialog != NULL && dialog->UseWideCommentBox());
	textPos.set_x(wideBox ? -377.0f : -318.0f);

	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	bool wasFocusShown = ScePafWidget_529FB877(env, -1);
	paf::Framework::Instance()->GetEnvironmentInstance()->HideFocus(-1);
	textBox->SetFocus(0);
	textBox->SetPos(textPos, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
			ANIM_CHANNEL_POS, 2, 0.0f, &OnShowFocusAfterAnim,
			reinterpret_cast<void *>(static_cast<uintptr_t>(wasFocusShown ? 1 : 0)));

	uint32_t lineCount = 0;
	textBox->GetLineCount(lineCount);
	textBox->m_user_data[0] = reinterpret_cast<void *>(static_cast<uintptr_t>(lineCount));
	textBox->SetLayoutAttribute(paf::graph::TextLayoutAttribute_MaxLineCount, 1);

	PafVec4 textSize = *textBox->GetSize(0);
	textSize.set_x((dialog != NULL && dialog->UseWideCommentBox()) ? 576.0f : 448.0f);
	float linePitch = 0.0f;
	textBox->GetLayoutAttribute(paf::graph::TextLayoutAttribute_LinePitch, linePitch);
	textSize.set_y(linePitch + 16.0f);
	textBox->SetSize(textSize, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
			 ANIM_CHANNEL_SIZE, 2, 0.0f, NULL, NULL);

	paf::ui::Widget *sendBtn =
		commentBar->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0);
	if (sendBtn != NULL) {
		PafVec4 sendPos = sendBtn->GetPos(0, NULL, zeroOffset);
		sendPos.set_x((dialog != NULL && dialog->UseWideCommentBox()) ? 373.0f : 312.0f);
		sendBtn->SetPos(sendPos, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
				ANIM_CHANNEL_POS, 2, 0.0f, NULL, NULL);
	}

	if (dialog != NULL) {
		float collapsedY = (-13.0f - dialog->GetListViewHeight()) + 3.0f;
		barPos.set_y(collapsedY);
	}
	commentBar->SetPos(barPos, new paf::Timer(0.1f, paf::Timer::FUNC_EXPONENT),
			   ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);

	barSize.set_y(66.0f);
	commentBar->SetSize(barSize, new paf::Timer(0.1f, paf::Timer::FUNC_EXPONENT),
			    ANIM_CHANNEL_SIZE, 2, 200.0f, NULL, NULL);

	if (dialog != NULL && dialog->m_listView != NULL) {
		const PafVec4 *listSize = dialog->m_listView->GetSize(0);
		if (dialog->m_listView->m_view_box_size.extract_y() != listSize->extract_y() &&
		    dialog->m_busyFooter.m_state != 1) {
			PafVec2 viewFocus = dialog->m_listView->GetViewFocus();
			viewFocus.set_y(viewFocus.extract_y() + 304.0f);
			dialog->m_listView->SetViewFocus(
				viewFocus, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT),
				false, false);
		}
		PafVec2 zeroPad(0.0f, 0.0f);
		dialog->m_listView->ScePafWidget_5B94F2B4(
			zeroPad, new paf::Timer(200.0f, paf::Timer::FUNC_EXPONENT));
	}
}

/* 0x81359BF8 */
void OnCommentBoxTextChangedCallback(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	paf::ui::Text *textBox = static_cast<paf::ui::Text *>(handler);
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL) {
		uint32_t textLen = 0;
		textBox->GetLength(textLen);
		if (textLen != 0)
			dialog->EnableSendButtonIfReady();
		else
			dialog->DisableSendButton();
	}
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
void OnHeaderTextSizeReadyCallback(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	paf::ui::Widget *textWidget = static_cast<paf::ui::Widget *>(handler);
	paf::graph::DrawObj *bodyObj = textWidget->GetDrawObj(0);
	paf::graph::DrawObj *timeObj = textWidget->GetDrawObj(1);
	if (bodyObj == NULL || timeObj == NULL)
		return;
	if (!bodyObj->m_flags.m_visible && bodyObj->m_flags.m_prepare)
		return;

	float origY = bodyObj->m_pos.extract_y();
	float textHeight = bodyObj->m_size.extract_y();
	PafVec3 nextPos(bodyObj->m_pos.extract_x(), origY - textHeight * 0.5f,
			bodyObj->m_pos.extract_z());
	bodyObj->SetPos(nextPos);
	bodyObj->SetVisible(true);
	timeObj->SetVisible(true);

	float minHeight = (textHeight < 72.0f) ? 72.0f : textHeight;
	float goodJobY = (origY - minHeight) - 24.0f;

	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->SetupGoodJobContainer(textWidget->m_parent, goodJobY);
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

	paf::graph::DrawObj *iconObj = btn->GetDrawObj(0);
	paf::graph::TextObj *labelObj = static_cast<paf::graph::TextObj *>(btn->GetDrawObj(1));
	if (iconObj == NULL || labelObj == NULL || labelObj->m_flags.m_prepare)
		return;

	float totalHeight = iconObj->m_size.extract_y() + labelObj->m_size.extract_y();
	uint32_t lineCount = 0;
	labelObj->GetLineCount(lineCount);
	float gap = (lineCount <= 1) ? 10.0f : 14.0f;
	float span = totalHeight - gap;

	float iconY = (span * 0.5f - iconObj->m_size.extract_y() * 0.5f) - 3.0f;
	float labelY = (labelObj->m_size.extract_y() * 0.5f - span * 0.5f) - 3.0f;
	iconObj->SetPos(PafVec3(iconObj->m_pos.extract_x(), iconY, iconObj->m_pos.extract_z()));
	labelObj->SetPos(PafVec3(labelObj->m_pos.extract_x(), labelY, labelObj->m_pos.extract_z()));
	btn->Show(0xff, NULL, NULL, 0.0f);
}

/* 0x81359F40 */
void OnStatusOptionMenuDeleteCallback(int index, int value, void *userdata)
{
	(void)index;
	(void)value;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL && dialog->m_record.get() != NULL &&
	    dialog->m_record->GetRecordKind() == LIVE2_RECORD_STATUS)
		dialog->ShowDeleteStatusConfirmDialog();
}

/* 0x81359F7E */
void OnStatusOptionMenuCloseCallback(int result, void *userdata)
{
	(void)result;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL) {
		dialog->m_popupMenuOpen = false;
		dialog->OnPopupMenuClosed();
	}
}

/* 0x81359FA0 */
void OnDeleteCommentMenuSelect(int index, int value, void *userdata)
{
	(void)index;
	(void)value;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL)
		dialog->OnDeleteCommentMenuSelected();
}

/* 0x81359FBC */
void OnDeleteOrReportCommentMenuSelect(int index, int value, void *userdata)
{
	(void)value;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog == NULL)
		return;

	if (index == 0)
		dialog->OnDeleteCommentMenuSelected();
	else if (index == 1)
		dialog->OnGriefReportMenuSelected();
}

/* 0x81359FE6 */
void OnCommentPopupMenuClosedCallback(int result, void *userdata)
{
	(void)result;
	int dialogId = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
	ActivityStatusDialog *dialog = activityUiFindDialogById(dialogId);
	if (dialog != NULL) {
		dialog->m_popupMenuOpen = false;
		dialog->OnPopupMenuClosed();
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
		dialog->OnDialogOpened();
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
		dialog->UpdatePostCommentAnimation();
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
	paf::ui::Widget *panel = static_cast<paf::ui::Widget *>(handler);
	if (panel == NULL)
		return;

	unsigned int packedCounts = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(userdata));
	unsigned int commentCount = packedCounts >> 16;
	unsigned int voteCount = packedCounts & 0xFFFFU;

	paf::graph::DrawObj *commentTextRaw = panel->GetDrawObj(STATUS_DRAWOBJ_COMMENT_COUNT);
	paf::graph::TextObj *commentText =
		(commentTextRaw != NULL && commentTextRaw->IsInherit(paf::graph::TextObj::DrawObjType()))
			? static_cast<paf::graph::TextObj *>(commentTextRaw)
			: NULL;
	paf::graph::DrawObj *voteTextRaw = panel->GetDrawObj(STATUS_DRAWOBJ_VOTE_COUNT);
	paf::graph::TextObj *voteText =
		(voteTextRaw != NULL && voteTextRaw->IsInherit(paf::graph::TextObj::DrawObjType()))
			? static_cast<paf::graph::TextObj *>(voteTextRaw)
			: NULL;

	if (commentCount != 0 && commentText != NULL &&
	    !commentText->m_flags.m_visible && commentText->m_flags.m_prepare)
		return;
	if (voteCount != 0 && voteText != NULL &&
	    !voteText->m_flags.m_visible && voteText->m_flags.m_prepare)
		return;

	if (commentCount != 0) {
		float rightEdge = panel->GetSize(0)->extract_x() * 0.5f - 10.0f;
		paf::graph::DrawObj *commentIconRaw = panel->GetDrawObj(STATUS_DRAWOBJ_COMMENT_ICON);
		paf::graph::PlaneObj *commentIcon =
			(commentIconRaw != NULL &&
			 commentIconRaw->IsInherit(paf::graph::PlaneObj::DrawObjType()))
				? static_cast<paf::graph::PlaneObj *>(commentIconRaw)
				: NULL;
		paf::graph::DrawObj *voteIconRaw = panel->GetDrawObj(STATUS_DRAWOBJ_VOTE_ICON);
		paf::graph::PlaneObj *voteIcon =
			(voteIconRaw != NULL &&
			 voteIconRaw->IsInherit(paf::graph::PlaneObj::DrawObjType()))
				? static_cast<paf::graph::PlaneObj *>(voteIconRaw)
				: NULL;

		if (commentText != NULL && commentIcon != NULL) {
			float badgeWidth = commentText->m_size.extract_x() + 32.0f;
			if (badgeWidth < 110.0f)
				badgeWidth = 110.0f;
			commentIcon->SetSize(PafVec3(badgeWidth, commentIcon->m_size.extract_y(),
						     0.0f));
			float iconWidth = commentIcon->m_size.extract_x();
			float iconX = rightEdge - iconWidth * 0.5f;
			commentIcon->SetPos(PafVec3(iconX, commentIcon->m_pos.extract_y(), 0.0f));
			commentText->SetPos(PafVec3(iconX + 4.0f,
						    commentText->m_pos.extract_y(), 0.0f));
			commentIcon->SetVisible(true);
			commentText->SetVisible(true);
			rightEdge -= (iconWidth - 5.0f);
		}

		if (voteCount != 0 && voteIcon != NULL && voteText != NULL) {
			float voteIconX = rightEdge - voteIcon->m_size.extract_x() * 0.5f;
			voteIcon->SetPos(PafVec3(voteIconX, voteIcon->m_pos.extract_y(), 0.0f));
			voteText->SetPos(PafVec3(voteIconX + 11.0f,
						 voteText->m_pos.extract_y(), 0.0f));
			voteText->SetVisible(true);
			voteIcon->SetVisible(true);
		}
	} else if (voteCount != 0) {
		float rightEdge = panel->GetSize(0)->extract_x() * 0.5f - 10.0f;
		paf::graph::DrawObj *voteIconRaw = panel->GetDrawObj(STATUS_DRAWOBJ_VOTE_ICON);
		paf::graph::PlaneObj *voteIcon =
			(voteIconRaw != NULL &&
			 voteIconRaw->IsInherit(paf::graph::PlaneObj::DrawObjType()))
				? static_cast<paf::graph::PlaneObj *>(voteIconRaw)
				: NULL;
		if (voteIcon != NULL && voteText != NULL) {
			float voteIconX = rightEdge - voteIcon->m_size.extract_x() * 0.5f;
			voteIcon->SetPos(PafVec3(voteIconX, voteIcon->m_pos.extract_y(), 0.0f));
			voteText->SetPos(PafVec3(voteIconX + 11.0f,
						 voteText->m_pos.extract_y(), 0.0f));
			voteText->SetVisible(true);
			voteIcon->SetVisible(true);
		}
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
			commentIcon->SetVisible(false);
			commentText->SetVisible(false);
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
			voteIcon->SetVisible(false);
			voteText->SetVisible(false);
		} else {
			voteIcon->SetVisible(false);
			voteText->SetVisible(false);
		}
	}

	uintptr_t packedCounts =
		(static_cast<uintptr_t>(commentCount & 0xFFFF) << 16) |
		static_cast<uintptr_t>(voteCount & 0xFFFF);
	if (panel->m_prepare_count != 0) {
		panel->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED,
					&Lv1StatusActivityVisualizer::LayoutCountsCallback,
					reinterpret_cast<void *>(packedCounts));
	} else {
		LayoutCountsCallback(0, panel, NULL, reinterpret_cast<void *>(packedCounts));
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
	UpdatePanelFocusShape(panel);
}

/* 0x8134F9F2 */
void Lv1StatusActivityVisualizer::UpdatePanelFocusShape(paf::ui::Widget *panel)
{
	if (panel == NULL)
		return;
	paf::ui::FocusShape shape;
	const PafVec4 *size = panel->GetSize(0);
	shape.m_size = PafVec2(size->extract_x() - 16.0f, size->extract_y() - 18.0f);
	ScePafWidget_13B246E0(panel, &shape);
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
		cb.func = reinterpret_cast<void (*)(int, int)>(&OnDetailSyncCallback);
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
	SceActivityDb_5B44F019(&m_syncHandle);
	if (m_syncingDetail) {
		activityUiInvokeNetRelease();
		m_syncingDetail = false;
	}
	if (m_commonDialogId >= 0) {
		int dlgId = m_commonDialogId;
		SceCommonGuiDialog_BC311DC0(dlgId);
		if (m_commonDialogId == dlgId)
			m_commonDialogId = -1;
	}
	if (m_popupMenuOpen) {
		shellPopupListClose(0);
		m_popupMenuOpen = false;
	}
	if (s_dialogManager != NULL)
		s_dialogManager->UnregisterDialog(m_dialogId);
	if (m_dialogRoot != NULL) {
		m_dialogRoot->m_name = paf::IDParam("");
		m_dialogRoot->DestroyWidget();
	}
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
		void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
		if (optionBtn->m_prepare_count != 0) {
			optionBtn->AddEventCallback(EVENT_WIDGET_SIZE_CHANGED,
						    &OnHeaderTextSizeReadyCallback, idArg);
		} else {
			OnHeaderTextSizeReadyCallback(0, optionBtn, NULL, idArg);
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
		StartPostCommentAnimation(false);
}

/* 0x8135134A */
void ActivityStatusDialog::SyncPostAnimOverlayToCommentBox()
{
	if (m_dialogRoot == NULL || m_postAnimWidget == NULL)
		return;

	paf::ui::Widget *commentBox =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0);
	if (commentBox == NULL)
		return;

	m_postAnimWidget->SetSize(*commentBox->GetSize(0), NULL, ANIM_CHANNEL_SIZE, 0,
				  0.0f, NULL, NULL);
	PafVec4 rootOrigin = m_dialogRoot->GetOriginWorldMatrix().w();
	PafVec4 boxWorld = commentBox->GetWorldMatrix().w();
	PafVec4 relPos(boxWorld.extract_x() - rootOrigin.extract_x(),
		       boxWorld.extract_y() - rootOrigin.extract_y(), 0.0f, 0.0f);
	m_postAnimWidget->SetPos(relPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
}

/* 0x81351462 */
void ActivityStatusDialog::CreatePostCommentOverlay()
{
	if (m_postAnimWidget != NULL || m_dialogRoot == NULL)
		return;

	paf::Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(m_dialogRoot, paf::IDParam(TEMPLATE_POST_COMMENT_OVERLAY),
			       openParam);
	m_postAnimWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_POST_COMMENT_OVERLAY), 0);
	SyncPostAnimOverlayToCommentBox();
	m_postAnimTime = 0.0f;
	if (m_postAnimWidget != NULL) {
		void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
		m_postAnimWidget->SetIntervalEvent(TIMER_ID_POST_COMMENT_ANIM, 0.0f,
						   &OnPostCommentAnimTimer, idArg);
		m_postAnimWidget->SetMetaAlpha(0.0f, NULL, ANIM_CHANNEL_META_ALPHA, 0,
					       0.0f, NULL, NULL);
	}
}

/* 0x813515E4 */
void ActivityStatusDialog::DisableCommentBox()
{
	if (m_dialogRoot == NULL)
		return;

	paf::ui::Widget *commentBox =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0);
	if (commentBox == NULL || m_inputDisabled)
		return;

	paf::ui::ButtonBase *sendBtn = static_cast<paf::ui::ButtonBase *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0));
	ScePafWidget_A96A0B4A(commentBox, 1);
	if (sendBtn != NULL) {
		PafVec4 dimColor(1.0f, 1.0f, 1.0f, 0.3f);
		if (sendBtn->m_disable_color != NULL)
			dimColor = *sendBtn->m_disable_color;
		commentBox->SetColor(dimColor, NULL, ANIM_CHANNEL_COLOR, 0, 0.0f, NULL, NULL);
	}
	m_inputDisabled = true;
}

/* 0x81351744 */
void ActivityStatusDialog::DisableSendButton()
{
	if (m_dialogRoot == NULL)
		return;

	paf::ui::ButtonBase *sendBtn = static_cast<paf::ui::ButtonBase *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0));
	(void)m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0);
	if (sendBtn != NULL && sendBtn->IsButtonEnabled())
		sendBtn->Disable(false);
}

/* 0x81352A7E */
void ActivityStatusDialog::UpdateHeaderTimeText(paf::graph::TextObj *timeText)
{
	if (timeText == NULL)
		return;

	paf::intrusive_ptr<Live2Record> rec = m_record;
	if (rec.get() == NULL || rec->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(rec.get());
	paf::wstring timeStr = BaseActivityVisualizer::FormatActivityTime(
		static_cast<SceUInt64>(statusRec->m_status.creationTime));
	if (ShouldShowTitleName()) {
		paf::wstring titleWide;
		paf::common::Utf8ToUtf16(statusRec->m_status.titleName.c_str(), &titleWide);
		timeStr = titleWide + L"  " + timeStr;
	}
	timeText->SetString(timeStr);
}

/* 0x81352C14 */
void ActivityStatusDialog::SetupControls()
{
	if (m_dialogRoot == NULL)
		return;

	void *idArg = reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));

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

	if (HasBackButton()) {
		paf::Plugin::TemplateOpenParam openParam;
		m_plugin->TemplateOpen(m_dialogRoot, GetBackButtonTemplateId(), openParam);
	}
	if (HasOptionButton()) {
		paf::Plugin::TemplateOpenParam openParam;
		m_plugin->TemplateOpen(m_dialogRoot, GetOptionButtonTemplateId(), openParam);
	}

	paf::ui::Widget *backBtn = m_dialogRoot->FindChild(GetBackButtonId(), 0);
	paf::ui::Widget *optionBtn = m_dialogRoot->FindChild(GetOptionButtonId(), 0);
	if (backBtn != NULL)
		backBtn->AddEventCallback(EVENT_BUTTON_DECIDE, &OnBackButtonEventCallback, idArg);
	if (optionBtn != NULL) {
		if (m_displayMode == ACTIVITY_DB_OWNER_SELF) {
			optionBtn->AddEventCallback(EVENT_BUTTON_DECIDE,
						    &OnOptionButtonEventCallback, idArg);
		} else {
			optionBtn->DestroyWidget();
		}
	}

	PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
	paf::ui::Widget *commentBox =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0);
	if (commentBox != NULL) {
		commentBox->AddEventCallback(EVENT_TEXTBOX_IME_OPEN,
					     &OnCommentBoxBeginEditCallback, idArg);
		commentBox->AddEventCallback(EVENT_TEXTBOX_EDIT_END,
					     &OnCommentBoxEndEditCallback, idArg);
		commentBox->AddEventCallback(EVENT_TEXTBOX_CHANGED,
					     &OnCommentBoxTextChangedCallback, idArg);
		commentBox->m_user_data[0] = reinterpret_cast<void *>(static_cast<uintptr_t>(1));

		PafVec4 boxPos = commentBox->GetPos(0, NULL, zeroOffset);
		PafVec4 boxSize = *commentBox->GetSize(0);
		if (UseWideCommentBox()) {
			boxPos.set_x(-377.0f);
			boxSize.set_x(576.0f);
		} else {
			boxPos.set_x(-318.0f);
			boxSize.set_x(448.0f);
		}
		commentBox->SetPos(boxPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
		commentBox->SetSize(boxSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);
	}

	paf::ui::Widget *sendBtn =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0);
	if (sendBtn != NULL) {
		sendBtn->AddEventCallback(EVENT_BUTTON_DECIDE,
					  &OnSendCommentButtonCallback, idArg);
		PafVec4 sendPos = sendBtn->GetPos(0, NULL, zeroOffset);
		sendPos.set_x(UseWideCommentBox() ? 373.0f : 312.0f);
		sendBtn->SetPos(sendPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
	}

	DisableSendButton();
}

/* 0x813531DA */
void ActivityStatusDialog::Open(paf::ui::ListItem *anchorItem)
{
	float scale = (anchorItem == NULL) ? 1.5f : 1.0f;
	static float s_openPosDuration = scale * 150.0f;
	static float s_openDoneDuration = scale * 150.0f;
	static float s_openFadeDuration = scale * 50.0f;
	static float s_openBarDuration = scale * 150.0f;
	static float s_openBtnDelay = s_openPosDuration;
	(void)s_openDoneDuration;

	SetupControls();

	paf::ui::Widget *headerWidget = NULL;
	if (m_listView != NULL) {
		headerWidget =
			m_listView->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_HEADER_ROOT), 0);
		if (headerWidget == NULL)
			headerWidget = m_listView->GetSegmentHeader(0);
	}
	if (headerWidget != NULL) {
		paf::ui::Widget *textWidget =
			headerWidget->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_OPTION_BTN), 0);
		if (textWidget != NULL) {
			paf::graph::TextObj *timeText =
				static_cast<paf::graph::TextObj *>(textWidget->GetDrawObj(1));
			UpdateHeaderTimeText(timeText);
			if (timeText != NULL)
				timeText->SetVisible(false);
		}
	}

	PafVec4 targetPos(0.0f, GetDialogCenterY(), 0.0f, 0.0f);
	PafVec4 targetSize(852.0f, GetDialogHeight(), 0.0f, 0.0f);
	(void)GetListViewHeight();

	paf::ui::Widget *bgWidget =
		(m_dialogRoot != NULL)
			? m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_BG), 0)
			: NULL;
	if (bgWidget != NULL) {
		if (IsUnread()) {
			bgWidget->SetLayouter(
				new paf::ui::Layouter(&DialogUnreadBadgeLayoutCallback, NULL));
		} else {
			paf::graph::DrawObj *unreadObj = bgWidget->GetDrawObj(1);
			if (unreadObj != NULL)
				unreadObj->SetVisible(false);
		}
	}

	if (m_dialogRoot != NULL) {
		PafVec4 startPos(0.0f, 0.0f, 0.0f, 0.0f);
		PafVec4 startSize(832.0f, 144.0f, 0.0f, 0.0f);
		paf::ui::Widget *parentWidget = m_dialogRoot->m_parent;
		if (anchorItem != NULL) {
			startPos = anchorItem->GetWorldMatrix().w();
			startSize = *anchorItem->GetSize(0);
		}
		m_anchorSize = startSize;

		if (ShouldAnimateOpen()) {
			m_dialogRoot->Hide(0xff, NULL, NULL, 0.0f);
			if (s_openFadeDuration > 0.0f) {
				m_dialogRoot->Show(0xff, new paf::Timer(s_openFadeDuration),
						   NULL, 0.0f);
			} else {
				m_dialogRoot->Show(0xff, NULL, NULL, 0.0f);
			}

			m_dialogRoot->SetSize(startSize, NULL, ANIM_CHANNEL_SIZE, 0,
					      0.0f, NULL, NULL);
			PafVec4 parentOrigin = parentWidget->GetOriginWorldMatrix().w();
			PafVec4 alignOffs = m_dialogRoot->GetAlignOffsetToCenter(0);
			PafVec4 initPos(startPos.extract_x() - parentOrigin.extract_x() -
						alignOffs.extract_x(),
					startPos.extract_y() - parentOrigin.extract_y() -
						alignOffs.extract_y(),
					0.0f, 0.0f);
			m_dialogRoot->SetPos(initPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
			m_dialogRoot->SetPos(targetPos,
					     new paf::Timer(s_openPosDuration,
							    paf::Timer::FUNC_QUAD_OUT),
					     ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);

			void *idArg =
				reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
			m_dialogRoot->SetSize(targetSize,
					      new paf::Timer(s_openPosDuration,
							     paf::Timer::FUNC_QUAD_OUT),
					      ANIM_CHANNEL_SIZE, 0, 0.0f,
					      &OnDialogOpenTransitionDone, idArg);

			ScePafWidget_A96A0B4A(m_dialogRoot, 1);
			if (m_listView != NULL)
				m_listView->m_clip_mode = 1;
			m_dialogRoot->SetIntervalEvent(TIMER_ID_DIALOG_CLIP, 0.0f,
						       &OnDialogClipTimer, NULL);
		} else {
			m_dialogRoot->SetPos(targetPos, NULL, ANIM_CHANNEL_POS, 0,
					     0.0f, NULL, NULL);
			m_dialogRoot->SetSize(targetSize, NULL, ANIM_CHANNEL_SIZE, 0,
					      0.0f, NULL, NULL);
		}
	}

	if (m_listView != NULL) {
		PafVec4 listPos(0.0f, -13.0f, 0.0f, 0.0f);
		m_listView->SetPos(listPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
		PafVec4 listSize(832.0f, GetListViewHeight(), 0.0f, 0.0f);
		m_listView->SetSize(listSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);
	}

	float commentBarY = (-13.0f - GetListViewHeight()) + 3.0f;
	PafVec4 commentBarPos(0.0f, commentBarY, 0.0f, 0.0f);
	paf::ui::Widget *panelWidget =
		(m_dialogRoot != NULL)
			? m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_PANEL), 0)
			: NULL;
	if (panelWidget != NULL) {
		if (m_displayMode == ACTIVITY_DB_OWNER_TITLE) {
			panelWidget->Hide(0xff, NULL, NULL, 0.0f);
		} else if (ShouldAnimateOpen()) {
			PafVec4 slideStart = commentBarPos;
			slideStart.set_y(commentBarY - panelWidget->GetSize(0)->extract_y());
			panelWidget->SetPos(slideStart, NULL, ANIM_CHANNEL_POS, 0,
					    0.0f, NULL, NULL);
			panelWidget->SetPos(commentBarPos,
					    new paf::Timer(s_openBarDuration,
							   paf::Timer::FUNC_QUAD_OUT),
					    ANIM_CHANNEL_SIZE, 0, s_openBtnDelay, NULL, NULL);
		} else {
			panelWidget->SetPos(commentBarPos, NULL, ANIM_CHANNEL_POS, 0,
					    0.0f, NULL, NULL);
		}
	}

	float btnY = commentBarY - 66.0f;
	PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
	if (HasBackButton() && m_dialogRoot != NULL) {
		paf::ui::Widget *backBtn = m_dialogRoot->FindChild(GetBackButtonId(), 0);
		if (backBtn != NULL) {
			if (ShouldAnimateOpen()) {
				ScePafWidget_44B22ACA(
					backBtn,
					static_cast<paf::common::transition::Type>(0xff),
					s_openBtnDelay, NULL, NULL);
			} else {
				backBtn->Show(0xff, NULL, NULL, 0.0f);
			}
			PafVec4 pos = backBtn->GetPos(0, NULL, zeroOffset);
			pos.set_y(btnY);
			backBtn->SetPos(pos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
		}
	}
	if (HasOptionButton() && m_dialogRoot != NULL) {
		paf::ui::Widget *optionBtn = m_dialogRoot->FindChild(GetOptionButtonId(), 0);
		if (optionBtn != NULL) {
			if (ShouldAnimateOpen()) {
				ScePafWidget_44B22ACA(
					optionBtn,
					static_cast<paf::common::transition::Type>(0xff),
					s_openBtnDelay, NULL, NULL);
			} else {
				optionBtn->Show(0xff, NULL, NULL, 0.0f);
			}
			PafVec4 pos = optionBtn->GetPos(0, NULL, zeroOffset);
			pos.set_y(btnY);
			optionBtn->SetPos(pos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
		}
	}

	if (!ShouldAnimateOpen())
		OnDialogOpened();
}

/* 0x81353E16 */
void ActivityStatusDialog::Close()
{
	OnDialogClosed();
	if (m_popupMenuOpen) {
		shellPopupListClose(0);
		m_popupMenuOpen = false;
	}

	if (!ShouldAnimateClose()) {
		ActivityStatusDialog *self = activityUiFindDialogById(m_dialogId);
		if (self != NULL)
			delete self;
		return;
	}

	PafVec4 targetPos(0.0f, 0.0f, 0.0f, 0.0f);
	PafVec4 targetSize = m_anchorSize;
	if (m_record.get() != NULL && m_record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		Live2StatusRecord *statusRec =
			static_cast<Live2StatusRecord *>(m_record.get());
		paf::ui::Widget *cardWidget =
			FindStatusCardWidgetByRowid(&statusRec->m_status.rowid);
		if (cardWidget != NULL) {
			targetSize = *cardWidget->GetSize(0);
			targetPos = cardWidget->GetWorldMatrix().w();
		}
	}

	if (m_dialogRoot != NULL) {
		ScePafWidget_A96A0B4A(m_dialogRoot, 1);

		paf::ui::Widget *bgWidget =
			m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_BG), 0);
		if (bgWidget != NULL) {
			paf::ui::Widget *bgParent = bgWidget->m_parent;
			bgWidget->Hide(0xff, new paf::Timer(200.0f, paf::Timer::FUNC_CUBIC_IN),
				       NULL, 0.0f);

			PafVec4 alignOffs = m_dialogRoot->GetAlignOffsetToCenter(0);
			bgWidget->SetAlign(0, 0, 0, NULL);
			PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
			PafVec4 bgPos = bgWidget->GetPos(0, NULL, zeroOffset);
			PafVec4 centeredPos(bgPos.extract_x() + alignOffs.extract_x(),
					    bgPos.extract_y() + alignOffs.extract_y(),
					    bgPos.extract_z() + alignOffs.extract_z(),
					    bgPos.extract_w() + alignOffs.extract_w());
			bgWidget->SetPos(centeredPos, NULL, ANIM_CHANNEL_POS, 0,
					 0.0f, NULL, NULL);
			bgWidget->SetAdjust(0, 0, 0);

			PafVec4 parentOrigin = bgParent->GetOriginWorldMatrix().w();
			PafVec4 destPos(targetPos.extract_x() - parentOrigin.extract_x(),
					targetPos.extract_y() - parentOrigin.extract_y(),
					0.0f, 0.0f);
			bgWidget->SetPos(destPos, new paf::Timer(250.0f, paf::Timer::FUNC_CUBIC_IN),
					 ANIM_CHANNEL_POS, 2, 0.0f, NULL, NULL);

			void *idArg =
				reinterpret_cast<void *>(static_cast<intptr_t>(m_dialogId));
			bgWidget->SetSize(targetSize,
					  new paf::Timer(250.0f, paf::Timer::FUNC_CUBIC_IN),
					  ANIM_CHANNEL_SIZE, 2, 0.0f,
					  &OnDialogCloseTransitionDone, idArg);

			if (m_listView != NULL)
				m_listView->m_clip_mode = 1;
			m_dialogRoot->SetIntervalEvent(TIMER_ID_DIALOG_CLIP, 0.0f,
						       &OnDialogClipTimer, NULL);
		}

		paf::ui::Widget *panelWidget =
			m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_PANEL), 0);
		if (panelWidget != NULL && m_displayMode != ACTIVITY_DB_OWNER_TITLE) {
			PafVec4 zeroOffset(0.0f, 0.0f, 0.0f, 0.0f);
			PafVec4 barPos = panelWidget->GetPos(0, NULL, zeroOffset);
			barPos.set_y(barPos.extract_y() - panelWidget->GetSize(0)->extract_y());
			panelWidget->SetPos(barPos,
					    new paf::Timer(250.0f, paf::Timer::FUNC_QUAD_IN),
					    ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
		}

		paf::ui::Widget *backBtn = m_dialogRoot->FindChild(GetBackButtonId(), 0);
		if (backBtn != NULL)
			ScePafWidget_F1176ACC(
				backBtn,
				static_cast<paf::common::transition::Type>(0xff),
				0.0f, NULL, NULL);
		paf::ui::Widget *optionBtn = m_dialogRoot->FindChild(GetOptionButtonId(), 0);
		if (optionBtn != NULL)
			ScePafWidget_F1176ACC(
				optionBtn,
				static_cast<paf::common::transition::Type>(0xff),
				0.0f, NULL, NULL);
	}
}

void ActivityStatusDialog::CloseWithReason(int reason)
{
	(void)reason;
	Close();
}

void ActivityStatusDialog::OnPopupMenuOpened()
{
}

void ActivityStatusDialog::OnPopupMenuClosed()
{
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

/* 0x81354504 */
void ActivityStatusDialog::StartVoteBusyIndicator(paf::ui::Widget *target)
{
	if (target == NULL)
		return;
	if (target->FindChild(paf::IDParam(WIDGET_STATUS_BUSY_INDICATOR), 0) != NULL)
		return;

	paf::Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(target, paf::IDParam(TEMPLATE_STATUS_BUSY_OVERLAY), openParam);
	paf::ui::BusyIndicator *indicator = static_cast<paf::ui::BusyIndicator *>(
		target->FindChild(paf::IDParam(WIDGET_STATUS_BUSY_INDICATOR), 0));
	if (indicator != NULL)
		indicator->Start();
}

/* 0x81354660 */
void ActivityStatusDialog::ShowVoteBusy(bool isZeroVotesButton)
{
	if (m_listView == NULL)
		return;
	paf::ui::Widget *segmentHeader = m_listView->GetSegmentHeader(0);
	if (segmentHeader == NULL)
		return;
	paf::ui::Widget *headerBox =
		segmentHeader->FindChild(paf::IDParam(WIDGET_GOODJOB_HEADER_BOX), 0);
	if (headerBox == NULL)
		return;

	if (isZeroVotesButton) {
		paf::ui::ButtonBase *voteBtn = static_cast<paf::ui::ButtonBase *>(
			headerBox->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_BTN), 0));
		if (voteBtn != NULL) {
			voteBtn->Disable(false);
			paf::ui::Widget *iconWidget =
				voteBtn->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_ICON), 0);
			if (iconWidget != NULL) {
				PafVec4 dimColor(1.0f, 1.0f, 1.0f, 0.3f);
				if (voteBtn->m_disable_color != NULL)
					dimColor = *voteBtn->m_disable_color;
				iconWidget->SetColor(dimColor, NULL, ANIM_CHANNEL_COLOR, 0,
						     0.0f, NULL, NULL);
				StartVoteBusyIndicator(iconWidget);
			}
			paf::ui::Widget *labelWidget =
				voteBtn->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_LABEL), 0);
			if (labelWidget != NULL) {
				PafVec4 dimColor(1.0f, 1.0f, 1.0f, 0.3f);
				if (voteBtn->m_disable_color != NULL)
					dimColor = *voteBtn->m_disable_color;
				labelWidget->SetColor(dimColor, NULL, ANIM_CHANNEL_COLOR, 0,
						      0.0f, NULL, NULL);
			}
		}
	} else {
		paf::ui::Widget *areaWidget =
			headerBox->FindChild(paf::IDParam(WIDGET_GOODJOB_AREA), 0);
		if (areaWidget != NULL) {
			paf::ui::ButtonBase *actionBtn = static_cast<paf::ui::ButtonBase *>(
				areaWidget->FindChild(paf::IDParam(WIDGET_GOODJOB_ACTION_BTN), 0));
			if (actionBtn != NULL) {
				actionBtn->Disable(false);
				StartVoteBusyIndicator(actionBtn);
			}
		}
	}
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

/* 0x81355EE8 */
void ActivityStatusDialog::StopVoteBusyIndicator(paf::ui::Widget *container)
{
	if (container == NULL)
		return;
	paf::ui::BusyIndicator *indicator = static_cast<paf::ui::BusyIndicator *>(
		container->FindChild(paf::IDParam(WIDGET_STATUS_BUSY_INDICATOR), 0));
	if (indicator != NULL) {
		indicator->Stop();
		ScePafWidget_5A9793B0(indicator, 0.0f);
		indicator->m_name = paf::IDParam("");
	}
}

/* 0x81355FE6 */
void ActivityStatusDialog::OnVoteFinished(int errorCode, bool isZeroVotesButton)
{
	if (m_listView == NULL)
		return;
	paf::ui::Widget *segmentHeader = m_listView->GetSegmentHeader(0);
	if (segmentHeader == NULL)
		return;
	paf::ui::Widget *headerBox =
		segmentHeader->FindChild(paf::IDParam(WIDGET_GOODJOB_HEADER_BOX), 0);
	if (headerBox == NULL)
		return;

	StopVoteBusyIndicator(headerBox);
	if (errorCode != 0) {
		if (isZeroVotesButton) {
			paf::ui::ButtonBase *voteBtn = static_cast<paf::ui::ButtonBase *>(
				headerBox->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_BTN), 0));
			if (voteBtn != NULL) {
				voteBtn->Enable(false);
				paf::ui::Widget *iconWidget =
					voteBtn->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_ICON),
							   0);
				if (iconWidget != NULL) {
					PafVec4 fullColor(1.0f, 1.0f, 1.0f, 1.0f);
					iconWidget->SetColor(fullColor, NULL, ANIM_CHANNEL_COLOR,
							     0, 0.0f, NULL, NULL);
				}
			}
			paf::ui::Widget *labelWidget =
				headerBox->FindChild(paf::IDParam(WIDGET_GOODJOB_VOTE_LABEL), 0);
			if (labelWidget != NULL) {
				PafVec4 fullColor(1.0f, 1.0f, 1.0f, 1.0f);
				labelWidget->SetColor(fullColor, NULL, ANIM_CHANNEL_COLOR, 0,
						      0.0f, NULL, NULL);
			}
		} else {
			paf::ui::ButtonBase *actionBtn = static_cast<paf::ui::ButtonBase *>(
				headerBox->FindChild(paf::IDParam(WIDGET_GOODJOB_ACTION_BTN), 0));
			if (actionBtn != NULL)
				actionBtn->Enable(false);
		}
	}
}

/* 0x813562A8 */
void ActivityStatusDialog::EnableCommentBox()
{
	if (m_dialogRoot == NULL)
		return;
	paf::ui::Widget *commentBox =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0);
	if (commentBox != NULL && m_inputDisabled) {
		ScePafWidget_A96A0B4A(commentBox, -1);
		PafVec4 fullColor(1.0f, 1.0f, 1.0f, 1.0f);
		commentBox->SetColor(fullColor, NULL, ANIM_CHANNEL_COLOR, 0, 0.0f, NULL, NULL);
		m_inputDisabled = false;
	}
}

/* 0x81356370 */
void ActivityStatusDialog::EnableSendButtonIfReady()
{
	if (m_dialogRoot == NULL)
		return;
	paf::ui::ButtonBase *sendBtn = static_cast<paf::ui::ButtonBase *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0));
	if (m_commentCount < ACTIVITY_MAX_COMMENTS && !m_postingComment && sendBtn != NULL) {
		paf::ui::Text *commentBox = static_cast<paf::ui::Text *>(
			m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
		if (commentBox != NULL) {
			uint32_t textLen = 0;
			commentBox->GetLength(textLen);
			if (!sendBtn->IsButtonEnabled() && textLen != 0)
				sendBtn->Enable(false);
		}
	}
}

/* 0x8135689A */
void ActivityStatusDialog::OnPostCommentFinished(int errorCode)
{
	if (m_dialogRoot != NULL) {
		paf::ui::ButtonBase *sendBtn = static_cast<paf::ui::ButtonBase *>(
			m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0));
		if (sendBtn != NULL) {
			m_postingComment = false;
			bool listFillsView = false;
			if (m_listView != NULL) {
				listFillsView =
					(m_listView->GetSize(0)->extract_y() <
					 m_listView->m_view_box_size.extract_y());
			}
			if (!listFillsView || errorCode != 0)
				m_busyFooter.HideBusyHeader();

			paf::ui::Widget *commentBox = m_dialogRoot->FindChild(
				paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0);
			if (commentBox != NULL) {
				PafVec4 fullColor(1.0f, 1.0f, 1.0f, 1.0f);
				commentBox->SetObjAnim(
					0, OBJ_ANIM_COLOR, fullColor,
					new paf::Timer(250.0f, paf::Timer::FUNC_SINE_IN), 2);
			}

			if (!sendBtn->IsButtonEnabled()) {
				paf::ui::Text *textBox = static_cast<paf::ui::Text *>(
					m_dialogRoot->FindChild(
						paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
				if (textBox != NULL) {
					if (errorCode != 0 &&
					    errorCode != ACTIVITY_ERR_POST_CANCELED) {
						if (m_record.get() != NULL &&
						    m_record->GetRecordKind() == LIVE2_RECORD_STATUS) {
							Live2StatusRecord *statusRec =
								static_cast<Live2StatusRecord *>(
									m_record.get());
							ShowStatusDialogError(
								&statusRec->m_status.rowid,
								errorCode, false);
						}
					} else {
						textBox->SetString(paf::wstring(L""));
						textBox->m_user_data[0] = reinterpret_cast<void *>(
							static_cast<uintptr_t>(1));
					}
				}
			}
		}
	}

	if (errorCode != 0) {
		EnableSendButtonIfReady();
		EnableCommentBox();
	} else {
		m_animateNewComment = true;
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

void ActivityStatusDialog::OnDetailSyncFinished(int errorCode)
{
	m_syncingDetail = false;
	if (errorCode == 0)
		RequestVoteInfo();
}

/* 0x81356B6C */
void ActivityStatusDialog::OnCacheInfoUpdated()
{
	if (m_record.get() == NULL || m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;

	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	statusRec->m_status.announcePanelBgColor = activityUiGetCachedPanelBgColor();
	statusRec->m_status.announcePanelUrl = activityUiGetCachedPanelUrl();
	statusRec->m_status.announceAvatarUrl = activityUiGetCachedAvatarUrl();

	paf::ui::Widget *bgWidget =
		(m_dialogRoot != NULL)
			? m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_BG), 0)
			: NULL;
	if (bgWidget == NULL)
		return;

	SceUInt32 swappedColor = 0;
	BaseActivityVisualizer::SwapColorEndian(statusRec->m_status.announcePanelBgColor,
						&swappedColor);
	if (m_displayMode == ACTIVITY_DB_OWNER_SELF) {
		ActivityDrawObj *panelObj =
			static_cast<ActivityDrawObj *>(bgWidget->GetDrawObj(STATUS_DRAWOBJ_PANEL));
		if (panelObj != NULL)
			panelObj->m_color = swappedColor;

		paf::intrusive_ptr<paf::cache::CacheImageInterface> imageCache;
		BaseActivityVisualizer::CreateActivityImageCache(
			statusRec->m_status.announcePanelUrl, imageCache, m_cacheManager);
		bgWidget->SetDrawObjMaskBit(&imageCache, STATUS_DRAWOBJ_PANEL, 2);
	}

	paf::ui::Widget *panelWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_PANEL), 0);
	if (panelWidget != NULL) {
		PafVec4 colorVec(static_cast<float>(swappedColor & 0xFF) / 255.0f,
				 static_cast<float>((swappedColor >> 8) & 0xFF) / 255.0f,
				 static_cast<float>((swappedColor >> 16) & 0xFF) / 255.0f,
				 static_cast<float>((swappedColor >> 24) & 0xFF) / 255.0f);
		panelWidget->SetColor(colorVec, NULL, ANIM_CHANNEL_COLOR, 0, 0.0f, NULL, NULL);
	}

	if (m_displayMode == ACTIVITY_DB_OWNER_SELF && m_listView != NULL) {
		paf::ui::Widget *segmentHeader = m_listView->GetSegmentHeader(0);
		if (segmentHeader != NULL) {
			paf::ui::Widget *avatarBtn = segmentHeader->FindChild(
				paf::IDParam(WIDGET_STATUS_AVATAR_BTN), 0);
			if (avatarBtn != NULL) {
				BaseActivityVisualizer::ApplyAvatarIconForRecord(
					m_record, avatarBtn, m_cacheManager);
			}
		}
	}
}

/* 0x8135743A */
void ActivityStatusDialog::SetupGoodJobContainer(paf::ui::Widget *headerRoot, float goodJobY)
{
	m_goodJobY = goodJobY;
	if (m_displayMode == ACTIVITY_DB_OWNER_SELF || m_displayMode == 0) {
		paf::Plugin::TemplateOpenParam openParam;
		m_plugin->TemplateOpen(headerRoot, paf::IDParam(TEMPLATE_GOODJOB_HEADER_BOX),
				       openParam);
		int voteCount = BaseActivityVisualizer::GetVoteCount(m_record);
		UpdateGoodJobArea(voteCount, false);
		RequestVoteInfo();
	}
}

/* 0x813576C6 */
void ActivityStatusDialog::OnCommentCountChanged(int count)
{
	if (m_commentCount == count)
		return;

	if (m_commentCount > 0 && m_commentCount < count &&
	    m_selectedCommentVis.get() == NULL && m_listView != NULL) {
		PafVec2 curFocus = m_listView->GetViewFocus();
		PafVec2 targetFocus(0.0f, 0.0f);
		m_listView->ScePafWidget_B3B40B75(
			&targetFocus, 0, m_commentCount - 1,
			static_cast<paf::ui::ListView::FocusAlignType>(0));
		float deltaY = __builtin_fabsf(curFocus.extract_y() - targetFocus.extract_y());
		if (deltaY < 1.0f)
			deltaY = 1.0f;
		float root = __builtin_sqrtf(deltaY);
		float duration = __builtin_sqrtf(root) * 100.0f;
		m_listView->SetFocus(
			0, m_commentCount - 1,
			static_cast<paf::ui::ListView::FocusAlignType>(0),
			new paf::Timer(duration, paf::Timer::FUNC_QUART_OUT));
	}

	m_commentCount = count;
	if (count < ACTIVITY_MAX_COMMENTS) {
		if (!m_postingComment) {
			EnableSendButtonIfReady();
			EnableCommentBox();
		}
	} else {
		DisableSendButton();
		DisableCommentBox();
		if (m_dialogRoot != NULL) {
			paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
				m_dialogRoot->FindChild(
					paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
			if (commentBox != NULL && commentBox->IsEditing())
				commentBox->EndEdit();
		}
	}
}

/* 0x81356B66 */
void ActivityStatusDialog::SetAnimateNewComment(bool animate)
{
	m_animateNewComment = animate;
}

/* 0x81356DD0 */
void ActivityStatusDialog::ClosePopupIfSelectedComment(Lv2CommentActivityVisualizer *visualizer)
{
	if (m_popupMenuOpen && m_selectedCommentVis.get() == visualizer) {
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
	param.SetCloseCallback(&OnCommentPopupMenuClosedCallback, idArg);

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
	param.SetCloseCallback(&OnCommentPopupMenuClosedCallback, idArg);

	m_selectedCommentVis = visualizer;
	m_popupMenuOpen = true;
	shellPopupListOpen(&param, 0, 0);
}

/* 0x81357A46 */
void ActivityStatusDialog::OnDeleteCommentMenuSelected()
{
	if (m_selectedCommentVis.get() == NULL)
		return;

	if (m_dialogRoot != NULL) {
		paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
			m_dialogRoot->FindChild(
				paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
		if (commentBox != NULL && commentBox->IsEditing())
			commentBox->EndEdit();
	}

	paf::intrusive_ptr<Live2Record> commentRec = m_selectedCommentVis->m_record;
	if (commentRec.get() != NULL && commentRec->GetRecordKind() == LIVE2_RECORD_COMMENT)
		ShowDeleteCommentConfirmDialog(commentRec);
	m_selectedCommentVis = NULL;
}

/* 0x81357C9C */
void ActivityStatusDialog::OnGriefReportMenuSelected()
{
	if (m_selectedCommentVis.get() == NULL)
		return;

	if (m_dialogRoot != NULL) {
		paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
			m_dialogRoot->FindChild(
				paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
		if (commentBox != NULL && commentBox->IsEditing())
			commentBox->EndEdit();
	}

	paf::intrusive_ptr<Live2Record> commentRec = m_selectedCommentVis->m_record;
	if (commentRec.get() != NULL && commentRec->GetRecordKind() == LIVE2_RECORD_COMMENT)
		StartGriefReport(commentRec);
	m_selectedCommentVis = NULL;
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
	param.SetSelectCallback(&OnStatusOptionMenuDeleteCallback, idArg);
	param.SetCloseCallback(&OnStatusOptionMenuCloseCallback, idArg);

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

/* 0x813566FC wrapper */
void ActivityStatusDialog::ShowErrorDialog(int errorCode, bool closeStatusDialogOnDismiss)
{
	(void)closeStatusDialogOnDismiss;
	if (m_record.get() == NULL || m_record->GetRecordKind() != LIVE2_RECORD_STATUS)
		return;
	Live2StatusRecord *statusRec = static_cast<Live2StatusRecord *>(m_record.get());
	ShowStatusDialogError(&statusRec->m_status.rowid, errorCode, false);
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

	StartPostCommentAnimation(true);

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
	ShowVoteBusy(m_voteInfo.numOfVotes == 0);

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
	ShowVoteBusy(false);

	paf::common::SharedPtr<paf::job::JobItem> job(
		new CancelGoodJob(s_serviceManager->syncHandle, statusRowid));
	s_serviceManager->postJobQueue->Enqueue(job);
}

void ActivityStatusDialog::SetInputEnabled(bool enabled)
{
	if (enabled) {
		EnableSendButtonIfReady();
		EnableCommentBox();
	} else {
		DisableSendButton();
		DisableCommentBox();
	}
}

/* 0x81351848 */
void ActivityStatusDialog::StartPostCommentAnimation(bool animateOverlay)
{
	if (m_dialogRoot == NULL)
		return;

	paf::ui::Widget *sendBtn =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_SEND_BTN), 0);
	if (sendBtn == NULL)
		return;

	m_postingComment = true;
	DisableSendButton();
	DisableCommentBox();

	paf::ui::TextBox *commentBox = static_cast<paf::ui::TextBox *>(
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0));
	if (commentBox != NULL && commentBox->IsEditing())
		commentBox->EndEdit();

	m_busyFooter.ShowBusyHeader();
	if (m_listView != NULL) {
		m_listView->BlockSync();
		m_listView->UnblockSync();
	}

	paf::ui::Widget *boxWidget =
		m_dialogRoot->FindChild(paf::IDParam(WIDGET_STATUS_DIALOG_COMMENT_BOX), 0);
	if (boxWidget != NULL) {
		PafVec4 fadeColor(1.0f, 1.0f, 1.0f, 0.0f);
		boxWidget->SetObjAnim(0, OBJ_ANIM_COLOR, fadeColor,
				      new paf::Timer(250.0f, paf::Timer::FUNC_SINE_IN), 2);
	}

	if (animateOverlay)
		CreatePostCommentOverlay();
	else
		m_busyFooter.StartBusyIndicator();

	if (m_selectedCommentVis.get() == NULL && animateOverlay && m_listView != NULL) {
		PafVec2 curFocus = m_listView->GetViewFocus();
		PafVec2 targetFocus(0.0f, 0.0f);
		m_listView->ScePafWidget_B3B40B75(
			&targetFocus, 1, -1,
			static_cast<paf::ui::ListView::FocusAlignType>(0));
		float deltaY = __builtin_fabsf(curFocus.extract_y() - targetFocus.extract_y());
		if (deltaY < 1.0f)
			deltaY = 1.0f;
		float root = __builtin_sqrtf(deltaY);
		float duration = __builtin_sqrtf(root) * 100.0f;
		m_listView->SetFocus(
			1, -1,
			static_cast<paf::ui::ListView::FocusAlignType>(0),
			new paf::Timer(duration, paf::Timer::FUNC_QUART_OUT));
	}
}

/* 0x81358EA0 */
float ActivityStatusDialog::GetEditCommentBarY()
{
	return ((-13.0f - GetListViewHeight()) + 3.0f) + 305.0f;
}

/* 0x8135A29A */
void ActivityStatusDialog::UpdatePostCommentAnimation()
{
	if (m_postAnimWidget == NULL)
		return;

	float prevTime = m_postAnimTime;
	float curTime = prevTime + paf::Timer::s_tick_unit * paf::Timer::s_tick_interval;
	m_postAnimTime = curTime;

	if (curTime < 0.23f) {
		float alpha = paf::Timer::DoFuncSineIn(curTime / 0.23f) * 0.7f;
		m_postAnimWidget->SetMetaAlpha(alpha, NULL, ANIM_CHANNEL_META_ALPHA, 0,
					       0.0f, NULL, NULL);
	} else if (curTime < 0.36f) {
		float eased = paf::Timer::DoFuncSineOut((curTime - 0.23f) / (0.36f - 0.23f));
		float alpha = 0.7f + eased * -0.2f;
		m_postAnimWidget->SetMetaAlpha(alpha, NULL, ANIM_CHANNEL_META_ALPHA, 0,
					       0.0f, NULL, NULL);
	} else if (curTime < 0.66f) {
		m_postAnimWidget->SetMetaAlpha(0.5f, NULL, ANIM_CHANNEL_META_ALPHA, 0,
					       0.0f, NULL, NULL);
	} else if (curTime < 0.93f) {
		float eased = paf::Timer::DoFuncLinear((curTime - 0.66f) / (0.93f - 0.66f));
		float alpha = 0.5f + eased * -0.5f;
		m_postAnimWidget->SetMetaAlpha(alpha, NULL, ANIM_CHANNEL_META_ALPHA, 0,
					       0.0f, NULL, NULL);
	} else {
		m_postAnimWidget->DestroyWidget();
		m_postAnimWidget = NULL;
		return;
	}

	if (m_postAnimTime < 0.36f) {
		SyncPostAnimOverlayToCommentBox();
		return;
	}

	if (m_listView == NULL || m_dialogRoot == NULL)
		return;

	paf::ui::Widget *footerHeader = m_listView->GetSegmentHeader(1);
	if (footerHeader == NULL)
		return;

	paf::ui::Widget *targetWidget =
		footerHeader->FindChild(paf::IDParam(WIDGET_BUSY_FOOTER_TARGET), 0);
	if (targetWidget == NULL)
		return;

	if (prevTime <= 0.73f && m_postAnimTime >= 0.73f)
		m_busyFooter.StartBusyIndicator();

	if (prevTime <= 0.36f && m_postAnimTime >= 0.36f) {
		m_postAnimStartPos = m_postAnimWidget->GetWorldMatrix().w();
		m_postAnimStartSize = *m_postAnimWidget->GetSize(0);
	}

	PafVec4 destPos = targetWidget->GetWorldMatrix().w();
	PafVec4 destSize = *targetWidget->GetSize(0);
	if (m_postAnimTime > 0.36f && m_postAnimTime < 0.76f) {
		float ratio = (m_postAnimTime - 0.36f) / (0.76f - 0.36f);
		float posEase = paf::Timer::DoFuncCubicOut(ratio);
		float sizeEase = paf::Timer::DoFuncQuadOut(ratio);
		destPos = PafVec4(
			m_postAnimStartPos.extract_x() +
				(destPos.extract_x() - m_postAnimStartPos.extract_x()) * posEase,
			m_postAnimStartPos.extract_y() +
				(destPos.extract_y() - m_postAnimStartPos.extract_y()) * posEase,
			m_postAnimStartPos.extract_z() +
				(destPos.extract_z() - m_postAnimStartPos.extract_z()) * posEase,
			m_postAnimStartPos.extract_w() +
				(destPos.extract_w() - m_postAnimStartPos.extract_w()) * posEase);
		destSize = PafVec4(
			m_postAnimStartSize.extract_x() +
				(destSize.extract_x() - m_postAnimStartSize.extract_x()) * sizeEase,
			m_postAnimStartSize.extract_y() +
				(destSize.extract_y() - m_postAnimStartSize.extract_y()) * sizeEase,
			m_postAnimStartSize.extract_z() +
				(destSize.extract_z() - m_postAnimStartSize.extract_z()) * sizeEase,
			m_postAnimStartSize.extract_w() +
				(destSize.extract_w() - m_postAnimStartSize.extract_w()) * sizeEase);
	}

	PafVec4 rootOrigin = m_dialogRoot->GetOriginWorldMatrix().w();
	PafVec4 relPos(destPos.extract_x() - rootOrigin.extract_x(),
		       destPos.extract_y() - rootOrigin.extract_y(), 0.0f, 0.0f);
	PafVec4 finalSize(destSize.extract_x(), destSize.extract_y(), 0.0f, 0.0f);
	m_postAnimWidget->SetPos(relPos, NULL, ANIM_CHANNEL_POS, 0, 0.0f, NULL, NULL);
	m_postAnimWidget->SetSize(finalSize, NULL, ANIM_CHANNEL_SIZE, 0, 0.0f, NULL, NULL);
}

/* -------------------------------------------------------------------------- */
/* Cross-unit Activity UI Registry & Service APIs                             */
/* -------------------------------------------------------------------------- */

/* 0x81356576 */
ActivityStatusDialog *activityUiFindDialogByStatusRowid(const SceInt64 *statusRowid)
{
	if (s_dialogManager == NULL)
		return NULL;
	return s_dialogManager->FindByStatusRowid(statusRowid);
}

/* 0x8135B002 */
ActivityStatusDialog *activityUiFindDialogById(int dialogId)
{
	if (s_dialogManager == NULL)
		return NULL;
	return s_dialogManager->FindById(dialogId);
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
		delete dialog;
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
}

/* 0x81350B0C */
void activityUiDialogManagerTerm(void)
{
	if (filelist::s_fileListRegistry != NULL) {
		delete filelist::s_fileListRegistry;
		filelist::s_fileListRegistry = NULL;
	}
}

/* 0x8135A13E */
void activityUiServiceInit(void)
{
	if (filelist::s_dialogManager == NULL)
		filelist::s_dialogManager = new filelist::ActivityDialogManager();
	if (filelist::s_serviceManager == NULL)
		filelist::s_serviceManager = new filelist::ActivityServiceManager();
}

/* 0x8135A244 */
void activityUiServiceTerm(void)
{
	if (filelist::s_serviceManager != NULL) {
		delete filelist::s_serviceManager;
		filelist::s_serviceManager = NULL;
	}
	if (filelist::s_dialogManager != NULL) {
		delete filelist::s_dialogManager;
		filelist::s_dialogManager = NULL;
	}
}

/* 0x8135B0AC */
paf::job::JobQueue *activityUiGetNetworkJobQueue(void)
{
	if (filelist::s_serviceManager == NULL)
		return NULL;
	return filelist::s_serviceManager->postJobQueue;
}

/* 0x8135B248 */
void activityUiNotifyCacheInfoUpdated(void)
{
	if (filelist::s_dialogManager != NULL)
		filelist::s_dialogManager->NotifyCacheInfoUpdated();
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
