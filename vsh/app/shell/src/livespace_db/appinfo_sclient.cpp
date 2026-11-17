/*
 * SceLsdb's start and stop, and the application information client the shell calls: each
 * export is a method of the client, which hands the request to its implementation.
 */
#include "livespace_db_internal.h"

extern "C" {

void lsdbModuleInit(void);
void lsdbModuleTerm(void);

int module_start(SceSize argSize, const void *argp)
{
	lsdbModuleInit();
	return 0;
}

int module_stop(SceSize argSize, const void *argp)
{
	lsdbModuleTerm();
	return 0;
}

/* the constructors (complete and base object) */
AppInfoSClient *SceLsdb_D322A870(AppInfoSClient *client)
{
	client->m_impl = new AppInfoSClientImpl;
	return client;
}

AppInfoSClient *SceLsdb_D55B5DD6(AppInfoSClient *client)
{
	client->m_impl = new AppInfoSClientImpl;
	return client;
}

/* the destructors */
AppInfoSClient *SceLsdb_56D8DCF0(AppInfoSClient *client)
{
	delete client->m_impl;
	return client;
}

AppInfoSClient *SceLsdb_571ACD2D(AppInfoSClient *client)
{
	delete client->m_impl;
	return client;
}

int SceLsdb_D74AC354(AppInfoSClient *client, int pageNo, unsigned int count,
	LsdbPageIconLists *pages)
{
	return client->m_impl->GetIcons(pageNo, count, pages);
}

int SceLsdb_8FAD093B(AppInfoSClient *client, int pageNo, unsigned int flags, LsdbPageList *pages)
{
	return client->m_impl->GetPage(pageNo, flags, pages);
}

int SceLsdb_854DB867(AppInfoSClient *client, paf::vector<int> *pageNos, LsdbPageIconLists *lists)
{
	return client->m_impl->ReplacePageIcons(pageNos, lists);
}

int SceLsdb_C1A82AC8(AppInfoSClient *client, int pageNo, const LsdbIconPtr *icon,
	unsigned int fields, const void *image, int imageSize)
{
	return client->m_impl->UpdateIcon(pageNo, icon, fields, image, imageSize);
}

int SceLsdb_ED996867(AppInfoSClient *client, int pageNo, const LsdbIconPtr *icon)
{
	return client->m_impl->AddIcon(pageNo, icon);
}

int SceLsdb_1A56D1F8(AppInfoSClient *client, const paf::string *titleId)
{
	return client->m_impl->DeleteTitleIcons(titleId);
}

int SceLsdb_BF7AFB42(AppInfoSClient *client, paf::vector<int> *fromPages,
	paf::vector<int> *fromPositions, paf::vector<int> *toPages, paf::vector<int> *toPositions)
{
	return client->m_impl->MoveIcons(fromPages, fromPositions, toPages, toPositions);
}

/* the icon's image, as a shared blob; the result goes to *pResult */
paf::common::SharedPtr<DbBlob> SceLsdb_41185388(AppInfoSClient *client, const SceInt64 *rowid,
	int *pResult)
{
	paf::common::SharedPtr<DbBlob> image;
	int result = LSDB_ERROR_NOT_READY;

	if (client->m_impl != NULL)
		result = client->m_impl->GetIconImage(rowid, &image);
	if (pResult != NULL)
		*pResult = result;
	return image;
}

int SceLsdb_6769DBC4(AppInfoSClient *client, paf::vector<int> *pageNos, LsdbPageList *pages)
{
	return client->m_impl->UpdatePage(pageNos, pages);
}

int SceLsdb_3387B5A3(AppInfoSClient *client, int pageNo, LsdbIconList *icons, LsdbPage *page)
{
	return client->m_impl->AddPage(pageNo, icons, page);
}

int SceLsdb_76C05B2E(AppInfoSClient *client, int pageNo)
{
	return client->m_impl->DeletePage(pageNo);
}

int SceLsdb_E786531F(AppInfoSClient *client, int pageNo, const LsdbIconPtr *icon,
	const LsdbIconPtr *folderIcon, const void *image, int imageSize)
{
	return client->m_impl->CreateFolder(pageNo, icon, folderIcon, image, imageSize);
}

int SceLsdb_94128795(AppInfoSClient *client)
{
	return client->m_impl->GetPageCount();
}

int SceLsdb_CF4B3684(AppInfoSClient *client, int pageNo, unsigned int pageCount)
{
	return client->m_impl->CountIcons(pageNo, pageCount);
}

int SceLsdb_4DB9BCB4(AppInfoSClient *client)
{
	return client->m_impl->CountUserIcons();
}

int SceLsdb_148D7572(AppInfoSClient *client, const paf::string *title,
	paf::vector<int> *folderIds)
{
	return client->m_impl->FindFolders(title, folderIds);
}

int SceLsdb_87EE0526(AppInfoSClient *client, const paf::string *titleId, LsdbAppInfoPtr *info)
{
	return client->m_impl->GetAppInfo(titleId, info);
}

int SceLsdb_DB73BD7E(AppInfoSClient *client, const paf::string *titleId, unsigned int key, LsdbValue *value)
{
	return client->m_impl->GetAppInfoValue(titleId, key, value);
}

int SceLsdb_76EC65E3(AppInfoSClient *client, const paf::string *titleId)
{
	return client->m_impl->HasAppInfo(titleId);
}

int SceLsdb_DE0DA538(AppInfoSClient *client, const paf::string *titleId, LsdbIconPtr *icon,
	int *pageNo, bool theme)
{
	return client->m_impl->FindTitleIcon(titleId, icon, pageNo, theme);
}

int SceLsdb_7F53B3B6(AppInfoSClient *client, const int *folderId, int *pageNo, const LsdbIconPtr *icon)
{
	return client->m_impl->GetFolderIcon(folderId, pageNo, icon);
}

int SceLsdb_DD7D68C0(AppInfoSClient *client, LsdbQueryCondition *conditions,
	paf::vector<paf::string> *titleIds, unsigned int flags)
{
	return client->m_impl->FindTitles(conditions, titleIds, flags);
}

int SceLsdb_FA25AF8D(AppInfoSClient *client, const char **categories, unsigned int count,
	int limit, paf::vector<paf::string> *titleIds)
{
	return client->m_impl->GetTitlesByCategory(categories, count, limit, titleIds);
}

/* the title that handles a URI best: the longest hostpath prefix, then the latest launch */
int SceLsdb_E2838878(AppInfoSClient *client, const char *uri, paf::string *titleId,
	unsigned int flags)
{
	return client->m_impl->GetUriTitle(uri, titleId, flags);
}

/* the URIs ("scheme:hostpath") the applications give for schemes */
int SceLsdb_391170D3(AppInfoSClient *client, const char **schemes, unsigned int count,
	paf::vector<paf::string> *uris)
{
	return client->m_impl->GetSchemeUris(schemes, count, uris);
}

int SceLsdb_84D3F674(AppInfoSClient *client, LsdbAppInfoPtr *app, int pageNo, const LsdbIconPtr *icon,
	paf::vector<LsdbAppInfoPtr> *related)
{
	return client->m_impl->RegisterApp(app, pageNo, icon, related);
}

int SceLsdb_B33FE18E(AppInfoSClient *client, const paf::string *titleId, bool removeFolder, int *pageNo)
{
	return client->m_impl->UnregisterApp(titleId, removeFolder, pageNo);
}

int SceLsdb_DEA13663(AppInfoSClient *client, LsdbAppInfoPtr *app, const LsdbIconPtr *icon,
	paf::vector<LsdbAppInfoPtr> *related)
{
	return client->m_impl->UpdateApp(app, icon, related);
}

int SceLsdb_1AD44F35(AppInfoSClient *client, LsdbAppInfoPtr *app, const LsdbIconPtr *icon,
	unsigned int fields, paf::vector<LsdbAppInfoPtr> *related)
{
	return client->m_impl->RefreshApp(app, icon, fields, related);
}

int SceLsdb_7D7F6809(AppInfoSClient *client, const paf::string *titleId, unsigned int key,
	const LsdbValue *bits, SceInt64 mask)
{
	return client->m_impl->UpdateAppInfoBits(titleId, key, bits, mask);
}

paf::string SceLsdb_786F479E(AppInfoSClient *client, int key, const char *defaultValue)
{
	return client->m_impl->GetConfigString(key, defaultValue);
}

SceInt64 SceLsdb_33587611(AppInfoSClient *client, int key, SceInt64 defaultValue)
{
	return client->m_impl->GetConfigInt64(key, defaultValue);
}

int SceLsdb_19E16A56(AppInfoSClient *client, int key, const char *value)
{
	return client->m_impl->SetConfigString(key, value);
}

int SceLsdb_B332121A(AppInfoSClient *client, int key, SceInt64 bits, SceInt64 mask)
{
	return client->m_impl->UpdateConfigBits(key, bits, mask);
}

int SceLsdb_6F73E332(AppInfoSClient *client, const paf::common::SharedPtr<LsdbThemeInfo> *theme,
	unsigned int flags)
{
	return client->m_impl->SetPageTheme(theme, flags);
}

int SceLsdb_01790EDE(AppInfoSClient *client, const LsdbTheme *theme, const LsdbThemeText *title,
	const LsdbThemeText *provider)
{
	return client->m_impl->SetTheme(theme, title, provider);
}

int SceLsdb_54F188A5(AppInfoSClient *client, const paf::string *id)
{
	return client->m_impl->DeleteTheme(id);
}

int SceLsdb_C5268ECB(AppInfoSClient *client)
{
	return client->m_impl->DeleteAllThemes();
}

int SceLsdb_637814FE(AppInfoSClient *client)
{
	return client->m_impl->GetThemeCount();
}

int SceLsdb_C525B99E(AppInfoSClient *client)
{
	return client->m_impl->BeginTransaction();
}

int SceLsdb_F406822E(AppInfoSClient *client, int state)
{
	return client->m_impl->EndTransaction(state);
}

int SceLsdb_A6CEEAB3(AppInfoSClient *client, LsdbAppInfoVisitor visitor, void *arg)
{
	return client->m_impl->ForEachAppInfo(visitor, arg);
}

}
