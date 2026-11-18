/*
 * The game plugin's PSP licenses: the name of a PSP application's license (its content ID and
 * ".rif"), read from its EBOOT.PBP or from an EDAT file, and the license removed (0x81059D68..). The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <vshbridge/vshbridge.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbValue, paf::allocator<LsdbValue>)

using namespace paf;

int gameTempDirPrepare(const char *dir);
const char *gamePathSkipDrive(const char *path);
int gameRenameIfExists(const char *src, const char *dst);

/* a removal asked to stop (0x81544108), and the folder it removes, of s_removePathLen characters */
static bool s_stopRequested;
static SceSize s_removePathLen;
static char s_removePath[SCE_IO_MAX_PATH_BUFFER_SIZE];

/* where a PSP EBOOT's data.psp keeps its content ID, and where a license file keeps its own */
#define GAME_PSP_DATA_CONTENT_ID_OFFSET	0x560
#define GAME_PSP_EDAT_CONTENT_ID_OFFSET	16
#define GAME_PSP_EDAT_MIN_SIZE		64

/* a file name as a removal step lists it */
#define GAME_PSP_NAME_SIZE		256

/* what a removal step reports: go on, the tree is removed, it failed */
#define GAME_PSP_REMOVE_CONTINUE	0
#define GAME_PSP_REMOVE_DONE		1
#define GAME_PSP_REMOVE_FAILED		2

/* where the PSP emulator keeps the licenses */
#define GAME_PSP_LICENSE_DIR		"ux0:pspemu/PSP/LICENSE"

/* a content ID as it is read, and the license name made of it */
#define GAME_PSP_CONTENT_ID_SIZE	48
#define GAME_PSP_RIF_NAME_SIZE		52

/* the key of a PSP application's game data folder, under ux0:pspemu/PSP/GAME, which others may share */
#define LSDB_KEY_GAMEDATA_ID		0x924BC88Du	/* "GAMEDATA_ID" */
/* the key of an application's save data folder, which others may share */
#define LSDB_KEY_INSTALL_DIR_SAVEDATA	0x10954174u	/* "INSTALL_DIR_SAVEDATA" */

/* where a PSP application's folders are moved to be removed */
#define GAME_PSP_GAME_DIR		"ux0:pspemu/PSP/GAME"
#define GAME_PSP_TEMP_DIR		"ux0:pspemu/temp/game"
#define GAME_UR0_TEMP_DIR		"ur0:temp/game"

/* where a PSM application's folders are, and are moved to be removed */
#define GAME_PSM_DIR			"ux0:psm/"
#define GAME_UX0_TEMP_DIR		"ux0:temp/game"

/* the category of a PSM application with a license and save data of its own, and its users */
#define GAME_PSM_CATEGORY_MBA		"mba"
#define GAME_PSM_USERS			64

/* the game card's writable partition (grw0:), formatted when its application is deleted */
#define GAME_GC_GAMERW_DEVICE		"sdstor0:gcd-lp-ign-gamerw"
#define GAME_GC_PARTITION_GAMERW	10
#define GAME_GC_FORMAT_BUF_SIZE		0x40000
#define GAME_GC_FORMAT_BUF_ALIGN	0x1000

/* a partition of the game card: its number, its MBR partition type and the clusters it is made with */
struct GameGcPartition {
	SceUInt8 index;
	SceUInt8 type;
	SceUInt16 reserved;
	SceUInt32 clusterSize;
	SceUInt32 reserved2;
};

static const GameGcPartition s_gcPartitions[] = {
	{ 9, 0x07, 0, 0x8000, 0 },
	{ GAME_GC_PARTITION_GAMERW, 0x07, 0, 0x8000, 0 },
	{ 13, 0xDA, 0, 0, 0 },
};

/*
 * PLACEHOLDER: what the FAT formatter linked into the shell (0x81383D8C, its messages name it
 * "FatFormat") is given, 48 bytes; the names are ours, from what the deletion stores.
 */
