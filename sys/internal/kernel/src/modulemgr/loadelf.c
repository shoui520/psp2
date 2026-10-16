/*
 * loadelf.c - placing the segments of a module in memory
 */

#include "modulemgr_internal.h"

/* name unknown: random extra pages in front of / behind a segment (ASLR) */
SceUInt32 LoadelfSetRandomOffset(SceKernelAllocMemBlockKernelOpt *opt, SceUInt32 mask)
{
	SceUInt32 rnd;
	SceUInt32 low, high;

	ksceMt19937GlobalUninit(&rnd, sizeof(rnd));
	high = (mask & (rnd >> 16)) * 0x1000;
	low = (mask & rnd) * 0x1000;
	opt->extraLow = high;
	opt->extraHigh = low;
	if (low != 0)
		opt->attr |= 0x10;
	if (high != 0)
		opt->attr |= 0x8;
	return high + low;
}

void LoadelfFreeMem(SceModuleCB *cb, void **vaddr, SceUID *memblk, LoadelfSegment *segx)
{
	if (*memblk <= 0)
		return;

	SCE_DBG_PRINTF_LEVEL(2, 0, 701,
		"LoadelfFreeMem(%p (uid=%x modname=%s file=%p,%s), seg=%p, segx=%p)\n",
		cb, cb->modid, cb->name, cb->path, cb->path != NULL ? cb->path : "(null)",
		vaddr, segx);

	if (cb->pid == KERNEL_PID) {
		ksceKernelFreeMemBlock(*memblk);
		*vaddr = NULL;
		*memblk = 0;
		if (segx == NULL)
			return;
	} else {
		if (segx == NULL) {
			ksceKernelFreeMemBlock(*memblk);
			*vaddr = NULL;
			*memblk = 0;
			return;
		}
		if (segx->tmpBlock > 0) {
			ksceKernelFreeMemBlock(segx->tmpBlock);
			segx->tmpBlock = 0;
			segx->kaddr = NULL;
		}
		ksceKernelFreeMemBlock(*memblk);
		*vaddr = NULL;
		*memblk = 0;
	}
	segx->kaddr = NULL;
}

/* name unknown */
void LoadelfFreeSegments(SceModuleCB *cb, LoadelfContext *ctx)
{
	int i;

	for (i = 0; i < 3; i++)
		LoadelfFreeMem(cb, &cb->segment[i].vaddr, &cb->segment[i].memblk,
			       ctx != NULL ? &ctx->seg[i] : NULL);

	if (cb->metadata != NULL && (cb->flags & MODULE_FLAG_SHARED_MASK) == 0)
		ModulemgrFree(cb->pid, cb->metadata);
	cb->metadata = NULL;
	cb->metadataSize = 0;
}

/* copy a segment of the original of a shared module */
static int LoadelfCopySharedSegment(LoadelfContext *ctx, int index)
{
	SceModuleCB *cb = ctx->cb;
	SceModuleSegment *seg = &cb->segment[index];
	SceModuleSegment *orig;
	SceKernelAllocMemBlockKernelOpt opt;
	void *base = NULL;
	SceUID tmp;

	if ((seg->perms & SEGMENT_PERM_WRITE) != 0) {
		if (seg->filesz != 0)
			memcpy(ctx->seg[index].kaddr, cb->shared->dataCopy, seg->filesz);
		return 0;
	}

	orig = &cb->shared->cb->segment[index];
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x1000040;
	opt.mirror_blockid = orig->memblk;
	tmp = ksceKernelAllocMemBlock("SceModuleMgrKernelTmpForSharedText", 0x6020d006, 0, &opt);
	if (tmp < 0)
		return tmp;
	ksceKernelGetMemBlockBase(tmp, &base);
	memcpy(ctx->seg[index].kaddr, (char *)base + ((SceUInt32)orig->vaddr & 0xfff), seg->filesz);
	return ksceKernelFreeMemBlock(tmp);
}

