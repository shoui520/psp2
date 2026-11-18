/*
 * The new events (tbl_newevent): the notifications the LiveArea lists and pops up. The
 * accessor's exports (Sony's newevent::NewEventAccessor) hand each request to its
 * implementation, which takes the daemon's connection for a transaction of its own; the daemon
 * (Sony's SceLsdbNeDaemon) runs the requests that change the table.
 */
#include <string.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <rtc/rtc.h>
#include <sqlite3.h>
#include <dbutil/statement.h>
#include <dbutil/function.h>
#include "livespace_db_internal.h"
#include "lsdb_statement.h"

using dbutil::RefPtr;
using dbutil::Statement;

/* the most events one request lists */
#define NE_EVENTS_MAX	64

/* the fields an update writes (and compares) */
#define NE_FIELD_ICON_PATH	0x001
#define NE_FIELD_ICON		0x002	/* icon_data */
#define NE_FIELD_MSG		0x004	/* msg_type and msg_arg0 to 8 */
#define NE_FIELD_ACT_TYPE	0x008
#define NE_FIELD_TITLE		0x010
#define NE_FIELD_DESC		0x020
#define NE_FIELD_EXEC		0x040	/* exec_mode, exec_title_id and exec_arg */
#define NE_FIELD_NEW_FLAG	0x080
#define NE_FIELD_TIME_UPDATED	0x100
#define NE_FIELD_SE		0x200	/* se_id and se_volume */
#define NE_FIELD_POPUP_NO	0x400

/* an application's notifications (its notifyFlags in bits 1 to 3, bit 0 set), name unknown */
#define NE_KEY_NOTIFY		0x5142196Bu

/* the configuration keys of the system's own event sources' notifications */
#define NE_CONFIG_NOTIFY_LAUPDATE	256
#define NE_CONFIG_NOTIFY_LOGSTATUS	257
#define NE_CONFIG_NOTIFY_FRIEND		258	/* NPXS10006 */
#define NE_CONFIG_NOTIFY_FRIEND_REQUEST	259	/* NPXS10006, msg_type 1025 */
#define NE_CONFIG_NOTIFY_BGCPSTATUS	260

/* a time_updated text: "YYYY-MM-DD HH:MM:SS" */
#define NE_TIME_LENGTH		19

extern "C" {

/* the constructors (complete and base object) */
NewEventAccessor *SceLsdb_919C0934(NewEventAccessor *accessor)
{
	accessor->m_impl = NULL;
	return accessor;
}

NewEventAccessor *SceLsdb_60C08DFF(NewEventAccessor *accessor)
{
	accessor->m_impl = NULL;
	return accessor;
}

int SceLsdb_4C137FCC(NewEventAccessor *accessor)
{
	if (accessor->m_impl != NULL)
		return LSDB_ERROR_NOT_READY;
	accessor->m_impl = new NewEventAccessorImpl;
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NO_MEMORY;

	int result = accessor->m_impl->Open();

	if (result == 0)
		return 0;
	delete accessor->m_impl;
	accessor->m_impl = NULL;
	return result;
}

int SceLsdb_937405B5(NewEventAccessor *accessor)
{
	if (accessor->m_impl == NULL)
		return 0;
	accessor->m_impl->Close();
	delete accessor->m_impl;
	accessor->m_impl = NULL;
	return 0;
}

int SceLsdb_315B9FD6(LsdbNewEvent *event, bool popup)
{
	return NewEventAccessorImpl::Insert(event, popup);
}

int SceLsdb_B3252544(LsdbNewEventPtr *events, int count, unsigned int fields)
{
	return NewEventAccessorImpl::Update(events, count, fields);
}

int SceLsdb_AE04DDEB(const LsdbNewEventKey *keys, int count)
{
	return NewEventAccessorImpl::Delete(keys, count);
}

int SceLsdb_C02E38AD(bool all)
{
	return NewEventAccessorImpl::DeleteAll(all);
}

int SceLsdb_07ADA030(NewEventAccessor *accessor, LsdbNewEvent *event)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->GetEvent(event);
}

int SceLsdb_B285FC5E(NewEventAccessor *accessor, paf::vector<LsdbNewEventPtr> *events, int max,
	int *count, int *newCount, bool clear)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->GetEvents(events, max, count, newCount, clear);
}

paf::common::SharedPtr<DbBlob> SceLsdb_03B27FDF(NewEventAccessor *accessor,
	const SceInt64 *rowid, int *pResult)
{
	paf::common::SharedPtr<DbBlob> icon;
	int result = LSDB_ERROR_NOT_READY;

	if (accessor->m_impl != NULL)
		result = accessor->m_impl->GetIcon(rowid, &icon);
	if (pResult != NULL)
		*pResult = result;
	return icon;
}

bool SceLsdb_8DE37E2E(NewEventAccessor *accessor)
{
	if (accessor->m_impl == NULL)
		return false;
	return accessor->m_impl->IsChanged();
}

void SceLsdb_B1F02AD0(NewEventAccessor *accessor)
{
	if (accessor->m_impl != NULL)
		accessor->m_impl->ClearChanged();
}

int SceLsdb_A2BD70BE(NewEventAccessor *accessor)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->GetPopupCount();
}

int SceLsdb_D046218A(NewEventAccessor *accessor, LsdbNewEvent *event)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->GetPopup(event);
}

int SceLsdb_98672963(NewEventAccessor *accessor, const SceInt64 *rowid)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->ClearPopup(rowid);
}

int SceLsdb_8A6B2B12(NewEventAccessor *accessor)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->ClearPopups();
}

int SceLsdb_464351D0(NewEventAccessor *accessor, LsdbNewEventPtr *events, int count)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->UpdateNewFlags(events, count);
}

int SceLsdb_FB3B1767(NewEventAccessor *accessor)
{
	if (accessor->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return accessor->m_impl->ClearNewFlags();
}

/* the notifications an event of a title gets; all of them without the accessor */
unsigned int SceLsdb_121C66FC(NewEventAccessor *accessor, const LsdbNewEvent *event)
{
	if (accessor->m_impl == NULL)
		return LSDB_NOTIFY_ALL;
	return accessor->m_impl->GetNotifyFlags(event);
}

}

__attribute__((noinline)) NewEventAccessorImpl::NewEventAccessorImpl() : m_changed(0)
{
}

__attribute__((noinline)) NewEventAccessorImpl::~NewEventAccessorImpl()
{
}

/* notes the daemon's change */
static void NewEventAccessorImplOnChange(void *userData)
{
	sceAtomicExchange32AcqRel(&((NewEventAccessorImpl *)userData)->m_changed, 1);
}

__attribute__((noinline)) int NewEventAccessorImpl::Open()
{
	return NeDaemon::GetInstance()->AddListener(NewEventAccessorImplOnChange, this);
}

__attribute__((noinline)) void NewEventAccessorImpl::Close()
{
	NeDaemon::GetInstance()->RemoveListener(this);
}

