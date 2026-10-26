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

/* RESF at the start of the main loop, reported with the version (serial1.c) */
unsigned char resetFlags;
