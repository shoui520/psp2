/*
 * dbutil::Record: the row a statement selects, read into Values the record owns, and kept
 * current by the connection's events.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <std/vector>
#include <sqlite3.h>
#include "connection.h"
#include "statement.h"
#include "record.h"
#include "value.h"

namespace dbutil {

Record::Record()
	: m_connection(NULL), m_columnCount(0), m_values(NULL), m_valid(false),
	  m_changeProcess(0), m_changeSource(NULL), m_changed(false), m_heap(NULL),
	  m_refCount(0), m_lock("SceDbUtilRecord", 0), m_flagLock("SceDbUtilRecordFlag", 0)
{
}

/* the statement's next row into the values, a copy of each text and blob */
int Record::ReadValues(RefPtr<Statement> &statement)
{
	paf::thread::RMutex *lock = &m_connection->m_lock;
	int ret;

	lock->Lock();
	ret = statement->Step(1);
	switch (ret) {
	case 0:
		break;
	case 1:
		ret = DBUTIL_ERROR_NOT_FOUND;
		/* fall through */
	default:
		lock->Unlock();
		return ret;
	}

	m_columnCount = statement->m_columnCount;
	if (m_columnCount != 0) {
		m_lock.Lock();
		m_values = (Value *)Allocate(m_heap, m_columnCount * sizeof(Value));
		if (m_values == NULL) {
			m_lock.Unlock();
			lock->Unlock();
			return DBUTIL_ERROR_NO_MEMORY;
		}
		sce_paf_memset(m_values, 0, m_columnCount * sizeof(Value));
		m_lock.Unlock();
	}

	m_lock.Lock();
	for (int i = 0; i < m_columnCount; i++) {
		Value value;

		ret = statement->GetValue(i, &value);

		Value *field = &m_values[i];

		field->name = value.name;
		field->type = value.type;
		switch (value.type) {
		case SQLITE_INTEGER:
			field->data.integer = value.data.integer;
			field->size = sizeof(SceInt64);
			break;
		case SQLITE_FLOAT:
			field->size = sizeof(double);
			field->data.real = value.data.real;
			break;
		case SQLITE_TEXT: {
			field->size = value.size;

			char *text = (char *)Allocate(m_heap, value.size + 1);

			if (text != NULL) {
				text[value.size] = '\0';
				sce_paf_memcpy(text, value.data.text, value.size);
				m_values[i].data.text = text;
			} else {
				ret = DBUTIL_ERROR_NO_MEMORY;
			}
			break;
		}
		case SQLITE_BLOB: {
			field->size = value.size;

			void *blob = Allocate(m_heap, value.size);

			if (blob != NULL) {
				sce_paf_memcpy(blob, value.data.blob, value.size);
				m_values[i].data.blob = blob;
			} else {
				ret = DBUTIL_ERROR_NO_MEMORY;
			}
			break;
		}
		case SQLITE_NULL:
			field->size = 0;
			break;
		default:
			ret = DBUTIL_ERROR_INVALID_TYPE;
			break;
		}
		if (ret != 0)
			break;
	}
	m_lock.Unlock();
	lock->Unlock();
	return ret;
}

/* the row of table with rowid */
Record::Record(Connection *connection, const paf::string &table, const paf::string &columns,
	       SceInt64 rowid, void *heap)
	: m_connection(connection), m_columnCount(0), m_values(NULL), m_valid(false),
	  m_changeProcess(0), m_changeSource(NULL), m_changed(false), m_heap(heap),
	  m_refCount(0), m_lock("SceDbUtilRecord", 0), m_flagLock("SceDbUtilRecordFlag", 0)
{
	Row row = { rowid, table };
	RefPtr<Statement> statement;
	char *sql = sqlite3_mprintf("SELECT %s FROM %s WHERE rowid=%lld", columns.c_str(),
				    table.c_str(), rowid);

	int ret = connection->Prepare(paf::string(sql), &statement);

	if (ret == 0 && ReadValues(statement) == 0) {
		m_statement = statement;
		m_statement->Reset();
		m_rows.push_back(row);
		m_valid = true;
	}
	sqlite3_free(sql);
}

/* the join of each table's row with its rowid; a rowid of 0 leaves its table out */
Record::Record(Connection *connection, const paf::vector<paf::string> &tables,
	       const paf::string &columns, const paf::vector<SceInt64> &rowids, void *heap)
	: m_connection(connection), m_columnCount(0), m_values(NULL), m_valid(false),
	  m_changeProcess(0), m_changeSource(NULL), m_changed(false), m_heap(heap),
	  m_refCount(0), m_lock("SceDbUtilRecord", 0), m_flagLock("SceDbUtilRecordFlag", 0)
{
	RefPtr<Statement> statement;

	if (tables.size() == rowids.size()) {
		for (unsigned int i = 0; i < tables.size(); i++) {
			Row row = { rowids[i], tables[i] };

			m_rows.push_back(row);
		}

		paf::string sql = "SELECT " + columns + " FROM ";

		for (unsigned int i = 0; i < tables.size(); i++) {
			if (rowids[i] != 0)
				sql.append(tables[i] + ",");
		}
		sql.erase(sql.rfind(','), 1);
		sql += " WHERE";
		for (unsigned int i = 0; i < rowids.size(); i++) {
			if (rowids[i] != 0) {
				char *where = sqlite3_mprintf(" %s.rowid=%lld ", tables[i].c_str(),
							      rowids[i]);

				sql += where;
				sql += "AND ";
				sqlite3_free(where);
			}
		}
		sql.erase(sql.rfind("AND", sql.size(), sce_paf_strlen("AND")), 3);
		if (m_connection->Prepare(sql, &statement) == 0) {
			if (ReadValues(statement) == 0) {
				m_statement = statement;
				m_valid = true;
			}
			statement->Reset();
		}
	}
}

