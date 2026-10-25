/*
 * The versions Kermit reads: the application's version and hardware info (the hardware
 * ID's low byte with flags the settings choose), and the downloader's version.
 */
#include <string.h>
#include "ernie.h"
#include "kernel.h"
#include "version.h"

struct VersionFlags {
	unsigned char	hardwareInfoFlags;	/* ORed into the hardware info's low byte */
	unsigned char	dlFlags;		/* byte 0x2d of NVS area 1 (task 2 reads it); bit 0:
						   report the downloader's version */
};

struct VersionFlags versionFlags;

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
