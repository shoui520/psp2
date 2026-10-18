/*
 * modulecb.c - the module control block
 */

#include "modulemgr_internal.h"

#define SEG_INDEX(x)	((x) >> 30)
#define SEG_OFFSET(x)	((x) & 0x3fffffff)

void (*g_statusCallback)(SceModuleCB *cb);

int ModuleObjNew(SceUID pid, SceUInt32 flags, SceModuleObject **out)
{
	UIDCreateOpt opt, *optp;
	SceModuleObject *obj;
	SceUID uid;
	int ret;

	if (pid == KERNEL_PID) {
		optp = NULL;
	} else {
		opt.flags = UID_CREATE_OPT_PROCESS_HEAP;
		opt.object = NULL;
		opt.heapGuid = 0;
		opt.pid = pid;
		opt.heapKind = (flags & LOAD_FLAG_TYPE1) != 0;
		opt.type = 0;
		opt.attr = 0;
		opt.ownerPid = 0;
		opt.cls = NULL;
		ret = SceProcessmgrForKernel_8B4086AF(pid, opt.heapKind);
		optp = &opt;
		if (ret < 0)
			return ret;
	}

	uid = sceGUIDKernelCreateWithOpt(g_moduleClass, "SceModuleMgrNewModule", optp, &obj);
	if (uid < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 78, "UIDcreate failed 0x%08x\n", uid);
		if (pid != KERNEL_PID)
			SceProcessmgrForKernel_F160B4D8(pid, opt.heapKind);
		return uid;
	}

	obj->cb.pid = pid;
	obj->cb.sdkVersion = pid == KERNEL_PID ? 0xffffffff : MODULE_OBJECT_VERSION;
	obj->cb.modid = uid;
	if (out != NULL)
		*out = obj;
	return 0;
}

/* top..btm lies in segment seg of the module */
static inline int ModuleInfoRangeOk(SceModuleCB *cb, SceUInt32 top, SceUInt32 btm, SceUInt32 seg)
{
	SceSize memsz = cb->segment[seg].memsz;

	top = SEG_OFFSET(top);
	btm = SEG_OFFSET(btm);
	return top <= memsz && top <= btm && btm <= memsz;
}

static inline int ModuleInfoSegReadOnly(SceModuleCB *cb, SceUInt32 seg)
{
	return (cb->segment[seg].perms & SEGMENT_PERM_WRITE) == 0;
}

