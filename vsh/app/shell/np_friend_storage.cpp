/*
 * np_friend_storage.cpp: the NP friend storage (0x810F60FC-0x810FB33C): the friend list, the
 * blocked players and the online play session log in friends.db on application data 101. Every
 * statement is built as text in the storage's heap; the players' and the entries' values are
 * bound. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/string>
#include <std/memory>
#include <datetime/datetime.h>	/* paf::rtc (<rtc/rtc.h> is the kernel's) */
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <sqlite3.h>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include <dbutil/record.h>
#include <dbutil/rowid_result.h>
#include "shell.h"
#include "np_friend_storage.h"

using namespace paf;

/* the application data the database is on, and its path there */
#define NP_FRIEND_STORAGE_APPDATA_ID	101
#define NP_FRIEND_STORAGE_DATABASE	"/database/friends.db"

/* PLACEHOLDER: how the database is opened (0x10000000 is not SQLite's) */
#define NP_FRIEND_STORAGE_OPEN_FLAGS	(0x10000000 | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE)

/* how long a statement waits for a locked database, in milliseconds */
#define NP_FRIEND_STORAGE_BUSY_TIMEOUT	30000

/* the tables and their columns */
#define NP_FRIEND_TABLE_PLAYERS		"tbl_game_psn_players"
#define NP_FRIEND_TABLE_SESSION_LOG	"tbl_game_online_play_session_log"

/* the size of the dates' text (RFC 3339) */
#define NP_FRIEND_DATE_SIZE		64

/* (0x815300E0) every column of a record */
static const char s_allColumns[] = "*";

/* (0x81544774, 0x81544778) */
NpFriendStorage *NpFriendStorage::s_instance = NULL;
dbutil::Connection *NpFriendStorage::s_connection = NULL;

static void npFriendStorageError(void *arg, int result);

/* (0x810F60FC) */
int npFriendStorageInit(memory::HeapAllocator *heap)
{
	return NpFriendStorage::Open(heap);
}

/* (0x810F6104) */
int npFriendStorageTerm(void)
{
	return NpFriendStorage::Close();
}

/* (0x810F610C) the database's path, kept in the storage's heap */
int NpFriendStorage::BuildPath()
{
	NpFriendString path;
	ScePfsRndDriveId mountPoint;

	sce_paf_memset(&mountPoint, 0, sizeof(mountPoint));

	int ret = sceAppMgrAppDataMount(NP_FRIEND_STORAGE_APPDATA_ID, &mountPoint);

	if (ret >= 0) {
		path.assign(mountPoint.drive_id, sce_paf_strlen(mountPoint.drive_id));
		path += NP_FRIEND_STORAGE_DATABASE;
		m_path = (char *)npFriendStorageMalloc(sce_paf_strlen(path.c_str()) + 1);
		if (m_path != NULL)
			sce_paf_strncpy(m_path, path.c_str(), sce_paf_strlen(path.c_str()) + 1);
		else
			ret = NP_FRIEND_STORAGE_ERROR_NO_MEMORY;
	}
	return ret;
}

/* (0x810F6214) the storage made on heap, and its database opened */
int NpFriendStorage::Open(memory::HeapAllocator *heap)
{
	static dbutil::Connection connection;

	s_connection = &connection;
	if (s_instance != NULL)
		return NP_FRIEND_STORAGE_ERROR_ALREADY_OPEN;
	npFriendStorageSetHeap(heap);

	int ret = 0;
	bool opened = false;
	void *mem = npFriendStorageMalloc(sizeof(NpFriendStorage));

	do {
		if (mem != NULL)
			s_instance = new (mem) NpFriendStorage();
		else
			ret = NP_FRIEND_STORAGE_ERROR_NO_MEMORY;
		if (ret < 0)
			break;
		ret = s_instance->BuildPath();
		if (ret < 0)
			break;
		ret = s_instance->m_connection->Open(string(s_instance->m_path),
						     NP_FRIEND_STORAGE_OPEN_FLAGS);
		if (ret < 0)
			break;
		opened = true;
		ret = s_instance->m_connection->SetBusyTimeout(NP_FRIEND_STORAGE_BUSY_TIMEOUT);
		if (ret < 0)
			break;
		{
			dbutil::Connection::ErrorHandler handler = { npFriendStorageError, s_instance };

			ret = s_connection->SetErrorHandler(&handler);
		}
		if (ret < 0)
			break;
		return ret;
	} while (0);

	if (opened)
		s_instance->m_connection->Close();
	npFriendStorageClearHeap();
	return ret;
}

