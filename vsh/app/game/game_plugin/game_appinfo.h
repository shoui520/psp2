/*
 * game_appinfo.h: the game plugin's helpers that read an application's livespace_db
 * information. The names are ours (no record names them).
 */
#ifndef _GAME_APPINFO_H
#define _GAME_APPINFO_H

#include <rtc.h>
#include <cxml/cxml.h>
#include <file/file.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "ftt_license.h"

/* an application's information from livespace_db (0x81018666) */
int gameAppGetInfo(const char *titleId, LsdbAppInfoPtr *info);

/* the title IDs of the applications that meet conditions (game_lsdb.cpp, 0x81018736) */
int gameFindTitles(LsdbQueryCondition *conditions, paf::vector<paf::string> *titleIds);

/* the title IDs of the applications whose text value of key is text (game_lsdb.cpp, 0x8101878A) */
int gameFindTitlesByText(paf::vector<paf::string> *titleIds, unsigned int key, const char *text);

/*
 * Where an application is installed (0x810111C0), from its attribute and original path: 1 and
 * 2 for gro0:, 3 for ux0:, 4 for pd0:.
 */
int gameAppGetStorage(const LsdbAppInfoPtr *info, int *storage);

/* where gameAppGetStorage says an application is installed */
#define GAME_STORAGE_GRO0		1
#define GAME_STORAGE_GRO0_OTHER		2
#define GAME_STORAGE_UX0		3
#define GAME_STORAGE_PD0		4

/* keys livespace_db_internal.h does not name yet (FNV-1a of the names, lsdb_keys.h) */
#ifndef LSDB_KEY_MTIME
#define LSDB_KEY_MTIME		0x18B45AA7u	/* "#_mtime": the title's last change */
#endif
#ifndef LSDB_KEY_LIBLOCATION
#define LSDB_KEY_LIBLOCATION	0xA9492ECAu	/* "#LIBLOCATION" */
#endif
#ifndef LSDB_KEY_APPCONF
#define LSDB_KEY_APPCONF	0x5142196Bu	/* "#appconf" */
#endif
#ifndef LSDB_KEY_APPSIZE
#define LSDB_KEY_APPSIZE	0x61CC8AE0u	/* "#appsize" */
#endif
#ifndef LSDB_KEY_DISP_VER
#define LSDB_KEY_DISP_VER	0x47C0E8ACu	/* "_disp_ver" */
#endif
#ifndef LSDB_KEY_TARGET_APP_VER
#define LSDB_KEY_TARGET_APP_VER	0x21EAAE0Eu	/* "TARGET_APP_VER" */
#endif
#ifndef LSDB_KEY_INSTALL_DIR_SAVEDATA
#define LSDB_KEY_INSTALL_DIR_SAVEDATA	0x10954174u	/* "INSTALL_DIR_SAVEDATA" */
#endif
#ifndef LSDB_KEY_INSTALL_DIR_ADDCONT
#define LSDB_KEY_INSTALL_DIR_ADDCONT	0xF11F5930u	/* "INSTALL_DIR_ADDCONT" */
#endif
#define LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_1	0x043E0A78u	/* "INSTALL_DIR_ADDCONT_ADD_1" */
#define LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_2	0x073E0F31u	/* "INSTALL_DIR_ADDCONT_ADD_2" */
#define LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_3	0x063E0D9Eu	/* "INSTALL_DIR_ADDCONT_ADD_3" */
#define LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_4	0x093E1257u	/* "INSTALL_DIR_ADDCONT_ADD_4" */
#define LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_5	0x083E10C4u	/* "INSTALL_DIR_ADDCONT_ADD_5" */
#define LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_6	0x0B3E157Du	/* "INSTALL_DIR_ADDCONT_ADD_6" */
#define LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_7	0x0A3E13EAu	/* "INSTALL_DIR_ADDCONT_ADD_7" */
#ifndef LSDB_KEY_INSTALL_DIR_CACHE
#define LSDB_KEY_INSTALL_DIR_CACHE	0x35EF415Bu	/* "INSTALL_DIR_CACHE0" */
#endif
#define LSDB_KEY_SAVEDATA_MAX_SIZE	0x98264E03u	/* "SAVEDATA_MAX_SIZE" */
#define LSDB_KEY_REGION_DENY		0xCC003704u	/* "REGION_DENY" */
#ifndef LSDB_KEY_IMPORT_SAVEDATA_INFO
#define LSDB_KEY_IMPORT_SAVEDATA_INFO	0x6DBCFF31u	/* "IMPORT_SAVEDATA_INFO" */
#endif
#ifndef LSDB_KEY_ATTRIBUTE2
#define LSDB_KEY_ATTRIBUTE2	0xE320FB47u	/* "ATTRIBUTE2" */
#endif
#define LSDB_KEY_SYSTEM_ROOT_VER	0x291BA145u	/* "PSP2_SYSTEM_ROOT_VER" */
#ifndef LSDB_KEY_VERSION
#define LSDB_KEY_VERSION	0xD3BBB9B7u	/* "VERSION" */
#endif

/*
 * PLACEHOLDER name: where a PlayStation Mobile application's record keeps its runtime's APP_VER;
 * no string of the firmware hashes to it
 */
#define LSDB_KEY_PSM_RUNTIME_APP_VER	0xE84B97C1u

/* a PSP application's param.sfo keys */
#define LSDB_KEY_PSP_SYSTEM_VER	0x16A32D32u	/* "PSP_SYSTEM_VER" */
#define LSDB_KEY_UPDATER_VER	0x8F1D7630u	/* "UPDATER_VER" */
#define LSDB_KEY_DISC_NUMBER	0x604B15D4u	/* "DISC_NUMBER" */
#define LSDB_KEY_DISC_TOTAL	0x4A5E0009u	/* "DISC_TOTAL" */
#define LSDB_KEY_USE_USB	0x3607E5DDu	/* "USE_USB" */

/* PLACEHOLDER name: the ATTRIBUTE bit that marks an application with a manual */
#define GAME_ATTRIBUTE_MANUAL	0x200000

/* PLACEHOLDER name: the ATTRIBUTE bit set when an application's app folder is not there */
#define GAME_ATTRIBUTE_NO_APP_DIR	0x1

/* an ATTRIBUTE bit: the application's license decides its #_up_status */
#define GAME_ATTRIBUTE_UP_STATUS_LICENSE	0x400

/* a value's integer, 0 when it is a text */
inline SceInt64 gameLsdbValueInteger(const LsdbValue &value)
{
	return value.type == LSDB_VALUE_INTEGER ? value.integer : 0;
}

/* a value's text as a string, "" when it has none */
inline paf::string gameLsdbValueString(const LsdbValue &value)
{
	if (value.type == LSDB_VALUE_TEXT && value.text != NULL)
		return *value.text;
	return paf::string("");
}

/* a value set to a copy of text (Sony's code inlines this everywhere) */
inline void gameLsdbValueSetText(LsdbValue &value, const char *text)
{
	if (value.type == LSDB_VALUE_TEXT && value.text != NULL)
		*value.text = text;
	else
		value.text = new paf::string(text);
	value.type = LSDB_VALUE_TEXT;
}

/* a value set to a copy of text */
inline void gameLsdbValueSetText(LsdbValue &value, const paf::string &text)
{
	if (value.type == LSDB_VALUE_TEXT && value.text != NULL)
		*value.text = text;
	else
		value.text = new paf::string(text);
	value.type = LSDB_VALUE_TEXT;
}

