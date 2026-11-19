/*
 * The application information daemon: one per process. Each request takes the activity
 * database's connection for one transaction, runs its query and ends the transaction with the
 * query's result.
 */
#include <std/string.h>
#include <datetime/datetime.h>
#include "livespace_db_internal.h"

/* a copy of a vector of ints, out of line where Sony's build calls it (MoveIcons) */
namespace paf {
template<> inline __attribute__((noinline))
vector<int, allocator<int> >::vector(const vector<int, allocator<int> > &src) : alloc(src.alloc)
{
	count = 0;
	limit = 0;

	int *data = alloc.allocate(src.limit);

	if (data == NULL)
		alloc.report_failure();
	ptr = data;
	if (data != NULL) {
		count = src.count;
		limit = src.limit;
		for (size_type i = 0; i < count; ++i)
			new (&ptr[i]) int(src.ptr[i]);
	}
}

/* CreateFolder's one-page list, which Sony's build makes out of line */
template<> inline __attribute__((noinline))
vector<common::SharedPtr<LsdbPage>, allocator<common::SharedPtr<LsdbPage> > >::vector(
	size_type size, const common::SharedPtr<LsdbPage> &val,
	const allocator<common::SharedPtr<LsdbPage> > &al) : alloc(al)
{
	count = 0;
	limit = 0;

	common::SharedPtr<LsdbPage> *buffer = alloc.allocate(size);

	if (buffer == NULL)
		alloc.report_failure();
	ptr = buffer;
	if (buffer != NULL) {
		count = size;
		limit = size;
		for (size_type i = 0; i < count; ++i)
			new (&ptr[i]) common::SharedPtr<LsdbPage>(val);
	}
}
}

/* the result of opening the database, until a rebuild clears it */
static int s_openResult = 0;
const char *g_lsdbAppDbPath = "ur0:shell/db/app.db";

/* the schema version the database is rebuilt for */
#define LSDB_APPDB_VERSION	7

/* the key of an application's install path ("ux0:..." when it is on the memory card) */
#define LSDB_KEY_PATH		0x9A9B3542u

static int AppInfoDaemonTransaction(int op, bool readOnly, void *arg,
	ActivityDbConnection **connection);

__attribute__((noinline)) AppInfoDaemon::AppInfoDaemon()
	: m_access(NULL), m_mutex("SceLsdbAppInfoDaemon", 0)
{
}

AppInfoDaemon *AppInfoDaemon::GetInstance()
{
	static AppInfoDaemon s_instance;

	return &s_instance;
}

/* brings the tables up from version: a new database also loses the old icon layout */
int AppInfoDaemon::Upgrade(ActivityDbConnection *connection, int version)
{
	if (version == 0)
		SceLsdb_06CD70C5();
	return AppDbCreateTables(connection, version);
}

int AppInfoDaemon::Open(ActivityDbAccess *access, int result)
{
	m_access = access;
	if (result != (int)0x801010F1 && result != (int)0x801010F4 && result != (int)0x801010FE &&
	    result != (int)0x801010FF && result != (int)0x801010F2) {
		int rebuilt = LsdbGetAppDbRebuildFlag();

		if (rebuilt < 0) {
		} else if (rebuilt == 0) {
			result = LSDB_ERROR_REBUILD;
		}
	}
	if (s_openResult == 0)
		s_openResult = result;
	return 0;
}

int AppInfoDaemon::Close()
{
	m_listeners.clear();
	m_access = NULL;
	return 0;
}

int AppInfoDaemon::AddListener(LsdbAppInfoListener listener, void *userData)
{
	m_mutex.Lock();
	if (m_access == NULL) {
		m_mutex.Unlock();
		return LSDB_ERROR_NOT_READY;
	}
	m_listeners.insert(paf::pair<unsigned int, LsdbAppInfoListener>((unsigned int)userData,
		listener));
	m_mutex.Unlock();
	return 0;
}

void AppInfoDaemon::RemoveListener(void *userData)
{
	m_mutex.Lock();
	m_listeners.erase((unsigned int)userData);
	m_mutex.Unlock();
}