int ModuleCBCopyInfo(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceModuleCB *orig;
	SceModuleInfo info;
	SceUInt32 s, off, base, stubSize, tlsEnd;
	int ret;

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0) {
		orig = cb->shared->cb;
		cb->libentTop = orig->libentTop;
		cb->libentBtm = orig->libentBtm;
		cb->libstubTop = orig->libstubTop;
		cb->libstubBtm = orig->libstubBtm;
		cb->fingerprint = orig->fingerprint;
		cb->tlsStart = orig->tlsStart;
		cb->tlsFilesz = orig->tlsFilesz;
		cb->tlsMemsz = orig->tlsMemsz;
		cb->exidxTop = orig->exidxTop;
		cb->exidxBtm = orig->exidxBtm;
		cb->extabStart = orig->extabStart;
		cb->extabEnd = orig->extabEnd;
		cb->attr = orig->attr;
		cb->version[0] = orig->version[0];
		cb->version[1] = orig->version[1];
		cb->name = orig->name;
		cb->moduleStart = orig->moduleStart;
		cb->moduleStop = orig->moduleStop;
		cb->moduleExit = orig->moduleExit;
		cb->moduleBootstart = orig->moduleBootstart;
		cb->procParam = orig->procParam;
		cb->startThreadParam = orig->startThreadParam;
		cb->stopThreadParam = orig->stopThreadParam;
		cb->sdkVersion = orig->sdkVersion;
		return 0;
	}

	s = SEG_INDEX(ctx->entry);
	off = SEG_OFFSET(ctx->entry);
	if ((int)s >= cb->nSegments || !ModuleInfoSegReadOnly(cb, s) ||
	    off + sizeof(info) > cb->segment[s].memsz)
		return 0x80025001;

	base = (SceUInt32)cb->segment[s].vaddr;
	memcpy(&info, (SceUInt8 *)ctx->seg[s].kaddr + off, sizeof(info));
	if (info.infoVersion != 6) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 168, "module infoversion is invalid. 0x%08x\n",
				     (int)(signed char)info.infoVersion);
		SCE_DBG_ASSERT_LEVEL(0, 0, 169);
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_TYPE;
	}

	if (SEG_INDEX(info.libentTop) != s || SEG_INDEX(info.libstubTop) != s)
		return 0x80025001;
	if (SEG_INDEX(info.libentBtm) != s || !ModuleInfoSegReadOnly(cb, s) ||
	    !ModuleInfoRangeOk(cb, info.libentTop, info.libentBtm, s))
		return 0x80025001;
	if (SEG_INDEX(info.libstubBtm) != s ||
	    !ModuleInfoRangeOk(cb, info.libstubTop, info.libstubBtm, SEG_INDEX(info.libstubBtm)))
		return 0x80025001;
	if (SEG_INDEX(info.exidxBtm) != SEG_INDEX(info.exidxTop) ||
	    !ModuleInfoSegReadOnly(cb, SEG_INDEX(info.exidxBtm)) ||
	    !ModuleInfoRangeOk(cb, info.exidxTop, info.exidxBtm, SEG_INDEX(info.exidxBtm)))
		return 0x80025001;
	if (SEG_INDEX(info.extabEnd) != SEG_INDEX(info.extabStart) ||
	    !ModuleInfoSegReadOnly(cb, SEG_INDEX(info.extabEnd)) ||
	    !ModuleInfoRangeOk(cb, info.extabStart, info.extabEnd, SEG_INDEX(info.extabEnd)))
		return 0x80025001;
	tlsEnd = info.tlsFilesz + info.tlsStart;
	if (SEG_INDEX(tlsEnd) != SEG_INDEX(info.tlsStart) ||
	    !ModuleInfoSegReadOnly(cb, SEG_INDEX(tlsEnd)) ||
	    !ModuleInfoRangeOk(cb, info.tlsStart, tlsEnd, SEG_INDEX(tlsEnd)))
		return 0x80025001;
	if (info.libentTop == info.libentBtm || ((info.libentBtm - info.libentTop) & 0x1f) != 0)
		return 0x80025001;
	stubSize = info.libstubBtm - info.libstubTop;
	if ((stubSize % 0x34 != 0 && stubSize % 0x24 != 0) || info.tlsFilesz > info.tlsMemsz)
		return 0x80025001;

	cb->attr = info.attr;
	cb->version[0] = info.version[0];
	cb->version[1] = info.version[1];
	info.name[26] = '\0';
	ret = SceSysmemForDriver_4CFA4100(cb->modid, info.name);
	if (ret < 0)
		return ret;
	cb->name = SceSysmemForDriver_E655852F(cb->modid);
	cb->fingerprint = info.fingerprint;
	cb->libstubBtm = (void *)(SEG_OFFSET(info.libstubBtm) + base);
	cb->tlsMemsz = info.tlsMemsz;
	cb->libentBtm = (SceModuleExports *)(SEG_OFFSET(info.libentBtm) + base);
	cb->libstubTop = (void *)(SEG_OFFSET(info.libstubTop) + base);
	cb->libentTop = (SceModuleExports *)(SEG_OFFSET(info.libentTop) + base);
	cb->tlsStart = (void *)(SEG_OFFSET(info.tlsStart) + base);
	cb->exidxBtm = (void *)(SEG_OFFSET(info.exidxBtm) + base);
	cb->extabStart = (void *)(SEG_OFFSET(info.extabStart) + base);
	cb->extabEnd = (void *)(SEG_OFFSET(info.extabEnd) + base);
	cb->exidxTop = (void *)(base + SEG_OFFSET(info.exidxTop));
	cb->tlsFilesz = info.tlsFilesz;
	return 0;
}

int SceModulemgrForKernel_74A4E0EE(void (*callback)(SceModuleCB *cb))
{
	if (g_statusCallback != NULL && g_statusCallback != callback)
		return SCE_KERNEL_ERROR_MODULEMGR_BUSY;
	g_statusCallback = callback;
	return 0;
}

int SceModulemgrForKernel_ACBC97C9(void (*callback)(SceModuleCB *cb))
{
	if (g_statusCallback != callback)
		return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
	g_statusCallback = NULL;
	return 0;
}

/* name unknown */
void ModuleCBSetStatus(SceModuleCB *cb, SceUInt8 status)
{
	cb->status = status;
	if (g_statusCallback != NULL)
		g_statusCallback(cb);
}

