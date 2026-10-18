/*
 * SceSblACMgr: program identity, capability and device access policy, and the
 * filesystem attribute entry points, which dispatch on the attribute's type to
 * the per-type implementations (acmgr_fatfs_attr.c: types 1-5, acmgr_pfs_attr.c:
 * type 6, acmgr_type7_attr.c: type 7, acmgr_idstorage_attr.c: type 8).
 *
 * Exported names: Sony's where another module's strings give them (iofilemgr:
 * sceSblACMgrGetPaid, IsKernelProgram, IsSystemProgram,
 * SetFSAttributeByAttributeWithMountPoint; exfatfs:
 * GetFSAttributeByThreadAccessLevel; syslibtrace: HasCapability, IsGameProgram;
 * applier: GetMediaType), otherwise <Library>_<NID>.
 */
#include <sysmem/ctype.h>
#include <sysmem/sysclib.h>
#include <sysmem/dipsw.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot_acmgr.h>
#include "acmgr_internal.h"

/*
 * Device and path names are stored bitwise inverted. Each rule list ends at its
 * known length, the program path lists at an entry without a mount point.
 */

/* a program and the paths under its mount points it may reach (E79C7A8D) */
typedef struct AcMgrPathRule {
	const SceChar8 *mountPoint;
	SceSize mountPointLen;
	const SceChar8 *path;		/* NULL: the whole mount point */
	SceSize pathLen;
} AcMgrPathRule;

typedef struct AcMgrProgramPaths {
	SceUInt64 programAuthorityId;
	const AcMgrPathRule *rules;
} AcMgrProgramPaths;

/* a device prefix and the answer for programs without / with attribute 0x81 */
typedef struct AcMgrDeviceRule {
	const SceChar8 *prefix;
	SceSize prefixLen;
	SceUInt32 result[2];
} AcMgrDeviceRule;

/* a mount point and the file mode of its root */
typedef struct AcMgrMountPoint {
	const SceChar8 *name;
	SceSize nameLen;
	SceUInt32 mode;
} AcMgrMountPoint;

/* a device name, its unit number, the text after it, and the media type */
typedef struct AcMgrMediaType {
	const SceChar8 *device;
	SceSize deviceLen;
	const SceChar8 *suffix;
	SceSize suffixLen;
	SceUInt32 mediaType;
} AcMgrMediaType;

#define ENC(s)	((const SceChar8 *)(s))

static const AcMgrPathRule webCorePaths[] = {			/* "vs0:" "data/external/cert/" ... */
	{ ENC("\x89\x8c\xcf\xc5"), 4, ENC("\x9b\x9e\x8b\x9e\xd0\x9a\x87\x8b\x9a\x8d\x91\x9e\x93\xd0\x9c\x9a\x8d\x8b\xd0"), 19 },
	{ ENC("\x89\x8c\xcf\xc5"), 4, ENC("\x9b\x9e\x8b\x9e\xd0\x9a\x87\x8b\x9a\x8d\x91\x9e\x93\xd0\x88\x9a\x9d\x9c\x90\x8d\x9a\xd0"), 22 },
	{ NULL, 0, NULL, 0 },
};

static const AcMgrPathRule externalPaths[] = {			/* "vs0:" "data/external", "sys/external" */
	{ ENC("\x89\x8c\xcf\xc5"), 4, ENC("\x9b\x9e\x8b\x9e\xd0\x9a\x87\x8b\x9a\x8d\x91\x9e\x93"), 13 },
	{ ENC("\x89\x8c\xcf\xc5"), 4, ENC("\x8c\x86\x8c\xd0\x9a\x87\x8b\x9a\x8d\x91\x9e\x93"), 12 },
	{ NULL, 0, NULL, 0 },
};

static const AcMgrPathRule certPaths[] = {
	{ ENC("\x89\x8c\xcf\xc5"), 4, ENC("\x9b\x9e\x8b\x9e\xd0\x9a\x87\x8b\x9a\x8d\x91\x9e\x93\xd0\x9c\x9a\x8d\x8b\xd0"), 19 },
	{ ENC("\x89\x8c\xcf\xc5"), 4, ENC("\x9b\x9e\x8b\x9e\xd0\x9a\x87\x8b\x9a\x8d\x91\x9e\x93"), 13 },
	{ ENC("\x89\x8c\xcf\xc5"), 4, ENC("\x8c\x86\x8c\xd0\x9a\x87\x8b\x9a\x8d\x91\x9e\x93"), 12 },
	{ NULL, 0, NULL, 0 },
};

