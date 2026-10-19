/*
 * PFS file attributes (type 6, pfsmgr; pfsmgr wraps them as scePfsAC*). The low
 * three bits of the attribute select an access class as for the FAT attributes;
 * bits 13-15 carry open mode flags. Sony's file (debug records: pfsSetAttr,
 * pfsGetAttr, checkPfsAttribute).
 *
 * The first word of the auth info's capability block holds the program's
 * access level (0x10, 0x20, 0x40 or 0x80, as the thread access levels).
 */
#include <sysmem/debug.h>
#include "acmgr_internal.h"

/* an attribute and the accesses it grants a kernel / a user program */
typedef struct PfsPermission {
	SceUInt16 attribute;
	SceUInt32 permission[2];
} PfsPermission;

/* an open mode and the attribute it sets */
typedef struct PfsModeAttribute {
	SceUInt32 mode;
	SceUInt16 attribute;
} PfsModeAttribute;

static const PfsPermission pfsPermissions[6] = {
	{ 0, { 0xb, 0xb } }, { 1, { 0xb, 0x9 } }, { 4, { 0xb, 0x1 } },
	{ 5, { 0x9, 0x1 } }, { 6, { 0xb, 0x0 } }, { 7, { 0x9, 0x0 } },
};

static const PfsModeAttribute pfsUserModes[2] = {
	{ 0x180, 0 }, { 0x100, 1 },
};

static const PfsModeAttribute pfsKernelModes[6] = {
	{ 0x186, 0 }, { 0x10106, 1 }, { 0x106, 4 }, { 0x104, 5 }, { 0x6, 6 }, { 0x4, 7 },
};

/* name unknown: the program's access level (see above) */
static inline const SceInt32 *pfsGetLevel(const SceSelfAuthInfo *info)
{
	return (const SceInt32 *)&info->capability.bytes[0];
}

static int pfsSetAttr(const SceSelfAuthInfo *info, unsigned char **ppValue, SceSize *pSize,
		      SceUInt16 attribute)
{
	const SceInt32 *level = pfsGetLevel(info);

	if ((level == NULL || (*level != 0x80 && *level != 0x40)) && (attribute & 4))
		return SCE_ERROR_ERRNO_EACCES;
	if (*pSize == 1)
		**ppValue = attribute;
	else if (*pSize == 2)
		*(SceUInt16 *)*ppValue = attribute;
	else
		SCE_DBG_ASSERT(0, 205);
	return 0;
}

static inline SceUInt16 pfsGetAttr(unsigned char *const *ppValue, const SceSize *pSize)
{
	const unsigned char *value = *ppValue;
	SceSize size = *pSize;

	if (size == 1)
		return value[0];
	if (size == 2)
		return *(const SceUInt16 *)value;
	SCE_DBG_ASSERT(0, 225);
	return 0;
}

int setPfsAttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode)
{
	const SceInt32 *level = pfsGetLevel(info);
	const PfsModeAttribute *table;
	SceUInt32 count, i, plain, masked;
	SceUInt16 attribute;

	/* the open mode without the pfs flags; kernel programs may pass 0x100000, 0x200000 */
	plain = mode & ~0xf00000;
	if (level == NULL)
		return ACMGR_ERROR_INVALID_LEVEL;
	switch (*level) {
	case 0x40: case 0x80:
		if ((plain | (mode & 0x300000)) != mode)
			return SCE_ERROR_ERRNO_EINVAL;
		break;
	case 0x10: case 0x20:
		if (plain != mode)
			return SCE_ERROR_ERRNO_EINVAL;
		break;
	default:
		return ACMGR_ERROR_INVALID_LEVEL;
	}
	switch (*level) {
	case 0x40: case 0x80:
		table = pfsKernelModes;
		count = 6;
		masked = mode & 0x10186;
		break;
	case 0x10: case 0x20:
		table = pfsUserModes;
		count = 2;
		masked = mode & 0x180;
		break;
	default:
		return ACMGR_ERROR_INVALID_LEVEL;
	}
	attribute = 0;
	for (i = 0; i < count; i++) {
		if (table[i].mode == masked) {
			attribute = table[i].attribute;
			break;
		}
	}
	if (i == count)
		return ACMGR_ERROR_NOT_FOUND;
	if (mode & 0x100000)
		attribute |= 0x4000;
	if (mode & 0x200000)
		attribute |= 0x2000;
	return pfsSetAttr(info, &attr->value, &attr->size, attribute);
}

int setPfsAttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
			       const SceSblACMgrValue *source)
{
	return pfsSetAttr(info, &attr->value, &attr->size, pfsGetAttr(&source->value, &source->size));
}

int checkPfsAttribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access)
{
	SceUInt32 attribute, permission;
	int privileged, i;

	switch (sceKernelSysrootGetThreadAccessLevel()) {
	case 0x40: case 0x80:
		privileged = 1;
		break;
	case 0x10: case 0x20:
		privileged = 0;
		break;
	default:
		SCE_DBG_PRINTF(0, 296, "%s(%d) %s: r=0x%x.\n", "acmgr_pfs_attr.c", 296,
			       "checkPfsAttribute", ACMGR_ERROR_INVALID_LEVEL);
		return ACMGR_ERROR_INVALID_LEVEL;
	}
	attribute = pfsGetAttr(&attr->value, &attr->size) & 7;
	for (i = 0; i < 6; i++) {
		if (pfsPermissions[i].attribute == attribute) {
			SceUInt32 kernel = pfsPermissions[i].permission[0];
			SceUInt32 user = pfsPermissions[i].permission[1];

			permission = privileged ? kernel : user;
			if ((permission & access) != access)
				return SCE_ERROR_ERRNO_EACCES;
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_EACCES;
}

int getPfsModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode)
{
	SceUInt16 attribute;
	int i;

	attribute = pfsGetAttr(&attr->value, &attr->size);
	for (i = 0; i < 6; i++) {
		if (pfsKernelModes[i].attribute == (attribute & 7)) {
			*mode = pfsKernelModes[i].mode & ~0x10000;
			if (attribute & 0x4000)
				*mode |= 0x100000;
			if (attribute & 0x2000)
				*mode |= 0x200000;
			if (attribute & 0x8000)
				*mode |= 0x1000;
			else
				*mode |= 0x2000;
			return 0;
		}
	}
	return ACMGR_ERROR_INVALID_ARGUMENT;
}