struct GameFatFormatParam {
	SceUInt32 reserved[2];
	const char *device;
	void *buf;
	SceSize bufSize;
	SceUInt32 clusterSize;
	SceUInt32 initialSize;		/* 512: only the partition table made */
	SceUInt32 reserved2;
	SceUInt32 fatTime;		/* the volume's date and time, as FAT keeps them */
	SceUInt32 reserved3[2];
	SceUInt32 fsKind;		/* from the partition type: 2 FAT16, 3 FAT32, 4 exFAT */
};

/*
 * the FAT formatter linked into the shell, not written yet (FatFormat is named by its messages;
 * the time helper's name is ours)
 */
extern "C" int FatFormat(GameFatFormatParam *param);		/* 0x81383D8C */
extern "C" SceUInt32 gameFatTimeFromDateTime(const SceDateTime *time);	/* 0x8137E2D8 */

/* how deep a PSP folder tree's removal goes, and the file names each of its steps reads */
#define GAME_PSP_REMOVE_MAX_DEPTH	512
#define GAME_PSP_REMOVE_NAMES		128

/* the bytes of two title IDs compared, the terminator with them */
#define GAME_PSP_TITLE_ID_LEN		10

/*
 * The name of a PSP application's license, from the content ID its EBOOT.PBP's data.psp holds
 * (0x81059D68).
 */
int gamePspEbootGetRifName(const char *path, char *rifName)
{
	int ret = 0;
	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);

	if (file.get() == NULL)
		return ret;

	SceInt64 size = file->GetFileSize();

	if (size < 0)
		return (int)size;

	GamePbpHeader header;
	int read = file->Read(&header, sizeof(header));

	if (read < 0) {
		file->Close();
		return read;
	}

	if (read != sizeof(header)) {
		file->Close();
		return GAME_ERROR_BAD_PBP;
	}

	if (header.magic[0] != '\0' || header.magic[1] != 'P' || header.magic[2] != 'B' ||
	    header.magic[3] != 'P') {
		file->Close();
		return GAME_ERROR_BAD_PBP;
	}

	SceUInt32 offset = header.dataPspOffset + GAME_PSP_DATA_CONTENT_ID_OFFSET;

	if (size < offset) {
		file->Close();
		return GAME_ERROR_BAD_PBP;
	}

	SceInt64 pos = file->Seek(offset, SCE_SEEK_SET);

	if (pos < 0) {
		file->Close();
		return (int)pos;
	}

	char contentId[GAME_PSP_CONTENT_ID_SIZE];

	read = file->Read(contentId, sizeof(contentId));
	if (read < 0) {
		file->Close();
		return read;
	}

	if (read != sizeof(contentId))
		ret = GAME_ERROR_BAD_PBP;

	file->Close();
	sce_paf_snprintf(rifName, GAME_PSP_RIF_NAME_SIZE, "%s.rif", contentId);

	return ret;
}

/* The name of a PSP license, from the content ID the EDAT file at path holds (0x8105A142). */
int gamePspEdatGetRifName(const char *path, char *rifName)
{
	LocalFileStat stat;
	int ret = LocalFile::Getstat(path, &stat);

	if (ret < 0)
		return ret;

	if (stat.st_size < GAME_PSP_EDAT_MIN_SIZE)
		return GAME_ERROR_BAD_PBP;

	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);

	if (file.get() == NULL)
		return ret;

	file->Seek(GAME_PSP_EDAT_CONTENT_ID_OFFSET, SCE_SEEK_SET);

	char contentId[GAME_PSP_CONTENT_ID_SIZE];
	int read = file->Read(contentId, sizeof(contentId));

	if (read < 0) {
		file->Close();
		return read;
	}

	if (read != sizeof(contentId))
		ret = GAME_ERROR_BAD_PBP;

	file->Close();
	sce_paf_snprintf(rifName, GAME_PSP_RIF_NAME_SIZE, "%s.rif", contentId);

	return ret;
}

/*
 * The license of the PSP application or content at path (an EBOOT.PBP or an EDAT file) removed
 * from the PSP emulator's licenses, when it is there (0x8105A346).
 */
