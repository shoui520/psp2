/*
 * Chip, baseband and RF version registers, and the version strings the
 * host reads with the extended version command. The filename is inferred.
 */
#include <stddef.h>
#include "robin/osa.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"

/* Versions read from the radio at start-up; their loaders are not converted. */
uint8_t rfVersion __attribute__((section(".itcm.rw")));
uint8_t rfSubVersion __attribute__((section(".itcm.rw")));
static uint8_t bbpVersionMajorCache __attribute__((section(".itcm.rw")));
static uint8_t bbpVersionMinorCache __attribute__((section(".itcm.rw")));
uint8_t opRegionMinor __attribute__((section(".itcm.rw")));
uint8_t opRegionMajor __attribute__((section(".itcm.rw")));
uint8_t calRegionMinor __attribute__((section(".dtcm.rw")));
uint8_t calRegionMajor __attribute__((section(".dtcm.rw")));

/* Selector of the previous banner request; 88 arms the build string (99). */
static char versionBannerSelector __attribute__((section(".itcm.rw")));
static char driverVersion[8] __attribute__((section(".itcm.rw"))) = "2.3.1.0";

/* Part of the firmware identification; the exact release text. */
static const char firmwareId[] = "w8787-Ax, RF878X, FP65, 14.65.9.p223, BT_SDIO $";
static const char firmwareBuild[] = "RC: , yhou, 2014/07/09, 12:09:01";

uint32_t socGetChipRevision(void)
{
	return (uint8_t)SOC_CHIP_REVISION;
}

uint32_t bbpReadVersionRegister(uint32_t index)
{
	return (uint8_t)BBP_VERSION_REGISTERS[index];
}

uint32_t bbpGetVersionMajor(void)
{
	bbpVersionMajorCache = bbpReadVersionRegister(0);
	return bbpVersionMajorCache;
}

uint32_t bbpGetVersionMinor(void)
{
	bbpVersionMinorCache = bbpReadVersionRegister(1);
	return bbpVersionMinorCache;
}

uint32_t bbpGetVersionPatch(void)
{
	return 0;
}

uint32_t rfGetVersion(void)
{
	return rfVersion;
}

uint32_t rfGetSubVersion(void)
{
	return rfSubVersion;
}

static void versionAppendByte(char *buffer, uint32_t value)
{
	osaFormatHexByte(value, buffer + strlen(buffer));
}

static void versionAppendChar(char *buffer, char c)
{
	char *end = buffer + strlen(buffer);

	end[0] = c;
	end[1] = '\0';
}

static void versionAppend(char *buffer, const char *text)
{
	strcpy(buffer + strlen(buffer), text);
}

/*
 * Builds the text for one version selector into buffer and remembers the
 * selector. Selectors: 0 firmware id, 1 chip, baseband and RF versions, 2
 * the O.R. revisions, 10 driver version, 99 build string (only straight
 * after selector 88). The result points at the stored selector.
 */
char *osaFormatVersionBanner(int selector, char *buffer)
{
	switch (selector) {
	case 0:
		strcpy(buffer, firmwareId);
		buffer[strlen(buffer) - 2] = '\0';
		break;
	case 1:
		versionAppend(buffer, "ChipRev:");
		versionAppendByte(buffer, socGetChipRevision());
		versionAppend(buffer, ", ");
		versionAppend(buffer, "BB:");
		versionAppendByte(buffer, bbpGetVersionMajor());
		versionAppendChar(buffer, '(');
		versionAppendByte(buffer, bbpGetVersionMinor());
		versionAppendChar(buffer, '.');
		versionAppendByte(buffer, bbpGetVersionPatch());
		versionAppend(buffer, "), RF:");
		versionAppendByte(buffer, rfGetVersion());
		versionAppendChar(buffer, '(');
		versionAppendByte(buffer, rfGetSubVersion());
		versionAppendChar(buffer, ')');
		break;
	case 2:
		strcpy(buffer, "O.R.:");
		versionAppendByte(buffer, opRegionMajor);
		versionAppendChar(buffer, '.');
		versionAppendByte(buffer, opRegionMinor);
		versionAppend(buffer, "  Cal O.R.:");
		versionAppendByte(buffer, calRegionMajor);
		versionAppendChar(buffer, '.');
		versionAppendByte(buffer, calRegionMinor);
		break;
	case 10:
		strcpy(buffer, driverVersion);
		break;
	case 99:
		if (versionBannerSelector == 88)
			strcpy(buffer, firmwareBuild);
		break;
	}
	versionBannerSelector = selector;
	return &versionBannerSelector;
}
