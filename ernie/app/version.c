/*
 * The versions Kermit reads: the application's version and hardware info (the hardware
 * ID's low byte with flags the settings choose), and the downloader's version.
 */
#include <string.h>
#include "ernie.h"
#include "board.h"
#include "kernel.h"
#include "version.h"
#ifdef ERNIE_UPDATER
#include "../updater/updater.h"
#else
#include "command.h"	/* timeStampDigits: the updater has a function of that name */
#endif

struct VersionFlags {
	unsigned char	hardwareInfoFlags;	/* ORed into the hardware info's low byte */
#if FW_SINCE(0x00090903) && (!defined(ERNIE_UPDATER) || FW_SINCE(0x0100060D))
	unsigned char	dlFlags;		/* byte 0x2d of NVS area 1 (task 2 reads it); bit 0:
						   report the downloader's version (0x00090804 always
						   does) */
#endif
#if SOURCE_0103
	unsigned char	hardwareInfo2[16];	/* the slim's: the bits of hardwareInfo2 flipped */
#endif
};

/* Subversion's $Date$ of each build, and the version it gives */
#define APP_VERSION	APP_FW

/* The source-control timestamp stored in each supported firmware patch. */
#if APP_FW == 0x00080003
#define APP_TIME_STAMP	"$Date:: 2010-07-13 23:53:42 +0900#$"
#elif APP_FW == 0x00080302
#define APP_TIME_STAMP	"$Date:: 2010-08-31 20:58:01 +0900#$"
#elif APP_FW == 0x00080307
#define APP_TIME_STAMP	"$Date:: 2010-10-05 12:24:46 +0900#$"
#elif APP_FW == 0x00080502
#define APP_TIME_STAMP	"$Date:: 2010-11-12 20:53:51 +0900#$"
#elif APP_FW == 0x00080504
#define APP_TIME_STAMP	"$Date:: 2010-11-30 19:50:00 +0900#$"
#elif APP_FW == 0x00090006
#define APP_TIME_STAMP	"$Date:: 2011-01-18 10:45:54 +0900#$"
#elif APP_FW == 0x00090007
#define APP_TIME_STAMP	"$Date:: 2011-01-24 15:58:12 +0900#$"
#elif APP_FW == 0x00090009
#define APP_TIME_STAMP	"$Date:: 2011-02-14 18:18:16 +0900#$"
#elif APP_FW == 0x00090203
#define APP_TIME_STAMP	"$Date:: 2011-03-07 11:46:37 +0900#$"
#elif APP_FW == 0x00090204
#define APP_TIME_STAMP	"$Date:: 2011-03-09 18:36:33 +0900#$"
#elif APP_FW == 0x00090206
#define APP_TIME_STAMP	"$Date:: 2011-03-24 18:14:50 +0900#$"
#elif APP_FW == 0x00090502
#define APP_TIME_STAMP	"$Date:: 2011-04-21 21:34:42 +0900#$"
#elif APP_FW == 0x00090804
#define APP_TIME_STAMP	"$Date:: 2011-05-23 17:11:08 +0900#$"
#elif APP_FW == 0x00090903
#define APP_TIME_STAMP	"$Date:: 2011-07-05 22:09:22 +0900#$"
#elif APP_FW == 0x00090800
#define APP_TIME_STAMP	"$Date:: 2011-05-13 13:18:11 +0900#$"
#elif APP_FW == 0x01000104
#define APP_TIME_STAMP	"$Date:: 2011-08-29 21:22:43 +0900#$"
#elif APP_FW == 0x01000107
#define APP_TIME_STAMP	"$Date:: 2011-09-08 07:34:21 +0900#$"
#elif APP_FW == 0x01000109
#define APP_TIME_STAMP	"$Date:: 2011-09-26 17:55:05 +0900#$"
#elif APP_FW == 0x0100010A
#define APP_TIME_STAMP	"$Date:: 2011-10-20 14:16:06 +0900#$"
#elif APP_FW == 0x0100010B
#define APP_TIME_STAMP	"$Date:: 2011-11-29 17:21:05 +0900#$"
#elif APP_FW == 0x01000306
#define APP_TIME_STAMP	"$Date:: 2012-01-26 14:43:38 +0900#$"
#elif APP_FW == 0x0100060B
#define APP_TIME_STAMP	"$Date:: 2012-07-31 22:13:45 +0900#$"
#elif APP_FW == 0x0100060D
#define APP_TIME_STAMP	"$Date:: 2012-11-08 17:04:38 +0900#$"
#elif APP_FW == 0x01030505
#define APP_TIME_STAMP	"$Date:: 2013-06-04 15:50:28 +0900#$"
#elif APP_FW == 0x01030511
#define APP_TIME_STAMP	"$Date:: 2013-12-13 15:52:05 +0900#$"
#elif APP_FW == 0x01030603
#define APP_TIME_STAMP	"$Date:: 2013-06-04 19:00:23 +0900#$"
#elif APP_FW == 0x01040301
#define APP_TIME_STAMP	"$Date:: 2014-07-01 14:30:57 +0900#$"
#elif APP_FW == 0x01050030
#define APP_TIME_STAMP	"$Date:: 2015-02-19 15:40:52 +0900#$"
#elif APP_FW == 0x01050031
#define APP_TIME_STAMP	"$Date:: 2015-03-02 17:31:33 +0900#$"
#elif APP_FW == 0x01050034
#define APP_TIME_STAMP	"$Date:: 2015-03-30 16:51:49 +0900#$"
#else
#error Unknown Ernie firmware timestamp
#endif