/* (0x810F63F4) the storage closed, and its application data unmounted */
__attribute__((noinline)) int NpFriendStorage::Close()
{
	ScePfsRndDriveId mountPoint;

	if (s_instance == NULL)
		return NP_FRIEND_STORAGE_ERROR_NOT_OPEN;
	if (s_instance->m_path != NULL) {
		npFriendStorageFree(s_instance->m_path);
		s_instance->m_path = NULL;
	}
	s_instance->~NpFriendStorage();
	npFriendStorageFree(s_instance);
	s_instance = NULL;
	npFriendStorageClearHeap();
	s_connection->Close();
	sce_paf_memset(&mountPoint, 0, sizeof(mountPoint));
	sceAppMgrAppDataMount(NP_FRIEND_STORAGE_APPDATA_ID, &mountPoint);
	sceAppMgrUmount(mountPoint.drive_id);
	return 0;
}

/* (0x810F648C) */
int NpFriendStorage::GetInstance(NpFriendStorage **storage)
{
	if (s_instance == NULL)
		return NP_FRIEND_STORAGE_ERROR_NOT_OPEN;
	*storage = s_instance;
	return 0;
}

/* (0x810F64EC) the blocked players removed */
int NpFriendStorage::DeleteBlocked()
{
	NpFriendString sql("DELETE FROM " NP_FRIEND_TABLE_PLAYERS);
	dbutil::RefPtr<dbutil::Statement> statement;
	dbutil::Value type;

	sql += " WHERE player_type = ?;";

	int ret = m_connection->Prepare(string(sql.c_str()), &statement);

	if (ret >= 0) {
		sce_paf_memset(&type, 0, sizeof(type));
		type.type = SQLITE_INTEGER;
		type.data.integer = NP_FRIEND_PLAYER_TYPE_BLOCKED;
		ret = statement->Bind(1, &type, false);
		if (ret >= 0) {
			ret = statement->Step(1);
			statement->Reset();
		}
	}
	return ret;
}

/* (0x810F6642) the friends removed */
int NpFriendStorage::DeleteFriends()
{
	NpFriendString sql("DELETE FROM " NP_FRIEND_TABLE_PLAYERS);
	dbutil::RefPtr<dbutil::Statement> statement;
	dbutil::Value typeFriend;
	dbutil::Value typeRequestRecv;
	dbutil::Value typeRequestSent;
	dbutil::Value typePending;

	sql += " WHERE ";
	sql += "player_type";
	sql += " = ? OR ";
	sql += "player_type";
	sql += " = ? OR ";
	sql += "player_type";
	sql += " = ? OR ";
	sql += "player_type";
	sql += " = ?;";

	int ret = m_connection->Prepare(string(sql.c_str()), &statement);

	if (ret >= 0) {
		sce_paf_memset(&typeFriend, 0, sizeof(typeFriend));
		typeFriend.type = SQLITE_INTEGER;
		typeFriend.data.integer = NP_FRIEND_PLAYER_TYPE_2;
		sce_paf_memset(&typeRequestRecv, 0, sizeof(typeRequestRecv));
		typeRequestRecv.type = SQLITE_INTEGER;
		typeRequestRecv.data.integer = NP_FRIEND_PLAYER_TYPE_3;
		sce_paf_memset(&typeRequestSent, 0, sizeof(typeRequestSent));
		typeRequestSent.type = SQLITE_INTEGER;
		typeRequestSent.data.integer = NP_FRIEND_PLAYER_TYPE_4;
		sce_paf_memset(&typePending, 0, sizeof(typePending));
		typePending.type = SQLITE_INTEGER;
		typePending.data.integer = NP_FRIEND_PLAYER_TYPE_1;
		ret = statement->Bind(1, &typeFriend, false);
		if (ret >= 0) {
			ret = statement->Bind(2, &typeRequestRecv, false);
			if (ret >= 0) {
				ret = statement->Bind(3, &typeRequestSent, false);
				if (ret >= 0) {
					ret = statement->Bind(4, &typePending, false);
					if (ret >= 0) {
						ret = statement->Step(1);
						statement->Reset();
					}
				}
			}
		}
	}
	return ret;
}

/* (0x810F6ADA) every player removed */
int NpFriendStorage::Clear()
{
	m_lock.Lock();

	int ret = m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);

	if (ret >= 0) {
		ret = DeleteFriends();
		if (ret >= 0 && (ret = DeleteBlocked()) >= 0)
			ret = m_connection->Commit();
		else
			m_connection->Rollback();
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810F6B28) */
bool NpFriendStorage::PlayerExists(dbutil::Connection *connection,
				   intrusive_ptr<NpFriendPlayer> player)
{
	bool exists = false;
	NpFriendString sql;
	dbutil::RowidResult result;
	dbutil::Value onlineId;

	sql = "SELECT rowid FROM ";
	sql += NP_FRIEND_TABLE_PLAYERS;
	sql += " WHERE ";
	sql += "online_id";
	sql += " = ?;";
	onlineId.type = SQLITE_TEXT;
	onlineId.data.text = player->GetOnlineId();
	onlineId.size = sce_paf_strlen(player->GetOnlineId());
	int ret = connection->Execute(string(sql.c_str()), 1, &onlineId, &result, NULL, NULL);

	if (ret >= 0 && result.m_rowCount > 0)
		exists = true;
	return exists;
}