/* a value set to an integer */
inline void gameLsdbValueSetInteger(LsdbValue &value, SceInt64 integer)
{
	if (value.type == LSDB_VALUE_TEXT) {
		delete value.text;
		value.text = NULL;
	}
	value.integer = integer;
	value.type = LSDB_VALUE_INTEGER;
}

/* a value set to a copy of another */
inline void gameLsdbValueAssign(LsdbValue &value, const LsdbValue &other)
{
	if (&value == &other)
		return;

	if (other.type == LSDB_VALUE_INTEGER) {
		if (value.type == LSDB_VALUE_TEXT) {
			delete value.text;
			value.text = NULL;
		}
		value.integer = other.integer;
	} else if (other.type == LSDB_VALUE_TEXT) {
		if (value.type == LSDB_VALUE_TEXT && value.text != NULL)
			*value.text = *other.text;
		else
			value.text = other.text != NULL ? new paf::string(*other.text) :
							  new paf::string("");
	}
	value.type = other.type;
}

/*
 * PLACEHOLDER: an application as the game plugin hands it to applauncher_plugin to register
 * (built on the stack by 0x810161EA). Only the fields this file's functions use are written
 * out; the rest is laid out from the caller's frame.
 */
/*
 * What applauncher_plugin shows of an application (0x58 bytes, built by 0x810161EA).
 * PLACEHOLDER: the fields this file does not set are not read yet.
 */
struct GameAppEntryOther;	/* PLACEHOLDER: what the entry's shared pointer holds */

struct GameAppEntry {
	GameAppEntry() : otherWord1(0), otherWord2(0), otherWord3(0), storage(0), kind(0),
			 parentalLevel(0), otherInt(0)
	{
	}

	int otherWord1;
	int otherWord2;
	int otherWord3;
	paf::string otherText;
	paf::string iconPath;			/* icon0.png, or icon0.dds for EG and ME */
	int storage;				/* 2 for wda, 0 otherwise */
	paf::string titleId;
	int kind;				/* 2-5 from the attribute or the path, 1 for ME */
	paf::string contentLocation;		/* wda's CONTENT_LOCATION */
	int parentalLevel;
	int otherInt;
	paf::common::SharedPtr<GameAppEntryOther> other;
	int otherTail;
};

struct GameAppRegistration {
	GameAppRegistration() : entry(new GameAppEntry), info(new LsdbAppInfo),
				liveAreaState(0xFFFFFF), otherFlag(true), mode(0)
	{
	}

	paf::common::SharedPtr<GameAppEntry> entry;
	LsdbAppInfoPtr info;
	paf::vector<LsdbAppInfoPtr> related;	/* the related applications' informations */
	int liveAreaState;			/* 0xFFFFFF when built; 0 when it has none */
	bool otherFlag;
	int mode;				/* what the registration is for (1: an install) */
};

/*
 * SceLsdb's exports this plugin calls (livespace_db's appinfo_accessor.cpp; declared here until
 * livespace_db_internal.h declares them)
 */
extern "C" {
const char *sceLsdbGetStitle(const LsdbAppInfoPtr *app);
const char *sceLsdbGetTitle(const LsdbAppInfoPtr *app);
}

/* a caller's cancel test, polled while a long count runs: nonzero cancels */
typedef int (*GameCancelFunc)(void *arg);

/* an application's sizes, by what holds them, and their total */
struct GameAppSizes {
	SceUInt64 app;		/* the application's own folder */
	SceUInt64 patch;
	SceUInt64 addcont;
	SceUInt64 data;		/* save data, licenses and appmeta */
	SceUInt64 total;
	bool addcontEncrypted;	/* every additional content has its sce_pfs */
};

/* a license's status: what the caller fills (the path) and what gameLicenseGetStatus reads */
struct GameLicenseStatus {
	char path[128];
	SceUInt64 startTime;
	SceUInt64 expireTime;
	SceUInt64 remaining;		/* the time left until expireTime */
	int result;			/* the license check's result */
	int expireTextResult;
	bool valid;
	bool flag1;			/* PLACEHOLDER: bit 1 of the license's flags */
	char expireText[65];		/* expireTime as text, "-" when it does not fit */
};

/* the cluster size of path's device (0x8101890E); a folder tree's size in clusters (0x8101898E) */
int gameDeviceGetClusterSize(const char *path, SceUInt64 *clusterSize);
int gameDirGetSize(const char *path, GameCancelFunc cancel, void *arg, SceUInt64 *size);

/* a device's size and free space (game_lsdb.cpp, 0x8101C038) */
int gameDeviceGetSpace(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize);

/* the copies and removals of game_lsdb.cpp (0x8101ACA0, 0x8101AEA0, 0x8101BAC8) */
int gameCopyFileWithPool(const char *src, const char *dst, int arg2, int arg3, int arg6,
			 int arg7);
int gameMoveTree(const char *src, const char *dst, GameCancelFunc cancel, void *cancelArg,
		 paf::string *detail);
int gameRemoveDirectoryContents(const char *path, GameCancelFunc cancel, void *cancelArg,
				GameJobProgress *progress, bool mapErrors);

/* an application's sizes, by flags (0x8101A78A), and its license's status (0x8101BD12) */
int gameTitleGetSizes(const char *titleId, unsigned int flags, GameCancelFunc cancel, void *arg,
		      GameAppSizes *sizes);
int gameLicenseGetStatus(GameLicenseStatus *status);
/* the time as the system's text, "-" without an end (0x81343D8E). PLACEHOLDER: its name */
paf::wstring gameLicenseTimeToText(const SceUInt64 *time);

/* an application deleted (game_lsdb.cpp: 0x8101B058), and a part of it (0x8101B3FC) */
int gameAppDelete(const char *titleId);
int gameAppDeletePart(const char *titleId, int kind);

/* the card's own param.sfo (gro0:gc/param.sfo) loaded into info (0x8102C6B8) */
int gameGcLoadParamSfo(LsdbAppInfoPtr *info);

/*
 * A signed param.sfo's reader into an application's information (sfo_reader.cpp: constructor
 * 0x810329E0, Load 0x81032F44; 8 bytes).
 */
class GameSfoReader {
public:
	GameSfoReader(LsdbAppInfoPtr *info);
	virtual ~GameSfoReader();

	int Load(const paf::string &path);

private:
	int Check(LsdbAppInfoPtr *info);
	int Verify(const char *path, void *sfo, SceSize *size);
	int Read(const paf::string &path, LsdbAppInfoPtr *info);

	LsdbAppInfoPtr *m_info;
};

/* a title ID: nine upper-case letters and digits (0x8101D662) */
int gameCheckTitleId(const char *titleId);
int gamePathCheckUnder(const char *path, const char *dir);
SceSize gamePathGetLength(const char *path, SceSize max);

/* a param.sfo parameter's format (values only) */
#define GAME_SFO_FORMAT_BINARY		0x0004
#define GAME_SFO_FORMAT_TEXT		0x0204
#define GAME_SFO_FORMAT_INTEGER		0x0404

/*
 * PLACEHOLDER: one parameter of a param.sfo to load into an application's information: its
 * name, the key it is stored under (0: the name's hash), its SFO format, its length and the
 * largest length accepted.
 */
