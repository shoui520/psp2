/*
 * game_appsize.h: an application's #appsize in livespace_db, as the game plugin's installers set
 * it (psp_license.cpp, ac_installer.cpp; gameAppSizeSetAddcont is out of line at 0x8140FAC4). The
 * header's name is ours.
 */
#ifndef _GAME_APPSIZE_H
#define _GAME_APPSIZE_H

#include <scetypes.h>
#include <std/string>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_appinfo.h"

/*
 * An application's #appsize: its sizes in 32 KiB units, packed in 61 bits: the application's in
 * bits 0-19, its patch's in bits 20-39 and its additional contents' in bits 40-60; a field of all
 * ones is a size not counted yet. The names are ours.
 */
#define GAME_APPSIZE_UNIT_SHIFT		15
#define GAME_APPSIZE_MAX		0x1FFFFFFFFFFFFFFFull
#define GAME_APPSIZE_PATCH_SHIFT	20
#define GAME_APPSIZE_PATCH_MASK		0xFFFFFull
#define GAME_APPSIZE_ADDCONT_SHIFT	40
#define GAME_APPSIZE_ADDCONT_MASK	0x1FFFFFull

/* a size in #appsize units, rounded up; the not-counted values kept as they are */
inline unsigned int gameAppSizeUnits(SceUInt64 size)
{
	if (size == 0)
		return 0;
	if (size < (1u << GAME_APPSIZE_UNIT_SHIFT))
		return 1;
	if (size == GAME_APPSIZE_PATCH_MASK || size == GAME_APPSIZE_ADDCONT_MASK)
		return (unsigned int)size;

	unsigned int units = (unsigned int)(size >> GAME_APPSIZE_UNIT_SHIFT);

	if ((size & ((1u << GAME_APPSIZE_UNIT_SHIFT) - 1)) != 0)
		units++;

	return units;
}

/*
 * SceLsdb's export defined in appinfo_daemon_api.cpp, which livespace_db_internal.h does not
 * declare yet: the bits of mask in an application's integer parameter set to bits
 */
extern "C" int SceLsdb_C36501F7(const paf::string *titleId, unsigned int key,
				const LsdbValue *bits, SceInt64 mask);

/*
 * A field of titleId's #appsize set to size: the others kept, or not counted when the application
 * has none yet.
 */
inline int gameAppSizeSet(const paf::string *titleId, SceUInt64 size, unsigned int shift,
			  SceUInt64 fieldMask)
{
	AppInfoQClient client;
	SceUInt64 bits = 0;
	unsigned int units;
	bool found = false;

	SceLsdb_A6D8772C(&client);

	int ret = SceLsdb_E90B81CD(&client);

	if (ret != 0) {
		SceLsdb_E2101B03(&client);
	} else {
		{
			LsdbValue value;

			ret = SceLsdb_65ECEF39(&client, titleId, LsdbKeyHash("#appsize"), &value);
			if (ret == 0) {
				if (((SceUInt64)gameLsdbValueInteger(value) >> 61) == 0) {
					bits = (SceUInt64)gameLsdbValueInteger(value);
					found = true;
				} else {
					ret = -1;
				}
			}
		}
		SceLsdb_E2101B03(&client);
	}

	if (found) {
		units = gameAppSizeUnits(size);
		if (units > fieldMask)
			return -1;
	} else {
		if (ret != (int)LSDB_ERROR_NOT_FOUND)
			return ret;

		units = gameAppSizeUnits(size);
		bits = GAME_APPSIZE_MAX;
	}

	bits = (bits & ~(fieldMask << shift)) | ((SceUInt64)units << shift);

	LsdbValue update;

	update.integer = (SceInt64)(bits & GAME_APPSIZE_MAX);

	return SceLsdb_C36501F7(titleId, LsdbKeyHash("#appsize"), &update, -1);
}

/* the patch's and the additional contents' fields of titleId's #appsize set to size */
inline int gameAppSizeSetPatch(const paf::string *titleId, SceUInt64 size)
{
	return gameAppSizeSet(titleId, size, GAME_APPSIZE_PATCH_SHIFT, GAME_APPSIZE_PATCH_MASK);
}

inline int gameAppSizeSetAddcont(const paf::string *titleId, SceUInt64 size)
{
	return gameAppSizeSet(titleId, size, GAME_APPSIZE_ADDCONT_SHIFT, GAME_APPSIZE_ADDCONT_MASK);
}

#endif /* _GAME_APPSIZE_H */
