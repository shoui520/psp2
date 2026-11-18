/*
 * USS-1001's (BOARD_USS1001) flash access (app/flash.c): the NVS's erase and write, and reads of
 * the data flash, on Renesas' data flash library (pfdl.h).
 */
#ifndef ERNIE_FLASH_H
#define ERNIE_FLASH_H

extern unsigned char nvsErase(unsigned char block);
extern unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words,
				   char verify);
extern unsigned char flashWrite(unsigned char *address, void *data, unsigned char size,
				char verify);
extern unsigned char flashRead(void *buffer, unsigned char *address, unsigned char size);
extern unsigned long flashReadLong(unsigned char *address);
extern unsigned int flashReadWord(unsigned char *address);
extern unsigned char configFlashWrite(unsigned long address);
extern unsigned char updaterFlashPrepare(void);
extern unsigned char flashInvertBootFlag(void);

#endif
