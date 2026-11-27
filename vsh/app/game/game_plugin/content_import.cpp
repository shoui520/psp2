/*
 * The game plugin's content import: the package folders the content manager leaves in
 * ux0:content_manager/import/NN, installed for the application whose additional content they
 * are, under an installing dialog, counted and removed (0x81055EF8..0x81057D74). The file's name
 * and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <kernel/iofilemgr.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <file/openurl.h>
#include <thread/thread.h>
#include <graphics/surface/surface.h>
#include <plugin.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbValue, paf::allocator<LsdbValue>)

/* where the content manager leaves the folders to import, and where one goes to be removed */
#define GAME_CONTENT_IMPORT_DIR		"ux0:content_manager/import"
#define GAME_CONTENT_IMPORT_TEMP	"ux0:content_manager/import/_TEMP"

/* the import folders 00..99, a package's head read into a buffer of this size */
#define GAME_CONTENT_IMPORT_SLOTS	100
#define GAME_CONTENT_IMPORT_BUF_SIZE	0x8000

/* the application parameters naming its additional contents' folders, and the length compared */
#define GAME_ADDCONT_DIR_NUM		8
#define GAME_ADDCONT_DIR_LEN		10

/* applauncher's text over the installing dialog (name ours) */
#define GAME_CONTENT_IMPORT_MSG_INSTALLING	0x2A862367

/*
 * The installing dialog of an import, 48 bytes: the title and icon of the application, the text,
 * and the progress the dialog shows (names ours).
 */
struct GameInstallingDialog {
	GameInstallingDialog()
	{
	}

	string title;
	intrusive_ptr<graph::Surface> icon;
	wstring text;
	bool frame;			/* the dialog's icon framed */
	float progress;			/* percent, what the dialog's bar shows */
	int dialogId;			/* the dialog open, or negative */
	float total;			/* the folders to install */
	float done;			/* and those installed */
};

typedef char GameInstallingDialogSize[sizeof(GameInstallingDialog) == 48 ? 1 : -1];

typedef int (*GamePluginGetStorageFunc)(const LsdbAppInfoPtr *info, int *storage);
typedef int (*GamePluginInstallExFunc)(GameInstallExParam *param);
typedef int (*GamePluginDialogOpenFunc)(const string &title, intrusive_ptr<graph::Surface> icon,
					const wstring &text, bool frame, const float *progress,
					bool second);
typedef void (*GamePluginDialogCloseFunc)(int dialogId);

/*
 * PLACEHOLDER declaration: livespace_db's export (appinfo_qclient.cpp), not yet in
 * livespace_db_internal.h; it moves there with that header's next change.
 */
extern "C" int SceLsdb_7AF9008C(AppInfoQClient *client, const string *titleId, LsdbIconPtr *icon,
				bool theme);

int gameContentImportCount(const char *titleId, int *count);
int gameContentImportDialogInit(GameInstallingDialog *dialog, const char *titleId);
int gameContentImportRemove(const char *path);
bool gameAddcontIsOnlyFor(const char *addcontDir, const char *titleId);
void gameInstallingDialogProgress(void *arg, float percent);
void gameInstallingDialogOpen(void *arg);
void gameInstallingDialogClose(void *arg);

/* the installing dialog of the import at work */
static GameInstallingDialog *s_installingDialog;

/* the keys of an application's additional contents' folders */
static unsigned int s_addcontKeys[GAME_ADDCONT_DIR_NUM] = {
	LsdbKeyHash("INSTALL_DIR_ADDCONT"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_1"), LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_2"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_3"), LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_4"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_5"), LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_6"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_7"),
};

/* The import folders that hold additional content of titleId, counted in *count (0x81055EF8). */
int gameContentImportCount(const char *titleId, int *count)
{
	*count = 0;

	LocalFileStat stat;
	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret != 0)
		return ret;

	char *buf = new char[GAME_CONTENT_IMPORT_BUF_SIZE];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	sce_paf_memset(buf, 0, GAME_CONTENT_IMPORT_BUF_SIZE);

	for (int i = 0; i < GAME_CONTENT_IMPORT_SLOTS; i++) {
		char path[256];

		sce_paf_snprintf(path, sizeof(path) - 2, "%s/%02d", GAME_CONTENT_IMPORT_DIR, i);
		if (LocalFile::Getstat(path, &stat) != 0)
			continue;

		string dir = path + string("/");
		GamePackageInfo package;

		sce_paf_memset(&package, 0, sizeof(package));
		if (gamePackageReadInfo(&package, dir.c_str(), buf, GAME_CONTENT_IMPORT_BUF_SIZE) != 0)
			continue;

		const unsigned int *key = s_addcontKeys;

		for (int j = 0; j < GAME_ADDCONT_DIR_NUM; j++, key++) {
			const char *addcontDir = LsdbValueText((*info)[*key]);

			if (addcontDir != NULL &&
			    sce_paf_strncmp(package.titleId, addcontDir, GAME_ADDCONT_DIR_LEN) == 0)
				(*count)++;
		}
	}

	delete[] buf;

	return ret;
}

