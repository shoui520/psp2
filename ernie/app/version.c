/*
 * The versions Kermit reads: the application's version and hardware info (the hardware
 * ID's low byte with flags the settings choose), and the downloader's version.
 */
#include <string.h>
#include "ernie.h"
#include "kernel.h"
#include "version.h"
#ifdef ERNIE_UPDATER
#include "../updater/updater.h"
#endif

struct VersionFlags {
	unsigned char	hardwareInfoFlags;	/* ORed into the hardware info's low byte */
#ifndef ERNIE_UPDATER
	unsigned char	dlFlags;		/* byte 0x2d of NVS area 1 (task 2 reads it); bit 0:
						   report the downloader's version */
#endif
};

/* Subversion's $Date$ of each build, and the version it gives */
#ifdef ERNIE_UPDATER
#define APP_VERSION	0x00090800
#else
#define APP_VERSION	ERNIE_FW
#endif

/* The source-control timestamp stored in each supported firmware patch. */
#if ERNIE_FW == 0x00080003
#define APP_TIME_STAMP	"$Date:: 2010-07-13 23:53:42 +0900#$"
#elif ERNIE_FW == 0x00080302
#define APP_TIME_STAMP	"$Date:: 2010-08-31 20:58:01 +0900#$"
#elif ERNIE_FW == 0x00080307
#define APP_TIME_STAMP	"$Date:: 2010-10-05 12:24:46 +0900#$"
#elif ERNIE_FW == 0x00080502
#define APP_TIME_STAMP	"$Date:: 2010-11-12 20:53:51 +0900#$"
#elif ERNIE_FW == 0x00080504
#define APP_TIME_STAMP	"$Date:: 2010-11-30 19:50:00 +0900#$"
#elif ERNIE_FW == 0x00090006
#define APP_TIME_STAMP	"$Date:: 2011-01-18 10:45:54 +0900#$"
#elif ERNIE_FW == 0x00090007
#define APP_TIME_STAMP	"$Date:: 2011-01-24 15:58:12 +0900#$"
#elif ERNIE_FW == 0x00090009
#define APP_TIME_STAMP	"$Date:: 2011-02-14 18:18:16 +0900#$"
#elif ERNIE_FW == 0x00090203
#define APP_TIME_STAMP	"$Date:: 2011-03-07 11:46:37 +0900#$"
#elif ERNIE_FW == 0x00090204
#define APP_TIME_STAMP	"$Date:: 2011-03-09 18:36:33 +0900#$"
#elif ERNIE_FW == 0x00090206
#define APP_TIME_STAMP	"$Date:: 2011-03-24 18:14:50 +0900#$"
#elif ERNIE_FW == 0x00090502
#define APP_TIME_STAMP	"$Date:: 2011-04-21 21:34:42 +0900#$"
#elif ERNIE_FW == 0x00090804
#define APP_TIME_STAMP	"$Date:: 2011-05-23 17:11:08 +0900#$"
#elif ERNIE_FW == 0x00090903
#define APP_TIME_STAMP	"$Date:: 2011-07-05 22:09:22 +0900#$"
#elif ERNIE_FW == 0x01000104
#define APP_TIME_STAMP	"$Date:: 2011-08-29 21:22:43 +0900#$"
#elif ERNIE_FW == 0x01000107
#define APP_TIME_STAMP	"$Date:: 2011-09-08 07:34:21 +0900#$"
#elif ERNIE_FW == 0x01000109
#define APP_TIME_STAMP	"$Date:: 2011-09-26 17:55:05 +0900#$"
#elif ERNIE_FW == 0x0100010A
#define APP_TIME_STAMP	"$Date:: 2011-10-20 14:16:06 +0900#$"
#elif ERNIE_FW == 0x0100010B
#define APP_TIME_STAMP	"$Date:: 2011-11-29 17:21:05 +0900#$"
#elif ERNIE_FW == 0x01000306
#ifdef ERNIE_UPDATER
#define APP_TIME_STAMP	"$Date:: 2011-05-13 13:18:11 +0900#$"
#else
#define APP_TIME_STAMP	"$Date:: 2012-01-26 14:43:38 +0900#$"
#endif
#elif ERNIE_FW == 0x0100060B
#define APP_TIME_STAMP	"$Date:: 2012-07-31 22:13:45 +0900#$"
#elif ERNIE_FW == 0x0100060D
#define APP_TIME_STAMP	"$Date:: 2012-11-08 17:04:38 +0900#$"
#elif ERNIE_FW == 0x01030505
#define APP_TIME_STAMP	"$Date:: 2013-06-04 15:50:28 +0900#$"
#elif ERNIE_FW == 0x01030511
#define APP_TIME_STAMP	"$Date:: 2013-12-13 15:52:05 +0900#$"
#elif ERNIE_FW == 0x01030603
#define APP_TIME_STAMP	"$Date:: 2013-06-04 19:00:23 +0900#$"
#elif ERNIE_FW == 0x01040301
#define APP_TIME_STAMP	"$Date:: 2014-07-01 14:30:57 +0900#$"
#elif ERNIE_FW == 0x01050031
#define APP_TIME_STAMP	"$Date:: 2015-03-02 17:31:33 +0900#$"
#elif ERNIE_FW == 0x01050034
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

struct VersionFlags versionFlags;

#ifdef ERNIE_UPDATER
/*
 * The updater's: the running application's version (updVersion), and the hardware info and
 * the downloader's version from the application's block as the updater's layout reads it
 */
void versionInit(void)
{
	versionFlags.hardwareInfoFlags = 0;
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

void getErnieDlVersion(unsigned char *buf)
{
	memcpy(buf, APP_VERSION_INFO->dlVersion, 4);
}
#else
void versionInit(void)
{
	versionFlags.hardwareInfoFlags = 0;
	versionFlags.dlFlags = 0xff;
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

unsigned char getHardwareInfoFlags(void)
{
	return versionFlags.hardwareInfoFlags | appVersionInfo.hardwareInfo[0];
}

void setHardwareInfoFlags(unsigned char flags)
{
	dis_dsp();
	versionFlags.hardwareInfoFlags |= flags;
	ena_dsp();
	return;
}

void clearHardwareInfoFlags(unsigned char flags)
{
	dis_dsp();
	versionFlags.hardwareInfoFlags &= ~flags;
	ena_dsp();
	return;
}

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
#endif