static const AcMgrProgramPaths programPaths[] = {
	{ 0x2800000000008003ULL, webCorePaths },
	{ 0x2800000000008005ULL, webCorePaths },
	{ 0x2800c0101cd2000bULL, externalPaths },
	{ 0x210000101cd20007ULL, externalPaths },
	{ 0x210000101cd20008ULL, externalPaths },
	{ 0x210000101cd2000aULL, externalPaths },
	{ 0x210000101cd20009ULL, externalPaths },
	{ 0x220000101cd2000cULL, certPaths },
	{ 0x220000101cd2000dULL, certPaths },
};

static const SysrootAcmgrOps sysrootOps = {
	.checkRemapCodeForUser = SceSblACMgrForKernel_30575458,
};

static const AcMgrDeviceRule deviceRules[] = {			/* ux0: sd0: pd0: host0: */
	{ ENC("\x8a\x87\xcf\xc5"), 4, { 0, 0 } },
	{ ENC("\x8c\x9b\xcf\xc5"), 4, { 0, 0 } },
	{ ENC("\x8f\x9b\xcf\xc5"), 4, { 0, 0 } },
	{ ENC("\x97\x90\x8c\x8b\xcf\xc5"), 6, { 0, 0 } },
};

static const AcMgrDeviceRule devDeviceRules[] = {
	{ ENC("\x8a\x87\xcf\xc5"), 4, { 1, 1 } },
	{ ENC("\x8c\x9b\xcf\xc5"), 4, { 1, 1 } },
	{ ENC("\x8f\x9b\xcf\xc5"), 4, { 0, 0 } },
	{ ENC("\x97\x90\x8c\x8b\xcf\xc5"), 6, { 1, 1 } },
};

static const AcMgrMountPoint mountPoints[] = {
	{ ENC("\x8c\x9b\xcf\xc5"), 4, 0x186 },
	{ ENC("\x90\x8c\xcf\xc5"), 4, 0x106 },
	{ ENC("\x89\x8c\xcf\xc5"), 4, 0x106 },
	{ ENC("\x89\x9b\xcf\xc5"), 4, 0x106 },
	{ ENC("\x8b\x92\xcf\xc5"), 4, 0x106 },
	{ ENC("\x8a\x8d\xcf\xc5"), 4, 0x106 },
	{ ENC("\x8a\x9b\xcf\xc5"), 4, 0x106 },
	{ ENC("\x8a\x87\xcf\xc5"), 4, 0x106 },
	{ ENC("\x98\x8d\x90\xcf\xc5"), 5, 0x106 },
	{ ENC("\x98\x8d\x88\xcf\xc5"), 5, 0x106 },
	{ ENC("\x8c\x9e\xcf\xc5"), 4, 0x106 },
	{ ENC("\x92\x99\x9e\xcf\xc5"), 5, 0x106 },
	{ ENC("\x92\x99\x9d\xcf\xc5"), 5, 0x106 },
	{ ENC("\x93\x92\x9e\xcf\xc5"), 5, 0x1000000 },
	{ ENC("\x93\x92\x9d\xcf\xc5"), 5, 0x106 },
	{ ENC("\x93\x92\x9c\xcf\xc5"), 5, 0x106 },
	{ ENC("\x93\x92\x9b\xcf\xc5"), 5, 0x106 },
	{ ENC("\x8f\x9b\xcf\xc5"), 4, 0x106 },
	{ ENC("\x96\x92\x9c\xcf\xc5"), 5, 0x106 },
	{ ENC("\x87\x92\x9c\xcf\xc5"), 5, 0x106 },
	{ ENC("\x8a\x92\x9e\xcf\xc5"), 5, 0x106 },
};

