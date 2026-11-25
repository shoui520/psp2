/*
 * SceFriendList2DialogPlugin: SceFriendSelect embedded plugin implementation
 * (single-select & multi-select friend picker pages, WebAPI list fetch job,
 *  background dimmer, header bar, and CommonGuiDialog error presentation).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <np/np_basic.h>
#include <std/list>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <thread/thread.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>
#include <widget/w_button.h>
#include <widget/w_list_view.h>
#include <widget/w_scene.h>
#include "../msg_dialog/msg_dialog_util.h"
#include "friend_select_plugin.h"
#include "friend_select_webapi.h"
#include "friend_lib.h"
#include "popup_list_view.h"

extern "C" {
int SceCommonGuiDialog_20B141F1(void *param);
int SceCommonGuiDialog_696DB9C9(void);
int SceCommonGuiDialog_BC311DC0(void);
}

using namespace paf;
using namespace paf::graph;
using namespace paf::thread;
using namespace paf::ui;

#define FRIEND_SELECT_DEFAULT_BG_COLOR		0xFF372C28U
#define FRIEND_SELECT_ERROR_NOMEM		-2136014814	/* 0x80AE0022 */
#define FRIEND_SELECT_ERROR_BUSY		-2136014815	/* 0x80AE0021 */
#define FRIEND_SELECT_ERROR_NP_OFFLINE		-2141902585	/* 0x80552907 */

/* Resource ID hashes in friend_select_plugin.rco */
#define RCO_ID_BG_PAGE				0x9573432EU
#define RCO_ID_BG_PLANE				0x80F8C348U
#define RCO_ID_SINGLE_PAGE			0xA50AC905U
#define RCO_ID_LIST_VIEW			0x38B62AC4U
#define RCO_ID_EMPTY_TEXT			0x19F33FA1U
#define RCO_ID_SORT_BY_STATUS_STR		0x4001387FU
#define RCO_ID_SORT_BY_ID_STR			0x113FB4D5U
#define RCO_ID_CHECK_BOX			0xB9804AB3U
#define RCO_ID_GAME_ICON_PLANE			0xA80D9F6CU

/* Forward declarations */
class FriendSelectApp;
class FriendSelectStateMachine;

static FriendSelectApp *s_selectApp = NULL;

/* Single-select session data */
struct FriendSelectSingleData {
	FriendSelectSingleData() : outputNpId(NULL), listCache() {}

	SceNpId *outputNpId;
	FriendListCache listCache;
};

/* Multi-select session data */
struct FriendSelectMultiData {
	FriendSelectMultiData() : externalParam(NULL), maxSelectable(0), listCache() {}

	void *externalParam;
	uint32_t maxSelectable;
	FriendListCache listCache;
};

static FriendSelectSingleData *s_singleData = NULL;
static FriendSelectMultiData *s_multiData = NULL;

/* Background plane manager */
class FriendSelectBackground {
public:
	explicit FriendSelectBackground(Plugin *plugin, uint32_t bgColor)
		: m_plugin(plugin), m_page(NULL)
	{
		Plugin::PageOpenParam openParam;

		openParam.option = Plugin::PageOption_Create;
		m_page = m_plugin->PageOpen(IDParam(RCO_ID_BG_PAGE), openParam);
		if (m_page != NULL) {
			Widget *bgPlane = m_page->FindChild(IDParam(RCO_ID_BG_PLANE), 0);

			if (bgPlane != NULL) {
				math::v4 color((float)(bgColor & 0xFFU) / 255.0f,
					       (float)((bgColor >> 8) & 0xFFU) / 255.0f,
					       (float)((bgColor >> 16) & 0xFFU) / 255.0f,
					       (float)((bgColor >> 24) & 0xFFU) / 255.0f);
				bgPlane->SetColor(color, NULL, Widget::ANIM_COLOR, 0, 0.0f, NULL,
						  NULL);
			}
		}
	}

	virtual ~FriendSelectBackground()
	{
		if (m_plugin != NULL && m_page != NULL) {
			Plugin::PageCloseParam closeParam;

			m_plugin->PageClose(IDParam(RCO_ID_BG_PAGE), closeParam);
			m_page = NULL;
		}
	}

	Plugin *m_plugin;
	Scene *m_page;
};

static FriendSelectBackground *s_background = NULL;

