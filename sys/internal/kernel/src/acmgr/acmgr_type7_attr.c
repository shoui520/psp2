/* Attribute type 7: no operation is supported. */
#include "acmgr_internal.h"

int setType7AttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode)
{
	return ACMGR_ERROR_NOT_SUPPORTED;
}

int setType7AttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
				 const SceSblACMgrValue *source)
{
	return ACMGR_ERROR_NOT_SUPPORTED;
}

int checkType7Attribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access)
{
	return ACMGR_ERROR_NOT_SUPPORTED;
}

int getType7ModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode)
{
	return ACMGR_ERROR_NOT_SUPPORTED;
}
