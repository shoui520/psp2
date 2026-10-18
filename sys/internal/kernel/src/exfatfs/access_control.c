/* Sony: access_control.c */
#include <threadmgr/threadmgr.h>
#include <acmgr/acmgr.h>
#include <sysmem/debug.h>

#include "exfatfs_internal.h"

int sceFatfsACSetFSAttributeByPermission(SceExfatfsDrive *drive,
				      SceUInt32 permission,
				      unsigned char *attribute)
{
	SceUID pid = SceThreadmgrForDriver_FA54D49A();
	SceSblACMgrValue value;

	value.value = attribute;
	if (drive->state & 0x1000)
		goto fatAttribute;
	if (drive->state & 0x8000) {
		value.type = 2;
		value.size = 1;
		return SceSblACMgrForDriver_6210D745(pid, &value, permission);
	}
fatAttribute:
	value.type = 3;
	value.size = 1;
	return SceSblACMgrForDriver_6210D745(pid, &value, permission);
}

int sceFatfsACSetFSAttributeByAttribute(SceExfatfsDrive *drive,
				     unsigned char attribute,
				     unsigned char *output)
{
	SceUID pid = SceThreadmgrForDriver_FA54D49A();
	SceSblACMgrValue inputValue;
	SceSblACMgrValue outputValue;

	inputValue.value = &attribute;
	if (drive->state & 0x1000) {
		inputValue.type = 3;
		inputValue.size = 1;
	} else if (drive->state & 0x8000) {
		inputValue.type = 2;
		inputValue.size = 1;
	} else {
		inputValue.type = 3;
		inputValue.size = 1;
	}
	outputValue.value = output;
	if (drive->state & 0x1000) {
		outputValue.type = 3;
		outputValue.size = 1;
	} else if (drive->state & 0x8000) {
		outputValue.type = 2;
		outputValue.size = 1;
	} else {
		outputValue.type = 3;
		outputValue.size = 1;
	}
	return SceSblACMgrForDriver_B12CEAA8(pid, &outputValue, &inputValue);
}

int sceFatfsACGetFSAttributeByThreadAccessLevel(SceExfatfsDrive *drive,
					     int accessLevel, int permission,
					     unsigned char *attribute)
{
	SceSblACMgrValue value;
	int result;

	value.value = attribute;
	if (drive->state & 0x1000) {
		value.type = 3;
		value.size = 1;
	} else if (drive->state & 0x8000) {
		value.type = 2;
		value.size = 1;
	} else {
		value.type = 3;
		value.size = 1;
	}
	result = sceSblACMgrGetFSAttributeByThreadAccessLevel(accessLevel, permission, &value);
	if (result < 0)
		SCE_DBG_PRINTF(0, 158,
			      "%s: sceSblACMgrGetFSAttributeByThreadAccessLevel failed[%#x]\n",
			      __func__, result);
	return result;
}

static inline int set_attr_info_for_check(SceExfatfsDrive *drive,
					unsigned char *attribute,
					SceSblACMgrValue *value)
{
	if (*attribute == 15) {
		unsigned char specialAcl;
		SceSblACMgrValue inputValue;
		SceUID pid;
		int result;

		if (!drive || !drive->deviceInfo.mount_misc)
			return (int)0x8001000e;
		specialAcl = *attribute;
		pid = SceThreadmgrForDriver_FA54D49A();
		inputValue.value = &specialAcl;
		if (drive->state & 0x1000) {
			inputValue.type = 3;
			inputValue.size = 1;
		} else if (drive->state & 0x8000) {
			inputValue.type = 2;
			inputValue.size = 1;
		} else {
			inputValue.type = 3;
			inputValue.size = 1;
		}
		value->value = attribute;
		if (drive->state & 0x1000) {
			value->type = 3;
			value->size = 1;
		} else if (drive->state & 0x8000) {
			value->type = 2;
			value->size = 1;
		} else {
			value->type = 3;
			value->size = 1;
		}
		result = sceSblACMgrSetFSAttributeByAttributeWithMountPoint(pid, value, &inputValue,
						    drive->deviceInfo.mount_misc->mntId);
		if (result < 0)
			return result;
		SCE_DBG_PRINTF_LEVEL(2, 0, 133, "%s: special ACL[%#x]->attr[%#x]\n",
				    __func__, specialAcl, *attribute);
		return 0;
	}
	value->value = attribute;
	if (drive->state & 0x1000) {
		value->type = 3;
		value->size = 1;
	} else if (drive->state & 0x8000) {
		value->type = 2;
		value->size = 1;
	} else {
		value->type = 3;
		value->size = 1;
	}
	return 0;
}

int sceFatfsACCheckRDirEntry(SceExfatfsDrive *drive, unsigned char attribute)
{
	unsigned char attrInfo[8];
	SceSblACMgrValue value;
	SceUID pid;
	int result;

	attrInfo[0] = attribute;
	pid = SceThreadmgrForDriver_FA54D49A();
	result = set_attr_info_for_check(drive, attrInfo, &value);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 197, "%s: set_attr_info_for_check failed[%#x]\n",
			      __func__, result);
		return result;
	}
	return SceSblACMgrForDriver_BE5667C5(pid, &value, 1);
}

int sceFatfsACGetDirPermission(SceExfatfsDrive *drive, unsigned char attribute)
{
	unsigned char attrInfo[8];
	SceSblACMgrValue value;
	SceUInt32 permission = 0;
	int result;

	attrInfo[0] = attribute;
	result = set_attr_info_for_check(drive, attrInfo, &value);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 332, "%s: set_attr_info_for_check failed[%#x]\n",
			      __func__, result);
		return result;
	}
	result = SceSblACMgrForDriver_3B356B98(&value, &permission);
	return result < 0 ? result : permission;
}

int sceFatfsACGetRootDirAttribute(SceExfatfsDrive *drive, unsigned char *attribute)
{
	SceInt32 permission;
	int result;

	{
		SceInt32 queriedPermission = 0;
		SceUID pid = SceThreadmgrForDriver_FA54D49A();

		result = SceSblACMgrForDriver_FF7125DE(pid, drive->assignName,
						    &queriedPermission);
		permission = queriedPermission;
	}
	if (result >= 0) {
		if (permission >= 0) {
			/* Sony inlines sceFatfsACSetFSAttributeByPermission here. */
			{
				SceUID pid = SceThreadmgrForDriver_FA54D49A();
				SceSblACMgrValue value;

				value.value = attribute;
				if (drive->state & 0x1000) {
					value.type = 3;
					value.size = 1;
				} else if (drive->state & 0x8000) {
					value.type = 2;
					value.size = 1;
				} else {
					value.type = 3;
					value.size = 1;
				}
				result = SceSblACMgrForDriver_6210D745(pid, &value,
								    permission);
			}
			if (result == 0) {
				if (*attribute == 15) {
					SCE_DBG_PRINTF_LEVEL(2, 0, 405,
							    "%s: special attr[%#x]\n",
							    __func__, *attribute);
				} else {
					*attribute |= 0x10;
				}
			}
		} else {
			result = permission;
		}
	}
	return result;
}