int AppInfoDaemon::GetOpenResult()
{
	return s_openResult;
}

paf::string AppInfoDaemon::GetConfigString(int key, const char *defaultValue)
{
	paf::string value;

	if (m_access == NULL)
		return paf::string(defaultValue);
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return paf::string(defaultValue);
	value = AppDbGetConfigString(connection, key, defaultValue);
	m_access->EndTransaction(0);
	return paf::string(value);
}

int AppInfoDaemon::SetConfigString(int key, const char *value)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbSetConfigString(connection, key, value);
	m_access->EndTransaction(result);
	return result;
}

SceInt64 AppInfoDaemon::GetConfigInt64(int key, SceInt64 defaultValue)
{
	if (m_access == NULL)
		return defaultValue;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return defaultValue;
	SceInt64 value = AppDbGetConfigInt64(connection, key, defaultValue);
	m_access->EndTransaction(0);
	return value;
}

int AppInfoDaemon::UpdateConfigBits(int key, SceInt64 bits, SceInt64 mask)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbUpdateConfigBits(connection, key, bits, mask);
	m_access->EndTransaction(result);
	return result;
}

ActivityDbConnection *AppInfoDaemon::BeginTransaction()
{
	ActivityDbConnection *connection = NULL;

	if (m_access != NULL)
		connection = m_access->BeginTransaction();
	return connection;
}

/* ends the transaction of a rebuild: state < 0 rolls it back */
int AppInfoDaemon::EndTransaction(int state)
{
	int result;

	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	if (state >= 0) {
		ActivityDbRebuildParam param;

		param.state = state;
		param.version = LSDB_APPDB_VERSION;
		param.language = SceLsdb_68059E87();
		param.region = LsdbGetRegion();
		result = m_access->SetParam(&param);
		if (result != 0) {
			m_access->EndTransaction(-1);
			return result;
		}
	}
	result = m_access->EndTransaction(state >= 0 ? 0 : -1);
	if (result == 0 && state >= 0) {
		s_openResult = 0;
		SceActivityDb_B9A3FDCC(g_lsdbAppDbPath);
		LsdbSetAppDbRebuildFlag(1);
	}
	return result;
}

int AppInfoDaemon::GetIcons(int pageNo, unsigned int count, LsdbPageIconLists *pages)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetIcons(connection, pageNo, count, pages);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::GetPage(int pageNo, unsigned int flags, LsdbPageList *pages)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetPage(connection, pageNo, flags, pages);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::ReplacePageIcons(paf::vector<int> *pageNos, LsdbPageIconLists *lists)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbReplacePageIcons(connection, pageNos, lists);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::UpdateIcon(int pageNo, const LsdbIconPtr *icon, unsigned int fields,
	const void *image, int imageSize)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	if (fields == 0)
		return LSDB_ERROR_INVALID_ARGUMENT;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbUpdateIcon(connection, pageNo, icon, fields, image, imageSize);
	if (result == 0 && (fields & 1) != 0) {
		LsdbIcon current;

		if (AppDbGetIconAt(connection, pageNo, (*icon)->pos, &current) == 0 &&
		    current.type == LSDB_ICON_TYPE_FOLDER)
			SceLsdb_6E4B6EBF(current.folder->id, &pageNo, &current.pos,
				current.title.c_str());
	}
	m_access->EndTransaction(result);
	return result;
}

/* whether path is on the memory card */
static inline bool AppInfoDaemonIsMemoryCardPath(const char *path)
{
	return sce_paf_memcmp(path, "ux0:", 4) == 0 ? true : false;
}

/* whether the application titleId is installed on the memory card */
static bool AppInfoDaemonIsOnMemoryCard(ActivityDbConnection *connection,
	const paf::string *titleId)
{
	bool onMemoryCard = false;
	LsdbValue path;

	if (AppDbGetAppInfoValue(connection, titleId, LSDB_KEY_PATH, &path) == 0) {
		if (AppInfoDaemonIsMemoryCardPath(LsdbValueText(path)))
			onMemoryCard = true;
	}
	return onMemoryCard;
}

