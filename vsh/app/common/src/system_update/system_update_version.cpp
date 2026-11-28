/*
 * system_update_version.cpp: the system software version and the console's region as the
 * system update needs them (0x81135E8C-0x81136464; a file of ours: no record or string names
 * Sony's, and the system update's other programs (NPXS10015) build these functions together
 * apart from the update list parser): a version's number from its text, the console's version
 * as text, its CDN region, the update's size and the list's log.
 */
#include <stddef.h>
#include <stdarg.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <common/char_encoding.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <ss_mgr/ss_mgr.h>
#include <update_mgr/update_mgr.h>
#include <updater_core/updater_core.h>
#include <vshbridge/vshbridge.h>

#include "system_update_registry.h"
#include "system_update_version.h"

using namespace paf;

#define SYSTEM_UPDATE_ERROR_ARGUMENT	((int)0x80182005)	/* PLACEHOLDER name */
#define PSCODE_PRODUCT_JAPAN		0x103			/* the CEX product codes */

static SceInt64 s_updateSize;		/* PLACEHOLDER: nothing reads it */

static inline bool isHexDigit(char ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

/*
 * "x.yyy" or "x.yyy.zzz" (x one or two digits, may be empty) as the number 0xXXYYYZZZ
 * (zzz 000 when missing), -1 when it is not one
 */
int Bgdl_81135E8C(string version)
{
	char number[12];
	size_t pos;
	int n;

	if (version.size() > 10)
		return -1;
	n = 0;
	if (version.at(0) == '.') {
		number[n++] = '0';
		number[n++] = '0';
		pos = 0;
	} else if (version.at(1) == '.') {
		number[n++] = '0';
		number[n++] = version.at(0);
		pos = 1;
	} else if (version.at(2) == '.') {
		number[n++] = version.at(0);
		number[n++] = version.at(1);
		pos = 2;
	} else {
		return -1;
	}
	pos++;
	if (version.size() - pos < 3)
		return -1;
	for (unsigned int i = 0; i < 3; i++)
		number[n++] = version.at(pos++);
	if (version.size() != pos && version.at(pos) != '.')
		return -1;
	if (version.size() == pos) {
		for (unsigned int i = 0; i < 3; i++)
			number[n++] = '0';
	} else {
		if (version.size() != pos + 4)
			return -1;
		pos++;
		for (unsigned int i = 0; i < 3; i++)
			number[n++] = version.at(pos++);
	}
	number[n] = '\0';
	for (int i = 0; i < n; i++) {
		if (!isHexDigit(number[i]))
			return -1;
	}
	return sce_paf_strtoul(number, NULL, 16);
}

/* the console's system software version, "3.650" */
string Bgdl_811360AA(void)
{
	char text[24];
	SceUpdaterDeviceInfo version;
	SceKernelSystemSwVersion info;

	info.size = sizeof(info);
	if (sceKernelGetSystemSwVersion(&info) == 0)
		return string(info.versionString);

	int major = 0xFF;
	int minor = 0xFFF;

	if (sceUpdaterGetDeviceInfo(&version) >= 0) {
		major = version.m_version >> 24;
		minor = (version.m_version >> 12) & 0xFFF;
	}
	sce_paf_snprintf(text, sizeof(text), "%x.%03x", major, minor);
	return string(text);
}

wstring Bgdl_8113614A(void)
{
	string version = Bgdl_811360AA();
	wstring result;

	common::Utf8ToUtf16(version, &result, 0);
	return result;
}

int Bgdl_811361A0(int *productCode)
{
	ScePsCode code;
	int ret = vshSblAimgrGetPscode(&code);

	if (ret == 0)
		*productCode = code.product_code;
	return ret;
}

/*
 * the console's CDN region: a retail console's by its product code ("jp" by default),
 * any other its product code in decimal
 */
int Bgdl_811361D4(string *region)
{
	if (region == NULL)
		return SYSTEM_UPDATE_ERROR_ARGUMENT;

	int ret = vshSblAimgrIsCEX();

	if (ret < 0 || ret == 1) {
		ScePsCode code;
		int product = 0;

		ret = vshSblAimgrGetPscode(&code);
		if (ret == 0)
			product = code.product_code;
		if (ret < 0)
			product = PSCODE_PRODUCT_JAPAN;
		switch (product) {
		case 0x103:
			*region = "jp";
			break;
		case 0x104:
			*region = "us";
			break;
		case 0x105:
			*region = "eu";
			break;
		case 0x106:
			*region = "kr";
			break;
		case 0x107:
			*region = "uk";
			break;
		case 0x108:
			*region = "mx";
			break;
		case 0x109:
			*region = "au";
			break;
		case 0x10A:
			*region = "sa";
			break;
		case 0x10B:
			*region = "tw";
			break;
		case 0x10C:
			*region = "ru";
			break;
		case 0x10D:
			*region = "cn";
			break;
		default:
			*region = "jp";
			break;
		}
		return 0;
	}

	ScePsCode code;
	int product = 0;
	char text[16];

	ret = vshSblAimgrGetPscode(&code);
	if (ret == 0)
		product = code.product_code;
	if (ret < 0)
		return ret;
	sce_paf_snprintf(text, sizeof(text), "%d", product);
	*region = text;
	return 0;
}

void Bgdl_811363D6(SceInt64 size)
{
	s_updateSize = size;
}

/* the update list's log: formatted, then dropped (the release build prints nothing) */
void Bgdl_811363E4(bool enabled, const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (enabled) {
		char text[1024] = "";

		sce_paf_vsnprintf(text, sizeof(text) - sce_paf_strlen(text), fmt, ap);
		sce_paf_printf_dummy();
	}
	va_end(ap);
}

/* (0x81132A2C) */
static int getSpkgVersionByKind(int kind, SceUInt32 *outVersion)
{
	if (outVersion == NULL)
		return SYSTEM_UPDATE_ERROR_ARGUMENT;

	int packageType;
	switch (kind) {
	case 0:
		packageType = SCE_SBL_US_SPKG_TYPE_SYSTEM;
		break;
	case 1:
		packageType = 0x0A;
		break;
	case 2:
		packageType = SCE_SBL_US_SPKG_TYPE_MANUFACTURING;
		break;
	case 3:
		packageType = 0x0D;
		break;
	default:
		return SYSTEM_UPDATE_ERROR_ARGUMENT;
	}

	SceSblUsSpkgInfo spkg;
	int ret = sceSblUsGetSpkgInfo(packageType, &spkg);
	if (ret < 0)
		return ret;
	*outVersion = spkg.version;
	return 0;
}

/* (0x81132AA2) */
int shellIsComTestMode(int *error)
{
	int localErr = 0;
	int *errOut = (error != NULL) ? error : &localErr;
	SceUInt32 version = 0;
	int ret = getSpkgVersionByKind(2, &version);
	if (ret < 0) {
		sce_paf_printf_dummy();
		*errOut = 2;
		return 0;
	}
	if (version == 0x01000010u)
		return 1;
	sce_paf_printf_dummy();
	return 0;
}
