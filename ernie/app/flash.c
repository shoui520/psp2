/*
 * USS-1001's flash access. The code flash (the configuration banks, the update) is written and
 * the boot swapped by Renesas' self-programming library; the NVS lives in the data flash (8 blocks of 0x400 bytes at 0xf1000),
 * which Renesas' data flash library erases, writes and reads: each call opens the library for an
 * 8 MHz clock in the wide voltage mode, runs one request to its end and closes it. The NVS's
 * erase and write keep the 78K0R build's names and calls (nvs.c).
 */
#pragma sfr
#pragma NOP
#pragma DI
#pragma EI

#include <string.h>

#include "ernie.h"
#include "flash.h"
#include "config.h"
#if BOARD_USS1001
#include "pfdl.h"
#include "fsl.h"
#include "kernel.h"
#endif

#define FLASH_BLOCKS		8
#define FLASH_BLOCK_SIZE	0x400
#define FLASH_BASE		0x1000		/* the data flash's near address */

#if BOARD_USS1001
/* USS-1002's 0x01050031 opens the library once, at the first request, and the NVS closes it at
   the end of each of its operations (flashClose); 0x01050034 opens and closes it in each request
   again, as 0x0103 does */
#define FLASH_KEEP_OPEN		(SOURCE_0104 && !FW_SINCE(0x01050034))

#if FLASH_KEEP_OPEN
/* opens the library unless it is open (DFLEN set) */
static unsigned char flashOpen(void)
{
	pfdl_descriptor_t descriptor;
	unsigned char result;

	result = PFDL_OK;
	if (!DFLCTL.0) {	/* DFLEN */
		descriptor.fx_MHz_u08 = 8;
		descriptor.wide_voltage_mode_u08 = 1;
		result = PFDL_Open(&descriptor);
	}
	return result;
}

void flashClose(void)
{
	PFDL_Close();
}
#endif

/* erases a block of the NVS when its blank check fails: tried 5 more times while the erase fails */
unsigned char nvsErase(unsigned char block)
{
	unsigned char result;
	pfdl_request_t request;
#if !FLASH_KEEP_OPEN
	pfdl_descriptor_t descriptor;
#endif
	unsigned char retry;

	retry = 5;
	if (block >= FLASH_BLOCKS)
		return 0x23;
#if FLASH_KEEP_OPEN
	result = flashOpen();
#else
	descriptor.fx_MHz_u08 = 8;
	descriptor.wide_voltage_mode_u08 = 1;
	result = PFDL_Open(&descriptor);
#endif
	if (result != PFDL_OK)
		return result;
	do {
		WDTE = 0xac;
		request.data_pu08 = 0;
		request.command_enu = PFDL_CMD_BLANKCHECK_BYTES;
		request.index_u16 = (unsigned int)block << 10;
		request.bytecount_u16 = FLASH_BLOCK_SIZE;
		result = PFDL_Execute(&request);
		while (result == PFDL_BUSY) {
			NOP();
			result = PFDL_Handler();
		}
		if (result == PFDL_ERR_MARGIN) {
			request.command_enu = PFDL_CMD_ERASE_BLOCK;
			request.index_u16 = block;
			result = PFDL_Execute(&request);
			while (result == PFDL_BUSY) {
#if FLASH_KEEP_OPEN
				dly_tsk(1);
#else
				NOP();
#endif
				result = PFDL_Handler();
			}
		}
	} while (retry-- != 0 && result == PFDL_ERR_ERASE);
#if !FLASH_KEEP_OPEN
	PFDL_Close();
#endif
	return result;
}

/* writes words of 4 bytes at an address of the NVS, verifying them if asked */
unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words, char verify)
{
	return flashWrite(address, data, words << 2, verify);
}

