/*
 * SceFriendSelect (vs0:sys/external/friend_select.suprx):
 * Internal architecture shared across the SceFriendSelect, SceFriendLib,
 * and PopupListView translation units.
 */
#ifndef SYS_USERMODE_FRIEND_SELECT_INTERNAL_H
#define SYS_USERMODE_FRIEND_SELECT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <np/np_basic.h>
#include <rtc.h>
#include <std/list>
#include <std/string>
#include <std/vector>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <job/job.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <memory/heap_allocator.h>
#include <plugin.h>
#include <thread/thread.h>
#include <url/url.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_plane_button.h>
#include <widget/w_scene.h>

/* Error codes returned by SceFriendSelect / SceFriendLib */
#define FRIEND_SELECT_ERROR_BUSY		(-2136014815)
#define FRIEND_SELECT_ERROR_NOMEM		(-2136014814)
#define FRIEND_SELECT_ERROR_NOT_READY		(-2136014811)
#define FRIEND_SELECT_ERROR_INVALID_ARG		(-2136014705)
#define FRIEND_SELECT_ERROR_LOCAL_FILE		(-2135994367)
#define FRIEND_SELECT_ERROR_HTTP_STATUS		(-2135994284)
#define FRIEND_SELECT_ERROR_NP_OFFLINE		(-2141902585)
#define FRIEND_SELECT_ERROR_MAX_SELECTED	(-2130706431)

#define FRIEND_SELECT_DEFAULT_BG_COLOR		0xFF372728U
#define FRIEND_SELECT_DEFAULT_PANEL_COLOR	0xFF989898U
#define FRIEND_SELECT_WEBAPI_SYSMODULE_ID	0x53U

/* Resource ID hashes in friend_select_plugin.rco */
#define RCO_PAGE_BG				0x9573096EU
#define RCO_PLANE_BG				0x80F87C88U
#define RCO_PAGE_HEADER				0x8E7551ECU
#define RCO_PAGE_SINGLE				0xA50A6B85U
#define RCO_PAGE_MULTI				0xE698C298U
#define RCO_LIST_VIEW				0x38B62BE4U
#define RCO_EMPTY_TEXT				0x19F34EE1U
#define RCO_BTN_CANCEL				0x2D1E0809U
#define RCO_BTN_SORT				0x970705A2U
#define RCO_BTN_UPDATE				0xF0E0299FU
#define RCO_MULTI_GROUP				0xD7EB6B34U
#define RCO_BTN_CLEAR				0x8E279F8FU
#define RCO_COUNT_GROUP				0x5005C793U
#define RCO_COUNT_SLASH				0xE91916EBU
#define RCO_COUNT_CURRENT			0x2A69FBD5U
#define RCO_COUNT_MAX				0x27668AA2U
#define RCO_BTN_OK				0x91820F63U
#define RCO_HEADER_TITLE			0xC379062EU
#define RCO_TMPL_SINGLE_ITEM			0x1124B9A6U
#define RCO_TMPL_MULTI_ITEM			0x49A9F07FU
#define RCO_ITEM_ROOT				0x63678FEEU
#define RCO_ITEM_CHECK_BOX			0xB9803DB3U
#define RCO_ITEM_PANEL_IMAGE			0x7061C2A1U
#define RCO_ITEM_PLUS_ICON			0xDFC31E9CU
#define RCO_ITEM_AVATAR				0xEF8E97ACU
#define RCO_ITEM_DEFAULT_AVATAR			0xAA3E58D1U
#define RCO_ITEM_ONLINE_ID			0x0C2FED06U
#define RCO_ITEM_STATUS_ICON			0xB01C9CF6U
#define RCO_ITEM_PRESENCE_GROUP			0x1C7FA71DU
#define RCO_ITEM_PRESENCE_TEXT			0x52DA255EU
#define RCO_ITEM_PLATFORM_ICON			0xA80C310CU
#define RCO_ITEM_TROPHY_ICON			0x421D32D1U
#define RCO_ITEM_TROPHY_TEXT			0xD1FEA5F5U
#define RCO_STR_SORT_BY_ID			0x40010FBFU
#define RCO_STR_SORT_BY_STATUS			0x113FD435U
#define RCO_STR_MAX_SELECT_ERR			0x59CAB969U
#define RCO_STR_HTTP_ERR			0x5EB9A66FU
#define RCO_STR_NP_OFFLINE_ERR			0xDB2420F4U
#define RCO_STR_TROPHY_DASH			0xAF471967U
#define RCO_TEX_STATUS_UNKNOWN			0x5192B4C9U
#define RCO_TEX_STATUS_OFFLINE			0xE9C9F599U
#define RCO_TEX_STATUS_STANDBY			0xE0BAFF88U
#define RCO_TEX_STATUS_ONLINE			0x03E4E78DU
#define RCO_TEX_PLAT_VITA			0x8EECD0B0U
#define RCO_TEX_PLAT_PS4			0xEA391626U
#define RCO_TEX_PLAT_PS3			0x6EEEA1DCU