/* an event without its key, its time or its icon's data is not inserted */
__attribute__((noinline)) int NewEventAccessorImpl::Insert(LsdbNewEvent *event, bool popup)
{
	if ((event->titleId.size() == 0 || event->itemId.size() == 0) && event->rowid == 0)
		return LSDB_ERROR_NO_EVENT_KEY;
	if (event->timeUpdated.size() != 0) {
		if (event->timeUpdated.size() < NE_TIME_LENGTH)
			return LSDB_ERROR_INVALID_ARGUMENT;
		if (event->timeUpdated.at(4) != '-' || event->timeUpdated.at(7) != '-' ||
		    event->timeUpdated.at(10) != ' ' || event->timeUpdated.at(13) != ':' ||
		    event->timeUpdated.at(16) != ':')
			return LSDB_ERROR_INVALID_ARGUMENT;
	}
	if (event->iconDataSize != 0 && event->iconData == NULL)
		return LSDB_ERROR_INVALID_ARGUMENT;
	return NeDaemon::GetInstance()->Insert(event, popup);
}

/* checks each event's key, and its time and icon where fields update them */
__attribute__((noinline)) int NewEventAccessorImpl::Update(LsdbNewEventPtr *events, int count,
	unsigned int fields, bool keepNew)
{
	if (events == NULL || count == 0 || fields == 0)
		return LSDB_ERROR_INVALID_ARGUMENT;
	for (int i = 0; i < count; i++) {
		const LsdbNewEvent *event = events[i].get();

		if (event == NULL)
			return LSDB_ERROR_INVALID_ARGUMENT;
		if ((event->titleId.size() == 0 || event->itemId.size() == 0) && event->rowid == 0)
			return LSDB_ERROR_NO_EVENT_KEY;
		if ((fields & NE_FIELD_TIME_UPDATED) != 0 && event->timeUpdated.size() != 0) {
			if (event->timeUpdated.size() < NE_TIME_LENGTH)
				return LSDB_ERROR_INVALID_ARGUMENT;
			if (event->timeUpdated.at(4) != '-' || event->timeUpdated.at(7) != '-' ||
			    event->timeUpdated.at(10) != ' ' || event->timeUpdated.at(13) != ':' ||
			    event->timeUpdated.at(16) != ':')
				return LSDB_ERROR_INVALID_ARGUMENT;
		}
		if ((fields & NE_FIELD_ICON) != 0 && event->iconDataSize != 0 &&
		    event->iconData == NULL)
			return LSDB_ERROR_INVALID_ARGUMENT;
	}
	return NeDaemon::GetInstance()->Update(events, count, fields, false, keepNew);
}

__attribute__((noinline)) int NewEventAccessorImpl::Update(LsdbNewEventPtr *events, int count,
	unsigned int fields)
{
	return Update(events, count, fields, false);
}

__attribute__((noinline)) int NewEventAccessorImpl::Delete(const LsdbNewEventKey *keys, int count)
{
	if (keys == NULL || count == 0)
		return LSDB_ERROR_INVALID_ARGUMENT;
	for (int i = 0; i < count; i++) {
		if ((keys[i].titleId.size() == 0 || keys[i].itemId.size() == 0) &&
		    keys[i].rowid == 0)
			return LSDB_ERROR_NO_EVENT_KEY;
	}
	return NeDaemon::GetInstance()->Delete(keys, count);
}

__attribute__((noinline)) int NewEventAccessorImpl::DeleteAll(bool all)
{
	return NeDaemon::GetInstance()->DeleteEvents(all);
}

/* a text column's value; empty for another type */
static inline paf::string NeColumnString(const dbutil::Value *value)
{
	if (value->type == SQLITE_TEXT)
		return paf::string(value->data.text, value->size);
	return paf::string();
}

static inline int NeColumnInt(const dbutil::Value *value)
{
	return value->type == SQLITE_INTEGER ? value->data.integer : 0;
}

/* an event's columns, each by its name, from the row a statement stepped to */
static int NeReadRow(RefPtr<Statement> *statement, LsdbNewEvent *event)
{
	int result = 0;

	event->iconData = NULL;

	int columns = (*statement)->m_columnCount;

	for (int column = 0; column < columns; column++) {
		dbutil::Value value;

		result = (*statement)->GetValue(column, &value);
		if (result != 0)
			break;
		if (sce_paf_strcmp(value.name, "rowid") == 0) {
			event->rowid = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		} else if (sce_paf_strcmp(value.name, "id_title_id") == 0) {
			event->titleId = NeColumnString(&value);
		} else if (sce_paf_strcmp(value.name, "id_item_id") == 0) {
			event->itemId = NeColumnString(&value);
		} else if (sce_paf_strcmp(value.name, "msg_type") == 0) {
			event->msgType = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		} else if (sce_paf_strcmp(value.name, "act_type") == 0) {
			SceInt64 actType = value.type == SQLITE_INTEGER ? value.data.integer : 0;

			event->actType = (int)actType;
			event->notifyFlags = (unsigned int)(actType >> 32);
		} else if (sce_paf_strcmp(value.name, "datetime(time_updated)") == 0) {
			event->timeUpdated = NeColumnString(&value);
		} else if (sce_paf_strcmp(value.name, "new_flag") == 0) {
			event->newFlag = value.type == SQLITE_INTEGER && value.data.integer != 0;
		} else if (sce_paf_strcmp(value.name, "popup_no") == 0) {
			event->popupNo = value.type == SQLITE_INTEGER && value.data.integer != 0;
		} else if (sce_paf_strcmp(value.name, "icon_path") == 0) {
			paf::string path = NeColumnString(&value);

			/* an application's own icon in vs0 is read from the application */
			int other = sce_paf_strcmp(path.substr(0, 4).c_str(),
				paf::string("vs0:").c_str());

			if (other == 0)
				other = sce_paf_strcmp(path.substr(path.size() - 9, 9).c_str(),
					paf::string("icon0.png").c_str());
			if (other == 0)
				event->iconPath.clear();
			else
				event->iconPath = path;
		} else if (sce_paf_strcmp(value.name, "length(icon_data)") == 0) {
			event->iconDataSize = NeColumnInt(&value);
		} else if (sce_paf_strcmp(value.name, "exec_mode") == 0) {
			event->execMode = NeColumnInt(&value);
		} else if (sce_paf_strcmp(value.name, "exec_title_id") == 0) {
			event->execTitleId = NeColumnString(&value);
		} else if (sce_paf_strcmp(value.name, "exec_arg") == 0) {
			event->execArg = NeColumnString(&value);
		} else if (sce_paf_strcmp(value.name, "title") == 0) {
			event->title = NeColumnString(&value);
		} else if (sce_paf_strcmp(value.name, "desc") == 0) {
			event->desc = NeColumnString(&value);
		} else if (sce_paf_strncmp(value.name, "msg_arg", 7) == 0) {
			unsigned int i = sce_paf_atoi(value.name + 7);

			if (i < 9)
				event->msgArg[i] = NeColumnString(&value);
		} else if (sce_paf_strcmp(value.name, "se_id") == 0) {
			event->seId = NeColumnInt(&value);
		} else if (sce_paf_strcmp(value.name, "se_volume") == 0) {
			event->seVolume = value.type == SQLITE_FLOAT ? value.data.real : 0.0;
		} else if (sce_paf_strcmp(value.name, "hash") == 0) {
			event->hash = NeColumnInt(&value);
		}
	}
	return result;
}