/* writes bytes at an address of the data flash, verifying them when verify is 1 */
unsigned char flashWrite(unsigned char *address, void *data, unsigned char size, char verify)
{
	unsigned char result;
	pfdl_request_t request;
#if !FLASH_KEEP_OPEN
	pfdl_descriptor_t descriptor;
#endif

#if FLASH_KEEP_OPEN
	result = flashOpen();
#else
	descriptor.fx_MHz_u08 = 8;
	descriptor.wide_voltage_mode_u08 = 1;
	result = PFDL_Open(&descriptor);
#endif
	if (result != PFDL_OK)
		return result;
	request.index_u16 = (unsigned int)address - FLASH_BASE;
	request.data_pu08 = (unsigned char __near *)data;
	request.bytecount_u16 = size;
	request.command_enu = PFDL_CMD_WRITE_BYTES;
	result = PFDL_Execute(&request);
	while (result == PFDL_BUSY) {
		NOP();
		result = PFDL_Handler();
	}
	if (result == PFDL_OK && verify == 1) {
		request.command_enu = PFDL_CMD_IVERIFY_BYTES;
		result = PFDL_Execute(&request);
		while (result == PFDL_BUSY) {
			NOP();
			result = PFDL_Handler();
		}
	}
#if !FLASH_KEEP_OPEN
	PFDL_Close();
#endif
	return result;
}

/* reads bytes of the data flash */
unsigned char flashRead(void *buffer, unsigned char *address, unsigned char size)
{
	unsigned char result;
	pfdl_request_t request;
#if !FLASH_KEEP_OPEN
	pfdl_descriptor_t descriptor;
#endif

#if FLASH_KEEP_OPEN
	result = flashOpen();
#else
	descriptor.fx_MHz_u08 = 8;
	descriptor.wide_voltage_mode_u08 = 1;
	result = PFDL_Open(&descriptor);
#endif
	if (result != PFDL_OK)
		return result;
	request.data_pu08 = (unsigned char __near *)buffer;
	request.command_enu = PFDL_CMD_READ_BYTES;
	request.index_u16 = (unsigned int)address - FLASH_BASE;
	request.bytecount_u16 = size;
	result = PFDL_Execute(&request);
#if !FLASH_KEEP_OPEN
	PFDL_Close();
#endif
	return result;
}

/* a long of the data flash, or 0xffffffff when it cannot be read */
unsigned long flashReadLong(unsigned char *address)
{
	unsigned long value;

	if (flashRead(&value, address, 4) == PFDL_OK)
		return value;
	return 0xffffffff;
}

/* a word of the data flash, or 0xffff when it cannot be read */
unsigned int flashReadWord(unsigned char *address)
{
	unsigned int value;

	if (flashRead(&value, address, 2) == PFDL_OK)
		return value;
	return 0xffff;
}

#endif	/* BOARD_USS1001 */

#if BOARD_DOL1001
/*
 * DOL-1001's: the 78K0R build's configFlashWrite (config.c's) and the updater's flash
 * preparation, on the 78K0R flash library (the updater's, at 0xf09c-0xf4dc)
 */
extern void FSL_Open(void);
extern unsigned char FSL_Init(unsigned char *descriptor);
extern unsigned char FSL_Init_cont(unsigned char *descriptor);
extern unsigned char FSL_ModeCheck(void);
extern unsigned char FSL_BlankCheck(unsigned int block);
extern unsigned char FSL_Erase(unsigned int block);
extern unsigned char FSL_IVerify(unsigned int block);
extern unsigned char FSL_Write(unsigned long address, unsigned char words);
extern unsigned char FSL_EEPROMWrite(unsigned long address, unsigned char words);
extern unsigned char FSL_GetSecurityFlags(unsigned char *flags);
extern unsigned char fslSetSecurityFlag(void);
extern unsigned char fslSetSecurityFlagSwap(void);
extern void FSL_Close(void);
extern unsigned char fslDataBuffer[];		/* the library's descriptor and write buffer */
#ifdef ERNIE_UPDATER
struct ConfigBank configBuffer;			/* the updater's: the bank configFlashWrite writes */
#else
extern struct ConfigBank configBuffer;
#endif