/* where the icon at pageNo, pos is, with the folder that holds it */
static __attribute__((noinline)) LsdbIconLocation AppInfoDaemonGetIconLocation(
	ActivityDbConnection *connection, int pageNo,
	int pos)
{
	int folderPageNo = 0;
	LsdbIcon folder;

	if (LSDB_IS_FOLDER_ID(pageNo))
		AppDbGetFolderIcon(connection, pageNo, &folderPageNo, &folder);
	LsdbIconLocation location = { pageNo, pos, folderPageNo, folder.pos,
		paf::string(folder.title.c_str()) };
	return location;
}

int AppInfoDaemon::AddIcon(int pageNo, const LsdbIconPtr *icon)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbAddIcon(connection, pageNo, icon, NULL, 0);
	if (result == 0 && pageNo != -1 &&
	    AppInfoDaemonIsOnMemoryCard(connection, &(*icon)->titleId)) {
		const char *titleId = (*icon)->titleId.c_str();
		LsdbIconLocation location = AppInfoDaemonGetIconLocation(connection, pageNo,
			(*icon)->pos);

		SceLsdb_2AB5A403(titleId, &location);
	}
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::DeleteTitleIcons(const paf::string *titleId)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbDeleteTitleIcons(connection, titleId);
	if (result == 0 && AppInfoDaemonIsOnMemoryCard(connection, titleId))
		SceLsdb_2DF6EA05(titleId->c_str());
	m_access->EndTransaction(result);
	return result;
}

/*
 * Moves icons: a folder left without its icons goes, then every moved icon on the memory card
 * and every moved folder is written to the icon layout.
 */
int AppInfoDaemon::MoveIcons(paf::vector<int> *fromPages, paf::vector<int> *fromPositions,
	paf::vector<int> *toPages, paf::vector<int> *toPositions)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbMoveIcons(connection, fromPages, fromPositions, toPages, toPositions);
	if (result != 0)
		goto end;
	for (paf::vector<int>::iterator page = fromPages->begin(); page != fromPages->end(); ++page) {
		bool emptied = false;

		if (LSDB_IS_FOLDER_ID(*page)) {
			paf::vector<int> targets(*toPages);
			paf::vector<int>::iterator target;

			for (target = targets.begin(); target != targets.end(); ++target) {
				if (*target == *page)
					break;
			}
			if (targets.end() == target)
				emptied = true;
		}
		if (emptied) {
			result = AppDbDeletePage(connection, *page);
			if (result == 0)
				continue;
			if (result != (int)LSDB_ERROR_NOT_FOUND && result != (int)0x80101004)
				goto end;
			result = 0;
		}
	}
	SceLsdb_30900530();
	for (paf::vector<int>::iterator page = toPages->begin(), pos = toPositions->begin();
	     page != toPages->end() && pos != toPositions->end(); ++page, ++pos) {
		LsdbIcon icon;
		int pageNo = *page;
		int position = *pos;

		if (AppDbIsNoPosition(pageNo, position))
			continue;
		if (AppDbGetIconAt(connection, pageNo, position, &icon) == 0 &&
		    icon.titleId.length() != 0 &&
		    AppInfoDaemonIsOnMemoryCard(connection, &icon.titleId)) {
			const char *titleId = icon.titleId.c_str();
			LsdbIconLocation location = AppInfoDaemonGetIconLocation(connection, pageNo,
				position);

			SceLsdb_2AB5A403(titleId, &location);
		} else if (icon.type == LSDB_ICON_TYPE_FOLDER) {
			SceLsdb_6E4B6EBF(icon.folder->id, &pageNo, &position, icon.title.c_str());
		}
	}
	SceLsdb_E05F71CB();
end:
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::GetIconImage(const SceInt64 *rowid, paf::common::SharedPtr<DbBlob> *image)
{
	int result = 0;

	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	*image = DbBlobOpen("main", "tbl_appinfo_icon", "reserved05", *rowid, 0,
		AppInfoDaemonTransaction, this, &result);
	return result;
}

