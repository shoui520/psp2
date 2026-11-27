/*
 * An application's #_up_status: the type of its license, read from its license folder, kept in
 * its record, which is registered again when the type changes, and told to topmenu_plugin
 * (0x8103C63C..0x8103E944). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <plugin.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "../../shell/plugins.h"
#include "game_plugin.h"
#include "game_appinfo.h"

/* Sony calls the condition's push_back here, where it inlines the rest */
PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbValue, paf::allocator<LsdbValue>)

using namespace paf;

/* the #_up_status values a license gives */
#define GAME_UP_STATUS_MAX	3

/* the key the eboot attribute is also kept under in a re-registered record */
#define LSDB_KEY_EBOOT_ATTR_COPY	1

/*
 * The type of the license in the license folder of the application at path (its "app/" made
 * "license/app/" unless the path is in license/ already), for accountId (0x8103C63C).
 */
static int gameRifGetLicenseType(const string &path, const SceUInt64 *accountId, int *type,
				 int userData)
{
	string dir;

	if (path.find("license/", 0, sce_paf_strlen("license/")) == string::npos) {
		dir = path;

		string::size_type pos = dir.find("app/", 0, sce_paf_strlen("app/"));

		if (pos == string::npos)
			return GAME_ERROR_NO_DRIVE;

		dir.replace(pos, 4, "license/app/", 0, 12);
	} else {
		dir = path;
	}

	GameRifInfo *rif = new GameRifInfo(userData);

	if (rif == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = rif->Load(string(dir.c_str()), accountId, true);

	if (ret < 0) {
		delete rif;
		return ret;
	}

	*type = rif->m_licenseType;
	delete rif;

	return 0;
}

/*
 * The application's #_up_status set to status: its record's title ID, paths, category, LiveArea
 * path and eboot attribute registered again with the new status (0x8103C860).
 */
int gameAppSetUpStatus(const char *titleId, SceUInt32 status, int userData)
{
	if (titleId == NULL || status == 0 || status > GAME_UP_STATUS_MAX)
		return GAME_ERROR_INVALID_ARGUMENT;

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator end = info->end();
	LsdbAppInfo::iterator it = info->find(LSDB_KEY_TITLE_ID);

	if (it == end)
		return GAME_ERROR_NO_RECORD;

	string id(LsdbValueText(it->second));

	it = info->find(LSDB_KEY_ORG_PATH);
	if (it == end)
		return GAME_ERROR_NO_RECORD;

	string orgPath(LsdbValueText(it->second));

	it = info->find(LSDB_KEY_CATEGORY);
	if (it == end)
		return GAME_ERROR_NO_RECORD;

	string category(LsdbValueText(it->second));

	it = info->find(LSDB_KEY_UP_STATUS);
	if (it == end)
		return GAME_ERROR_NO_RECORD;

	int oldStatus = (int)gameLsdbValueInteger(it->second);

	if (oldStatus == 0)
		return GAME_ERROR_BAD_STORAGE;
	if (oldStatus == (int)status)
		return 0;

	it = info->find(LSDB_KEY_LIVEAREA_PATH);
	if (it == end)
		return GAME_ERROR_NO_RECORD;

	string liveAreaPath(LsdbValueText(it->second));

	it = info->find(LSDB_KEY_EBOOT_ATTR);
	if (it == end)
		return GAME_ERROR_NO_RECORD;

	SceUInt32 ebootAttr = (SceUInt32)gameLsdbValueInteger(it->second);
	GameAppRegistration reg;

	if (reg.info.get() == NULL || reg.entry.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	reg.liveAreaState = 0;

	gameLsdbValueSetText((*reg.info)[LSDB_KEY_CATEGORY], category);
	gameLsdbValueSetText((*reg.info)[LSDB_KEY_ORG_PATH], orgPath);
	gameLsdbValueSetText((*reg.info)[LSDB_KEY_TITLE_ID], id);
	gameLsdbValueSetInteger((*reg.info)[LSDB_KEY_UP_STATUS], (int)status);
	gameLsdbValueSetText((*reg.info)[LSDB_KEY_LIVEAREA_PATH], liveAreaPath);
	gameLsdbValueSetInteger((*reg.info)[LSDB_KEY_EBOOT_ATTR_COPY], ebootAttr);
	gameLsdbValueSetInteger((*reg.info)[LSDB_KEY_EBOOT_ATTR], ebootAttr);

	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);

	bool keepLiveArea = false;
	bool updated = false;

	ret = gameAppPrepareRegister(&reg.info, &keepLiveArea, (string *)userData);
	if (ret >= 0) {
		ret = gameAppRegister(&reg, 1, keepLiveArea, false, &updated);
		if (ret >= 0) {
			ret = gameAppUpdate(&reg.info, updated, false);
			if (ret >= 0)
				ret = 0;
		}
	}

	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);

	return ret;
}

/*
 * The #_up_status of a game whose ATTRIBUTE has the license bit set from its license, and
 * topmenu_plugin told (0x8103D648).
 */
