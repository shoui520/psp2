/*
 * The channel measurement of the SQRAM code (sqram/chanrpt.c): the host's
 * channel report and the access point's channel survey (wlan/apbss.c) both
 * start one.
 */
#ifndef ROBIN_CHANRPT_H
#define ROBIN_CHANRPT_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"

/* What the measurement is asked to do. */
struct ChanRptRequest {
	uint8_t band;			/* bits 0-1: the band; bits 2-3: the command's band configuration bits 0-1 */
	uint8_t channel;
	uint8_t reserved2[2];
	uint32_t delay;			/* before the slot starts */
	uint32_t reserved8[2];
	uint32_t dwell;			/* microseconds */
	uint8_t reserved20[40];
	uint8_t histogramWideRange;	/* 1: the noise histogram's wide range */
	uint8_t reserved61[3];
	uint32_t radar;			/* the host asked for the radar bit */
	uint8_t reserved68[4];
};

_Static_assert(offsetof(struct ChanRptRequest, histogramWideRange) == 60, "channel report request");
_Static_assert(sizeof(struct ChanRptRequest) == 72, "channel report request");

/* Gets the report buffer when the measurement ends, or (0, NULL) when none could start. */
typedef int (*ChanRptDone)(int status, OsaBuffer *buffer);

int chanRptStart(struct WlanConnection *connection, const struct ChanRptRequest *request,
		 ChanRptDone done);						/* SQRAM 0xc00098be */

#endif