/* (0x810F6D80) */
int NpFriendStorage::UpdatePlayerType(dbutil::Connection *connection,
				      intrusive_ptr<NpFriendPlayer> player)
{
	NpFriendString sql("UPDATE ");
	dbutil::RefPtr<dbutil::Statement> statement;
	dbutil::Value type;
	dbutil::Value onlineId;

	sql += NP_FRIEND_TABLE_PLAYERS;
	sql += " SET ";
	sql += "player_type";
	sql += " = ? ";
	sql += "WHERE ";
	sql += "online_id";
	sql += " = ?;";

	int ret = connection->Prepare(string(sql.c_str()), &statement);

	if (ret >= 0) {
		type.type = SQLITE_INTEGER;
		type.data.integer = player->GetPlayerType();
		onlineId.type = SQLITE_TEXT;
		onlineId.data.text = player->GetOnlineId();
		onlineId.size = sce_paf_strlen(player->GetOnlineId());
		ret = statement->Bind(1, &type, false);
		if (ret >= 0) {
			ret = statement->Bind(2, &onlineId, false);
			if (ret >= 0)
				ret = statement->Step(1);
		}
	}
	return ret;
}

/* (0x810F7128) */
int NpFriendStorage::InsertPlayer(dbutil::Connection *connection,
				  intrusive_ptr<NpFriendPlayer> player)
{
	NpFriendString sql("INSERT INTO ");
	dbutil::RefPtr<dbutil::Statement> statement;
	dbutil::Value onlineId;
	dbutil::Value type;
	dbutil::Value npId;
	dbutil::Value date;
	char dateText[NP_FRIEND_DATE_SIZE];

	sql += NP_FRIEND_TABLE_PLAYERS;
	sql += " (";
	sql += "online_id";
	sql += ", ";
	sql += "player_type";
	sql += ", ";
	sql += "sce_np_id";
	sql += ", ";
	sql += "friend_req_received_date";
	sql += ") VALUES ( ?, ?, ?, ? );";

	int ret = connection->Prepare(string(sql.c_str()), &statement);

	if (ret >= 0) {
		onlineId.type = SQLITE_TEXT;
		onlineId.data.text = player->GetOnlineId();
		onlineId.size = sce_paf_strlen(player->GetOnlineId());
		type.type = SQLITE_INTEGER;
		type.data.integer = player->GetPlayerType();
		npId.type = SQLITE_BLOB;
		npId.data.blob = player->GetNpId();
		npId.size = sizeof(SceNpId);
		date.type = SQLITE_TEXT;
		ret = rtc::FormatRFC3339(dateText, player->GetFriendRequestDate(), 0);
		if (ret >= 0) {
			date.data.text = dateText;
			date.size = sce_paf_strlen(dateText);
			ret = statement->Bind(1, &onlineId, false);
			if (ret >= 0) {
				ret = statement->Bind(2, &type, false);
				if (ret >= 0) {
					ret = statement->Bind(3, &npId, false);
					if (ret >= 0) {
						ret = statement->Bind(4, &date, false);
						if (ret >= 0) {
							ret = statement->Step(1);
							statement->Reset();
						}
					}
				}
			}
		}
	}
	return ret;
}

/* (0x810F7648) the player added, or its type changed; the first friend or blocked player stored
   removes the ones there were */
int NpFriendStorage::StorePlayer(intrusive_ptr<NpFriendPlayer> player)
{
	if (player->GetPlayerType() == NP_FRIEND_PLAYER_TYPE_BLOCKED) {
		if (!m_blockedCleared) {
			DeleteBlocked();
			m_blockedCleared = 1;
		}
	} else if (!m_friendsCleared) {
		DeleteFriends();
		m_friendsCleared = 1;
	}
	if (PlayerExists(m_connection, player))
		return UpdatePlayerType(m_connection, player);
	return InsertPlayer(m_connection, player);
}

