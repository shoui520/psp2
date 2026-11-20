/*
 * np_friend_storage.h: the NP friend storage (0x810F60FC-0x810FB388), the shell's copy of the
 * friend list, the blocked players and the online play session log, kept in an SQLite database
 * on application data 101 (/database/friends.db), the tables tbl_game_psn_players and
 * tbl_game_online_play_session_log. It allocates from the heap it is opened with. The friend list
 * and the importer read and write it. No record or string names the library, its functions or
 * its types beyond the mutex's name (SceNpFriendVshStorate): the names are ours (PLACEHOLDER).
 */
#ifndef _SHELL_NP_FRIEND_STORAGE_H
#define _SHELL_NP_FRIEND_STORAGE_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_npid.h>
#include <np/np_common.h>
#include <std/string>
#include <std/memory>
#include <common/ref_obj.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>
#include <datetime/datetime.h>	/* paf::rtc (<rtc/rtc.h> is the kernel's) */
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include <dbutil/result.h>

/* PLACEHOLDER names: the storage's errors */
#define NP_FRIEND_STORAGE_ERROR_INVALID_ARGUMENT	0x80558B01
#define NP_FRIEND_STORAGE_ERROR_ALREADY_OPEN		0x80558B03
#define NP_FRIEND_STORAGE_ERROR_NOT_OPEN		0x80558B04
#define NP_FRIEND_STORAGE_ERROR_NO_MEMORY		0x80558B05
#define NP_FRIEND_STORAGE_ERROR_INVALID_PLAYER_TYPE	0x80558B07
#define NP_FRIEND_STORAGE_ERROR_NOT_FOUND		0x80558B09
#define NP_FRIEND_STORAGE_ERROR_OUT_OF_RANGE		0x80558B0A

/* PLACEHOLDER names: a player's type (player_type); 2 to 4 are the friends counted against the
   limit of NP_FRIEND_STORAGE_FRIENDS_MAX, 5 the blocked players */
#define NP_FRIEND_PLAYER_TYPE_1		1
#define NP_FRIEND_PLAYER_TYPE_2		2
#define NP_FRIEND_PLAYER_TYPE_3		3
#define NP_FRIEND_PLAYER_TYPE_4		4
#define NP_FRIEND_PLAYER_TYPE_BLOCKED	5

/* the friends kept at most, and the session log's entries */
#define NP_FRIEND_STORAGE_FRIENDS_MAX	100
#define NP_FRIEND_SESSION_LOG_MAX	50

/* which players DeletePlayer removes */
#define NP_FRIEND_DELETE_FRIEND		1
#define NP_FRIEND_DELETE_BLOCKED	2

/* which session log entries FindSessionLogs finds */
#define NP_FRIEND_SESSION_LOG_ALL	0
#define NP_FRIEND_SESSION_LOG_TITLE	1	/* one title's (sce_np_communication_id) */

/* the storage's heap (np_friend_storage_heap.cpp) */
void npFriendStorageSetHeap(paf::memory::HeapAllocator *heap);		/* (0x810FB33C) */
void npFriendStorageClearHeap(void);					/* (0x810FB348) */
void *npFriendStorageMalloc(SceSize size);				/* (0x810FB356) */
void npFriendStorageFree(void *ptr);					/* (0x810FB372) */

/* a container allocator over the storage's heap; a failed allocation is not reported */
template<class _Ty>
class NpFriendStorageAllocator {
public:
	_Ty *allocate(size_t count)
	{
		return (_Ty *)npFriendStorageMalloc(count * sizeof(_Ty));
	}

	void deallocate(_Ty *ptr, size_t)
	{
		npFriendStorageFree(ptr);
	}

	void report_failure()
	{
	}
};

/* the storage's SQL statements are built in its heap */
typedef paf::basic_string<char, paf::char_traits<char>, NpFriendStorageAllocator<char> >
	NpFriendString;

