/*
 * The client's implementation: every request is the daemon's.
 */
#include "livespace_db_internal.h"

int AppInfoSClientImpl::GetIcons(int pageNo, unsigned int count, LsdbPageIconLists *pages)
{
	return AppInfoDaemon::GetInstance()->GetIcons(pageNo, count, pages);
}

int AppInfoSClientImpl::GetPage(int pageNo, unsigned int flags, LsdbPageList *pages)
{
	return AppInfoDaemon::GetInstance()->GetPage(pageNo, flags, pages);
}

int AppInfoSClientImpl::ReplacePageIcons(paf::vector<int> *pageNos, LsdbPageIconLists *lists)
{
	return AppInfoDaemon::GetInstance()->ReplacePageIcons(pageNos, lists);
}

int AppInfoSClientImpl::UpdateIcon(int pageNo, const LsdbIconPtr *icon, unsigned int fields,
	const void *image, int imageSize)
{
	return AppInfoDaemon::GetInstance()->UpdateIcon(pageNo, icon, fields, image, imageSize);
}

int AppInfoSClientImpl::AddIcon(int pageNo, const LsdbIconPtr *icon)
{
	return AppInfoDaemon::GetInstance()->AddIcon(pageNo, icon);
}

int AppInfoSClientImpl::DeleteTitleIcons(const paf::string *titleId)
{
	return AppInfoDaemon::GetInstance()->DeleteTitleIcons(titleId);
}

int AppInfoSClientImpl::MoveIcons(paf::vector<int> *fromPages,
	paf::vector<int> *fromPositions, paf::vector<int> *toPages, paf::vector<int> *toPositions)
{
	return AppInfoDaemon::GetInstance()->MoveIcons(fromPages, fromPositions, toPages, toPositions);
}

int AppInfoSClientImpl::GetIconImage(const SceInt64 *rowid, paf::common::SharedPtr<LsdbDbBlob> *image)
{
	return AppInfoDaemon::GetInstance()->GetIconImage(rowid, image);
}

int AppInfoSClientImpl::UpdatePage(paf::vector<int> *pageNos, LsdbPageList *pages)
{
	return AppInfoDaemon::GetInstance()->UpdatePage(pageNos, pages);
}

int AppInfoSClientImpl::AddPage(int pageNo, LsdbIconList *icons, LsdbPage *page)
{
	return AppInfoDaemon::GetInstance()->AddPage(pageNo, icons, page);
}

int AppInfoSClientImpl::DeletePage(int pageNo)
{
	return AppInfoDaemon::GetInstance()->DeletePage(pageNo);
}

int AppInfoSClientImpl::CreateFolder(int pageNo, const LsdbIconPtr *icon,
	const LsdbIconPtr *folderIcon, const void *image, int imageSize)
{
	return AppInfoDaemon::GetInstance()->CreateFolder(pageNo, icon, folderIcon, image, imageSize);
}

int AppInfoSClientImpl::GetPageCount()
{
	return AppInfoDaemon::GetInstance()->GetPageCount();
}

int AppInfoSClientImpl::CountIcons(int pageNo, unsigned int pageCount)
{
	return AppInfoDaemon::GetInstance()->CountIcons(pageNo, pageCount);
}

int AppInfoSClientImpl::CountUserIcons()
{
	return AppInfoDaemon::GetInstance()->CountUserIcons();
}

int AppInfoSClientImpl::FindFolders(const paf::string *title, paf::vector<int> *folderIds)
{
	return AppInfoDaemon::GetInstance()->FindFolders(title, folderIds);
}

int AppInfoSClientImpl::GetAppInfo(const paf::string *titleId, LsdbAppInfoPtr *info)
{
	return AppInfoDaemon::GetInstance()->GetAppInfo(titleId, info);
}

int AppInfoSClientImpl::GetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value)
{
	return AppInfoDaemon::GetInstance()->GetAppInfoValue(titleId, key, value);
}

int AppInfoSClientImpl::HasAppInfo(const paf::string *titleId)
{
	return AppInfoDaemon::GetInstance()->HasAppInfo(titleId);
}

int AppInfoSClientImpl::SetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value,
	bool replace)
{
	return AppInfoDaemon::GetInstance()->SetAppInfoValue(titleId, key, value, replace);
}

int AppInfoSClientImpl::GetFolderIcon(const int *folderId, int *pageNo, const LsdbIconPtr *icon)
{
	return AppInfoDaemon::GetInstance()->GetFolderIcon(folderId, pageNo, icon);
}

