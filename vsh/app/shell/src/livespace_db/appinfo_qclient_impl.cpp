/*
 * The reader's implementation: each query takes the daemon's connection for a transaction of
 * its own, which it rolls back.
 */
#include "livespace_db_internal.h"

/* the transaction's end that keeps nothing */
#define LSDB_TRANSACTION_ROLLBACK	-1

AppInfoQClientImpl::AppInfoQClientImpl() : m_changed(0)
{
}

AppInfoQClientImpl::~AppInfoQClientImpl()
{
	AppInfoDaemon::GetInstance()->RemoveListener(this);
}

/* notes the daemon's change */
static void AppInfoQClientImplOnChange(int event, void *info, unsigned int userData)
{
	sceAtomicExchange32AcqRel(&((AppInfoQClientImpl *)userData)->m_changed, 1);
}

int AppInfoQClientImpl::Open()
{
	return AppInfoDaemon::GetInstance()->AddListener(AppInfoQClientImplOnChange, this);
}

int AppInfoQClientImpl::GetAppInfo(const paf::string *titleId, LsdbAppInfoPtr *info)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbGetAppInfo(connection, titleId, info);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::GetAppInfoValue(const paf::string *titleId, unsigned int key,
	LsdbValue *value)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbGetAppInfoValue(connection, titleId, key, value);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::HasAppInfo(const paf::string *titleId)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return 0;

	int result = AppDbHasAppInfo(connection, titleId);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::FindTitleIcon(const paf::string *titleId, LsdbIconPtr *icon, bool theme)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbFindTitleIcon(connection, titleId, icon, NULL, theme);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::FindTitles(LsdbQueryCondition *conditions,
	paf::vector<paf::string> *titleIds, unsigned int flags)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbFindTitles(connection, conditions, titleIds, flags);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::GetTitlesByCategory(const char **categories, unsigned int count,
	int limit, paf::vector<paf::string> *titleIds)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbGetTitlesByCategory(connection, categories, count, limit, titleIds);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::GetUriTitle(const char *uri, paf::string *titleId, unsigned int flags)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbGetUriTitle(connection, uri, flags, titleId);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::GetSchemeUris(const char **schemes, unsigned int count,
	paf::vector<paf::string> *uris)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbGetSchemeUris(connection, schemes, count, uris);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

paf::string AppInfoQClientImpl::GetConfigString(int key, const char *defaultValue)
{
	paf::string value;
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return paf::string(defaultValue);
	value = AppDbGetConfigString(connection, key, defaultValue);
	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return value;
}

SceInt64 AppInfoQClientImpl::GetConfigInt64(int key, SceInt64 defaultValue)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return defaultValue;

	SceInt64 value = AppDbGetConfigInt64(connection, key, defaultValue);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return value;
}

bool AppInfoQClientImpl::IsChanged(bool clear)
{
	if (!clear)
		return m_changed != 0;
	return sceAtomicCompareAndSwap32AcqRel(&m_changed, 1, 0) != 0;
}

int AppInfoQClientImpl::GetThemeIds(paf::vector<paf::string> *ids)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbGetThemeIds(connection, ids);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

int AppInfoQClientImpl::GetTheme(const paf::string *id, int language, LsdbTheme *theme)
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = AppDbGetTheme(connection, id, language, theme);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return result;
}

SceInt64 AppInfoQClientImpl::GetThemesSize()
{
	ActivityDbConnection *connection = AppInfoDaemon::GetInstance()->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	SceInt64 size = AppDbGetThemesSize(connection);

	AppInfoDaemon::GetInstance()->EndTransaction(LSDB_TRANSACTION_ROLLBACK);
	return size;
}
