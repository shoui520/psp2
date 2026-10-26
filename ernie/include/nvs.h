/*
 * The NVS (app/nvs.c): the settings kept in the data flash, read and written by offset in
 * area 0 (0-0xf3f; PCH-1000: 0-0xb5f) or area 1 (0x40 bytes), at most 0x20 bytes at a time.
 */
#ifndef ERNIE_NVS_H
#define ERNIE_NVS_H

extern unsigned char nvsRead(unsigned int offset, unsigned int size, unsigned char *data,
			     unsigned char area);
extern unsigned char nvsWrite(unsigned int offset, unsigned int size, unsigned char *data,
			      unsigned char area);
extern unsigned char nvsInit(void);
extern unsigned char nvsGetState(void);
extern unsigned char nvsFormat(void);
extern unsigned char nvsMove(void);
extern unsigned char nvsClean(unsigned char count);
extern unsigned char nvsGetInfo(unsigned char *mark, unsigned char *missing, unsigned int *moves);

#endif