int AppInfoSClientImpl::RegisterApp(LsdbAppInfoPtr *app, int pageNo, const LsdbIconPtr *icon,
	paf::vector<LsdbAppInfoPtr> *related)
{
	return AppInfoDaemon::GetInstance()->RegisterApp(app, pageNo, icon, related);
}

int AppInfoSClientImpl::UnregisterApp(const paf::string *titleId, bool removeFolder, int *pageNo)
{
	return AppInfoDaemon::GetInstance()->UnregisterApp(titleId, removeFolder, pageNo);
}

int AppInfoSClientImpl::UpdateApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon,
	paf::vector<LsdbAppInfoPtr> *related)
{
	return AppInfoDaemon::GetInstance()->UpdateApp(app, icon, related);
}

int AppInfoSClientImpl::RefreshApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon,
	unsigned int fields, paf::vector<LsdbAppInfoPtr> *related)
{
	return AppInfoDaemon::GetInstance()->RefreshApp(app, icon, fields, related, true);
}

int AppInfoSClientImpl::UpdateAppInfoBits(const paf::string *titleId, unsigned int key, const LsdbValue *bits, SceInt64 mask)
{
	return AppInfoDaemon::GetInstance()->UpdateAppInfoBits(titleId, key, bits, mask, true);
}

int AppInfoSClientImpl::ForEachAppInfo(LsdbAppInfoVisitor visitor, void *arg)
{
	return AppInfoDaemon::GetInstance()->ForEachAppInfo(visitor, arg);
}

int AppInfoSClientImpl::BeginTransaction()
{
	int result = LSDB_ERROR_NOT_READY;

	if (AppInfoDaemon::GetInstance()->BeginTransaction() != NULL)
		result = 0;
	return result;
}

int AppInfoSClientImpl::EndTransaction(int state)
{
	return AppInfoDaemon::GetInstance()->EndTransaction(state);
}

int AppInfoSClientImpl::FindTitles(LsdbQueryCondition *conditions,
	paf::vector<paf::string> *titleIds, unsigned int flags)
{
	return AppInfoDaemon::GetInstance()->FindTitles(conditions, titleIds, flags);
}

int AppInfoSClientImpl::GetTitlesByCategory(const char **categories, unsigned int count,
	int limit, paf::vector<paf::string> *titleIds)
{
	return AppInfoDaemon::GetInstance()->GetTitlesByCategory(categories, count, limit,
		titleIds);
}

int AppInfoSClientImpl::GetUriTitles(const char *scheme, int arg2, int arg3)
{
	return AppInfoDaemon::GetInstance()->GetUriTitles(scheme, arg2, arg3);
}

int AppInfoSClientImpl::GetUriHostPath(const char *scheme, unsigned int arg2, void *result)
{
	return AppInfoDaemon::GetInstance()->GetUriHostPath(scheme, arg2, result);
}

paf::string AppInfoSClientImpl::GetConfigString(int key, const char *defaultValue)
{
	return AppInfoDaemon::GetInstance()->GetConfigString(key, defaultValue);
}

SceInt64 AppInfoSClientImpl::GetConfigInt64(int key, SceInt64 defaultValue)
{
	return AppInfoDaemon::GetInstance()->GetConfigInt64(key, defaultValue);
}

int AppInfoSClientImpl::SetConfigString(int key, const char *value)
{
	return AppInfoDaemon::GetInstance()->SetConfigString(key, value);
}

int AppInfoSClientImpl::UpdateConfigBits(int key, SceInt64 bits, SceInt64 mask)
{
	return AppInfoDaemon::GetInstance()->UpdateConfigBits(key, bits, mask);
}

int AppInfoSClientImpl::SetPageTheme(const paf::common::SharedPtr<LsdbThemeInfo> *theme,
	unsigned int flags)
{
	return AppInfoDaemon::GetInstance()->SetPageTheme(theme, flags);
}

int AppInfoSClientImpl::SetTheme(const LsdbTheme *theme, const LsdbThemeText *title,
	const LsdbThemeText *provider)
{
	return AppInfoDaemon::GetInstance()->SetTheme(theme, title, provider);
}

int AppInfoSClientImpl::DeleteTheme(const paf::string *id)
{
	return AppInfoDaemon::GetInstance()->DeleteTheme(id);
}

int AppInfoSClientImpl::DeleteAllThemes()
{
	return AppInfoDaemon::GetInstance()->DeleteAllThemes();
}

int AppInfoSClientImpl::GetThemeCount()
{
	return AppInfoDaemon::GetInstance()->GetThemeCount();
}
