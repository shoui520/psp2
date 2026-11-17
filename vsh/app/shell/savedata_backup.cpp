/*
 * The local save data backups: a title's backup in a user's savedata_backup folder, its
 * status, and its removal. A backup's name is the title ID, its number and the account's check
 * word: "PCSA00000_00000001_0123ABCD.xxx" (31 characters). The file's and every name here are
 * ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
#include <std/stdlib.h>
#include <std/string>
#include <dir/dir.h>
#include <file/localfile.h>
#include <common/string_util.h>

#include "savedata_backup.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* the errors (values only) */
#define SHELL_SAVEDATA_BACKUP_ERROR_NOT_FOUND		-2146416420	/* 0x801048DC */
#define SHELL_SAVEDATA_BACKUP_ERROR_INVALID_TITLE_ID	-2146416415	/* 0x801048E1 */
#define SHELL_SAVEDATA_BACKUP_ERROR_OTHER_ACCOUNT	-2146416412	/* 0x801048E4 */

/* vshAppMgrCloudDataSetupKey's error when no account is signed in (value only) */
#define SHELL_CLOUD_DATA_KEY_ERROR_NO_ACCOUNT		-2139094912	/* 0x80800080 */

/* the ux0 user folder sceAppMgrGetUserDirPath gives ("ux0:user/00") */
#define SHELL_USER_DIR_ID		2
#define SHELL_USER_DIR_SIZE		32
#define SHELL_USER_COUNT		64

/* a backup's name: the title ID, '_', its number, '_', the check word, '.', its extension */
#define SHELL_TITLE_ID_LENGTH		9
#define SHELL_TITLE_ID_LETTERS		4
#define SHELL_SAVEDATA_BACKUP_NAME_LENGTH	31

/* a title ID: four capital letters and five digits */
static inline bool shellIsTitleId(const string &titleId)
{
	int i;

	if (titleId.length() != SHELL_TITLE_ID_LENGTH)
		return false;
	for (i = 0; i < SHELL_TITLE_ID_LETTERS; i++) {
		if (titleId[i] < 'A' || titleId[i] > 'Z')
			return false;
	}
	for (i = SHELL_TITLE_ID_LETTERS; i < SHELL_TITLE_ID_LENGTH; i++) {
		if (titleId[i] < '0' || titleId[i] > '9')
			return false;
	}
	return true;
}

/* the title's backup file in dir: its name */
static int shellSavedataBackupFind(const string &dir, const string &titleId, string *name)
{
	int ret = 0;
	int err;

	name->clear();

	Dir folder;

	err = folder.Open(dir.c_str());
	if (err < 0)
		return err;
	for (;;) {
		DirEnt entry;

		err = folder.Read(&entry);
		if (err == 0 && entry.type == DirEnt::Type_File &&
		    entry.name.length() == SHELL_SAVEDATA_BACKUP_NAME_LENGTH &&
		    entry.name.substr(0, SHELL_TITLE_ID_LENGTH) == titleId) {
			*name = entry.name;
			break;
		}
		if (err != 0)
			break;
	}
	folder.Close();
	if (name->length() == 0)
		ret = SHELL_SAVEDATA_BACKUP_ERROR_NOT_FOUND;
	return ret;
}

/* the check word of a backup's name, between the last '_' and the last '.' */
static inline string shellSavedataBackupCheckWord(const string &name)
{
	if (name.length() == SHELL_SAVEDATA_BACKUP_NAME_LENGTH) {
		string::size_type sep = name.rfind("_", string::npos, sce_paf_strlen("_"));
		string::size_type dot = name.rfind(".", string::npos, sce_paf_strlen("."));

		if (sep != string::npos && dot != string::npos && sep != dot)
			return name.substr(sep + 1, dot - sep - 1);
	}
	return string("");
}

