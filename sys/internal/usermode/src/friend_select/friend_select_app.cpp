/*
 * SceFriendSelect: Application controller, state flow machine, single-select
 * and multi-select friend list pages, and plugin environment helpers.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/transition.h>
#include <framework.h>
#include <inputdevice/inputdevice.h>
#include <plugin.h>
#include <thread/thread.h>
#include <versatileparam/idparam.h>
#include <widget/core/environment.h>
#include <widget/core/widget.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_scene.h>
#include "friend_select_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::inputdevice;
using namespace paf::thread;
using namespace paf::ui;

class FriendSelectFlow;
class FriendSelectApp;

static FriendSelectApp *s_selectApp = NULL;
static FriendSelectSingleData *s_singleData = NULL;
static FriendSelectMultiData *s_multiData = NULL;
static int32_t s_transitionMode = 1;

/* ========================================================================= */
/* Class declarations (methods defined out-of-line below, matching Sony TU)  */
/* ========================================================================= */

class FriendSelectSingleData {
public:
	FriendSelectSingleData();
	virtual ~FriendSelectSingleData();

	void SetOutputBuffer(SceNpId *dst);
	int32_t SelectAt(int32_t index);

	FriendListCache m_listCache;
	SceNpId *m_outputNpId;
};

class FriendSelectTaskBase {
public:
	FriendSelectTaskBase();
	virtual ~FriendSelectTaskBase();

	virtual void Start(Callback1<int32_t> *callback);
	virtual void Finish();
	virtual void Abort();

	Callback1<int32_t> *m_callback;
	int32_t m_result;
	bool m_closing;
};

class FriendSelectPageBase : public FriendSelectTaskBase {
public:
	explicit FriendSelectPageBase(const IDParam &pageId);
	virtual ~FriendSelectPageBase();

	virtual void Start(Callback1<int32_t> *callback);
	virtual void Finish();
	virtual void Abort();
	virtual void OpenPage();
	virtual void ClosePage(int32_t result);
	virtual void OnPageCreated();
	virtual void OnPageDestroyed();
	virtual void OnPageClosing();
	virtual void OnHeaderAction(int32_t action, void *arg);
	virtual void OnClockChanged();

	void BeginCloseTransition();
	void OnCloseTimerDone(int32_t type, Handler *self, Event *e, void *userData);
	void OnImagesCanceled(void *arg);

	IDParam m_pageId;
	Scene *m_scene;
};

class FriendSelectInitTask : public FriendSelectTaskBase {
public:
	FriendSelectInitTask();
	virtual ~FriendSelectInitTask();

	virtual void Start(Callback1<int32_t> *callback);
	void OnNpReady(int32_t result, void *arg);
	void OnErrorDismissed(void *arg);
};

class FriendSelectCloseTask : public FriendSelectTaskBase {
public:
	FriendSelectCloseTask();
	virtual ~FriendSelectCloseTask();

	virtual void Start(Callback1<int32_t> *callback);
	virtual void Abort();
	void OnCanceled(void *arg);
};

class FriendSelectSinglePage : public FriendSelectPageBase {
public:
	FriendSelectSinglePage();
	virtual ~FriendSelectSinglePage();

	virtual void OnPageCreated();
	virtual void OnPageDestroyed();
	virtual void OnHeaderAction(int32_t action, void *arg);
	virtual void OnClockChanged();

	ListItem *CreateItem(Widget *parent, void *arg);
	void BindItem(Widget *widget, int32_t index, void *arg);
	void UnbindItem(Widget *widget, int32_t index, void *arg);
	void OnSelectItem(int32_t index, void *arg);
	void RefreshList();
	void OnListFetched(int32_t result, void *arg);
	void OnSortMenuSelected(int32_t choice, void *arg);

	FriendSelectHeader m_header;
	ListView *m_listView;
	Widget *m_emptyText;
	int32_t m_restoreIndex;
	FriendSelectItemFactory *m_itemFactory;
};

class FriendSelectMultiPage : public FriendSelectPageBase {
public:
	FriendSelectMultiPage();
	virtual ~FriendSelectMultiPage();

	virtual void OnPageCreated();
	virtual void OnPageDestroyed();
	virtual void OnHeaderAction(int32_t action, void *arg);
	virtual void OnClockChanged();

	ListItem *CreateItem(Widget *parent, void *arg);
	void BindItem(Widget *widget, int32_t index, void *arg);
	void UnbindItem(Widget *widget, int32_t index, void *arg);
	void OnToggleItem(int32_t index, void *arg);
	void RefreshList();
	void OnListFetched(int32_t result, void *arg);
	void OnSortMenuSelected(int32_t choice, void *arg);

	FriendSelectHeader m_header;
	ListView *m_listView;
	Widget *m_emptyText;
	int32_t m_restoreIndex;
	FriendSelectItemFactory *m_itemFactory;
};

class FriendSelectStateHolderBase {
public:
	FriendSelectStateHolderBase() : m_task(NULL) {}
	virtual ~FriendSelectStateHolderBase()
	{
		if (m_task != NULL) {
			delete m_task;
			m_task = NULL;
		}
	}

	virtual void Create() = 0;
	virtual FriendSelectStateHolderBase *Next(int32_t *result) = 0;

	FriendSelectTaskBase *m_task;
};

template <typename TaskT>
class FriendSelectStateHolder : public FriendSelectStateHolderBase {
public:
	FriendSelectStateHolder() : FriendSelectStateHolderBase() {}
	virtual ~FriendSelectStateHolder() {}

	virtual void Create()
	{
		m_task = new TaskT();
	}

	virtual FriendSelectStateHolderBase *Next(int32_t *result);
};

class FriendSelectFlow {
public:
	FriendSelectFlow();
	virtual ~FriendSelectFlow();

	void FinishFlow();
	void Start(Callback1<int32_t> *doneCallback);
	void OnStateFinished(int32_t result, void *arg);

	Callback1<int32_t> *m_doneCallback;
	FriendSelectStateHolderBase *m_state;
	int32_t m_result;
};

class FriendSelectApp {
public:
	explicit FriendSelectApp(Plugin *plugin);
	virtual ~FriendSelectApp();

	void OnFlowDone(int32_t result, void *arg);
	void Start();
	void Abort();

