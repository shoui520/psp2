/*
 * filelist_internal.h: shared internal classes and declarations for SceShell's
 * FileList / Activity UI framework (Sony's vsh/app/common/src/filelist/src/,
 * 0x81348E70..0x8135D9C9).
 */
#ifndef SHELL_FILELIST_INTERNAL_H
#define SHELL_FILELIST_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <std/memory>
#include <plugin.h>
#include <framework.h>
#include <math/math.h>
#include <thread/thread.h>
#include <job/job.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
#include <cache/cache_manager.h>
#include <cache/cache_image.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <widget/core/layouter.h>
#include <widget/core/animlistener.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/core/environment.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_file_list.h>
#include <widget/w_check_box.h>
#include <timer/timer.h>
#include <system/system.h>
#include <activity_db/activity_db.h>
#include <activity_db/activity_db_internal.h>
#include <np/np_manager/np_manager.h>

typedef __typeof__(static_cast<paf::ui::ListView *>(NULL)->m_min_size) PafVec2;
typedef __typeof__(static_cast<paf::ui::FocusShape *>(NULL)->m_rot) PafVec3;
typedef __typeof__(static_cast<paf::ui::Widget *>(NULL)->m_pos) PafVec4;

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
int sceAppMgrLaunchAppByUri(int flags, const char *uri);
int sceAppMgrWorkDirMount(int id, char *mountPoint);
int sceAppMgrUmount(const char *mountPoint);
int sceRtcFormatRFC3339(char *outBuf, const SceRtcTick *utcTick, int tzOffsetMinutes);

/* SceActivityDb plain C export (imported by NID in SceShell) */
int SceActivityDb_738D4B49(void);
}

/* SceActivityDb C++ query/sync handle method forwarders */
static inline void *SceActivityDb_6C98B745(void *handle)
{
	return new (handle) ActivityDbQueryHandle();
}
static inline int SceActivityDb_F65C0C21(void *handle, const ActivityDbQueryCondition *condition)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->Create(condition);
}
static inline void *SceActivityDb_6E198FFA(void *handle)
{
	static_cast<ActivityDbQueryHandle *>(handle)->~ActivityDbQueryHandle();
	return handle;
}
static inline int SceActivityDb_402A5F72(void *handle)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->IsChanged() ? 1 : 0;
}
static inline int SceActivityDb_E6601703(void *handle)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->ClearChanged();
}
static inline int SceActivityDb_6C29F640(void *handle, int *outCount)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->GetInfoCount(outCount);
}
static inline int SceActivityDb_8F2E07A1(void *handle, int offset, int count,
					 paf::vector<SceInt64> *outRowids)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->GetRowids(offset, count, outRowids);
}
static inline int SceActivityDb_74F96707(void *handle, paf::vector<SceInt64> *rowids,
					 paf::vector<ActivityDbStatus> *outStatuses)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->GetStatuses(rowids, outStatuses);
}
static inline int SceActivityDb_572A6463(void *handle, const SceInt64 *parentRowid, int offset,
					 int count, paf::vector<SceInt64> *outRowids)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->GetCommentRowids(
		parentRowid, offset, count, outRowids);
}
extern "C" int _ZN21ActivityDbQueryHandle11GetCommentsEPN3paf6vectorIxNS0_9allocatorIxEEEEPNS1_I17ActivityDbCommentNS2_IS6_EEEE(
	void *handle, paf::vector<SceInt64> *rowids,
	paf::vector<ActivityDbComment> *outComments);

static inline int SceActivityDb_D512F568(void *handle, paf::vector<SceInt64> *rowids,
					 paf::vector<ActivityDbComment> *outComments)
{
	return _ZN21ActivityDbQueryHandle11GetCommentsEPN3paf6vectorIxNS0_9allocatorIxEEEEPNS1_I17ActivityDbCommentNS2_IS6_EEEE(
		handle, rowids, outComments);
}
static inline int SceActivityDb_7ED9033C(void *handle, int *outCount, SceUInt64 *outLastReadTime,
					 SceUInt64 *outLastUpdateTime,
					 SceUInt64 *outLatestActivityTime)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->GetCountAndSyncTimes(
		outCount, reinterpret_cast<SceInt64 *>(outLastReadTime),
		reinterpret_cast<SceInt64 *>(outLastUpdateTime),
		reinterpret_cast<SceInt64 *>(outLatestActivityTime));
}
static inline int SceActivityDb_8D4B7F73(void *handle, const SceInt64 *statusRowid,
					 ActivityDbGoodJob *outGoodJob)
{
	return static_cast<ActivityDbQueryHandle *>(handle)->GetGoodJob(statusRowid, outGoodJob);
}
static inline void *SceActivityDb_B83F0A77(void *syncHandle)
{
	return new (syncHandle) ActivityDbSyncHandle();
}
static inline void *SceActivityDb_937D49BA(void *syncHandle)
{
	static_cast<ActivityDbSyncHandle *>(syncHandle)->~ActivityDbSyncHandle();
	return syncHandle;
}
static inline int SceActivityDb_5B44F019(void *syncHandle)
{
	return static_cast<ActivityDbSyncHandle *>(syncHandle)->Cancel();
}
static inline int SceActivityDb_FEB3A02F(void *syncHandle, const SceInt64 *statusRowid,
					 const paf::string *titleId, const void *netCallback)
{
	return static_cast<ActivityDbSyncHandle *>(syncHandle)->SyncDetail(
		statusRowid, titleId,
		static_cast<const ActivityDbNetCallback *>(netCallback));
}
static inline int SceActivityDb_2929CAD1(void *syncHandle, const SceInt64 *statusRowid)
{
	return static_cast<ActivityDbSyncHandle *>(syncHandle)->VoteGoodJob(statusRowid);
}
static inline int SceActivityDb_DF874DD2(void *syncHandle, const SceInt64 *statusRowid)
{
	return static_cast<ActivityDbSyncHandle *>(syncHandle)->CancelGoodJob(statusRowid);
}
static inline int SceActivityDb_C715B8BC(ActivityDbComment *outComment, const paf::string *message)
{
	ActivityDbSyncHandle::MakeComment(outComment, *message);
	return 0;
}
static inline int SceActivityDb_3F501885(void *syncHandle, const SceInt64 *statusRowid,
					 const ActivityDbComment *comment)
{
	return static_cast<ActivityDbSyncHandle *>(syncHandle)->PostComment(statusRowid, comment);
}
static inline int SceActivityDb_5FC8EE9A(void *syncHandle, const SceInt64 *statusRowid)
{
	return static_cast<ActivityDbSyncHandle *>(syncHandle)->DeleteStatus(statusRowid);
}
static inline int SceActivityDb_2A7ABB45(void *syncHandle, const SceInt64 *statusRowid,
					 const SceInt64 *commentRowid)
{
	(void)statusRowid;
	return static_cast<ActivityDbSyncHandle *>(syncHandle)->DeleteComment(commentRowid);
}