int AppInfoDaemon::UpdatePage(paf::vector<int> *pageNos, LsdbPageList *pages)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbUpdatePage(connection, pageNos, pages);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::AddPage(int pageNo, LsdbIconList *icons, LsdbPage *page)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbAddPage(connection, pageNo, icons, page, 0);
	if (result == 0)
		SceLsdb_A92420DA(1, pageNo);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::DeletePage(int pageNo)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbDeletePage(connection, pageNo);
	if (result == 0)
		SceLsdb_A92420DA(0, pageNo);
	m_access->EndTransaction(result);
	return result;
}

/*
 * Makes a folder at pageNo: folderIcon gets a new folder (a page of its own, numbered from the
 * clock until one is free), and icon moves into it.
 */
int AppInfoDaemon::CreateFolder(int pageNo, const LsdbIconPtr *icon,
	const LsdbIconPtr *folderIcon, const void *image, int imageSize)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	if (pageNo < 0)
		return LSDB_ERROR_INVALID_ARGUMENT;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	bool onMemoryCard = false;

	if ((*icon)->type == 0)
		onMemoryCard = (*icon)->titleId.length() != 0;
	(*folderIcon)->folder = paf::common::SharedPtr<LsdbFolder>(new LsdbFolder);

	LsdbFolder *folder = (*folderIcon)->folder.get();

	folder->iconCount++;
	if (onMemoryCard)
		folder->titleIds.push_back((*icon)->titleId);

	int result;
	LsdbFolder *newFolder = (*folderIcon)->folder.get();
	{
		LsdbPageList pages(1, paf::common::SharedPtr<LsdbPage>(new LsdbPage));

		for (;;) {
			paf::rtc::Tick tick = 0;

			paf::rtc::GetCurrentTick(&tick);
			newFolder->id = ((unsigned int)tick & 0x7FFFFFFF) % 10000000 + LSDB_FOLDER_ID_MIN;
			result = AppDbGetPage(connection, newFolder->id, 1, &pages);
			if (result == (int)LSDB_ERROR_NOT_FOUND) {
				result = 0;
				break;
			}
			if (result != 0)
				break;
		}
	}
	if (result != 0)
		goto end;
	{
		LsdbIconList icons;
		LsdbPage page;

		result = AppDbAddPage(connection, (*folderIcon)->folder->id, &icons, &page, 1);
	}
	if (result != 0)
		goto end;
	{
		paf::vector<int> fromPages(1, pageNo);
		paf::vector<int> fromPositions(1, (*icon)->pos);
		paf::vector<int> toPages(1, (*folderIcon)->folder->id);
		paf::vector<int> toPositions(1, (*folderIcon)->pos);

		result = AppDbMoveIcons(connection, &fromPages, &fromPositions, &toPages, &toPositions);
	}
	if (result != 0)
		goto end;
	result = AppDbAddIcon(connection, pageNo, folderIcon, image, imageSize);
	if (result == 0 && onMemoryCard &&
	    AppInfoDaemonIsOnMemoryCard(connection, &(*icon)->titleId)) {
		const char *titleId = (*icon)->titleId.c_str();
		LsdbIconLocation location = { (*folderIcon)->folder->id, (*icon)->pos, pageNo,
			(*folderIcon)->pos, paf::string((*folderIcon)->title.c_str()) };

		SceLsdb_2AB5A403(titleId, &location);
	}
end:
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::GetPageCount()
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	int count = 0;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetPageCount(connection, &count);
	m_access->EndTransaction(result);
	if (result == 0)
		result = count;
	return result;
}

int AppInfoDaemon::CountIcons(int pageNo, unsigned int pageCount)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	int count = 0;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbCountIcons(connection, pageNo, pageCount, &count);
	m_access->EndTransaction(result);
	if (result == 0)
		result = count;
	return result;
}

int AppInfoDaemon::CountUserIcons()
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	int count = 0;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbCountUserIcons(connection, &count);
	m_access->EndTransaction(result);
	if (result == 0)
		result = count;
	return result;
}