int LoadelfLoadSegment(LoadelfContext *ctx, SelfReader *reader, int index, LoadelfTiming *timing)
{
	SceModuleCB *cb = ctx->cb;
	SceModuleSegment *seg = &cb->segment[index];
	SceUInt32 vaddr = (SceUInt32)seg->vaddr;
	SceUInt32 pageOff = vaddr & 0xfff;
	SceUInt32 end = PAGE_ROUND(vaddr + seg->memsz);
	SceUInt32 size, total, memtype;
	SceUID ret;
	int fill;

	if (seg->memsz != 0 && end <= vaddr - pageOff)
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
	size = end - (vaddr - pageOff);
	total = size + seg->extraPages * 0x1000;
	if (total < size)
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;

	{
		SceUInt8 perms = seg->perms;
		SceUInt16 flags = cb->flags;
		SceUID pid = cb->pid;
		int text = (perms & SEGMENT_PERM_MASK) == SEGMENT_PERM_TEXT;
		SceUInt32 type, base, allocSize;
		SceKernelAllocMemBlockKernelOpt opt;

		memset(&opt, 0, sizeof(opt));
		opt.attr = 0xb0400000;
		if (text) {
			opt.attr = 0xa0400000;
			base = 0xa0000000;
		} else {
			base = 0xb0000000;
		}
		opt.size = sizeof(opt);
		type = flags & MODULE_FLAG_TYPE_MASK;

		if (pid == KERNEL_PID) {
			memtype = 0x1020d006;
			if (type == 2)
				memtype = 0x10f0d006;
			ret = ksceKernelAllocMemBlock("SceModuleMgrSegment", memtype, total, &opt);
		} else {
			opt.attr = base | 0x480080;
			allocSize = total;
			opt.pid = pid;
			if ((flags & MODULE_FLAG_SHARED_ANY) == 0) {
				if (type == 1)
					memtype = text ? 0x0e20d050 : 0x0e20d060;
				else if (type == 2)
					memtype = text ? 0x08f0d050 : 0x08f0d060;
				else
					memtype = text ? 0x0c20d050 : 0x0c20d060;
				if ((flags & MODULE_FLAG_PROCESS_IMAGE) != 0 && LibraryDBIsAslr(pid) && text)
					allocSize = total + LoadelfSetRandomOffset(&opt, 0x7f);
				if ((cb->flags & MODULE_FLAG_SELF_FIXED) == 0) {
					opt.vbase = PAGE_TRUNC((SceUInt32)seg->vaddr);
					opt.attr |= 1;
				}
			} else {
				if ((flags & MODULE_FLAG_SHARED_TOOL) == 0 || (perms & SEGMENT_PERM_WRITE) != 0) {
					if (type == 1) {
						memtype = text ? 0x03a0d050 : 0x03a0d060;
					} else if (type == 2) {
						memtype = text ? 0x03f0d050 : 0x03f0d060;
					} else {
						memtype = 0x0390d050;
						if (!text)
							memtype = ksceSblACMgrIsGameProgram2(pid) ?
								  0x0390d060 : 0x0320d060;
					}
				} else {
					memtype = text ? 0x03f0d050 : 0x03f0d060;
				}
				if ((cb->flags & MODULE_FLAG_SHARED_ORIGINAL) != 0) {
					if ((seg->perms & SEGMENT_PERM_MASK) == SEGMENT_PERM_TEXT &&
					    sceKernelGetProcessType(pid) == 0x4000000)
						allocSize = total + LoadelfSetRandomOffset(&opt,
								total < 0x10000 ? 3 : 0xf);
				} else {
					opt.mirror_blockid = seg->memblk;
					opt.attr |= 0x800040;
					if ((seg->perms & SEGMENT_PERM_WRITE) == 0 &&
					    (cb->flags & MODULE_FLAG_SHARED_COPY) != 0) {
						memtype = 0x0320d050;
						opt.attr |= 0x1800040;
					}
				}
			}
			ret = ksceKernelAllocMemBlock("SceModuleMgrUserTextData", memtype, allocSize, &opt);
		}
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 336, "memory allocation error (ret:0x%08x, size:%d)\n",
					     ret, total);
			return ret;
		}

		seg->memblk = ret;
		ksceKernelGetMemBlockBase(ret, &seg->vaddr);
		ctx->seg[index].kaddr = seg->vaddr;
		seg->vaddr = (char *)seg->vaddr + pageOff;

		if (cb->pid != KERNEL_PID) {
			if ((seg->perms & SEGMENT_PERM_WRITE) == 0 &&
			    (cb->flags & MODULE_FLAG_SHARED_COPY) != 0) {
				ctx->seg[index].kaddr = NULL;
				return 0;
			}
			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = 0x1000040;
			opt.mirror_blockid = seg->memblk;
			ret = ksceKernelAllocMemBlock("SceModuleMgrKernelTmp", 0x6020d006, 0, &opt);
			if (ret < 0) {
				LoadelfFreeMem(cb, &seg->vaddr, &seg->memblk, &ctx->seg[index]);
				return ret;
			}
			ctx->seg[index].tmpBlock = ret;
			ksceKernelGetMemBlockBase(ret, &ctx->seg[index].kaddr);
		}
	}

	fill = (seg->perms & SEGMENT_PERM_MASK) == SEGMENT_PERM_TEXT ? 0xb6 : 0;
	if (pageOff != 0)
		memset(ctx->seg[index].kaddr, fill, pageOff);
	ctx->seg[index].kaddr = (char *)ctx->seg[index].kaddr + pageOff;
	if (total - pageOff != seg->filesz)
		memset((char *)ctx->seg[index].kaddr + seg->filesz, fill,
		       total - pageOff - seg->filesz);

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) == 0)
		return SelfReaderLoadSegment(reader, index, ctx->seg[index].kaddr, timing);
	return LoadelfCopySharedSegment(ctx, index);
}

