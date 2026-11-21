/*
 * The NVS (app/nvs.c): the settings kept in the data flash, read and written by offset in
 * area 0 (0-0xf3f; PCH-1000: 0-0xb5f) or area 1 (0x40 bytes), at most 0x20 bytes at a time.
 */
#ifndef ERNIE_NVS_H
#define ERNIE_NVS_H

#include "ernie.h"
#include "board.h"

/* nvsGetState: the NVS has failed (no data blocks found), its spares are full (a move is due),
   the data was moved (the old blocks are still to be erased) */
#if FW_SINCE(0x01000306)
#define NVS_STATE_FAILED	0xf0
#define NVS_STATE_FULL		0x03
#define NVS_STATE_MOVED		0x12
#else
#define NVS_STATE_FAILED	0x80
#define NVS_STATE_FULL		0x81
#define NVS_STATE_MOVED		0x82
#endif

extern unsigned char nvsRead(unsigned int offset, unsigned int size, unsigned char *data,
			     unsigned char area);
extern unsigned char nvsWrite(unsigned int offset, unsigned int size, unsigned char *data,
			      unsigned char area);
extern unsigned char nvsInit(void);
extern unsigned char nvsGetState(void);
extern unsigned char nvsFormat(void);
extern unsigned char nvsMove(void);
extern unsigned char nvsClean(unsigned char count);
/* the slim's nvsGetInfo with the map of the records written (0x01030505: without, as the
   others') */
#define NVS_INFO_MAP	(BOARD_USS1001 && APP_SINCE(0x01030511))

#if NVS_INFO_MAP
/* nvsGetInfo's map of the records written */
struct NvsMap {
	unsigned char	count;
	unsigned char	size;			/* of bits */
	unsigned char	bits[12];		/* record n: bits[n >> 3] & 0x80 >> (n & 7) */
};

extern unsigned char nvsGetInfo(unsigned char *mark, unsigned char *missing, unsigned long *moves,
				struct NvsMap *map);
#else
extern unsigned char nvsGetInfo(unsigned char *mark, unsigned char *missing, unsigned int *moves);
#endif

#endif