#define NE_SELECT	"SELECT rowid,id_title_id,id_item_id,msg_type,act_type," \
	"datetime(time_updated),new_flag,popup_no,icon_path,exec_mode,exec_title_id,exec_arg," \
	"title,desc,msg_arg0,msg_arg1,msg_arg2,msg_arg3,msg_arg4,msg_arg5,msg_arg6,msg_arg7," \
	"msg_arg8,se_id,se_volume,hash,length(icon_data) FROM tbl_newEvent"

/*
 * Reads an event by its rowid (deleted ones too with deleted), else by its title's and item's
 * IDs; an event not found by its rowid is looked for by its IDs.
 */
static int NeReadEvent(ActivityDbConnection *connection, LsdbNewEvent *event, bool deleted)
{
	RefPtr<Statement> statement;
	int result;

	if (event->rowid != 0) {
		const char *sql = NE_SELECT " WHERE rowid=? AND del_flag=0";

		if (deleted)
			sql = NE_SELECT " WHERE rowid=?";

		paf::string query(sql);

		result = connection->Prepare(query, &statement);
		if (result == 0)
			result = AppDbBindInt(&statement, 1, event->rowid);
	} else {
		result = connection->Prepare(
			paf::string(NE_SELECT " WHERE del_flag=0 AND id_title_id=? AND "
				"id_item_id=?"), &statement);
		if (result == 0) {
			result = AppDbBindString(&statement, 1, &event->titleId);
			if (result == 0)
				result = AppDbBindString(&statement, 2, &event->itemId);
		}
	}
	if (result == 0) {
		result = statement->Step(1);
		if (result == 0) {
			result = NeReadRow(&statement, event);
		} else if (result == 1) {
			result = LSDB_ERROR_NOT_FOUND;
			if (event->rowid != 0 && event->titleId.size() != 0 &&
			    event->itemId.size() != 0) {
				event->rowid = 0;
				result = NeReadEvent(connection, event, false);
			}
		}
	}
	return result;
}