/* (0x810F76F8) the friends replaced by count players */
int NpFriendStorage::StoreFriends(unsigned int count, const intrusive_ptr<NpFriendPlayer> *players,
				  bool clear)
{
	m_lock.Lock();

	int ret = m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);

	if (ret >= 0) {
		if (count != 0 || clear) {
			ret = DeleteFriends();
			if (ret >= 0) {
				for (unsigned int i = 0; i < count; i++, players++) {
					if ((*players)->GetPlayerType() != NP_FRIEND_PLAYER_TYPE_4 &&
					    (*players)->GetPlayerType() != NP_FRIEND_PLAYER_TYPE_2 &&
					    (*players)->GetPlayerType() != NP_FRIEND_PLAYER_TYPE_3) {
						ret = NP_FRIEND_STORAGE_ERROR_INVALID_PLAYER_TYPE;
						break;
					}
					ret = StorePlayer(*players);
					if (ret < 0)
						break;
				}
			}
			if (ret >= 0)
				m_friendsCleared = 1;
		}
		if (ret < 0)
			m_connection->Rollback();
		else
			ret = m_connection->Commit();
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810F77DE) the blocked players replaced by count players */
int NpFriendStorage::StoreBlocked(unsigned int count, const intrusive_ptr<NpFriendPlayer> *players,
				  bool clear)
{
	m_lock.Lock();

	int ret = m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);

	if (ret < 0) {
		m_lock.Unlock();
		return ret;
	}
	if (count != 0 || clear) {
		ret = DeleteBlocked();
		if (ret >= 0) {
			for (unsigned int i = 0; i < count; i++, players++) {
				if ((*players)->GetPlayerType() != NP_FRIEND_PLAYER_TYPE_BLOCKED) {
					ret = NP_FRIEND_STORAGE_ERROR_INVALID_PLAYER_TYPE;
					break;
				}
				ret = StorePlayer(*players);
				if (ret < 0)
					break;
			}
		}
		if (ret >= 0)
			m_blockedCleared = 1;
	}
	if (ret < 0)
		m_connection->Rollback();
	else
		ret = m_connection->Commit();
	m_lock.Unlock();
	return ret;
}

/* a value as SQL text */
static inline string npFriendInt64ToString(SceInt64 value)
{
	char text[64];

	sce_paf_snprintf(text, sizeof(text), "%lld", value);
	return string(text);
}

/* (0x810F78C8) the friends counted against NP_FRIEND_STORAGE_FRIENDS_MAX */
int NpFriendStorage::GetFriendCount(unsigned int *count)
{
	string sql;
	string typeRequestSent = npFriendInt64ToString(NP_FRIEND_PLAYER_TYPE_4);
	string typeFriend = npFriendInt64ToString(NP_FRIEND_PLAYER_TYPE_2);
	string typeRequestRecv = npFriendInt64ToString(NP_FRIEND_PLAYER_TYPE_3);

	sql = "SELECT count(*) FROM ";
	sql += NP_FRIEND_TABLE_PLAYERS;
	sql += " WHERE ";
	sql += "player_type";
	sql += " = ";
	sql += typeRequestSent.c_str();
	sql += " OR ";
	sql += "player_type";
	sql += " = ";
	sql += typeFriend.c_str();
	sql += " OR ";
	sql += "player_type";
	sql += " = ";
	sql += typeRequestRecv.c_str();
	sql += ";";

	dbutil::RefPtr<dbutil::Statement> statement;
	int ret = m_connection->Prepare(sql, &statement);

	if (ret >= 0) {
		ret = statement->Step(1);
		if (ret >= 0) {
			dbutil::Value value;

			ret = statement->GetValue(0, &value);
			if (ret >= 0)
				*count = (unsigned int)value.data.integer;
		}
	}
	return ret;
}