#include "../activity_drawobj/activity_drawobj.h"
#include "../../plugins.h"

/* External shell helpers used by the filelist / activity UI framework */
extern paf::job::JobQueue *activityUiGetNetworkJobQueue(void);
extern void activityUiDialogManagerInit(void);
extern void activityUiDialogManagerTerm(void);
extern void activityUiServiceInit(void);
extern void activityUiServiceTerm(void);
extern void activityUiNotifyCacheInfoUpdated(void);

namespace filelist {

/* Activity URI categories returned by BaseActivityVisualizer::ClassifyUriCategory */
enum ActivityUriCategory {
	ACTIVITY_URI_CATEGORY_WEB = 0,
	ACTIVITY_URI_CATEGORY_STORE = 1,
	ACTIVITY_URI_CATEGORY_GAME = 2,
	ACTIVITY_URI_CATEGORY_TROPHY = 3,
	ACTIVITY_URI_CATEGORY_MAP = 4,
	ACTIVITY_URI_CATEGORY_PROFILE = 5,
	ACTIVITY_URI_CATEGORY_OTHER = 6,
	ACTIVITY_URI_CATEGORY_EMPTY = 7
};

/* Activity record kind returned by Live2Record::GetRecordKind */
enum Live2RecordKind {
	LIVE2_RECORD_STATUS = 1,
	LIVE2_RECORD_COMMENT = 2
};

/*
 * Activity record base and derived types (ref-counted snapshots fetched from SceActivityDb).
 * Live2StatusRecord is 184 bytes (0xB8); Live2CommentRecord is 104 bytes (0x68).
 */
class Live2Record : public paf::common::RefObj {
public:
	Live2Record() : m_rowid(0)
	{
	}

	virtual ~Live2Record()
	{
	}

	virtual int GetRecordKind() const = 0;

	SceInt64 m_rowid;
};

class Live2StatusRecord : public Live2Record {
public:
	Live2StatusRecord() : m_status()
	{
	}

	virtual ~Live2StatusRecord()
	{
	}

	virtual int GetRecordKind() const
	{
		return LIVE2_RECORD_STATUS;
	}

	ActivityDbStatus m_status;
};

class Live2CommentRecord : public Live2Record {
public:
	Live2CommentRecord() : m_comment()
	{
	}

	virtual ~Live2CommentRecord()
	{
	}

	virtual int GetRecordKind() const
	{
		return LIVE2_RECORD_COMMENT;
	}

	ActivityDbComment m_comment;
};

/*
 * Live2AccessorProxy: owns an ActivityDbQueryHandle on the heap and dispatches
 * change notifications to registered Live2Accessors. Disposes the handle on
 * the filelist fast task queue when destroyed.
 */
class Live2AccessorProxy {
public:
	typedef void (*ChangeCallback)(void *userdata);

	struct Listener {
		ChangeCallback callback;
		void *userdata;
	};

	explicit Live2AccessorProxy(const ActivityDbQueryCondition &condition);
	virtual ~Live2AccessorProxy();

	void AddListener(ChangeCallback callback, void *userdata);
	void RemoveListener(ChangeCallback callback, void *userdata);
	void NotifyIfChanged();

	void *m_queryHandle;
	paf::vector<Listener> m_listeners;
	paf::thread::Mutex m_mutex;
};

/*
 * Live2Accessor: windowed cache over an ActivityDbQueryHandle (either statuses
 * or comments for a parent status).
 */
class Live2Accessor {
public:
	explicit Live2Accessor(const paf::common::SharedPtr<Live2AccessorProxy> &proxy);
	virtual ~Live2Accessor();

	virtual paf::intrusive_ptr<Live2Record> GetRecord(SceInt64 rowid)
	{
		(void)rowid;
		return paf::intrusive_ptr<Live2Record>();
	}
	virtual bool IsChanged();
	virtual void FetchCount() {}
	virtual void FetchWindow(int index) { (void)index; }