__attribute__((noinline)) int NewEventAccessorImpl::GetEvent(LsdbNewEvent *event)
{
	if ((event->titleId.size() == 0 || event->itemId.size() == 0) && event->rowid == 0)
		return LSDB_ERROR_NO_EVENT_KEY;

	ActivityDbConnection *connection = NeDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = NeReadEvent(connection, event, false);

	NeDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

__attribute__((noinline)) void NewEventAccessorImpl::ClearChanged()
{
	sceAtomicExchange32AcqRel(&m_changed, 0);
}

/* the number of events (and condition) */
static int NeCountEvents(ActivityDbConnection *connection, int *count, const char *condition)
{
	paf::string query("SELECT count(*) FROM tbl_newEvent WHERE del_flag=0");

	if (condition != NULL)
		query += condition;

	RefPtr<Statement> statement;
	int result = connection->Prepare(query, &statement);

	if (result == 0) {
		result = statement->Step(1);
		if (result == 0) {
			dbutil::Value value;

			result = statement->GetValue(0, &value);
			if (result == 0)
				*count = value.type == SQLITE_INTEGER ? (int)value.data.integer : 0;
		}
	}
	return result;
}

/* the newest max events */
static int NeReadEvents(ActivityDbConnection *connection, paf::vector<LsdbNewEventPtr> *events,
	int max)
{
	events->clear();

	RefPtr<Statement> statement;
	int result = connection->Prepare(
		paf::string(NE_SELECT " WHERE del_flag=0 ORDER BY time_updated DESC LIMIT ?"),
		&statement);

	if (result == 0) {
		result = AppDbBindInt(&statement, 1, max);
		if (result == 0) {
			do {
				result = statement->Step(1);
				if (result != 0)
					break;

				LsdbNewEventPtr event(new LsdbNewEvent);

				result = NeReadRow(&statement, event.get());
				if (result != 0)
					goto end;
				events->push_back(event);
			} while (result == 0);
			if (result == 1)
				result = 0;
		}
	}
end:
	return result;
}

/*
 * The newest events, at most max (and NE_EVENTS_MAX); count and newCount, when given, the
 * number of events and of new ones (each at most NE_EVENTS_MAX). clear takes the change.
 */
__attribute__((noinline)) int NewEventAccessorImpl::GetEvents(paf::vector<LsdbNewEventPtr> *events,
	int max, int *count, int *newCount, bool clear)
{
	int limit = paf::min(max, NE_EVENTS_MAX);
	ActivityDbConnection *connection = NeDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = NeReadEvents(connection, events, limit);

	if (result == 0) {
		if (count != NULL) {
			result = NeCountEvents(connection, count, NULL);
			if (result != 0)
				goto end;
			*count = paf::min(*count, NE_EVENTS_MAX);
		}
		if (newCount != NULL) {
			result = NeCountEvents(connection, newCount, " AND 0 < new_flag");
			if (result != 0)
				goto end;
			*newCount = paf::min(*newCount, NE_EVENTS_MAX);
		}
		if (clear)
			sceAtomicExchange32AcqRel(&m_changed, 0);
	}
end:
	NeDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

/* the transaction of an event's icon: the daemon's connection, rolled back at its end */
static int NeIconTransaction(int op, bool readOnly, void *arg, ActivityDbConnection **connection)
{
	if (op == DB_BLOB_TRANSACTION_BEGIN) {
		*connection = NeDaemon::GetInstance()->BeginTransaction();
		if (*connection == NULL)
			return LSDB_ERROR_NOT_READY;
		return 0;
	}
	if (op == DB_BLOB_TRANSACTION_END) {
		int result = NeDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);

		*connection = NULL;
		return result;
	}
	return 0;
}

/* an event's icon_data, as a file read in transactions of its own */
__attribute__((noinline)) int NewEventAccessorImpl::GetIcon(const SceInt64 *rowid,
	paf::common::SharedPtr<DbBlob> *icon)
{
	int result = 0;
	ActivityDbConnection *connection = NeDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	{
		RefPtr<Statement> statement;
		SceInt64 id = *rowid;

		result = connection->Prepare(
			paf::string("SELECT 1 FROM tbl_newEvent WHERE rowid=?"), &statement);
		if (result == 0) {
			result = AppDbBindInt(&statement, 1, id);
			if (result == 0) {
				result = statement->Step(1);
				if (result == 1)
					result = LSDB_ERROR_NOT_FOUND;
			}
		}
	}
	if (result == 0)
		*icon = DbBlobOpen("main", "tbl_newEvent", "icon_data", *rowid, 0,
			NeIconTransaction, this, &result);
	NeDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

__attribute__((noinline)) bool NewEventAccessorImpl::IsChanged()
{
	return m_changed != 0;
}

/* the first event to pop up; none left clears the daemon's count */
__attribute__((noinline)) int NewEventAccessorImpl::GetPopup(LsdbNewEvent *event)
{
	ActivityDbConnection *connection = NeDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(
			paf::string(NE_SELECT " WHERE 0<popup_no ORDER BY popup_no LIMIT 1"),
			&statement);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 0) {
			if (result == 1) {
				result = LSDB_ERROR_NOT_FOUND;
				NeDaemon::GetInstance()->ResetPopupCount();
			}
			goto end;
		}
		result = NeReadRow(&statement, event);
		if (result != 0)
			goto end;
	}
end:
	NeDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

/* an event no longer pops up */
__attribute__((noinline)) int NewEventAccessorImpl::ClearPopup(const SceInt64 *rowid)
{
	LsdbNewEventPtr event(new LsdbNewEvent);

	event->rowid = *rowid;
	event->popupNo = false;
	return NeDaemon::GetInstance()->Update(&event, 1, NE_FIELD_POPUP_NO, true, false);
}

__attribute__((noinline)) int NewEventAccessorImpl::ClearPopups()
{
	return NeDaemon::GetInstance()->ClearPopups();
}

__attribute__((noinline)) int NewEventAccessorImpl::GetPopupCount()
{
	return NeDaemon::GetInstance()->GetPopupCount();
}

__attribute__((noinline)) int NewEventAccessorImpl::UpdateNewFlags(LsdbNewEventPtr *events,
	int count)
{
	return Update(events, count, NE_FIELD_NEW_FLAG, true);
}

__attribute__((noinline)) int NewEventAccessorImpl::ClearNewFlags()
{
	return NeDaemon::GetInstance()->ClearNewFlags();
}

__attribute__((noinline)) unsigned int NewEventAccessorImpl::GetNotifyFlags(
	const LsdbNewEvent *event)
{
	return NeDaemon::GetInstance()->GetNotifyFlags(event, 256);
}

/*
 * The daemon.
 */

/* the next popup_no: each event to pop up takes one */
static unsigned int s_popupNo = 1;

/* one of the daemon's SQL functions, taking no argument */
class NeFunction : public dbutil::Function {
public:
	NeFunction(const char *name,
		void (*func)(sqlite3_context *context, int argc, sqlite3_value **argv))
	{
		m_name = name;
		m_argCount = 0;
		m_textRep = SQLITE_UTF8;
		m_func = func;
	}
};

__attribute__((noinline)) NeDaemon::NeDaemon()
	: m_access(NULL), m_mutex("SceLsdbNeDaemon", 0), m_eventCount(-1), m_popupCount(0xFFFFFFF)
{
}

/* the one daemon, made at its first use (the SQL functions take it inline) */
inline NeDaemon *NeDaemon::Instance()
{
	static NeDaemon s_instance;

	return &s_instance;
}

__attribute__((noinline)) NeDaemon *NeDaemon::GetInstance()
{
	return Instance();
}

/* creates tbl_newevent and the triggers that count its events to pop up */
__attribute__((noinline)) int NeDaemon::Upgrade(ActivityDbConnection *connection, int version)
{
	int result = 0;

	if (version <= 0) {
		const char *sql[5];

		sql[0] = "CREATE TABLE IF NOT EXISTS tbl_newevent ("
			"rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
			"id_title_id TEXT NOT NULL,id_item_id TEXT NOT NULL,"
			"del_flag INT NOT NULL,msg_type INT NOT NULL,act_type INT NOT NULL,"
			"time_updated DEFAULT CURRENT_TIMESTAMP,new_flag INT,popup_no INT,"
			"icon_path TEXT,exec_mode INT NOT NULL,exec_title_id TEXT,exec_arg TEXT,"
			"title TEXT,desc TEXT,msg_arg0 TEXT, msg_arg1 TEXT, msg_arg2 TEXT,"
			"msg_arg3 TEXT, msg_arg4 TEXT, msg_arg5 TEXT,msg_arg6 TEXT, msg_arg7 TEXT, "
			"msg_arg8 TEXT,se_id INT,se_volume REAL,hash INT DEFAULT 1 NOT NULL,"
			"icon_data BLOB)";
		sql[1] = "CREATE TRIGGER IF NOT EXISTS tgr_newevent_popup_ins AFTER INSERT ON "
			"tbl_newevent WHEN 0<NEW.popup_no BEGIN SELECT increment_popup_num();END";
		sql[2] = "CREATE TRIGGER IF NOT EXISTS tgr_newevent_popup_upins BEFORE UPDATE OF "
			"popup_no ON tbl_newevent WHEN OLD.popup_no=0 AND 0<NEW.popup_no BEGIN "
			"SELECT increment_popup_num();END";
		sql[3] = "CREATE TRIGGER IF NOT EXISTS tgr_newevent_popup_updel BEFORE UPDATE OF "
			"popup_no ON tbl_newevent WHEN 0<OLD.popup_no AND NEW.popup_no=0 BEGIN "
			"SELECT decrement_popup_num();END";
		sql[4] = "CREATE TRIGGER IF NOT EXISTS tgr_newevent_autoDel AFTER UPDATE ON "
			"tbl_newevent WHEN NEW.popup_no=0 AND 1=NEW.del_flag BEGIN "
			"DELETE FROM tbl_newevent WHERE ROWID=NEW.ROWID;END";
		result = SceActivityDb_D3277649(connection, sql, 5);
	}
	return result;
}

/* tells every listener of a change */
__attribute__((noinline)) void NeDaemon::NotifyListeners()
{
	m_mutex.Lock();
	for (paf::map<unsigned int, NeDaemonListener>::iterator listener = m_listeners.begin();
	     listener != m_listeners.end(); ++listener)
		listener->second((void *)listener->first);
	m_mutex.Unlock();
}

__attribute__((noinline)) int NeDaemon::ResetPopupCount()
{
	sceAtomicExchange32AcqRel(&m_popupCount, 0);
	return 0;
}

/* no event pops up any more */
__attribute__((noinline)) int NeDaemon::ClearPopups()
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = SceActivityDb_82B45E74(m_access);

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(
			paf::string("UPDATE tbl_newevent SET popup_no=0 WHERE 0 < popup_no"),
			&statement);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 1)
			goto end;
	}
	sceAtomicExchange32AcqRel(&m_popupCount, 0);
	result = 0;
	if (connection->GetChanges() != 0) {
		NotifyListeners();
		result = 0;
	}
end:
	SceActivityDb_11D77056(m_access, result);
	return result;
}

/* takes the database: the daemon's SQL functions go on it, and no event pops up */
__attribute__((noinline)) int NeDaemon::Open(ActivityDbAccess *access)
{
	m_access = access;
	m_eventCount = -1;
	{
		NeFunction increment("increment_popup_num", IncrementPopupNum);

		SceActivityDb_76F10DA7(m_access, &increment);

		NeFunction decrement("decrement_popup_num", DecrementPopupNum);

		SceActivityDb_76F10DA7(m_access, &decrement);

		NeFunction next("get_next_popup_no", GetNextPopupNo);

		SceActivityDb_76F10DA7(m_access, &next);
	}
	ClearPopups();
	return 0;
}

__attribute__((noinline)) void NeDaemon::Close()
{
	if (m_access != NULL) {
		NeFunction increment("increment_popup_num", IncrementPopupNum);

		SceActivityDb_94516607(m_access, &increment);

		NeFunction decrement("decrement_popup_num", DecrementPopupNum);

		SceActivityDb_94516607(m_access, &decrement);

		NeFunction next("get_next_popup_no", GetNextPopupNo);

		SceActivityDb_94516607(m_access, &next);
		m_access = NULL;
	}
	m_listeners.clear();
}

