/*
 * FAT file attributes (types 1-5, exfatfs; Sony's exfatfs wraps these as
 * sceFatfsAC*). The low three bits of the attribute byte or halfword select an
 * access class; types 1-3 store one byte, types 4-5 a halfword that need not be
 * aligned. No debug records name this file; its tables sit before
 * acmgr_pfs_attr.c's.
 */
#include "acmgr_internal.h"

/* an open mode and the attribute it sets */
typedef struct FatfsModeAttribute {
	SceUInt32 mode;
	SceUInt32 attribute;
} FatfsModeAttribute;

static const FatfsModeAttribute fatfsKernelModes[10] = {
	{ 0x10186, 0 }, { 0x186, 0 }, { 0x10106, 1 }, { 0x106, 4 }, { 0x10104, 5 },
	{ 0x104, 5 }, { 0x10006, 6 }, { 0x6, 6 }, { 0x10004, 7 }, { 0x4, 7 },
};

/* the open mode of each attribute */
static const SceUInt32 fatfsModes[8] = {
	0x186, 0x106, 0x186, 0x106, 0x106, 0x104, 0x6, 0x4,
};

/* the accesses each attribute grants a user thread / a kernel thread */
static const SceUInt32 fatfsUserPermissions[8] = {
	0xb, 0x9, 0xb, 0x9, 0x1, 0x1, 0x0, 0x0,
};

static const FatfsModeAttribute fatfsUserModes[2] = {
	{ 0x180, 0 }, { 0x100, 1 },
};

static const SceUInt32 fatfsKernelPermissions[8] = {
	0xb, 0xb, 0xb, 0xb, 0xb, 0x9, 0xb, 0x9,
};

/* types 4-5: the halfword need not be aligned, so it moves a byte at a time */
static inline SceUInt16 fatfsLoadHalf(const SceUInt8 *p)
{
	SceUInt16 value;

	((SceUInt8 *)&value)[0] = p[0];
	((SceUInt8 *)&value)[1] = p[1];
	return value;
}

static inline void fatfsStoreHalf(SceUInt8 *p, SceUInt16 value)
{
	p[0] = ((SceUInt8 *)&value)[0];
	p[1] = ((SceUInt8 *)&value)[1];
}

/* name unknown: the attribute's access class */
SceUInt32 fatfsGetAttr(const SceSblACMgrValue *attr)
{
	const SceUInt8 *p = attr->value;

	if (attr->size == 1)
		return p[0] & 7;
	if (attr->size == 2)
		return fatfsLoadHalf(p) & 7;
	return 0;
}

/* name unknown: replace the access class, keeping the other attribute bits */
void fatfsSetAttr(SceSblACMgrValue *attr, SceUInt32 attribute)
{
	SceUInt8 *p = attr->value;

	if (attr->size == 1)
		p[0] = (p[0] & ~7) | (attribute & 7);
	else if (attr->size == 2)
		fatfsStoreHalf(p, (fatfsLoadHalf(p) & ~7) | (attribute & 7));
}

/* name unknown: classes with bit 2 set (old or new) need a kernel thread */
static int fatfsSetAttrByThreadAccessLevel(SceSblACMgrValue *attr, SceUInt32 attribute)
{
	SceUInt32 bits;
	int level;

	bits = fatfsGetAttr(attr) | attribute;
	level = SceSysrootForKernel_20009397();
	if ((bits & 4) && level != 0x80 && level != 0x40)
		return SCE_ERROR_ERRNO_EACCES;
	fatfsSetAttr(attr, attribute);
	return 0;
}

int setFatfsAttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode)
{
	const FatfsModeAttribute *table;
	SceUInt32 count, i, attribute;
	SceUInt8 *p;
	int level;

	if (mode == 0x1000000) {
		p = attr->value;
		if (attr->size == 1) {
			p[0] = 0xf;
		} else if (attr->size == 2) {
			fatfsStoreHalf(p, 0xf);
		}
		return 0;
	}
	level = SceSysrootForKernel_20009397();
	if (level == 0x40 || level == 0x80) {
		table = fatfsKernelModes;
		count = 10;
		mode &= 0x10186;
	} else if (level == 0x10 || level == 0x20) {
		table = fatfsUserModes;
		count = 2;
		mode &= 0x180;
	} else {
		return SCE_ERROR_ERRNO_EINVAL;
	}
	attribute = 0;
	for (i = 0; i < count; i++) {
		if (table[i].mode == mode) {
			attribute = table[i].attribute;
			break;
		}
	}
	if (i == count)
		return SCE_ERROR_ERRNO_EINVAL;
	return fatfsSetAttrByThreadAccessLevel(attr, attribute);
}

int setFatfsAttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
				 const SceSblACMgrValue *source)
{
	if (source == NULL || source->value == NULL || source->type < 1 || source->type > 5)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	return fatfsSetAttrByThreadAccessLevel(attr, fatfsGetAttr(source));
}

int setFatfsAttributeByAttributeWithMountPoint(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
					       const SceSblACMgrValue *source, SceUInt32 mountPoint)
{
	SceUInt16 value;
	SceUInt64 paid;

	if (source == NULL || source->value == NULL || source->type < 1 || source->type > 5 ||
	    mountPoint != 0x10000)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	if (source->size == 1) {
		value = source->value[0];
	} else if (source->size == 2) {
		value = fatfsLoadHalf(source->value);
	} else {
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
	if (value != 0xf)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	paid = info->program_authority_id;
	if (paid == 0x280000000000001eULL || paid == 0x220000101cc73883ULL ||
	    paid == 0x2800000000000010ULL || paid == 0x2800000000000031ULL ||
	    (info->capability[16] & 0x20))
		value = 0;
	else
		value = 4;
	if (attr->size == 1) {
		attr->value[0] = value;
	} else if (attr->size == 2) {
		fatfsStoreHalf(attr->value, value);
	}
	return 0;
}

int checkFatfsAttribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access)
{
	const SceUInt32 *permissions;
	SceUInt32 attribute;

	switch (SceSysrootForKernel_20009397()) {
	case 0x10: case 0x20:
		permissions = fatfsUserPermissions;
		break;
	case 0x40: case 0x80:
		permissions = fatfsKernelPermissions;
		break;
	default:
		return SCE_ERROR_ERRNO_EACCES;
	}
	switch (attr->type) {
	case 1: case 2: case 3:
		if (attr->size != 1)
			return ACMGR_ERROR_INVALID_ARGUMENT;
		attribute = attr->value[0] & 7;
		break;
	case 4: case 5:
		if (attr->size != 2)
			return ACMGR_ERROR_INVALID_ARGUMENT;
		attribute = fatfsLoadHalf(attr->value) & 7;
		break;
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
	if ((permissions[attribute] & access) != access)
		return SCE_ERROR_ERRNO_EACCES;
	return 0;
}

int getFatfsModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode)
{
	if (attr->type < 1 || attr->type > 5)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	*mode = fatfsModes[fatfsGetAttr(attr) & 7];
	return 0;
}

int getFatfsAttributeByThreadAccessLevel(SceUInt32 mode, int level, SceSblACMgrValue *attr)
{
	const FatfsModeAttribute *table;
	SceUInt32 count, i, attribute;

	if (level == 0x40 || level == 0x80) {
		table = fatfsKernelModes;
		count = 10;
	} else if (level == 0x10 || level == 0x20) {
		table = fatfsUserModes;
		count = 2;
	} else {
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
	attribute = 0;
	for (i = 0; i < count; i++) {
		if (table[i].mode == mode) {
			attribute = table[i].attribute;
			break;
		}
	}
	if (i == count)
		return SCE_ERROR_ERRNO_ENOENT;
	fatfsSetAttr(attr, attribute);
	return 0;
}