	int GetCount();
	SceInt64 GetRowid(int index);
	int FindIndex(SceInt64 rowid);
	void ClearCache();
	void ClearDirty();
	void MarkDirty();

	static void OnProxyChanged(void *userdata);

	paf::common::SharedPtr<Live2AccessorProxy> m_proxy;
	int m_windowStart;
	paf::vector<SceInt64> m_windowRowids;
	int m_totalCount;
	volatile unsigned int m_dirty;
};

class Live2StatusAccessor : public Live2Accessor {
public:
	Live2StatusAccessor(const paf::common::SharedPtr<Live2AccessorProxy> &proxy,
			    int windowSize);
	virtual ~Live2StatusAccessor();

	virtual paf::intrusive_ptr<Live2Record> GetRecord(SceInt64 rowid);
	virtual bool IsChanged();
	virtual void FetchCount();
	virtual void FetchWindow(int index);

	int m_windowSize;
	paf::vector<ActivityDbStatus> m_statuses;
};

class Live2CommentAccessor : public Live2Accessor {
public:
	Live2CommentAccessor(const paf::common::SharedPtr<Live2AccessorProxy> &proxy,
			     SceInt64 parentRowid, int windowSize);
	virtual ~Live2CommentAccessor();

	virtual paf::intrusive_ptr<Live2Record> GetRecord(SceInt64 rowid);
	virtual void FetchCount();
	virtual void FetchWindow(int index);

	int m_windowSize;
	SceInt64 m_parentRowid;
	paf::vector<ActivityDbComment> m_comments;
};

/* Factory interface for creating a Live2Accessor on a background query job */
class Live2AccessorFactory {
public:
	virtual ~Live2AccessorFactory();
	virtual paf::common::SharedPtr<Live2Accessor> CreateAccessor();
};

class Live2StatusAccessorFactory : public Live2AccessorFactory {
public:
	explicit Live2StatusAccessorFactory(int windowSize);
	virtual ~Live2StatusAccessorFactory();
	virtual paf::common::SharedPtr<Live2Accessor> CreateAccessor();

	int m_windowSize;
	ActivityDbQueryCondition m_condition;
};

class Live2CommentAccessorFactory : public Live2AccessorFactory {
public:
	Live2CommentAccessorFactory(const paf::common::SharedPtr<Live2AccessorProxy> &proxy,
				    SceInt64 parentRowid, int windowSize);
	virtual ~Live2CommentAccessorFactory();
	virtual paf::common::SharedPtr<Live2Accessor> CreateAccessor();

	paf::common::SharedPtr<Live2AccessorProxy> m_proxy;
	SceInt64 m_parentRowid;
	int m_windowSize;
};

/* Synchronization interface passed from Live2DataQueryJob to Live2Iterator::Refresh */
class Live2SyncController {
public:
	typedef void (*ApplyFunc)(paf::ui::filelist::CellListAccessor &accessor, void *userdata);

	virtual ~Live2SyncController()
	{
	}

	virtual bool IsCanceled() = 0;
	virtual bool WaitForMainThreadApply(ApplyFunc func, void *userdata) = 0;
	virtual int GetRefreshMode() = 0;
};

/* Maps stable PAF FileList cell IDs to current Live2Accessor row indices and DB rowids */
struct Live2CellMapping {
	int index;
	SceInt64 rowid;
};

class Live2Iterator {
public:
	explicit Live2Iterator(const paf::common::SharedPtr<Live2AccessorFactory> &factory);
	~Live2Iterator();

	int GetCount();
	bool IsChanged();
	SceInt64 GetOrCreateCellId(int index);
	paf::intrusive_ptr<Live2Record> GetRecordByCellId(SceInt64 cellId);
	void Refresh(Live2SyncController *controller);
	void PinCellRowid(SceInt64 cellId);
	void ReleaseCellId(SceInt64 cellId);
	paf::common::SharedPtr<Live2AccessorProxy> GetAccessorProxy();

	paf::common::SharedPtr<Live2Accessor> m_accessor;
	paf::thread::Mutex m_mutex;
	paf::map<SceInt64, Live2CellMapping> m_cellMap;
};

/* Factory interface used by Live2CellSource to construct visualizers for cells */
class Live2VisualizerFactory {
public:
	virtual ~Live2VisualizerFactory()
	{
	}

	virtual paf::intrusive_ptr<paf::ui::filelist::Visualizer> CreateVisualizer(
		paf::ui::Widget *parent,
		const paf::common::SharedPtr<Live2Iterator> &iterator,
		int index, SceInt64 cellId) = 0;
	virtual PafVec4 GetVisualSize() = 0;
};

/*
 * Live2CellSource: FileList CellSource backed by a Live2Accessor and background
 * ScePafFileListLive2DataQuery jobs.
 */
class Live2CellSource : public paf::ui::filelist::CellSource {
public:
	typedef void (*CountCallback)(int count, int arg);

	static const char *TypeName();

	Live2CellSource(const paf::common::SharedPtr<Live2VisualizerFactory> &visualizerFactory,
			const paf::common::SharedPtr<Live2AccessorFactory> &accessorFactory);
	virtual ~Live2CellSource();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual bool IsDirty();
	virtual void Update(paf::ui::filelist::CellListAccessor &accessor);
	virtual int64_t GetID(int32_t index);
	virtual paf::intrusive_ptr<paf::ui::filelist::Visualizer> GetVisualizer(
		paf::ui::filelist::VisualizerBindParam &param);
	virtual PafVec4 GetVisualSize();
	virtual void OnCountChanged();