static const AcMgrMediaType mediaTypes[] = {
	{ ENC("\x8c\x9b"), 2, ENC("\xc5"), 1, 0x1 },
	{ ENC("\x90\x8c"), 2, ENC("\xc5"), 1, 0x2 },
	{ ENC("\x89\x8c"), 2, ENC("\xc5"), 1, 0x3 },
	{ ENC("\x89\x9b"), 2, ENC("\xc5"), 1, 0x4 },
	{ ENC("\x8b\x92"), 2, ENC("\xc5"), 1, 0x5 },
	{ ENC("\x8a\x8d"), 2, ENC("\xc5"), 1, 0x6 },
	{ ENC("\x97\x90\x8c\x8b"), 4, ENC("\xc5"), 1, 0x7 },
	{ ENC("\x8a\x9b"), 2, ENC("\xc5"), 1, 0xb },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\x9e\x8f\x8f"), 4, 0x17 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\xd0\x9e\x8f\x8f"), 5, 0x17 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\x8f\x9e\x8b\x9c\x97"), 6, 0x18 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\xd0\x8f\x9e\x8b\x9c\x97"), 7, 0x18 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\x9b\x9e\x8b\x9e"), 5, 0x19 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\xd0\x9b\x9e\x8b\x9e"), 6, 0x19 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\x8a\x8c\x9a\x8d"), 5, 0x0 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5\xd0\x8a\x8c\x9a\x8d"), 6, 0x0 },
	{ ENC("\x8a\x87"), 2, ENC("\xc5"), 1, 0xc },
	{ ENC("\x98\x8d\x90"), 3, ENC("\xc5\x9e\x8f\x8f"), 4, 0xd },
	{ ENC("\x98\x8d\x90"), 3, ENC("\xc5\xd0\x9e\x8f\x8f"), 5, 0xd },
	{ ENC("\x98\x8d\x88"), 3, ENC("\xc5\x8f\x9e\x8b\x9c\x97"), 6, 0xe },
	{ ENC("\x98\x8d\x88"), 3, ENC("\xc5\xd0\x8f\x9e\x8b\x9c\x97"), 7, 0xe },
	{ ENC("\x8c\x9e"), 2, ENC("\xc5"), 1, 0xf },
	{ ENC("\x92\x99\x9e"), 3, ENC("\xc5"), 1, 0x10 },
	{ ENC("\x92\x99\x9d"), 3, ENC("\xc5"), 1, 0x11 },
	{ ENC("\x93\x92\x9e"), 3, ENC("\xc5"), 1, 0x12 },
	{ ENC("\x93\x92\x9d"), 3, ENC("\xc5"), 1, 0x13 },
	{ ENC("\x93\x92\x9c"), 3, ENC("\xc5"), 1, 0x14 },
	{ ENC("\x93\x92\x9b"), 3, ENC("\xc5"), 1, 0x15 },
	{ ENC("\x8f\x9b"), 2, ENC("\xc5"), 1, 0x16 },
	{ ENC("\x8a\x92\x9e"), 3, ENC("\xc5"), 1, 0x1b },
};

/* the first len bytes of s are the inverted bytes of encoded */
static inline int matchEncoded(const SceChar8 *s, const SceChar8 *encoded, SceSize len)
{
	SceSize i;

	for (i = 0; i < len; i++)
		if (s[i] != ~encoded[i])
			break;
	return i == len;
}

