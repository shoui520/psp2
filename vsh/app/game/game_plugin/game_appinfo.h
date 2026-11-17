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
struct GameAppEntry {
	int otherHeader[3];
	paf::string otherText;
	paf::string iconPath;			/* icon0.png, or icon0.dds for EG and ME */
	int storage;				/* 2 for wda, 0 otherwise */
	paf::string titleId;
	int kind;				/* 2-5 from the attribute or the path, 1 for ME */
	paf::string contentLocation;		/* wda's CONTENT_LOCATION */
	int parentalLevel;
	int otherTail[4];
};

struct GameAppRegistration {
	paf::common::SharedPtr<GameAppEntry> entry;
	LsdbAppInfoPtr info;
	SceUInt8 otherFields[0x10];
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
const unsigned int *gameGetAddcontDirKeys(void);
unsigned int gameGetAddcontDirKeyCount(void);
bool gameCategoryIsGameName(const paf::string &category);
int gameAppIsOnUx0(const LsdbAppInfoPtr *info, bool *onUx0);
bool gameCategoryMatches(const paf::string &category, const paf::string &other);
bool gameCategoryIsDataPair(const paf::string &category, const paf::string &other);
int gamePathGetStorage(const paf::string &path, SceUInt64 flags);
int gamePatchPathGetStorage(const paf::string &path, SceUInt64 flags,
			    const paf::string &category);
int gameAppRegister(GameAppRegistration *app, int mode, bool keepLiveArea, bool notifyTopmenu,
		    const void *param);

#endif
