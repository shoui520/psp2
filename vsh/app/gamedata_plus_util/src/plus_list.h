/*
 * plus_list.h: PlayStation Plus's lists of the applications and save data to upload
 * (savedata_plus/auto_upload), as gamedata_plus_util keeps them. The names are ours (no record
 * names them).
 */
#ifndef _PLUS_LIST_H
#define _PLUS_LIST_H

#include <stddef.h>
#include <scetypes.h>

/* the lists' errors (values only) */
#define PLUS_ERROR_INVALID_ARG	-2146416448	/* 0x8010A1C0 */
#define PLUS_ERROR_NO_ENTRY	-2146416447	/* 0x8010A1C1 */
#define PLUS_ERROR_IO		-2146416446	/* 0x8010A1C2: a list that does not open or ends early */
#define PLUS_ERROR_BAD_LIST	-2146416443	/* 0x8010A1C5: a list without its magic */

/* what the application list and the save data list start with */
#define PLUS_LIST_MAGIC		"SDUT"
#define PLUS_SD_LIST_MAGIC	"SDUS"

/* a title ID's room in an entry, its terminator included */
#define PLUS_TITLE_ID_SIZE	16

/* a list's header, 56 bytes */
struct PlusListHeader {
	char magic[4];		/* PLUS_LIST_MAGIC */
	SceUInt32 reserved[2];
	SceUInt32 count;	/* the entries after the header */
	SceUInt8 padding[40];
};

/* an application's entry, 64 bytes */
struct PlusListEntry {
	char titleId[PLUS_TITLE_ID_SIZE];
	SceUInt8 status;
	SceUInt8 padding[7];
	SceUInt64 time;		/* when the status was set */
	SceUInt8 reserved[32];
};

/* the save data list's header, 56 bytes */
struct PlusSdListHeader {
	char magic[4];		/* PLUS_SD_LIST_MAGIC */
	SceUInt32 reserved;
	SceUInt32 count;	/* the entries in use */
	SceUInt8 padding[44];
};

/* a save data's entry, 164 bytes: in use when its slot's first byte is not 0 */
struct PlusSdListEntry {
	char slot[16];
	char titleId[PLUS_TITLE_ID_SIZE];
	SceUInt8 reserved[132];
};

typedef char PlusListHeaderSize[sizeof(PlusListHeader) == 56 ? 1 : -1];
typedef char PlusListEntrySize[sizeof(PlusListEntry) == 64 ? 1 : -1];
typedef char PlusSdListHeaderSize[sizeof(PlusSdListHeader) == 56 ? 1 : -1];
typedef char PlusSdListEntrySize[sizeof(PlusSdListEntry) == 164 ? 1 : -1];

/* the status of an application in the current user's list, and in every user's */
int plusAppListSetStatus(const char *titleId, SceUInt8 status);
int plusAppListSetStatusAllUsers(const char *titleId, SceUInt8 status);

/* the save data of an application taken out of the current user's list, and every user's */
int plusSdListDelete(const char *titleId);
int plusSdListDeleteAllUsers(const char *titleId);

#endif