/* (0x810F7FC0) count friends added, as many as the limit leaves room for */
int NpFriendStorage::AddFriends(unsigned int count, const intrusive_ptr<NpFriendPlayer> *players,
				int *added)
{
	m_lock.Lock();

	unsigned int friends = 0;
	int ret = m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);

	if (ret >= 0) {
		ret = GetFriendCount(&friends);
		if (ret >= 0) {
			*added = 0;
			if (count != 0) {
				unsigned int i = 0;

				do {
					if (friends >= NP_FRIEND_STORAGE_FRIENDS_MAX)
						break;
					ret = StorePlayer(*players);
					if (ret < 0)
						break;
					i++;
					friends++;
					(*added)++;
					players++;
				} while (i < count);
			}
		}
		if (ret < 0)
			m_connection->Rollback();
		else
			ret = m_connection->Commit();
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810F8098) the player removed, if it is one of which */
int NpFriendStorage::DeletePlayer(const char *onlineId, int which)
{
	NpFriendString sql;
	dbutil::RefPtr<dbutil::Statement> statement;
	int ret;

	sql = "DELETE FROM ";
	sql += NP_FRIEND_TABLE_PLAYERS;
	sql += " WHERE ";
	sql += "online_id";
	sql += " = ? AND (";
	switch (which) {
	case NP_FRIEND_DELETE_FRIEND: {
		dbutil::Value id;
		dbutil::Value typeFriend;
		dbutil::Value typeRequestRecv;
		dbutil::Value typeRequestSent;
		dbutil::Value typePending;

		sql += "player_type";
		sql += " = ? OR ";
		sql += "player_type";
		sql += " = ? OR ";
		sql += "player_type";
		sql += " = ? OR ";
		sql += "player_type";
		sql += " = ? );";
		ret = m_connection->Prepare(string(sql.c_str()), &statement);
		if (ret < 0)
			break;
		id.type = SQLITE_TEXT;
		id.size = sce_paf_strlen(onlineId);
		id.data.text = onlineId;
		sce_paf_memset(&typeFriend, 0, sizeof(typeFriend));
		typeFriend.type = SQLITE_INTEGER;
		typeFriend.data.integer = NP_FRIEND_PLAYER_TYPE_2;
		sce_paf_memset(&typeRequestRecv, 0, sizeof(typeRequestRecv));
		typeRequestRecv.type = SQLITE_INTEGER;
		typeRequestRecv.data.integer = NP_FRIEND_PLAYER_TYPE_3;
		sce_paf_memset(&typeRequestSent, 0, sizeof(typeRequestSent));
		typeRequestSent.type = SQLITE_INTEGER;
		typeRequestSent.data.integer = NP_FRIEND_PLAYER_TYPE_4;
		sce_paf_memset(&typePending, 0, sizeof(typePending));
		typePending.type = SQLITE_INTEGER;
		typePending.data.integer = NP_FRIEND_PLAYER_TYPE_1;
		ret = statement->Bind(1, &id, false);
		if (ret < 0)
			break;
		ret = statement->Bind(2, &typeFriend, false);
		if (ret < 0)
			break;
		ret = statement->Bind(3, &typeRequestRecv, false);
		if (ret < 0)
			break;
		ret = statement->Bind(4, &typeRequestSent, false);
		if (ret < 0)
			break;
		ret = statement->Bind(5, &typePending, false);
		break;
	}
	case NP_FRIEND_DELETE_BLOCKED: {
		dbutil::Value id;
		dbutil::Value type;

		sql += "player_type";
		sql += " = ? );";
		ret = m_connection->Prepare(string(sql.c_str()), &statement);
		if (ret < 0)
			break;
		id.type = SQLITE_TEXT;
		id.size = sce_paf_strlen(onlineId);
		id.data.text = onlineId;
		sce_paf_memset(&type, 0, sizeof(type));
		type.type = SQLITE_INTEGER;
		type.data.integer = NP_FRIEND_PLAYER_TYPE_BLOCKED;
		ret = statement->Bind(1, &id, false);
		if (ret < 0)
			break;
		ret = statement->Bind(2, &type, false);
		break;
	}
	default:
		return NP_FRIEND_STORAGE_ERROR_INVALID_ARGUMENT;
	}
	if (ret >= 0) {
		ret = statement->Step(1);
		statement->Reset();
	}
	return ret;
}

