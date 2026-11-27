/*
 * The database check library the shell links (also linked by the vs0 applications, e.g.
 * NPXS10000/10004): a content database is opened by its ID or path, copied from its initial
 * image when missing, reset when its schema version is not the expected one, and marked when
 * damaged ("-corrupt" files beside it, "-journal" removed with it). No record or string names
 * the library, its functions or its types: every name here is ours.
 */
#ifndef _SHELL_DB_CHECK_H
#define _SHELL_DB_CHECK_H

#include <scetypes.h>

/* the database's schema version is not the one expected */
#define SCE_DB_CHECK_ERROR_VERSION	0x80105D05

/* DbCheckParam's options */
#define DB_CHECK_OPTION_REMOVE			0x2	/* removed and made again */
#define DB_CHECK_OPTION_RESET_ON_VERSION	0x4	/* made again on a version mismatch */

typedef struct DbCheckParam {
	int id;				/* a known database, 0 for one given by path */
	const char *path;		/* by path: the database */
	const char *initialPath;	/* by path: its initial image */
	int copyFlags;			/* how the initial image is copied */
	int dirMode;			/* by path: the mode of the folders made for it */
	int alignPad;
	SceUInt64 optionalParam;
	int optionalFlags;
	int tailPad;
	SceUInt64 option;		/* DB_CHECK_OPTION_* */
} DbCheckParam;

typedef struct DbCheckResult {
	SceUInt64 status;		/* 1: made from the initial image, 2: was there */
	unsigned int version;		/* the database's schema version */
} DbCheckResult;

int DbCheckOpen(const DbCheckParam *param, DbCheckResult *result);	/* 0x8135FB5C */
int DbCheckNeedsRecovery(int *needsRecovery);				/* 0x813603D8 */
int DbCheckReset(int id);						/* 0x81360766 */
int DbCheckSetCorrupt(int id, int corrupt);				/* 0x813607DA */

#endif