/* FriendList WebAPI GET job */
class FriendListFetchJob : public FriendSelectWebApiJob {
public:
	FriendListFetchJob(FriendListCache *cache, int32_t startIndex,
			   FriendLibTaskCallback *callback)
		: FriendSelectWebApiJob(callback), m_cache(cache),
		  m_alignedOffset((startIndex / 20) * 20)
	{
		if (m_cache != NULL && m_cache->BeginFetch() < 0)
			m_cache = NULL;
	}

	virtual ~FriendListFetchJob() {}

	class PageJsonReceiver : public FriendSelectJsonReceiver {
	public:
		explicit PageJsonReceiver(FriendListCache *cache) : m_cache(cache) {}
		virtual ~PageJsonReceiver() {}

		virtual int InitParser(void *parserCtx)
		{
			if (m_cache == NULL || m_cache->m_fetchingPage == NULL)
				return FRIEND_SELECT_ERROR_BUSY;
			return m_cache->m_fetchingPage->InitJsonParser(parserCtx, 20U);
		}

		FriendListCache *m_cache;
	};

	virtual int Run()
	{
		if (m_cache == NULL)
			return FRIEND_SELECT_ERROR_BUSY;

		const char *sortParam =
			(m_cache->m_sortByStatus != 0) ? "onlineStatus%2BonlineId" : "onlineId";
		char pathBuf[256];

		sce_paf_snprintf(
			pathBuf, sizeof(pathBuf),
			"/v1/users/%s/friendList?friendStatus=friend&fields=npId,avatarUrls,"
			"panelBgc,panelUrl,plus,trophySummary&offset=%d&limit=%d&sort=%s"
			"&avatarSizes=n&presenceType=primary",
			FriendLibGetSelfOnlineId(), m_alignedOffset, 20, sortParam);

		return ExecuteGetRequest("userProfile", pathBuf, new PageJsonReceiver(m_cache));
	}

	virtual void NotifyCallback(int result)
	{
		if (m_cache != NULL)
			m_cache->EndFetch(result);
		FriendLibAsyncTask::NotifyCallback(result);
	}

	FriendListCache *m_cache;
	int32_t m_alignedOffset;
};

/* Base state node in SceFriendSelect state machine */
class FriendSelectState {
public:
	FriendSelectState() : m_plugin(NULL), m_page(NULL), m_callback(NULL), m_result(0) {}
	virtual ~FriendSelectState() {}

	virtual void Enter(FriendLibTaskCallback *callback) = 0;
	virtual void Abort() = 0;
	virtual FriendSelectState *NextState(int &resultCode) = 0;

	Plugin *m_plugin;
	Scene *m_page;
	FriendLibTaskCallback *m_callback;
	int m_result;
};

/* Single-select friend list page state */
class FriendSelectSingleState : public FriendSelectState {
public:
	explicit FriendSelectSingleState(Plugin *plugin)
		: m_listView(NULL), m_emptyText(NULL), m_scrollIndex(-1)
	{
		m_plugin = plugin;
	}

	virtual ~FriendSelectSingleState()
	{
		ClosePage();
	}

	void ClosePage()
	{
		if (m_plugin != NULL && m_page != NULL) {
			Plugin::PageCloseParam closeParam;

			m_plugin->PageClose(IDParam(RCO_ID_SINGLE_PAGE), closeParam);
			m_page = NULL;
			m_listView = NULL;
			m_emptyText = NULL;
		}
	}

	void FinishWithResult(int resultCode)
	{
		m_result = resultCode;
		ClosePage();
		if (m_callback != NULL) {
			FriendLibTaskCallback *cb = m_callback;

			m_callback = NULL;
			cb->OnFinished(resultCode);
			delete cb;
		}
	}

	class FetchDoneCallback : public FriendLibTaskCallback {
	public:
		explicit FetchDoneCallback(FriendSelectSingleState *owner) : m_owner(owner) {}
		virtual ~FetchDoneCallback() {}

		virtual void OnFinished(int result)
		{
			m_owner->OnListFetched(result);
		}

		FriendSelectSingleState *m_owner;
	};

	class SortSelectCallback : public PopupListCallback {
	public:
		explicit SortSelectCallback(FriendSelectSingleState *owner) : m_owner(owner) {}
		virtual ~SortSelectCallback() {}

		virtual void OnSelect(int32_t selectedIndex)
		{
			m_owner->OnSortSelected(selectedIndex);
		}

		FriendSelectSingleState *m_owner;
	};

	void RefreshVisibleList()
	{
		if (m_listView == NULL || s_singleData == NULL)
			return;
		int32_t total = s_singleData->listCache.m_totalResults;

		if (total < 0)
			total = 0;
		if (m_emptyText != NULL) {
			if (total == 0)
				m_emptyText->Show(0, NULL, NULL, NULL);
			else
				m_emptyText->Hide(0, NULL, NULL, NULL);
		}
	}