SceUInt32 Loadelf_GetAddrBias(LoadelfContext *ctx, const void *addr)
{
	SceModuleCB *cb = ctx->cb;
	int idx = ModuleCBGetSegmentIndex(cb, (SceUInt32)addr);
	SceModuleSegment *seg = idx < cb->nSegments ? &cb->segment[idx] : NULL;

	SCE_DBG_ASSERT_LEVEL(0, ctx->seg[idx].kaddr != NULL, 789);
	return (SceUInt32)ctx->seg[idx].kaddr - (SceUInt32)seg->vaddr;
}

static int LoadelfLoadSharedImage(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceUInt16 i;
	int ret;

	memcpy(&cb->nSegments, &cb->shared->cb->nSegments, 0x48);
	cb->flags |= MODULE_FLAG_SELF_FIXED;
	for (i = 0; i < cb->nSegments; i++) {
		ret = LoadelfLoadSegment(ctx, NULL, i, NULL);
		if (ret < 0) {
			for (i++; i < cb->nSegments; i++)
				cb->segment[i].memblk = 0;
			LoadelfFreeSegments(cb, ctx);
			return ret;
		}
	}
	return 0;
}

static int LoadelfAllocRelaTmp(SceModuleCB *cb, SceSize size)
{
	SceUInt32 type = cb->flags & MODULE_FLAG_TYPE_MASK;
	SceUInt32 memtype;
	SceKernelAllocMemBlockKernelOpt opt;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x400000;
	if (cb->pid == KERNEL_PID) {
		memtype = type == 2 ? 0x10f0d006 : 0x1020d006;
	} else {
		if (type == 1)
			memtype = 0x10a0d006;
		else
			memtype = type == 2 ? 0x10f0d006 : 0x10c0d006;
		opt.attr = 0x400080;
		opt.pid = cb->pid;
	}
	return ksceKernelAllocMemBlock("SceModuleMgrRelaTmp", memtype, PAGE_ROUND(size), &opt);
}