/* Generic callback interfaces matching Sony's MemberCallback template family */
class Callback0 {
public:
	virtual ~Callback0() {}
	virtual void Invoke() = 0;
};

template <typename A1>
class Callback1 {
public:
	virtual ~Callback1() {}
	virtual void Invoke(A1 a1) = 0;
};

template <typename A1, typename A2>
class Callback2 {
public:
	virtual ~Callback2() {}
	virtual void Invoke(A1 a1, A2 a2) = 0;
};

template <typename R, typename A1>
class FuncCallback1 {
public:
	virtual ~FuncCallback1() {}
	virtual R Invoke(A1 a1) = 0;
};

template <typename R, typename A1, typename A2, typename A3>
class FuncCallback3 {
public:
	virtual ~FuncCallback3() {}
	virtual R Invoke(A1 a1, A2 a2, A3 a3) = 0;
};

template <typename Obj, typename Arg = void *>
class MemberCallback0 : public Callback0 {
public:
	typedef void (Obj::*Method)(Arg);

	MemberCallback0(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberCallback0() {}

	virtual void Invoke()
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(m_arg);
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename A1, typename Arg = void *>
class MemberCallback1 : public Callback1<A1> {
public:
	typedef void (Obj::*Method)(A1, Arg);

	MemberCallback1(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberCallback1() {}

	virtual void Invoke(A1 a1)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(a1, m_arg);
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename A1, typename Arg = void *>
class TaskMemberCallback1 : public Callback1<A1> {
public:
	typedef void (Obj::*Method)(A1, Arg);

	TaskMemberCallback1(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~TaskMemberCallback1() {}

	virtual void Invoke(A1 a1)
	{
		if (m_obj->m_result == 0x7FFFFFFF) {
			if (m_obj != NULL && m_method != NULL)
				(m_obj->*m_method)(a1, m_arg);
		} else if (!m_obj->m_closing) {
			m_obj->Finish();
		}
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename A1, typename A2, typename Arg = void *>
class MemberCallback2 : public Callback2<A1, A2> {
public:
	typedef void (Obj::*Method)(A1, A2, Arg);

	MemberCallback2(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberCallback2() {}

	virtual void Invoke(A1 a1, A2 a2)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(a1, a2, m_arg);
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename R, typename A1, typename Arg = void *>
class MemberFuncCallback1 : public FuncCallback1<R, A1> {
public:
	typedef R (Obj::*Method)(A1, Arg);

	MemberFuncCallback1(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberFuncCallback1() {}

	virtual R Invoke(A1 a1)
	{
		if (m_obj != NULL && m_method != NULL)
			return (m_obj->*m_method)(a1, m_arg);
		return (R)0;
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename R, typename A1, typename A2, typename A3, typename Arg = void *>
class MemberFuncCallback3 : public FuncCallback3<R, A1, A2, A3> {
public:
	typedef R (Obj::*Method)(A1, A2, A3, Arg);

	MemberFuncCallback3(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberFuncCallback3() {}

	virtual R Invoke(A1 a1, A2 a2, A3 a3)
	{
		if (m_obj != NULL && m_method != NULL)
			return (m_obj->*m_method)(a1, a2, a3, m_arg);
		return R();
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

/* Member-function EventListener adapter (24 bytes, matching FriendSelectItemFactory) */
template <typename Obj>
class MemberEventListener : public paf::ui::EventListener {
public:
	typedef void (Obj::*Method)(int32_t, paf::ui::Handler *, paf::ui::Event *, void *);

	MemberEventListener(Obj *obj, Method method, void *userData)
		: m_obj(obj), m_method(method), m_userData(userData)
	{
	}

	virtual ~MemberEventListener() {}

	virtual int32_t Do(int32_t type, paf::ui::Handler *self, paf::ui::Event *e)
	{
		if (m_killed)
			return -1;
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(type, self, e, m_userData);
		return 0;
	}

	Obj *m_obj;
	Method m_method;
	void *m_userData;
};

/* Interface table published by friend_select_plugin via Plugin::SetInterface(1, ...) */
struct FriendSelectInterface {
	void (*abort)(void);
};

/* Plugin lifecycle entrypoints (friend_select_module.cpp) */
void FriendSelectSetParam(paf::Plugin::InitParam2 *param);
void FriendSelectInit(paf::Plugin *plugin);
void FriendSelectStart(paf::Plugin *plugin);
void FriendSelectStop(paf::Plugin *plugin);
void FriendSelectExit(paf::Plugin *plugin);

/* Application & plugin environment helpers (friend_select_app.cpp) */
void FriendSelectAppCreate(paf::Plugin *plugin);
void FriendSelectAppDestroy(void);
void FriendSelectAppStart(void);
void FriendSelectAppAbort(void);
paf::Plugin *FriendSelectGetPlugin(void);
int32_t FriendSelectGetMode(void);
uint32_t FriendSelectGetBgColor(void);
void FriendSelectSetTransitionMode(int32_t mode);

void FriendSelectEnvInit(void);
void FriendSelectEnvTerm(void);
const wchar_t *FriendSelectGetString(const paf::IDParam &id);
paf::intrusive_ptr<paf::graph::Surface> FriendSelectGetTexture(const paf::IDParam &id);
paf::ui::Scene *FriendSelectPageOpen(const paf::IDParam &id, int32_t prioOffset);
void FriendSelectPageClose(const paf::IDParam &id);
void FriendSelectPageCloseAll(void);
void FriendSelectTemplateOpen(paf::ui::Widget *parent, const paf::IDParam &id,
			      const paf::Plugin::TemplateOpenParam &param);
void FriendSelectDismissPopups(void);

/* Multi-select external parameter passed via Plugin::GetArg("multi_data") */
struct FriendSelectMultiExternalParam {
	uint32_t maxTotal;
	paf::list<SceNpId> existingList;
	paf::list<SceNpId> selectedList;
};

class FriendSelectSingleData;
class FriendSelectMultiData;

void FriendSelectSingleDataCreate(void);
void FriendSelectSingleDataDestroy(void);
FriendSelectSingleData *FriendSelectGetSingleData(void);
void FriendSelectMultiDataCreate(void);
void FriendSelectMultiDataDestroy(void);
FriendSelectMultiData *FriendSelectGetMultiData(void);
uint32_t FriendSelectGetMultiMaxTotal(void);
bool FriendSelectMultiIsSelected(const class FriendUserProfile &profile);

/* Background, Error Dialog, Header Bar (friend_select_ui.cpp) */
void FriendSelectBgCreate(void);
void FriendSelectBgDestroy(void);
void FriendSelectBgShow(void);
void FriendSelectBgHide(void);

void FriendSelectErrorInit(void);
void FriendSelectErrorClose(void);
void FriendSelectErrorTerm(void);
int32_t FriendSelectErrorShow(int32_t errorCode, Callback0 *onDismiss);

void FriendSelectHeaderBarInit(void);
void FriendSelectHeaderBarTerm(void);
void FriendSelectHeaderBarShow(int32_t transitionMode);
void FriendSelectHeaderBarHide(int32_t transitionMode);
paf::ui::Widget *FriendSelectHeaderGetButton(int32_t buttonId);

struct FriendSelectHeaderParam {
	bool showMultiControls;
	Callback1<int32_t> *actionCallback;
};

class FriendSelectHeader {
public:
	FriendSelectHeader();
	virtual ~FriendSelectHeader();

	void Configure(const FriendSelectHeaderParam &param);
	void SetSortMode(int32_t sortMode);
	void SetUpdating(bool updating);
	bool IsUpdating(void);
	void SetClearEnabled(bool enabled, bool animate);
	void SetSelectedCount(int32_t count);
	void SetMaxCount(int32_t maxCount);
	void SetOkEnabled(bool enabled, bool animate);

	Callback1<int32_t> *m_callback;
};

/* Item factory, item binding, and message formatting (friend_select_item.cpp) */
class FriendSelectItemFactory : public paf::ui::listview::ItemFactory {
public:
	FriendSelectItemFactory(FuncCallback1<paf::ui::ListItem *, paf::ui::Widget *> *createCb,
				Callback2<paf::ui::Widget *, int32_t> *bindCb,
				Callback2<paf::ui::Widget *, int32_t> *unbindCb,
				Callback1<int32_t> *selectCb);
	virtual ~FriendSelectItemFactory();

	virtual paf::ui::ListItem *Create(paf::ui::listview::ItemFactory::CreateParam &param);
	virtual void Start(paf::ui::listview::ItemFactory::StartParam &param) { (void)param; }
	virtual void Stop(paf::ui::listview::ItemFactory::StopParam &param);
	virtual void Dispose(paf::ui::listview::ItemFactory::DisposeParam &param) { (void)param; }

	void DetachCallbacks(void);
	void OnItemDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);
	void OnRowButtonDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
			       void *userData);

	FuncCallback1<paf::ui::ListItem *, paf::ui::Widget *> *m_createCb;
	Callback2<paf::ui::Widget *, int32_t> *m_bindCb;
	Callback2<paf::ui::Widget *, int32_t> *m_unbindCb;
	Callback1<int32_t> *m_selectCb;
	paf::list<paf::ui::ListItem *> m_recycled;
	bool m_detached;
};

paf::ui::ListItem *FriendSelectCreateSingleItem(paf::ui::Widget *parent);
paf::ui::ListItem *FriendSelectCreateMultiItem(paf::ui::Widget *parent);
void FriendSelectBindItemWidget(paf::ui::Widget *widget, const class FriendUserProfile &profile);
void FriendSelectUnbindItemWidget(paf::ui::Widget *widget, const class FriendUserProfile &profile);

paf::wstring FriendSelectFormatMessage(const wchar_t *fmt, const wchar_t *a1,
				       const wchar_t *a2 = NULL, const wchar_t *a3 = NULL,
				       const wchar_t *a4 = NULL, const wchar_t *a5 = NULL,
				       const wchar_t *a6 = NULL, const wchar_t *a7 = NULL,
				       const wchar_t *a8 = NULL, const wchar_t *a9 = NULL,
				       const wchar_t *a10 = NULL, const wchar_t *a11 = NULL);

/* SceFriendLib list & profile cache (friend_lib_list.cpp) */
enum FriendRelation {
	FRIEND_RELATION_NONE = 0,
	FRIEND_RELATION_SELF = 1,
	FRIEND_RELATION_FRIEND = 2,
	FRIEND_RELATION_REQUESTING = 3,
	FRIEND_RELATION_REQUESTED = 4,
	FRIEND_RELATION_BLOCKED = 5,
	FRIEND_RELATION_OTHER = 6
};

enum FriendOnlineStatus {
	FRIEND_STATUS_UNKNOWN = 0,
	FRIEND_STATUS_OFFLINE = 1,
	FRIEND_STATUS_STANDBY = 2,
	FRIEND_STATUS_ONLINE = 3
};

enum FriendPlatform {
	FRIEND_PLATFORM_PSVITA = 0,
	FRIEND_PLATFORM_PS4 = 1,
	FRIEND_PLATFORM_PS3 = 2,
	FRIEND_PLATFORM_OTHER = 3
};

class FriendUserProfile {
public:
	FriendUserProfile();
	virtual ~FriendUserProfile();
	virtual void Reset();

	bool MatchesNpId(const SceNpId &other) const;
	bool IsSelf() const;
	void SetRelationFromString(const paf::string &relationStr);
	void SetTrophyLevel(int32_t level);
	void SetOnlineStatusFromString(const paf::string &statusStr);

	int32_t m_relation;
	SceNpId m_npId;
	paf::string m_avatarUrl;
	uint32_t m_panelBgColor;
	paf::string m_panelUrl;
	int32_t m_onlineStatus;
	int32_t m_platform;
	paf::string m_gameTitleName;
	int32_t m_trophyLevel;
	bool m_isPlus;
	bool m_hasRequestMessage;
};

class FriendListPageCache {
public:
	FriendListPageCache();
	virtual ~FriendListPageCache();

	int32_t InitJsonParser(void *parserCtx, uint32_t reserveCount);
	int32_t HandleJsonEvent(int32_t parserHandle, int32_t eventType, int32_t depth,
				uint32_t fieldIndex, int32_t arrayIndex, void *valuePtr);

	int32_t m_defaultRelation;
	int32_t m_startOffset;
	paf::vector<FriendUserProfile> m_profiles;
	int32_t m_totalResults;
};

class FriendListCache {
public:
	FriendListCache();
	virtual ~FriendListCache();

	void ClearAll();
	void SetSortMode(int32_t sortMode);
	int32_t BeginFetch();
	void EndFetch(int32_t result);
	const FriendUserProfile &GetProfileAt(int32_t index);

	paf::thread::RMutex m_mutex;
	paf::list<FriendListPageCache *> m_pages;
	FriendListPageCache *m_fetchingPage;
	int32_t m_totalResults;
	int32_t m_sortMode;
	bool m_invalidated;
};

class FriendSelectMultiData {
public:
	inline __attribute__((always_inline)) FriendSelectMultiData()
		: m_listCache(), m_existingList(NULL), m_outputList(NULL), m_maxTotal(0),
		  m_selected(), m_maxSelectable(0)
	{
	}
	virtual ~FriendSelectMultiData();

	void SetExternalParam(FriendSelectMultiExternalParam *param);
	int32_t ToggleAt(int32_t index);
	void ClearSelection();
	void CommitSelection();
	uint32_t GetSelectedCount() const;
	uint32_t GetTotalSelectedCount() const;
	bool IsSelected(const FriendUserProfile &profile);
	bool IsExisting(const FriendUserProfile &profile);

	FriendListCache m_listCache;
	paf::list<SceNpId> *m_existingList;
	paf::list<SceNpId> *m_outputList;
	uint32_t m_maxTotal;
	paf::list<SceNpId> m_selected;
	uint32_t m_maxSelectable;
};

/* Global SceFriendLib lifecycle (friend_lib_list.cpp & friend_lib_core.cpp) */
extern SceNpId g_selfNpId;

void FriendLibInit(paf::memory::HeapAllocator *allocator, paf::graph::SurfacePool *surfacePool);
void FriendLibStart(void);
void FriendLibStop(void);
void FriendLibTerm(void);
void FriendLibRegisterWidgetFactory(void);
void FriendLibUnregisterWidgetFactory(void);

/* SceFriendLib async tasks, clock watcher, image manager (friend_lib_core.cpp) */
class FriendLibClockListener {
public:
	virtual ~FriendLibClockListener() {}
	virtual void OnClockChanged() = 0;
	virtual bool MatchesOwner(void *owner) const = 0;
};

template <typename Obj>
class MemberClockListener : public FriendLibClockListener {
public:
	typedef void (Obj::*Method)();

	MemberClockListener(Obj *obj, Method method) : m_obj(obj), m_method(method) {}
	virtual ~MemberClockListener() {}

	virtual void OnClockChanged()
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)();
	}

	virtual bool MatchesOwner(void *owner) const
	{
		return (void *)m_obj == owner;
	}

	Obj *m_obj;
	Method m_method;
};

class FriendLibAsyncTask {
public:
	explicit FriendLibAsyncTask(Callback1<int32_t> *callback) : m_callback(callback) {}
	virtual ~FriendLibAsyncTask();

	virtual void Start() = 0;
	virtual void RequestCancel() = 0;
	virtual void OnComplete(int32_t result);
	virtual void NotifyCallback(int32_t result);

	Callback1<int32_t> *m_callback;
};

class FriendLibQueuedTask : public FriendLibAsyncTask, public paf::job::JobItem {
public:
	FriendLibQueuedTask(paf::job::JobQueue *queue, Callback1<int32_t> *callback,
			    const char *jobName);
	virtual ~FriendLibQueuedTask();

	virtual void Start();
	virtual void RequestCancel();
	virtual void OnComplete(int32_t result);
	virtual void Finish(int32_t result);

	paf::job::JobQueue *m_queue;
};

class FriendLibNpTaskBase : public FriendLibQueuedTask {
public:
	explicit FriendLibNpTaskBase(Callback1<int32_t> *callback);
	virtual ~FriendLibNpTaskBase() {}
};

class FriendLibGetSelfNpIdTask : public FriendLibNpTaskBase {
public:
	FriendLibGetSelfNpIdTask(SceNpId *outNpId, Callback1<int32_t> *callback);
	virtual ~FriendLibGetSelfNpIdTask() {}
	virtual int32_t Run();

	SceNpId *m_outNpId;
};

void FriendLibCoreInit(paf::memory::HeapAllocator *allocator,
		       paf::graph::SurfacePool *surfacePool);
void FriendLibCoreTerm(void);
paf::common::SharedPtr<paf::Buffer> FriendLibAllocBuffer(size_t size);
paf::graph::SurfacePool *FriendLibGetSurfacePool(void);
paf::job::JobQueue *FriendLibGetJobQueue(void);

bool FriendLibIsTaskRunning(void);
void FriendLibCancelActiveTask(void);
void FriendLibStartTask(FriendLibAsyncTask *task);

void FriendLibClockInit(void);
void FriendLibClockTerm(void);
void FriendLibClockRegister(FriendLibClockListener *listener);
void FriendLibClockUnregister(void *owner);
void FriendLibPlayCommonSE(uint32_t index);
void FriendLibNormalizeNewlines(paf::string &text);

void FriendLibImageManagerInit(paf::graph::SurfacePool *surfacePool);
void FriendLibImageManagerTerm(void);
void FriendLibRequestWidgetImage(const char *url, paf::ui::Widget *widget);
void FriendLibCancelAllImages(Callback0 *onDone);
void FriendLibCancelWidgetImage(const char *url, paf::ui::Widget *widget);
bool FriendLibHasPendingImages(void);

/* SceFriendSelectWebApi (friend_select_webapi.cpp) */
class FriendSelectJsonReceiver {
public:
	virtual ~FriendSelectJsonReceiver() {}
	virtual int32_t InitParser(void *parserCtx) = 0;
};

void FriendSelectWebApiInit(void);
void FriendSelectWebApiTerm(void);
void *FriendSelectWebApiAlloc(size_t size);
void FriendSelectWebApiFree(void *ptr);

class FriendSelectWebApiJob : public FriendLibQueuedTask {
public:
	explicit FriendSelectWebApiJob(Callback1<int32_t> *callback);
	virtual ~FriendSelectWebApiJob();
	virtual int32_t Run() = 0;
	virtual void Cancel();

protected:
	int32_t ExecuteGetRequest(const char *apiGroup, const char *path,
				  FriendSelectJsonReceiver *receiver);

	int64_t m_requestId;
};

class FriendListFetchJob : public FriendSelectWebApiJob {
public:
	FriendListFetchJob(FriendListCache *cache, int32_t startIndex,
			   Callback1<int32_t> *callback);
	virtual ~FriendListFetchJob() {}

	virtual int32_t Run();
	virtual void NotifyCallback(int32_t result);

	FriendListCache *m_cache;
	int32_t m_alignedOffset;
};

/* UpdateButton, PopupListView wrapper, and number formatter (friend_lib_update_button.cpp) */
class UpdateButtonWidget : public paf::ui::PlaneButton {
public:
	static const char *TypeName(void);

	UpdateButtonWidget(paf::ui::Widget *parent, paf::ui::Widget::CreateParam *param);
	virtual ~UpdateButtonWidget() {}
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual bool StartSpin();
	virtual void StopSpin();

	void EnsureHighlightObj(void);

	bool m_spinning;
};

extern paf::ui::FactoryFuncTable g_updateButtonFactoryTable;

struct PopupListItemData {
	PopupListItemData() : label(), icon(), disabled(false), hasSubMenu(false), userValue(0) {}

	paf::wstring label;
	paf::intrusive_ptr<paf::graph::Surface> icon;
	bool disabled;
	bool hasSubMenu;
	int32_t userValue;
};

struct PopupListOpenArg {
	PopupListOpenArg()
		: mode(0), plugin(NULL), anchorWidget(NULL), selectedIndex(-1), items(),
		  callback(NULL)
	{
	}

	int32_t mode;
	paf::Plugin *plugin;
	paf::ui::Widget *anchorWidget;
	int32_t selectedIndex;
	paf::vector<PopupListItemData> items;
	Callback1<int32_t> *callback;
};

void FriendLibPopupInit(void);
void FriendLibPopupTerm(void);
void FriendLibPopupClose(void);
int32_t FriendLibPopupOpen(const PopupListOpenArg &arg);
void FriendLibSetNumberText(paf::ui::Widget *widget, int32_t value, int32_t maxDigits);

/* SceFriendLibHttp & SceFriendLibImage (friend_lib_http.cpp) */
typedef FuncCallback3<paf::intrusive_ptr<paf::graph::Surface>, paf::graph::SurfacePool *,
		      paf::image::Image *, int32_t>
	FriendLibSurfaceAllocCallback;

struct FriendLibImageLoadOption {
	FriendLibImageLoadOption()
		: maxWidth(0), maxHeight(0), surfacePool(NULL), allocCb(NULL), sourceBuffer()
	{
	}

	int32_t maxWidth;
	int32_t maxHeight;
	paf::graph::SurfacePool *surfacePool;
	FriendLibSurfaceAllocCallback *allocCb;
	paf::common::SharedPtr<paf::Buffer> sourceBuffer;
};

void FriendLibHttpInit(void);
void FriendLibHttpTerm(void);

class FriendLibHttpConnection {
public:
	FriendLibHttpConnection(int32_t templateId, const paf::Url &url);
	virtual ~FriendLibHttpConnection();

	paf::string m_baseUrl;
	int32_t m_connId;
	bool m_isHttps;
};

class FriendLibHttpManager;

class FriendLibHttpJob : public paf::job::JobItem {
public:
	FriendLibHttpJob(FriendLibHttpManager *mgr, const char *urlStr, int32_t responseMode,
			 uint32_t optionFlags, Callback2<FriendLibHttpJob *, int32_t> *callback,
			 const char *jobName);
	virtual ~FriendLibHttpJob();
	virtual void Cancel();
	virtual int32_t Run();
	virtual void Finish(int32_t result);
	virtual void NotifyCallback(int32_t result);

protected:
	int32_t ExecuteHttpRequest();
	int32_t ReadFixedLength(uint32_t contentLength);
	int32_t ReadChunked();

public:
	FriendLibHttpManager *m_mgr;
	paf::Url m_url;
	int32_t m_responseMode;
	uint32_t m_optionFlags;
	Callback2<FriendLibHttpJob *, int32_t> *m_callback;
	int32_t m_requestId;
	paf::common::SharedPtr<paf::Buffer> m_responseBody;
	int32_t m_httpStatus;
};

class FriendLibHttpManager {
public:
	explicit FriendLibHttpManager(paf::job::JobQueue *jobQueue);
	virtual ~FriendLibHttpManager();

	paf::common::SharedPtr<FriendLibHttpConnection> AcquireConnection(const paf::Url &url);
	void CancelAllJobs();
	void CancelJobByUrl(const char *url);
	paf::list<paf::common::SharedPtr<paf::job::JobItem> >::iterator FindJob(const char *url);
	void RemoveJob(FriendLibHttpJob *job);

	paf::job::JobQueue *m_jobQueue;
	paf::list<paf::common::SharedPtr<paf::job::JobItem> > m_activeJobs;
	int32_t m_maxHttpConns;
	int32_t m_maxHttpsConns;
	paf::list<paf::common::SharedPtr<FriendLibHttpConnection> > m_httpConns;
	paf::list<paf::common::SharedPtr<FriendLibHttpConnection> > m_httpsConns;
	uint32_t m_defaultOptionFlags;
};

class FriendLibImageJob : public FriendLibHttpJob {
public:
	FriendLibImageJob(FriendLibHttpManager *mgr, const char *urlStr, uint32_t optionFlags,
			  Callback2<FriendLibImageJob *, int32_t> *callback,
			  const FriendLibImageLoadOption &option);
	virtual ~FriendLibImageJob();
	virtual void Cancel();
	virtual int32_t Run();
	virtual void NotifyCallback(int32_t result);

private:
	int32_t DecodeSurface();
	int32_t ReadLocalFile();

public:
	Callback2<FriendLibImageJob *, int32_t> *m_imageCallback;
	int32_t m_maxWidth;
	int32_t m_maxHeight;
	paf::graph::SurfacePool *m_surfacePool;
	FriendLibSurfaceAllocCallback *m_allocCb;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
	paf::common::SharedPtr<paf::File> m_localFile;
};

class FriendLibImageLoader : public FriendLibHttpManager {
public:
	explicit FriendLibImageLoader(paf::job::JobQueue *jobQueue);
	virtual ~FriendLibImageLoader();

	void EnqueueImage(const char *url, Callback2<FriendLibImageJob *, int32_t> *callback,
			  const FriendLibImageLoadOption *option);
};

/* PopupListView engine & RTC helper (popup_list_view.cpp) */
int32_t FriendLibGetCurrentNetworkTick(SceRtcTick *outTick);

struct PopupListPlacementRule {
	uint32_t dirMask;
	float minAngle;
	float maxAngle;
	uint32_t alignX;
	uint32_t alignY;
	float offsetX;
	float offsetY;
	uint32_t textureHash;
	bool rotatePi;
};

class PopupListCloseCallback {
public:
	virtual ~PopupListCloseCallback() {}
	virtual void Invoke(int32_t selectedIndex) = 0;
};

template <typename Obj>
class PopupListCloseMemberCallback : public PopupListCloseCallback {
public:
	typedef void (Obj::*Method)(int32_t, void *);

	PopupListCloseMemberCallback(Obj *obj, Method method, void *arg = NULL)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~PopupListCloseMemberCallback() {}

	virtual void Invoke(int32_t selectedIndex)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(selectedIndex, m_arg);
	}

	Obj *m_obj;
	Method m_method;
	void *m_arg;
};

struct PopupListViewConfig {
	inline __attribute__((always_inline)) PopupListViewConfig()
		: style(0), plugin(NULL), pagePriority(0), titleText(), itemCount(0),
		  initialSelectIndex(-1), legacyLabels(), items(), customCreateCb(NULL),
		  customBindCb(NULL), selectCb(NULL), closeCb(NULL), checkMarkSide(1),
		  position(0.0f, 0.0f, 0.0f, 0.0f), alignX(0), alignY(0), anchorX(4), anchorY(4),
		  minSize(0.0f, 0.0f, 0.0f, 0.0f), maxSize(0.0f, 0.0f, 0.0f, 0.0f),
		  topPad(0.0f), bottomPad(0.0f), leftPad(0.0f), rightPad(0.0f),
		  anchorWidget(NULL), anchorRadius(0.0f), dirMask(0),
		  topLimit(272.0f), bottomLimit(-272.0f), leftLimit(-480.0f), rightLimit(480.0f),
		  autoClose(true), transparentBg(true), playSe(true), wrapText(true),
		  titleMinFontSize(0.0f), itemMinFontSize(0.0f), colorMask(0), bgColor(0),
		  customRules()
	{
		sce_paf_memset(itemTexHashes, 0, sizeof(itemTexHashes));
	}
	~PopupListViewConfig();

	int32_t style;
	paf::Plugin *plugin;
	int32_t pagePriority;
	paf::wstring titleText;
	int32_t itemCount;
	int32_t initialSelectIndex;
	paf::vector<paf::wstring> legacyLabels;
	paf::vector<PopupListItemData> items;
	void *customCreateCb;
	void *customBindCb;
	Callback2<int32_t, const PopupListItemData *> *selectCb;
	PopupListCloseCallback *closeCb;
	int32_t checkMarkSide;
	paf::math::v4 position;
	uint32_t alignX;
	uint32_t alignY;
	uint32_t anchorX;
	uint32_t anchorY;
	paf::math::v4 minSize;
	paf::math::v4 maxSize;
	float topPad;
	float bottomPad;
	float leftPad;
	float rightPad;
	paf::ui::Widget *anchorWidget;
	float anchorRadius;
	uint32_t dirMask;
	float topLimit;
	float bottomLimit;
	float leftLimit;
	float rightLimit;
	bool autoClose;
	bool transparentBg;
	bool playSe;
	bool wrapText;
	float titleMinFontSize;
	float itemMinFontSize;
	uint32_t colorMask;
	uint32_t bgColor;
	uint32_t itemTexHashes[8];
	paf::vector<PopupListPlacementRule> customRules;
};

int32_t PopupListViewPush(const PopupListViewConfig &cfg, uint32_t depth, int32_t parentItemIdx);
void PopupListViewPopTo(int32_t depth);

#endif /* SYS_USERMODE_FRIEND_SELECT_INTERNAL_H */