int gamePspLicenseRemove(const string &path)
{
	LocalFileStat stat;
	char rifName[64];
	int ret;

	if (path.find("EBOOT.PBP") != string::npos)
		ret = gamePspEbootGetRifName(path.c_str(), rifName);
	else if (path.find(".EDAT") != string::npos || path.find(".edat") != string::npos)
		ret = gamePspEdatGetRifName(path.c_str(), rifName);
	else
		return 0;

	if (ret != 0)
		return 0;

	string licensePath = string(GAME_PSP_LICENSE_DIR) + "/" + string(rifName);

	if (LocalFile::Getstat(licensePath.c_str(), &stat) != 0)
		return 0;

	ret = LocalFile::RemoveFile(licensePath.c_str());
	if (ret < 0)
		return ret;

	return ret;
}

/*
 * The names of at most maxNames files (not folders) of the folder at path, each in GAME_PSP_NAME_SIZE
 * bytes of names; count is what was read (inlined into gamePspTreeRemoveStep).
 */
static int gamePspDirReadFileNames(const char *path, char *names, unsigned int maxNames,
				   unsigned int *count)
{
	Dir dir;
	DirEnt entry;
	unsigned int n = 0;
	int ret;

	*count = 0;

	ret = dir.Open(path);
	if (ret >= 0) {
		for (;;) {
			ret = dir.Read(&entry);
			if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY) {
				ret = 0;
				*count = n;
				break;
			}

			if (ret < 0)
				break;

			if (entry.type != DirEnt::Type_Dir) {
				sce_paf_strncpy(names, entry.name.c_str(), GAME_PSP_NAME_SIZE - 1);
				names[GAME_PSP_NAME_SIZE - 1] = '\0';
				n++;
				names += GAME_PSP_NAME_SIZE;
				if (n == maxNames) {
					ret = 0;
					*count = n;
					break;
				}
			}
		}
	}

	dir.Close();

	return ret;
}

/*
 * One step of a PSP folder tree's removal (0x8105A760): the files of the folder at path removed,
 * maxNames at a time with their licenses, then the step goes down into its first folder (depth
 * one more) or, once the folder is empty, removes it and goes back up. path holds pathLen
 * characters; names is the room for maxNames file names. GAME_PSP_REMOVE_DONE once the tree is
 * removed, GAME_PSP_REMOVE_FAILED with the error in result.
 */