	void OnListFetched(int result)
	{
		if (result == -1) {
			int32_t targetIdx = (m_scrollIndex >= 0) ? m_scrollIndex : 0;

			if (s_singleData != NULL &&
			    s_singleData->listCache.GetProfileAt(targetIdx)->relation ==
				    FRIEND_RELATION_NONE) {
				FriendLibStartTask(new FriendListFetchJob(
					&s_singleData->listCache, targetIdx,
					new FetchDoneCallback(this)));
				return;
			}
			result = 0;
		}
		if (result >= 0) {
			RefreshVisibleList();
			m_scrollIndex = -1;
		} else if (s_singleData != NULL) {
			s_singleData->listCache.ClearAll();
#ifndef NP_MESSAGE_DIALOG_NO_COMMON_GUI_DIALOG
			SceCommonGuiDialog_696DB9C9();
#endif
		}
	}

	void OnSortSelected(int32_t selectedIndex)
	{
		if (selectedIndex < 0 || s_singleData == NULL)
			return;
		bool byStatus = (selectedIndex != 0);

		s_singleData->listCache.SetSortByStatus(byStatus);
		m_scrollIndex = -1;
		if (FriendLibIsTaskRunning()) {
			FriendLibCancelActiveTask();
		} else {
			FriendLibStartTask(new FriendListFetchJob(
				&s_singleData->listCache, 0, new FetchDoneCallback(this)));
		}
	}

	void OpenSortMenu()
	{
		if (m_plugin == NULL || s_singleData == NULL)
			return;
		PopupListOpenParam openParam;

		openParam.depth = 1;
		openParam.plugin = m_plugin;
		openParam.selectedIndex = (s_singleData->listCache.m_sortByStatus != 0) ? 1 : 0;
		openParam.callback = new SortSelectCallback(this);

		PopupListEntry byId;
		const wchar_t *idStr = m_plugin->GetString(IDParam(RCO_ID_SORT_BY_ID_STR));

		if (idStr != NULL)
			byId.label = idStr;
		openParam.items.push_back(byId);

		PopupListEntry byStatus;
		const wchar_t *stStr = m_plugin->GetString(IDParam(RCO_ID_SORT_BY_STATUS_STR));

		if (stStr != NULL)
			byStatus.label = stStr;
		openParam.items.push_back(byStatus);

		PopupListOpen(openParam);
	}

	void SelectFriendAt(int32_t index)
	{
		if (s_singleData == NULL || s_singleData->outputNpId == NULL)
			return;
		const FriendUserProfile *prof = s_singleData->listCache.GetProfileAt(index);

		if (prof != NULL && prof->relation != FRIEND_RELATION_NONE) {
			sce_paf_memcpy(s_singleData->outputNpId, &prof->npId, sizeof(SceNpId));
			FinishWithResult(0);
		}
	}

	virtual void Enter(FriendLibTaskCallback *callback)
	{
		m_callback = callback;
		Plugin::PageOpenParam openParam;

		openParam.option = Plugin::PageOption_Create;
		m_page = m_plugin->PageOpen(IDParam(RCO_ID_SINGLE_PAGE), openParam);
		if (m_page != NULL) {
			m_listView = (ListView *)m_page->FindChild(IDParam(RCO_ID_LIST_VIEW), 0);
			m_emptyText = m_page->FindChild(IDParam(RCO_ID_EMPTY_TEXT), 0);
		}
		if (s_singleData != NULL) {
			FriendLibStartTask(new FriendListFetchJob(
				&s_singleData->listCache, 0, new FetchDoneCallback(this)));
		}
	}

	virtual void Abort()
	{
		if (FriendLibIsTaskRunning())
			FriendLibCancelActiveTask();
		PopupListClose();
		FinishWithResult(-2);
	}

	virtual FriendSelectState *NextState(int &resultCode)
	{
		(void)resultCode;
		return NULL;
	}

	ListView *m_listView;
	Widget *m_emptyText;
	int32_t m_scrollIndex;
};

/* Initial NP login check state */
class FriendSelectInitState : public FriendSelectState {
public:
	FriendSelectInitState(Plugin *plugin, int32_t mode) : m_mode(mode)
	{
		m_plugin = plugin;
	}

	virtual ~FriendSelectInitState() {}

	class NpDoneCallback : public FriendLibTaskCallback {
	public:
		explicit NpDoneCallback(FriendSelectInitState *owner) : m_owner(owner) {}
		virtual ~NpDoneCallback() {}