/* The package folder at path moved into the first free import folder (0x810562E4). */
int gameContentImportPrepare(const char *path)
{
	int ret = 0;
	LocalFileStat stat;

	if (LocalFile::Getstat(GAME_CONTENT_IMPORT_DIR, &stat) != 0) {
		ret = Dir::CreateRecursive(GAME_CONTENT_IMPORT_DIR, 6);
		if (ret < 0)
			return ret;
	}

	for (int i = 0; i < GAME_CONTENT_IMPORT_SLOTS; i++) {
		char slot[256];

		sce_paf_snprintf(slot, sizeof(slot) - 2, "%s/%02d", GAME_CONTENT_IMPORT_DIR, i);
		if (LocalFile::Getstat(slot, &stat) != 0) {
			ret = LocalFile::RenameFile(path, slot);
			break;
		}
	}

	return ret;
}

/*
 * The installing dialog of titleId made: the application's title and icon from its home screen
 * icon, and the import folders to install (0x810563D6).
 */
int gameContentImportDialogInit(GameInstallingDialog *dialog, const char *titleId)
{
	if (dialog == NULL)
		return GAME_ERROR_NOT_READY;

	Plugin *plugin = Plugin::Find("game_plugin");

	if (plugin == NULL)
		return GAME_ERROR_NOT_READY;

	AppInfoQClient client;

	SceLsdb_A6D8772C(&client);

	int ret = SceLsdb_E90B81CD(&client);

	if (ret < 0) {
		SceLsdb_E2101B03(&client);
		return ret;
	}

	{
		LsdbIconPtr icon(new LsdbIcon);

		ret = SceLsdb_7AF9008C(&client, &string(titleId), &icon, false);
		if (ret >= 0) {
			OpenUrl url;

			url.RegisterOpenFunc(LocalFile::OpenUrl);

			common::SharedPtr<File> file =
				url.Open(icon->iconPath.c_str(), SCE_O_RDONLY, 0, &ret);

			if (ret >= 0) {
				int count = 0;

				ret = gameContentImportCount(titleId, &count);
				if (ret == 0) {
					graph::Surface::LoadOption option;

					option.decode_heap = plugin->m_private_heap;
					option.data_load_heap = plugin->m_private_heap;
					option.direct_load = true;

					dialog->title = icon->title;
					dialog->icon = graph::Surface::Load(plugin->m_surface_pool,
									    file, &option);
					dialog->progress = 0;
					dialog->frame = true;
					dialog->done = 0;
					dialog->total = (float)count;
				}
			}
		}
	}

	SceLsdb_E2101B03(&client);

	return ret;
}

/* The import folder at path removed, through _TEMP (0x810568F8). */
int gameContentImportRemove(const char *path)
{
	LocalFileStat stat;
	int ret;

	const char *temp = GAME_CONTENT_IMPORT_TEMP;

	if (LocalFile::Getstat(temp, &stat) == 0) {
		ret = gameRemoveDirectory(temp, 0, 0, 0, true);
		if (ret < 0)
			return ret;
	}

	ret = LocalFile::RenameFile(path, temp);
	if (ret != 0)
		return ret;

	ret = gameRemoveDirectory(temp, 0, 0, 0, true);
	if (ret < 0)
		return ret;

	return ret;
}

/*
 * The import folders of titleId's additional content installed, under the installing dialog,
 * and removed once installed (0x810569FC).
 */