/* the version block at 0xf00 */
const struct AppVersionInfo appVersionInfo = {
	{ ERNIE_HW & 0xff, (ERNIE_HW >> 8) & 0xff, (ERNIE_HW >> 16) & 0xff, ERNIE_HW >> 24 },
	{ APP_VERSION & 0xff, (APP_VERSION >> 8) & 0xff, (APP_VERSION >> 16) & 0xff,
	  APP_VERSION >> 24 }
};
static const char timeStamp[] = APP_TIME_STAMP;
const char * const appTimeStamp = timeStamp;
#if SOURCE_0103
/* the hardware info 2 (command 0x0006): 128 bits, each this default flipped by its bit
   in versionFlags.hardwareInfo2 */
#if BOARD_DOL1002
static const unsigned char hardwareInfo2[16] = { 0x00, 0x01 };
/* the layout of the log's records (app/log.c) */
const unsigned int logVersion = 0x14;
#elif BOARD_USS1001 && SOURCE_0104
static const unsigned char hardwareInfo2[16] = { 0x47, 0x02 };
/* the layout of the log's records (app/log.c) */
const unsigned int logVersion = 0x21;
#elif BOARD_DOL1001
static const unsigned char hardwareInfo2[16] = { 0x00 };
/* the layout of the log's records (app/log.c) */
const unsigned int logVersion = 0x10;
#else
static const unsigned char hardwareInfo2[16] = { 0x07 };
const unsigned int logVersion = 9;
#endif
#if defined(ERNIE_UPDATER)
/* the time stamp's digits (the application's command.c has them) */
const unsigned char timeStampDigits[12] = { 8, 9, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23 };
#endif
#endif

struct VersionFlags versionFlags;

#if defined(ERNIE_UPDATER) && SOURCE_0103
/*
 * The PS TV's updater: the application's set, the running application's version and build
 * (updVersion, updBuild) copied at the start
 */
void versionInit(void)
{
	memcpy(updVersion, APP_VERSION_INFO->version, 4);
	copyTimeStamp(updBuild, APP_VERSION_INFO->timeStamp);
	versionFlags.dlFlags = 1;
	versionFlags.hardwareInfoFlags = 0;
	memset(versionFlags.hardwareInfo2, 0, sizeof(versionFlags.hardwareInfo2));
}

void getErnieVersion(unsigned char *buf)
{
	memcpy(buf, updVersion, 4);
}