	void SetAccessorFactory(const paf::common::SharedPtr<Live2AccessorFactory> &accessorFactory);
	void RequestRefresh(int refreshMode);
	void SetRefreshMode(int refreshMode)
	{
		m_refreshMode = refreshMode;
	}
	void SetCountCallback(CountCallback callback, int arg);

	static void OnQueryJobRemoved(uint32_t slotIndex,
				      paf::common::SharedPtr<paf::job::JobItem> job,
				      void *userdata);

	paf::common::SharedPtr<Live2VisualizerFactory> m_visualizerFactory;
	paf::common::SharedPtr<Live2AccessorFactory> m_accessorFactory;
	paf::common::SharedPtr<Live2Iterator> m_iterator;
	paf::common::JobSlots m_jobSlots;
	int m_refreshMode;
	int m_suspended;
	int m_pendingRefreshMode;
	CountCallback m_countCallback;
	int m_countCallbackArg;
	int m_totalCount;
	SceUInt64 m_lastReadTime;
	SceUInt64 m_lastUpdateTime;
	SceUInt64 m_latestActivityTime;
};

/*
 * BasicSegmentSource: FileList SegmentSource that queues insert/remove operations
 * and applies them during FileList::Update.
 */
class BasicSegmentSource : public paf::ui::filelist::SegmentSource {
public:
	struct SegmentEntry {
		SceInt64 segmentId;
		paf::intrusive_ptr<paf::ui::filelist::CellSource> cellSource;
	};

	struct PendingOp {
		PendingOp();
		PendingOp(int segIndex, const paf::common::SharedPtr<SegmentEntry> &segEntry,
			  const paf::ui::filelist::SegmentListAccessor::OptionParam &opt);

		int opType;
		int index;
		paf::common::SharedPtr<SegmentEntry> entry;
		paf::ui::filelist::SegmentListAccessor::OptionParam option;
	};

	static const char *TypeName();

	BasicSegmentSource();
	virtual ~BasicSegmentSource();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual bool IsDirty();
	virtual void Update(paf::ui::filelist::SegmentListAccessor &accessor);
	virtual int64_t GetID(int32_t index);
	virtual paf::intrusive_ptr<paf::ui::filelist::CellSource> GetCellSource(
		int32_t index, int64_t id);

	void InsertSegment(int index,
			   const paf::intrusive_ptr<paf::ui::filelist::CellSource> &cellSource,
			   const paf::ui::filelist::SegmentListAccessor::OptionParam &option);
	void RemoveSegment(int index);
	paf::intrusive_ptr<paf::ui::filelist::CellSource> GetSegmentCellSource(int index);

	paf::vector<paf::common::SharedPtr<SegmentEntry> > m_segments;
	paf::vector<PendingOp> m_pendingOps;
};

/* Parameters for BasicVisualizer::SetCheckBoxState */
struct CheckBoxStateParam {
	CheckBoxStateParam()
		: templateId(), animate(false), reserved(false), parentWidget(NULL)
	{
	}

	paf::IDParam templateId;
	bool animate;
	bool reserved;
	paf::ui::Widget *parentWidget;
};

/*
 * BasicVisualizer: FileList Visualizer base managing a ListItem template,
 * shadow overlay, checkbox overlay, focus/decide event forwarding, and JobSlots.
 */
class BasicVisualizer : public paf::ui::filelist::Visualizer {
public:
	static const char *TypeName();

	BasicVisualizer(paf::ui::Widget *parent, paf::Plugin *plugin,
			const paf::IDParam &templateId, const paf::IDParam &itemId);
	virtual ~BasicVisualizer();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual void Stop();
	virtual paf::ui::ListItem *GetListItem();