int gamePspTreeRemoveStep(char *path, SceSize *pathLen, char *names, unsigned int maxNames,
			  int *depth, int *result)
{
	LocalFileStat stat;
	Dir dir;
	DirEnt entry;
	LocalFileStat entryStat;
	char filePath[SCE_IO_MAX_PATH_BUFFER_SIZE + 1];
	int ret;

	if (result != NULL)
		*result = 0;

	SceSize baseLen = sce_paf_strlen(path);

	for (;;) {
		if (s_stopRequested) {
			ret = GAME_ERROR_ABORTED;
			break;
		}

		unsigned int count;

		ret = gamePspDirReadFileNames(path, names, maxNames, &count);
		if (ret < 0)
			break;

		if (count == 0) {
			ret = 0;
			break;
		}

		sce_paf_strncpy(filePath, path, SCE_IO_MAX_PATH_BUFFER_SIZE);

		const char *name = names;

		for (unsigned int i = 0; i < count; i++) {
			if (baseLen + sce_paf_strlen(name) + 1 > SCE_IO_MAX_PATH_BUFFER_SIZE) {
				ret = GAME_ERROR_TOO_DEEP;
				goto removed;
			}

			if (sce_paf_strlen(filePath) != SCE_IO_MAX_PATH_BUFFER_SIZE)
				sce_paf_strncat(filePath, "/",
						SCE_IO_MAX_PATH_BUFFER_SIZE - sce_paf_strlen(filePath));
			if (sce_paf_strlen(filePath) != SCE_IO_MAX_PATH_BUFFER_SIZE)
				sce_paf_strncat(filePath, name,
						SCE_IO_MAX_PATH_BUFFER_SIZE - sce_paf_strlen(filePath));

			ret = gamePspLicenseRemove(string(filePath));
			if (ret < 0)
				goto removed;

			ret = LocalFile::RemoveFile(filePath);
			if (ret < 0)
				goto removed;

			filePath[baseLen] = '\0';
			name += GAME_PSP_NAME_SIZE;
		}
	}

removed:
	if (ret < 0)
		goto failed;

	if (dir.Open(path) < 0) {
		ret = LocalFile::Getstat(path, &stat);
		if (ret < 0)
			goto failed;

		if (stat.st_mode & SCE_STM_FREG) {
			ret = LocalFile::RemoveFile(path);
			if (ret < 0)
				goto failed;
		}

		*result = 0;
		return GAME_PSP_REMOVE_DONE;
	}

	for (;;) {
		if (s_stopRequested) {
			ret = GAME_ERROR_ABORTED;
			goto failed;
		}

		ret = dir.Read(&entry);
		if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY)
			break;

		if (ret < 0)
			goto failed;

		if (entry.type != DirEnt::Type_Dir ||
		    sce_paf_strncmp(entry.name.c_str(), ".", 2) == 0 ||
		    sce_paf_strncmp(entry.name.c_str(), "..", 3) == 0)
			continue;

		dir.Close();

		SceSize size = entry.name.size();
		const char *dirName = entry.name.c_str();

		if (*pathLen + size + 1 <= SCE_IO_MAX_PATH_BUFFER_SIZE) {
			if (sce_paf_strlen(path) != SCE_IO_MAX_PATH_BUFFER_SIZE)
				sce_paf_strncat(path, "/",
						SCE_IO_MAX_PATH_BUFFER_SIZE - sce_paf_strlen(path));
			sce_paf_memcpy(&path[*pathLen + 1], dirName, size);
			ret = 0;
			path[*pathLen + 1 + size] = '\0';
			*pathLen += size + 1;
		} else {
			ret = GAME_ERROR_TOO_DEEP;
		}

		if (ret < 0)
			goto failed;

		(*depth)++;
		*result = 0;
		return GAME_PSP_REMOVE_CONTINUE;
	}

	dir.Close();

	if (*depth > 0) {
		ret = LocalFile::Getstat(path, &stat);
		if (ret < 0)
			goto failed;

		ret = Dir::Remove(path);
		if (ret < 0)
			goto failed;
	} else {
		ret = 0;
	}

	(*depth)--;

	{
		char *slash = sce_paf_strrchr(path, '/');

		if (slash != NULL) {
			*slash = '\0';
			*pathLen = slash - path;
		}
	}

	if (*depth >= 0) {
		*result = 0;
		return GAME_PSP_REMOVE_CONTINUE;
	}

	*result = ret;
	return GAME_PSP_REMOVE_DONE;

failed:
	*result = ret;
	return GAME_PSP_REMOVE_FAILED;
}

/* path as one file name: each '/' made a '_' (0x8105AD9A) */
string gamePathToFileName(const char *path)
{
	string name(path);

	for (unsigned int i = 0; i < name.size(); i++) {
		if (name.at(i) == '/')
			name.at(i) = '_';
	}

	return name;
}

/*
 * Whether no application but titleId has gamedataId as its PSP game data folder; true also when the
 * applications cannot be asked (0x8105ADF6).
 */
bool gamePspGamedataIsOnlyFor(const char *gamedataId, const char *titleId)
{
	if (gamedataId == NULL)
		return false;

	vector<string> titleIds;
	AppInfoQClient client;

	SceLsdb_A6D8772C(&client);

	if (SceLsdb_E90B81CD(&client) == 0) {
		bool other = false;

		{
			LsdbQueryCondition condition;

			condition.key = LSDB_KEY_GAMEDATA_ID;
			condition.next = NULL;
			condition.values.push_back(LsdbValue(gamedataId));

			SceLsdb_76B75713(&client, &condition, &titleIds, 0);

			for (unsigned int i = 0; i < titleIds.size(); i++) {
				if (sce_paf_strncmp(titleIds[i].c_str(), titleId,
						    GAME_PSP_TITLE_ID_LEN) != 0) {
					other = true;
					break;
				}
			}
		}

		if (other) {
			SceLsdb_E2101B03(&client);
			return false;
		}
	}

	SceLsdb_E2101B03(&client);

	return true;
}