/* a player of the friend list (the friend list's class; the storage reads it only) */
class NpFriendPlayer : public paf::common::RefObj {
public:
	virtual const SceNpId *GetNpId() = 0;
	virtual const char *GetOnlineId() = 0;
	virtual int GetPlayerType() = 0;
	virtual const paf::rtc::Tick *GetFriendRequestDate() = 0;
};

/* a player's profile, as UpdateProfile writes it */
typedef struct NpFriendProfile {
	const char *avatarUrl;
	const char *aboutMe;
	const char *panelUrl;		/* NULL for none */
	unsigned int color;
	SceUInt8 language1;
	SceUInt8 language2;
	SceUInt8 language3;
	unsigned int plus;
} NpFriendProfile;

/* an entry of the online play session log */
struct NpFriendSessionLog {
	int reserved;
	SceNpId npId;
	paf::rtc::Tick date;
	paf::string titleId;
	SceNpCommunicationId communicationId;
	paf::string shortTitleName;
	paf::string longTitleName;
	paf::string description;
};

class NpFriendStorage {
public:
	NpFriendStorage()
		: m_path(NULL), m_connection(s_connection), m_lock("SceNpFriendVshStorate", 0),
		  m_friendsCleared(0), m_blockedCleared(0)
	{
	}

	virtual ~NpFriendStorage()
	{
	}

	int BuildPath();
	static int Open(paf::memory::HeapAllocator *heap);
	static int Close();
	static int GetInstance(NpFriendStorage **storage);

	int DeleteBlocked();
	int DeleteFriends();
	int Clear();
	static bool PlayerExists(dbutil::Connection *connection,
				 paf::intrusive_ptr<NpFriendPlayer> player);
	static int UpdatePlayerType(dbutil::Connection *connection,
				    paf::intrusive_ptr<NpFriendPlayer> player);
	static int InsertPlayer(dbutil::Connection *connection,
				paf::intrusive_ptr<NpFriendPlayer> player);
	int StorePlayer(paf::intrusive_ptr<NpFriendPlayer> player);
	int StoreFriends(unsigned int count, const paf::intrusive_ptr<NpFriendPlayer> *players,
			 bool clear);
	int StoreBlocked(unsigned int count, const paf::intrusive_ptr<NpFriendPlayer> *players,
			 bool clear);
	int GetFriendCount(unsigned int *count);
	int AddFriends(unsigned int count, const paf::intrusive_ptr<NpFriendPlayer> *players,
		       int *added);
	int DeletePlayer(const char *onlineId, int which);
	int DeletePlayers(unsigned int count, const SceNpId *npIds, int which);
	int UpdatePlayerTypes(unsigned int count, const paf::intrusive_ptr<NpFriendPlayer> *players);
	int UpdateProfile(const char *onlineId, const NpFriendProfile *profile, int msgFlag);
	int DeleteOldestSessionLog();
	static int FindSessionLogs(dbutil::Connection *connection, dbutil::Result *result, int which,
				   const SceNpCommunicationId *communicationId);
	int GetSessionLogCount(int which, const SceNpCommunicationId *communicationId,
			       unsigned int *count);
	int AddSessionLog(const NpFriendSessionLog *log);
	int GetSessionLogs(int which, const SceNpCommunicationId *communicationId,
			   unsigned int start, unsigned int num, NpFriendSessionLog **logs,
			   unsigned int *got);
	int DeleteSessionLog(SceInt64 logId);
	int DeleteSessionLogs();
	static int FindPlayerSessionLogs(dbutil::Connection *connection, dbutil::Result *result,
					 const char *onlineId);
	int GetPlayerSessionLogCount(const char *onlineId, unsigned int *count);
	int GetPlayerSessionLog(const char *onlineId, unsigned int index, NpFriendSessionLog *log);

	static NpFriendStorage *s_instance;		/* the storage open */
	static dbutil::Connection *s_connection;	/* its database connection */

	char *m_path;			/* the database's, in the storage's heap */
	dbutil::Connection *m_connection;
	paf::thread::RMutex m_lock;
	int m_friendsCleared;		/* the friends were removed once before the first stored */
	int m_blockedCleared;		/* and the blocked players */
};

#endif
