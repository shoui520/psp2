/*
 * libSceCes's DBCS.c (Sony's internal/usermode/src/ces/libces/src/DBCS/DBCS.c): binary searches
 * of the sorted index and code tables, between the first entry tbl0 and the last tbl1.
 */
#include <stdint.h>
#include "DBCS.h"

/* the middle entry of tbl0..tbl1 */
#define CES_DB_MIDDLE(tbl0, tbl1)	(&(tbl0)[((tbl1) - (tbl0) + 1) / 2])

/* the code point of a character set's index; 0 for none */
uint32_t _sceCesIndexUcsDB_getUcs4(const CesIndexUcsDB_t *tbl0, const CesIndexUcsDB_t *tbl1,
				   uint16_t index)
{
	const CesIndexUcsDB_t *tbl;

	for (;;) {
		tbl = CES_DB_MIDDLE(tbl0, tbl1);
		if (index < tbl->index) {
			if (tbl == tbl0)
				break;
			tbl1 = tbl - 1;
		} else if (index >= tbl[1].index) {
			if (tbl == tbl1)
				break;
			tbl0 = tbl + 1;
		} else {
			uint32_t ucs = tbl->ucs;

			if (ucs != 0)
				return ucs + (index - tbl->index);
			break;
		}
	}
	return 0;
}

/* the character set's index of a code point; -1 for none */
int _sceCesIndexUcsDB_getIndex(const CesIndexUcsDB_t *tbl0, const CesIndexUcsDB_t *tbl1,
			       uint32_t ucs4)
{
	const CesIndexUcsDB_t *tbl;
	uint32_t ucs0;
	uint32_t ucs1;
	int index;

	for (;;) {
		tbl = CES_DB_MIDDLE(tbl0, tbl1);
		ucs0 = tbl->ucs;
		if (ucs0 == 0) {
			/* a run of none: search on either side */
			if (tbl == tbl0) {
				if (tbl == tbl1)
					break;
				tbl0 = tbl + 1;
			} else {
				tbl1 = tbl - 1;
			}
		} else if (ucs4 < ucs0) {
			if (tbl == tbl0)
				break;
			tbl1 = tbl - 1;
		} else {
			ucs1 = tbl[1].ucs;
			if (ucs1 == 0)
				ucs1 = ucs0 + (tbl[1].index - tbl->index);
			if (ucs4 >= ucs1) {
				if (tbl == tbl1)
					break;
				tbl0 = tbl + 1;
			} else {
				index = tbl->index + (ucs4 - ucs0);
				if (index < tbl[1].index)
					return index;
				break;
			}
		}
	}
	return -1;
}

/* the double-byte code of a code point; 0 for none */
uint16_t _sceCesUcsDbcDB_getDbc(const CesUcsDbcDB_t *tbl0, const CesUcsDbcDB_t *tbl1,
				uint32_t ucs4)
{
	const CesUcsDbcDB_t *tbl;
	uint32_t ucs0;
	uint32_t ucs1;
	uint16_t dbc;

	for (;;) {
		tbl = CES_DB_MIDDLE(tbl0, tbl1);
		ucs0 = tbl->ucs;
		if (ucs0 != 0) {
			if (ucs4 < ucs0) {
				if (tbl == tbl0)
					break;
				tbl1 = tbl - 1;
			} else if (ucs4 > ucs0) {
				if (tbl == tbl1)
					break;
				tbl0 = tbl + 1;
			} else {
				return tbl->dbc;
			}
		} else {
			/* the end of the run the entry before starts */
			dbc = tbl[-1].dbc;
			ucs0 = tbl[-1].ucs;
			ucs1 = tbl->dbc - dbc + ucs0;
			if (ucs4 < ucs0) {
				if (tbl - 1 <= tbl0)
					break;
				tbl1 = tbl - 2;
			} else if (ucs4 > ucs1) {
				if (tbl == tbl1)
					break;
				tbl0 = tbl + 1;
			} else {
				return dbc + (ucs4 - ucs0);
			}
		}
	}
	return 0;
}
