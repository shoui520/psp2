/*
 * The application database's reader, for threads other than the daemon's: each export is a
 * method of the reader, which hands the query to its implementation once it is open.
 */
#include "livespace_db_internal.h"

extern "C" {

/* the constructors (complete and base object) */
AppInfoQClient *SceLsdb_A6D8772C(AppInfoQClient *client)
{
	client->m_impl = NULL;
	return client;
}

AppInfoQClient *SceLsdb_23098254(AppInfoQClient *client)
{
	client->m_impl = NULL;
	return client;
}

int SceLsdb_DEF5C3F9(AppInfoQClient *client)
{
	if (client->m_impl != NULL) {
		delete client->m_impl;
		client->m_impl = NULL;
	}
	return 0;
}

/* the destructors */
AppInfoQClient *SceLsdb_E2101B03(AppInfoQClient *client)
{
	if (client->m_impl != NULL) {
		delete client->m_impl;
		client->m_impl = NULL;
	}
	return client;
}

AppInfoQClient *SceLsdb_D13CE1CD(AppInfoQClient *client)
{
	if (client->m_impl != NULL) {
		delete client->m_impl;
		client->m_impl = NULL;
	}
	return client;
}

int SceLsdb_E90B81CD(AppInfoQClient *client)
{
	if (client->m_impl != NULL)
		return LSDB_ERROR_NOT_READY;
	client->m_impl = new AppInfoQClientImpl;

	int result = client->m_impl->Open();

	if (result != 0 && client->m_impl != NULL) {
		delete client->m_impl;
		client->m_impl = NULL;
	}
	return result;
}

int SceLsdb_D239031A(AppInfoQClient *client, const paf::string *titleId, LsdbAppInfoPtr *info)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetAppInfo(titleId, info);
}

int SceLsdb_65ECEF39(AppInfoQClient *client, const paf::string *titleId, unsigned int key,
	LsdbValue *value)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetAppInfoValue(titleId, key, value);
}

int SceLsdb_9B5ABE3E(AppInfoQClient *client, const paf::string *titleId)
{
	if (client->m_impl == NULL)
		return 0;
	return client->m_impl->HasAppInfo(titleId);
}

int SceLsdb_7AF9008C(AppInfoQClient *client, const paf::string *titleId, LsdbIconPtr *icon,
	bool theme)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->FindTitleIcon(titleId, icon, theme);
}

int SceLsdb_76B75713(AppInfoQClient *client, LsdbQueryCondition *conditions,
	paf::vector<paf::string> *titleIds, unsigned int flags)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->FindTitles(conditions, titleIds, flags);
}

int SceLsdb_5AF49C73(AppInfoQClient *client, const char **categories, unsigned int count,
	int limit, paf::vector<paf::string> *titleIds)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetTitlesByCategory(categories, count, limit, titleIds);
}

int SceLsdb_02D26FE2(AppInfoQClient *client, const char *uri, paf::string *titleId,
	unsigned int flags)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetUriTitle(uri, titleId, flags);
}

int SceLsdb_52DAB131(AppInfoQClient *client, const char **schemes, unsigned int count,
	paf::vector<paf::string> *uris)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetSchemeUris(schemes, count, uris);
}

paf::string SceLsdb_A5264247(AppInfoQClient *client, int key, const char *defaultValue)
{
	if (client->m_impl == NULL)
		return paf::string(defaultValue);
	return client->m_impl->GetConfigString(key, defaultValue);
}

SceInt64 SceLsdb_61F17FDA(AppInfoQClient *client, int key, SceInt64 defaultValue)
{
	if (client->m_impl == NULL)
		return defaultValue;
	return client->m_impl->GetConfigInt64(key, defaultValue);
}

/* whether the daemon changed an application since the last clear */
bool SceLsdb_924913DF(AppInfoQClient *client, bool clear)
{
	if (client->m_impl == NULL)
		return false;
	return client->m_impl->IsChanged(clear);
}

int SceLsdb_24EB9524(AppInfoQClient *client, paf::vector<paf::string> *ids)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetThemeIds(ids);
}

int SceLsdb_64DD44C2(AppInfoQClient *client, const paf::string *id, int language,
	LsdbTheme *theme)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetTheme(id, language, theme);
}

SceInt64 SceLsdb_353E5BEF(AppInfoQClient *client)
{
	if (client->m_impl == NULL)
		return LSDB_ERROR_NOT_READY;
	return client->m_impl->GetThemesSize();
}

}