	Plugin *m_plugin;
	FriendSelectFlow *m_flow;
	Mutex m_mutex;
	int32_t m_mode;
	uint32_t m_bgColor;
};

/* ========================================================================= */
/* FriendSelectApp & global plugin app entrypoints                           */
/* ========================================================================= */

FriendSelectApp::FriendSelectApp(Plugin *plugin)
	: m_plugin(plugin), m_flow(NULL), m_mutex("SceFriendSelect", 0), m_mode(-1),
	  m_bgColor(FRIEND_SELECT_DEFAULT_BG_COLOR)
{
	bool found = false;
	Plugin::Argument modeArg = m_plugin->GetArg("mode", &found);
	m_mode = found ? (int32_t)modeArg : -1;

	Plugin::Argument bgArg = m_plugin->GetArg("bg_color", &found);
	m_bgColor = found ? (uint32_t)(int32_t)bgArg : FRIEND_SELECT_DEFAULT_BG_COLOR;
}

FriendSelectApp::~FriendSelectApp()
{
}

void FriendSelectApp::OnFlowDone(int32_t result, void *arg)
{
	(void)arg;
	if (m_flow != NULL) {
		delete m_flow;
		m_flow = NULL;
	}
	int32_t notifyCode = result;
	if (result == 0) {
		notifyCode = 0;
	} else {
		switch (result) {
		case -1:
			notifyCode = -1;
			break;
		case -2:
			notifyCode = -2;
			break;
		case -3:
			notifyCode = -3;
			break;
		case FRIEND_SELECT_ERROR_NP_OFFLINE:
			notifyCode = -5;
			break;
		default:
			break;
		}
	}
	m_plugin->Notify(notifyCode);
	m_mutex.Unlock();
}

void FriendSelectApp::Start()
{
	m_mutex.Lock();
	m_flow = new FriendSelectFlow();
	if (m_flow == NULL) {
		OnFlowDone(FRIEND_SELECT_ERROR_NOMEM, NULL);
		return;
	}
	m_flow->Start(new MemberCallback1<FriendSelectApp, int32_t>(
		this, &FriendSelectApp::OnFlowDone));
}

void FriendSelectApp::Abort()
{
	if (m_flow != NULL && m_flow->m_state != NULL && m_flow->m_state->m_task != NULL)
		m_flow->m_state->m_task->Abort();
}

void FriendSelectAppCreate(Plugin *plugin)
{
	s_selectApp = new FriendSelectApp(plugin);
	FriendSelectWebApiInit();
}

void FriendSelectAppDestroy(void)
{
	if (s_selectApp != NULL) {
		s_selectApp->m_mutex.Lock();
		s_selectApp->m_mutex.Unlock();
	}
	FriendSelectWebApiTerm();
	if (s_selectApp != NULL) {
		delete s_selectApp;
		s_selectApp = NULL;
	}
}

void FriendSelectAppStart(void)
{
	if (s_selectApp != NULL)
		s_selectApp->Start();
}

void FriendSelectAppAbort(void)
{
	if (s_selectApp != NULL)
		s_selectApp->Abort();
}