__attribute__((noinline)) ActivityDbConnection *NeDaemon::BeginTransaction()
{
	if (m_access == NULL)
		return NULL;

	ActivityDbConnection *connection = SceActivityDb_82B45E74(m_access);

	if (connection == NULL)
		return NULL;
	return connection;
}

__attribute__((noinline)) int NeDaemon::EndTransaction(int result)
{
	return SceActivityDb_11D77056(m_access, result);
}

__attribute__((noinline)) int NeDaemon::AddListener(NeDaemonListener listener, void *userData)
{
	m_mutex.Lock();
	if (m_access == NULL) {
		m_mutex.Unlock();
		return LSDB_ERROR_NOT_READY;
	}
	m_listeners.insert(paf::pair<unsigned int, NeDaemonListener>((unsigned int)userData,
		listener));
	m_mutex.Unlock();
	return 0;
}

__attribute__((noinline)) void NeDaemon::RemoveListener(void *userData)
{
	m_mutex.Lock();
	m_listeners.erase((unsigned int)userData);
	m_mutex.Unlock();
}

/*
 * Deletes the events: all of them, or (all) only the messages (msg_type 80 on), which go when
 * they no longer pop up.
 */
__attribute__((noinline)) int NeDaemon::DeleteEvents(bool all)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = SceActivityDb_82B45E74(m_access);

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	if (all) {
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("UPDATE tbl_newevent SET del_flag=1 "
			"WHERE del_flag=0 AND ?<=msg_type"), &statement);
		if (result != 0)
			goto end;
		result = AppDbBindInt(&statement, 1, 80);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 1)
			goto end;
		m_eventCount -= connection->GetChanges();
	} else {
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("DELETE FROM tbl_newevent"), &statement);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 1)
			goto end;
		sceAtomicExchange32AcqRel(&m_popupCount, 0);
		m_eventCount = 0;
	}
	result = 0;
	if (connection->GetChanges() != 0)
		NotifyListeners();
end:
	SceActivityDb_11D77056(m_access, result);
	return result;
}

/*
 * An event's notifications: the configuration's for the system's own event sources, else the
 * application's own when it gives them, else all.
 */
__attribute__((noinline)) unsigned int NeDaemon::GetNotifyFlags(const LsdbNewEvent *event,
	SceInt64 msgType)
{
	unsigned int flags = LSDB_NOTIFY_ALL;
	AppInfoQClient client;

	SceLsdb_A6D8772C(&client);
	if (SceLsdb_E90B81CD(&client) != 0) {
		SceLsdb_E2101B03(&client);
		return LSDB_NOTIFY_ALL;
	}

	const char *titleId = event->titleId.c_str();
	int key = -1;

	if (sce_paf_strcmp(titleId, "LAUPDATE") == 0) {
		key = NE_CONFIG_NOTIFY_LAUPDATE;
	} else if (sce_paf_strcmp(titleId, "LOGSTATUS0") == 0) {
		key = NE_CONFIG_NOTIFY_LOGSTATUS;
	} else if (sce_paf_strcmp(titleId, "BGCPSTATUS") == 0) {
		key = NE_CONFIG_NOTIFY_BGCPSTATUS;
	} else if (sce_paf_strcmp(titleId, "NPXS10006") == 0) {
		key = msgType == 1025 ? NE_CONFIG_NOTIFY_FRIEND_REQUEST : NE_CONFIG_NOTIFY_FRIEND;
	} else {
		LsdbValue value;

		if (SceLsdb_65ECEF39(&client, &event->titleId, NE_KEY_NOTIFY, &value) == 0) {
			if (value.type == LSDB_VALUE_INTEGER && (value.integer & 1) != 0)
				flags = (unsigned int)value.integer & 0xE;
		}
	}
	if (key != -1)
		flags = (unsigned int)SceLsdb_61F17FDA(&client, key, flags);
	SceLsdb_E2101B03(&client);
	return flags;
}

/* the same nine message arguments */
static inline bool NeSameMsgArgs(const LsdbNewEvent *event, const LsdbNewEvent *old)
{
	for (int i = 0; i < 9; i++)
		if (event->msgArg[i] != old->msgArg[i])
			return false;
	return true;
}

/* event's icon_data differs from old's: in size, else in old's row */
static inline bool NeIconChanged(ActivityDbConnection *connection, const LsdbNewEvent *event,
	const LsdbNewEvent *old)
{
	if (event->iconDataSize != old->iconDataSize)
		return true;
	if (event->iconDataSize == 0)
		return false;
	if (event->iconData != NULL && old->iconDataSize == 0)
		return true;

	int result;
	{
		RefPtr<Statement> statement;
		dbutil::Value value;

		result = connection->Prepare(paf::string("SELECT 1 FROM tbl_newevent WHERE rowid=? "
			"AND icon_data=?"), &statement);
		if (result != 0)
			goto compared;
		result = AppDbBindInt(&statement, 1, old->rowid);
		if (result != 0)
			goto compared;
		memset(&value, 0, sizeof(value));
		value.type = SQLITE_BLOB;
		value.size = event->iconDataSize;
		value.data.blob = event->iconData;
		result = statement->Bind(2, &value, true);
		if (result != 0)
			goto compared;
		result = statement->Step(1);
		if (result != 0 && result != 1)
			goto compared;
	}
compared:
	return result == 1;
}

/*
 * The fields where event differs from old (its row), of those fields names, and bit 32 when
 * both pop up. An icon_data of the same size is compared in the row.
 */
static __attribute__((noinline)) SceUInt64 NeCompare(ActivityDbConnection *connection,
	const LsdbNewEvent *event, const LsdbNewEvent *old, unsigned int fields)
{
	SceUInt64 changes = 0;

	if ((fields & NE_FIELD_MSG) != 0 &&
	    (event->msgType != old->msgType || !NeSameMsgArgs(event, old)))
		changes = NE_FIELD_MSG;
	if ((fields & NE_FIELD_ACT_TYPE) != 0 && event->actType != old->actType)
		changes |= NE_FIELD_ACT_TYPE;
	if ((fields & NE_FIELD_NEW_FLAG) != 0 && event->newFlag != old->newFlag)
		changes |= NE_FIELD_NEW_FLAG;
	if ((fields & NE_FIELD_POPUP_NO) != 0 && event->popupNo != old->popupNo)
		changes |= NE_FIELD_POPUP_NO;
	if (event->popupNo && old->popupNo)
		changes |= 1ULL << 32;
	if ((fields & NE_FIELD_ICON_PATH) != 0 && event->iconPath != old->iconPath)
		changes |= NE_FIELD_ICON_PATH;
	if ((fields & NE_FIELD_ICON) != 0 && NeIconChanged(connection, event, old))
		changes |= NE_FIELD_ICON;
	if ((fields & NE_FIELD_TITLE) != 0 && event->title != old->title)
		changes |= NE_FIELD_TITLE;
	if ((fields & NE_FIELD_DESC) != 0 && event->desc != old->desc)
		changes |= NE_FIELD_DESC;
	if ((fields & NE_FIELD_EXEC) != 0 &&
	    (event->execMode != old->execMode || event->execTitleId != old->execTitleId ||
	     event->execArg != old->execArg))
		changes |= NE_FIELD_EXEC;
	if ((fields & NE_FIELD_TIME_UPDATED) != 0 && event->timeUpdated != old->timeUpdated)
		changes |= NE_FIELD_TIME_UPDATED;
	if ((fields & NE_FIELD_SE) != 0 &&
	    (event->seId != old->seId || event->seVolume != old->seVolume))
		changes |= NE_FIELD_SE;
	return changes;
}