struct GameSfoParam {
	const char *name;
	unsigned int key;
	SceUInt16 format;
	SceUInt16 length;
	SceUInt16 maxLength;
	SceUInt16 reserved;
};

/*
 * sfo_param.cpp: a file read whole into a new buffer (0x8103397C), a param.sfo's parameters
 * stored into an application's information (0x81033D70), whether a version is "N.NN"
 * (0x81033DE0), "NN.NN" (0x81033E22) or "NN.NNN" (0x81033E70), 0 or GAME_ERROR_BAD_DATA, and a
 * version's digits as a number (0x81033ECC).
 */
/*
 * PLACEHOLDER names: the param.sfo library the shell links (0x81342xxx; also in gamedata_plugin,
 * gamecard_installer_plugin, NPXS10026 and NPXS10031; no Sony name found): a param.sfo opened
 * from memory and closed, a parameter found by its key, its length and largest length, and its
 * value.
 */
struct SfoHandle;

int sfoOpen(const void *sfo, SceSize size, SfoHandle **handle);
int sfoClose(SfoHandle *handle);
int sfoFindParam(SfoHandle *handle, const char *key, void **param);
int sfoGetParamInfo(SfoHandle *handle, void *param, const char **key, SceSize *length,
		    SceSize *maxLength);
int sfoGetParamValue(SfoHandle *handle, void *param, const void **value);

/*
 * PLACEHOLDER names, the same library: a new param.sfo made in memory (0x81342E04), a parameter
 * added with its format, length and largest length (0x813426CE), a parameter's value replaced
 * (0x81342D82), the size of the param.sfo written (0x8134331E), and the param.sfo written into
 * a buffer of that size (0x813432DE).
 */
int sfoCreate(SfoHandle **handle);
int sfoAddParam(SfoHandle *handle, const char *key, const void *value, SceUInt16 format,
		SceSize length, SceSize maxLength);
int sfoSetParamValue(SfoHandle *handle, void *param, const void *value, SceSize length);
int sfoGetSize(SfoHandle *handle, SceSize *size);
int sfoWrite(void *buf, SceSize size, SfoHandle *handle);

/* the library's error for a parameter the param.sfo does not have */
#define SFO_ERROR_NOT_FOUND	-2146412027	/* 0x80105A05 */

int gameFileReadAlloc(const char *path, SceInt64 offset, void **buf, SceSize size);
int gameSfoLoadParams(const void *sfo, SceSize size, const GameSfoParam *params,
		      unsigned int count, LsdbAppInfoPtr *info, bool checkLength);
int gameVersionCheckShort(const char *version);
int gameVersionCheck(const char *version);
int gameSystemVersionCheck(const char *version);
int gameVersionToNumber(const char *version, SceSize size, SceUInt32 *number);

/* whether the system runs the application of info, by its category (version_check.cpp) */
int gameAppCheckSystemVersion(const LsdbAppInfoPtr *info, paf::string *version);

/* the application's update checked for, with callback told of the result (version_check.cpp) */
int gameAppCheckUpdate(const LsdbAppInfoPtr *info, void (*callback)(int result, void *arg),
		       void *arg, bool gameCard, int mode, int priority);

/*
 * What a SELF's eboot parameters (its control information of type 6) give the shell, with the
 * check value of the saved copy (eboot_param.cpp). Only the fields the game plugin reads are
 * named.
 */
struct GameEbootParamData {
	SceUInt8 digest[0x20];			/* the saved copy's check value */
	SceUInt32 header;			/* the record's word at 0x10; 1 without a copy */
	SceUInt32 attribute;			/* stored as _eboot_attr */
	SceUInt32 phyMemsize;			/* stored as _phy_memsize */
	SceUInt32 appMemsize;			/* stored as _app_memsize */
	SceUInt8 otherParams[0xF0];
};

/*
 * A SELF's eboot parameters, kept with a check value in a saved copy (eboot_param.cpp:
 * constructor 0x81032FD4, destructors 0x81032FF8 and 0x81033006).
 */
class GameEbootParam {
public:
	GameEbootParam();
	virtual ~GameEbootParam();
	int Read(const paf::string &path);	/* from a SELF (0x8103328C) */
	int Load(const paf::string &path);	/* from a saved copy (0x81033456) */
	int Save(const paf::string &selfPath, const paf::string &path);	/* 0x8103375E */

	GameEbootParamData data;
};

size_t gameStrnlen(const char *str, size_t max);
int gameAppInfoSetTimeNow(unsigned int key, LsdbAppInfoPtr *info);
int gameAppInfoCopyTime(unsigned int key, const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);
int gameAppInfoCopyShellKeys(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);
int gameAppInfoCopyKept(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);
int gameAppInfoSetPromoteAttr(SceUInt32 attr, LsdbAppInfoPtr *info);
int gameAppUnregister(const char *const *titleId);
void gameAppInfoGetStitle(const LsdbAppInfoPtr *info, paf::string *stitle);
void gameAppInfoGetTitle(const LsdbAppInfoPtr *info, paf::string *title);
int gamePathGetDrive(const paf::string &path, paf::string *drive);
bool gameDriveIsNotData(const paf::string &drive);
int gameAppInfoSetLiveAreaPath(LsdbAppInfoPtr *info, const paf::string *appDir,
			       const paf::string *dir);
int gameAppInfoGetAttributeKind(const LsdbAppInfoPtr *info);
int gameAppInfoGetCategoryKind(const LsdbAppInfoPtr *info, int *kind);
int gameAppEntryFill(GameAppRegistration *app, const paf::string *appDir,
		     const paf::string *dir);
bool gameCategoryIsGame(const paf::string &category);
bool gameCategoryIsPatch(const paf::string &category);
bool gameTypeIsSpecial(int type);
bool gameCategoryIsEgMe(const paf::string &category);
void gameUnmountGameDrives(void);
/* an application's param.sfo, from its folder */
#define GAME_SFO_PATH		"/sce_sys/param.sfo"

float gameSumPercent(int last, const float *values, unsigned int count);

/*
 * PLACEHOLDER name, defined in another file of the shell: whether the system is older than the
 * version required (0x81363DA0)
 */
int shellSystemVersionCheck(SceUInt32 required, int *older);
int gameStitleKey(unsigned int language, unsigned int *key);
int gameTitleKey(unsigned int language, unsigned int *key);
int gameSfoLoad(const paf::string &path, const GameSfoParam *params, unsigned int count,
		LsdbAppInfoPtr *info);
int gameSfoLoadVerified(const paf::string &path, const GameSfoParam *params, unsigned int count,
			LsdbAppInfoPtr *info, const char *digest, SceSize digestLength);
int gameFileWrite(const paf::string &path, const void *buf, size_t size, paf::string *detail);
int gameFileRead(const paf::string &path, void *buf, size_t size, paf::string *detail);
int gameAppRegisterEnd(const LsdbAppInfoPtr *info);
int gameAppUpdate(const LsdbAppInfoPtr *info, bool flag, bool notifyTopmenu);
int gameThemeSetInfo(const LsdbTheme *theme,
		     const paf::common::SharedPtr<LsdbThemeInfo> *info);