Plugin *FriendSelectGetPlugin(void)
{
	return (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
}

int32_t FriendSelectGetMode(void)
{
	return (s_selectApp != NULL) ? s_selectApp->m_mode : -1;
}

uint32_t FriendSelectGetBgColor(void)
{
	return (s_selectApp != NULL) ? s_selectApp->m_bgColor : FRIEND_SELECT_DEFAULT_BG_COLOR;
}

/* ========================================================================= */
/* FriendSelectFlow & StateHolder specializations                            */
/* ========================================================================= */

void FriendSelectDataTerm(void)
{
	FriendSelectSingleDataDestroy();
}

FriendSelectFlow::FriendSelectFlow() : m_doneCallback(NULL), m_state(NULL), m_result(0)
{
	FriendSelectEnvInit();
}

FriendSelectFlow::~FriendSelectFlow()
{
	FriendSelectPageCloseAll();
	FriendSelectEnvTerm();
}

void FriendSelectFlow::FinishFlow()
{
	int32_t mode = FriendSelectGetMode();
	if (mode == 0)
		FriendSelectSingleDataDestroy();
	else if (mode == 1)
		FriendSelectMultiDataDestroy();
	FriendSelectDataTerm();
	if (m_doneCallback != NULL) {
		Callback1<int32_t> *cb = m_doneCallback;
		int32_t res = m_result;
		m_doneCallback = NULL;
		cb->Invoke(res);
		delete cb;
	}
}

void FriendSelectFlow::Start(Callback1<int32_t> *doneCallback)
{
	m_doneCallback = doneCallback;
	Plugin *plugin = FriendSelectGetPlugin();
	bool foundSort = false;
	Plugin::Argument sortArg = plugin->GetArg("sort", &foundSort);
	int32_t sortMode = (foundSort && (int32_t)sortArg == 1) ? 1 : 0;

	int32_t mode = FriendSelectGetMode();
	if (mode == 0) {
		FriendSelectSingleDataCreate();
		bool foundNpId = false;
		Plugin::Argument npArg = plugin->GetArg("npid", &foundNpId);
		SceNpId *npIdPtr = (SceNpId *)(uintptr_t)(int32_t)npArg;
		if (!foundNpId || npIdPtr == NULL) {
			m_result = -4;
			FinishFlow();
			return;
		}
		s_singleData->SetOutputBuffer(npIdPtr);
		s_singleData->m_listCache.SetSortMode(sortMode);
	} else if (mode == 1) {
		FriendSelectMultiDataCreate();
		bool foundMulti = false;
		Plugin::Argument mArg = plugin->GetArg("multi_data", &foundMulti);
		FriendSelectMultiExternalParam *mPtr =
			(FriendSelectMultiExternalParam *)(uintptr_t)(int32_t)mArg;
		if (!foundMulti || mPtr == NULL) {
			m_result = -4;
			FinishFlow();
			return;
		}
		s_multiData->SetExternalParam(mPtr);
		if (s_multiData->m_maxSelectable == 0) {
			m_result = -6;
			FinishFlow();
			return;
		}
		s_multiData->m_listCache.SetSortMode(sortMode);
	} else {
		m_result = -4;
		FinishFlow();
		return;
	}

	m_state = new FriendSelectStateHolder<FriendSelectInitTask>();
	m_state->Create();
	m_state->m_task->Start(new MemberCallback1<FriendSelectFlow, int32_t>(
		this, &FriendSelectFlow::OnStateFinished));
}

void FriendSelectFlow::OnStateFinished(int32_t result, void *arg)
{
	(void)arg;
	int32_t code = result;
	if (m_state->m_task != NULL) {
		delete m_state->m_task;
		m_state->m_task = NULL;
	}
	FriendSelectStateHolderBase *next = m_state->Next(&code);
	delete m_state;
	m_state = next;
	if (m_state == NULL) {
		if (code == 1) {
			FinishFlow();
			return;
		}
		m_result = code;
		m_state = new FriendSelectStateHolder<FriendSelectCloseTask>();
	}
	m_state->Create();
	m_state->m_task->Start(new MemberCallback1<FriendSelectFlow, int32_t>(
		this, &FriendSelectFlow::OnStateFinished));
}

template <>
FriendSelectStateHolderBase *
FriendSelectStateHolder<FriendSelectInitTask>::Next(int32_t *result)
{
	if (*result != 0)
		return NULL;
	FriendSelectSetTransitionMode(1);
	int32_t mode = FriendSelectGetMode();
	if (mode == 0)
		return new FriendSelectStateHolder<FriendSelectSinglePage>();
	if (mode == 1)
		return new FriendSelectStateHolder<FriendSelectMultiPage>();
	return NULL;
}

template <>
FriendSelectStateHolderBase *
FriendSelectStateHolder<FriendSelectSinglePage>::Next(int32_t *result)
{
	if (*result != 0)
		return NULL;
	return new FriendSelectStateHolder<FriendSelectCloseTask>();
}

template <>
FriendSelectStateHolderBase *
FriendSelectStateHolder<FriendSelectMultiPage>::Next(int32_t *result)
{
	if (*result != 0)
		return NULL;
	return new FriendSelectStateHolder<FriendSelectCloseTask>();
}

template <>
FriendSelectStateHolderBase *
FriendSelectStateHolder<FriendSelectCloseTask>::Next(int32_t *result)
{
	(void)result;
	return NULL;
}

/* ========================================================================= */
/* Plugin environment helpers                                                */
/* ========================================================================= */

void FriendSelectEnvInit(void)
{
	FriendSelectBgCreate();
	FriendSelectHeaderBarInit();
	FriendSelectErrorInit();
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	uint32_t prio = (uint32_t)plugin->m_draw_priority;
	Environment *env = Framework::Instance()->GetEnvironmentInstance();
	EntryRange range;
	range.m_first = prio - 3U;
	range.m_last = prio;
	ScePafWidget_094DF388(env, &range);
}

void FriendSelectEnvTerm(void)
{
	Environment *env = Framework::Instance()->GetEnvironmentInstance();
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	uint32_t prio = (uint32_t)plugin->m_draw_priority;
	const EntryRange *found = NULL;
	if (ScePafWidget_F5842A6B(env, prio - 3U, &found) >= 0) {
		vector<EntryRange>::iterator it((EntryRange *)found);
		vector<EntryRange>::iterator nextIt = it;
		++nextIt;
		env->m_entry_ranges.erase(it, nextIt);
	}
	FriendSelectErrorTerm();
	FriendSelectHeaderBarTerm();
	FriendSelectBgDestroy();
}

const wchar_t *FriendSelectGetString(const IDParam &id)
{
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	return plugin->GetString(id);
}

intrusive_ptr<Surface> FriendSelectGetTexture(const IDParam &id)
{
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	return plugin->GetTexture(id);
}

Scene *FriendSelectPageOpen(const IDParam &id, int32_t prioOffset)
{
	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = prioOffset;
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	Plugin::PageCloseParam closeParam;
	plugin->PageClose(id, closeParam);
	if (openParam.overwrite_draw_priority >= 0)
		openParam.overwrite_draw_priority =
			plugin->m_draw_priority - openParam.overwrite_draw_priority;
	return plugin->PageOpen(id, openParam);
}

void FriendSelectPageClose(const IDParam &id)
{
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	Plugin::PageCloseParam closeParam;
	plugin->PageClose(id, closeParam);
}

void FriendSelectPageCloseAll(void)
{
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	plugin->PageCloseAll();
}

void FriendSelectTemplateOpen(Widget *parent, const IDParam &id,
			      const Plugin::TemplateOpenParam &param)
{
	Plugin *plugin = (s_selectApp != NULL) ? s_selectApp->m_plugin : NULL;
	plugin->TemplateOpen(parent, id, param);
}

void FriendSelectDismissPopups(void)
{
	FriendLibPopupClose();
	FriendSelectErrorClose();
}

/* ========================================================================= */
/* InitTask & CloseTask                                                      */
/* ========================================================================= */

FriendSelectInitTask::FriendSelectInitTask() : FriendSelectTaskBase()
{
}

FriendSelectInitTask::~FriendSelectInitTask()
{
}

void FriendSelectInitTask::Start(Callback1<int32_t> *callback)
{
	FriendSelectTaskBase::Start(callback);
	FriendLibStartTask(new FriendLibGetSelfNpIdTask(
		&g_selfNpId,
		new TaskMemberCallback1<FriendSelectInitTask, int32_t>(
			this, &FriendSelectInitTask::OnNpReady)));
}

void FriendSelectInitTask::OnNpReady(int32_t result, void *arg)
{
	(void)arg;
	if (result == FRIEND_SELECT_ERROR_NP_OFFLINE) {
		m_result = FRIEND_SELECT_ERROR_NP_OFFLINE;
		Finish();
	} else if (result >= 0) {
		m_result = 0;
		Finish();
	} else {
		FriendSelectErrorShow(result,
				      new MemberCallback0<FriendSelectInitTask>(
					      this, &FriendSelectInitTask::OnErrorDismissed));
	}
}

void FriendSelectInitTask::OnErrorDismissed(void *arg)
{
	(void)arg;
	m_result = -3;
	Finish();
}

FriendSelectCloseTask::FriendSelectCloseTask() : FriendSelectTaskBase()
{
}

FriendSelectCloseTask::~FriendSelectCloseTask()
{
}

void FriendSelectCloseTask::Start(Callback1<int32_t> *callback)
{
	FriendSelectTaskBase::Start(callback);
	FriendLibCancelAllImages(new MemberCallback0<FriendSelectCloseTask>(
		this, &FriendSelectCloseTask::OnCanceled));
}

void FriendSelectCloseTask::Abort()
{
}

void FriendSelectCloseTask::OnCanceled(void *arg)
{
	(void)arg;
	m_result = 1;
	Finish();
}

/* ========================================================================= */
/* FriendSelectSinglePage                                                    */
/* ========================================================================= */

FriendSelectSinglePage::FriendSelectSinglePage()
	: FriendSelectPageBase(IDParam(RCO_PAGE_SINGLE)), m_header(), m_listView(NULL),
	  m_emptyText(NULL), m_restoreIndex(-1), m_itemFactory(NULL)
{
}

FriendSelectSinglePage::~FriendSelectSinglePage()
{
}

void FriendSelectSinglePage::OnPageCreated()
{
	FriendSelectHeaderParam hparam;
	hparam.showMultiControls = false;
	hparam.actionCallback = new MemberCallback1<FriendSelectSinglePage, int32_t>(
		this, &FriendSelectSinglePage::OnHeaderAction);
	m_header.Configure(hparam);

	m_listView = (ListView *)m_scene->FindChild(IDParam(RCO_LIST_VIEW), 0);
	m_emptyText = m_scene->FindChild(IDParam(RCO_EMPTY_TEXT), 0);

	m_itemFactory = new FriendSelectItemFactory(
		new MemberFuncCallback1<FriendSelectSinglePage, ListItem *, Widget *>(
			this, &FriendSelectSinglePage::CreateItem),
		new MemberCallback2<FriendSelectSinglePage, Widget *, int32_t>(
			this, &FriendSelectSinglePage::BindItem),
		new MemberCallback2<FriendSelectSinglePage, Widget *, int32_t>(
			this, &FriendSelectSinglePage::UnbindItem),
		new MemberCallback1<FriendSelectSinglePage, int32_t>(
			this, &FriendSelectSinglePage::OnSelectItem));
	m_listView->SetItemFactory(m_itemFactory);
	m_listView->InsertSegment(0, 1);
	m_emptyText->Hide(0xFF, NULL, NULL, 0.0f);

	FriendListCache &cache = s_singleData->m_listCache;
	m_header.SetSortMode(cache.m_sortMode);
	m_header.SetUpdating(true);
	FriendLibStartTask(new FriendListFetchJob(
		&cache, 0,
		new TaskMemberCallback1<FriendSelectSinglePage, int32_t>(
			this, &FriendSelectSinglePage::OnListFetched)));
}

void FriendSelectSinglePage::OnPageDestroyed()
{
	if (m_itemFactory != NULL) {
		m_itemFactory->DetachCallbacks();
		m_itemFactory = NULL;
	}
	FriendSelectPageBase::OnPageDestroyed();
}

void FriendSelectSinglePage::RefreshList()
{
	m_listView->BlockSync();
	int32_t total = s_singleData->m_listCache.m_totalResults;
	int32_t curNum = m_listView->GetCellNum(0);
	if (total != 0) {
		m_emptyText->Hide(0xFF, NULL, NULL, 0.0f);
		m_listView->Show(0xFF, NULL, NULL, 0.0f);
	} else {
		m_listView->Hide(0xFF, NULL, NULL, 0.0f);
		m_emptyText->Show(0xFF, NULL, NULL, 0.0f);
	}
	if (curNum < total)
		m_listView->InsertCell(0, curNum, (uint32_t)(total - curNum));
	else if (curNum > total)
		m_listView->DeleteCell(0, total, (uint32_t)(curNum - total));

	int32_t genNum = m_listView->GetGeneratedNum();
	for (int32_t i = 0; i < genNum; ++i) {
		listview::ListPos pos = m_listView->GetGeneratedPos(i);
		ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);
		if (item != NULL) {
			int32_t cellIdx = item->GetCellIndex();
			Widget *child = item->GetChild(0);
			BindItem(child, cellIdx, NULL);
		}
	}
	m_listView->UnblockSync();
}