	virtual void OnCheckChanged(bool checked);
	virtual void LayoutCheckBox(paf::ui::Widget *checkBox);
	virtual void SetIconTexture(paf::ui::Widget *widget,
				    const paf::intrusive_ptr<paf::graph::Surface> &surf,
				    int status);
	virtual void OnFocusIn(int eventId, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnFocusOut(int eventId, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnDecide(int eventId, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnKeyEvent(int eventId, paf::ui::Handler *handler, paf::ui::Event *event);

	paf::ui::ListItem *CreateListItemWidget(paf::ui::Widget *parent,
						const paf::IDParam &templateId,
						const paf::IDParam &itemId);
	void AttachListItemEvents(paf::ui::ListItem *item);
	void SetShadowVisible(bool visible);
	void SetCheckBoxState(int checkState, const CheckBoxStateParam &param);
	paf::ui::Widget *DetachListItem();

	static void FocusInHandler(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata);
	static void FocusOutHandler(int eventId, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata);
	static void DecideHandler(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata);
	static void KeyEventHandler(int eventId, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata);
	static void CheckBoxValueChangedHandler(int eventId, paf::ui::Handler *handler,
						paf::ui::Event *event, void *userdata);
	static void DestroyShadowCallback(paf::Timer *timer, paf::ui::AnimListener *listener,
					  void *widgetArg, void *userdataArg, void *extraArg);

	paf::ui::ListItem *m_listItem;
	paf::Plugin *m_plugin;
	paf::graph::SurfacePool *m_surfacePool;
	paf::common::JobSlots m_jobSlots;
};

/*
 * Live2Visualizer: BasicVisualizer subclass that asynchronously loads a
 * Live2Record from a Live2Iterator via Live2UpdateRecordJob.
 */
class Live2Visualizer : public BasicVisualizer {
public:
	static const char *TypeName();

	Live2Visualizer(paf::ui::Widget *parent, paf::Plugin *plugin,
			const paf::IDParam &templateId, const paf::IDParam &itemId,
			const paf::common::SharedPtr<Live2Iterator> &iterator, SceInt64 cellId);
	virtual ~Live2Visualizer();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual void Stop();

	virtual void RequestRecordUpdate();
	virtual void OnRecordUpdated();
	virtual void OnRecordCleared();
	virtual void OnRecordJobRemoved(uint32_t slotIndex,
					paf::common::SharedPtr<paf::job::JobItem> job);

	paf::common::SharedPtr<Live2AccessorProxy> GetAccessorProxy();

	static void RecordJobRemovedCallback(uint32_t slotIndex,
					     paf::common::SharedPtr<paf::job::JobItem> job,
					     void *userdata);

	paf::common::JobSlots m_recordJobSlots;
	paf::intrusive_ptr<Live2Record> m_record;
	paf::common::SharedPtr<Live2Iterator> m_iterator;
	SceInt64 m_cellId;
	bool m_isRecordLoaded;
};

/*
 * BaseActivityVisualizer: Live2Visualizer subclass providing activity icon/avatar
 * rendering, URI classification and launch, profile navigation, and NP account
 * cache access.
 */
class BaseActivityVisualizer : public Live2Visualizer {
public:
	static const char *TypeName();

	BaseActivityVisualizer(paf::ui::Widget *parent, paf::Plugin *plugin,
			       paf::cache::CacheManager *cacheManager,
			       const paf::common::SharedPtr<Live2Iterator> &iterator,
			       SceInt64 cellId);
	virtual ~BaseActivityVisualizer();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual void OnRecordCleared();

	virtual void LaunchAuthorProfile();
	virtual bool IsUnread();
	virtual int OpenTemplate(paf::ui::Widget *parent, const paf::IDParam &templateId);

	void ApplyActionIcon(paf::ui::Widget *iconWidget);
	void ApplyAvatarIcon(paf::ui::Widget *avatarWidget);
	int GetCategory() const;
	void SetDisplayMode(int displayMode);
	int GetDisplayMode() const
	{
		return m_displayMode;
	}
	void UpdateCategory();

	static int CreateActivityImageCache(const paf::string &url,
					    paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef,
					    paf::cache::CacheManager *cacheManager);
	static void ApplyActionIconForRecord(int category,
					     const paf::intrusive_ptr<Live2Record> &record,
					     paf::ui::Widget *iconWidget,
					     paf::cache::CacheManager *cacheManager);
	static const paf::string &GetRecordImageUrl(const paf::intrusive_ptr<Live2Record> &record,
						    int urlKind);
	static SceUInt32 SwapColorEndian(SceUInt32 rgba, SceUInt32 *outAbgr);
	static void ApplyAvatarIconForRecord(const paf::intrusive_ptr<Live2Record> &record,
					     paf::ui::Widget *avatarWidget,
					     paf::cache::CacheManager *cacheManager);
	static void LaunchProfileForRecord(const paf::intrusive_ptr<Live2Record> &record);
	static int ClassifyUriCategory(const paf::intrusive_ptr<Live2Record> &record);
	static void LaunchActionUri(const paf::intrusive_ptr<Live2Record> &record, int category);
	static int GetVoteCount(const paf::intrusive_ptr<Live2Record> &record);
	static void TruncateAtChar(paf::string &str, char delimiter);
	static void TruncateAtWideChar(paf::wstring &str, wchar_t delimiter);
	static paf::wstring FormatActivityTime(SceUInt64 utcTick);

	paf::cache::CacheManager *m_cacheManager;
	int m_displayMode;
	int m_category;
};

/* Global Activity UI / NP cache helpers (base_activity2_visualizer.cpp) */
SceUInt32 activityUiGetCachedPanelBgColor(void);
const paf::string &activityUiGetCachedAvatarUrl(void);
const paf::string &activityUiGetCachedPanelUrl(void);
int activityUiInvokeNetAcquire(void);
int activityUiInvokeNetRelease(void);
int activityUiInvokeNetConnect(void);
int activityUiGetDialogPriority(void);

/* Opaque handle matching SceActivityDb's ActivityDbSyncHandle (4 bytes) */
struct ActivitySyncHandle {
	ActivitySyncHandle() : impl(NULL)
	{
		SceActivityDb_B83F0A77(this);
	}

	~ActivitySyncHandle()
	{
		SceActivityDb_937D49BA(this);
	}

	void *impl;
};

/* Callback pair passed to SceActivityDb_FEB3A02F (SyncDetail) */
struct ActivityDetailSyncCallback {
	void (*func)(int errorCode, int dialogId);
	int dialogId;
};

/* Forward declarations for Batch L3 classes */
class ActivityStatusDialog;
class Lv2CommentActivityVisualizer;

/*
 * ActivityBusyFooter (20 bytes, lv2comment_activity_visualizer.cpp):
 * manages the busy indicator segment header shown while posting comments.
 */
class ActivityBusyFooter {
public:
	ActivityBusyFooter(int segmentIndex, paf::Plugin *plugin);
	virtual ~ActivityBusyFooter();

	virtual void SetState(int state, paf::ui::Widget *header);
	virtual void AttachSegment(bool showHeader);
	virtual void DetachSegment();
	virtual void ShowBusyHeader();
	virtual void HideBusyHeader();
	virtual paf::IDParam GetTemplateId();
	virtual paf::IDParam GetHeaderId();
	virtual void StartBusyIndicator();

	void SetListView(paf::ui::FileList *listView)
	{
		m_listView = listView;
	}

	int m_segmentIndex;
	paf::Plugin *m_plugin;
	paf::ui::FileList *m_listView;
	int m_state;
};

/*
 * Lv1StatusActivityVisualizer (108 bytes, lv1status_activity2_visualizer.cpp):
 * renders a level-1 activity status card with avatar, panel art, body text,
 * timestamp, vote/comment badges, and busy overlay.
 */
class Lv1StatusActivityVisualizer : public BaseActivityVisualizer {
public:
	static const char *TypeName();

	Lv1StatusActivityVisualizer(paf::ui::Widget *parent, paf::Plugin *plugin,
				    paf::cache::CacheManager *cacheManager,
				    const paf::common::SharedPtr<Live2Iterator> &iterator,
				    SceInt64 cellId);
	virtual ~Lv1StatusActivityVisualizer();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual void Stop();
	virtual void OnRecordUpdated();

	virtual paf::graph::TextObj *GetBodyTextObj(paf::ui::Widget *panel);
	virtual paf::graph::TextObj *GetTimeTextObj(paf::ui::Widget *panel);
	virtual paf::ui::Widget *GetSubWidget(int kind);
	virtual paf::ui::Widget *GetCardRootWidget();
	virtual void OnActionIconDecide(int eventId, paf::ui::Handler *handler,
					paf::ui::Event *event);
	virtual void OnPanelDecide(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event);
	virtual bool CustomizeActionIcon(paf::ui::Widget *iconWidget);
	virtual PafVec4 GetPreferredSize();
	virtual bool HasNewComment();
	virtual bool ShouldShowTitleName();
	virtual void UpdateBadges();

	paf::graph::DrawObj *GetNewlineMarkObj(paf::ui::Widget *panel);
	paf::graph::TextObj *GetVoteCountTextObj(paf::ui::Widget *panel);
	paf::graph::TextObj *GetCommentCountTextObj(paf::ui::Widget *panel);

	void UpdateCard();
	void ShowBusy();
	void HideBusy();
	void StopBusyIndicator(paf::ui::Widget *panel);
	void UpdateVoteAndCommentCounts(paf::ui::Widget *panel);
	void UpdateTimeText(paf::graph::TextObj *timeText);
	void UpdateStatusText(paf::ui::Widget *listItem, paf::ui::Widget *panel);
	void UpdatePanel(paf::ui::Widget *listItem, paf::ui::Widget *panel);
	void ShowDeleteErrorDialog(int errorCode);
	void CloseErrorDialog();

	static void LayoutCountsCallback(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, void *userdata);
	static void AvatarDecideThunk(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata);
	static void ActionIconDecideThunk(int eventId, paf::ui::Handler *handler,
					  paf::ui::Event *event, void *userdata);
	static void PanelDecideThunk(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata);
	static void OnPanelImageLoaded(int eventId, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *userdata);
	static void UpdatePanelFocusShape(paf::ui::Widget *panel);

	int m_errorDialogId;
};

class Lv1StatusVisualizerFactory : public Live2VisualizerFactory {
public:
	Lv1StatusVisualizerFactory(paf::Plugin *plugin, paf::cache::CacheManager *cacheManager);
	virtual ~Lv1StatusVisualizerFactory();

	virtual paf::intrusive_ptr<paf::ui::filelist::Visualizer> CreateVisualizer(
		paf::ui::Widget *parent,
		const paf::common::SharedPtr<Live2Iterator> &iterator,
		int index, SceInt64 cellId);
	virtual PafVec4 GetVisualSize();

	paf::Plugin *m_plugin;
	paf::cache::CacheManager *m_cacheManager;
};

/*
 * ActivityStatusDialog (192 bytes, lv1status_activity2_visualizer.cpp):
 * full status detail dialog showing the status card, "Good Job" vote bar,
 * comment FileList, and comment input TextBox.
 */
class ActivityStatusDialog {
public:
	ActivityStatusDialog(paf::ui::Widget *parent, paf::Plugin *plugin,
			     paf::cache::CacheManager *cacheManager,
			     const paf::intrusive_ptr<Live2Record> &record,
			     int displayMode,
			     const paf::common::SharedPtr<Live2AccessorProxy> &accessorProxy,
			     const paf::common::SharedPtr<Live2VisualizerFactory> &commentFactory);
	virtual ~ActivityStatusDialog();

	virtual bool HandleSysUpdateError(int errorCode);
	virtual int GetVoteQueryType();
	virtual void OnAvatarDecide();
	virtual void OnStatusActionDecide();
	virtual void OnBackButtonDecide();
	virtual void OpenStatusOptionMenu(paf::ui::Widget *anchorWidget);
	virtual void OnSendComment();
	virtual void OnPopupMenuOpened();
	virtual void OnPopupMenuClosed();
	virtual float GetDialogHeight();
	virtual float GetDialogCenterY();
	virtual float GetListViewHeight();
	virtual bool ShouldAnimateOpen();
	virtual bool ShouldAnimateClose();
	virtual bool IsUnread();
	virtual bool ShouldShowTitleName();
	virtual bool UseWideCommentBox();
	virtual bool HasBackButton();
	virtual bool HasOptionButton();
	virtual const paf::IDParam &GetBackButtonTemplateId();
	virtual const paf::IDParam &GetOptionButtonTemplateId();
	virtual const paf::IDParam &GetBackButtonId();
	virtual const paf::IDParam &GetOptionButtonId();
	virtual void OnDialogOpened();
	virtual void OnDialogClosed();

	void Open(paf::ui::ListItem *anchorItem);
	void Close();
	void CloseWithReason(int reason);
	bool CustomizeStatusActionIcon(paf::ui::Widget *iconWidget);
	void SetupPanel();
	void SetupHeader(paf::ui::Widget *headerWidget);
	void SetupCommentList(const paf::common::SharedPtr<Live2AccessorProxy> &accessorProxy,
			      const paf::common::SharedPtr<Live2VisualizerFactory> &commentFactory);
	void SetupControls();
	void SyncPostAnimOverlayToCommentBox();
	void CreatePostCommentOverlay();
	void DisableCommentBox();
	void DisableSendButton();
	void EnableCommentBox();
	void EnableSendButtonIfReady();
	void UpdateHeaderTimeText(paf::graph::TextObj *timeText);
	void StartVoteBusyIndicator(paf::ui::Widget *target);
	void ShowVoteBusy(bool isZeroVotesButton);
	void StopVoteBusyIndicator(paf::ui::Widget *container);
	void UpdateGoodJobArea(int voteCount, bool hasVoteInfo);
	void SetupGoodJobContainer(paf::ui::Widget *headerRoot, float goodJobY);
	void RequestVoteInfo();
	void OnVoteInfoReady(int errorCode, const ActivityDbGoodJob &goodJob);
	void OnVoteFinished(int errorCode, bool isZeroVotesButton);
	void OnPostCommentFinished(int errorCode);
	void OnDeleteStatusFinished(int errorCode);
	void OnDeleteCommentFinished(int errorCode, SceInt64 commentRowid);
	void OnGriefReportFinished(int errorCode, SceInt64 commentRowid);
	void OnDetailSyncFinished(int errorCode);
	void OnCacheInfoUpdated();
	void OnCommentCountChanged(int count);
	void SetAnimateNewComment(bool animate);
	void ClosePopupIfSelectedComment(Lv2CommentActivityVisualizer *visualizer);
	void OpenDeleteCommentMenu(paf::ui::Widget *anchorWidget,
				   Lv2CommentActivityVisualizer *visualizer);
	void OpenDeleteOrReportCommentMenu(paf::ui::Widget *anchorWidget,
					   Lv2CommentActivityVisualizer *visualizer);
	void OnDeleteCommentMenuSelected();
	void OnGriefReportMenuSelected();
	void ShowDeleteStatusConfirmDialog();
	void ShowDeleteCommentConfirmDialog(const paf::intrusive_ptr<Live2Record> &commentRecord);
	void ShowErrorDialog(int errorCode, bool closeStatusDialogOnDismiss);
	void ShowTooManyJobsDialog();
	void StartDeleteStatus();
	void StartDeleteComment(const paf::intrusive_ptr<Live2Record> &commentRecord);
	void StartGriefReport(const paf::intrusive_ptr<Live2Record> &commentRecord);
	void OnVoteGoodJob();
	void OnCancelGoodJob();
	void SetInputEnabled(bool enabled);
	void StartPostCommentAnimation(bool animateOverlay);
	float GetEditCommentBarY();
	void UpdatePostCommentAnimation();

	int m_dialogId;
	paf::Plugin *m_plugin;
	ActivityBusyFooter m_busyFooter;
	paf::cache::CacheManager *m_cacheManager;
	paf::intrusive_ptr<Live2Record> m_record;
	int m_displayMode;
	paf::ui::Widget *m_dialogRoot;
	paf::ui::FileList *m_listView;
	paf::common::JobSlots m_jobSlots;
	ActivitySyncHandle m_syncHandle;
	ActivityDbGoodJob m_voteInfo;
	float m_goodJobY;
	int m_commonDialogId;
	bool m_popupMenuOpen;
	bool m_syncingDetail;
	bool m_postingComment;
	bool m_inputDisabled;
	bool m_animateNewComment;
	int m_commentCount;
	paf::intrusive_ptr<Lv2CommentActivityVisualizer> m_selectedCommentVis;
	paf::ui::Widget *m_postAnimWidget;
	float m_postAnimTime;
	PafVec4 m_postAnimStartPos;
	PafVec4 m_postAnimStartSize;
	PafVec4 m_anchorSize;
};

/*
 * Lv2CommentActivityVisualizer (108 bytes, lv2comment_activity_visualizer.cpp):
 * renders a comment row inside ActivityStatusDialog.
 */
class Lv2CommentActivityVisualizer : public BaseActivityVisualizer {
public:
	static const char *TypeName();

	Lv2CommentActivityVisualizer(paf::ui::Widget *parent, paf::Plugin *plugin,
				     paf::cache::CacheManager *cacheManager,
				     const paf::common::SharedPtr<Live2Iterator> &iterator,
				     SceInt64 cellId,
				     const paf::intrusive_ptr<Live2Record> &parentRecord);
	virtual ~Lv2CommentActivityVisualizer();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual void Stop();
	virtual void OnRecordUpdated();

	virtual paf::graph::TextObj *GetBodyTextObj(paf::ui::Widget *panel);
	virtual paf::graph::TextObj *GetTimeTextObj(paf::ui::Widget *panel);
	virtual paf::ui::Widget *GetSubWidget(int kind);
	virtual void OnDeleteOptionDecide(int eventId, paf::ui::Handler *handler,
					  paf::ui::Event *event);
	virtual void OnDeleteOrReportOptionDecide(int eventId, paf::ui::Handler *handler,
						  paf::ui::Event *event);
	virtual void UpdateUnreadBadge();

	void UpdateCard();
	void StartCommentBusyOnButton(paf::ui::Widget *optionButton);
	void ShowCommentBusy();
	void StopCommentBusyOnButton(paf::ui::Widget *optionButton);
	void OnDeleteFinished(int errorCode);
	void OnGriefReportFinished();
	void UpdateCommentTime(paf::graph::TextObj *timeText);
	void UpdateCommentText(paf::ui::Widget *panel, paf::ui::Widget *bodyWidget,
			       bool newlyCreated);
	void UpdateCommentPanel(paf::ui::Widget *panel, paf::ui::Widget *bodyWidget);

	static void AnimateNewComment(paf::ui::Widget *commentWidget);
	static void OnCommentSizeReady(int eventId, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *userdata);
	static void DeleteOptionDecideThunk(int eventId, paf::ui::Handler *handler,
					    paf::ui::Event *event, void *userdata);

	paf::intrusive_ptr<Live2Record> m_parentRecord;
};

class Lv2CommentVisualizerFactory : public Live2VisualizerFactory {
public:
	Lv2CommentVisualizerFactory(paf::Plugin *plugin, paf::cache::CacheManager *cacheManager,
				    const paf::intrusive_ptr<Live2Record> &parentRecord);
	virtual ~Lv2CommentVisualizerFactory();

	virtual paf::intrusive_ptr<paf::ui::filelist::Visualizer> CreateVisualizer(
		paf::ui::Widget *parent,
		const paf::common::SharedPtr<Live2Iterator> &iterator,
		int index, SceInt64 cellId);
	virtual PafVec4 GetVisualSize();

	paf::Plugin *m_plugin;
	paf::cache::CacheManager *m_cacheManager;
	paf::intrusive_ptr<Live2Record> m_parentRecord;
};

/*
 * NullVisualizerFactory & NullCellSource (28 bytes, null_cell_source.cpp):
 * simple count-driven CellSource backed by a single-method factory.
 */
class NullVisualizerFactory {
public:
	virtual paf::intrusive_ptr<paf::ui::filelist::Visualizer> CreateVisualizer(
		paf::ui::Widget *parent, int index) = 0;
};

class NullCellSource : public paf::ui::filelist::CellSource {
public:
	static const char *TypeName();

	NullCellSource(const paf::common::SharedPtr<NullVisualizerFactory> &factory,
		       int initialCount);
	virtual ~NullCellSource();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual bool IsDirty();
	virtual void Update(paf::ui::filelist::CellListAccessor &accessor);
	virtual int64_t GetID(int32_t index);
	virtual paf::intrusive_ptr<paf::ui::filelist::Visualizer> GetVisualizer(
		paf::ui::filelist::VisualizerBindParam &param);

	paf::common::SharedPtr<NullVisualizerFactory> m_factory;
	int m_pendingCount;
	int m_focusIndex;
	int m_pendingFocus;
};

/* Activity service & dialog registry cross-unit helpers (lv1status_activity2_visualizer.cpp) */
ActivityStatusDialog *activityUiFindDialogByStatusRowid(const SceInt64 *statusRowid);
ActivityStatusDialog *activityUiFindDialogById(int dialogId);
bool activityUiIsStatusDeleting(const SceInt64 *statusRowid);
bool activityUiIsCommentDeleting(const SceInt64 *commentRowid);
bool activityUiIsStatusPostingComment(const SceInt64 *statusRowid);
void activityUiOpenDeleteCommentMenu(const SceInt64 *statusRowid, paf::ui::Widget *anchorWidget,
				     Lv2CommentActivityVisualizer *visualizer);
void activityUiOpenDeleteOrReportCommentMenu(const SceInt64 *statusRowid,
					     paf::ui::Widget *anchorWidget,
					     Lv2CommentActivityVisualizer *visualizer);
void activityUiInitFileList(paf::ui::FileList *fileList);
void activityUiCloseDialog(int dialogId);
void activityUiCreateStatusContexts(paf::common::SharedPtr<Live2AccessorProxy> *outProxy,
				    paf::common::SharedPtr<Live2Accessor> *outAccessor,
				    const ActivityDbQueryCondition *condition);

/* Activity grief report export helpers (grief_report.cpp) */
int activityGriefReportBegin(const ActivityDbStatus *status, const ActivityDbComment *comment);
int activityGriefReportAppendComment(const ActivityDbComment *comment);
int activityGriefReportEnd(bool launchReportApp);

} /* namespace filelist */

/* Activity UI module lifecycle & callback setup (linked by livearea_plugin) */
void activityUiInit(void);
void activityUiTerm(void);
void activityUiSetNetCallbacks(void (*acquireFn)(void), void (*releaseFn)(void),
			       void (*connectFn)(void));
void activityUiSetDialogPriority(int priority);
void activityUiEnqueueCacheInfoJob(void);
void activityUiCloseDialog(void);
void activityUiCloseDialogFast(void);
int activityUiIsDialogActive(void);

#endif /* SHELL_FILELIST_INTERNAL_H */