/* the flash library's status */
#define FLASH_OK		0x00
#define FLASH_ERASE_ERROR	0x1a
#define FLASH_NOT_BLANK		0x1b
#define FLASH_WRITE_ERROR	0x1c
#define FLASH_BUSY		0x1f

/* erases a block of the NVS (0-0xf, the flash's blocks 0xf0-0xff): tried 5 more times while
   the erase fails */
unsigned char nvsErase(unsigned char block)
{
	unsigned char result;
	unsigned char retry;

	retry = 5;
	if (block >= 0x10)
		return 0x23;
	block += 0xf0;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	if (result == FLASH_OK) {
		do {
			WDTE = 0xac;
			result = FSL_ModeCheck();
			if (result == FLASH_OK) {
				do
					result = FSL_BlankCheck(block);
				while (result == FLASH_BUSY);
				if (result == FLASH_NOT_BLANK) {
					do
						result = FSL_Erase(block);
					while (result == FLASH_BUSY);
				}
			}
#if BOARD_DOL1001	/* the PS TV's: an empty statement of its source */
			if (result != FLASH_OK)
				;
#endif
		} while (retry-- != 0 && result == FLASH_ERASE_ERROR);
	}
	DI();
	FSL_Close();
#ifndef ERNIE_UPDATER	/* the updater's leaves the interrupts off */
	EI();
#endif
	return result;
}

/* writes words of 4 bytes at an address of the NVS (0x3c000 on), verifying the block if asked */
unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words, char verify)
{
	unsigned char result;
	unsigned char retry;

	retry = 0;
	if ((unsigned long)address < 0x3c000)
		return 0x23;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	if (result == FLASH_OK) {
		memcpy(fslDataBuffer, data, words << 2);
		do {
			result = FSL_ModeCheck();
			if (result == FLASH_OK) {
				do
					result = FSL_EEPROMWrite((unsigned long)address, words);
				while (result == FLASH_BUSY);
			}
			if (result == FLASH_OK && verify == 1) {
				do
					result = FSL_IVerify((unsigned long)address >> 10);
				while (result == FLASH_BUSY);
			}
		} while (retry-- != 0 && result == FLASH_WRITE_ERROR);
	}
	FSL_Close();
	return result;
}

/* erases the bank at address and writes the open copy there, three times at most */
unsigned char configFlashWrite(unsigned long address)
{
	unsigned int i;
	unsigned char tries;
	unsigned char result;

	tries = 3;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == 0x1f)
		result = FSL_Init_cont(fslDataBuffer);
	if (result == 0) {
		do {
			WDTE = 0xac;
			result = FSL_ModeCheck();
			if (result == 0) {
				do
					result = FSL_BlankCheck((unsigned int)(address >> 10));
				while (result == 0x1f);
				if (result == 0x1b) {
					do
						result = FSL_Erase((unsigned int)(address >> 10));
					while (result == 0x1f);
				}
			}
			if (result == 0) {
				for (i = 0; i < 0x100; i += 0x40) {
					memcpy(fslDataBuffer, (unsigned char *)&configBuffer + i * 4, 0x100);
					do
						result = FSL_Write(address + i * 4, 0x40);
					while (result == 0x1f);
					if (result != 0)
						break;
				}
			}
			if (result == 0) {
				do
					result = FSL_IVerify((unsigned int)(address >> 10));
				while (result == 0x1f);
			}
		} while (tries-- != 0 && result != 0);
	}
	FSL_Close();
	return result;
}

#ifndef ERNIE_UPDATER	/* the updater has no downloader to prepare */
/* opens the flash library for the downloader and sets its security flag, three times at most;
   0 when it may start */