void FriendSelectSinglePage::OnHeaderAction(int32_t action, void *arg)
{
	if (m_result != 0x7FFFFFFF)
		return;
	if (action == 1) {
		PopupListOpenArg openArg;
		openArg.mode = 1;
		openArg.plugin = FriendSelectGetPlugin();
		openArg.anchorWidget = FriendSelectHeaderGetButton(1);
		openArg.items.resize(2);
		const wchar_t *s0 = FriendSelectGetString(IDParam(RCO_STR_SORT_BY_ID));
		openArg.items[0].label = (s0 != NULL) ? s0 : L"";
		const wchar_t *s1 = FriendSelectGetString(IDParam(RCO_STR_SORT_BY_STATUS));
		openArg.items[1].label = (s1 != NULL) ? s1 : L"";
		openArg.selectedIndex = (s_singleData->m_listCache.m_sortMode != 0) ? 1 : 0;
		openArg.callback = new MemberCallback1<FriendSelectSinglePage, int32_t>(
			this, &FriendSelectSinglePage::OnSortMenuSelected);
		FriendLibPopupOpen(openArg);
		return;
	}
	if (action == 2) {
		FriendListCache &cache = s_singleData->m_listCache;
		m_header.SetUpdating(true);
		ListItem *topItem = m_listView->ScePafWidget_E744731A(0);
		m_restoreIndex = (topItem != NULL) ? topItem->GetCellIndex() : 0;
		int32_t cellNum = m_listView->GetCellNum(0);
		m_listView->DeleteCell(0, 0, (uint32_t)cellNum);
		cache.ClearAll();
		if (FriendLibIsTaskRunning()) {
			FriendLibCancelActiveTask();
		} else {
			FriendLibStartTask(new FriendListFetchJob(
				&cache, m_restoreIndex,
				new TaskMemberCallback1<FriendSelectSinglePage, int32_t>(
					this, &FriendSelectSinglePage::OnListFetched)));
		}
		return;
	}
	FriendSelectPageBase::OnHeaderAction(action, arg);
}

void FriendSelectSinglePage::OnClockChanged()
{
	if (m_header.IsUpdating())
		FriendLibCancelActiveTask();
	else
		OnHeaderAction(2, NULL);
}

