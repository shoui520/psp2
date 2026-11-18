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

#include "ernie.h"
#include "flash.h"
#include "pfdl.h"
#include "fsl.h"
#include "config.h"

#define FLASH_BLOCKS		8
#define FLASH_BLOCK_SIZE	0x400
#define FLASH_BASE		0x1000		/* the data flash's near address */

/* erases a block of the NVS when its blank check fails: tried 5 more times while the erase fails */
unsigned char nvsErase(unsigned char block)
{
	unsigned char result;
	pfdl_request_t request;
	pfdl_descriptor_t descriptor;
	unsigned char retry;

	retry = 5;
	if (block >= FLASH_BLOCKS)
		return 0x23;
	descriptor.fx_MHz_u08 = 8;
	descriptor.wide_voltage_mode_u08 = 1;
	result = PFDL_Open(&descriptor);
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
				NOP();
				result = PFDL_Handler();
			}
		}
	} while (retry-- != 0 && result == PFDL_ERR_ERASE);
	PFDL_Close();
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
	pfdl_descriptor_t descriptor;

	descriptor.fx_MHz_u08 = 8;
	descriptor.wide_voltage_mode_u08 = 1;
	result = PFDL_Open(&descriptor);
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
	PFDL_Close();
	return result;
}

/* reads bytes of the data flash */
unsigned char flashRead(void *buffer, unsigned char *address, unsigned char size)
{
	unsigned char result;
	pfdl_request_t request;
	pfdl_descriptor_t descriptor;

	descriptor.fx_MHz_u08 = 8;
	descriptor.wide_voltage_mode_u08 = 1;
	result = PFDL_Open(&descriptor);
	if (result != PFDL_OK)
		return result;
	request.data_pu08 = (unsigned char __near *)buffer;
	request.command_enu = PFDL_CMD_READ_BYTES;
	request.index_u16 = (unsigned int)address - FLASH_BASE;
	request.bytecount_u16 = size;
	result = PFDL_Execute(&request);
	PFDL_Close();
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