int shellSavedataBackupGetInfo(const string &titleId, ShellSavedataBackupInfo *info)
{
	int ret;

	sce_paf_memset(info, 0, sizeof(*info));
	if (!shellIsTitleId(titleId))
		return SHELL_SAVEDATA_BACKUP_ERROR_INVALID_TITLE_ID;

	string dir;
	char userDir[SHELL_USER_DIR_SIZE];

	dir.clear();
	sce_paf_memset(userDir, 0, sizeof(userDir));
	ret = sceAppMgrGetUserDirPath(SHELL_USER_DIR_ID, userDir, sizeof(userDir));
	if (ret < 0)
		return ret;
	dir = userDir;
	dir += "/savedata_backup";

	string name;

	ret = shellSavedataBackupFind(dir, titleId, &name);
	if (ret < 0)
		return ret;

	string path = dir + string("/") + name;

	ret = LocalFile::Getstat(path.c_str(), &info->stat);
	if (ret < 0)
		return ret;

	/* the number between the first and the last '_', without its leading zeros */
	int number = 0;

	if (name.length() == SHELL_SAVEDATA_BACKUP_NAME_LENGTH) {
		string::size_type first = name.find("_");
		string::size_type last = name.rfind("_", string::npos, sce_paf_strlen("_"));

		if (first != string::npos && last != string::npos && first != last) {
			string digits = name.substr(first + 1, last - first - 1);
			string::size_type i;

			for (i = 0; i < digits.length(); i++) {
				if (digits[i] != '0')
					break;
			}
			digits = digits.substr(i, digits.length() - i);
			number = sce_paf_atoll(digits.c_str());
		}
	}
	info->number = number;

	/* a backup of no account's, or of the signed-in account's or this unit's offline ID */
	string checkWord = shellSavedataBackupCheckWord(name);

	if (checkWord == "00000000")
		return 0;

	string current;
	SceAppMgrCloudDataKey key;
	SceUInt32 word = 0;

	current.clear();
	sce_paf_memset(&key, 0, sizeof(key));
	ret = vshAppMgrCloudDataSetupKey(&key);
	if (ret != 0) {
		if (ret == SHELL_CLOUD_DATA_KEY_ERROR_NO_ACCOUNT) {
			SceUInt64 offlineId = 0;
			SceUInt32 offlineWord = 0;

			ret = vshAppMgrLocalBackupGetOfflineId(&offlineId, &offlineWord);
			if (ret >= 0)
				word = offlineWord;
		}
		if (ret < 0)
			return ret;
	}
	current = common::FormatString("%08X", word);
	if (checkWord != current)
		return SHELL_SAVEDATA_BACKUP_ERROR_OTHER_ACCOUNT;
	return 0;
}

/* a backup file removed: renamed to "_<name>" first, which an earlier attempt may have left */
static int shellSavedataBackupRemove(const string &dir, const string &name)
{
	string path = dir + string("/") + name;
	string removing = dir + string("/_") + name;
	int ret;

	if (LocalFile::Exists(removing.c_str())) {
		ret = LocalFile::RemoveFile(removing.c_str());
		if (ret < 0)
			return ret;
	}
	ret = LocalFile::RenameFile(path.c_str(), removing.c_str());
	if (ret < 0)
		return ret;
	ret = LocalFile::RemoveFile(removing.c_str());
	if (ret < 0)
		return ret;
	return 0;
}

/* the title's backup in a user's folder ("00".."63") removed */
static int shellSavedataBackupDelete(const string &titleId, const string &user)
{
	string dir = common::FormatString("%s/%s/%s", "ux0:user", user.c_str(), "savedata_backup");
	string name;
	int ret;

	ret = shellSavedataBackupFind(dir, titleId, &name);
	if (ret < 0)
		return ret;
	ret = shellSavedataBackupRemove(dir, name);
	if (ret < 0)
		return ret;
	return 0;
}

int shellSavedataBackupDeleteCurrentUser(const string &titleId)
{
	int ret;

	if (!shellIsTitleId(titleId))
		return SHELL_SAVEDATA_BACKUP_ERROR_INVALID_TITLE_ID;

	char userDir[SHELL_USER_DIR_SIZE];
	string user;

	user.clear();
	sce_paf_memset(userDir, 0, sizeof(userDir));
	ret = sceAppMgrGetUserDirPath(SHELL_USER_DIR_ID, userDir, sizeof(userDir));
	if (ret < 0)
		return ret;
	{
		string path(userDir);
		string::size_type sep = path.rfind("/", string::npos, sce_paf_strlen("/"));

		user = path.substr(sep + 1, path.length() - sep);
	}
	ret = shellSavedataBackupDelete(titleId, user);
	if (ret < 0)
		return ret;
	return 0;
}

int shellSavedataBackupDeleteAll(const string &titleId)
{
	int i;

	if (!shellIsTitleId(titleId))
		return SHELL_SAVEDATA_BACKUP_ERROR_INVALID_TITLE_ID;
	for (i = 0; i < SHELL_USER_COUNT; i++) {
		string user = common::FormatString("%02d", i);

		if (shellSavedataBackupDelete(titleId, user) < 0)
			continue;
	}
	return 0;
}

int shellSavedataBackupDeleteUser(const string &titleId, int user)
{
	int ret;

	if (!shellIsTitleId(titleId))
		return SHELL_SAVEDATA_BACKUP_ERROR_INVALID_TITLE_ID;

	string name = common::FormatString("%02d", user);

	ret = shellSavedataBackupDelete(titleId, name);
	if (ret < 0)
		return ret;
	return 0;
}