int LoadelfParsePhdrs(LoadelfContext *ctx, SceUID fd, ModuleAuthInfo *auth, SceUInt32 flags)
{
	SceModuleCB *cb = ctx->cb;
	LoadelfTiming timing, *t;
	SelfReader reader;
	Elf32Phdr *ph;
	SceUInt32 rnd, state, align;
	SceUInt16 phnum;
	SceUID pid, uid;
	void *buf;
	int i, loaded, relaCount, ret;
#if FW_DEBUG
	int t0;
#endif

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0)
		return LoadelfLoadSharedImage(ctx);

	ret = SelfReaderOpen(&reader, cb->pid, fd, auth);
	if (ret < 0)
		return ret;

	ret = SelfReaderGetType(&reader);
	if (ret < 0)
		goto close;
	if (ret == ET_SCE_FIXEDEXEC)
		cb->flags |= MODULE_FLAG_SELF_FIXED;
	ctx->entry = SelfReaderGetEntry(&reader);

	if ((cb->flags & MODULE_FLAG_PROCESS_IMAGE) != 0 &&
	    (cb->flags & MODULE_FLAG_SELF_FIXED) != 0 &&
	    (((SceUInt32 *)reader.ehdr)[9] & 0x1000) == 0)
		LibraryDBSetAslr(cb->pid);

	if ((cb->flags & MODULE_FLAG_SHARED_ORIGINAL) != 0 &&
	    (cb->flags & MODULE_FLAG_SELF_FIXED) == 0) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		goto close;
	}

	phnum = SelfReaderGetPhnum(&reader);
	cb = ctx->cb;
	pid = cb->pid;
#if FW_DEBUG
	if (g_loadTiming == NULL) {
		t = NULL;
	} else {
		t = &timing;
		timing.loadUsec = 0;
		timing.readUsec = 0;
		timing.relocUsec = 0;
	}
#else
	t = NULL;
