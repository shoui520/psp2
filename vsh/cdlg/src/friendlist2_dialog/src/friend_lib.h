/*
 * SceFriendList2DialogPlugin: SceFriendLib shared core
 * (SceFriendLibList, SceFriendLibNp, SceFriendLibJobQueue,
 *  SceFriendLibImageManager, UpdateButtonWidget).
 */
#ifndef FRIENDLIST2_DIALOG_FRIEND_LIB_H
#define FRIENDLIST2_DIALOG_FRIEND_LIB_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <np/np_basic.h>
#include <std/list>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <job/job.h>
#include <buffer/buffer.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>
#include <widget/w_plane_button.h>
#include "friend_select_webapi.h"
#include "friend_lib_http.h"

/* Relation enum parsed from "relation" JSON field */
enum FriendRelation {
	FRIEND_RELATION_NONE = 0,
	FRIEND_RELATION_SELF = 1,
	FRIEND_RELATION_FRIEND = 2,
	FRIEND_RELATION_REQUESTING = 3,
	FRIEND_RELATION_REQUESTED = 4,
	FRIEND_RELATION_BLOCKED = 5,
	FRIEND_RELATION_OTHER = 6
};

/* Presence status enum parsed from "onlineStatus" JSON field */
enum FriendOnlineStatus {
	FRIEND_STATUS_UNKNOWN = 0,
	FRIEND_STATUS_OFFLINE = 1,
	FRIEND_STATUS_STANDBY = 2,
	FRIEND_STATUS_ONLINE = 3
};

/* Platform enum parsed from "platform" JSON field */
enum FriendPlatform {
	FRIEND_PLATFORM_PSVITA = 0,
	FRIEND_PLATFORM_PS4 = 1,
	FRIEND_PLATFORM_PS3 = 2,
	FRIEND_PLATFORM_OTHER = 3
};

/* Parsed user profile entry from PSN friendList WebAPI (100 bytes in original) */
struct FriendUserProfile {
	FriendUserProfile();
	virtual ~FriendUserProfile();

	bool IsSelf() const;
	void SetRelationFromString(const paf::string &relationStr);
	void SetTrophyLevel(int32_t level);
	void SetOnlineStatusFromString(const paf::string &statusStr);

	int32_t relation;
	SceNpId npId;
	paf::string avatarUrl;
	uint32_t panelBgColor;
	paf::string panelUrl;
	int32_t onlineStatus;
	int32_t platform;
	paf::string gameTitleName;
	int32_t trophyLevel;
	bool isPlus;
	bool hasRequestMessage;
};

/* One cached page of 20 friend entries */
class FriendListPageCache {
public:
	FriendListPageCache();
	virtual ~FriendListPageCache();

	int InitJsonParser(void *parserCtx, uint32_t reserveCount);
	void Clear();

	int32_t defaultRelation;
	int32_t startOffset;
	paf::vector<FriendUserProfile> profiles;
	int32_t totalResults;
};

/* 3-page LRU cache of friend list entries ("SceFriendLibList") */
class FriendListCache {
public:
	FriendListCache();
	virtual ~FriendListCache();

	void ClearAll();
	void SetSortByStatus(bool sortByStatus);
	int BeginFetch();
	void EndFetch(int result);
	const FriendUserProfile *GetProfileAt(int32_t index);

	paf::thread::RMutex m_mutex;
	paf::list<FriendListPageCache *> m_pages;
	FriendListPageCache *m_fetchingPage;
	int32_t m_totalResults;
	int32_t m_sortByStatus;
	bool m_invalidated;
};

/* Async NP login/NpId fetch job ("SceFriendLibNp") */
class FriendLibNpJob : public FriendLibQueuedTask {
public:
	FriendLibNpJob(SceNpId *outNpId, FriendLibTaskCallback *callback);
	virtual ~FriendLibNpJob();
	virtual int Run();

	SceNpId *m_outNpId;
};

/* Clock jump watcher callback interface */
class FriendLibClockListener {
public:
	virtual ~FriendLibClockListener() {}
	virtual void OnClockChanged() = 0;
	virtual bool MatchesOwner(void *owner) const = 0;
};

/* Image cache callback interface */
class FriendLibImageRequestCallback {
public:
	virtual ~FriendLibImageRequestCallback() {}
	virtual void OnImageReady(const paf::string &url, int result,
				  paf::intrusive_ptr<paf::graph::Surface> *surface) = 0;
};

/* Custom rotating refresh button widget ("update_button" / "style_update_button") */
class UpdateButtonWidget : public paf::ui::PlaneButton {
public:
	UpdateButtonWidget(paf::ui::Widget *parent, paf::ui::Widget::CreateParam *param);
	virtual ~UpdateButtonWidget();
	virtual bool IsInherit(const char *name) const;
	virtual char *GetTypeName() const;

	static const char *TypeName();
	bool StartSpin();
	void StopSpin();

	bool m_spinning;
};

/* Global SceFriendLib lifecycle */
void FriendLibInit(paf::memory::HeapAllocator *allocator, paf::graph::SurfacePool *surfacePool);
void FriendLibTerm(void);
void FriendLibStart(void);
void FriendLibStop(void);
void FriendLibRegisterWidgetFactory(void);
void FriendLibUnregisterWidgetFactory(void);

paf::job::JobQueue *FriendLibGetJobQueue(void);
paf::graph::SurfacePool *FriendLibGetSurfacePool(void);
void FriendLibSetSelfOnlineId(const char *onlineId);
const char *FriendLibGetSelfOnlineId(void);

void FriendLibAddClockListener(FriendLibClockListener *listener);
void FriendLibRemoveClockListener(void *owner);
void FriendLibPlaySystemSound(uint32_t soundIndex);
void FriendLibNormalizeNewlines(paf::string &text);

void FriendLibRequestImage(const char *url, FriendLibImageRequestCallback *callback,
			   int32_t maxWidth, int32_t maxHeight);
void FriendLibCancelImageRequests(const char *url);
void FriendLibClearImageCache(void);

#endif /* FRIENDLIST2_DIALOG_FRIEND_LIB_H */