/* an event to insert against its row: the row's rowid and hash, and every field that differs */
static __attribute__((noinline)) int NeFindChanges(ActivityDbConnection *connection,
	LsdbNewEvent *event, SceUInt64 *changes)
{
	LsdbNewEvent old;

	old.titleId = event->titleId;
	old.itemId = event->itemId;
	old.rowid = event->rowid;

	int result = NeReadEvent(connection, &old, false);

	if (result != 0)
		return result;
	event->rowid = old.rowid;
	event->hash = old.hash;
	*changes = NeCompare(connection, event, &old, 0xFFFFFFFF);
	return result;
}

/*
 * Marks the events of count keys deleted (by rowid, else by title and item ID); deleted, when
 * given, the number marked. LSDB_ERROR_NOT_FOUND when none was.
 */
static __attribute__((noinline)) int NeDeleteRows(ActivityDbConnection *connection,
	const LsdbNewEventKey *keys, int count, int *deleted)
{
	RefPtr<Statement> statement;
	RefPtr<Statement> keyStatement;
	int total = 0;
	int result;

	for (int i = 0; i < count; i++) {
		const LsdbNewEventKey *key = &keys[i];

		if (key->rowid != 0) {
			if (statement.Get() == NULL) {
				result = connection->Prepare(paf::string("UPDATE tbl_newevent SET "
					"del_flag=1 WHERE rowid=? AND del_flag=0"), &statement);
				if (result != 0)
					goto end;
			}
			result = AppDbBindInt(&statement, 1, key->rowid);
			if (result != 0)
				goto end;
		} else {
			if (keyStatement.Get() == NULL) {
				result = connection->Prepare(paf::string("UPDATE tbl_newevent "
					"SET del_flag=1 WHERE del_flag=0 AND id_title_id=? AND "
					"id_item_id=?"), &keyStatement);
				if (result != 0)
					goto end;
			}
			statement = keyStatement;
			result = AppDbBindString(&statement, 1, &key->titleId);
			if (result != 0)
				goto end;
			result = AppDbBindString(&statement, 2, &key->itemId);
			if (result != 0)
				goto end;
		}
		do
			result = statement->Step(1);
		while (result == 0);
		if (result != 1)
			goto end;
		total += connection->GetChanges();
		statement->ClearBindings();
		statement->Reset();
	}
	result = 0;
	if (deleted != NULL)
		*deleted = total;
	if (total == 0)
		result = LSDB_ERROR_NOT_FOUND;
end:
	return result;
}

/* an event's time_updated to insert: its own, else the time of the insert */
static inline paf::string NeInsertTime(const LsdbNewEvent *event)
{
	if (event->timeUpdated.size() != 0)
		return event->timeUpdated;
	return paf::string("now");
}

/* binds every column of an event's row, a new popup_no when it pops up */
static __attribute__((noinline)) int NeBindInsert(RefPtr<Statement> *statement,
	const LsdbNewEvent *event)
{
	int result = AppDbBindNull(statement, 1);

	if (result != 0)
		return result;
	result = AppDbBindString(statement, 2, &event->titleId);
	if (result != 0)
		return result;
	result = AppDbBindString(statement, 3, &event->itemId);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 4, 0);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 5, event->msgType);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 6,
		(SceInt64)((SceUInt64)event->notifyFlags << 32 | (unsigned int)event->actType));
	if (result != 0)
		return result;
	{
		paf::string time = NeInsertTime(event);

		result = AppDbBindString(statement, 7, &time);
	}
	if (result != 0)
		return result;
	if (event->newFlag)
		result = AppDbBindInt(statement, 8, 1);
	else
		result = AppDbBindInt(statement, 8, 0);
	if (result != 0)
		return result;

	unsigned int popupNo = 0;

	if (event->popupNo)
		popupNo = s_popupNo++;
	result = AppDbBindInt(statement, 9, popupNo);
	if (result != 0)
		return result;
	result = AppDbBindString(statement, 10, &event->iconPath);
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, 11, event->execMode);
	if (result != 0)
		return result;
	result = AppDbBindString(statement, 12, &event->execTitleId);
	if (result != 0)
		return result;
	result = AppDbBindString(statement, 13, &event->execArg);
	if (result != 0)
		return result;
	result = AppDbBindString(statement, 14, &event->title);
	if (result != 0)
		return result;
	result = AppDbBindString(statement, 15, &event->desc);
	if (result != 0)
		return result;

	int index = 16;

	for (unsigned int i = 0; i < 9; i++) {
		result = AppDbBindString(statement, index++, &event->msgArg[i]);
		if (result != 0)
			return result;
	}
	result = AppDbBindInt(statement, index++, event->seId);
	if (result != 0)
		return result;
	if (event->seVolume == 0.0f) {
		result = AppDbBindNull(statement, index++);
	} else {
		dbutil::Value value;

		memset(&value, 0, sizeof(value));
		value.type = SQLITE_FLOAT;
		value.data.real = event->seVolume;
		result = (*statement)->Bind(index++, &value, false);
	}
	if (result != 0)
		return result;
	result = AppDbBindInt(statement, index++, (unsigned int)event->hash);
	if (result != 0)
		return result;

	dbutil::Value value;

	memset(&value, 0, sizeof(value));
	value.type = SQLITE_BLOB;
	value.size = event->iconDataSize;
	value.data.blob = event->iconData;
	return (*statement)->Bind(index, &value, true);
}

/*
 * Inserts an event; one of the same key is LSDB_ERROR_EXISTS unless replace, which deletes it.
 * The oldest messages go when the table would hold more than NE_EVENTS_MAX events.
 */