/*
 * The PSP application titleId deleted (0x8105AFA4): its folder, its PSP game folder when no other
 * application has it, and its appmeta moved into the temporary folders, then those removed, the
 * PSP one a step at a time with the licenses of its files.
 */
int gamePspAppDelete(const char *titleId, LsdbAppInfoPtr *info)
{
	LocalFileStat stat;
	const char *orgPath = LsdbValueText((**info)[LSDB_KEY_ORG_PATH]);

	if (gamePathCheckUnder(orgPath, GAME_PSP_GAME_DIR "/") < 0)
		return GAME_ERROR_PATH_OUTSIDE;

	const char *gamedataId = LsdbValueText((**info)[LSDB_KEY_GAMEDATA_ID]);
	int ret;

	ret = gameTempDirPrepare(GAME_PSP_TEMP_DIR);
	if (ret < 0)
		return ret;

	ret = gameTempDirPrepare(GAME_UR0_TEMP_DIR);
	if (ret < 0)
		return ret;

	if (orgPath != NULL && gamePathSkipDrive(orgPath) != NULL) {
		string path(orgPath);
		string name = gamePathToFileName(gamePathSkipDrive(orgPath));
		string dst = string(GAME_PSP_TEMP_DIR) + "/" + name;

		ret = gameRenameIfExists(orgPath, dst.c_str());
		if (ret < 0)
			return ret;
	}

	if (gamedataId != NULL && gamedataId[0] != '\0' && gamePspGamedataIsOnlyFor(gamedataId, titleId)) {
		string src = string(GAME_PSP_GAME_DIR) + "/" + gamedataId;
		string dst = string(GAME_PSP_TEMP_DIR) + "/" + gamedataId;

		ret = gameRenameIfExists(src.c_str(), dst.c_str());
		if (ret < 0)
			return ret;
	}

	{
		string appmeta = string("appmeta/") + titleId;
		string src = string("ur0:") + appmeta;
		string name = gamePathToFileName(appmeta.c_str());
		string dst = string(GAME_UR0_TEMP_DIR) + "/" + name;

		ret = gameRenameIfExists(src.c_str(), dst.c_str());
		if (ret < 0)
			return ret;
	}

	s_stopRequested = false;
	s_removePathLen = gamePathGetLength(GAME_PSP_TEMP_DIR, sizeof(s_removePath) + 1);

	if (s_removePathLen <= sizeof(s_removePath)) {
		sce_paf_strncpy(s_removePath, GAME_PSP_TEMP_DIR, sizeof(s_removePath));

		int depth = 0;
		int result = 0;
		char *names = new char[GAME_PSP_REMOVE_NAMES * GAME_PSP_NAME_SIZE];

		if (names != NULL) {
			for (;;) {
				if (depth >= GAME_PSP_REMOVE_MAX_DEPTH) {
					result = GAME_ERROR_TOO_DEEP;
					break;
				}

				if (s_stopRequested) {
					result = GAME_ERROR_ABORTED;
					break;
				}

				ret = gamePspTreeRemoveStep(s_removePath, &s_removePathLen, names,
							    GAME_PSP_REMOVE_NAMES, &depth, &result);
				if (ret == GAME_PSP_REMOVE_DONE || ret == GAME_PSP_REMOVE_FAILED)
					break;
			}

			delete[] names;

			ret = result;
			if (ret >= 0)
				return gameRemoveDirectoryContents(GAME_UR0_TEMP_DIR, NULL, NULL, NULL,
								   true);
		} else {
			ret = GAME_ERROR_NOT_READY;
		}
	} else {
		ret = GAME_ERROR_INVALID_ARGUMENT;
	}

	return ret;
}

