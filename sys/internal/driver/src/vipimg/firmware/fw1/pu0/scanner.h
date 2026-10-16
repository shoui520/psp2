/*
 * PU0's start code scanner (control bus 0x9000): fw1's demultiplexer (demux.h)
 * and FW2's parser0 (fw2/parser0) run it.
 */
#pragma once

/* the start code scanner */
#define VIP_CB_SCANNER_START		0x9001	/* write 1: run; reads 0 when done */
#define VIP_CB_SCANNER_MODE		0x9002
#define VIP_CB_SCANNER_READ		0x9003	/* window offsets */
#define VIP_CB_SCANNER_END		0x9004
#define VIP_CB_SCANNER_WRITE		0x9005
#define VIP_CB_SCANNER_STOP_AFTER	0x9006	/* 1: stop on the byte after a start code (inferred) */
#define VIP_CB_SCANNER_PATTERN		0x9007
#define VIP_CB_SCANNER_PATTERN_MASK	0x9008	/* its bits to ignore (inferred) */
#define VIP_CB_SCANNER_STATUS		0x9010
#define VIP_CB_SCANNER_CONFIG		0x9011

/* the scanner sees local RAM through an 8 KB window */
#define VIP_SCANNER_WINDOW		(VIP_LOCAL_RAM + 0x2000)
#define VIP_SCANNER_MASK		0x1fff

#define VIP_SCANNER_MODE_COPY		2
#define VIP_SCANNER_MODE_SEARCH		0x30	/* for start codes */
#define VIP_SCANNER_MODE_PAYLOAD	0x32	/* and copy to the output */
#define VIP_SCANNER_MODE_FIND		5	/* FW2: a start code and the byte after it */
#define VIP_SCANNER_MODE_FIND_COPY	7	/* and copy to the output */
#define VIP_SCANNER_MODE_FIND_END	0x34	/* the next start code or zero bytes */
#define VIP_SCANNER_CONFIG_START	0x10	/* FW2: no part of a start code seen */
#define VIP_SCANNER_CONFIG_RESTART	0x100
#define VIP_SCANNER_CONFIG_KEEP		0xffffc1e0	/* bits kept on a restart */

/* VIP_CB_SCANNER_STATUS: where the scanner stopped */
#define VIP_SCANNER_STOPPED		0x01	/* FW2: with nothing else, at the range's end */
#define VIP_SCANNER_FOUND_ZEROS		0x10	/* three zero bytes */
#define VIP_SCANNER_FOUND_PREFIX	0x20	/* 00 00 01 */

/*
 * FW2's offsets: the scanner reads a piece of input at VIP_SCANNER_INPUT and
 * copies to VIP_SCANNER_OUTPUT, both in bank 1, where the core sees an offset X
 * at VIP_SCANNER_ADDRESS(X) (inferred from the original's addresses)
 */
#define VIP_SCANNER_INPUT		0x10000
#define VIP_SCANNER_OUTPUT		0x10200
#define VIP_SCANNER_ADDRESS(offset)	(VIP_LOCAL_RAM + VIP_LOCAL_BANK1 + 0x1600 - \
	VIP_SCANNER_INPUT + (offset))