int gameContentImportRun(const char *titleId)
{
	LocalFileStat stat;

	if (LocalFile::Getstat(GAME_CONTENT_IMPORT_DIR, &stat) != 0)
		return 0;

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret != 0)
		return ret;

	s_installingDialog = new GameInstallingDialog;
	if (s_installingDialog == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameContentImportDialogInit(s_installingDialog, titleId);
	if (ret != 0) {
		if (s_installingDialog != NULL) {
			delete s_installingDialog;
			s_installingDialog = NULL;
		}
		return ret;
	}

	if (&thread::CallQueue::main_thread_callqueue == NULL) {
		if (s_installingDialog != NULL) {
			delete s_installingDialog;
			s_installingDialog = NULL;
		}
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	thread::CallQueue::main_thread_callqueue.Enqueue("SyncCallOpenInstallingDialog",
							 gameInstallingDialogOpen,
							 s_installingDialog);

	char *buf = new char[GAME_CONTENT_IMPORT_BUF_SIZE];

	if (buf == NULL) {
		if (s_installingDialog != NULL) {
			delete s_installingDialog;
			s_installingDialog = NULL;
		}
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	sce_paf_memset(buf, 0, GAME_CONTENT_IMPORT_BUF_SIZE);

	int storage = 0;
	Plugin *plugin = Plugin::Find("game_plugin");

	if (plugin != NULL) {
		GamePluginInterface *iface = (GamePluginInterface *)plugin->GetInterface(1);

		if (iface != NULL)
			ret = ((GamePluginGetStorageFunc)iface->func[GAME_PLUGIN_FUNC_GET_STORAGE])(
				&info, &storage);
	}

	for (int i = 0; i < GAME_CONTENT_IMPORT_SLOTS; i++) {
		char path[256];

		sce_paf_snprintf(path, sizeof(path) - 2, "%s/%02d", GAME_CONTENT_IMPORT_DIR, i);
		if (LocalFile::Getstat(path, &stat) != 0)
			continue;

		string dir = path + string("/");
		GamePackageInfo package;

		sce_paf_memset(&package, 0, sizeof(package));
		if (gamePackageReadInfo(&package, dir.c_str(), buf, GAME_CONTENT_IMPORT_BUF_SIZE) != 0)
			continue;

		const unsigned int *key = s_addcontKeys;

		for (int j = 0; j < GAME_ADDCONT_DIR_NUM; j++, key++) {
			const char *addcontDir = LsdbValueText((*info)[*key]);

			if (addcontDir == NULL ||
			    sce_paf_strncmp(package.titleId, addcontDir, GAME_ADDCONT_DIR_LEN) != 0)
				continue;

			GameInstallExParam param;

			sce_paf_memset(&param, 0, sizeof(param));
			sce_paf_strncpy(param.path, path, sizeof(param.path));
			param.progressArg = s_installingDialog;
			param.progressFunc = gameInstallingDialogProgress;
			param.imported = true;
			param.storage = storage;

			plugin = Plugin::Find("game_plugin");
			if (plugin != NULL) {
				GamePluginInterface *iface =
					(GamePluginInterface *)plugin->GetInterface(1);

				if (iface != NULL) {
					ret = ((GamePluginInstallExFunc)
					       iface->func[GAME_PLUGIN_FUNC_INSTALL_EX])(&param);
					s_installingDialog->done += 1.0f;
				}
			}

			if (LocalFile::Getstat(path, &stat) == 0)
				ret = gameContentImportRemove(path);
		}
	}

	s_installingDialog->progress = 100.0f;

	delete[] buf;

	thread::CallQueue::main_thread_callqueue.Enqueue("SyncCallCloseInstallingDialog",
							 gameInstallingDialogClose,
							 s_installingDialog);
	s_installingDialog = NULL;

	return ret;
}

/* The import folders that hold additional content of titleId (0x8105718C). */
int gameContentImportCheck(const char *titleId)
{
	LocalFileStat stat;

	if (LocalFile::Getstat(GAME_CONTENT_IMPORT_DIR, &stat) != 0)
		return 0;

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret != 0)
		return ret;

	char *buf = new char[GAME_CONTENT_IMPORT_BUF_SIZE];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	sce_paf_memset(buf, 0, GAME_CONTENT_IMPORT_BUF_SIZE);

	int count = 0;

	for (int i = 0; i < GAME_CONTENT_IMPORT_SLOTS; i++) {
		char path[256];

		sce_paf_snprintf(path, sizeof(path) - 2, "%s/%02d/", GAME_CONTENT_IMPORT_DIR, i);
		if (LocalFile::Getstat(path, &stat) != 0)
			continue;

		GamePackageInfo package;

		sce_paf_memset(&package, 0, sizeof(package));
		if (gamePackageReadInfo(&package, path, buf, GAME_CONTENT_IMPORT_BUF_SIZE) != 0)
			continue;

		const unsigned int *key = s_addcontKeys;

		for (int j = 0; j < GAME_ADDCONT_DIR_NUM; j++, key++) {
			const char *addcontDir = LsdbValueText((*info)[*key]);

			if (addcontDir != NULL &&
			    sce_paf_strncmp(package.titleId, addcontDir, GAME_ADDCONT_DIR_LEN) == 0)
				count++;
		}
	}

	delete[] buf;

	return count;
}

/*
 * Whether the additional content folder addcontDir belongs to no application but titleId: no
 * other application names it in its INSTALL_DIR_ADDCONT parameters (0x81057476).
 */
bool gameAddcontIsOnlyFor(const char *addcontDir, const char *titleId)
{
	vector<string> titleIds;
	AppInfoQClient client;
	bool onlyFor = true;

	SceLsdb_A6D8772C(&client);

	if (SceLsdb_E90B81CD(&client) < 0) {
		SceLsdb_E2101B03(&client);
		return false;
	}

	const unsigned int *key = s_addcontKeys;

	for (int i = 0; i < GAME_ADDCONT_DIR_NUM && onlyFor; i++, key++) {
		LsdbQueryCondition condition;

		condition.key = *key;
		condition.next = NULL;
		condition.values.push_back(LsdbValue(addcontDir));

		SceLsdb_76B75713(&client, &condition, &titleIds, 0);

		for (unsigned int j = 0; j < titleIds.size(); j++) {
			if (sce_paf_strncmp(titleIds[j].c_str(), titleId, GAME_ADDCONT_DIR_LEN) != 0) {
				onlyFor = false;
				break;
			}
		}
	}

	SceLsdb_E2101B03(&client);

	return onlyFor;
}

/*
 * The import folders of additional content only titleId uses removed, and those whose package
 * cannot be read (0x810576A2).
 */
int gameContentImportDelete(const char *titleId)
{
	const char *importDir = GAME_CONTENT_IMPORT_DIR;
	LocalFileStat stat;

	if (LocalFile::Getstat(GAME_CONTENT_IMPORT_DIR, &stat) != 0)
		return 0;

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret != 0)
		return ret;

	char *buf = new char[GAME_CONTENT_IMPORT_BUF_SIZE];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	sce_paf_memset(buf, 0, GAME_CONTENT_IMPORT_BUF_SIZE);

	for (int i = 0; i < GAME_CONTENT_IMPORT_SLOTS; i++) {
		char path[256];

		sce_paf_snprintf(path, sizeof(path) - 2, "%s/%02d", importDir, i);
		if (LocalFile::Getstat(path, &stat) != 0)
			continue;

		string dir = path + string("/");
		GamePackageInfo package;

		sce_paf_memset(&package, 0, sizeof(package));
		if (gamePackageReadInfo(&package, dir.c_str(), buf, GAME_CONTENT_IMPORT_BUF_SIZE) != 0) {
			ret = gameContentImportRemove(path);
			continue;
		}

		const unsigned int *key = s_addcontKeys;

		for (int j = 0; j < GAME_ADDCONT_DIR_NUM; j++, key++) {
			const char *addcontDir = LsdbValueText((*info)[*key]);

			if (addcontDir != NULL &&
			    sce_paf_strncmp(package.titleId, addcontDir, GAME_ADDCONT_DIR_LEN) == 0 &&
			    gameAddcontIsOnlyFor(package.titleId, titleId))
				ret = gameContentImportRemove(path);
		}
	}

	delete[] buf;

	return ret;
}