/* (0x810F87E4) count players removed, each if it is one of which */
int NpFriendStorage::DeletePlayers(unsigned int count, const SceNpId *npIds, int which)
{
	m_lock.Lock();

	int ret = m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);

	if (ret >= 0) {
		for (unsigned int i = 0; i < count; i++, npIds++) {
			ret = DeletePlayer(npIds->handle.data, which);
			if (ret < 0)
				break;
		}
		if (ret < 0)
			m_connection->Rollback();
		else
			ret = m_connection->Commit();
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810F884E) count players' types changed */
int NpFriendStorage::UpdatePlayerTypes(unsigned int count,
				       const intrusive_ptr<NpFriendPlayer> *players)
{
	m_lock.Lock();

	int ret = m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);

	if (ret >= 0) {
		unsigned int i = 0;

		if (count != 0) do {
			NpFriendString sql("UPDATE ");
			dbutil::RefPtr<dbutil::Statement> statement;
			dbutil::Value type;
			dbutil::Value onlineId;

			sql += NP_FRIEND_TABLE_PLAYERS;
			sql += " SET ";
			sql += "player_type";
			sql += " = ? WHERE ";
			sql += "online_id";
			sql += " = ?;";
			ret = m_connection->Prepare(string(sql.c_str()), &statement);
			if (ret < 0)
				break;
			type.type = SQLITE_INTEGER;
			type.data.integer = (*players)->GetPlayerType();
			onlineId.type = SQLITE_TEXT;
			onlineId.size = sce_paf_strlen((*players)->GetOnlineId());
			onlineId.data.text = (*players)->GetOnlineId();
			ret = statement->Bind(1, &type, false);
			if (ret < 0)
				break;
			ret = statement->Bind(2, &onlineId, false);
			if (ret < 0)
				break;
			ret = statement->Step(1);
			statement->Reset();
			if (ret < 0)
				break;
			i++;
			players++;
		} while (i < count);
		if (ret >= 0)
			ret = m_connection->Commit();
		else
			m_connection->Rollback();
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810F8C4C) the player's profile */
int NpFriendStorage::UpdateProfile(const char *onlineId, const NpFriendProfile *profile,
				   int msgFlag)
{
	int ret;

	m_lock.Lock();
	{
		NpFriendString sql;
		dbutil::RefPtr<dbutil::Statement> statement;

		ret = m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);
		if (ret >= 0) {
			dbutil::Value avatarUrl;
			dbutil::Value aboutMe;
			dbutil::Value color;
			dbutil::Value language1;
			dbutil::Value language2;
			dbutil::Value language3;
			dbutil::Value plus;
			dbutil::Value panelUrl;
			dbutil::Value msgFlg;
			dbutil::Value id;

			sql = "UPDATE " NP_FRIEND_TABLE_PLAYERS;
			sql += " SET ";
			sql += "avatar_url = ?, ";
			sql += "about_me = ?, ";
			sql += "color = ?, ";
			sql += "language1 = ?, ";
			sql += "language2 = ?, ";
			sql += "language3 = ?, ";
			sql += "plus = ?, ";
			sql += "panel_url = ?, ";
			sql += "msg_flg = ? ";
			sql += "WHERE online_id = ?;";
			ret = m_connection->Prepare(string(sql.c_str()), &statement);
			if (ret >= 0) {
				avatarUrl.type = SQLITE_TEXT;
				avatarUrl.size = sce_paf_strlen(profile->avatarUrl);
				avatarUrl.data.text = profile->avatarUrl;
				aboutMe.type = SQLITE_TEXT;
				aboutMe.size = sce_paf_strlen(profile->aboutMe);
				aboutMe.data.text = profile->aboutMe;
				color.type = SQLITE_INTEGER;
				color.data.integer = profile->color;
				language1.type = SQLITE_INTEGER;
				language1.data.integer = profile->language1;
				language2.type = SQLITE_INTEGER;
				language2.data.integer = profile->language2;
				language3.type = SQLITE_INTEGER;
				language3.data.integer = profile->language3;
				plus.type = SQLITE_INTEGER;
				plus.data.integer = profile->plus;
				panelUrl.type = SQLITE_TEXT;
				if (profile->panelUrl != NULL) {
					panelUrl.size = sce_paf_strlen(profile->panelUrl);
					panelUrl.data.text = profile->panelUrl;
				} else {
					panelUrl.size = 0;
					panelUrl.data.text = "";
				}
				msgFlg.type = SQLITE_INTEGER;
				msgFlg.data.integer = msgFlag;
				id.type = SQLITE_TEXT;
				id.size = sce_paf_strlen(onlineId);
				id.data.text = onlineId;
				if ((ret = statement->Bind(1, &avatarUrl, false)) >= 0 &&
				    (ret = statement->Bind(2, &aboutMe, false)) >= 0 &&
				    (ret = statement->Bind(3, &color, false)) >= 0 &&
				    (ret = statement->Bind(4, &language1, false)) >= 0 &&
				    (ret = statement->Bind(5, &language2, false)) >= 0 &&
				    (ret = statement->Bind(6, &language3, false)) >= 0 &&
				    (ret = statement->Bind(7, &plus, false)) >= 0 &&
				    (ret = statement->Bind(8, &panelUrl, false)) >= 0 &&
				    (ret = statement->Bind(9, &msgFlg, false)) >= 0 &&
				    (ret = statement->Bind(10, &id, false)) >= 0) {
					ret = statement->Step(1);
					statement->Reset();
				}
			}
			if (ret < 0)
				m_connection->Rollback();
			else
				ret = m_connection->Commit();
		}
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810F92A4) the oldest entry of the session log removed */
int NpFriendStorage::DeleteOldestSessionLog()
{
	NpFriendString sql("DELETE FROM ");

	sql += NP_FRIEND_TABLE_SESSION_LOG;
	sql += " ORDER BY ";
	sql += "date";
	sql += " LIMIT 1;";
	return m_connection->Execute(string(sql.c_str()), 0, NULL, NULL, NULL, NULL);
}

