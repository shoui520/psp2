/*
 * Who may do what on a PFS mount: file attributes through SceSblACMgr, open
 * flags per mode, and the access check every operation begins with.
 */

#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <sysmem/cpu.h>
#include <acmgr/acmgr.h>
#include "pfsmgr_internal.h"

#define PFS_ACMGR_ATTR		6	/* name unknown: SceSblACMgrValue.type of a file attribute */
#define PFS_ACCESS_LEVEL_FULL	0x80	/* name unknown */
#define PFS_PERMISSION		0x40	/* name unknown */
#define SCE_PFS_ERROR_ACCESS	0x80142014	/* name unknown */
#define SCE_PFS_ERROR_FS_TYPE	0x80140906	/* name unknown */
#define PFS_MODE_COUNT		0x22

static const ScePfsModeInfo g_pfsModeInfoMode5 = { 0, 1, 1, 0x203, 0x203, 6, 6, 0, 0, 0 };
static const ScePfsModeInfo g_pfsModeInfoMode2 = { 1, 0, 1, 1, 1, 0, 0, 1, 1, 1 };
static const ScePfsModeInfo g_pfsModeInfoMode0 = { 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 };
static const ScePfsModeInfo g_pfsModeInfoMode10 = { 1, 0, 1, 0x203, 0x203, 6, 6, 0, 0, 1 };
static const ScePfsModeInfo g_pfsModeInfoMode3 = { 1, 0, 1, 1, 1, 6, 6, 1, 0, 1 };
static const ScePfsModeInfo g_pfsModeInfoMode4 = { 0, 1, 1, 0x203, 0x203, 6, 6, 0, 0, 0 };
static const ScePfsModeInfo g_pfsModeInfoMode21 = { 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 };
static const ScePfsModeInfo g_pfsModeInfoMode12 = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0 };
static const ScePfsModeInfo g_pfsModeInfoMode32 = { 1, 0, 1, 1, 1, 0, 0, 1, 1, 1 };

static const ScePfsModeInfo g_pfsModeInfoMode20;

/* names unknown: each entry is named by the first mode that uses it */
static const ScePfsModeInfo *const g_pfsModeInfo[PFS_MODE_COUNT] = {
	[0]  = &g_pfsModeInfoMode0,
	[2]  = &g_pfsModeInfoMode2,
	[3]  = &g_pfsModeInfoMode3,
	[4]  = &g_pfsModeInfoMode4,
	[5]  = &g_pfsModeInfoMode5,
	[6]  = &g_pfsModeInfoMode5,
	[7]  = &g_pfsModeInfoMode5,
	[8]  = &g_pfsModeInfoMode4,
	[9]  = &g_pfsModeInfoMode5,
	[10] = &g_pfsModeInfoMode10,
	[11] = &g_pfsModeInfoMode10,
	[12] = &g_pfsModeInfoMode12,
	[13] = &g_pfsModeInfoMode2,
	[20] = &g_pfsModeInfoMode20,
	[21] = &g_pfsModeInfoMode21,
	[22] = &g_pfsModeInfoMode21,
	[23] = &g_pfsModeInfoMode21,
	[32] = &g_pfsModeInfoMode32,
	[33] = &g_pfsModeInfoMode32,
};

static const ScePfsModeInfo g_pfsModeInfoMode20 = { 2, 2, 0, 0, 0, 0, 0, 1, 1, 0 };

static const SceUInt16 g_pfsModeFsType[PFS_MODE_COUNT] = {
	0, 0xffff, 1, 1, 3, 2, 2, 2, 3, 2, 1, 4, 3, 4,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0, 0, 0, 0,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	4, 4,
};

int pfsModeToAttr(SceUInt32 mode, SceUInt16 *attr)
{
	SceSblACMgrValue value;

	value.type = PFS_ACMGR_ATTR;
	value.value = (unsigned char *)attr;
	value.size = sizeof(*attr);
	return SceSblACMgrForDriver_6210D745(SceThreadmgrForDriver_FA54D49A(), &value, mode);
}

int pfsAttrToMode(SceUInt16 attr, SceUInt32 *mode)
{
	SceSblACMgrValue value;

	value.type = PFS_ACMGR_ATTR;
	value.value = (unsigned char *)&attr;
	value.size = sizeof(attr);
	return SceSblACMgrForDriver_3B356B98(&value, mode);
}

SceUInt32 pfsAdjustAttr(ScePfsPmi *pmi, SceUInt32 flags)
{
	SceUInt16 mode = pmi->mode;

	if (!(flags & 6) && (mode == 2 || mode == 5 || mode == 6 || mode == 13))
		flags |= 1;
	if (mode == 9) {
		flags = (flags & 0xfffd) | 5;
	} else if (mode == 7) {
		if ((flags & 6) == 4)
			flags &= 0xfffb;
		flags &= 0xfffd;
	}
	return flags;
}

