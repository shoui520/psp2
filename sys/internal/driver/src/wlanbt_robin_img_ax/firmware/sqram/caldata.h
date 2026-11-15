/*
 * The calibration data's annex header and annex 42, which both halves of the
 * decoder read: sqram/caldata.c loads the annexes, sqram/caltable.c walks
 * the chain and loads annex 42's second half.
 */
#ifndef ROBIN_SQRAM_CALDATA_H
#define ROBIN_SQRAM_CALDATA_H

#include <stdint.h>

/* The header every annex starts with. */
struct CalDataHeader {
	uint8_t type;
	uint8_t reserved;
	uint16_t length;	/* the annex's bytes, the header's included */
	uint32_t next;		/* image offset of the next annex; 0xffffffff ends the chain */
};

_Static_assert(sizeof(struct CalDataHeader) == 8, "annex header");

/* One entry of annex 42; the bytes that go into a block (see calDataLoadAnnex42()). */
struct CalDataAnnex42Entry {
	uint8_t bytes[12];		/* byte 0, bits 0-3: 0 for band index 0, 2 for both */
};

/* Annex 42: up to five entries of the transmit limit curve (wlanCalibBlock). */
struct CalDataAnnex42 {
	struct CalDataHeader header;
	struct CalDataAnnex42Entry entries[5];
};

_Static_assert(sizeof(struct CalDataAnnex42) == 68, "annex 42");

#define CAL_DATA_ANNEX42_BAND_INDEX0	0
#define CAL_DATA_ANNEX42_BOTH		2

#endif /* ROBIN_SQRAM_CALDATA_H */
