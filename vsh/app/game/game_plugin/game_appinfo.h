/*
 * game_appinfo.h: the game plugin's helpers that read an application's livespace_db
 * information. The names are ours (no record names them).
 */
#ifndef _GAME_APPINFO_H
#define _GAME_APPINFO_H

#include "../../shell/src/livespace_db/livespace_db_internal.h"

/* an application's information from livespace_db (0x81018666) */
int gameAppGetInfo(const char *titleId, LsdbAppInfoPtr *info);

/*
 * Where an application is installed (0x810111C0), from its attribute and original path: 1 and
 * 2 for gro0:, 3 for ux0:.
 */
int gameAppGetStorage(const LsdbAppInfoPtr *info, int *storage);

/* keys livespace_db_internal.h does not name yet (FNV-1a of the names, lsdb_keys.h) */
#ifndef LSDB_KEY_LIBLOCATION
#define LSDB_KEY_LIBLOCATION	0xA9492ECAu	/* "#LIBLOCATION" */
#endif
#ifndef LSDB_KEY_APPCONF
#define LSDB_KEY_APPCONF	0x5142196Bu	/* "#appconf" */
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

/* a value's integer, 0 when it is a text */
inline SceInt64 gameLsdbValueInteger(const LsdbValue &value)
{
	return value.type == LSDB_VALUE_INTEGER ? value.integer : 0;
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
	GameAppEntry() : otherHeader(), storage(0), kind(0), parentalLevel(0), otherInt(0)
	{
	}

	int otherHeader[3];
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
	SceUInt8 otherFields[4];
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

/* an application's sizes, by flags (0x8101A78A), and its license's status (0x8101BD12) */
int gameTitleGetSizes(const char *titleId, unsigned int flags, GameCancelFunc cancel, void *arg,
		      GameAppSizes *sizes);
int gameLicenseGetStatus(GameLicenseStatus *status);

/* an application deleted (game_lsdb.cpp: 0x8101B058), and a part of it (0x8101B3FC) */
int gameAppDelete(const char *titleId);
int gameAppDeletePart(const char *titleId, int kind);

/* the card's own param.sfo (gro0:gc/param.sfo) loaded into info (0x8102C6B8) */
int gameGcLoadParamSfo(LsdbAppInfoPtr *info);

/*
 * PLACEHOLDER: a param.sfo's reader into an application's information, defined in another file
 * of the plugin (constructor 0x810329E0, Load 0x81032F44; 8 bytes).
 */
class GameSfoReader {
public:
	GameSfoReader(LsdbAppInfoPtr *info);
	virtual ~GameSfoReader();

	int Load(const paf::string &path);

	LsdbAppInfoPtr *m_info;
};

/* a title ID: nine upper-case letters and digits (0x8101D662) */
int gameCheckTitleId(const char *titleId);

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
 * PLACEHOLDER names, defined in another file of the shell: a file read whole into a new buffer
 * (0x8103397C), and a param.sfo's parameters stored into an application's information
 * (0x81033D70).
 */
int gameFileReadAlloc(const char *path, SceInt64 offset, void **buf, SceSize size);
int gameSfoLoadParams(const void *sfo, SceSize size, const GameSfoParam *params,
		      unsigned int count, LsdbAppInfoPtr *info, bool checkLength);

/*
 * PLACEHOLDER names, defined in another file of the shell: whether a version is "NN.NN"
 * (0x81033E22) and whether it is "NN.NNN" (0x81033E70), 0 or GAME_ERROR_BAD_DATA.
 */
int gameVersionCheck(const char *version);
int gameSystemVersionCheck(const char *version);

/* whether the system runs the application of info, by its category (version_check.cpp) */
int gameAppCheckSystemVersion(const LsdbAppInfoPtr *info, paf::string *version);

/* the application's update checked for, with callback told of the result (version_check.cpp) */
int gameAppCheckUpdate(const LsdbAppInfoPtr *info, void (*callback)(int result, void *arg),
		       void *arg, bool gameCard, int mode, int priority);

/*
 * PLACEHOLDER: what a SELF's eboot parameters (its control information of type 6) give the
 * shell, read by 0x8103301C. Only the fields the game plugin reads are named.
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
 * PLACEHOLDER: a SELF's eboot parameters, defined in another file of the shell (constructor
 * 0x81032FD4, destructors 0x81032FF8 and 0x81033006, Read 0x8103328C, Load 0x81033456).
 */
class GameEbootParam {
public:
	GameEbootParam();
	virtual ~GameEbootParam();
	int Read(const paf::string &path);	/* from a SELF */
	int Load(const paf::string &path);	/* from a saved copy (0x81033456) */

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
float gameSumPercent(int last, const float *values);
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

/*
 * PLACEHOLDER: a folder's statistics, as 0x8104639C fills them; only the digest its param.sfo is
 * checked against is known (gamePluginDirGetAttributeMinor).
 */
struct GameDirStat {
	SceUInt8 data[0x378];
	char digest[40];
	SceSize digestLength;
	SceUInt8 reserved[4];
};

typedef char GameDirStatSize[sizeof(GameDirStat) == 0x3A8 ? 1 : -1];

/* PLACEHOLDER: a folder's statistics (0x8104639C, in a file of the shell not written yet) */
int gameDirReadStat(const paf::string &path, GameDirStat *stat);

/* a folder's statistics read into a cleared stat (0x8101731A) */
int gameDirGetStat(const paf::string &path, GameDirStat *stat);

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

/*
 * PLACEHOLDER: what 0x810A4EC4 reads from a free-trial license file, 0x50 bytes: its content ID
 * and two times it converts to dates.
 */
struct GameFttLicense {
	SceUInt8 header[8];
	char contentId[48];
	SceUInt64 time1;
	SceUInt64 time2;
	int otherField;
	SceUInt32 flags;	/* bit 0: GameFttInfo's flag */
};

/* what gamePluginFttRead gives of an application's free-trial license */
struct GameFttInfo {
	char contentId[48];
	SceUInt64 time1;
	SceUInt64 time2;
	int otherField;
	bool flag;
};

/*
 * PLACEHOLDER: what sets an application up in ur0 under the plugin's SceGamePluginSetupUr0
 * mutex (constructor 0x81035AA8, 96 bytes), defined in another file of the plugin.
 */
class GameUr0Setup {
public:
	GameUr0Setup(const LsdbAppInfoPtr *info, int arg);
	virtual ~GameUr0Setup();

	int Run(void);		/* 0x8103C03E */

	SceUInt8 otherFields[92];
};

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

/* PLACEHOLDER name: an imported content deleted, defined in another file (0x810576A2) */
int gameContentImportDelete(const char *titleId);

/*
 * PLACEHOLDER: the plugin's alarm, its "SceGameAlarm" callback (constructor 0x81063BD0, 36
 * bytes), defined in another file of the plugin.
 */
class GameAlarm {
public:
	GameAlarm();
	virtual ~GameAlarm();

	/* PLACEHOLDER names: what is known of the alarm's methods */
	int SetForPath(const char *path, const char *arg);	/* 0x81063D4A */
	int Update(const LsdbAppInfoPtr *info);			/* 0x81063EEC */
	int Remove(const LsdbAppInfoPtr *info);			/* 0x81064154 */

	SceUInt8 otherFields[32];
};

#endif