ListItem *FriendSelectSinglePage::CreateItem(Widget *parent, void *arg)
{
	(void)arg;
	return FriendSelectCreateSingleItem(parent);
}

void FriendSelectSinglePage::BindItem(Widget *widget, int32_t index, void *arg)
{
	(void)arg;
	if (s_singleData == NULL)
		return;
	FriendListCache &cache = s_singleData->m_listCache;
	const FriendUserProfile &prof = cache.GetProfileAt(index);
	FriendSelectBindItemWidget(widget, prof);
	if (prof.m_relation == FRIEND_RELATION_NONE && !FriendLibIsTaskRunning()) {
		FriendLibStartTask(new FriendListFetchJob(
			&cache, index,
			new TaskMemberCallback1<FriendSelectSinglePage, int32_t>(
				this, &FriendSelectSinglePage::OnListFetched)));
	}
}

void FriendSelectSinglePage::UnbindItem(Widget *widget, int32_t index, void *arg)
{
	(void)arg;
	if (s_singleData != NULL) {
		const FriendUserProfile &prof = s_singleData->m_listCache.GetProfileAt(index);
		FriendSelectUnbindItemWidget(widget, prof);
	}
}

void FriendSelectSinglePage::OnSelectItem(int32_t index, void *arg)
{
	(void)arg;
	if (s_singleData != NULL && s_singleData->SelectAt(index) >= 0)
		ClosePage(0);
}

void FriendSelectSinglePage::OnListFetched(int32_t result, void *arg)
{
	(void)arg;
	if (result == -1) {
		int32_t targetIdx = m_restoreIndex;
		if (targetIdx < 0) {
			listview::ListPos pos = m_listView->GetGeneratedPos(0);
			ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);
			targetIdx = (item != NULL) ? item->GetCellIndex() : 0;
		}
		FriendListCache &cache = s_singleData->m_listCache;
		if (cache.GetProfileAt(targetIdx).m_relation == FRIEND_RELATION_NONE) {
			FriendLibStartTask(new FriendListFetchJob(
				&cache, targetIdx,
				new TaskMemberCallback1<FriendSelectSinglePage, int32_t>(
					this, &FriendSelectSinglePage::OnListFetched)));
			return;
		}
		result = 0;
	}
	m_header.SetUpdating(false);
	if (result >= 0) {
		RefreshList();
		if (m_restoreIndex >= 0) {
			m_listView->SetFocus(0, m_restoreIndex, (ListView::FocusAlignType)1, NULL);
			m_restoreIndex = -1;
		}
	} else {
		int32_t cellNum = m_listView->GetCellNum(0);
		m_listView->DeleteCell(0, 0, (uint32_t)cellNum);
		s_singleData->m_listCache.ClearAll();
		FriendSelectErrorShow(result, NULL);
	}
}

void FriendSelectSinglePage::OnSortMenuSelected(int32_t choice, void *arg)
{
	(void)arg;
	if (choice < 0)
		return;
	FriendListCache &cache = s_singleData->m_listCache;
	m_header.SetUpdating(true);
	int32_t cellNum = m_listView->GetCellNum(0);
	m_listView->DeleteCell(0, 0, (uint32_t)cellNum);
	int32_t sortMode = (choice != 0) ? 1 : 0;
	cache.SetSortMode(sortMode);
	m_restoreIndex = -1;
	m_header.SetSortMode(sortMode);
	if (FriendLibIsTaskRunning()) {
		FriendLibCancelActiveTask();
	} else {
		FriendLibStartTask(new FriendListFetchJob(
			&cache, 0,
			new TaskMemberCallback1<FriendSelectSinglePage, int32_t>(
				this, &FriendSelectSinglePage::OnListFetched)));
	}
}

/* ========================================================================= */
/* FriendSelectMultiPage                                                     */
/* ========================================================================= */

FriendSelectMultiPage::FriendSelectMultiPage()
	: FriendSelectPageBase(IDParam(RCO_PAGE_MULTI)), m_header(), m_listView(NULL),
	  m_emptyText(NULL), m_restoreIndex(-1), m_itemFactory(NULL)
{
}

FriendSelectMultiPage::~FriendSelectMultiPage()
{
}

void FriendSelectMultiPage::OnPageCreated()
{
	FriendSelectHeaderParam hparam;
	hparam.showMultiControls = true;
	hparam.actionCallback = new MemberCallback1<FriendSelectMultiPage, int32_t>(
		this, &FriendSelectMultiPage::OnHeaderAction);
	m_header.Configure(hparam);
	m_header.SetSelectedCount((int32_t)s_multiData->GetTotalSelectedCount());
	m_header.SetMaxCount((int32_t)s_multiData->m_maxTotal);

	m_listView = (ListView *)m_scene->FindChild(IDParam(RCO_LIST_VIEW), 0);
	m_emptyText = m_scene->FindChild(IDParam(RCO_EMPTY_TEXT), 0);

	m_itemFactory = new FriendSelectItemFactory(
		new MemberFuncCallback1<FriendSelectMultiPage, ListItem *, Widget *>(
			this, &FriendSelectMultiPage::CreateItem),
		new MemberCallback2<FriendSelectMultiPage, Widget *, int32_t>(
			this, &FriendSelectMultiPage::BindItem),
		new MemberCallback2<FriendSelectMultiPage, Widget *, int32_t>(
			this, &FriendSelectMultiPage::UnbindItem),
		new MemberCallback1<FriendSelectMultiPage, int32_t>(
			this, &FriendSelectMultiPage::OnToggleItem));
	m_listView->SetItemFactory(m_itemFactory);
	m_listView->InsertSegment(0, 1);
	m_listView->SetFocusDirectionWidget(1, FriendSelectHeaderGetButton(4));
	m_emptyText->Hide(0xFF, NULL, NULL, 0.0f);

	FriendListCache &cache = s_multiData->m_listCache;
	m_header.SetSortMode(cache.m_sortMode);
	m_header.SetUpdating(true);
	m_header.SetClearEnabled(false, false);
	m_header.SetOkEnabled(false, false);
	FriendLibStartTask(new FriendListFetchJob(
		&cache, 0,
		new TaskMemberCallback1<FriendSelectMultiPage, int32_t>(
			this, &FriendSelectMultiPage::OnListFetched)));
}