bool gameAppVersionDiffers(const LsdbAppInfoPtr *info, const LsdbAppInfoPtr *other);
const GameSfoParam *gameSfoGetVersionParams(void);
unsigned int gameSfoGetVersionParamCount(void);
const GameSfoParam *gameSfoGetAppVerParams(void);
unsigned int gameSfoGetAppVerParamCount(void);
const GameSfoParam *gameSfoGetCheckParams(void);
unsigned int gameSfoGetCheckParamCount(void);
const GameSfoParam *gameSfoGetAppParams(void);
unsigned int gameSfoGetAppParamCount(void);
const GameSfoParam *gameSfoGetAttributeParams(void);
unsigned int gameSfoGetAttributeParamCount(void);
const GameSfoParam *gameSfoGetTitleParams(void);
unsigned int gameSfoGetTitleParamCount(void);
const GameSfoParam *gameSfoGetSaveDataParams(void);
unsigned int gameSfoGetSaveDataParamCount(void);
const GameSfoParam *gameSfoGetPspCheckParams(void);
unsigned int gameSfoGetPspCheckParamCount(void);
const GameSfoParam *gameSfoGetPspGameParams(void);
unsigned int gameSfoGetPspGameParamCount(void);
const GameSfoParam *gameSfoGetPspAppParams(void);
unsigned int gameSfoGetPspAppParamCount(void);
const GameSfoParam *gameSfoGetPspPs1Params(void);
unsigned int gameSfoGetPspPs1ParamCount(void);
const GameSfoParam *gameSfoGetPspDiscGameParams(void);
unsigned int gameSfoGetPspDiscGameParamCount(void);
const GameSfoParam *gameSfoGetPspGameBootParams(void);
unsigned int gameSfoGetPspGameBootParamCount(void);
const GameSfoParam *gameSfoGetPspPatchParams(void);
unsigned int gameSfoGetPspPatchParamCount(void);
const GameSfoParam *gameSfoGetPspUpdaterParams(void);
unsigned int gameSfoGetPspUpdaterParamCount(void);
const GameSfoParam *gameSfoGetPspWgParams(void);
unsigned int gameSfoGetPspWgParamCount(void);
const unsigned int *gameGetAddcontDirKeys(void);
unsigned int gameGetAddcontDirKeyCount(void);
bool gameCategoryIsGameName(const paf::string &category);
int gameAppIsOnUx0(const LsdbAppInfoPtr *info, bool *onUx0);
bool gameCategoryMatches(const paf::string &category, const paf::string &other);
bool gameCategoryIsDataPair(const paf::string &category, const paf::string &other);
int gamePathGetStorage(const paf::string &path, SceUInt64 flags);
int gamePatchPathGetStorage(const paf::string &path, SceUInt64 flags,
			    const paf::string &category);
int gameTitleGetStorage(const char *titleId, int *storage);
int gameAppInfoReplacePaths(LsdbAppInfoPtr *info, const paf::string &from, const paf::string &to);
int gameAppRegistrationReplacePaths(GameAppRegistration *app, const paf::string &from,
				    const paf::string &to);
int gameAppInfoGetSfoParams(const LsdbAppInfoPtr *info, const GameSfoParam **params,
			    unsigned int *count);
int gameSfoLoadAppVer(const paf::string &path, LsdbAppInfoPtr *info, const GameSfoParam **params,
		      unsigned int *count);
int gameSfoLoadAppVerVerified(const paf::string &path, LsdbAppInfoPtr *info,
			      const GameSfoParam **params, unsigned int *count,
			      const char *digest, SceSize digestLength);
int gameAttributeCheck(const paf::string &category, SceInt64 attribute);
int gameAppInfoCheckSfo(const LsdbAppInfoPtr *info);
int gameAttributeCompare(const LsdbAppInfoPtr *info, const LsdbAppInfoPtr *other);

int gameAppInfoCheckPatchSfo(const LsdbAppInfoPtr *info);
int gameAppInfoSetEbootParam(const paf::string &selfPath, LsdbAppInfoPtr info);
int gameAppInfoCopyEbootParam(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);
int gameAppInfoUpdateEbootParam(const paf::string &selfPath, LsdbAppInfoPtr *info,
				paf::string *detail);
int gameAppInfoLoadEbootParam(const paf::string &path, LsdbAppInfoPtr *info, paf::string *detail);
void gameAppInfoSetSaveDataDir(LsdbAppInfoPtr *info, const paf::string &dir);
void gameAppInfoSetAddcontDir(LsdbAppInfoPtr *info, const paf::string &dir);
void gameAppInfoSetCacheDir(LsdbAppInfoPtr *info, const paf::string &dir);

/* an application's IMPORT_SAVEDATA_INFO emptied (0x810145EE) */
void gameAppInfoClearImportSaveDataInfo(LsdbAppInfoPtr *info);

/* an application's #appconf, 0 without one (0x810146BE) */
SceInt64 gameAppInfoGetAppconf(const LsdbAppInfoPtr *info);

/* a patch's #appconf bits 0-3 for flags (0x81014724) */
SceUInt64 gamePatchAppconfLow(const LsdbAppInfoPtr *info, const paf::string &category,
			     SceUInt64 flags);

/* a patch's #appconf bits 5-7 for flags (0x810148D8) */
SceUInt64 gamePatchAppconfHigh(const LsdbAppInfoPtr *info, const paf::string &category,
			     SceUInt64 flags);


/* a patch's #LIBLOCATION, ATTRIBUTE and #appconf over its application's (0x81014A8E) */
int gameAppInfoSetPatchAttribute(const LsdbAppInfoPtr *app, LsdbAppInfoPtr *info);

/* selfPath and path from the application a BOOT_INSTALL_DIR names (0x81014EDA) */
int gameAppInfoUseBootSelfPath(const LsdbAppInfoPtr *info, paf::string *selfPath,
			       paf::string *path);

/* 1 for a game, 2 for NPXS10998, 0 for other data (0x810153C8) */
void gameAppInfoGetGameKind(const LsdbAppInfoPtr *info, const paf::string &titleId, int *kind);

/* an application's _bg_title_id from its background application (0x810154C0) */
void gameAppRegistrationSetBgTitleId(GameAppRegistration *app);

/* the drive an application's data goes to: grw0: or ux0: (0x81015614) */
int gamePathGetDataDrive(const paf::string &path, bool toUx0, paf::string *drive);

/* whether info differs from other in the system version or parental keys (0x810157BA) */
int gameAppInfoParentalDiffers(const LsdbAppInfoPtr *info, const LsdbAppInfoPtr *other);

/* the system version and parental keys an update keeps, copied (0x81015A7E) */
int gameAppInfoCopyParental(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);

/* an error when an application or its background application is running (0x81015ED8) */
int gameAppCheckNotRunning(const paf::string &titleId, const LsdbAppInfoPtr *info);

/* bit 11 of an application's #appconf set or cleared (0x81015FA2) */
void gameAppInfoSetAppconfBit11(LsdbAppInfoPtr *info, bool set);

/* bit 10 of an application's #appconf set or cleared (0x810160C6) */
void gameAppInfoSetAppconfBit10(LsdbAppInfoPtr *info, bool set);

/* a game card application's information moved to ux0: (0x810161EA) */
int gameAppInfoMoveToUx0(LsdbAppInfoPtr *info, bool doRegister, bool notify,
			 const paf::string *dir);

/* an application's PARENTAL_LEVEL, 0 without one (0x81016A74) */
int gameAppInfoGetParentalLevel(const LsdbAppInfoPtr *info);