int gameAppUpdateUpStatus(const char *titleId, int userData)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret < 0)
		return ret;

	if (info->size() == 0)
		return GAME_ERROR_NO_RECORD;

	LsdbAppInfo::iterator end = info->end();
	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ATTRIBUTE);
	SceUInt64 attribute = 0;

	if (it != end)
		attribute = gameLsdbValueInteger(it->second);

	if (!(attribute & GAME_ATTRIBUTE_UP_STATUS_LICENSE))
		return 0;

	string category;

	it = info->find(LSDB_KEY_CATEGORY);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	category = LsdbValueText(it->second);

	if (!gameCategoryIsGame(category))
		return GAME_ERROR_BAD_STORAGE;

	it = info->find(LSDB_KEY_ORG_PATH);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));
	SceUInt64 accountId = 0;

	if (gameNpGetAccountId(&accountId) < 0)
		accountId = 0;

	int type = 0;

	ret = gameRifGetLicenseType(string(orgPath.c_str()), &accountId, &type, userData);
	if (ret < 0)
		return ret;

	ret = gameAppSetUpStatus(titleId, type, userData);
	if (ret < 0)
		return ret;

	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface != NULL)
			((void (*)(const char *, int))iface->func[TOPMENU_PLUGIN_FUNC_UP_STATUS])(
				titleId, type);
	}

	return 0;
}

/*
 * The #_up_status of the application titleId, or of every application whose #_up_status is not
 * 0 when titleId is "", the appmeta work folders emptied after (0x8103DCF4).
 */
int gameAppUpdateUpStatusAll(const char *titleId, int userData)
{
	int ret;

	if (titleId[0] == '\0') {
		LsdbQueryCondition condition;

		condition.next = NULL;
		condition.op = LSDB_QUERY_OP_NOT_EQUAL;
		condition.key = LSDB_KEY_UP_STATUS;
		condition.values.push_back(LsdbValue());

		vector<string> titles;

		ret = gameFindTitles(&condition, &titles);
		if (ret >= 0) {
			SceUInt32 count = titles.size();

			for (SceUInt32 i = 0; i < count; i++)
				gameAppUpdateUpStatus(titles[i].c_str(), userData);
			ret = 0;
		}
	} else {
		ret = gameAppUpdateUpStatus(titleId, userData);
	}

	if (ret >= 0)
		ret = 0;

	gameRemoveDirectory("ur0:appmeta/new", 0, 0, 0, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
	gameRemoveDirectory("ux0:temp/appmeta", 0, 0, 0, true);

	return ret;
}

/*
 * A record's #_up_status from the license in the license folder path: 0 unless it is a game
 * whose ATTRIBUTE has the license bit and noLicense is not set (0x8103DEB6).
 */
int gameAppInfoSetUpStatus(LsdbAppInfoPtr *info, const string *path, const string *dir,
			   int userData, const bool *noLicense)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_ORG_PATH);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	SceUInt64 attribute = 0;

	it = (*info)->find(LSDB_KEY_ATTRIBUTE);
	if (it != end)
		attribute = gameLsdbValueInteger(it->second);

	if (gameCategoryIsGame(category)) {
		if ((attribute & GAME_ATTRIBUTE_UP_STATUS_LICENSE) && !*noLicense) {
			SceUInt64 accountId = 0;

			if (gameNpGetAccountId(&accountId) < 0)
				accountId = 0;

			GameRifInfo *rif = new GameRifInfo(userData);

			if (rif == NULL)
				return GAME_ERROR_NO_INFO_MEMORY;

			int ret = rif->Load(string(path->c_str()), &accountId, true);

			if (ret < 0) {
				delete rif;
				return ret;
			}

			SceUInt32 type = rif->m_licenseType;

			delete rif;

			if (type == 0)
				return GAME_ERROR_NO_UP_STATUS;

			gameLsdbValueSetInteger((**info)[LSDB_KEY_UP_STATUS], type);
		} else {
			gameLsdbValueSetInteger((**info)[LSDB_KEY_UP_STATUS], 0);
		}
	}

	return 0;
}

/*
 * A record's #_up_status from the SKU flags of license (size bytes): 0 unless it is a game
 * whose ATTRIBUTE has the license bit and noLicense is not set (0x8103E3C2).
 */
int gameAppInfoSetUpStatusFromLicense(SceNpDrmLicense *license, SceSize size, LsdbAppInfoPtr *info,
				      int unused, int userData, const bool *noLicense)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_ORG_PATH);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_TITLE_ID);
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	SceUInt64 attribute = 0;

	it = (*info)->find(LSDB_KEY_ATTRIBUTE);
	if (it != end)
		attribute = gameLsdbValueInteger(it->second);

	if (gameCategoryIsGame(category)) {
		if ((attribute & GAME_ATTRIBUTE_UP_STATUS_LICENSE) && !*noLicense) {
			SceUInt64 accountId = 0;

			if (gameNpGetAccountId(&accountId) < 0)
				accountId = 0;

			SceUInt32 skuFlags = 0;
			char rifName[GAME_RIF_NAME_SIZE];
			int ret = sceNpDrmGetRifNameForInstall(rifName, license, 1);

			if (ret >= 0) {
				GameRifInfo *rif = new GameRifInfo(userData);

				if (rif == NULL) {
					ret = GAME_ERROR_NO_INFO_MEMORY;
				} else {
					ret = rif->GetInfo(license, size, NULL, NULL, NULL,
							   &skuFlags, true);
					delete rif;
				}
			}
			if (ret < 0)
				return ret;

			if (skuFlags == 0)
				return GAME_ERROR_NO_UP_STATUS;

			gameLsdbValueSetInteger((**info)[LSDB_KEY_UP_STATUS], skuFlags);
		} else {
			gameLsdbValueSetInteger((**info)[LSDB_KEY_UP_STATUS], 0);
		}
	} else {
		gameLsdbValueSetInteger((**info)[LSDB_KEY_UP_STATUS], 0);
	}

	return 0;
}