void FriendSelectMultiPage::OnPageDestroyed()
{
	if (m_itemFactory != NULL) {
		m_itemFactory->DetachCallbacks();
		m_itemFactory = NULL;
	}
	FriendSelectPageBase::OnPageDestroyed();
}

void FriendSelectMultiPage::RefreshList()
{
	m_listView->BlockSync();
	int32_t total = s_multiData->m_listCache.m_totalResults;
	int32_t curNum = m_listView->GetCellNum(0);
	if (total != 0) {
		m_emptyText->Hide(0xFF, NULL, NULL, 0.0f);
		m_listView->Show(0xFF, NULL, NULL, 0.0f);
	} else {
		s_multiData->ClearSelection();
		m_listView->Hide(0xFF, NULL, NULL, 0.0f);
		m_emptyText->Show(0xFF, NULL, NULL, 0.0f);
	}
	if (curNum < total)
		m_listView->InsertCell(0, curNum, (uint32_t)(total - curNum));
	else if (curNum > total)
		m_listView->DeleteCell(0, total, (uint32_t)(curNum - total));

	int32_t genNum = m_listView->GetGeneratedNum();
	for (int32_t i = 0; i < genNum; ++i) {
		listview::ListPos pos = m_listView->GetGeneratedPos(i);
		ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);
		if (item != NULL) {
			int32_t cellIdx = item->GetCellIndex();
			Widget *child = item->GetChild(0);
			BindItem(child, cellIdx, NULL);
		}
	}
	m_listView->UnblockSync();
}

void FriendSelectMultiPage::OnHeaderAction(int32_t action, void *arg)
{
	if (m_result != 0x7FFFFFFF)
		return;
	switch (action) {
	case 0:
		s_multiData->ClearSelection();
		ClosePage(-1);
		break;
	case 1: {
		PopupListOpenArg openArg;
		openArg.mode = 1;
		openArg.plugin = FriendSelectGetPlugin();
		openArg.anchorWidget = FriendSelectHeaderGetButton(1);
		openArg.items.resize(2);
		const wchar_t *s0 = FriendSelectGetString(IDParam(RCO_STR_SORT_BY_ID));
		openArg.items[0].label = (s0 != NULL) ? s0 : L"";
		const wchar_t *s1 = FriendSelectGetString(IDParam(RCO_STR_SORT_BY_STATUS));
		openArg.items[1].label = (s1 != NULL) ? s1 : L"";
		openArg.selectedIndex = (s_multiData->m_listCache.m_sortMode != 0) ? 1 : 0;
		openArg.callback = new MemberCallback1<FriendSelectMultiPage, int32_t>(
			this, &FriendSelectMultiPage::OnSortMenuSelected);
		FriendLibPopupOpen(openArg);
		break;
	}
	case 2: {
		FriendListCache &cache = s_multiData->m_listCache;
		m_header.SetUpdating(true);
		m_header.SetClearEnabled(false, false);
		m_header.SetOkEnabled(false, false);
		ListItem *topItem = m_listView->ScePafWidget_E744731A(0);
		m_restoreIndex = (topItem != NULL) ? topItem->GetCellIndex() : 0;
		int32_t cellNum = m_listView->GetCellNum(0);
		m_listView->DeleteCell(0, 0, (uint32_t)cellNum);
		cache.ClearAll();
		if (FriendLibIsTaskRunning()) {
			FriendLibCancelActiveTask();
		} else {
			FriendLibStartTask(new FriendListFetchJob(
				&cache, m_restoreIndex,
				new TaskMemberCallback1<FriendSelectMultiPage, int32_t>(
					this, &FriendSelectMultiPage::OnListFetched)));
		}
		break;
	}
	case 3: {
		s_multiData->ClearSelection();
		RefreshList();
		bool hasSel = (s_multiData->GetSelectedCount() != 0);
		m_header.SetClearEnabled(hasSel, true);
		m_header.SetOkEnabled(hasSel, true);
		m_header.SetSelectedCount((int32_t)s_multiData->GetTotalSelectedCount());
		break;
	}
	case 4:
		s_multiData->CommitSelection();
		ClosePage(0);
		break;
	default:
		FriendSelectPageBase::OnHeaderAction(action, arg);
		break;
	}
}

void FriendSelectMultiPage::OnClockChanged()
{
	if (m_header.IsUpdating())
		FriendLibCancelActiveTask();
	else
		OnHeaderAction(2, NULL);
}

ListItem *FriendSelectMultiPage::CreateItem(Widget *parent, void *arg)
{
	(void)arg;
	return FriendSelectCreateMultiItem(parent);
}

void FriendSelectMultiPage::BindItem(Widget *widget, int32_t index, void *arg)
{
	(void)arg;
	if (s_multiData == NULL)
		return;
	FriendListCache &cache = s_multiData->m_listCache;
	const FriendUserProfile &prof = cache.GetProfileAt(index);
	FriendSelectBindItemWidget(widget, prof);
	if (prof.m_relation == FRIEND_RELATION_NONE && !FriendLibIsTaskRunning()) {
		FriendLibStartTask(new FriendListFetchJob(
			&cache, index,
			new TaskMemberCallback1<FriendSelectMultiPage, int32_t>(
				this, &FriendSelectMultiPage::OnListFetched)));
	}
}

void FriendSelectMultiPage::UnbindItem(Widget *widget, int32_t index, void *arg)
{
	(void)arg;
	if (s_multiData != NULL) {
		const FriendUserProfile &prof = s_multiData->m_listCache.GetProfileAt(index);
		FriendSelectUnbindItemWidget(widget, prof);
	}
}

void FriendSelectMultiPage::OnToggleItem(int32_t index, void *arg)
{
	(void)arg;
	if (s_multiData == NULL)
		return;
	int32_t res = s_multiData->ToggleAt(index);
	if (res == FRIEND_SELECT_ERROR_NOMEM) {
		const FriendUserProfile &prof = s_multiData->m_listCache.GetProfileAt(index);
		ListItem *item = m_listView->GetListItem(0, index);
		if (item != NULL)
			FriendSelectBindItemWidget(item->GetChild(0), prof);
		FriendSelectErrorShow(FRIEND_SELECT_ERROR_MAX_SELECTED, NULL);
	} else if (res >= 0) {
		bool hasSel = (s_multiData->GetSelectedCount() != 0);
		m_header.SetClearEnabled(hasSel, true);
		m_header.SetOkEnabled(hasSel, true);
		m_header.SetSelectedCount((int32_t)s_multiData->GetTotalSelectedCount());
	}
}