#endif
	g_relocForms = 0;
	if ((flags & LOAD_FLAG_TIMING) != 0) {
		timing.path = cb->path;
		timing.totalSize = 0;
		for (i = 0; i < phnum; i++) {
			ph = (Elf32Phdr *)reader.phdr + i;
			if (ph->p_type == PT_LOAD || ph->p_type == PT_SCE_RELA)
				timing.totalSize += ph->p_filesz;
		}
		timing.start = ksceKernelGetSystemTimeLow();
		t = &timing;
		timing.loadedSize = 0;
	}

	if (phnum == 0)
		goto bad;

	relaCount = 0;
	loaded = 0;
	state = 0;
	for (i = 0; i < phnum; i++) {
		ph = (Elf32Phdr *)reader.phdr + i;
		if (ph->p_type == PT_LOAD) {
			SceModuleSegment *seg = &cb->segment[i];

			ctx->seg[i].vaddr = ph->p_vaddr;
			seg->filesz = ph->p_filesz;
			seg->memsz = ph->p_memsz;
			seg->perms = ph->p_flags;
			seg->extraPages = (ph->p_flags >> 20) & 0x7f;
			seg->alignLog2 = 31 - __builtin_clz(ph->p_align);
			if (loaded > 2 || state > 1 || ph->p_memsz < ph->p_filesz)
				goto bad;
			align = ph->p_align;
			if ((align & (align - 1)) != 0 || align < 0x10 ||
			    ((cb->flags & MODULE_FLAG_SELF_FIXED) != 0 && align > 0x1000))
				goto bad;
			if (seg->memsz != 0 || seg->extraPages != 0) {
				if ((cb->flags & MODULE_FLAG_SELF_FIXED) != 0) {
					if (LibraryDBIsAslr(pid)) {
						if (cb->pid == KERNEL_PID)
							ksceMt19937GlobalUninit(&rnd, sizeof(rnd));
						else
							SceProcessmgrForDriver_2CEB1C7A(pid, &rnd, sizeof(rnd));
						seg->vaddr = (void *)(rnd & 0xfff & -ph->p_align);
					}
				} else {
					seg->vaddr = (void *)ph->p_vaddr;
				}
				ret = LoadelfLoadSegment(ctx, &reader, i, t);
				loaded = i + 1;
				if (ret < 0)
					goto fail;
			}
			state = 1;
		} else {
			if (ph->p_filesz != 0 && ph->p_align != 0x10)
				goto bad;
			if (ph->p_type == PT_SCE_COMMENT) {
				if (state - 1 > 2)
					goto bad;
				if (ph->p_filesz != 0) {
					SceModuleCB *mcb = ctx->cb;
					SceUID mpid = mcb->pid;

					buf = ModulemgrAlloc(mpid, ph->p_filesz);
					if (buf == NULL) {
						ret = SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
						goto fail;
					}
					ret = SelfReaderLoadSegment(&reader, i, buf, NULL);
					if (ret < 0) {
						ModulemgrFree(mpid, buf);
						goto fail;
					}
					mcb->metadata = buf;
					mcb->metadataSize = ph->p_filesz;
				}
				state = 4;
			} else if (ph->p_type == PT_SCE_VERSION) {
				if (state - 1 > 1)
					goto bad;
				state = 3;
			} else if (ph->p_type == PT_SCE_RELA) {
				if ((cb->flags & MODULE_FLAG_SELF_FIXED) == 0 || state - 1 > 1 ||
				    relaCount > 2)
					goto bad;
				if (ph->p_filesz != 0) {
					uid = LoadelfAllocRelaTmp(ctx->cb, ph->p_filesz);
					ret = uid;
					if (uid < 0)
						goto fail;
					ksceKernelGetMemBlockBase(uid, &buf);
					if (buf == NULL) {
						ret = SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
					} else {
						ret = SelfReaderLoadSegment(&reader, i, buf, t);
						if (ret >= 0) {
#if FW_DEBUG
							if (t == NULL) {
								ret = RelocatorRelSegment(ctx, buf, ph->p_filesz);
							} else {
								t0 = ksceKernelGetSystemTimeLow();
								ret = RelocatorRelSegment(ctx, buf, ph->p_filesz);
								t->relocUsec += ksceKernelGetSystemTimeLow() - t0;
							}
#else
							ret = RelocatorRelSegment(ctx, buf, ph->p_filesz);
#endif
							ksceKernelFreeMemBlock(uid);
							if (ret < 0)
								goto fail;
							relaCount++;
							state = 2;
							goto next;
						}
					}
					ksceKernelFreeMemBlock(uid);
					goto fail;
				}
				state = 2;
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 274, "found unknown segment (p_type:0x%08X)\n",
						     ph->p_type);
				ret = SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
				goto fail;
			}
		}
next:
		cb->nSegments = loaded;
	}

	if (loaded != 0) {
		ret = 0;
#if FW_DEBUG
		if (g_loadTiming != NULL)
			SCE_DBG_PRINTF_LEVEL(1, 0, 286,
				"%s: load: %d usec (file read: %d usec), relocation: %d usec  use_form: 0x%03x\n",
				cb->path, t->loadUsec, t->readUsec, t->relocUsec, g_relocForms);
#endif
		goto close;
	}
bad:
	ret = SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
fail:
	LoadelfFreeSegments(cb, ctx);
close:
	SelfReaderClose(&reader);
	return ret;
}

/* name unknown */
int LoadelfFreeImage(SceModuleCB *cb)
{
	LoadelfFreeSegments(cb, NULL);
	return 0;
}

