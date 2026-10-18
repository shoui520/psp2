/*
 * Attribute type 8: idstorage leaves (idstorage builds a 6-byte attribute and
 * checks access 1 or 2). Only the check is supported; each program family
 * reads its access bits from its own nibble of the attribute.
 */
#include "acmgr_internal.h"

int setIdStorageAttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode)
{
	return ACMGR_ERROR_NOT_SUPPORTED;
}

int setIdStorageAttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
				     const SceSblACMgrValue *source)
{
	return ACMGR_ERROR_NOT_SUPPORTED;
}

int checkIdStorageAttribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access)
{
	SceUInt32 required, permission;
	SceUInt64 paid;
	int level;

	if (info == NULL || attr == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	switch (access) {
	case 1:
		required = 1;
		break;
	case 2:
		required = 2;
		break;
	case 3:
		required = 3;
		break;
	case 4:
		required = 4;
		break;
	default:
		return ACMGR_ERROR_INVALID_ARGUMENT;
	}
	level = SceSysrootForKernel_20009397();
	if (attr->size != 6)
		return ACMGR_ERROR_ACCESS_DENIED;
	paid = info->programAuthorityId;
	if (level == 0x80 || (paid & 0xfff8000000000000ULL) == 0x2808000000000000ULL)
		permission = 0xf;
	else if ((paid & ~0xfULL) == 0x2800800000000000ULL ||
		 paid == 0x2800000000007001ULL || paid == 0x2800000000000015ULL)
		permission = attr->value[0] & 0xf;
	else if ((paid & 0xffff000000000000ULL) == 0x2801000000000000ULL ||
		 paid == 0x2800800000000022ULL || paid == 0x2800800000000018ULL)
		permission = attr->value[1] & 0xf;
	else if (paid == 0x2800000000008005ULL || paid == 0x2800000000008003ULL ||
		 paid == 0x2800000000000025ULL)
		return ACMGR_ERROR_ACCESS_DENIED;
	else if ((paid & 0xffffffffffff0000ULL) == 0x2800000000000000ULL)
		permission = attr->value[0] >> 4;
	else
		return ACMGR_ERROR_ACCESS_DENIED;
	if ((permission & required) != required)
		return ACMGR_ERROR_ACCESS_DENIED;
	return 0;
}

int getIdStorageModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode)
{
	return ACMGR_ERROR_NOT_SUPPORTED;
}
