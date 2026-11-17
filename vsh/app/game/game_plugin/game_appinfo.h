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
struct GameAppPaths;			/* PLACEHOLDER: an 88-byte object, not read yet */

struct GameAppRegistration {
	paf::common::SharedPtr<GameAppPaths> paths;
	LsdbAppInfoPtr info;
	SceUInt8 otherFields[0x10];
	int liveAreaState;			/* 0xFFFFFF when built; 0 when it has none */
	bool otherFlag;
	int mode;				/* what the registration is for (1: an install) */
};

/* a title ID: nine upper-case letters and digits (0x8101D662) */
int gameCheckTitleId(const char *titleId);

size_t gameStrnlen(const char *str, size_t max);
int gameAppInfoSetTimeNow(unsigned int key, LsdbAppInfoPtr *info);
int gameAppInfoCopyTime(unsigned int key, const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);
int gameAppInfoCopyShellKeys(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);
int gameAppInfoCopyKept(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);
int gameAppInfoSetPromoteAttr(SceUInt32 attr, LsdbAppInfoPtr *info);
int gameAppRegister(GameAppRegistration *app, int mode, bool keepLiveArea, bool notifyTopmenu,
		    const void *param);

#endif