		virtual void OnFinished(int result)
		{
			if (result >= 0)
				FriendLibSetSelfOnlineId(m_owner->m_selfNpId.handle.data);
			if (m_owner->m_callback != NULL) {
				FriendLibTaskCallback *cb = m_owner->m_callback;

				m_owner->m_callback = NULL;
				cb->OnFinished(result);
				delete cb;
			}
		}

		FriendSelectInitState *m_owner;
	};

	virtual void Enter(FriendLibTaskCallback *callback)
	{
		m_callback = callback;
		sce_paf_memset(&m_selfNpId, 0, sizeof(m_selfNpId));
		FriendLibStartTask(new FriendLibNpJob(&m_selfNpId, new NpDoneCallback(this)));
	}

	virtual void Abort()
	{
		if (FriendLibIsTaskRunning())
			FriendLibCancelActiveTask();
	}

	virtual FriendSelectState *NextState(int &resultCode)
	{
		if (resultCode != 0)
			return NULL;
		if (m_mode == 0 || m_mode == 1)
			return new FriendSelectSingleState(m_plugin);
		return NULL;
	}

	int32_t m_mode;
	SceNpId m_selfNpId;
};

/* SceFriendSelect state machine controller */
class FriendSelectStateMachine {
public:
	FriendSelectStateMachine(Plugin *plugin, int32_t mode, uint32_t bgColor)
		: m_plugin(plugin), m_mode(mode), m_bgColor(bgColor), m_doneCallback(NULL),
		  m_currentState(NULL), m_finalResult(0)
	{
	}

	virtual ~FriendSelectStateMachine()
	{
		if (m_currentState != NULL) {
			delete m_currentState;
			m_currentState = NULL;
		}
		if (s_background != NULL) {
			delete s_background;
			s_background = NULL;
		}
		if (s_singleData != NULL) {
			delete s_singleData;
			s_singleData = NULL;
		}
		if (s_multiData != NULL) {
			delete s_multiData;
			s_multiData = NULL;
		}
	}

	class StateDoneCallback : public FriendLibTaskCallback {
	public:
		explicit StateDoneCallback(FriendSelectStateMachine *sm) : m_sm(sm) {}
		virtual ~StateDoneCallback() {}

		virtual void OnFinished(int result)
		{
			m_sm->OnStateFinished(result);
		}

		FriendSelectStateMachine *m_sm;
	};

	void Finish(int resultCode)
	{
		if (m_doneCallback != NULL) {
			FriendLibTaskCallback *cb = m_doneCallback;

			m_doneCallback = NULL;
			cb->OnFinished(resultCode);
			delete cb;
		}
	}

	void OnStateFinished(int resultCode)
	{
		FriendSelectState *next = NULL;

		if (m_currentState != NULL) {
			next = m_currentState->NextState(resultCode);
			delete m_currentState;
			m_currentState = NULL;
		}
		if (next == NULL) {
			m_finalResult = resultCode;
			Finish(m_finalResult);
			return;
		}
		m_currentState = next;
		m_currentState->Enter(new StateDoneCallback(this));
	}

	void Start(FriendLibTaskCallback *doneCallback)
	{
		m_doneCallback = doneCallback;

		bool hasSort = false;
		Plugin::Argument sortArg = m_plugin->GetArg("sort", &hasSort);
		bool sortByStatus = hasSort && ((int32_t)sortArg == 1);

		if (m_mode == 0) {
			bool hasNpId = false;
			Plugin::Argument npIdArg = m_plugin->GetArg("npid", &hasNpId);
			SceNpId *npIdPtr =
				hasNpId ? (SceNpId *)(uintptr_t)(int32_t)npIdArg : NULL;

			if (npIdPtr == NULL) {
				Finish(-4);
				return;
			}
			s_singleData = new FriendSelectSingleData();
			s_singleData->outputNpId = npIdPtr;
			s_singleData->listCache.SetSortByStatus(sortByStatus);
		} else if (m_mode == 1) {
			bool hasMulti = false;
			Plugin::Argument multiArg = m_plugin->GetArg("multi_data", &hasMulti);
			void *multiPtr = hasMulti ? (void *)(uintptr_t)(int32_t)multiArg : NULL;

			if (multiPtr == NULL) {
				Finish(-4);
				return;
			}
			s_multiData = new FriendSelectMultiData();
			s_multiData->externalParam = multiPtr;
			s_multiData->listCache.SetSortByStatus(sortByStatus);
		} else {
			Finish(-4);
			return;
		}

		s_background = new FriendSelectBackground(m_plugin, m_bgColor);
		m_currentState = new FriendSelectInitState(m_plugin, m_mode);
		m_currentState->Enter(new StateDoneCallback(this));
	}

