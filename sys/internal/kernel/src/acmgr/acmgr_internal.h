/* SceSblACMgr: declarations shared by its source files. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <acmgr/acmgr.h>
#include <kernel/error.h>

/* SceSblACMgr's own error codes (no Sony names found) */
#define ACMGR_ERROR_NOT_FOUND		((int)0x800f0902)
#define ACMGR_ERROR_AUTH_INFO		((int)0x800f0903)
#define ACMGR_ERROR_ACCESS_DENIED	((int)0x800f090d)
#define ACMGR_ERROR_INVALID_PATH	((int)0x800f090e)
#define ACMGR_ERROR_INVALID_ARGUMENT	((int)0x800f0916)
#define ACMGR_ERROR_NOT_SUPPORTED	((int)0x800f0925)
#define ACMGR_ERROR_INVALID_LEVEL	((int)0x800f0928)

/* acmgr.c */

/*
 * The filesystem attribute operations, one set per attribute type: set from an
 * open mode, set from another attribute, check an access, get the open mode.
 */

/* acmgr_fatfs_attr.c: types 1-5 (exfatfs) */
int setFatfsAttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode);
int setFatfsAttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
				 const SceSblACMgrValue *source);
int setFatfsAttributeByAttributeWithMountPoint(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
					       const SceSblACMgrValue *source, SceUInt32 mountPoint);
int checkFatfsAttribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access);
int getFatfsModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode);
int getFatfsAttributeByThreadAccessLevel(SceUInt32 mode, int level, SceSblACMgrValue *attr);

/* acmgr_pfs_attr.c: type 6 (pfsmgr) */
int setPfsAttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode);
int setPfsAttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
			       const SceSblACMgrValue *source);
int checkPfsAttribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access);
int getPfsModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode);

/* acmgr_type7_attr.c: type 7, not supported */
int setType7AttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode);
int setType7AttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
				 const SceSblACMgrValue *source);
int checkType7Attribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access);
int getType7ModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode);

/* acmgr_idstorage_attr.c: type 8 (idstorage leaves) */
int setIdStorageAttributeByMode(const SceSelfAuthInfo *info, SceSblACMgrValue *attr, SceUInt32 mode);
int setIdStorageAttributeByAttribute(const SceSelfAuthInfo *info, SceSblACMgrValue *attr,
				     const SceSblACMgrValue *source);
int checkIdStorageAttribute(const SceSelfAuthInfo *info, const SceSblACMgrValue *attr, SceUInt32 access);
int getIdStorageModeByAttribute(const SceSblACMgrValue *attr, SceUInt32 *mode);