int LoadelfMapImage(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceModuleSegment *seg;
	LoadelfSegment *segx;
	SceUInt32 memtype;
	int i, ret;

	memtype = (cb->flags & MODULE_FLAG_TYPE_MASK) == 2 ? 0x10f0d005 : 0x1020d005;
	for (i = 0; i < cb->nSegments; i++) {
		seg = &cb->segment[i];
		segx = &ctx->seg[i];
		if ((seg->perms & SEGMENT_PERM_MASK) == SEGMENT_PERM_TEXT &&
		    (cb->flags & MODULE_FLAG_SHARED_COPY) == 0)
			ksceKernelDcacheCleanRange((void *)PAGE_TRUNC((SceUInt32)segx->kaddr),
				PAGE_ROUND(seg->memsz + ((SceUInt32)seg->vaddr & 0xfff)));
		if (cb->pid == KERNEL_PID) {
			if ((seg->perms & SEGMENT_PERM_MASK) != SEGMENT_PERM_TEXT)
				continue;
			ret = ksceKernelRemapMemBlock(seg->memblk, memtype);
		} else {
			if (segx->tmpBlock > 0) {
				ksceKernelFreeMemBlock(segx->tmpBlock);
				segx->tmpBlock = 0;
				segx->kaddr = NULL;
			}
			ret = ksceKernelMapMemBlock(seg->memblk);
		}
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 955,
				"failed(0x%08x) uid=0x%08x, memblk_addr=%p, base_addr=%p\n",
				ret, seg->memblk, segx->kaddr, seg->vaddr);
			return ret;
		}
	}
	return 0;
}

int sceKernelLoadPtLoadSegForFwloader(const char *path, int index, void *dst, SceSize size,
				      int flags, SceSize *out)
{
	ModuleAuthInfo auth;
	SelfReader reader;
	Elf32Phdr *ph;
	SceUID fd;
	int ret, type;

	if (flags != 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	memset(&auth, 0, sizeof(auth));
	ret = ksceKernelGetProcessSelfAuthInfo(sceKernelGetProcessId(), &auth.caller);
	if (ret < 0)
		return ret;
	ret = ksceSblACMgrGetMediaType2(path, &auth.mediaType);
	if (ret < 0)
		return ret;
	auth.flags = flags;

	fd = ksceIoOpen(path, 1, 0);
	if (fd < 0)
		return fd;
	ret = SelfReaderOpen(&reader, KERNEL_PID, fd, &auth);
	if (ret >= 0) {
		type = SelfReaderGetType(&reader);
		ret = type;
		if (type >= 0) {
			ph = (Elf32Phdr *)reader.phdr + index;
			if (index < SelfReaderGetPhnum(&reader) && type == ET_SCE_FIXEDEXEC &&
			    ph->p_type == PT_LOAD && ph->p_filesz != 0 && ph->p_filesz <= size) {
				ret = SelfReaderLoadSegment(&reader, index, dst, NULL);
				if (ret >= 0)
					*out = ph->p_filesz;
			} else {
				ret = SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			}
		}
		SelfReaderClose(&reader);
	}
	ksceIoClose(fd);
	SCE_DBG_PRINTF_LEVEL(2, 0, 1098,
		"sceKernelLoadPtLoadSegForFwloader : path=%s, idx=%d, dst=%p, size=0x%08x, filesz=0x%08x\n",
		path, index, dst, size, *out);
	return ret;
}

int sceKernelGetProgramIdentificationInfo(const char *path, SceUInt64 *authId, SceUInt64 *version)
{
	SelfReader reader;
	SceUID fd;
	int ret;

	if (path == NULL || authId == NULL || version == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	fd = ksceIoOpen(path, 1, 0);
	if (fd < 0)
		return fd;
	ret = SelfReaderOpen(&reader, KERNEL_PID, fd, NULL);
	if (ret >= 0) {
		*authId = SelfReaderGetProgramAuthId(&reader);
		*version = SelfReaderGetProgramVersion(&reader);
		SelfReaderClose(&reader);
		ret = 0;
	}
	ksceIoClose(fd);
	if (ret != 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1141, "getProgramIdentificationInfo failed. 0x%08x\n", ret);
	return ret;
}