int AppInfoDaemon::FindFolders(const paf::string *title, paf::vector<int> *folderIds)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbFindFolders(connection, title, folderIds);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::GetAppInfo(const paf::string *titleId, LsdbAppInfoPtr *info)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetAppInfo(connection, titleId, info);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::GetAppInfoValue(const paf::string *titleId, unsigned int key, LsdbValue *value)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetAppInfoValue(connection, titleId, key, value);
	m_access->EndTransaction(result);
	return result;
}

/* only reads: the transaction is rolled back */
int AppInfoDaemon::HasAppInfo(const paf::string *titleId)
{
	if (m_access == NULL)
		return 0;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return 0;
	int result = AppDbHasAppInfo(connection, titleId);
	m_access->EndTransaction(-1);
	return result;
}

int AppInfoDaemon::FindTitleIcon(const paf::string *titleId, LsdbIconPtr *icon, int *pageNo,
	bool theme)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbFindTitleIcon(connection, titleId, icon, pageNo, theme);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::GetFolderIcon(const int *folderId, int *pageNo, const LsdbIconPtr *icon)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetFolderIcon(connection, *folderId, pageNo, icon->get());
	m_access->EndTransaction(result);
	return result;
}

/* tells every listener of an application's change: event 0 registered, 1 unregistered */
void AppInfoDaemon::NotifyListeners(int event, LsdbAppInfoPtr *info)
{
	m_mutex.Lock();
	for (paf::map<unsigned int, LsdbAppInfoListener>::iterator listener = m_listeners.begin();
	     listener != m_listeners.end(); ++listener)
		listener->second(event, info, listener->first);
	m_mutex.Unlock();
}

int AppInfoDaemon::RegisterApp(LsdbAppInfoPtr *app, int pageNo, const LsdbIconPtr *icon,
	paf::vector<LsdbAppInfoPtr> *related)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbAddIcon(connection, pageNo, icon, NULL, 0);
	if (result != 0)
		goto end;
	result = AppDbAddAppInfo(connection, app, false, -1);
	if (result != 0)
		goto end;
	for (paf::vector<LsdbAppInfoPtr>::iterator other = related->begin(); other != related->end();
	     ++other) {
		result = AppDbAddAppInfo(connection, &*other, false, -1);
		if (result != 0)
			goto end;
	}
	result = AppDbAddUris(connection, app);
	if (result != 0)
		goto end;
	if (pageNo != -1) {
		LsdbAppInfo::iterator path = (*app)->find(LSDB_KEY_ORG_PATH);

		if (path != (*app)->end()) {
			if (sce_paf_memcmp(LsdbValueText(path->second), "ux0:", 4) == 0) {
				if (pageNo == LSDB_PAGE_NO_FOLDERS) {
					SceLsdb_2DF6EA05((*icon)->titleId.c_str());
				} else {
					const char *titleId = (*icon)->titleId.c_str();
					LsdbIconLocation location = AppInfoDaemonGetIconLocation(
						connection, pageNo, (*icon)->pos);

					SceLsdb_2AB5A403(titleId, &location);
				}
			}
		}
	}
	NotifyListeners(0, app);
end:
	m_access->EndTransaction(result);
	return result;
}

/*
 * Removes an application: its theme, parameters, URIs and icons, and with removeFolder the
 * folder it was the last icon of. *pageNo gets the page its icon was on.
 */