void getHardwareInfo(unsigned char *buf)
{
	memcpy(buf, APP_VERSION_INFO->hardwareInfo, 4);
	buf[0] |= versionFlags.hardwareInfoFlags;
}

void getHardwareInfo2(unsigned char *buf)
{
	unsigned char i;

	for (i = 0; i < sizeof(hardwareInfo2); i++)
		*buf++ = hardwareInfo2[i] ^ versionFlags.hardwareInfo2[i];
}

unsigned char getHardwareInfoFlags(void)
{
	return versionFlags.hardwareInfoFlags | appVersionInfo.hardwareInfo[0];
}

char getHardwareInfo2Bit(unsigned char number)
{
	unsigned char byte;

	if (number >= 0x80)
		return 0;
	byte = number >> 3;
	return ((hardwareInfo2[byte] ^ versionFlags.hardwareInfo2[byte]) &
		(1 << (number & 7))) ? 1 : 0;
}

void setHardwareInfoFlags(unsigned char flags)
{
	versionFlags.hardwareInfoFlags |= flags;
}

void clearHardwareInfoFlags(unsigned char flags)
{
	versionFlags.hardwareInfoFlags &= ~flags;
}

void setHardwareInfo2Bit(unsigned char number, unsigned char on)
{
	unsigned char index;
	unsigned char mask;

	if (number < 0x80) {
		index = number >> 3;
		mask = (unsigned char)1 << (unsigned char)(number & 7);
		if ((hardwareInfo2[index] & mask) ^ (on ? mask : 0))
			versionFlags.hardwareInfo2[index] |= mask;
		else
			versionFlags.hardwareInfo2[index] &= ~mask;
	}
}

void setDlFlags(unsigned char flags)
{
	versionFlags.dlFlags = flags;
}

void getErnieDlVersion(unsigned char *buf)
{
	memcpy(buf, APP_VERSION_INFO->dlVersion, 4);
}

/* command 0x0002: the build's digits */
void getTimeStamp(unsigned char *out)
{
	memcpy(out, updBuild, 12);
}

/* a time stamp's 12 digits (the image's and the application's, into updBuild) */
int copyTimeStamp(char *out, const char *stamp)
{
	unsigned char i;

	for (i = 0; i < 12; i++)
		out[i] = stamp[timeStampDigits[i]];
	return 0;
}
#elif defined(ERNIE_UPDATER)
/*
 * The updater's: the running application's version (updVersion), and the hardware info and
 * the downloader's version from the application's block as the updater's layout reads it
 */
void versionInit(void)
{
	versionFlags.hardwareInfoFlags = 0;
#if FW_SINCE(0x0100060D)
	versionFlags.dlFlags = 0xff;
#endif
}

void getErnieVersion(unsigned char *buf)
{
	memcpy(buf, updVersion, 4);
}

void getHardwareInfo(unsigned char *buf)
{
	memcpy(buf, APP_VERSION_INFO->hardwareInfo, 4);
	buf[0] |= versionFlags.hardwareInfoFlags;
}

unsigned char getHardwareInfoFlags(void)
{
	return versionFlags.hardwareInfoFlags | appVersionInfo.hardwareInfo[0];
}

void setHardwareInfoFlags(unsigned char flags)
{
	versionFlags.hardwareInfoFlags |= flags;
}

#if FW_SINCE(0x0100060D)
void clearHardwareInfoFlags(unsigned char flags)
{
	versionFlags.hardwareInfoFlags &= ~flags;
}

/* kept, not read: the updater always reports the downloader's version */
void setDlFlags(unsigned char flags)
{
	versionFlags.dlFlags = flags;
}
#endif

void getErnieDlVersion(unsigned char *buf)
{
	memcpy(buf, APP_VERSION_INFO->dlVersion, 4);
}
#else
void versionInit(void)
{
#if SOURCE_0103
	versionFlags.dlFlags = 0xff;
	versionFlags.hardwareInfoFlags = 0;
	memset(versionFlags.hardwareInfo2, 0, sizeof(versionFlags.hardwareInfo2));
#else
	versionFlags.hardwareInfoFlags = 0;
#if FW_SINCE(0x00090903)
	versionFlags.dlFlags = 0xff;
#endif
#endif
}