/* name unknown: the caller's auth info, and an attribute with a value buffer */
static int getAuthInfoAndCheckAttribute(SceUID pid, SceSblACMgrValue *attr, SceSelfAuthInfo *info)
{
	if (SceSysrootForKernel_4F0A4066(pid, info) != 0)
		return ACMGR_ERROR_AUTH_INFO;
	if (attr == NULL || attr->value == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	return 0;
}

int sceSblACMgrModuleStart(unsigned int arglen, void *argp)
{
	SCE_DBG_PRINTF_LEVEL(2, 0xb, 120, "()\n");
	SceSysrootForKernel_BE1EF51C(&sysrootOps);
	return 0;
}

int SceSblACMgrForKernel_FBA1A256(void)
{
	return 0;
}

int SceSblACMgrForKernel_1948E9DB(void)
{
	return 0;
}

int sceSblACMgrGetMediaType(const SceChar8 *path, SceUInt32 *mediaType)
{
	const AcMgrMediaType *entry;
	const SceChar8 *device, *suffix, *p;
	SceSize deviceLen, suffixLen, i;
	SceUInt32 type;

	if (path == NULL || mediaType == NULL)
		return ACMGR_ERROR_INVALID_PATH;
	*mediaType = 0;
	for (entry = mediaTypes; entry != &mediaTypes[30]; entry++) {
		device = entry->device;
		deviceLen = entry->deviceLen;
		suffix = entry->suffix;
		suffixLen = entry->suffixLen;
		type = entry->mediaType;
		if (deviceLen == 0 || suffixLen == 0)
			continue;
		for (i = 0; i < deviceLen; i++)
			if (path[i] != ~device[i])
				break;
		if (i != deviceLen)
			continue;
		/* the unit number */
		p = path + i;
		while (isdigit(*p))
			p++;
		for (i = 0; i < suffixLen; i++)
			if (p[i] != ~suffix[i])
				break;
		if (i == suffixLen) {
			*mediaType = type;
			return 0;
		}
	}
	return ACMGR_ERROR_AUTH_INFO;
}

int SceSblACMgrForDriver_5F9AF49C(SceUID pid, const SceChar8 *path)
{
	SceSelfAuthInfo info;
	const AcMgrDeviceRule *rules;
	SceUInt64 group;
	SceSize i;
	int restricted, n;

	if (SceSysrootForKernel_4F0A4066(pid, &info) != 0 || path == NULL)
		return 0;
	group = info.programAuthorityId & 0xfff0000000000000ULL;
	if (group == 0x2100000000000000ULL || group == 0x2f00000000000000ULL)
		restricted = (info.attribute[16] >> 6) & 1;
	else if (info.attribute[16] & 0x40)
		restricted = 1;
	else
		return 1;
	rules = SceDipswForDriver_A98FC2FD(0x9f) ? devDeviceRules : deviceRules;
	for (n = 0; n < 4; n++) {
		for (i = 0; i < rules[n].prefixLen; i++)
			if (path[i] != ~rules[n].prefix[i])
				break;
		if (i == rules[n].prefixLen)
			return rules[n].result[restricted];
	}
	return 0;
}

int SceSblACMgrForKernel_2AE6CF27(SceUID pid, SceSblACMgrValue *attr, SceUInt32 mode)
{
	SceSelfAuthInfo info;
	int ret;

	ret = getAuthInfoAndCheckAttribute(pid, attr, &info);
	if (ret != 0)
		return ret;
	switch (attr->type) {
	case 1: case 2: case 3: case 4: case 5:
		return setFatfsAttributeByMode(&info, attr, mode);
	case 6:
		return setPfsAttributeByMode(&info, attr, mode);
	case 7:
		return setType7AttributeByMode(&info, attr, mode);
	case 8:
		return setIdStorageAttributeByMode(&info, attr, mode);
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
}

int SceSblACMgrForDriver_B12CEAA8(SceUID pid, SceSblACMgrValue *attr, const SceSblACMgrValue *source)
{
	SceSelfAuthInfo info;
	int ret;

	ret = getAuthInfoAndCheckAttribute(pid, attr, &info);
	if (ret != 0)
		return ret;
	if (source == NULL || source->value == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	switch (attr->type) {
	case 1: case 2: case 3: case 4: case 5:
		return setFatfsAttributeByAttribute(&info, attr, source);
	case 6:
		return setPfsAttributeByAttribute(&info, attr, source);
	case 7:
		return setType7AttributeByAttribute(&info, attr, source);
	case 8:
		return setIdStorageAttributeByAttribute(&info, attr, source);
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
}

int sceSblACMgrSetFSAttributeByAttributeWithMountPoint(SceUID pid, SceSblACMgrValue *attr,
						       const SceSblACMgrValue *source, SceUInt32 mountPoint)
{
	SceSelfAuthInfo info;
	int ret;

	ret = getAuthInfoAndCheckAttribute(pid, attr, &info);
	if (ret != 0)
		return ret;
	if (source == NULL || source->value == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	switch (attr->type) {
	case 1: case 2: case 3: case 4: case 5:
		return setFatfsAttributeByAttributeWithMountPoint(&info, attr, source, mountPoint);
	case 6: case 7: case 8:
		return ACMGR_ERROR_NOT_SUPPORTED;
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
}

int SceSblACMgrForDriver_BE5667C5(SceUID pid, SceSblACMgrValue *attr, SceUInt32 access)
{
	SceSelfAuthInfo info;
	int ret;

	ret = getAuthInfoAndCheckAttribute(pid, attr, &info);
	if (ret != 0)
		return ret;
	switch (attr->type) {
	case 1: case 2: case 3: case 4: case 5:
		return checkFatfsAttribute(&info, attr, access);
	case 6:
		return checkPfsAttribute(&info, attr, access);
	case 7:
		return checkType7Attribute(&info, attr, access);
	case 8:
		return checkIdStorageAttribute(&info, attr, access);
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
}

int SceSblACMgrForDriver_3B356B98(SceSblACMgrValue *attr, SceUInt32 *mode)
{
	if (attr == NULL || attr->value == NULL || mode == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	switch (attr->type) {
	case 1: case 2: case 3: case 4: case 5:
		return getFatfsModeByAttribute(attr, mode);
	case 6:
		return getPfsModeByAttribute(attr, mode);
	case 7:
		return getType7ModeByAttribute(attr, mode);
	case 8:
		return getIdStorageModeByAttribute(attr, mode);
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
}

int sceSblACMgrGetFSAttributeByThreadAccessLevel(SceUInt32 mode, int level, SceSblACMgrValue *attr)
{
	if (attr == NULL || attr->value == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	switch (attr->type) {
	case 1: case 2: case 3: case 4: case 5:
		return getFatfsAttributeByThreadAccessLevel(mode, level, attr);
	case 6: case 7: case 8:
		return ACMGR_ERROR_NOT_SUPPORTED;
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
}

int SceSblACMgrForDriver_FF7125DE(SceUID pid, const SceChar8 *path, SceUInt32 *mode)
{
	SceSize i;
	int n;

	if (path == NULL || mode == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	for (n = 0; n < 21; n++) {
		for (i = 0; i < mountPoints[n].nameLen; i++)
			if (path[i] != ~mountPoints[n].name[i])
				break;
		if (i == mountPoints[n].nameLen) {
			*mode = mountPoints[n].mode;
			return 0;
		}
	}
	return ACMGR_ERROR_NOT_FOUND;
}

/* Capability and attribute bits are numbered from the most significant bit of byte 0. */
int sceSblACMgrHasCapability(SceUID pid, int bit)
{
	SceSelfAuthInfo info;

	if (SceSysrootForKernel_4F0A4066(pid, &info) != 0)
		return 0;
	return (info.capability[bit >> 3] >> (~bit & 7)) & 1;
}

int SceSblACMgrForKernel_8241AB5C(SceUID pid, int bit)
{
	if (bit != 1 && bit != 0x85)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	if (sceSblACMgrHasCapability(pid, bit) == 1)
		return 0;
	return ACMGR_ERROR_NOT_FOUND;
}

/* name unknown: sceSblACMgrHasCapability for the attribute bits */
static int hasAttribute(SceUID pid, int bit)
{
	SceSelfAuthInfo info;

	if (SceSysrootForKernel_4F0A4066(pid, &info) != 0)
		return 0;
	return (info.attribute[bit >> 3] >> (~bit & 7)) & 1;
}

int sceSblACMgrGetPaid(SceUID pid, SceUInt64 *paid)
{
	SceSelfAuthInfo info;

	if (paid == NULL || SceSysrootForKernel_4F0A4066(pid, &info) != 0)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	*paid = info.programAuthorityId;
	return 0;
}

int SceSblACMgrForDriver_E79C7A8D(SceUID pid, const SceChar8 *path)
{
	const AcMgrProgramPaths *program;
	const AcMgrPathRule *rule;
	SceUInt64 paid = 0;
	SceSize offset;

	if (!hasAttribute(pid, 0x81))
		return 1;
	if (sceSblACMgrGetPaid(pid, &paid) != 0 || path == NULL)
		return 0;
	for (program = programPaths; program != programPaths + 9; program++) {
		if (program->programAuthorityId != paid)
			continue;
		for (rule = program->rules; rule->mountPoint != NULL; rule++) {
			offset = rule->mountPointLen;
			if (offset != 0 && !matchEncoded(path, rule->mountPoint, offset))
				continue;
			if (path[offset] == '/')
				offset++;
			if (rule->pathLen == 0 ||
			    matchEncoded(path + offset, rule->path, rule->pathLen))
				return 1;
		}
	}
	return 0;
}

int sceSblACMgrIsKernelProgram(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 0);
}

int sceSblACMgrIsSystemProgram(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 1);
}

int SceSblACMgrForDriver_8A54DF3A(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 2) || sceSblACMgrHasCapability(pid, 3);
}

int SceSblACMgrForDriver_6C5AB07F(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 3);
}

int sceSblACMgrIsGameProgram(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return (paid & 0xfff0000000000000ULL) == 0x2100000000000000ULL ||
		paid == 0x2f00000000000001ULL;
}

int SceSblACMgrForDriver_426A4E8C(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return (paid & 0xfff0000000000000ULL) == 0x2f00000000000000ULL;
}

/* name unknown: the check sceSblACMgrModuleStart registers with sysroot */
int SceSblACMgrForKernel_30575458(SceUID pid)
{
	if (sceSblACMgrHasCapability(pid, 0x22))
		return 1;
	return SceSblACMgrForDriver_426A4E8C(pid) && SceSysrootForDriver_26AA237C() != 0;
}

int SceSblACMgrForDriver_991FDC15(SceUID pid)
{
	SceUInt64 paid = 0;

	return sceSblACMgrGetPaid(pid, &paid) == 0 && paid == 0x2800000000000001ULL;
}

int SceSblACMgrForDriver_0139FC20(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return paid == 0x2800000000008003ULL || paid == 0x2800000000008005ULL;
}

int SceSblACMgrForDriver_0924896F(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return paid == 0x220000101cd2000cULL || paid == 0x220000101cd2000dULL;
}

int SceSblACMgrForDriver_962CD237(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return (paid & ~0xfULL) == 0x2800800000000000ULL;
}

int SceSblACMgrForDriver_2E992B02(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return (paid & 0xffff000000000000ULL) == 0x2801000000000000ULL;
}

int SceSblACMgrForDriver_84604EED(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 0x1f);
}

int SceSblACMgrForDriver_4CBD6156(SceUID pid)
{
	SceUInt64 paid = 0;
	SceUInt64 group;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	group = paid & 0xfff0000000000000ULL;
	if (group == 0x2800000000000000ULL) {
		if (SceQafMgrForDriver_CAD47130())
			return 1;
	} else if (group == 0x2200000000000000ULL || group == 0x2100000000000000ULL) {
		if (SceQafMgrForDriver_694D1096() || SceSysrootForDriver_56D85EB0(paid))
			return 1;
	}
	return hasAttribute(pid, 0x10) == 0;
}

int SceSblACMgrForDriver_E87D1777(void)
{
	return SceDipswForDriver_A98FC2FD(0x9f);
}

int SceSblACMgrForDriver_AD717E7A(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 0x20);
}

int SceSblACMgrForDriver_062CAEB2(SceUID pid)
{
	if (sceSblACMgrHasCapability(pid, 1) || sceSblACMgrHasCapability(pid, 0x83))
		return 1;
	return SceSblACMgrForDriver_426A4E8C(pid) && SceSysrootForDriver_26AA237C() != 0;
}

int SceSblACMgrForDriver_B836CF13(SceUID pid)
{
	return hasAttribute(pid, 0x81);
}

int SceSblACMgrForDriver_C98D82EE(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	if (paid == 0x2800c0101cd2000bULL || paid - 0x210000101cd20007ULL < 4)
		return 1;
	return SceSblACMgrForDriver_426A4E8C(pid) && SceSysrootForDriver_26AA237C() != 0;
}

int SceSblACMgrForDriver_091F7247(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return paid == 0x210000101cd2000aULL || paid == 0x2800c0101cd2000bULL;
}

int SceSblACMgrForDriver_5C4BC352(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return paid == 0x210000101cd2000aULL;
}

int SceSblACMgrForDriver_A67E8E5B(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return paid - 0x210000101cd20007ULL < 3;
}

int SceSblACMgrForDriver_D0E11C89(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	if (paid == 0x220000101cc73883ULL || paid == 0x2800000000000010ULL)
		return 1;
	return sceSblACMgrHasCapability(pid, 0x82) != 0;
}

int SceSblACMgrForDriver_4DB7F512(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	if (paid == 0x220000101cc73883ULL || paid == 0x280000000000001eULL ||
	    paid == 0x2800000000000010ULL || paid == 0x2800000000000031ULL)
		return 1;
	return sceSblACMgrHasCapability(pid, 0x82) != 0;
}

int SceSblACMgrForDriver_D7AD8471(SceUID pid, const SceChar8 *path)
{
	SceUInt64 paid;
	SceSize nameLen, i;
	int level;
	int n;

	level = SceSysrootForKernel_20009397();
	paid = 0;
	if (sceSblACMgrGetPaid(pid, &paid) != 0 || path == NULL)
		return 0;
	for (n = 0; n < 21; n++) {
		nameLen = mountPoints[n].nameLen;
		for (i = 0; i < nameLen; i++)
			if (path[i] != ~mountPoints[n].name[i])
				break;
		if (i != nameLen)
			continue;
		if (sceSblACMgrHasCapability(pid, 1) || level == 0x40 || level == 0x80)
			return 1;
		if (sceSblACMgrHasCapability(pid, 3) &&
		    memcmp(ENC("\x8a\x87\xcf\xc5"), mountPoints[n].name, nameLen) == 0)
			return 1;
		if ((paid == 0x220000101cc73883ULL || paid == 0x220000101cc60019ULL) &&
		    memcmp(ENC("\x93\x92\x9e\xcf\xc5"), mountPoints[n].name, nameLen) == 0)
			return 1;
		return 0;
	}
	return 0;
}

int SceSblACMgrForDriver_0B6E6CD7(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return paid == 0x2800000000000013ULL || paid == 0x2800000000007009ULL ||
		paid == 0x2800000000000010ULL || paid - 0x280000000000002dULL < 2 ||
		paid == 0x2800000000000022ULL || paid == 0x2800000000000039ULL ||
		paid == 0x2800000000000001ULL;
}

int SceSblACMgrForDriver_6D8A88B7(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return sceSblACMgrIsGameProgram(pid) || SceSblACMgrForDriver_0B6E6CD7(pid);
}

int SceSblACMgrForDriver_456DA7AC(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	if (sceSblACMgrHasCapability(pid, 3) || paid - 0x210000101cd20007ULL < 3)
		return 1;
	if (SceSblACMgrForDriver_426A4E8C(pid) && SceSysrootForDriver_26AA237C() != 0)
		return 1;
	return paid == 0x2800000000000044ULL || paid == 0x280000000000001eULL;
}

int SceSblACMgrForDriver_FD00C72A(SceUID pid)
{
	SceUInt64 paid = 0;

	if (sceSblACMgrGetPaid(pid, &paid) != 0)
		return 0;
	return paid == 0x2800000000000013ULL || paid == 0x2800000000007009ULL;
}

int SceSblACMgrForDriver_2A29453C(SceUID pid)
{
	if (sceSblACMgrHasCapability(pid, 0x84))
		return 1;
	return SceSblACMgrForDriver_426A4E8C(pid) && SceSysrootForDriver_26AA237C() != 0;
}

int SceSblACMgrForDriver_A27E47A7(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 0x81);
}

int SceSblACMgrForDriver_F5AE24AC(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 0x80);
}

int SceSblACMgrForDriver_AE1AF154(SceUID pid)
{
	if (sceSblACMgrHasCapability(pid, 0x86))
		return 1;
	return SceSblACMgrForDriver_426A4E8C(pid) && SceSysrootForDriver_E2515A08() != 0;
}

int SceSblACMgrForDriver_A92CD636(SceUID pid)
{
	return sceSblACMgrHasCapability(pid, 0x87);
}