__attribute__((noinline)) int NeDaemon::Insert(LsdbNewEvent *event, bool replace)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	SceUInt64 changes = 0;
	ActivityDbConnection *connection = SceActivityDb_82B45E74(m_access);

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = NeFindChanges(connection, event, &changes);
	int excess;

	if (result != 0) {
		if (result != LSDB_ERROR_NOT_FOUND)
			goto end;
		changes = 0xFFFFFFFF;
		replace = false;
	} else if (!replace) {
		result = LSDB_ERROR_EXISTS;
		goto end;
	}

	{
		unsigned int flags = event->notifyFlags;

		event->notifyFlags = (flags & 0x10) != 0 ? flags | LSDB_NOTIFY_ALL :
			flags | GetNotifyFlags(event, event->msgType);
	}
	if (replace) {
		result = NeDeleteRows(connection, event, 1, NULL);
		excess = 0;
		if (result != 0)
			goto end;
	} else {
		int count = m_eventCount;

		if (count < 0) {
			int counted = 0;

			count = 0;
			if (NeCountEvents(connection, &counted, NULL) == 0) {
				count = counted;
				m_eventCount = counted;
			}
		}
		excess = 0;
		if (count >= NE_EVENTS_MAX) {
			excess = count - (NE_EVENTS_MAX - 1);
			{
				RefPtr<Statement> statement;

				result = connection->Prepare(paf::string("UPDATE tbl_newevent "
					"SET del_flag=1 WHERE del_flag=0 AND ?<=msg_type ORDER BY "
					"time_updated ASC LIMIT ?"), &statement);
				if (result == 0) {
					result = AppDbBindInt(&statement, 1, 80);
					if (result == 0) {
						result = AppDbBindInt(&statement, 2, excess);
						if (result == 0) {
							result = statement->Step(1);
							if (result == 1)
								result = 0;
						}
					}
				}
			}
			if (result != 0)
				goto end;
		}
	}
	{
		paf::rtc::Tick tick = 0;

		paf::rtc::GetCurrentTick(&tick);
		event->hash = (int)tick;
	}
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("INSERT INTO tbl_newevent "
			"VALUES(?,?,?,?,?,?,strftime('%Y-%m-%d %H:%M:%f',?),"
			"?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"), &statement);
		if (result != 0)
			goto inserted;
		result = NeBindInsert(&statement, event);
		if (result != 0)
			goto inserted;
		result = statement->Step(1);
		if (result != 1) {
			if (result == LSDB_DBUTIL_ERROR_CONSTRAINT)
				result = LSDB_ERROR_EXISTS;
			goto inserted;
		}
	}
	event->rowid = connection->GetLastInsertRowid();
	result = 0;
inserted:
	if (result == 0) {
		if (!replace)
			m_eventCount += 1 - excess;
		NotifyListeners();
	}
end:
	SceActivityDb_11D77056(m_access, result);
	return result;
}

/* the UPDATE of an event's row for the changed fields (NULL when it does not prepare) */
static __attribute__((noinline)) RefPtr<Statement> NePrepareUpdate(ActivityDbConnection *connection,
	SceUInt64 changes)
{
	paf::string sql("UPDATE tbl_newevent SET ");

	if ((changes & NE_FIELD_ICON_PATH) != 0)
		sql += "icon_path=?,";
	if ((changes & NE_FIELD_ICON) != 0)
		sql += "icon_data=?,";
	if ((changes & NE_FIELD_MSG) != 0)
		sql += "msg_type=?,msg_arg0=?,msg_arg1=?,msg_arg2=?,msg_arg3=?,msg_arg4=?,"
			"msg_arg5=?,msg_arg6=?,msg_arg7=?,msg_arg8=?,";
	if ((changes & NE_FIELD_ACT_TYPE) != 0) {
		char text[128];

		sce_paf_snprintf(text, sizeof(text), "act_type=((act_type & %lld) | (? & %lld)),",
			(SceInt64)0, (SceInt64)-1);
		sql += text;
	}
	if ((changes & NE_FIELD_TITLE) != 0)
		sql += "title=?,";
	if ((changes & NE_FIELD_DESC) != 0)
		sql += "desc=?,";
	if ((changes & NE_FIELD_EXEC) != 0)
		sql += "exec_mode=?,exec_title_id=?,exec_arg=?,";
	if ((changes & NE_FIELD_NEW_FLAG) != 0)
		sql += "new_flag=?,";
	if ((changes & NE_FIELD_TIME_UPDATED) != 0)
		sql += "time_updated=strftime('%Y-%m-%d %H:%M:%f',?),";
	if ((changes & NE_FIELD_SE) != 0)
		sql += "se_id=?,se_volume=?,";
	if ((changes & NE_FIELD_POPUP_NO) != 0 || (changes & 1ULL << 32) != 0)
		sql += "popup_no=?,";
	if (changes != 1ULL << 32)
		sql += "hash=?,";
	sql.replace(sql.size() - 1, ' ', 1);
	sql += "WHERE rowid=?";

	RefPtr<Statement> statement;

	if (connection->Prepare(sql, &statement) != 0)
		return RefPtr<Statement>();
	return statement;
}

/* binds the changed fields of an event to its UPDATE, the next hash and the row's rowid */
static __attribute__((noinline)) int NeBindUpdate(RefPtr<Statement> *statement,
	ActivityDbConnection *connection, SceUInt64 changes, const LsdbNewEvent *event,
	SceInt64 *rowid, int *hash)
{
	int index = 1;
	int result;

	if ((changes & NE_FIELD_ICON_PATH) != 0) {
		result = AppDbBindString(statement, index++, &event->iconPath);
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_ICON) != 0) {
		dbutil::Value value;

		memset(&value, 0, sizeof(value));
		value.type = SQLITE_BLOB;
		value.size = event->iconDataSize;
		value.data.blob = event->iconData;
		result = (*statement)->Bind(index++, &value, true);
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_MSG) != 0) {
		result = AppDbBindInt(statement, index++, event->msgType);
		if (result != 0)
			return result;
		for (unsigned int i = 0; i < 9; i++) {
			result = AppDbBindString(statement, index++, &event->msgArg[i]);
			if (result != 0)
				return result;
		}
	}
	if ((changes & NE_FIELD_ACT_TYPE) != 0) {
		result = AppDbBindInt(statement, index++, event->actType);
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_TITLE) != 0) {
		result = AppDbBindString(statement, index++, &event->title);
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_DESC) != 0) {
		result = AppDbBindString(statement, index++, &event->desc);
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_EXEC) != 0) {
		result = AppDbBindInt(statement, index++, event->execMode);
		if (result != 0)
			return result;
		result = AppDbBindString(statement, index++, &event->execTitleId);
		if (result != 0)
			return result;
		result = AppDbBindString(statement, index++, &event->execArg);
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_NEW_FLAG) != 0) {
		if (event->newFlag)
			result = AppDbBindInt(statement, index++, 1);
		else
			result = AppDbBindInt(statement, index++, 0);
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_TIME_UPDATED) != 0) {
		{
			paf::string time = NeInsertTime(event);

			result = AppDbBindString(statement, index++, &time);
		}
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_SE) != 0) {
		result = AppDbBindInt(statement, index++, event->seId);
		if (result != 0)
			return result;
		if (event->seVolume == 0.0f) {
			result = AppDbBindNull(statement, index++);
		} else {
			dbutil::Value value;

			memset(&value, 0, sizeof(value));
			value.type = SQLITE_FLOAT;
			value.data.real = event->seVolume;
			result = (*statement)->Bind(index++, &value, false);
		}
		if (result != 0)
			return result;
	}
	if ((changes & NE_FIELD_POPUP_NO) != 0 || (changes & 1ULL << 32) != 0) {
		unsigned int popupNo = 0;

		if (event->popupNo)
			popupNo = s_popupNo++;
		result = AppDbBindInt(statement, index++, popupNo);
		if (result != 0)
			return result;
	}
	if (changes != 1ULL << 32) {
		*hash += 1;
		result = AppDbBindInt(statement, index++, (unsigned int)*hash);
		if (result != 0)
			return result;
	}
	return AppDbBindInt(statement, index, *rowid);
}