/*
 * The installing dialog's bar: an install's percent placed within the share of the folder at
 * work (0x81057AC6).
 */
void gameInstallingDialogProgress(void *arg, float percent)
{
	GameInstallingDialog *dialog = (GameInstallingDialog *)arg;

	if (dialog == NULL)
		return;

	if (dialog->total < 1.0f)
		dialog->progress = percent;
	else
		dialog->progress = percent * (1.0f / dialog->total) +
				   (1.0f / dialog->total) * dialog->done * 100.0f;
}

/* The installing dialog opened, on the main thread (0x81057B06). */
void gameInstallingDialogOpen(void *arg)
{
	GameInstallingDialog *dialog = (GameInstallingDialog *)arg;
	Plugin *game = Plugin::Find("game_plugin");

	if (game == NULL)
		return;

	GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

	if (iface == NULL)
		return;

	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher == NULL || dialog == NULL)
		return;

	dialog->text = applauncher->GetString(
		IDParam(s_empty_name, GAME_CONTENT_IMPORT_MSG_INSTALLING));
	dialog->dialogId = ((GamePluginDialogOpenFunc)iface->func[GAME_PLUGIN_FUNC_DIALOG_OPEN])(
		dialog->title, dialog->icon, dialog->text, dialog->frame, &dialog->progress, false);
}

/* The installing dialog closed and deleted, on the main thread (0x81057C14). */
void gameInstallingDialogClose(void *arg)
{
	GameInstallingDialog *dialog = (GameInstallingDialog *)arg;
	Plugin *game = Plugin::Find("game_plugin");

	if (game == NULL)
		return;

	GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

	if (iface == NULL || dialog == NULL)
		return;

	if (dialog->dialogId >= 0)
		((GamePluginDialogCloseFunc)iface->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE])(
			dialog->dialogId);

	delete dialog;
}
