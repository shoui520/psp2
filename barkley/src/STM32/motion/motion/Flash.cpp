/*
 * Flash: a write starts on a page (1 KiB) and erases the pages it covers, then
 * programs the data a word at a time; a failure is reported with the address.
 */

#include <stdio.h>

extern "C" {
#include "stm32f10x_lib.h"
}
#include "Flash.h"

#define FLASH_PAGE_SIZE		1024

/* the flash unlocked for the write, locked again after it */
s32 Flash::Program(u32 dst, const u8 *src, u32 size)
{
	s32 result;

	FLASH_Unlock();
	result = Write(dst, src, size);
	FLASH_Lock();
	return result;
}

/* the bytes written, or -1 */
s32 Flash::Write(u32 dst, const u8 *src, u32 size)
{
	const u32 *word = (const u32 *)src;
	s32 pages;
	u32 page;
	s32 i;

	if (dst % FLASH_PAGE_SIZE != 0) {
		printf("Flash::Write() invalid dst =0x%08X.\n", dst);
		return -1;
	}
	pages = (size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
	FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	page = dst;
	for (i = 0; i < pages; i++) {
		if (FLASH_ErasePage(page) != FLASH_COMPLETE) {
			printf("Flash::Write() FLASH_ErasePage(0x%08X) failed.\n", page);
			return -1;
		}
		page += FLASH_PAGE_SIZE;
	}
	for (u32 n = size; n != 0; n -= 4) {
		if (FLASH_ProgramWord(dst, *word) != FLASH_COMPLETE) {
			printf("Flash::Write() FLASH_ProgramWord() dst=0x%08X, src=0x%08X, data=0x%08X failed.\n",
			       dst, word, *word);
			return -1;
		}
		dst += 4;
		word++;
	}
	return size;
}