/*
 * Whether no application but titleId has savedataDir as its save data folder; true also when the
 * applications cannot be asked (0x8105BD36).
 */
bool gameSavedataDirIsOnlyFor(const char *savedataDir, const char *titleId)
{
	if (savedataDir == NULL)
		return false;

	vector<string> titleIds;
	AppInfoQClient client;

	SceLsdb_A6D8772C(&client);

	if (SceLsdb_E90B81CD(&client) == 0) {
		bool other = false;

		{
			LsdbQueryCondition condition;

			condition.key = LSDB_KEY_INSTALL_DIR_SAVEDATA;
			condition.next = NULL;
			condition.values.push_back(LsdbValue(savedataDir));

			SceLsdb_76B75713(&client, &condition, &titleIds, 0);

			for (unsigned int i = 0; i < titleIds.size(); i++) {
				if (sce_paf_strncmp(titleIds[i].c_str(), titleId,
						    GAME_PSP_TITLE_ID_LEN) != 0) {
					other = true;
					break;
				}
			}
		}

		if (other) {
			SceLsdb_E2101B03(&client);
			return false;
		}
	}

	SceLsdb_E2101B03(&client);

	return true;
}

/*
 * The PSM application titleId deleted (0x8105BEE4): its folder, its license and the save data of
 * each user when no other application has them, and its appmeta moved into the temporary
 * folders, then those removed.
 */
int gamePsmAppDelete(const char *titleId, LsdbAppInfoPtr *info)
{
	LocalFileStat stat;
	const char *orgPath = LsdbValueText((**info)[LSDB_KEY_ORG_PATH]);

	if (gamePathCheckUnder(orgPath, GAME_PSM_DIR) < 0)
		return GAME_ERROR_PATH_OUTSIDE;

	int ret = gameTempDirPrepare(GAME_UX0_TEMP_DIR);

	if (ret != SCE_ERROR_ERRNO_ENODEV && ret != SCE_ERROR_ERRNO_ENOENT && ret < 0)
		return ret;

	ret = gameTempDirPrepare(GAME_UR0_TEMP_DIR);
	if (ret < 0)
		return ret;

	if (orgPath != NULL && gamePathSkipDrive(orgPath) != NULL) {
		string path(orgPath);
		string dst = string(GAME_UX0_TEMP_DIR) + "/" + string("psm_") + titleId;

		ret = gameRenameIfExists(orgPath, dst.c_str());
		if (ret < 0)
			return ret;
	}

	const char *category = LsdbValueText((**info)[LSDB_KEY_CATEGORY]);

	if (category != NULL &&
	    sce_paf_strncmp(category, GAME_PSM_CATEGORY_MBA, sizeof(GAME_PSM_CATEGORY_MBA)) == 0) {
		string license = string("license/app/") + titleId;
		string licenseSrc = string("ux0:") + license;
		string licenseName = gamePathToFileName(license.c_str());
		string licenseDst = string(GAME_UX0_TEMP_DIR) + "/" + licenseName;

		if (LocalFile::Exists(licenseSrc.c_str())) {
			ret = LocalFile::RenameFile(licenseSrc.c_str(), licenseDst.c_str());
			if (ret < 0)
				return ret;
		}

		const char *savedataDir = LsdbValueText((**info)[LSDB_KEY_INSTALL_DIR_SAVEDATA]);

		if (savedataDir != NULL && gameCheckTitleId(savedataDir) < 0)
			return GAME_ERROR_BAD_SAVEDATA_DIR;

		if (gameSavedataDirIsOnlyFor(savedataDir, titleId)) {
			for (int i = 0; i < GAME_PSM_USERS; i++) {
				char userNo[4];

				sce_paf_snprintf(userNo, 3, "%02d", i);

				string savedata = string("user/") + string(userNo) + string("/savedata/") +
						  string(savedataDir);
				string src = string("ux0:") + savedata;
				string name = gamePathToFileName(savedata.c_str());
				string dst = string(GAME_UX0_TEMP_DIR) + "/" + name;

				ret = gameRenameIfExists(src.c_str(), dst.c_str());
				if (ret < 0)
					return ret;
			}
		}
	}

	{
		string appmeta = string("appmeta/") + titleId;
		string src = string("ur0:") + appmeta;
		string name = gamePathToFileName(appmeta.c_str());
		string dst = string(GAME_UR0_TEMP_DIR) + "/" + name;

		ret = gameRenameIfExists(src.c_str(), dst.c_str());
		if (ret < 0)
			return ret;
	}

	ret = gameRemoveDirectoryContents(GAME_UX0_TEMP_DIR, NULL, NULL, NULL, false);
	if (ret == SCE_ERROR_ERRNO_ENODEV || ret == SCE_ERROR_ERRNO_ENOENT || ret >= 0)
		return gameRemoveDirectoryContents(GAME_UR0_TEMP_DIR, NULL, NULL, NULL, true);

	return ret;
}