void getErnieVersion(unsigned char *buf)
{
	memcpy(buf, appVersionInfo.version, 4);
}

void getHardwareInfo(unsigned char *buf)
{
	memcpy(buf, appVersionInfo.hardwareInfo, 4);
	buf[0] |= versionFlags.hardwareInfoFlags;
}

#if SOURCE_0103
void getHardwareInfo2(unsigned char *buf)
{
	unsigned char i;

	for (i = 0; i < sizeof(hardwareInfo2); i++)
		*buf++ = hardwareInfo2[i] ^ versionFlags.hardwareInfo2[i];
}
#endif

#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
unsigned char getHardwareInfoFlags(void)
{
	return versionFlags.hardwareInfoFlags | appVersionInfo.hardwareInfo[0];
}
#endif

#if SOURCE_0103
/* one bit of the hardware info 2 (0-127; 0 for any other) */
char getHardwareInfo2Bit(unsigned char number)
{
#if BOARD_DOL1001 || SOURCE_0104
	unsigned char byte;
#endif

	if (number >= 0x80)
		return 0;
#if BOARD_DOL1001 || SOURCE_0104
	byte = number >> 3;
	return ((hardwareInfo2[byte] ^ versionFlags.hardwareInfo2[byte]) &
		(1 << (number & 7))) ? 1 : 0;
#else
	return ((hardwareInfo2[number >> 3] ^ versionFlags.hardwareInfo2[number >> 3]) &
		(1 << (number & 7))) ? 1 : 0;
#endif
}
#endif

void setHardwareInfoFlags(unsigned char flags)
{
	dis_dsp();
	versionFlags.hardwareInfoFlags |= flags;
	ena_dsp();
	return;
}

#if FW_SINCE(0x01000104)
void clearHardwareInfoFlags(unsigned char flags)
{
	dis_dsp();
	versionFlags.hardwareInfoFlags &= ~flags;
	ena_dsp();
	return;
}
#endif

#if SOURCE_0103
/* sets one bit of the hardware info 2 to on (bits past 127 are left alone) */
void setHardwareInfo2Bit(unsigned char number, unsigned char on)
{
	unsigned char index;
	unsigned char mask;

	if (number < 0x80) {
		index = number >> 3;
		mask = (unsigned char)1 << (unsigned char)(number & 7);
		dis_dsp();
		if ((hardwareInfo2[index] & mask) ^ (on ? mask : 0))
			versionFlags.hardwareInfo2[index] |= mask;
		else
			versionFlags.hardwareInfo2[index] &= ~mask;
		ena_dsp();
	}
}
#endif

#if FW_SINCE(0x00090903)
void setDlFlags(unsigned char flags)
{
	versionFlags.dlFlags = flags;
}

void getErnieDlVersion(unsigned char *buf)
{
	unsigned long none;

	none = 0;
	if (versionFlags.dlFlags & 0x01)
		memcpy(buf, UPDATER_VERSION_INFO->dlVersion, 4);
	else
		memcpy(buf, &none, 4);
}

#if SOURCE_0103
static int copyTimeStamp(unsigned char *out, const char *stamp);

/* the slim's: the time stamp's digits (command 0x0002, serial 0x0107) */
void getTimeStamp(unsigned char *out)
{
	copyTimeStamp(out, appTimeStamp);
}

static int copyTimeStamp(unsigned char *out, const char *stamp)
{
	unsigned char i;

	for (i = 0; i < 12; i++)
		out[i] = stamp[timeStampDigits[i]];
	return 0;
}
#endif
#elif FW_SINCE(0x00080502)	/* 0x00080307's host command copies it itself */
void getErnieDlVersion(unsigned char *buf)
{
	memcpy(buf, UPDATER_VERSION_INFO->dlVersion, 4);
}
#endif
#endif