/* whether a folder has a folder in it (0x81016ACC) */
bool gameDirHasSubDirs(const paf::string &path);

/* the number of icons on count pages from pageNo (0x81018892), and of the user's (0x810188D4) */
int gameCountIcons(int pageNo, unsigned int count);
int gameCountUserIcons(void);

/* bit 12 of an application's #appconf set or cleared (0x81016B8E) */
void gameAppInfoSetAppconfBit12(LsdbAppInfoPtr *info, bool set);

/* bit 2 of an application's #_promote_attr set or cleared (0x81016CB2) */
void gameAppInfoSetPromoteAttrBit2(LsdbAppInfoPtr *info, bool set);

/* whether bit 2 of an application's #_promote_attr is set (0x81016E02) */
bool gameAppInfoPromoteAttrBit2(const LsdbAppInfoPtr *info);

/*
 * PLACEHOLDER: the free trial license's file name, <name>.ftt (0x8101D49E, in a file of the
 * plugin not written yet)
 */
int gameFttLicenseName(paf::string *name);

/* whether an application has its free trial license file (0x81016E6C) */
bool gameAppHasFttLicense(const paf::string &titleId);

/* the game card application's title ID (0x810170A0) */
paf::string gameCardGetTitleId(void);

/* PLACEHOLDER: a folder's install information (0x8104639C, in a file of the shell not written yet) */
int gameDirReadStat(const paf::string &path, SceNpDrmInstallInfo *stat);

/* a folder's install information read into a cleared one (0x8101731A) */
int gameDirGetStat(const paf::string &path, SceNpDrmInstallInfo *stat);


#include "theme_copy.h"

/*
 * A theme's theme.xml read into its record (theme_reader.cpp, 0x81033FA4..0x81034650): its
 * package at packagePath mounted with the license, the record's ID the installPath.
 */
class GameThemeReader {
public:
	GameThemeReader(SceNpDrmInstallInfo *info, const paf::string *themeId,
			const paf::string *packagePath, const paf::string *installPath,
			bool extracted);
	virtual ~GameThemeReader();

	int Read(LsdbTheme *theme, paf::common::SharedPtr<LsdbThemeInfo> *info,
		 GameCancelFunc cancel, void *arg);

private:
	int GetSize(SceSize *size, GameCancelFunc cancel, void *arg);
	int LoadInfo(paf::common::SharedPtr<LsdbThemeInfo> *info);
	int ReadEmpty(LsdbTheme *theme, paf::common::SharedPtr<LsdbThemeInfo> *info,
		      GameCancelFunc cancel, void *arg);

	SceNpDrmInstallInfo *m_info;
	paf::string m_themeId;
	paf::string m_packagePath;
	paf::string m_installPath;
	bool m_extracted;
	paf::string m_mountPoint;	/* the package's, while it is read */
};

/* the steps of a theme's installation, for its progress */
#define GAME_THEME_INSTALL_STEPS	5

/*
 * PLACEHOLDER names: a theme's package installed into ux0:theme, or a theme already there
 * registered (0x81030A6C..0x81031C1C), 80 bytes.
 */
class GameThemeInstaller {
public:
	GameThemeInstaller(const GameJobParam *param, SceNpDrmInstallInfo *info,
			   GameJobProgress *progress, paf::string *errorTitleId, float scale);
	virtual ~GameThemeInstaller();

	int Run(bool (*cancel)(void *arg), void *arg);

	/* whether the system is new enough for the theme's param.sfo */
	int CheckSystemVersion(void);

	/* the theme's package installed, and the theme already in ux0:theme registered */
	int InstallPackage(bool (*cancel)(void *arg), void *arg);
	int InstallExtracted(bool (*cancel)(void *arg), void *arg);

	/* the progress after step, of GAME_THEME_INSTALL_STEPS */
	void SetProgress(int step)
	{
		float percent = gameSumPercent(step, m_weights, GAME_THEME_INSTALL_STEPS);

		if (m_progress.percent == NULL || percent < *m_progress.percent ||
		    percent > 100.0f)
			return;

		*m_progress.percent = percent;
		if (m_progress.func != NULL)
			m_progress.func(m_progress.arg, *m_progress.percent);
		m_progress.added = 0;
	}

	const GameJobParam *m_param;
	SceNpDrmInstallInfo *m_info;
	GameJobProgress m_progress;
	float m_weights[GAME_THEME_INSTALL_STEPS];	/* each step's part of the progress */
	paf::string *m_errorTitleId;
	paf::string m_reserved;			/* not set in this file */
	paf::string m_themeId;			/* "<title ID>-<the content ID's label>" */
};

typedef char GameThemeInstallerSize[sizeof(GameThemeInstaller) == 80 ? 1 : -1];

/*
 * PLACEHOLDER names: a package folder installed with the shell locked, from a GameJobParam
 * (0x810305E8..0x81030A6C), 1136 bytes.
 */
class GameParamJob {
public:
	GameParamJob(const GameJobParam *param, bool (*cancel)(void *arg), void *arg);
	virtual ~GameParamJob();

	int Run(void);

	/* the install information of the folder at path ("GetInstallInfo", Sony's name in 0.990) */
	int GetInstallInfo(const paf::string &path, SceNpDrmInstallInfo *info);

	/* whether the parameters name an application's package folder */
	int Check(void);

	GameJobParam m_param;
	int m_reserved;			/* not written: Sony clears the parameters only */
	bool (*m_cancel)(void *arg);
	void *m_cancelArg;
	int m_result;
	SceNpDrmInstallInfo m_info;
	paf::string m_errorTitleId;
	float m_percent;
	GameJobProgress m_progress;
	int m_padding;
};

typedef char GameParamJobSize[sizeof(GameParamJob) == 0x470 ? 1 : -1];

struct SceNpDrmLicense;

/* what kind of license a GameRifInfo read: by its SKU flags */
#define GAME_RIF_LICENSE_NONE		0	/* no SKU flags */
#define GAME_RIF_LICENSE_PERMANENT	1
#define GAME_RIF_LICENSE_TRIAL		3	/* more than one flag */

/*
 * An application's NpDrm license and its information (npdrm_license.cpp, 0x810349D4..0x81035AA8),
 * 48 bytes; the shell's other parts read it too.
 */
class GameRifInfo {
public:
	GameRifInfo(int userData);
	virtual ~GameRifInfo();

	int GetInfo(SceNpDrmLicense *license, SceSize size, SceUInt64 *accountId,
		    SceUInt64 *startTime, SceUInt64 *expireTime, SceUInt32 *skuFlags,
		    bool allowPeriod);
	int Load(const paf::string &dir, const SceUInt64 *accountId, bool allowPeriod);
	int GetPeriod(SceNpDrmLicense *license, SceSize size, char *contentId,
		      SceUInt64 *startTime, SceUInt64 *expireTime);

	SceUInt64 m_accountId;
	SceUInt64 m_startTime;
	SceUInt64 m_expireTime;
	int m_licenseType;	/* GAME_RIF_LICENSE_* */
	bool m_loaded;		/* Load found a license, for an account or not */
	int m_result;		/* what reading it returned */
	int m_userData;		/* the creator's, not read here */

private:
	int ReadFile(const paf::string &path, SceUInt64 *accountId, SceUInt64 *startTime,
		     SceUInt64 *expireTime, SceUInt32 *skuFlags, bool allowPeriod);
};