/* the game card's partition of number index, or NULL (inlined) */
static inline const GameGcPartition *gameGcPartitionFind(unsigned int index)
{
	for (unsigned int i = 0; i < sizeof(s_gcPartitions) / sizeof(s_gcPartitions[0]); i++) {
		if (s_gcPartitions[i].index == index)
			return &s_gcPartitions[i];
	}

	return NULL;
}

/*
 * The game card application titleId deleted (0x8105CF9C): its cache moved and removed, the game
 * card's writable partition formatted again when the application is there, and its appmeta moved
 * into the temporary folder and removed.
 */
int gameGcAppDelete(const char *titleId, LsdbAppInfoPtr *info)
{
	LocalFileStat stat;
	const char *orgPath = LsdbValueText((**info)[LSDB_KEY_ORG_PATH]);

	if (orgPath == NULL)
		return GAME_ERROR_NO_VALUE;

	int ret = gameTempDirPrepare(GAME_UR0_TEMP_DIR);

	if (ret < 0)
		return ret;

	ret = gameTempDirPrepare(GAME_UX0_TEMP_DIR);

	if (ret == 0) {
		string cache = string("cache/") + titleId;
		string src = string("ux0:") + cache;
		string name = gamePathToFileName(cache.c_str());
		string dst = string(GAME_UX0_TEMP_DIR) + "/" + name;

		if (LocalFile::Exists(src.c_str())) {
			ret = LocalFile::RenameFile(src.c_str(), dst.c_str());
			if (ret < 0)
				return ret;
		}

		ret = gameRemoveDirectoryContents(GAME_UX0_TEMP_DIR, NULL, NULL, NULL, true);
		if (ret < 0)
			return ret;
	}

	LocalFileStat orgStat;

	if (LocalFile::Getstat(orgPath, &orgStat) == 0) {
		vshIoUmount(GAME_MOUNT_ID_GRW0, 0, 0, 0);

		const GameGcPartition *partition = gameGcPartitionFind(GAME_GC_PARTITION_GAMERW);

		if (partition != NULL) {
			unsigned int type = partition->type;
			SceUInt32 clusterSize = partition->clusterSize;
			SceDateTime time;

			ret = sceRtcGetCurrentClockLocalTime(&time);
			if (ret >= 0) {
				SceUInt32 fatTime = gameFatTimeFromDateTime(&time);
				void *buf = sce_paf_memalign(GAME_GC_FORMAT_BUF_ALIGN, GAME_GC_FORMAT_BUF_SIZE);

				if (buf == NULL) {
					ret = SCE_ERROR_ERRNO_ENOMEM;
				} else {
					GameFatFormatParam param;

					param.device = GAME_GC_GAMERW_DEVICE;
					param.buf = buf;
					param.reserved[0] = 0;
					param.reserved[1] = 0;
					param.bufSize = GAME_GC_FORMAT_BUF_SIZE;
					param.clusterSize = clusterSize;
					param.fatTime = fatTime;
					param.reserved3[0] = 0;
					param.reserved3[1] = 0;
					param.initialSize = 0;
					param.reserved2 = 0;

					switch (type) {
					case 0x06:
						param.fsKind = 2;
						break;
					case 0x07:
						param.fsKind = 4;
						break;
					case 0x0B:
						param.fsKind = 3;
						break;
					default:
						param.fsKind = 0;
						break;
					}

					ret = FatFormat(&param);
					if (ret >= 0)
						sceIoSync(param.device, 0);

					sce_paf_free(buf);
				}
			}

			if (ret < 0 && ret != SCE_ERROR_ERRNO_ENODEV)
				return ret;
		}

		vshIoMount(GAME_MOUNT_ID_GRW0, NULL, 0, 0, 0, 0);
	}

	{
		string appmeta = string("appmeta/") + titleId;
		string src = string("ur0:") + appmeta;
		string name = gamePathToFileName(appmeta.c_str());
		string dst = string(GAME_UR0_TEMP_DIR) + "/" + name;

		ret = gameRenameIfExists(src.c_str(), dst.c_str());
		if (ret < 0)
			return ret;
	}

	return gameRemoveDirectoryContents(GAME_UR0_TEMP_DIR, NULL, NULL, NULL, true);
}