/* frees the values and each text and blob they hold */
int Record::FreeValues()
{
	m_lock.Lock();
	if (m_values != NULL) {
		for (int i = 0; i < m_columnCount; i++) {
			switch (m_values[i].type) {
			case SQLITE_TEXT:
				if (m_values[i].data.text != NULL)
					Free(m_heap, (void *)m_values[i].data.text);
				m_values[i].data.text = NULL;
				break;
			case SQLITE_BLOB:
				if (m_values[i].data.blob != NULL)
					Free(m_heap, (void *)m_values[i].data.blob);
				m_values[i].data.blob = NULL;
				break;
			}
		}
		Free(m_heap, m_values);
		m_values = NULL;
	}
	m_lock.Unlock();
	return 0;
}

Record::~Record()
{
	if (m_connection != NULL)
		m_connection->RemoveListener(this);
	if (m_statement.Get() != NULL)
		m_statement->Finalize();
	FreeValues();
}

int Record::AddRef()
{
	int count;

	m_lock.Lock();
	count = ++m_refCount;
	m_lock.Unlock();
	return count;
}

int Record::Release()
{
	int count;

	m_lock.Lock();
	count = --m_refCount;
	if (count != 0) {
		m_lock.Unlock();
		return count;
	}
	m_lock.Unlock();
	delete this;
	return 0;
}

/* Sony's test lets every index through: the first test passes for any index from 0 */
const char *Record::GetTableName(int index)
{
	if (index >= 0 || index < (int)m_rows.size())
		return m_rows[index].table.c_str();
	return NULL;
}

int Record::GetValue(int column, Value *value)
{
	if (column < 0 || column > m_columnCount - 1 || value == NULL)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	sce_paf_memcpy(value, &m_values[column], sizeof(Value));
	return 0;
}

int Record::GetValue(const paf::string &name, Value *value)
{
	sce_paf_memset(value, 0, sizeof(Value));
	if (!m_valid)
		return DBUTIL_ERROR_INVALID_STATE;
	for (int i = 0; i < m_columnCount; i++) {
		if (sce_paf_strcmp(m_values[i].name, name.c_str()) == 0)
			return GetValue(i, value);
	}
	return DBUTIL_ERROR_NOT_FOUND;
}

bool Record::IsChanged()
{
	bool changed;

	m_flagLock.Lock();
	changed = m_changed;
	m_flagLock.Unlock();
	return changed;
}

/* reads the row again when an event changed it */
int Record::Update()
{
	int ret;

	if (!m_valid)
		return DBUTIL_ERROR_INVALID_STATE;
	m_flagLock.Lock();
	if (!m_changed) {
		m_flagLock.Unlock();
		return 0;
	}
	m_changed = false;
	m_flagLock.Unlock();
	FreeValues();
	ret = ReadValues(m_statement);
	m_statement->Reset();
	return ret;
}

/*
 * A change to one of the record's rows is remembered until its transaction ends: its commit
 * marks the record changed, its rollback forgets it. An overflow may have lost such events, so
 * it marks the record changed; the connection's close ends the statement.
 */
void Record::OnEvent(Event *event)
{
	switch (event->type) {
	case DBUTIL_EVENT_COMMIT:
		m_lock.Lock();
		if (event->processId == m_changeProcess && event->source == m_changeSource)
			m_changed = true;
		m_lock.Unlock();
		break;
	case DBUTIL_EVENT_ROLLBACK:
		m_lock.Lock();
		if (event->processId == m_changeProcess && event->source == m_changeSource)
			m_changeProcess = 0;
		m_lock.Unlock();
		break;
	case DBUTIL_EVENT_UPDATE:
	case DBUTIL_EVENT_INSERT:
	case DBUTIL_EVENT_DELETE: {
		paf::string table = event->database + "." + event->table;

		for (paf::vector<Row>::iterator it = m_rows.begin(); it != m_rows.end(); ++it) {
			if (it->rowid == event->rowid && it->table == table) {
				m_changeProcess = event->processId;
				m_changeSource = event->source;
				break;
			}
		}
		break;
	}
	case DBUTIL_EVENT_OVERFLOW:
		m_lock.Lock();
		m_changed = true;
		m_lock.Unlock();
		break;
	case DBUTIL_EVENT_CLOSE: {
		paf::thread::RMutex *lock = &m_connection->m_lock;

		lock->Lock();
		m_lock.Lock();
		if (m_valid) {
			m_valid = false;
			m_statement->Finalize();
			m_statement.Clear();
		}
		m_connection = NULL;
		m_lock.Unlock();
		lock->Unlock();
		break;
	}
	}
}

}