void FriendSelectMultiPage::OnListFetched(int32_t result, void *arg)
{
	(void)arg;
	if (result == -1) {
		int32_t targetIdx = m_restoreIndex;
		if (targetIdx < 0) {
			listview::ListPos pos = m_listView->GetGeneratedPos(0);
			ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);
			targetIdx = (item != NULL) ? item->GetCellIndex() : 0;
		}
		FriendListCache &cache = s_multiData->m_listCache;
		if (cache.GetProfileAt(targetIdx).m_relation == FRIEND_RELATION_NONE) {
			FriendLibStartTask(new FriendListFetchJob(
				&cache, targetIdx,
				new TaskMemberCallback1<FriendSelectMultiPage, int32_t>(
					this, &FriendSelectMultiPage::OnListFetched)));
			return;
		}
		result = 0;
	}
	m_header.SetUpdating(false);
	if (result >= 0) {
		RefreshList();
		bool hasSel = (s_multiData->GetSelectedCount() != 0);
		m_header.SetClearEnabled(hasSel, true);
		m_header.SetOkEnabled(hasSel, true);
		m_header.SetSelectedCount((int32_t)s_multiData->GetTotalSelectedCount());
		if (m_restoreIndex >= 0) {
			m_listView->SetFocus(0, m_restoreIndex, (ListView::FocusAlignType)1, NULL);
			m_restoreIndex = -1;
		}
	} else {
		int32_t cellNum = m_listView->GetCellNum(0);
		m_listView->DeleteCell(0, 0, (uint32_t)cellNum);
		s_multiData->m_listCache.ClearAll();
		bool hasSel = (s_multiData->GetSelectedCount() != 0);
		m_header.SetClearEnabled(hasSel, true);
		m_header.SetOkEnabled(hasSel, true);
		m_header.SetSelectedCount((int32_t)s_multiData->GetTotalSelectedCount());
		FriendSelectErrorShow(result, NULL);
	}
}

void FriendSelectMultiPage::OnSortMenuSelected(int32_t choice, void *arg)
{
	(void)arg;
	if (choice < 0)
		return;
	FriendListCache &cache = s_multiData->m_listCache;
	m_header.SetUpdating(true);
	int32_t cellNum = m_listView->GetCellNum(0);
	m_listView->DeleteCell(0, 0, (uint32_t)cellNum);
	m_header.SetClearEnabled(false, false);
	m_header.SetOkEnabled(false, false);
	int32_t sortMode = (choice != 0) ? 1 : 0;
	cache.SetSortMode(sortMode);
	m_restoreIndex = -1;
	m_header.SetSortMode(sortMode);
	if (FriendLibIsTaskRunning()) {
		FriendLibCancelActiveTask();
	} else {
		FriendLibStartTask(new FriendListFetchJob(
			&cache, 0,
			new TaskMemberCallback1<FriendSelectMultiPage, int32_t>(
				this, &FriendSelectMultiPage::OnListFetched)));
	}
}

/* ========================================================================= */
/* Session data objects (FriendSelectSingleData, FriendSelectMultiData)      */
/* ========================================================================= */

FriendSelectSingleData::FriendSelectSingleData() : m_listCache(), m_outputNpId(NULL)
{
}

FriendSelectSingleData::~FriendSelectSingleData()
{
}

void FriendSelectSingleData::SetOutputBuffer(SceNpId *dst)
{
	m_outputNpId = dst;
	if (dst != NULL)
		sce_paf_memset(dst, 0, sizeof(SceNpId));
}

int32_t FriendSelectSingleData::SelectAt(int32_t index)
{
	const FriendUserProfile &prof = m_listCache.GetProfileAt(index);
	if (prof.m_relation == FRIEND_RELATION_NONE)
		return FRIEND_SELECT_ERROR_NOT_READY;
	if (m_outputNpId != NULL)
		sce_paf_memcpy(m_outputNpId, &prof.m_npId, sizeof(SceNpId));
	return 0;
}

FriendSelectMultiData::~FriendSelectMultiData()
{
}

void FriendSelectMultiData::SetExternalParam(FriendSelectMultiExternalParam *param)
{
	m_maxTotal = param->maxTotal;
	m_existingList = &param->existingList;
	m_outputList = &param->selectedList;
	uint32_t existingCount = (uint32_t)param->existingList.size();
	m_maxSelectable = (m_maxTotal > existingCount) ? (m_maxTotal - existingCount) : 0;
	m_selected.clear();
	if (m_outputList != NULL) {
		list<SceNpId> copy(*m_outputList);
		m_selected.swap(copy);
	}
}

namespace paf {
template <>
inline __attribute__((always_inline)) list<SceNpId, allocator<SceNpId> >::iterator
list<SceNpId, allocator<SceNpId> >::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;
	node->next->prev = node->prev;
	node->prev->next = node->next;
	destroy(&node->value);
	operator delete(node);
	--count;
	return iterator(next);
}
}

int32_t FriendSelectMultiData::ToggleAt(int32_t index)
{
	const FriendUserProfile &prof = m_listCache.GetProfileAt(index);
	if (prof.m_relation == FRIEND_RELATION_NONE)
		return FRIEND_SELECT_ERROR_NOT_READY;
	for (list<SceNpId>::iterator it = m_selected.begin(); it != m_selected.end(); ++it) {
		if (prof.MatchesNpId(*it)) {
			m_selected.erase(it);
			return 0;
		}
	}
	if (m_selected.size() < m_maxSelectable) {
		m_selected.push_back(prof.m_npId);
		return 1;
	}
	return FRIEND_SELECT_ERROR_NOMEM;
}

void FriendSelectMultiData::ClearSelection()
{
	m_selected.clear();
}

void FriendSelectMultiData::CommitSelection()
{
	if (m_outputList != NULL)
		m_outputList->swap(m_selected);
}

uint32_t FriendSelectMultiData::GetSelectedCount() const
{
	return (uint32_t)m_selected.size();
}

uint32_t FriendSelectMultiData::GetTotalSelectedCount() const
{
	return (m_maxTotal - m_maxSelectable) + (uint32_t)m_selected.size();
}