typedef char GameRifInfoSize[sizeof(GameRifInfo) == 48 ? 1 : -1];

/*
 * An application's #_up_status: its license's type, set from the license in its license folder
 * (up_status.cpp, 0x8103D648), and set to status in its record re-registered (0x8103C860).
 */
int gameAppUpdateUpStatus(const char *titleId, int userData);
int gameAppSetUpStatus(const char *titleId, SceUInt32 status, int userData);

/*
 * The #_up_status of the application titleId, or of every application with one when titleId is
 * "" (0x8103DCF4); an application's record given its #_up_status from the license in path
 * (0x8103DEB6) or from license (0x8103E3C2), 0 unless it is a game whose ATTRIBUTE has the
 * license bit and noLicense is not set.
 */
int gameAppUpdateUpStatusAll(const char *titleId, int userData);
int gameAppInfoSetUpStatus(LsdbAppInfoPtr *info, const paf::string *path,
			   const paf::string *dir, int userData,
			   const bool *noLicense);
int gameAppInfoSetUpStatusFromLicense(SceNpDrmLicense *license, SceSize size, LsdbAppInfoPtr *info,
				      int unused, int userData, const bool *noLicense);

/*
 * livearea_copier.cpp: an application's record prepared for its registration, keepLiveArea set
 * when its appmeta has a retail LiveArea (0x81050A8A); whether a card application has one
 * (0x8105159A)
 */
int gameAppPrepareRegister(LsdbAppInfoPtr *info, bool *keepLiveArea, paf::string *detail);
int gameAppInfoCheckAppmeta(LsdbAppInfoPtr *info, bool *hasAppmeta, paf::string *detail);

/* the SDK's glue over SceNpDrm's imports (0x81449964, 0x8144996C, 0x81449974) */
extern "C" int sceNpDrmGetRifName(char *rifName, SceUInt64 accountId);
extern "C" int sceNpDrmGetFixedRifName(char *rifName, SceUInt64 accountId);
extern "C" int sceNpDrmGetRifNameForInstall(char *rifName, const void *license, int flags);

/* the SDK's glue over ScePsmDrm's import (0x8144D538) */
extern "C" int scePsmDrmGetRifNameForInstall(char *rifName, const void *license);

/* the SDK's glue over SceVshBridge's import (0x81417C70) */
extern "C" int vshRtcGetCurrentSecureTick(SceRtcTick *tick);

/* the SDK's glue over SceVshBridge's import (0x81417C80) */
extern "C" int vshRtcSetSecureAlarmTick(const SceRtcTick *tick);

/* where the content ID is in a PSP game's DATA.PSP, and its size */
#define GAME_PSP_CONTENT_ID_OFFSET	0x560
#define GAME_PSP_CONTENT_ID_SIZE	48

/* the end of a license without one, and the license check's error of one past its end */
#define GAME_LICENSE_NO_EXPIRY		0x7FFFFFFFFFFFFFFFLL
#define GAME_LICENSE_ERROR_EXPIRED	-2139095023	/* 0x80800011 */

/* the size of a license's file name */
#define GAME_RIF_NAME_SIZE	48

/* the content types whose licenses are written: applications (21, 22) and themes (31) */
#define GAME_LICENSE_CONTENT_APP		21
#define GAME_LICENSE_CONTENT_APP_ADDCONT	22
#define GAME_LICENSE_CONTENT_THEME	31

/* license_file.cpp: an application's license files (0x8103E944..0x8103FB34) */
int gameFileWriteToDir(const void *buf, SceSize size, const paf::string *dir, const char *name,
		       paf::string *path, paf::string *detail);
int gameLicenseWrite(SceUInt32 contentType, const void *license, SceSize licenseSize,
		     const paf::string *dir, paf::string *path, paf::string *detail);
int gameFttRemove(const paf::string &titleId);
int gameLicenseActivate(const paf::string *path);
int gameFttInstall(const paf::string *path, const paf::string &titleId);

/*
 * A license read into a new buffer (0x81035528); the PocketStation's license checked
 * (0x810357F4); the path of an application's license in its license folder (0x81035860).
 */
int gameRifRead(const paf::string &path, SceNpDrmLicense **license, int *size);
int gamePspRifCheck(const paf::string &rifPath);
int gameRifGetPath(const paf::string &dir, const SceUInt64 *accountId, paf::string *path,
		   paf::string *detail);

/* a multi-disc game's entry of a disc list: its product code first */
struct GameDiscInfo {
	SceUInt8 data[256];
};

/*
 * A PlayStation game's multi-disc signature made from the PSP emulator's disc list
 * (ps1_discinfo.cpp, 0x81034650..0x81034964), read into a block of the plugin's pool
 * "SceGamePs1DiscInfoParser".
 */
class GamePs1DiscInfoParser {
public:
	GamePs1DiscInfoParser();
	virtual ~GamePs1DiscInfoParser();

	int Load(const paf::string &path);
	int Generate(const SceNpDrmInstallInfo *info, const paf::string &ebootPath,
		     void *signature);

private:
	paf::common::SharedPtr<paf::memory::PoolAllocator> m_pool;
	GameDiscInfo *m_discInfo;	/* the pool's block */
	SceSize m_size;			/* the block's size */
	int m_error;			/* what the construction failed with */
	SceUInt32 m_count;		/* the entries read */
};

/*
 * A PSP application's EBOOT.PBP signature (eboot_sig.cpp): made for an installation and written
 * to sigPath (0x810321A4), and checked, the old PSP signature only where allowPsp is set; the
 * check of an application's folder, the PSP emulator's work folder mounted (0x810326AA).
 */
int gameEbootSigGen(const SceNpDrmInstallInfo *info, const paf::string &ebootPath,
		    const void *sha256, const paf::string &sigPath, paf::string *detail);
int gameEbootCheck(const char *dir, paf::string *detail);

/*
 * PLACEHOLDER name: what other files check an EBOOT signature with; the application's title ID,
 * which its callers pass first, is not read (0x81032952)
 */
int gameEbootSigCheck(const paf::string &titleId, const paf::string &ebootPath,
		      const paf::string &sigPath, bool allowPsp, paf::string *detail);

/*
 * PLACEHOLDER name, defined in another file of the shell: whether the PSP emulator's list
 * (ur0:emu_list/list.dat, /CONFIG/PSPEMU enable_list) allows a title ID (0x813728C8)
 */
int gamePspEmuListAllows(const char *titleId);

/* whether a PSP content ID names a title the emulator allows (0x8101810A) */
int gamePspCheckContentId(const char *contentId);

/* whether a content ID is the PocketStation application's (0x81018184) */
bool gameIsPocketStation(const paf::string &contentId);

/* a PSP game's boot file: EBOOT.PBP, PBOOT.PBP or PARAM.PBP (0x81017D80) */
int gamePspGetBootPath(const paf::string &dir, paf::string *path);

/* a PSP game's game data ID (0x810178A8) */
int gamePspGetGameDataId(const LsdbAppInfoPtr *info, paf::string *gameDataId);

/* GAME_ERROR_APP_RUNNING when the foreground application shares a PSP game's data (0x81017A32) */
int gamePspCheckGameDataNotRunning(const paf::string &titleId, const LsdbAppInfoPtr *info);

/* GAME_ERROR_APP_RUNNING when the foreground application is titleId (0x81017850) */
int gameAppCheckNotForeground(const paf::string &titleId);