/* name unknown */
int ModuleCBSetPath(SceModuleCB *cb, const char *path, SceUInt32 flags)
{
	SceSize len;

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0) {
		cb->path = cb->shared->cb->path;
		return 0;
	}

	len = strnlen(path, 0xff);
	if ((cb->flags & MODULE_FLAG_SHARED_ORIGINAL) != 0 && len > 0xfe)
		return SCE_KERNEL_ERROR_MODULEMGR_NAMETOOLONG;

	cb->path = ModulemgrAlloc(cb->pid, len + 1);
	if (cb->path == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
	memcpy(cb->path, path, len);
	cb->path[len] = '\0';
	if ((flags & LOAD_FLAG_NO_AUTH) != 0)
		__builtin_memcpy(cb->path, "bootfs:", 7);
	return 0;
}

void ModuleCBDumpLoadAddress(SceModuleCB *cb)
{
	if (!SceQafMgrForDriver_382C71E8())
		return;
	if (cb->nSegments < 2) {
		SCE_PRINTF_WITH_INFO(0, 330, "[%-27s]:text=%p(0x%08x), (no data)\n",
				     cb->name, cb->segment[0].vaddr, cb->segment[0].memsz);
		return;
	}
	SCE_PRINTF_WITH_INFO(0, 325, "[%-27s]:text=%p(0x%08x), data=%p(0x%08x/0x%08x)\n",
			     cb->name, cb->segment[0].vaddr, cb->segment[0].memsz,
			     cb->segment[1].vaddr, cb->segment[1].filesz, cb->segment[1].memsz);
}

void ModuleCBShowStartMessage(SceModuleCB *cb, int noResident, SceUInt32 usec)
{
	if (!SceSblACMgrForKernel_BBA13D9C() && cb->pid != KERNEL_PID)
		return;
	if (noResident < 1)
		SCE_PRINTF_WITH_INFO(0, 354, "[%-27s]:Starting...  OK   %8uusec\n", cb->name, usec);
	else
		SCE_PRINTF_WITH_INFO(0, 354, "[%-27s]:Starting...  OK(NO_RESIDENT)   %8uusec\n",
				     cb->name, usec);
}

/* name unknown: addr..addr+size lies in one segment */
int ModuleCBIsInRange(SceModuleCB *cb, SceUInt32 addr, SceSize size)
{
	int i;

	if (addr + size < addr)
		return 0;
	i = 0;
	do {
		SceModuleSegment *seg = &cb->segment[i];

		i++;
		if ((SceUInt32)seg->vaddr <= addr &&
		    addr + size <= (SceUInt32)seg->vaddr + seg->memsz)
			return 1;
	} while (i < cb->nSegments);
	return 0;
}

/* name unknown */
int ModuleCBGetSegmentIndex(SceModuleCB *cb, SceUInt32 addr)
{
	int i;

	for (i = 0; i < cb->nSegments; i++) {
		SceModuleSegment *seg = &cb->segment[i];

		if ((SceUInt32)seg->vaddr <= addr && addr < (SceUInt32)seg->vaddr + seg->memsz)
			return i;
	}
	return -1;
}

/* name unknown */
int ModuleCBIsInText(SceModuleCB *cb, SceUInt32 addr, SceSize size)
{
	int i;

	if (addr + size < addr)
		return 0;
	for (i = 0; i < cb->nSegments; i++) {
		SceModuleSegment *seg = &cb->segment[i];

		if ((SceUInt32)seg->vaddr <= addr && addr < (SceUInt32)seg->vaddr + seg->memsz) {
			if ((seg->perms & SEGMENT_PERM_MASK) != SEGMENT_PERM_TEXT)
				return 0;
			return addr + size <= seg->memsz + (SceUInt32)seg->vaddr;
		}
	}
	return 0;
}

/* name unknown */
int ModuleCBIsInReadOnly(SceModuleCB *cb, SceUInt32 addr, SceSize size)
{
	int i;

	if (addr + size < addr)
		return 0;
	for (i = 0; i < cb->nSegments; i++) {
		SceModuleSegment *seg = &cb->segment[i];

		if ((SceUInt32)seg->vaddr <= addr && addr < (SceUInt32)seg->vaddr + seg->memsz) {
			if ((seg->perms & SEGMENT_PERM_WRITE) != 0)
				return 0;
			return addr + size <= seg->memsz + (SceUInt32)seg->vaddr;
		}
	}
	return 0;
}

/* name unknown */
int ModuleCBGetSegmentOffset(SceModuleCB *cb, SceUInt32 addr, int *seg, SceUInt32 *offset)
{
	int i;

	for (i = 0; i < cb->nSegments; i++) {
		if ((SceUInt32)cb->segment[i].vaddr <= addr &&
		    addr < (SceUInt32)cb->segment[i].vaddr + cb->segment[i].memsz) {
			*offset = addr - (SceUInt32)cb->segment[i].vaddr;
			*seg = i;
			return 0;
		}
	}
	return -1;
}
