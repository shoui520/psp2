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
#if ERNIE_HW == 0x00415000 && ERNIE_FW == 0x01000306
#define APP_TIME_STAMP	"$Date:: 2011-05-13 13:18:11 +0900#$"
#endif
#else
#define APP_VERSION	ERNIE_FW
#if ERNIE_HW == 0x00415000 && ERNIE_FW == 0x01000306
#define APP_TIME_STAMP	"$Date:: 2012-01-26 14:43:38 +0900#$"
#endif
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
