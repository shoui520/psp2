/*
 * The updater's version block at 0x1f00 (version.h), which the application reads too.
 */
#include "ernie.h"
#include "version.h"

const struct UpdaterVersionInfo updaterVersionInfo = {
	{ 0x01, 0x00, 0x33, 0x00 },
	{ ERNIE_HW & 0xff, (ERNIE_HW >> 8) & 0xff, (ERNIE_HW >> 16) & 0xff, ERNIE_HW >> 24 },
	{ 0x00, 0x08, 0x09, 0x00 },
	"$Date:: 2011-05-13 13:18:11 +0900#$",
	"$Date:: 2011-05-13 13:11:31 +0900#$"
};

/* RESF at the start of the main loop, reported with the version (serial1.c); near since
   0x0100060D, with the id records (as the application's since 0x01000107) */
#if FW_SINCE(0x0100060D)
__near unsigned char resetFlags;
#else
unsigned char resetFlags;
#endif

#if FW_SINCE(0x0100060D)
/* since 0x0100060D the application's id records and their accessors too (sysinfo.c's) */
__near unsigned int idRecordsLength;
__near unsigned long idRecord1;

unsigned char getResetFlags(void)
{
	return resetFlags;
}

unsigned long getIdRecord1(void)
{
	return idRecord1;
}

unsigned int getIdRecordsLength(void)
{
	return idRecordsLength;
}
#endif