/* keeps a deleted copy of the row rowid (for a popup it still has) */
static inline int NeCopyRow(ActivityDbConnection *connection, const SceInt64 *rowid)
{
	RefPtr<Statement> statement;
	int result;

	result = connection->Prepare(paf::string("INSERT INTO tbl_newevent SELECT NULL,"
		"id_title_id,id_item_id,1,msg_type,act_type,time_updated,new_flag,popup_no,"
		"icon_path,exec_mode,exec_title_id,exec_arg,title,desc,msg_arg0,msg_arg1,"
		"msg_arg2,msg_arg3,msg_arg4,msg_arg5,msg_arg6,msg_arg7,msg_arg8,se_id,se_volume,"
		"hash,icon_data FROM tbl_newevent WHERE rowid=?"), &statement);
	if (result == 0) {
		result = AppDbBindInt(&statement, 1, *rowid);
		if (result == 0) {
			result = statement->Step(1);
			if (result == 1)
				result = 0;
		}
	}
	return result;
}

/*
 * Writes an event's changes to its row; an event that still pops up first leaves a deleted copy
 * of the row to pop up.
 */
__attribute__((noinline)) int NeDaemon::UpdateRow(ActivityDbConnection *connection,
	SceUInt64 changes, const LsdbNewEvent *event, SceInt64 *rowid, int *hash)
{
	int result = 0;

	if ((changes & 1ULL << 32) != 0)
		result = NeCopyRow(connection, rowid);
	if (result == 0) {
		RefPtr<Statement> statement = NePrepareUpdate(connection, changes);

		if (statement.Get() == NULL)
			return LSDB_ERROR_INVALID_ARGUMENT;
		result = NeBindUpdate(&statement, connection, changes, event, rowid, hash);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 1)
			goto end;
		result = 0;
		if (connection->GetChanges() == 0) {
			result = LSDB_ERROR_NOT_FOUND;
			goto end;
		}
	}
end:
	return result;
}

/* an event to update against its row: the row's rowid and hash, and the fields that differ */
static __attribute__((noinline)) int NeFindUpdate(ActivityDbConnection *connection,
	const LsdbNewEvent *event, unsigned int fields, SceUInt64 *changes, SceInt64 *rowid,
	int *hash, bool deleted)
{
	LsdbNewEvent old;

	old.titleId = event->titleId;
	old.itemId = event->itemId;
	old.rowid = event->rowid;

	int result = NeReadEvent(connection, &old, deleted);

	if (result != 0)
		return result;
	*hash = old.hash;
	*rowid = old.rowid;
	*changes = NeCompare(connection, event, &old, fields);
	return result;
}

/*
 * Updates the fields of count events (deleted ones too, when deleted); keepPopup leaves no
 * copy of an event that still pops up. LSDB_ERROR_NOT_FOUND when none changed.
 */
__attribute__((noinline)) int NeDaemon::Update(LsdbNewEventPtr *events, int count,
	unsigned int fields, bool deleted, bool keepPopup)
{
	int updated = 0;

	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = SceActivityDb_82B45E74(m_access);

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	for (int i = 0; i < count; i++) {
		const LsdbNewEvent *event = events[i].get();
		SceUInt64 changes = 0;
		SceInt64 rowid = 0;
		int hash = 0;

		result = NeFindUpdate(connection, event, fields, &changes, &rowid, &hash, deleted);
		if (result != 0) {
			if (result != LSDB_ERROR_NOT_FOUND)
				goto end;
			result = 0;
		} else {
			if (keepPopup)
				changes &= ~(1ULL << 32);
			result = UpdateRow(connection, changes, event, &rowid, &hash);
			if (result != 0) {
				if (result != LSDB_ERROR_NOT_FOUND)
					goto end;
			} else {
				updated++;
			}
		}
	}
	if (updated != 0)
		NotifyListeners();
	else
		result = LSDB_ERROR_NOT_FOUND;
end:
	SceActivityDb_11D77056(m_access, result);
	return result;
}

/* marks count events deleted, in a transaction of its own */
inline int NeDaemon::DeleteKeys(const LsdbNewEventKey *keys, int count)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = SceActivityDb_82B45E74(m_access);

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int deleted = 0;
	int result = NeDeleteRows(connection, keys, count, &deleted);

	if (result == 0) {
		m_eventCount -= deleted;
		NotifyListeners();
	}
	SceActivityDb_11D77056(m_access, result);
	return result;
}

__attribute__((noinline)) int NeDaemon::Delete(const LsdbNewEventKey *keys, int count)
{
	return DeleteKeys(keys, count);
}

/* no event is new any more */
__attribute__((noinline)) int NeDaemon::ClearNewFlags()
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = SceActivityDb_82B45E74(m_access);

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("UPDATE tbl_newevent SET new_flag=0, "
			"hash=hash+1 WHERE new_flag=1"), &statement);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 1)
			goto end;
	}
	result = 0;
	if (connection->GetChanges() != 0) {
		NotifyListeners();
		result = 0;
	}
end:
	SceActivityDb_11D77056(m_access, result);
	return result;
}

__attribute__((noinline)) int NeDaemon::GetPopupCount()
{
	return m_popupCount;
}

/* deletes the messages (msg_type 256 on) that run an application, when it goes */
__attribute__((noinline)) int NeDaemon::DeleteTitleEvents(const char *titleId)
{
	paf::vector<LsdbNewEventKey> keys;
	ActivityDbConnection *connection;

	if (m_access == NULL || (connection = SceActivityDb_82B45E74(m_access)) == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;
	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("SELECT rowid FROM tbl_newevent WHERE "
			"exec_title_id=? AND ?<=msg_type AND del_flag=0"), &statement);
		if (result != 0)
			goto end;
		result = AppDbBindText(&statement, 1, titleId, -1);
		if (result != 0)
			goto end;
		result = AppDbBindInt(&statement, 2, 256);
		if (result != 0)
			goto end;
		while ((result = statement->Step(1)) == 0) {
			dbutil::Value value;

			result = statement->GetValue(0, &value);
			if (result != 0)
				goto end;
			keys.push_back(LsdbNewEventKey());
			keys[keys.size() - 1].rowid =
				value.type == SQLITE_INTEGER ? value.data.integer : 0;
		}
		if (result != 1)
			goto end;
	}
	result = 0;
	if (keys.size() != 0) {
		result = DeleteKeys(&keys[0], keys.size());
		if (result == LSDB_ERROR_NOT_FOUND)
			result = 0;
	}
end:
	SceActivityDb_11D77056(m_access, result);
	return result;
}

/* the SQL functions: the count of events to pop up, and the next popup_no */
inline void NeDaemon::IncrementPopupNum(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	sceAtomicIncrement32AcqRel(&Instance()->m_popupCount);
}

inline void NeDaemon::DecrementPopupNum(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	sceAtomicDecrement32AcqRel(&Instance()->m_popupCount);
}

inline void NeDaemon::GetNextPopupNo(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	sqlite3_result_int64(context, s_popupNo++);
}
