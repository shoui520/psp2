/*
 * libSceCes's DBCS.h (Sony's internal/usermode/src/ces/libces/src/DBCS/DBCS.h): the sorted tables
 * double-byte character sets map with, and their binary searches. Types and names from the
 * library's debug information (SDK 3.570.011 libSceCes.a).
 */
#ifndef _CES_DBCS_H
#define _CES_DBCS_H

#include <stdint.h>

/*
 * a run of a character set's indexes from index on: ucs and the indexes after it map to
 * consecutive code points, up to the next entry's index; ucs 0 is a run of none
 */
typedef struct CesIndexUcsDB_t {
	uint16_t index;
	uint16_t ucs;
} CesIndexUcsDB_t;

/* a code point's double-byte code; an entry with ucs 0 ends the run its predecessor starts */
typedef struct CesUcsDbcDB_t {
	uint16_t ucs;
	uint16_t dbc;
} CesUcsDbcDB_t;

typedef struct CesDbcUcsDB_t {
	uint16_t dbc;
	uint16_t ucs;
} CesDbcUcsDB_t;

uint32_t _sceCesIndexUcsDB_getUcs4(const CesIndexUcsDB_t *tbl0, const CesIndexUcsDB_t *tbl1,
				   uint16_t index);
int _sceCesIndexUcsDB_getIndex(const CesIndexUcsDB_t *tbl0, const CesIndexUcsDB_t *tbl1,
			       uint32_t ucs4);
uint16_t _sceCesUcsDbcDB_getDbc(const CesUcsDbcDB_t *tbl0, const CesUcsDbcDB_t *tbl1,
				uint32_t ucs4);

/* a table's last entry, and an index table's last run (the entry before its end entry) */
#define CES_DB_LAST(db)		(&(db)[sizeof(db) / sizeof((db)[0]) - 1])
#define CES_INDEX_DB_LAST(db)	(&(db)[sizeof(db) / sizeof((db)[0]) - 2])

#endif /* _CES_DBCS_H */