int AppInfoDaemon::UnregisterApp(const paf::string *titleId, bool removeFolder, int *pageNo)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	int iconPageNo = -1;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	if (removeFolder || pageNo != NULL) {
		bool inFolder = false;
		LsdbIconPtr icon(new LsdbIcon);

		if (AppDbGetTitleIconOnly(connection, titleId, &icon, &iconPageNo, 0) != 0) {
			removeFolder = false;
		} else {
			if (removeFolder && LSDB_IS_FOLDER_ID(iconPageNo))
				inFolder = true;
			removeFolder = inFolder;
		}
	}

	bool onMemoryCard = AppInfoDaemonIsOnMemoryCard(connection, titleId);
	int result;

	AppDbDeleteBgAppInfo(connection, titleId);
	result = AppDbDeleteAppInfo(connection, titleId);
	if (result != 0)
		goto end;
	result = AppDbDeleteUris(connection, titleId);
	if (result != 0)
		goto end;
	result = AppDbDeleteTitleIcons(connection, titleId);
	if (result == (int)LSDB_ERROR_NOT_FOUND) {
		result = 0;
	} else {
		if (result != 0)
			goto end;
		if (pageNo != NULL)
			*pageNo = iconPageNo;
	}
	if (removeFolder) {
		result = AppDbDeletePage(connection, iconPageNo);
		if (result != 0) {
			if (result != (int)0x80101004)
				goto end;
			result = 0;
		}
	}
	if (onMemoryCard)
		SceLsdb_2DF6EA05(titleId->c_str());
	{
		LsdbAppInfoPtr info(new LsdbAppInfo);

		if (info.get() == NULL) {
			result = LSDB_ERROR_NO_MEMORY;
			goto end;
		}
		paf::string *id = new paf::string(*titleId);

		info->insert(paf::pair<unsigned int, LsdbValue>(LSDB_KEY_TITLE_ID, LsdbValue(id)));
		delete id;
		NotifyListeners(1, &info);
	}
end:
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::UpdateApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon,
	paf::vector<LsdbAppInfoPtr> *related)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;
	LsdbAppInfo::iterator id = (*app)->find(LSDB_KEY_TITLE_ID);

	if (id == (*app)->end()) {
		result = LSDB_ERROR_NO_TITLE_ID;
		goto end;
	}
	{
		const char *titleId = LsdbValueText(id->second);
		{
			paf::string theme(titleId);

			AppDbDeleteBgAppInfo(connection, &theme);
		}
		for (paf::vector<LsdbAppInfoPtr>::iterator other = related->begin();
		     other != related->end(); ++other) {
			result = AppDbAddAppInfo(connection, &*other, false, -1);
			if (result != 0)
				goto end;
		}
		result = AppDbReplaceAppInfo(connection, app);
		if (result != 0)
			goto end;
		if (icon->get() != NULL) {
			if ((*icon)->titleId.length() == 0) {
				result = LSDB_ERROR_NO_TITLE_ID;
				goto end;
			}
			{
				paf::string iconTitleId(titleId);

				result = AppDbUpdateTitleIcons(connection, &iconTitleId, icon,
					LSDB_ICON_FIELD_ALL);
			}
			if (result != 0)
				goto end;
		}
		result = AppDbUpdateUris(connection, app);
		if (result == 0)
			NotifyListeners(2, app);
	}
end:
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::RefreshApp(LsdbAppInfoPtr *app, const LsdbIconPtr *icon, unsigned int fields,
	paf::vector<LsdbAppInfoPtr> *related, bool notify)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbSetAppInfo(connection, app, true);
	if (result != 0)
		goto end;
	for (paf::vector<LsdbAppInfoPtr>::iterator other = related->begin(); other != related->end();
	     ++other) {
		result = AppDbSetAppInfo(connection, &*other, false);
		if (result != 0)
			goto end;
	}
	if (fields != 0) {
		if (icon->get() == NULL) {
			result = LSDB_ERROR_INVALID_ARGUMENT;
			goto end;
		}
		LsdbAppInfo::iterator id = (*app)->find(LSDB_KEY_TITLE_ID);

		if (id == (*app)->end()) {
			result = LSDB_ERROR_NO_TITLE_ID;
			goto end;
		}
		paf::string titleId(LsdbValueText(id->second));

		result = AppDbUpdateTitleIcons(connection, &titleId, icon, fields);
		if (result != 0)
			goto end;
	}
	result = AppDbRefreshUris(connection, app);
	if (result == 0 && notify)
		NotifyListeners(3, app);
end:
	m_access->EndTransaction(result);
	return result;
}

/*
 * Updates a parameter's bits: (value & ~mask) | (bits & mask). A parental level also goes to the
 * application's icons; with notify the listeners get the application's title ID and the value.
 */