SceUInt32 pfsCreateAttr(ScePfsPmi *pmi, SceUInt32 mode)
{
	SceUInt32 attr = (mode & IO_STA_FDIR) ? 0x8000 : 0;

	if (pmi->mode == 20)
		return attr | 5;
	if (pmi->mode == 22)
		return attr | 4;
	return attr;
}

SceUInt32 pfsLowerMode(SceUInt32 mode)
{
	if (mode & SCE_STM_RU)
		mode = (mode & ~SCE_STM_RU) | IO_STM_RS;
	if (mode & IO_STM_WU)
		mode = (mode & ~IO_STM_WU) | IO_STM_WS;
	return mode;
}

/* operations that only read */
static inline int pfsOpIsRead(int op, SceUInt32 arg)	/* name unknown */
{
	switch (op) {
	case PFS_OP_OPEN:
		return (arg & 3) == 1;
	case PFS_OP_IOCTL:
		return arg - 0x4420 < 3;
	case PFS_OP_DEVCTL:
		return 0;
	case 6:
	case PFS_OP_SYNC:
	case PFS_OP_DECODE_PATH_ELEM:
		return 1;
	case PFS_OP_CLOSE:
	case PFS_OP_LOOKUP:
	case PFS_OP_READ:
	case PFS_OP_PREAD:
	case PFS_OP_LSEEK:
	case PFS_OP_REMOVE:
	case PFS_OP_GETSTAT:
	case PFS_OP_DOPEN:
	case PFS_OP_DREAD:
	case 28:
	case PFS_OP_VNODE_SYNC:
	case PFS_OP_CLEANUP:
		return 1;
	default:
		return 0;
	}
}

int pfsAccessBegin(ScePfsPmi *pmi, int op, SceUInt32 arg, int *oldPermission)
{
	SceSelfAuthInfo authInfo;
	ScePfsPmi *owner;
	int always, isRead, level, isSystem, approved, approvedSystem;
	SceUInt16 flags;

	always = (unsigned int)op < 9 && ((1 << op) & 0x1ef) != 0;
	isRead = pfsOpIsRead(op, arg);
	level = SceSysrootForKernel_20009397();
	isSystem = sceSblACMgrIsSystemProgram(SceThreadmgrForDriver_FA54D49A());
	if ((unsigned int)(pmi->mode - 0x1f) < 3) {
		owner = pmi->parent;
		if (sceKernelGetProcessSelfAuthInfo(SceThreadmgrForDriver_FA54D49A(), &authInfo) < 0)
			approved = 0;
		else
			approved = pfsPmiIsApproved(owner, authInfo.program_authority_id);
		flags = pmi->parent->flags;
	} else {
		if (sceKernelGetProcessSelfAuthInfo(SceThreadmgrForDriver_FA54D49A(), &authInfo) < 0)
			approved = 0;
		else
			approved = pfsPmiIsApproved(pmi, authInfo.program_authority_id);
		flags = pmi->flags;
	}
	if (flags & PFS_PMI_NOT_APPROVED)
		approved = 0;
	if (always)
		goto grant;

	approvedSystem = approved && isSystem == 1;
	switch (pmi->mode) {
	case 2:
	case 9:
	case 13:
	case 20:
	case 33:
		if (isRead)
			goto approvedOnly;
		break;
	case 3:
	case 31:
	systemOnly:
		if (approvedSystem || level == PFS_ACCESS_LEVEL_FULL)
			goto grant;
		break;
	case 4:
	case 5:
	case 6:
	case 8:
	case 22:
		if (approvedSystem || level == PFS_ACCESS_LEVEL_FULL ||
		    (isSystem != 1 && approved && isRead))
			goto grant;
		break;
	case 7:
	case 21:
	case 23:
	case 10:
	approvedOnly:
		if (approved || level == PFS_ACCESS_LEVEL_FULL)
			goto grant;
		break;
	case 12:
		if (isRead)
			goto grant;
		break;
	case 32:
		if (isRead)
			goto systemOnly;
		break;
	}
	return SCE_PFS_ERROR_ACCESS;

grant:
	*oldPermission = sceKernelSetThreadAccessLevel(PFS_PERMISSION);
	SceCpuForDriver_5F6A8743(&g_pfsBusy, 1);
	return 0;
}

void pfsAccessEnd(int oldPermission)
{
	sceKernelSetThreadAccessLevel(oldPermission);
	SceCpuForDriver_A4884C4E(&g_pfsBusy, 1);
}

const ScePfsModeInfo *pfsModeInfo(SceUInt32 mode)
{
	if (mode < PFS_MODE_COUNT)
		return g_pfsModeInfo[mode];
	return NULL;
}

SceUInt16 pfsModeFsType(SceUInt32 mode)
{
	if (mode < PFS_MODE_COUNT)
		return g_pfsModeFsType[mode];
	return 0xffff;
}

int pfsModeCheckFsType(SceUInt32 mode, SceUInt32 fsType)
{
	if (fsType == pfsModeFsType(mode))
		return 0;
	return SCE_PFS_ERROR_FS_TYPE;
}

int pfsFsTypeIsX(int fsType)	/* name unknown */
{
	return fsType == 1 || fsType == 4;
}