	void Abort()
	{
		if (m_currentState != NULL)
			m_currentState->Abort();
	}

	Plugin *m_plugin;
	int32_t m_mode;
	uint32_t m_bgColor;
	FriendLibTaskCallback *m_doneCallback;
	FriendSelectState *m_currentState;
	int m_finalResult;
};

/* Top-level SceFriendSelect application object */
class FriendSelectApp {
public:
	explicit FriendSelectApp(Plugin *plugin)
		: m_plugin(plugin), m_stateMachine(NULL), m_mutex("SceFriendSelect", 0),
		  m_mode(-1), m_bgColor(FRIEND_SELECT_DEFAULT_BG_COLOR)
	{
		bool foundMode = false;
		Plugin::Argument modeArg = m_plugin->GetArg("mode", &foundMode);

		m_mode = foundMode ? (int32_t)modeArg : -1;

		bool foundBg = false;
		Plugin::Argument bgArg = m_plugin->GetArg("bg_color", &foundBg);

		m_bgColor = foundBg ? (uint32_t)(int32_t)bgArg : FRIEND_SELECT_DEFAULT_BG_COLOR;
	}

	virtual ~FriendSelectApp()
	{
		if (m_stateMachine != NULL) {
			delete m_stateMachine;
			m_stateMachine = NULL;
		}
	}

	class DoneCallback : public FriendLibTaskCallback {
	public:
		explicit DoneCallback(FriendSelectApp *app) : m_app(app) {}
		virtual ~DoneCallback() {}

		virtual void OnFinished(int result)
		{
			m_app->OnComplete(result);
		}

		FriendSelectApp *m_app;
	};

	void OnComplete(int result)
	{
		if (m_stateMachine != NULL) {
			delete m_stateMachine;
			m_stateMachine = NULL;
		}
		int notifyCode = result;

		if (result == FRIEND_SELECT_ERROR_NP_OFFLINE)
			notifyCode = -5;
		else if (result == 0 || result == -1 || result == -2 || result == -3)
			notifyCode = result;

		m_plugin->Notify(notifyCode);
		m_mutex.Unlock();
	}

	void Start()
	{
		m_mutex.Lock();
		m_stateMachine = new FriendSelectStateMachine(m_plugin, m_mode, m_bgColor);
		if (m_stateMachine == NULL) {
			OnComplete(FRIEND_SELECT_ERROR_NOMEM);
			return;
		}
		m_stateMachine->Start(new DoneCallback(this));
	}

	void Abort()
	{
		if (m_stateMachine != NULL)
			m_stateMachine->Abort();
	}

	Plugin *m_plugin;
	FriendSelectStateMachine *m_stateMachine;
	Mutex m_mutex;
	int32_t m_mode;
	uint32_t m_bgColor;
};

static void FriendSelectAbortApi(void)
{
	if (s_selectApp != NULL)
		s_selectApp->Abort();
}

static FriendSelectInterface s_friendSelectInterface = { FriendSelectAbortApi };

void FriendSelectSetParam(Plugin::InitParam2 *param)
{
	FriendLibRegisterWidgetFactory();
	param->draw_priority = 100;
}

void FriendSelectInit(Plugin *plugin)
{
	plugin->SetInterface(1, &s_friendSelectInterface);
	FriendLibInit(plugin->m_private_heap, plugin->m_surface_pool);
	s_selectApp = new FriendSelectApp(plugin);
	FriendSelectWebApiInit();
}

void FriendSelectStart(Plugin *plugin)
{
	(void)plugin;
	FriendLibStart();
	if (s_selectApp != NULL)
		s_selectApp->Start();
}

void FriendSelectStop(Plugin *plugin)
{
	(void)plugin;
	if (s_selectApp != NULL)
		s_selectApp->Abort();
	FriendLibStop();
}

void FriendSelectExit(Plugin *plugin)
{
	(void)plugin;
	if (s_selectApp != NULL) {
		s_selectApp->m_mutex.Lock();
		s_selectApp->m_mutex.Unlock();
	}
	FriendSelectWebApiTerm();
	if (s_selectApp != NULL) {
		delete s_selectApp;
		s_selectApp = NULL;
	}
	FriendLibTerm();
	FriendLibUnregisterWidgetFactory();
}