int AppInfoDaemon::UpdateAppInfoBits(const paf::string *titleId, unsigned int key,
	const LsdbValue *bits, SceInt64 mask, bool notify)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbUpdateAppInfoBits(connection, titleId, key, bits, mask);
	if (result == 0) {
		if (key == LSDB_KEY_PARENTAL_LEVEL) {
			LsdbIconPtr icon(new LsdbIcon);

			icon->parentalLockLv = bits->type == LSDB_VALUE_INTEGER ? (int)bits->integer : 0;
			AppDbUpdateTitleIcons(connection, titleId, &icon, 16);
		}
		if (notify) {
			LsdbAppInfoPtr info(new LsdbAppInfo);

			/* leaves the transaction open */
			if (info.get() == NULL)
				return LSDB_ERROR_NO_MEMORY;
			paf::string *id = new paf::string(*titleId);

			info->insert(paf::pair<unsigned int, LsdbValue>(LSDB_KEY_TITLE_ID,
				LsdbValue(id)));
			delete id;
			info->insert(paf::pair<unsigned int, LsdbValue>(key, *bits));
			NotifyListeners(3, &info);
		}
	}
	m_access->EndTransaction(result);
	return result;
}

/* Calls visitor with each application's information. */
int AppInfoDaemon::ForEachAppInfo(LsdbAppInfoVisitor visitor, void *arg)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	if (visitor == NULL)
		return LSDB_ERROR_INVALID_ARGUMENT;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbForEachAppInfo(connection, visitor, arg);
	m_access->EndTransaction(result);
	return result;
}

/* Deletes the icons left by applications whose information is gone. */
int AppInfoDaemon::UpdateIconTitles(int language)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbUpdateIconTitles(connection, SceLsdb_63C29E70(language));
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::FindTitles(LsdbQueryCondition *conditions, paf::vector<paf::string> *titleIds,
	unsigned int flags)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int ret = AppDbFindTitles(connection, conditions, titleIds, flags);
	m_access->EndTransaction(ret);
	return ret;
}

/* the one request that does not check for the database first */
int AppInfoDaemon::GetTitlesByCategory(const char **categories, unsigned int count, int limit,
	paf::vector<paf::string> *titleIds)
{
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int ret = AppDbGetTitlesByCategory(connection, categories, count, limit, titleIds);
	m_access->EndTransaction(ret);
	return ret;
}

int AppInfoDaemon::GetUriTitle(const char *uri, paf::string *titleId, unsigned int flags)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetUriTitle(connection, uri, flags, titleId);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::GetSchemeUris(const char **schemes, unsigned int count,
	paf::vector<paf::string> *uris)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int ret = AppDbGetSchemeUris(connection, schemes, count, uris);
	m_access->EndTransaction(ret);
	return ret;
}

/* a file of the theme: its path in the theme's folder, or nothing for no file */
static __attribute__((noinline)) paf::string LsdbThemeInfoGetFilePath(const LsdbThemeInfo *theme,
	const paf::string *name)
{
	if (name->size() == 0)
		return paf::string();
	return theme->path + *name;
}

/*
 * Applies an installed theme: with LSDB_SET_PAGE_THEME_ICONS the system applications' icons
 * (and the power button's) take the theme's images, with LSDB_SET_PAGE_THEME_PAGES each page
 * its background.
 */