/* a PSP PBP file's header: its magic ("\0PBP"), version and the offsets of its files */
struct GamePbpHeader {
	SceUInt8 magic[4];
	SceUInt32 version;
	SceUInt32 paramSfoOffset;
	SceUInt32 icon0Offset;
	SceUInt32 icon1Offset;
	SceUInt32 pic0Offset;
	SceUInt32 pic1Offset;
	SceUInt32 snd0Offset;
	SceUInt32 dataPspOffset;
	SceUInt32 dataPsarOffset;
};

/* a PBP file's header, and whether it is a PS1 multi-disc image (0x81017422) */
int gamePbpReadHeader(const char *path, GamePbpHeader *header, bool *multiDisc);

/* the ATTRIBUTE_MINOR flags (0x8101733E) */
int gameAppInfoGetAttributeMinor(const LsdbAppInfoPtr *info, bool *valid,
				 bool *bit2Clear, bool *bit3Clear);

int gameAddcontCheck(const LsdbAppInfoPtr *app, const LsdbAppInfoPtr *info,
		     const paf::vector<LsdbAppInfoPtr> *related);
int gameAppRegister(GameAppRegistration *app, int mode, bool keepLiveArea, bool notifyTopmenu,
		    const void *param);

/*
 * What an application's folders on ux0 take, the folders dirs names (game_lsdb.cpp,
 * 0x8101C298).
 */
int gameAppGetDirsSize(const char *titleId, int dirs, GameCancelFunc cancel, void *arg,
		       SceUInt64 *size);

/* PLACEHOLDER: whether a title is one the shell keeps as its own, and which list holds it */
struct GamePresetTitle {
	bool isPreset;
	int kind;	/* 1: gamePresetGetTitleIds's list, 2: PCSC80012 */
};

/* an application's files deleted, by its kind (psp_license.cpp, 0x810609AC) */
int gameAppDeleteFiles(const char *titleId, LsdbAppInfoPtr *info);

/* a part of an application's files deleted, by its kind (psp_license.cpp, 0x81062BBA) */
int gameAppDeleteFilesPart(const char *titleId, LsdbAppInfoPtr *info, int part);

/* an application's appmeta in ur0 set aside, by mode (psp_license.cpp, 0x81062CC6) */
int gameUr0AppmetaSetup(const paf::string *titleId, int mode);

/* whether ux0:umass holds files titleId leaves behind (psp_license.cpp, 0x8106371E) */
int gameUmassHasFiles(const paf::string *titleId, bool *found);

/* an application's save data deleted, whose by what (psp_license.cpp, 0x81063906) */
int gameAppDataOperateFiles(const paf::string &titleId, LsdbAppInfoPtr *info, int what,
			    int userNo, int reserved, int reserved2);

/* the title IDs of the shell's own applications (game_lsdb.cpp, 0x8101CEFA) */
void gamePresetGetTitleIds(paf::vector<paf::string> *titleIds);

/* whether titleId is one the shell keeps as its own (game_lsdb.cpp, 0x8101D098) */
int gamePresetCheckTitle(const char *titleId, GamePresetTitle *preset);

/* PLACEHOLDER: what gamePresetGetDataSize reports of a title the shell keeps as its own */
struct GamePresetData {
	int kind;		/* GamePresetTitle's kind, 0 for any other title */
	SceUInt64 size;		/* what its data folder takes */
};

/* what the data of a title the shell keeps as its own takes (game_lsdb.cpp, 0x8101D2C8) */
int gamePresetGetDataSize(const char *titleId, GameCancelFunc cancel, void *arg,
			  GamePresetData *data);

/* the name of the user's free-trial license file (game_lsdb.cpp, 0x8101D49E) */
int gameNpDrmGetFttName(paf::string *fttName);

/* what gamePluginFttRead gives of an application's free-trial license */
struct GameFttInfo {
	char contentId[48];
	SceUInt64 time1;
	SceUInt64 time2;
	int otherField;
	bool flag;
};

/* the name of the user's license file (game_lsdb.cpp, 0x8101D3DE) */
int gameNpDrmGetRifName(paf::string *rifName);

/* an application's free-trial license read (game_main.cpp, 0x8102F198) */
int gamePluginFttRead(const char *titleId, GameFttInfo *info);

/*
 * A PSM application's app.info (psm_appinfo.cpp, constructor 0x810673D8, 204 bytes): the
 * document read into the application's record, and its icon and LiveArea background written out.
 */
class GamePsmAppInfo {
public:
	GamePsmAppInfo();
	virtual ~GamePsmAppInfo();

	/* the app.info at path read, with the record info, into appInfo (0x810681A2) */
	int Load(const char *path, const LsdbAppInfoPtr *info, LsdbAppInfoPtr *appInfo);

	/* the application's icon and background written to path (0x8106876C, 0x810693F0) */
	int WriteIcon(const paf::string &path);
	int WritePic(const paf::string &path);

private:
	int SetAttributeMinor(const LsdbAppInfoPtr *info, const bool &appXmlFormat,
			      const bool &psVitaTv, LsdbAppInfoPtr *appInfo);
	int ReadFeatureList(const paf::cxml::Element &list, LsdbAppInfoPtr *appInfo,
			    bool *psVitaTv);
	int ReadDeveloper(const paf::cxml::Element &developer);
	static int ReadParentalControl(const paf::cxml::Element &control,
				       LsdbAppInfoPtr *appInfo);
	static int GetLanguage(const paf::string &locale);
	int ReadShortName(const paf::cxml::Element &name, LsdbAppInfoPtr *appInfo);
	int ReadName(const paf::cxml::Element &name, LsdbAppInfoPtr *appInfo);
	int ReadVersions(const paf::cxml::Element &root, LsdbAppInfoPtr *appInfo);
	int ReadWebsite(const paf::cxml::Element &website);
	int WriteImage(const paf::string &name, const paf::string &path, void *buf,
		       SceSize bufSize);

	paf::cxml::Document m_doc;
	paf::common::SharedPtr<paf::File> m_file;	/* the app.info, read as its document needs */

public:
	paf::string m_author;		/* written under the title in the LiveArea */
	paf::string m_website;		/* the LiveArea's developer link */
	unsigned int m_defaultLanguage;	/* the language of the title shown by default */
};

typedef char GamePsmAppInfoSize[sizeof(GamePsmAppInfo) == 204 ? 1 : -1];

/* PLACEHOLDER names: whether ur0 has size bytes free (0x81055628, in another file) */
int gameUr0CheckFreeSpace(SceUInt64 size, paf::string *detail);

/*
 * PLACEHOLDER names: an image's extent, mode and format as paf::image gives them (0x81055462, in
 * another file), read from the image in buf.
 */
struct GameImageInfo {
	GameImageInfo() : width(0), height(0), mode(-1), format(0)
	{
	}

	int width;
	int height;
	int mode;		/* an ImageMode, ImageMode_NONE before it is read */
	int format;		/* an ImageFormat */
};

int gameImageGetInfo(paf::memory::HeapAllocator *allocator, void *buf, size_t size,
		     GameImageInfo *info);

/* PLACEHOLDER name: the same read from the image file at path (0x8105534E) */
int gameImageGetFileInfo(paf::memory::HeapAllocator *allocator, const paf::string &path,
			 GameImageInfo *info);

