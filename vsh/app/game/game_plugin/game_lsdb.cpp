/*
 * The game plugin's calls into livespace_db and the system: the NP account ID, an application's
 * information, title queries, icon counts and a device's cluster size. The file's and every name
 * here are ours (no record names them); the strings are Sony's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <sceerror.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_license.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <regmgr/regmgr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/plugins.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "copier.h"

using namespace paf;

#define GAME_ERROR_NO_DEVICE		-2146430678	/* 0x8010112A: a path without a device name */
#define GAME_ERROR_INVALID_TITLE_ID	-2146430573	/* 0x80101193: not nine capitals and digits */

/* what devctl 0x3001 reports of a device */
struct GameDevInfo {
	SceInt64 maxSize;
	SceInt64 freeSize;
	SceUInt32 clusterSize;
	void *reserved;
};

/* the license errors gameLicenseGetStatus reports (values only) */
#define GAME_LICENSE_ERROR_NOT_FOUND	-2139095036	/* 0x80800004 */
#define GAME_LICENSE_ERROR_NO_ACCOUNT	-2138636280	/* 0x80870008 */

/* PLACEHOLDER names, defined in other files of the shell */
int gameAppDeletePrepare(const char *titleId);			/* 0x810FCA70 */
int gameAppDeleteFiles(const char *titleId, LsdbAppInfoPtr *info);	/* 0x810609AC */
int gameAppDirCleanup(string *appDir);				/* 0x81020438 */
int gameAppDeleteFilesPart(const char *titleId, LsdbAppInfoPtr *info, int part);	/* 0x81062BBA */
extern "C" int sceNpDrmGetRifName(char *rifName, SceUInt64 accountId);	/* 0x81449964 */
int gameIoGetDeviceSpace(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize);	/* 0x8137E308 */
int gameAppDataOperateFiles(const string &titleId, LsdbAppInfoPtr *info, int arg1, int arg2,
			    int arg3, int arg4);				/* 0x81063906 */

/* The NP account ID, 0 when the registry has none (0x81018610). */
int gameNpGetAccountId(SceUInt64 *accountId)
{
	SceUInt8 id[8];

	*accountId = 0;

	int ret = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", id, sizeof(id));

	if (ret < 0)
		return ret;

	sce_paf_memcpy(accountId, id, sizeof(id));

	return 0;
}