int AppInfoDaemon::SetPageTheme(const paf::common::SharedPtr<LsdbThemeInfo> *theme,
	unsigned int flags)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = 0;

	if (flags & LSDB_SET_PAGE_THEME_ICONS) {
		unsigned int count = 0;
		const char *const *titleIds = LsdbThemeGetSystemTitleIds(&count);

		for (unsigned int i = 0; i < count; i++) {
			const char *titleId = titleIds[i];
			const IconParam *param = (*theme)->m_homeProperty.GetIconParam(titleId, 0);

			if (param == NULL)
				continue;
			LsdbIconPtr icon(new LsdbIcon);

			icon->iconPath = LsdbThemeInfoGetFilePath(theme->get(), &param->m_iconFilePath);
			result = AppDbUpdateTitleIcons(connection, &paf::string(titleId), &icon,
				LSDB_ICON_FIELD_THEME_ICON_PATH);
			if (result != 0) {
				if (result != LSDB_ERROR_NOT_FOUND)
					goto end;
				result = 0;
			}
		}
		const IconParam *param = (*theme)->m_homeProperty.GetIconParam("", 6);

		if (param != NULL) {
			LsdbIconPtr icon(new LsdbIcon);

			icon->iconPath = LsdbThemeInfoGetFilePath(theme->get(), &param->m_iconFilePath);
			result = AppDbUpdateTypeIcons(connection, 6, &icon, LSDB_ICON_FIELD_THEME_ICON_PATH);
			if (result != 0) {
				if (result != LSDB_ERROR_NOT_FOUND)
					goto end;
				result = 0;
			}
		}
	}
	if (flags & LSDB_SET_PAGE_THEME_PAGES) {
		int count = 0;

		result = AppDbGetPageCount(connection, &count);
		if (result == 0) {
			paf::vector<int> pageNos;
			LsdbPageList pages;

			pageNos.resize(count, 0);
			pages.resize(count, paf::common::SharedPtr<LsdbPage>());

			for (int i = 0; i < count; i++) {
				pageNos[i] = i;
				pages[i] = paf::common::SharedPtr<LsdbPage>(new LsdbPage);

				const BackgroundParam *bg = &(*theme)->m_homeProperty.m_bgParam[i];

				pages[i]->bgColor = bg->m_waveType;
				pages[i]->reserved01 = bg->GetFontColor();
				if (bg->m_imageFilePath.size() != 0) {
					pages[i]->themeFile = LsdbThemeInfoGetFilePath(theme->get(),
						&bg->m_imageFilePath);
					pages[i]->texWidth = 960;
					pages[i]->texHeight = 512;
					pages[i]->imageWidth = 960;
					pages[i]->imageHeight = 512;
				}
			}
			result = AppDbUpdatePage(connection, &pageNos, &pages);
		}
	}
end:
	m_access->EndTransaction(result);
	return result;
}

/* Records a theme with its title and provider in each language. */
int AppInfoDaemon::SetTheme(const LsdbTheme *theme, const LsdbThemeText *title,
	const LsdbThemeText *provider)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbSetTheme(connection, &theme->id, theme->type,
		&theme->packageImageFilePath, &theme->homePreviewFilePath,
		&theme->startPreviewFilePath, &theme->titleDefault, &theme->providerDefault,
		theme->contentVer, &theme->lastModifiedTime, theme->size, title->m_language,
		LSDB_THEME_LANGUAGES, provider->m_language, LSDB_THEME_LANGUAGES);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::DeleteTheme(const paf::string *id)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbDeleteTheme(connection, id);
	m_access->EndTransaction(result);
	return result;
}

int AppInfoDaemon::DeleteAllThemes()
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbDeleteAllThemes(connection);
	m_access->EndTransaction(result);
	return result;
}

/* the number of themes, or the error */
int AppInfoDaemon::GetThemeCount()
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	int count = -1;
	ActivityDbConnection *connection = m_access->BeginTransaction();
	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	int result = AppDbGetThemeCount(connection, &count);
	m_access->EndTransaction(result);
	if (count >= 0)
		result = count;
	return result;
}

/* the transaction an icon's image is read in (DbBlobOpen) */
static int AppInfoDaemonTransaction(int op, bool readOnly, void *arg,
	ActivityDbConnection **connection)
{
	AppInfoDaemon *daemon = AppInfoDaemon::GetInstance();

	if (daemon->m_access == NULL)
		return LSDB_ERROR_NOT_READY;
	if (op == DB_BLOB_TRANSACTION_BEGIN) {
		*connection = daemon->m_access->BeginTransaction();
		if (*connection == NULL)
			return LSDB_ERROR_NOT_READY;
		return 0;
	}
	if (op == DB_BLOB_TRANSACTION_END)
		return daemon->m_access->EndTransaction(0);
	return 0;
}