/*
 * PLACEHOLDER names: what an application's icon or background must be: its extent, its largest
 * file, its ImageMode and its ImageFormat (24 bytes)
 */
struct GameImageFormat {
	GameImageFormat(int _width, int _height, SceUInt64 _maxSize, int _mode, int _format)
		: width(_width), height(_height), maxSize(_maxSize), mode(_mode), format(_format)
	{
	}

	int width;
	int height;
	SceUInt64 maxSize;
	int mode;
	int format;
};

typedef char GameImageFormatSize[sizeof(GameImageFormat) == 24 ? 1 : -1];

/*
 * PLACEHOLDER names: a PSP icon, size bytes at offset of the file at path, read into buf and
 * checked: a PNG of no more than 144x80 pixels (0x8105388C)
 */
int gameFileReadAt(const paf::string *path, SceUInt32 offset, SceSize size, void *buf);

/*
 * PLACEHOLDER names: the parameters of a PBP's param.sfo read into sfo, those of its content
 * type (0x81049274) or those params names (0x810491E2), and a PSP param.sfo's parameters checked
 * (0x81048A7A)
 */
int gamePbpReadSfo(const paf::string *path, LsdbAppInfoPtr *sfo);
int gamePbpLoadSfo(const paf::string *path, const GameSfoParam *params, unsigned int count,
		   LsdbAppInfoPtr *info);
int gameSfoCheck(const LsdbAppInfoPtr *sfo);

/*
 * PLACEHOLDER names: an application's icon (0x81055DCE) or background (0x81055DD8) format, and an
 * image at src converted to it at dst (0x81055C04)
 */
const GameImageFormat *gameIcon0Format(void);
const GameImageFormat *gamePic0Format(void);
int gameImageConvert(const paf::string *src, const paf::string *dst,
		     const GameImageFormat *format, paf::string *detail);

/*
 * PLACEHOLDER names: a package's parameters copied from srcDir into an appmeta dstDir
 * (0x810556D6), and an EBOOT's parameters written to paramPath (0x81055DE4)
 */
int gameAppmetaCopyParams(const paf::string &srcDir, const paf::string &dstDir,
			  paf::string *detail);
int gameAppmetaWriteEbootParam(const paf::string *ebootPath, const paf::string *paramPath,
			       paf::string *detail);

/* PLACEHOLDER names: buf written as the file at path (0x81053B58) */
int gameFileWrite(const paf::string *path, const void *buf, SceSize size);

/*
 * PLACEHOLDER names: a PSP application's ICON0 in buf written to path as its icon, for its
 * category kind and attribute kind (0x81054930)
 */
int gameIconWrite(int kind, int attributeKind, const paf::string *path, const void *buf,
		  SceSize size);

/*
 * PLACEHOLDER names: an application's appmeta made in tempDir moved into the appmeta folder of
 * tempDir's drive, tempDir removed when removeTempDir is set (0x81054B14)
 */
int gameAppmetaCommit(const paf::string *tempDir, const paf::string *titleId, bool removeTempDir);

/*
 * PLACEHOLDER names: the appmeta made in ux0:appmeta/new moved to ux0:appmeta/titleId (0x81055138);
 * whether the home screen has room for another icon (0x810552D8); whether ux0: has size bytes
 * free when check is set, the drive in detail when it cannot tell (0x8105556A)
 */
int gameAppmetaCommitNew(const paf::string *titleId);
int gameLsdbHasRoom(bool *hasRoom);
int gameAppmetaCheckSpace(bool check, SceUInt64 size, paf::string *detail);

/* the room an application's appmeta needs on its drive */
#define GAME_APPMETA_SIZE		0x500000

/* the largest param.sfo (a card's, an application's) an appmeta keeps */
#define GAME_PARAM_SFO_MAX_SIZE		0x8000

/* the share of each step in an appmeta copier's progress: its checks, its copy and its commit */
#define GAME_APPMETA_WEIGHT_CHECK	0.1f
#define GAME_APPMETA_WEIGHT_COPY	0.8f
#define GAME_APPMETA_WEIGHT_COMMIT	0.1f

/*
 * What sets an application up in ur0 (its appmeta, its livearea) under the plugin's
 * SceGamePluginSetupUr0 mutex (ur0_setup.cpp, 0x81035AA8..0x8103C120), 96 bytes.
 */
class GameUr0Setup {
public:
	GameUr0Setup(const LsdbAppInfoPtr *info, int type);
	virtual ~GameUr0Setup();

	int Run(void);

private:
	int SetupPsmLiveArea(const paf::string &dir, GamePsmAppInfo *appInfo);
	int SetupPsmIcons(const paf::string &dir, GamePsmAppInfo *appInfo);
	int SetupPsm(void);
	int SetupPs1LiveArea(const paf::string &dir);
	int SetupPspLiveArea(const paf::string &dir);
	int SetupPspIcons(const paf::string &dir, const paf::string *ebootPath);
	int GetPspBootPath(paf::string *path);
	int SetupPsp(void);
	int SetupGameAppmeta(const paf::string &dir);
	int SetupGcParamSfo(const paf::string &dir);
	int SetupGameDir(const paf::string &dir, bool skipParams);
	int SetupGameAppmetaDir(bool skipParams);
	int SetupPromoteAppmeta(const paf::string *titleId);
	int Mount(void);
	bool IsGameCategory(void);
	int SetupGame(void);

	LsdbAppInfoPtr m_info;
	int m_type;			/* below 3 */
	int m_error;			/* the constructor's */
	paf::string m_titleId;
	paf::string m_category;
	paf::string m_orgPath;
	int m_storage;			/* gameAppGetStorage's */
	SceUInt32 m_upStatus;		/* #_up_status */
	bool m_mounted;
	bool m_dataMounted;
	paf::string m_mountPoint;
	paf::string m_detail;		/* where a step that failed was */
	bool m_promoteAttrBit2;
};

typedef char GameUr0SetupSize[sizeof(GameUr0Setup) == 96 ? 1 : -1];

/*
 * What a promote from parameters (gamePluginPromoteParam) is given, 164 bytes (names ours, from
 * the gamecard promoter's use).
 */
struct GamePromoteParam {
	char path[128];				/* the title's directory */
	LsdbAppInfoPtr appInfo;
	bool option;
	void (*progressFunc)(void *arg, float progress);	/* told the install's progress */
	void *progressArg;
	char errorTitleId[16];			/* the title ID an error names */
};

typedef char GamePromoteParamSize[sizeof(GamePromoteParam) == 164 ? 1 : -1];

/* an installed application's sizes on ux0, for what flags asks (game_lsdb.cpp, 0x8101C950) */
int gameAppGetInstalledSizes(const paf::string &titleId, unsigned int flags, GameCancelFunc cancel,
			     void *arg, GameAppSizes *sizes);

/* an operation on an application's data (game_lsdb.cpp, 0x8101B746; its words are not read yet) */
int gameAppDataOperate(const char *titleId, int arg1, int arg2, int arg3, int arg4);

/*
 * PLACEHOLDER names: the content import (content_import.cpp): a package folder at path queued
 * for import (0x810562E4), the queued folders of titleId's additional content installed
 * (0x810569FC), counted (0x8105718C) and removed (0x810576A2)
 */
int gameContentImportPrepare(const char *path);
int gameContentImportRun(const char *titleId);
int gameContentImportCheck(const char *titleId);
int gameContentImportDelete(const char *titleId);

#endif