bool FriendSelectMultiData::IsExisting(const FriendUserProfile &prof)
{
	if (prof.m_relation == FRIEND_RELATION_NONE || m_existingList == NULL)
		return false;
	for (list<SceNpId>::iterator it = m_existingList->begin(); it != m_existingList->end();
	     ++it) {
		if (prof.MatchesNpId(*it))
			return true;
	}
	return false;
}

bool FriendSelectMultiData::IsSelected(const FriendUserProfile &prof)
{
	if (prof.m_relation == FRIEND_RELATION_NONE)
		return false;
	for (list<SceNpId>::iterator it = m_selected.begin(); it != m_selected.end(); ++it) {
		if (prof.MatchesNpId(*it))
			return true;
	}
	return false;
}

void FriendSelectSingleDataCreate(void)
{
	if (s_singleData == NULL)
		s_singleData = new FriendSelectSingleData();
}

void FriendSelectSingleDataDestroy(void)
{
	if (s_singleData != NULL) {
		delete s_singleData;
		s_singleData = NULL;
	}
}

FriendSelectSingleData *FriendSelectGetSingleData(void)
{
	return s_singleData;
}

void FriendSelectMultiDataCreate(void)
{
	if (s_multiData == NULL)
		s_multiData = new FriendSelectMultiData();
}

void FriendSelectMultiDataDestroy(void)
{
	if (s_multiData != NULL) {
		delete s_multiData;
		s_multiData = NULL;
	}
}

FriendSelectMultiData *FriendSelectGetMultiData(void)
{
	return s_multiData;
}

uint32_t FriendSelectGetMultiMaxTotal(void)
{
	return (s_multiData != NULL) ? s_multiData->m_maxTotal : 0;
}

bool FriendSelectMultiIsSelected(const FriendUserProfile &profile)
{
	return (s_multiData != NULL) && s_multiData->IsSelected(profile);
}

void FriendSelectSetTransitionMode(int32_t mode)
{
	s_transitionMode = mode;
}

/* ========================================================================= */
/* FriendSelectTaskBase & FriendSelectPageBase                               */
/* ========================================================================= */

FriendSelectTaskBase::FriendSelectTaskBase()
	: m_callback(NULL), m_result(0x7FFFFFFF), m_closing(false)
{
}

FriendSelectTaskBase::~FriendSelectTaskBase()
{
}

void FriendSelectTaskBase::Start(Callback1<int32_t> *callback)
{
	m_callback = callback;
}

void FriendSelectTaskBase::Finish()
{
	if (FriendLibIsTaskRunning()) {
		FriendLibCancelActiveTask();
		return;
	}
	FriendSelectDismissPopups();
	if (m_callback != NULL) {
		Callback1<int32_t> *cb = m_callback;
		int32_t res = m_result;
		m_callback = NULL;
		cb->Invoke(res);
		delete cb;
	}
}

void FriendSelectTaskBase::Abort()
{
	m_result = -2;
	Finish();
}

FriendSelectPageBase::FriendSelectPageBase(const IDParam &pageId)
	: FriendSelectTaskBase(), m_pageId(pageId), m_scene(NULL)
{
}

FriendSelectPageBase::~FriendSelectPageBase()
{
}

void FriendSelectPageBase::Start(Callback1<int32_t> *callback)
{
	FriendSelectTaskBase::Start(callback);
	OpenPage();
}

void FriendSelectPageBase::Finish()
{
	if (FriendLibHasPendingImages()) {
		FriendLibCancelAllImages(new MemberCallback0<FriendSelectPageBase>(
			this, &FriendSelectPageBase::OnImagesCanceled));
		return;
	}
	FriendSelectTaskBase::Finish();
}

void FriendSelectPageBase::Abort()
{
	if (m_result == 0x7FFFFFFF) {
		ClosePage(-2);
		return;
	}
	m_result = -2;
}

void FriendSelectPageBase::OpenPage()
{
	m_scene = FriendSelectPageOpen(m_pageId, 2);
	OnPageCreated();
	FriendLibClockRegister(new MemberClockListener<FriendSelectPageBase>(
		this, &FriendSelectPageBase::OnClockChanged));
	if (s_transitionMode == 1) {
		FriendSelectBgShow();
		FriendSelectHeaderBarShow(1);
		ScePafCommon_6CF3A3C8(0.0f, m_scene, (transition::Type)3, false, false);
	}
}

void FriendSelectPageBase::ClosePage(int32_t result)
{
	m_result = result;
	m_closing = true;
	FriendLibCancelAllImages(NULL);
	FriendLibClockUnregister(this);
	Lock(127);
	OnPageClosing();
	BeginCloseTransition();
}

void FriendSelectPageBase::OnPageCreated()
{
}

void FriendSelectPageBase::OnPageDestroyed()
{
	m_scene->KillTimer(0x1000000);
	FriendSelectPageClose(m_pageId);
	m_scene = NULL;
	Unlock(127);
	m_closing = false;
	Finish();
}

void FriendSelectPageBase::OnPageClosing()
{
	s_transitionMode = (m_result == -2) ? 0 : 2;
}

void FriendSelectPageBase::OnHeaderAction(int32_t action, void *arg)
{
	(void)arg;
	if (m_result == 0x7FFFFFFF) {
		if (action == 4)
			ClosePage(0);
		else if (action == 0)
			ClosePage(-1);
	}
}

void FriendSelectPageBase::OnClockChanged()
{
}

void FriendSelectPageBase::BeginCloseTransition()
{
	float duration = 0.0f;
	if (s_transitionMode == 2) {
		FriendSelectBgHide();
		FriendSelectHeaderBarHide(2);
		duration = ScePafCommon_B48C9BDC(0.0f, m_scene, (transition::Type)3, false, false);
	}
	if (duration > 0.0f) {
		m_scene->SetTimerListener(
			Handler::CB_TIMEOUT, duration,
			new MemberEventListener<FriendSelectPageBase>(
				this, &FriendSelectPageBase::OnCloseTimerDone, NULL));
	} else {
		OnPageDestroyed();
	}
}

void FriendSelectPageBase::OnCloseTimerDone(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	OnPageDestroyed();
}

void FriendSelectPageBase::OnImagesCanceled(void *arg)
{
	(void)arg;
	Finish();
}