/* (0x810F94C8) the session log's entries, all or one title's, oldest first */
int NpFriendStorage::FindSessionLogs(dbutil::Connection *connection, dbutil::Result *result,
				     int which, const SceNpCommunicationId *communicationId)
{
	NpFriendString sql("SELECT rowid FROM ");
	dbutil::Value id;
	int ret;

	sql += NP_FRIEND_TABLE_SESSION_LOG;
	switch (which) {
	case NP_FRIEND_SESSION_LOG_ALL:
		sql += " ORDER BY ";
		sql += "date";
		sql += ";";
		ret = connection->Execute(string(sql.c_str()), 0, NULL, result, NULL, NULL);
		break;
	case NP_FRIEND_SESSION_LOG_TITLE:
		sql += " WHERE ";
		sql += "sce_np_communication_id";
		sql += " = ?";
		sql += " ORDER BY ";
		sql += "date";
		sql += ";";
		id.data.blob = communicationId;
		id.type = SQLITE_BLOB;
		id.size = sizeof(SceNpCommunicationId);
		ret = connection->Execute(string(sql.c_str()), 1, &id, result, NULL, NULL);
		break;
	default:
		ret = NP_FRIEND_STORAGE_ERROR_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

/* (0x810F9990) */
int NpFriendStorage::GetSessionLogCount(int which, const SceNpCommunicationId *communicationId,
					unsigned int *count)
{
	int ret;

	m_lock.Lock();
	{
		dbutil::RowidResult result;

		ret = FindSessionLogs(m_connection, &result, which, communicationId);
		if (ret >= 0)
			*count = result.m_rowCount;
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810F99FA) an entry added to the session log, the oldest removed when it is full */
int NpFriendStorage::AddSessionLog(const NpFriendSessionLog *log)
{
	int ret;

	m_lock.Lock();
	{
		NpFriendString sql;
		unsigned int count = 0;
		char date[NP_FRIEND_DATE_SIZE];

		ret = GetSessionLogCount(NP_FRIEND_SESSION_LOG_ALL, NULL, &count);
		if (ret >= 0 && (count != NP_FRIEND_SESSION_LOG_MAX ||
				 (ret = DeleteOldestSessionLog()) >= 0)) {
			sql = "INSERT INTO ";
			sql += NP_FRIEND_TABLE_SESSION_LOG;
			sql += " (";
			sql += "date";
			sql += ", ";
			sql += "tbl_game_psn_players_online_id";
			sql += ", ";
			sql += "sce_np_id";
			sql += ", ";
			sql += "sce_np_communication_id";
			sql += ", ";
			sql += "tbl_game_title_title_id";
			sql += ", ";
			sql += "short_title_name";
			sql += ", ";
			sql += "long_title_name";
			sql += ", ";
			sql += "description";
			sql += ") VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
			sce_paf_memset(date, 0, sizeof(date));
			ret = rtc::FormatRFC3339(date, &log->date, 0);
			if (ret >= 0) {
				dbutil::Value dateValue;
				dbutil::Value onlineId;
				dbutil::Value npId;
				dbutil::Value communicationId;
				dbutil::Value titleId;
				dbutil::Value shortTitleName;
				dbutil::Value longTitleName;
				dbutil::Value description;

				dateValue.type = SQLITE_TEXT;
				dateValue.size = sce_paf_strlen(date);
				dateValue.data.text = date;
				onlineId.type = SQLITE_TEXT;
				onlineId.size = sce_paf_strlen(log->npId.handle.data);
				onlineId.data.text = log->npId.handle.data;
				npId.type = SQLITE_BLOB;
				npId.size = sizeof(SceNpId);
				npId.data.blob = &log->npId;
				communicationId.type = SQLITE_BLOB;
				communicationId.size = sizeof(SceNpCommunicationId);
				communicationId.data.blob = &log->communicationId;
				titleId.type = SQLITE_TEXT;
				titleId.size = sce_paf_strlen(log->titleId.c_str());
				titleId.data.text = log->titleId.c_str();
				shortTitleName.type = SQLITE_TEXT;
				shortTitleName.size = sce_paf_strlen(log->shortTitleName.c_str());
				shortTitleName.data.text = log->shortTitleName.c_str();
				longTitleName.type = SQLITE_TEXT;
				longTitleName.size = sce_paf_strlen(log->longTitleName.c_str());
				longTitleName.data.text = log->longTitleName.c_str();
				description.type = SQLITE_TEXT;
				description.size = sce_paf_strlen(log->description.c_str());
				description.data.text = log->description.c_str();

				dbutil::Value values[8] = {
					dateValue, onlineId, npId, communicationId, titleId, shortTitleName,
					longTitleName, description
				};

				ret = m_connection->Execute(string(sql.c_str()), 8, values, NULL, NULL, NULL);
			}
		}
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810FA2BA) num entries of the session log from start, all or one title's, read into logs; how
   many into got */
int NpFriendStorage::GetSessionLogs(int which, const SceNpCommunicationId *communicationId,
				    unsigned int start, unsigned int num, NpFriendSessionLog **logs,
				    unsigned int *got)
{
	int ret;

	m_lock.Lock();
	{
		dbutil::RowidResult result;

		ret = FindSessionLogs(m_connection, &result, which, communicationId);
		if (ret >= 0) {
			unsigned int total = result.m_rowCount;

			unsigned int end = start + num;
			unsigned int i = start;

			*got = 0;
			if (i < end) do {
				unsigned int j = i - start;

				if (i >= total)
					break;

				dbutil::RefPtr<dbutil::Record> record;
				dbutil::Value value;

				ret = result.OpenRecord((int)i, string(s_allColumns), &record);
				if (ret < 0)
					break;
				ret = record->GetValue(string("date"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT) {
					ret = rtc::ParseRFC3339(&logs[j]->date, value.data.text);
					if (ret < 0)
						break;
				}
				ret = record->GetValue(string("sce_np_id"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_BLOB)
					sce_paf_memcpy(&logs[j]->npId, value.data.blob, sizeof(SceNpId));
				ret = record->GetValue(string("tbl_game_title_title_id"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					logs[j]->titleId = value.data.text;
				ret = record->GetValue(string("long_title_name"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					logs[j]->longTitleName = value.data.text;
				ret = record->GetValue(string("short_title_name"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					logs[j]->shortTitleName = value.data.text;
				ret = record->GetValue(string("sce_np_communication_id"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_BLOB)
					sce_paf_memcpy(&logs[j]->communicationId, value.data.blob,
						       sizeof(SceNpCommunicationId));
				ret = record->GetValue(string("description"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					logs[j]->description = value.data.text;
				(*got)++;
			} while (++i < end);
		}
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810FA762) an entry of the session log removed */
int NpFriendStorage::DeleteSessionLog(SceInt64 logId)
{
	int ret;

	m_lock.Lock();
	{
		NpFriendString sql;
		int changes = 0;
		dbutil::Value id;

		sql = "DELETE FROM ";
		sql += NP_FRIEND_TABLE_SESSION_LOG;
		sql += " WHERE ";
		sql += "log_id";
		sql += " = ?;";
		id.data.integer = logId;
		id.type = SQLITE_INTEGER;
		ret = m_connection->Execute(string(sql.c_str()), 1, &id, NULL, NULL, &changes);
		if (ret >= 0 && changes != 1) {
			if (changes <= 1)
				ret = NP_FRIEND_STORAGE_ERROR_NOT_FOUND;
			else
				ret = NP_FRIEND_STORAGE_ERROR_INVALID_ARGUMENT;
		}
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810FA9D4) the session log emptied */
int NpFriendStorage::DeleteSessionLogs()
{
	NpFriendString sql("DELETE FROM ");

	sql += NP_FRIEND_TABLE_SESSION_LOG;
	sql += ";";
	return m_connection->Execute(string(sql.c_str()), 0, NULL, NULL, NULL, NULL);
}

/* (0x810FAB44) the player's entries of the session log, oldest first */
int NpFriendStorage::FindPlayerSessionLogs(dbutil::Connection *connection, dbutil::Result *result,
					   const char *onlineId)
{
	NpFriendString sql("SELECT rowid FROM ");
	dbutil::Value id;

	sql += NP_FRIEND_TABLE_SESSION_LOG;
	sql += " WHERE ";
	sql += "tbl_game_psn_players_online_id";
	sql += " = ?";
	sql += " ORDER BY ";
	sql += "date";
	sql += ";";
	id.type = SQLITE_TEXT;
	id.size = sce_paf_strlen(onlineId);
	id.data.text = onlineId;
	return connection->Execute(string(sql.c_str()), 1, &id, result, NULL, NULL);
}

/* (0x810FAE84) */
int NpFriendStorage::GetPlayerSessionLogCount(const char *onlineId, unsigned int *count)
{
	int ret;

	m_lock.Lock();
	{
		dbutil::RowidResult result;

		ret = FindPlayerSessionLogs(m_connection, &result, onlineId);
		if (ret >= 0)
			*count = result.m_rowCount;
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810FAEEA) the player's entry index of the session log */
int NpFriendStorage::GetPlayerSessionLog(const char *onlineId, unsigned int index,
					 NpFriendSessionLog *log)
{
	int ret;

	m_lock.Lock();
	{
		dbutil::RowidResult result;

		ret = FindPlayerSessionLogs(m_connection, &result, onlineId);
		if (ret >= 0 && index >= result.m_rowCount)
			ret = NP_FRIEND_STORAGE_ERROR_OUT_OF_RANGE;
		if (ret >= 0) {
			dbutil::RefPtr<dbutil::Record> record;
			dbutil::Value value;

			do {
				ret = result.OpenRecord((int)index, string(s_allColumns), &record);
				if (ret < 0)
					break;
				ret = record->GetValue(string("date"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT) {
					ret = rtc::ParseRFC3339(&log->date, value.data.text);
					if (ret < 0)
						break;
				}
				ret = record->GetValue(string("sce_np_id"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_BLOB)
					sce_paf_memcpy(&log->npId, value.data.blob, sizeof(SceNpId));
				ret = record->GetValue(string("tbl_game_title_title_id"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					log->titleId = value.data.text;
				ret = record->GetValue(string("long_title_name"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					log->longTitleName = value.data.text;
				ret = record->GetValue(string("short_title_name"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					log->shortTitleName = value.data.text;
				ret = record->GetValue(string("sce_np_communication_id"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_BLOB)
					sce_paf_memcpy(&log->communicationId, value.data.blob,
						       sizeof(SceNpCommunicationId));
				ret = record->GetValue(string("description"), &value);
				if (ret < 0)
					break;
				if (value.type == SQLITE_TEXT)
					log->description = value.data.text;
			} while (0);
		}
	}
	m_lock.Unlock();
	return ret;
}

/* (0x810FB32A) an SQLite error on the database, told to the database recovery */
static void npFriendStorageError(void *arg, int result)
{
	shellDbRecoveryNotify(result, ((NpFriendStorage *)arg)->m_path, 0);
}