/* An application's information from livespace_db (0x81018666); Sony's callers call it whole. */
__attribute__((noinline)) int gameAppGetInfo(const char *titleId, LsdbAppInfoPtr *info)
{
	AppInfoQClient *client = (AppInfoQClient *)operator new(sizeof(AppInfoQClient));

	if (client == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	client = SceLsdb_A6D8772C(client);
	if (client == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = SceLsdb_E90B81CD(client);

	if (ret < 0) {
		operator delete(SceLsdb_E2101B03(client));
		return ret;
	}

	ret = SceLsdb_D239031A(client, &string(titleId), info);
	if (ret < 0) {
		operator delete(SceLsdb_E2101B03(client));
		return ret;
	}

	operator delete(SceLsdb_E2101B03(client));

	return 0;
}

/* The title IDs of the applications that meet conditions (0x81018736). */
int gameFindTitles(LsdbQueryCondition *conditions, vector<string> *titleIds)
{
	AppInfoQClient *client = (AppInfoQClient *)operator new(sizeof(AppInfoQClient));

	if (client == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	client = SceLsdb_A6D8772C(client);
	if (client == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = SceLsdb_E90B81CD(client);

	if (ret >= 0) {
		ret = SceLsdb_76B75713(client, conditions, titleIds, 1);
		if (ret >= 0)
			ret = 0;
	}

	operator delete(SceLsdb_E2101B03(client));

	return ret;
}

/* The title IDs of the applications whose parameter key is text (0x8101878A). */
int gameFindTitlesByText(vector<string> *titleIds, unsigned int key, const char *text)
{
	LsdbQueryCondition condition;

	condition.key = key;
	condition.next = NULL;
	condition.values.push_back(LsdbValue(text));

	AppInfoQClient *client = (AppInfoQClient *)operator new(sizeof(AppInfoQClient));
	int ret;

	if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL) {
		ret = GAME_ERROR_NO_INFO_MEMORY;
	} else {
		ret = SceLsdb_E90B81CD(client);
		if (ret >= 0) {
			ret = SceLsdb_76B75713(client, &condition, titleIds, 1);
			if (ret >= 0)
				ret = 0;
		}
		operator delete(SceLsdb_E2101B03(client));
	}

	if (ret < 0)
		return ret;

	return 0;
}

/* The number of icons on count pages from pageNo (0x81018892). */
int gameCountIcons(int pageNo, unsigned int count)
{
	AppInfoSClient client;

	SceLsdb_D322A870(&client);

	int ret = SceLsdb_CF4B3684(&client, pageNo, count);

	SceLsdb_56D8DCF0(&client);

	return ret;
}

/* The number of the user's icons (0x810188D4). */
int gameCountUserIcons(void)
{
	AppInfoSClient client;

	SceLsdb_D322A870(&client);

	int ret = SceLsdb_4DB9BCB4(&client);

	SceLsdb_56D8DCF0(&client);

	return ret;
}

/* The cluster size of the device a path is on (0x8101890E). */
int gameDeviceGetClusterSize(const char *path, SceUInt64 *clusterSize)
{
	GameDevInfo info;
	char device[32];
	const char *colon = sce_paf_strchr(path, ':');

	if (colon == NULL)
		return GAME_ERROR_NO_DEVICE;

	int len = colon - path;

	sce_paf_strncpy(device, path, len + 1);
	device[len + 1] = '\0';

	int ret = sceIoDevctl(device, 0x3001, NULL, 0, &info, sizeof(info));

	if (ret < 0)
		return ret;

	*clusterSize = info.clusterSize;

	return 0;
}

/*
 * The size a folder tree takes on its device, counted in the device's clusters (0x8101898E).
 */
int gameDirGetSize(const char *path, GameCancelFunc cancel, void *arg, SceUInt64 *size)
{
	SceUInt64 clusterSize = 0;
	int ret = gameDeviceGetClusterSize(path, &clusterSize);

	if (ret < 0)
		return ret;

	if (size != NULL)
		*size = 0;

	SceUInt64 counted = 0;
	GameDirSizeCounter *counter = new GameDirSizeCounter(cancel, arg, 15);

	if (counter == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = counter->Count(&string(path), (SceUInt32)clusterSize, &counted);
	if (ret < 0) {
		delete counter;
		return ret;
	}

	delete counter;

	if (size != NULL)
		*size = counted;

	return 0;
}

/*
 * The size of an application's additional contents' licenses: grw0: for a game card's (storage
 * 1), ux0: for an installed one's (2, 3) (0x81018ACE).
 */
int gameAddcontLicenseGetSize(const string &titleId, const int *storage, GameCancelFunc cancel,
			      void *arg, SceUInt64 *size)
{
	*size = 0;

	string path;

	if (*storage == 1)
		path = string("grw0:license/addcont/") + titleId;
	else if (*storage == 2 || *storage == 3)
		path = string("ux0:license/addcont/") + titleId;

	SceUInt64 licenseSize = 0;

	if (path.length() != 0 && Dir::IsExist(path.c_str())) {
		int ret = gameDirGetSize(path.c_str(), cancel, arg, &licenseSize);

		if (ret < 0)
			return ret;
	}

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	*size = licenseSize;

	return 0;
}

/*
 * The size of an application's additional contents, the folders INSTALL_DIR_ADDCONT and
 * INSTALL_DIR_ADDCONT_ADD_1..7 name on grw0: (storage 1) or ux0: (2, 3); encrypted is cleared
 * when one of them has no sce_pfs. Sony's code does not read titleId (0x81018D8C).
 */
int gameAddcontGetSize(const string &titleId, LsdbAppInfoPtr info, const int *storage, GameCancelFunc cancel,
		       void *arg, SceUInt64 *size, bool *encrypted)
{
	unsigned int keys[] = {
		LSDB_KEY_INSTALL_DIR_ADDCONT,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_1, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_2,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_3, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_4,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_5, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_6,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_7,
	};
	SceUInt64 total = 0;
	int i;

	*size = 0;
	*encrypted = true;

	for (i = 0; i < 8; i++) {
		LsdbAppInfo::iterator it = info->find(keys[i]);

		if (it == info->end()) {
			if (i == 0)
				return GAME_ERROR_NO_VALUE;
			continue;
		}

		string path;
		string dir(LsdbValueText(it->second));

		if (dir.length() == 0)
			continue;

		if (*storage == 1)
			path = string("grw0:addcont/") + dir;
		else if (*storage == 2 || *storage == 3)
			path = string("ux0:addcont/") + dir;

		SceUInt64 dirSize = 0;

		if (Dir::IsExist(path.c_str())) {
			string pfs = path + string("/sce_pfs");

			if (!Dir::IsExist(pfs.c_str())) {
				*encrypted = false;
				continue;
			}

			int ret = gameDirGetSize(path.c_str(), cancel, arg, &dirSize);

			if (ret < 0)
				return ret;

			total += dirSize;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	*size = total;

	return 0;
}

/*
 * An application's sizes, for what flags asks: its folder (1, installed applications only), its
 * patch (2), its additional contents (4), and its save data, licenses and appmeta (8)
 * (0x810192A4).
 */
int gameAppGetSizes(const string *appDir, const string &titleId, unsigned int flags,
		    const LsdbAppInfoPtr *info, GameCancelFunc cancel, void *arg,
		    GameAppSizes *sizes)
{
	int storage;
	SceUInt64 size;
	int ret;

	sce_paf_memset(sizes, 0, sizeof(*sizes));
	size = 0;

	LsdbAppInfo::iterator end = (*info)->end();

	ret = gameAppGetStorage(info, &storage);
	if (ret < 0)
		return ret;

	if ((flags & 1) != 0 && storage == 3) {
		SceUInt64 appSize = 0;

		ret = gameDirGetSize(appDir->c_str(), cancel, arg, &appSize);
		if (ret < 0)
			return ret;

		sizes->app = appSize;
		sizes->total += appSize;
	}

	string path;

	if ((flags & 2) != 0) {
		path.clear();
		if (storage == 2 || storage == 3)
			path = string("ux0:patch/") + titleId;
		else if (storage == 1)
			path = string("grw0:patch/") + titleId;

		size = 0;
		if (path.length() != 0 && Dir::IsExist(path.c_str())) {
			SceUInt64 patchSize = 0;

			ret = gameDirGetSize(path.c_str(), cancel, arg, &patchSize);
			if (ret < 0)
				return ret;

			sizes->patch = patchSize;
			sizes->total += patchSize;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	unsigned int dataFlag = flags & 8;

	if (dataFlag != 0) {
		path.clear();

		string saveDir;
		LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_INSTALL_DIR_SAVEDATA);

		if (it == end)
			return GAME_ERROR_NO_VALUE;

		saveDir = LsdbValueText(it->second);

		if (storage == 2 || storage == 3) {
			char userDir[32];

			ret = sceAppMgrGetUserDirPath(2, userDir, sizeof(userDir));
			if (ret < 0)
				return ret;

			path = string(userDir) + string("/savedata/") + saveDir;
		} else if (storage == 1) {
			path = string("grw0:savedata/") + saveDir;
		}

		size = 0;
		if (path.length() != 0 && Dir::IsExist(path.c_str())) {
			ret = gameDirGetSize(path.c_str(), cancel, arg, &size);
			if (ret < 0)
				return ret;

			sizes->data += size;
			sizes->total += size;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	if ((flags & 4) != 0) {
		SceUInt64 addcontSize = 0;
		bool encrypted = false;

		ret = gameAddcontGetSize(titleId, *info, &storage, cancel, arg, &addcontSize, &encrypted);
		if (ret < 0)
			return ret;

		sizes->addcontEncrypted = encrypted;
		sizes->addcont = addcontSize;
		sizes->total += addcontSize;
	}

	if (dataFlag != 0) {
		path.clear();
		if (storage == 2 || storage == 3)
			path = string("ux0:license/app/") + titleId;
		else if (storage == 1)
			path = string("grw0:license/app/") + titleId;

		size = 0;
		if (path.length() != 0 && Dir::IsExist(path.c_str())) {
			ret = gameDirGetSize(path.c_str(), cancel, arg, &size);
			if (ret < 0)
				return ret;

			sizes->data += size;
			sizes->total += size;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;

		SceUInt64 licenseSize = 0;

		ret = gameAddcontLicenseGetSize(titleId, &storage, cancel, arg, &licenseSize);
		if (ret < 0)
			return ret;

		sizes->data += licenseSize;
		sizes->total += licenseSize;

		path.clear();
		if (storage == 2 || storage == 3)
			path = string("ux0:appmeta/") + titleId;

		size = 0;
		if (path.length() != 0 && Dir::IsExist(path.c_str())) {
			ret = gameDirGetSize(path.c_str(), cancel, arg, &size);
			if (ret < 0)
				return ret;

			sizes->data += size;
			sizes->total += size;
		}
	}

	return 0;
}

/*
 * A PSP game's sizes, for what flags asks: its folder (1) and, in the patch's place, its game
 * data in ux0:pspemu/PSP/GAME (2). Sony's code does not read titleId (0x81019FEA).
 */
int gamePspGetSizes(const string *appDir, const string &titleId, unsigned int flags,
		    const LsdbAppInfoPtr *info, GameCancelFunc cancel, void *arg,
		    GameAppSizes *sizes)
{
	SceUInt64 size;
	int ret;

	sce_paf_memset(sizes, 0, sizeof(*sizes));
	size = 0;

	if ((flags & 1) != 0) {
		ret = gameDirGetSize(appDir->c_str(), cancel, arg, &size);
		if (ret < 0)
			return ret;

		sizes->app = size;
		sizes->total += size;

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	if ((flags & 2) == 0)
		return 0;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_GAMEDATA_ID);

	if (it != (*info)->end()) {
		string gameDataId(LsdbValueText(it->second));

		if (gameDataId.length() != 0) {
			string path = string("ux0:pspemu/PSP/GAME/") + gameDataId;

			if (Dir::IsExist(path.c_str())) {
				ret = gameDirGetSize(path.c_str(), cancel, arg, &size);
				if (ret < 0)
					return ret;

				sizes->patch = size;
				sizes->total += size;
			}
		}
	}

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	return 0;
}

/*
 * A system application's sizes, for what flags asks: its folder (1), and its appmeta in
 * ur0:appmeta and its license in ux0:license/app (8). Sony's code does not read info
 * (0x8101A28E).
 */
int gameSysAppGetSizes(const string *appDir, const string &titleId, unsigned int flags,
		       const LsdbAppInfoPtr *info, GameCancelFunc cancel, void *arg,
		       GameAppSizes *sizes)
{
	SceUInt64 size;
	int ret;

	sce_paf_memset(sizes, 0, sizeof(*sizes));
	size = 0;

	if ((flags & 1) != 0) {
		ret = gameDirGetSize(appDir->c_str(), cancel, arg, &size);
		if (ret < 0)
			return ret;

		sizes->app = size;
		sizes->total += size;

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	if ((flags & 8) == 0)
		return 0;

	{
		string appmeta = string("ur0:appmeta") + string("/") + titleId;

		ret = gameDirGetSize(appmeta.c_str(), cancel, arg, &size);
		if (ret < 0)
			return ret;

		sizes->data += size;
		sizes->total += size;

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	string license = string("ux0:") + string("license/app/") + titleId;

	if (LocalFile::Exists(license.c_str())) {
		ret = gameDirGetSize(license.c_str(), cancel, arg, &size);
		if (ret < 0)
			return ret;

		sizes->data += size;
		sizes->total += size;
	}

	return 0;
}

/*
 * An application's sizes by its title ID, for what flags asks, counted the way its category
 * kind needs: a Vita application (1), a PSP game (2, 3) or a system application (4, 5)
 * (0x8101A78A).
 */
int gameTitleGetSizes(const char *titleId, unsigned int flags, GameCancelFunc cancel, void *arg,
		      GameAppSizes *sizes)
{
	sce_paf_memset(sizes, 0, sizeof(*sizes));

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret < 0)
		return ret;

	string appDir;
	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);

	if (it == info->end())
		return GAME_ERROR_NO_VALUE;

	appDir = LsdbValueText(it->second);

	int kind;

	ret = gameAppInfoGetCategoryKind(&info, &kind);
	if (ret < 0)
		return ret;

	switch (kind) {
	case 1:
		ret = gameAppGetSizes(&appDir, string(titleId), flags, &info, cancel, arg, sizes);
		break;
	case 2:
	case 3:
		ret = gamePspGetSizes(&appDir, string(titleId), flags, &info, cancel, arg, sizes);
		break;
	case 4:
	case 5:
		ret = gameSysAppGetSizes(&appDir, string(titleId), flags, &info, cancel, arg, sizes);
		break;
	}

	return ret;
}

/* A file copied, by a copier of its own, with buf as its work memory (0x8101AB80). */
int gameCopyFile(const char *src, const char *dst, int arg2, int arg3, void *buf,
		 SceSize bufSize, int arg6, int arg7)
{
	GameCopyParam param;

	param.cancel = (GameCancelFunc)arg2;
	param.reserved2 = false;
	param.replace = false;
	param.reserved = false;
	param.heap = NULL;
	param.maxDepth = 128;
	param.cancelArg = (void *)arg3;
	param.bufSize = bufSize;
	param.buf = buf;
	param.progress = (GameJobProgress *)arg6;
	param.detail = (string *)arg7;

	GameFileCopier *copier = new GameFileCopier(&param);

	if (copier == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	SceUInt64 copied = 0;
	int ret = copier->Copy(&string(src), &string(dst), &copied);

	delete copier;

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * A file copied with work memory from the "GamePromoter" pool (0x8101ACA0). PLACEHOLDER name;
 * the other words are gameCopyFile's.
 */
int gameCopyFileWithPool(const char *src, const char *dst, int arg2, int arg3, int arg6,
			 int arg7)
{
	common::SharedPtr<memory::PoolAllocator> pool;
	int ret;

	ret = gamePluginGetPool("GamePromoter", &pool);
	if (ret < 0)
		return ret;

	SceSize size = pool->GetFreeSize();
	void *buf = pool->Alloc();

	if (buf == NULL) {
		gamePluginReleasePool(&pool);
		return GAME_ERROR_NO_MEMORY;
	}

	ret = gameCopyFile(src, dst, arg2, arg3, buf, size, arg6, arg7);
	if (ret < 0) {
		pool->Free(buf);
		gamePluginReleasePool(&pool);
		return ret;
	}

	pool->Free(buf);
	gamePluginReleasePool(&pool);
	return 0;
}

/*
 * A file or a folder tree moved to dst, by a mover of its own, its walk stopped when cancel says
 * so (0x8101AEA0). The mover's progress word is left as the stack holds it, as Sony's is.
 */
int gameMoveTree(const char *src, const char *dst, GameCancelFunc cancel, void *cancelArg,
		 string *detail)
{
	GameTreeMoveParam param;

	param.cancel = cancel;
	param.cancelArg = cancelArg;
	param.maxDepth = 128;
	param.reserved2 = false;
	param.reserved = false;
	param.moveFiles = true;
	param.detail = detail;

	GameTreeMover *mover = new GameTreeMover(&param);

	if (mover == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = mover->Move(string(src), string(dst));

	delete mover;

	if (ret < 0)
		return ret;

	return 0;
}

/* The device of a path, up to and including its colon (0x8101AF9E). */
int gamePathGetDevice(const string &path, string *device)
{
	string::size_type pos = path.find(':');

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	*device = path.substr(0, pos + 1);

	return 0;
}

/*
 * An application deleted: its content manager imports, its files by the kind of application,
 * its registration with applauncher_plugin and its directory's leftovers (0x8101B058).
 */
int gameAppDelete(const char *titleId)
{
	int ret;

	ret = gameAppDeletePrepare(titleId);
	if (ret < 0)
		return ret;

	ret = gameContentImportDelete(titleId);
	if (ret < 0)
		return ret;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	ret = gameAppGetInfo(titleId, &info);
	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);

	if (it == info->end())
		return GAME_ERROR_NO_VALUE;

	string appDir(LsdbValueText(it->second));

	ret = gameAppDeleteFiles(titleId, &info);
	if (ret < 0)
		return ret;

	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)applauncher->GetInterface(1);

		if (iface != NULL) {
			string id(titleId);

			if (((int (*)(string *))iface->func[1])(&id) != 0)
				return GAME_ERROR_UNREGISTER;
		}
	}

	gameAppDirCleanup(&appDir);

	return 0;
}

/*
 * A part of an application deleted, by kind 1 to 4 (0x8101B3FC). PLACEHOLDER: which part each
 * kind names is not read yet; the application's directory is read but not used.
 */
int gameAppDeletePart(const char *titleId, int kind)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);

	if (it == info->end())
		return GAME_ERROR_NO_VALUE;

	string appDir(LsdbValueText(it->second));
	int part;

	switch (kind) {
	case 1:
		part = 0;
		break;
	case 2:
		part = 1;
		break;
	case 3:
		part = 2;
		break;
	case 4:
		part = 3;
		break;
	default:
		return GAME_ERROR_INVALID_ARGUMENT;
	}

	ret = gameAppDeleteFilesPart(titleId, &info, part);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * An operation on an application's data, for applications only (0x8101B746). PLACEHOLDER: what
 * the operation and its words are is not read yet.
 */
int gameAppDataOperate(const char *titleId, int arg1, int arg2, int arg3, int arg4)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret < 0)
		return ret;

	int kind = 0;

	ret = gameAppInfoGetCategoryKind(&info, &kind);
	if (ret < 0)
		return ret;

	if (kind != 1)
		return GAME_ERROR_INVALID_ARGUMENT;

	ret = gameAppDataOperateFiles(string(titleId), &info, arg1, arg2, arg3, arg4);
	if (ret < 0)
		return ret;

	return 0;
}

int gameRemoveDirectory(const char *path, GameCancelFunc cancel, void *cancelArg,
			GameJobProgress *progress, bool mapErrors)
{
	GameTreeRemover *remover = new GameTreeRemover(progress, cancel, cancelArg, mapErrors, 15);

	if (remover == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = remover->Remove(string(path));

	delete remover;

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * What a directory holds removed, each entry by gameRemoveDirectory with the same words; entries
 * whose names start with a dot are kept (0x8101BAC8).
 */
int gameRemoveDirectoryContents(const char *path, GameCancelFunc cancel, void *cancelArg,
				GameJobProgress *progress, bool mapErrors)
{
	Dir dir;
	int ret = dir.Open(path);

	if (ret < 0)
		return ret;

	for (;;) {
		DirEnt entry;

		ret = dir.Read(&entry);
		if (ret == 0 && sce_paf_strncmp(entry.name.c_str(), ".", 1) != 0) {
			string child = string(path) + string("/") + entry.name;

			ret = gameRemoveDirectory(child.c_str(), cancel, cancelArg, progress, mapErrors);
			if (ret < 0) {
				dir.Close();
				return ret;
			}
		}
		if (ret != 0)
			break;
	}

	dir.Close();

	return 0;
}

/*
 * The status of the license of the application at status->path: a PSP game's from its own
 * license, any other's from appmgr's (0x8101BD12).
 */
int gameLicenseGetStatus(GameLicenseStatus *status)
{
	int ret;

	if (status == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (status->path[0] == '\0')
		return GAME_ERROR_INVALID_ARGUMENT;
	if (gameStrnlen(status->path, sizeof(status->path)) > sizeof(status->path) - 1)
		return GAME_ERROR_INVALID_ARGUMENT;

	if (sce_paf_strncmp("ux0:pspemu/PSP/GAME", status->path, 19) == 0) {
		GamePspLicense *license = new GamePspLicense;

		if (license == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = license->Check(string(status->path));
		if (ret < 0) {
			delete license;
			if (ret == GAME_PAF_ERROR_NOT_FOUND) {
				status->valid = false;
				status->result = GAME_LICENSE_ERROR_NOT_FOUND;
				return 0;
			}
			return ret;
		}

		status->result = license->m_result;
		status->valid = license->m_valid;
		status->startTime = license->m_startTime;
		status->expireTime = license->m_expireTime;

		delete license;

		gameLicenseGetRemaining(&status->expireTime, &status->remaining);
		status->expireTextResult = gameLicenseFormatTime(&status->expireTime,
								 status->expireText,
								 sizeof(status->expireText));

		switch (status->result) {
		case 0:
			ret = gameLicenseCheckPeriod(&status->startTime, &status->expireTime);
			if (ret < 0)
				status->result = ret;
			break;
		case -2138636282:	/* 0x80870006: PLACEHOLDER, meaning not known */
		case -2138636281: {	/* 0x80870007: PLACEHOLDER, meaning not known */
			SceUInt64 accountId;

			if (gameNpGetAccountId(&accountId) >= 0 && accountId == 0)
				status->result = GAME_LICENSE_ERROR_NO_ACCOUNT;
			break;
		}
		}
	} else {
		SceUInt64 accountId;

		ret = gameNpGetAccountId(&accountId);
		if (ret < 0)
			return ret;

		AppmgrLicenseInfo info;

		sce_paf_memset(&info, 0, sizeof(info));
		ret = sceAppMgrCheckRifGD(status->path, &info);
		status->result = ret;
		if (ret < 0) {
			if (ret == GAME_LICENSE_ERROR_NOT_FOUND) {
				status->result = GAME_LICENSE_ERROR_NOT_FOUND;
				status->valid = false;
				return 0;
			}
		}

		status->startTime = info.licStartTime;
		status->expireTime = info.licExpTime;

		gameLicenseGetRemaining(&status->expireTime, &status->remaining);
		status->expireTextResult = gameLicenseFormatTime(&status->expireTime,
								 status->expireText,
								 sizeof(status->expireText));
		if ((status->result != 0 || accountId != 0) &&
		    status->result != (int)0x80010002)
			status->valid = true;
		else
			status->valid = false;
		if (info.flags & 2)
			status->flag1 = true;
		else
			status->flag1 = false;
	}

	return 0;
}

/* 0x8101BFB4 */
void gamePathGetMountedDrive(const string &path, string *drive)
{
	if (drive == NULL)
		return;

	string found;

	gameMountGetDrive(path, &found);
	if (found.size() != 0)
		*drive = found;
	else if (gamePathGetDrive(path, &found) == 0)
		*drive = found;
}

/* A device's size and its free space (0x8101C038). */
int gameDeviceGetSpace(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize)
{
	int ret = gameIoGetDeviceSpace(device, maxSize, freeSize);

	if (ret < 0)
		return ret;

	return 0;
}

/* A path's length, at most max (0x8101C04A). */
SceSize gamePathGetLength(const char *path, SceSize max)
{
	return gameStrnlen(path, max);
}

/* How many files and folders a folder tree holds (0x8101C052). */
int gameTreeGetCount(const char *path, GameCancelFunc cancel, void *arg, SceUInt32 *count)
{
	GameTreeScanner *scanner = new GameTreeScanner(cancel, arg, 15);

	if (scanner == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = scanner->Scan(string(path));

	if (ret < 0) {
		delete scanner;
		return ret;
	}

	if (count != NULL)
		*count = scanner->m_fileCount + scanner->m_dirCount;

	delete scanner;

	return 0;
}

/* What the files of a folder tree add up to (0x8101C118). */
int gameTreeGetSize(const char *path, GameCancelFunc cancel, void *arg, SceUInt64 *size)
{
	GameTreeScanner *scanner = new GameTreeScanner(cancel, arg, 15);

	if (scanner == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = scanner->Scan(string(path));

	if (ret < 0) {
		delete scanner;
		return ret;
	}

	if (size != NULL)
		*size = scanner->m_totalSize;

	delete scanner;

	return 0;
}

/* an IO error and the paf error it is reported as */
struct GameIoErrorMap {
	int ioError;
	int pafError;
};

static const GameIoErrorMap s_dirErrors[] = {
	{ SCE_ERROR_ERRNO_ENOENT, SCE_PAF_ERROR_DIR_NO_MORE_ENTRY },
};

static const GameIoErrorMap s_fileErrors[] = {
	{ SCE_ERROR_ERRNO_EINVAL, SCE_PAF_ERROR_FILE_INVALID_ARGUMENT },
	{ SCE_ERROR_ERRNO_ENOMEM, SCE_PAF_ERROR_FILE_NO_MEMORY },
	{ SCE_ERROR_ERRNO_ENOSYS, SCE_PAF_ERROR_FILE_READ_ONLY },
	{ SCE_ERROR_ERRNO_ENOTSUP, SCE_PAF_ERROR_FILE_NOT_SUPPORTED },
	{ SCE_ERROR_ERRNO_EBADF, SCE_PAF_ERROR_FILE_BAD_FILE },
	{ SCE_ERROR_ERRNO_ENOENT, SCE_PAF_ERROR_FILE_NO_ENTRY },
	{ SCE_ERROR_ERRNO_EMFILE, SCE_PAF_ERROR_FILE_TOO_MANY },
	{ SCE_ERROR_ERRNO_ENODEV, SCE_PAF_ERROR_FILE_NO_DEVICE },
	{ SCE_ERROR_ERRNO_EACCES, SCE_PAF_ERROR_FILE_NO_ACCESS },
	{ SCE_ERROR_ERRNO_EEXIST, SCE_PAF_ERROR_FILE_EXIST },
	{ SCE_ERROR_ERRNO_ENOSPC, SCE_PAF_ERROR_FILE_NO_SPACE },
};

/* A folder error as paf reports it; an error the map does not hold is an invalid argument (0x8101C1DC). */
int gameDirToPafError(int error)
{
	for (unsigned int i = 0; i < sizeof(s_dirErrors) / sizeof(s_dirErrors[0]); i++) {
		if (s_dirErrors[i].ioError == error)
			return s_dirErrors[i].pafError;
	}

	return SCE_PAF_ERROR_DIR_INVALID_ARGUMENT;
}

/* A file error as paf reports it; an error the map does not hold is 0x80AF5001 (0x8101C212). */
int gameFileToPafError(int error)
{
	for (unsigned int i = 0; i < sizeof(s_fileErrors) / sizeof(s_fileErrors[0]); i++) {
		if (s_fileErrors[i].ioError == error)
			return s_fileErrors[i].pafError;
	}

	return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
}

/*
 * Appends "/" and name to path, a buffer of pathSize bytes; a path that would not fit is
 * GAME_ERROR_NO_DRIVE (0x8101C248).
 */
int gamePathAppend(char *path, SceSize pathSize, const char *name, SceSize nameSize)
{
	SceSize pathLen = gameStrnlen(path, pathSize);
	SceSize nameLen = gameStrnlen(name, nameSize);

	if (pathSize <= nameLen + pathLen + 1)
		return GAME_ERROR_NO_DRIVE;

	sce_paf_strncat(path, "/", 1);
	sce_paf_strncat(path, name, nameLen);

	return 0;
}

/* the folders of an application gameAppGetDirsSize adds up */
#define GAME_APP_DIR_APP	0x1	/* ux0:app/<title ID> */
#define GAME_APP_DIR_APPMETA	0x2	/* ux0:appmeta/<title ID> */
#define GAME_APP_DIR_LICENSE	0x4	/* ux0:license/app/<title ID> */
#define GAME_APP_DIR_PATCH	0x8	/* ux0:patch/<title ID> */

/*
 * What an application's folders on ux0 take, the folders dirs names, each counted in its
 * device's clusters; cancel is polled after each folder (0x8101C298).
 */
int gameAppGetDirsSize(const char *titleId, int dirs, GameCancelFunc cancel, void *arg,
		       SceUInt64 *size)
{
	if (titleId == NULL || size == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	*size = 0;

	if (dirs & GAME_APP_DIR_APP) {
		string path = string("ux0:app/") + string(titleId);

		if (Dir::IsExist(path.c_str())) {
			SceUInt64 dirSize = 0;
			int ret = gameDirGetSize(path.c_str(), cancel, arg, &dirSize);

			if (ret < 0)
				return ret;

			*size += dirSize;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	if (dirs & GAME_APP_DIR_APPMETA) {
		string path = string("ux0:appmeta/") + string(titleId);

		if (Dir::IsExist(path.c_str())) {
			SceUInt64 dirSize = 0;
			int ret = gameDirGetSize(path.c_str(), cancel, arg, &dirSize);

			if (ret < 0)
				return ret;

			*size += dirSize;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	if (dirs & GAME_APP_DIR_LICENSE) {
		string path = string("ux0:license/app/") + string(titleId);

		if (Dir::IsExist(path.c_str())) {
			SceUInt64 dirSize = 0;
			int ret = gameDirGetSize(path.c_str(), cancel, arg, &dirSize);

			if (ret < 0)
				return ret;

			*size += dirSize;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	if (dirs & GAME_APP_DIR_PATCH) {
		string path = string("ux0:patch/") + string(titleId);

		if (Dir::IsExist(path.c_str())) {
			SceUInt64 dirSize = 0;
			int ret = gameDirGetSize(path.c_str(), cancel, arg, &dirSize);

			if (ret < 0)
				return ret;

			*size += dirSize;
		}

		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;
	}

	return 0;
}

/*
 * An installed application's sizes on ux0, for what flags asks: its folder (1), its patch (2),
 * its additional contents (4), and its appmeta and licenses (8) (0x8101C950).
 */
int gameAppGetInstalledSizes(const string &titleId, unsigned int flags, GameCancelFunc cancel,
			     void *arg, GameAppSizes *sizes)
{
	int ret;

	if (titleId == "")
		return GAME_ERROR_INVALID_ARGUMENT;

	sce_paf_memset(sizes, 0, sizeof(*sizes));

	if ((flags & 1) != 0) {
		SceUInt64 appSize = 0;

		ret = gameAppGetDirsSize(titleId.c_str(), GAME_APP_DIR_APP, cancel, arg, &appSize);
		if (ret < 0)
			return ret;

		sizes->app = appSize;
		sizes->total += appSize;
	}

	if ((flags & 2) != 0) {
		SceUInt64 patchSize = 0;

		ret = gameAppGetDirsSize(titleId.c_str(), GAME_APP_DIR_PATCH, cancel, arg, &patchSize);
		if (ret < 0)
			return ret;

		sizes->patch = patchSize;
		sizes->total += patchSize;
	}

	if ((flags & (4 | 8)) == 0)
		return 0;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	ret = gameAppGetInfo(titleId.c_str(), &info);
	if (ret < 0)
		return ret;

	int storage;

	ret = gameAppGetStorage(&info, &storage);
	if (ret < 0)
		return ret;

	if ((flags & 4) != 0) {
		SceUInt64 addcontSize = 0;
		bool encrypted = false;

		ret = gameAddcontGetSize(titleId, info, &storage, cancel, arg, &addcontSize, &encrypted);
		if (ret < 0)
			return ret;

		sizes->addcont = addcontSize;
		sizes->total += addcontSize;
		sizes->addcontEncrypted = encrypted;
	}

	if ((flags & 8) != 0) {
		SceUInt64 dataSize = 0;

		ret = gameAppGetDirsSize(titleId.c_str(), GAME_APP_DIR_APPMETA | GAME_APP_DIR_LICENSE,
					 cancel, arg, &dataSize);
		if (ret < 0)
			return ret;

		SceUInt64 licenseSize = 0;

		ret = gameAddcontLicenseGetSize(titleId, &storage, cancel, arg, &licenseSize);
		if (ret < 0)
			return ret;

		dataSize += licenseSize;
		sizes->data = dataSize;
		sizes->total += dataSize;
	}

	return 0;
}

/* Whether path holds no file or folder; what is not a folder counts as empty (0x8101CDFA). */
bool gameDirIsEmpty(const string &path)
{
	if (!Dir::IsDirectory(path.c_str()))
		return true;

	bool found = false;
	bool empty = false;
	Dir dir;

	if (dir.Open(path.c_str()) < 0)
		return false;

	int ret;

	do {
		DirEnt entry;

		ret = dir.Read(&entry);
		if (ret == 0) {
			if (entry.name == "." || entry.name == "..")
				continue;

			if (entry.type == DirEnt::Type_Dir || entry.type == DirEnt::Type_File) {
				found = true;
				break;
			}
		}
	} while (ret == 0);

	dir.Close();

	if (!found)
		empty = true;

	return empty;
}

/* The title IDs of the applications the shell keeps as its own (0x8101CEFA). */
void gamePresetGetTitleIds(vector<string> *titleIds)
{
	titleIds->push_back(string("PCSC00025"));
	titleIds->push_back(string("PCSC80003"));
	titleIds->push_back(string("PCSC00046"));
}

/* Whether titleId is one the shell keeps as its own (0x8101D098). */
int gamePresetCheckTitle(const char *titleId, GamePresetTitle *preset)
{
	if (titleId == NULL || preset == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	preset->isPreset = false;
	preset->kind = 0;

	vector<string> titleIds;

	gamePresetGetTitleIds(&titleIds);

	for (unsigned int i = 0; i < titleIds.size(); i++) {
		if (titleIds[i] == string(titleId)) {
			preset->isPreset = true;
			preset->kind = 1;
			return 0;
		}
	}

	bool match = string("PCSC80012") == string(titleId);

	if (match) {
		preset->isPreset = true;
		preset->kind = 2;
		return 0;
	}

	return 0;
}

/*
 * What the data of a title the shell keeps as its own takes: ux0:umass for one of
 * gamePresetGetTitleIds's, ux0:book for PCSC80012 (0x8101D2C8).
 */
int gamePresetGetDataSize(const char *titleId, GameCancelFunc cancel, void *arg,
			  GamePresetData *data)
{
	if (titleId == NULL || data == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	data->kind = 0;
	data->size = 0;

	GamePresetTitle preset;
	int ret = gamePresetCheckTitle(titleId, &preset);

	if (ret < 0)
		return ret;

	if (preset.isPreset) {
		data->kind = preset.kind;

		string path;

		switch (preset.kind) {
		case 1:
			path = "ux0:umass";
			break;
		case 2:
			path = "ux0:book";
			break;
		}

		if (Dir::IsExist(path.c_str()) && !gameDirIsEmpty(path)) {
			SceUInt64 size = 0;

			ret = gameDirGetSize(path.c_str(), cancel, arg, &size);
			if (ret < 0)
				return ret;

			data->size = size;
		}
	}

	return 0;
}

/* The name of the user's license file, from the NP account ID (0x8101D3DE). */
int gameNpDrmGetRifName(string *rifName)
{
	rifName->clear();

	SceUInt64 accountId;
	int ret = gameNpGetAccountId(&accountId);

	if (ret >= 0) {
		char name[48];

		sce_paf_memset(name, 0, sizeof(name));
		ret = sceNpDrmGetRifName(name, accountId);
		if (ret >= 0)
			*rifName = string(name);
	}

	return ret;
}

/*
 * The name of the user's free-trial license file: the license file's name with its extension
 * replaced by .ftt (0x8101D49E).
 */
int gameNpDrmGetFttName(string *fttName)
{
	fttName->clear();

	string rifName;
	int ret = gameNpDrmGetRifName(&rifName);

	if (ret < 0)
		return ret;

	*fttName = rifName;

	string::size_type pos = fttName->find('.');

	if (pos != string::npos) {
		string base = fttName->substr(0, pos);
		string name = base + ".ftt";

		*fttName = name;
	}

	return ret;
}

/* Whether titleId is a title ID: nine capital letters and digits (0x8101D662). */
int gameCheckTitleId(const char *titleId)
{
	if (titleId == NULL)
		return GAME_ERROR_INVALID_TITLE_ID;

	if (gameStrnlen(titleId, 10) != 9)
		return GAME_ERROR_INVALID_TITLE_ID;

	for (int i = 0; i < 9; i++) {
		char c = titleId[i];
		bool upper = true;
		bool digit = true;

		if (c < 'A' || c > 'Z')
			upper = false;
		if (c < '0' || c > '9')
			digit = false;

		if (!(upper | digit))
			return GAME_ERROR_INVALID_TITLE_ID;
	}

	return 0;
}