/* dir emptied, or made when it is not there (0x8105DBE2) */
int gameTempDirPrepare(const char *dir)
{
	if (Dir::IsExist(dir))
		return gameRemoveDirectoryContents(dir, NULL, NULL, NULL, false);

	return Dir::CreateRecursive(dir, 6);
}

/* what follows the drive of path, or NULL when it names none (0x8105DC0E) */
const char *gamePathSkipDrive(const char *path)
{
	const char *colon = sce_paf_strrchr(path, ':');

	if (colon != NULL)
		return colon + 1;

	return NULL;
}

/* src renamed to dst, when it is there (0x8105DC22) */
int gameRenameIfExists(const char *src, const char *dst)
{
	int ret = 0;

	if (LocalFile::Exists(src))
		return LocalFile::RenameFile(src, dst);

	return ret;
}

/*
 * The save data folder savedataDir of user userNo moved into ux0:temp/game, when it is there
 * (0x8105DC40).
 */
int gameSavedataMoveToTemp(const string &savedataDir, const string &userNo)
{
	string savedata = string("user/") + userNo + string("/savedata/") + savedataDir;
	string src = string("ux0:") + savedata;
	string name = gamePathToFileName(savedata.c_str());
	string dst = string(GAME_UX0_TEMP_DIR) + "/" + name;
	int ret = gameRenameIfExists(src.c_str(), dst.c_str());

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * Whether no application but titleId names addcontDir among its additional contents' folders;
 * true also when the applications cannot be asked (0x8105E202).
 */
bool gameAddcontDirIsOnlyFor(const char *addcontDir, const char *titleId)
{
	if (addcontDir == NULL)
		return false;

	unsigned int keys[] = {
		LSDB_KEY_INSTALL_DIR_ADDCONT,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_1, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_2,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_3, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_4,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_5, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_6,
		LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_7,
	};
	vector<string> titleIds;
	AppInfoQClient client;

	SceLsdb_A6D8772C(&client);

	if (SceLsdb_E90B81CD(&client) == 0) {
		for (int i = 0; i < 8; i++) {
			bool other = false;

			{
				LsdbQueryCondition condition;

				condition.key = keys[i];
				condition.next = NULL;
				condition.values.push_back(LsdbValue(addcontDir));

				SceLsdb_76B75713(&client, &condition, &titleIds, 0);

				for (unsigned int j = 0; j < titleIds.size(); j++) {
					if (sce_paf_strncmp(titleIds[j].c_str(), titleId,
							    GAME_PSP_TITLE_ID_LEN) != 0) {
						other = true;
						break;
					}
				}
			}

			if (other) {
				SceLsdb_E2101B03(&client);
				return false;
			}
		}
	}

	SceLsdb_E2101B03(&client);

	return true;
}