unsigned char updaterFlashPrepare(void)
{
	unsigned char result;
	unsigned int flags;
	unsigned char tries;

	tries = 3;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	if (result == 0) {
		result = FSL_ModeCheck();
		if (result == 0) {
			result = FSL_GetSecurityFlags((unsigned char *)&flags);
			if (result == 0 || (flags & 0x0e) == 0x0e) {
				WDTE = 0xac;
				do
					result = fslSetSecurityFlag();
				while (tries-- != 0 && result != 0);
			}
		}
	}
	FSL_Close();
	return result;
}

#endif

/* swaps the boot clusters, three times at most (nothing in the application calls it; the
   updater's image check does) */
unsigned char flashBootClusterSwap(void)
{
	unsigned char tries;
	unsigned char result;

	tries = 3;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	if (result == 0) {
		result = FSL_ModeCheck();
		if (result == 0) {
			WDTE = 0xac;
			do
				result = fslSetSecurityFlagSwap();
			while (tries-- != 0 && result != 0);
		}
	}
	FSL_Close();
	return result;
}
#endif	/* BOARD_DOL1001 */

#if BOARD_USS1001
/* the self-programming library's settings: the wide voltage mode, 8 MHz, each command to its end */
const fsl_descriptor_t fslDescriptor = { 1, 8, 1 };

/* erases the block at address and writes the open copy there, three times at most */
unsigned char configFlashWrite(unsigned long address)
{
	unsigned char result;
	fsl_write_t write;
	unsigned int i;
	unsigned char tries;

	tries = 3;
	_rcopy(1);
	if ((address & (FSL_BLOCK_SIZE - 1)) != 0)
		return 5;
	result = FSL_Init(&fslDescriptor);
	if (result != FSL_OK)
		return result;
	FSL_Open();
	FSL_PrepareFunctions();
	do {
		WDTE = 0xac;
		DI();
		result = FSL_BlankCheck((unsigned int)(address / FSL_BLOCK_SIZE));
		EI();
		if (result == FSL_ERR_BLANKCHECK) {
			DI();
			result = FSL_Erase((unsigned int)(address / FSL_BLOCK_SIZE));
			EI();
		}
		if (result == FSL_OK) {
			for (i = 0; i < 0x100; i += 0x40) {
				write.fsl_data_buffer_p_u08 =
					(unsigned char __near *)((unsigned char *)&configBuffer + i * 4);
				write.fsl_destination_address_u32 = address + i * 4;
				write.fsl_word_count_u08 = 0x40;
				DI();
				result = FSL_Write(&write);
				EI();
				if (result != FSL_OK)
					break;
			}
		}
		if (result == FSL_OK) {
			DI();
			result = FSL_IVerify((unsigned int)(address / FSL_BLOCK_SIZE));
			EI();
		}
	} while (tries-- != 0 && result != FSL_OK);
	FSL_Close();
	return result;
}

/* swaps the boot cluster for the downloader, three times at most; 0 when it may start */
unsigned char updaterFlashPrepare(void)
{
	unsigned char result;
	unsigned char tries;

	tries = 3;
	_rcopy(1);
	result = FSL_Init(&fslDescriptor);
	if (result != FSL_OK)
		return result;
	FSL_Open();
	FSL_PrepareFunctions();
	FSL_PrepareExtFunctions();
	WDTE = 0xac;
	do
		result = FSL_SwapBootCluster();
	while (tries-- != 0 && result != FSL_OK);
	FSL_Close();
	return result;
}

/* inverts the boot flag, with interrupts off, three times at most */
unsigned char flashInvertBootFlag(void)
{
	unsigned char result;
	unsigned char tries;

	tries = 3;
	result = FSL_Init(&fslDescriptor);
	if (result != FSL_OK)
		return result;
	FSL_Open();
	FSL_PrepareFunctions();
	FSL_PrepareExtFunctions();
	DI();
	WDTE = 0xac;
	do
		result = FSL_InvertBootFlag();
	while (tries-- != 0 && result != FSL_OK);
	EI();
	FSL_Close();
	return result;
}

#endif	/* BOARD_USS1001 */
