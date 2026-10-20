/*
 * self_decrypter.c - reading ELF and SELF images
 *
 * The SELF path streams the file through the auth manager on a helper
 * thread (double buffered, with partial inflate).  That pipeline is a
 * implemented below with the original auth-manager interface.
 */

#include "modulemgr_internal.h"
#include <modulemgr/self.h>

#define ET_SCE_EXEC		0xfe00

/* the communication event flag */
#define SELF_EVF_SLOT0		0x00000001
#define SELF_EVF_SLOT1		0x00000002
#define SELF_EVF_DONE		0x00000008
#define SELF_EVF_UNLOCKED	0x80000000

typedef struct SceSelfSegmentInfo {
	SceUInt64 offset;
	SceUInt64 length;
	SceUInt64 compression;
	SceUInt64 encryption;
} SceSelfSegmentInfo;

/* one of the two read buffers shared with the decrypter thread */
typedef struct SelfDecrypterSlot {
	int result;
	int state;			/* <0: setup, 1: read, 2: loaded, 3: done */
	void *buf;
	SceSize size;
} SelfDecrypterSlot;

/* the state shared with the decrypter thread, in Sony's order */
static struct {
	SelfDecrypterSlot slot[2];
	SelfParser *reader;
	SceUID commEvf;
	SceUID decrypterThread;
	SceUInt8 *destination;
	SceSize segmentSize;
	SceSize outputLeft;
	SceUInt16 segmentIndex;
	SceInt16 compression;
	SceDeflatePartialInputParam stream;	/* its cookie is the slot being inflated */
} g_self;

/* Auth manager's segment decompression failure (official name unknown). */
#define SELF_DECOMPRESSION_ERROR ((int)0x800f0516)
static SceUInt8 g_buffer[2][0x10000] __attribute__((aligned(0x40)));

int SelfParserCheckElfHeader(const void *ehdr, SceSize size)
{
	const Elf32_Ehdr *e = ehdr;

	if (size < sizeof(*e) ||
	    (*(const SceUInt32 *)&e->e_ident[4] & 0xff00ff) != 0x10001 ||
	    e->e_version != 1 || e->e_ehsize != sizeof(*e))
		return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	if (e->e_type == ET_SCE_EXEC) {
		if ((SceUInt32)(e->e_phnum - 1) > 4)
			return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	} else if (e->e_type == ET_SCE_FIXEDEXEC) {
		if ((SceUInt32)(e->e_phnum - 2) > 6)
			return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	} else {
		return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	}
	if (e->e_phentsize != sizeof(Elf32_Phdr) ||
	    (e->e_shnum != 0 && e->e_shentsize != 0x28))
		return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	return e->e_phoff <= sizeof(*e) ? 0 : SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
}

int SelfParserOpen(SelfParser *reader, SceUID pid, SceUID fd, SelfAuthRequest *auth)
{
	const SceSelfImageHeader *self;
	const Elf32_Ehdr *ehdr;
	const char *p;
	SceSize size, left, phsize;
	int ret;

	memset(reader, 0, sizeof(*reader));
	reader->fd = -1;
	reader->pid = pid;
	reader->authContext = -1;
	if (fd == BOOTIMAGE_FD) {
		p = BootimageGetData();
		reader->header = (void *)p;
		size = BootimageGetSize();
		reader->headerSize = size;
	} else {
		reader->heap = ModulemgrAlloc(pid, 0x103f);
		if (reader->heap == NULL)
			return SCE_KERNEL_ERROR_MODULEMGR_NOMEM_SELF;
		reader->header = (void *)(((SceUInt32)reader->heap + 0x3f) & ~0x3f);
		ret = sceIoRead(fd, reader->header, 0x1000);
		if (ret < 0)
			goto fail;
		p = reader->header;
		size = ret;
		reader->headerSize = size;
		reader->auth = auth;
	}
	if (size < 0x10)
		goto bad_elf;

	if (p[0] == 'S') {
		if (p[1] != 'C' || p[2] != 'E' || p[3] != '\0')
			goto bad_elf;
		self = (const SceSelfImageHeader *)p;
		reader->fd = fd;
		reader->type = 2;
		left = size - 0x80;
		if (size < 0x80 || self->header.cfHeader.version != 3 ||
		    self->header.cfHeader.category != 1 ||
		    self->header.cfHeader.file_offset - 1 >= 0x1000 ||
		    self->header.elfOffset > left || self->header.phdrOffset > left ||
		    self->header.segmentInfoOffset > left) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_SELF_HEADER;
			goto fail;
		}
		ehdr = (const Elf32_Ehdr *)(p + (SceUInt32)self->header.elfOffset);
		reader->ehdr = (void *)ehdr;
		ret = SelfParserCheckElfHeader(ehdr, size - (SceUInt32)self->header.elfOffset);
		if (ret < 0)
			goto fail;
		reader->phdr = (void *)(p + (SceUInt32)self->header.phdrOffset);
		phsize = ehdr->e_phnum * sizeof(Elf32_Phdr);
		if (phsize > size - (SceUInt32)self->header.phdrOffset)
			goto bad_elf;
		reader->segmentInfo = (void *)(p + (SceUInt32)self->header.segmentInfoOffset);
		if (phsize > size - (SceUInt32)self->header.segmentInfoOffset)
			goto bad_elf;
		SelfDecrypterLock();
		return 0;
	}
	if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F') {
		ehdr = (const Elf32_Ehdr *)p;
		reader->fd = fd;
		reader->ehdr = (void *)ehdr;
		reader->type = 1;
		ret = SelfParserCheckElfHeader(ehdr, size);
		if (ret < 0)
			goto fail;
		if (ehdr->e_phnum * sizeof(Elf32_Phdr) <= size - sizeof(*ehdr)) {
			reader->phdr = (void *)(p + ehdr->e_phoff);
			return 0;
		}
	}
bad_elf:
	ret = SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
fail:
	if (reader->heap != NULL) {
		ModulemgrFree(reader->pid, reader->heap);
		reader->heap = NULL;
	}
	return ret;
}

int SelfParserClose(SelfParser *reader)
{
	if (reader->heap != NULL) {
		ModulemgrFree(reader->pid, reader->heap);
		reader->heap = NULL;
	}
	if (reader->type == 2) {
		sceSblAuthMgrClose(reader->authContext);
		SelfDecrypterUnlock();
	}
	return 0;
}

int SelfParserGetType(SelfParser *reader)
{
	if (reader->ehdr == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	return ((Elf32_Ehdr *)reader->ehdr)->e_type;
}

SceUInt16 SelfParserGetPhnum(SelfParser *reader)
{
	return ((Elf32_Ehdr *)reader->ehdr)->e_phnum;
}

SceUInt32 SelfParserGetEntry(SelfParser *reader)
{
	return ((Elf32_Ehdr *)reader->ehdr)->e_entry;
}

__attribute__((noinline)) void *SelfParserGetSegmentInfo(SelfParser *reader, SceUInt32 index)
{
	if (reader->ehdr == NULL)
		return NULL;
	if (reader->segmentInfo != NULL && index < ((Elf32_Ehdr *)reader->ehdr)->e_phnum)
		return (SceSelfSegmentInfo *)reader->segmentInfo + index;
	return NULL;
}

__attribute__((noinline)) SceUInt32 SelfParserGetPhdrOffset(SelfParser *reader, SceUInt32 index)
{
	Elf32_Phdr *phdr;

	if (reader->ehdr != NULL && reader->phdr != NULL &&
	    index < ((Elf32_Ehdr *)reader->ehdr)->e_phnum) {
		phdr = (Elf32_Phdr *)reader->phdr + index;
		if (phdr != NULL)
			return phdr->p_offset;
	}
	return SCE_KERNEL_ERROR_ILLEGAL_SELF_HEADER;
}

SceUInt64 SelfParserGetProgramAuthId(SelfParser *reader)
{
	return ((SceSelfImageHeader *)reader->header)->appInfo.authId;
}

SceUInt64 SelfParserGetProgramVersion(SelfParser *reader)
{
	return ((SceSelfImageHeader *)reader->header)->appInfo.version;
}

int selfDecrypterWaitBlock(int slot)
{
	int ret;

	ret = sceKernelWaitEventFlag(g_self.commEvf, SELF_EVF_DONE, 5, NULL, NULL);
	return ret < 0 ? ret : g_self.slot[slot].result;
}

const void *selfDecrypterRefill(SceDeflatePartialInputParam *param, unsigned int outputsize)
{
	SelfDecrypterSlot *slot;
	SceSize size;
	SceUInt32 bits;
	int ret;

	g_self.slot[(int)param->cookie].result = 0;
	g_self.slot[(int)param->cookie].state = 3;
	sceKernelSetEventFlag(g_self.commEvf, SELF_EVF_DONE);
	sceKernelWaitEventFlag(g_self.commEvf, SELF_EVF_SLOT0 | SELF_EVF_SLOT1,
				5, &bits, NULL);
	g_self.stream.cookie = (void *)((bits & SELF_EVF_SLOT0) ^ 1);
	slot = &g_self.slot[(int)g_self.stream.cookie];
	size = slot->size;
	if (slot->state < 2) {
		ret = SceSblAuthMgrForKernel_BC422443(g_self.reader->authContext, slot->buf, size, 0, 0);
		if (ret < 0)
			return NULL;
		slot->state = 2;
	}
	g_self.stream.pBufEnd = (void *)(((SceUInt32)slot->buf + size + 3) & ~3);
	return slot->buf;
}

int selfDecrypter_thread_entry(SceSize args, void *argp)
{
	SelfDecrypterSlot *slot;
	SelfParser *reader;
	SceUInt32 bits, status;
	const void *input;
	SceSize size;
	SceUInt16 index;
	int ret = 0, inflateResult;

	(void)args;
	(void)argp;
	for (;;) {
		sceKernelWaitEventFlag(g_self.commEvf, SELF_EVF_SLOT0 | SELF_EVF_SLOT1,
					5, &bits, NULL);
		slot = &g_self.slot[(bits & SELF_EVF_SLOT0) ^ 1];
		if (__builtin_expect(slot->state < 0, 0)) {
			reader = g_self.reader;
			index = g_self.segmentIndex;
			if (!reader->authenticated) {
				ret = sceSblAuthMgrOpen(&reader->authContext);
				if (ret < 0)
					goto complete;
				ret = sceSblAuthMgrAuthHeader(reader->authContext,
						reader->header, reader->headerSize, reader->auth);
				if (ret < 0)
					goto complete;
				reader->authenticated = 1;
			}
			ret = SceSblAuthMgrForKernel_89CCDA2C(reader->authContext, index);
			if (ret >= 0)
				g_self.compression = ret;
		} else {
			size = slot->size;
			if (ret < 0)
				goto complete;
			if (slot->state < 2) {
				ret = SceSblAuthMgrForKernel_BC422443(g_self.reader->authContext, slot->buf, size, 0, 0);
				slot->state = 2;
				if (ret < 0)
					goto complete;
			}
			if (g_self.compression == 1) {
				if (g_self.outputLeft < size) {
					ret = SELF_DECOMPRESSION_ERROR;
				} else {
					memcpy(g_self.destination, slot->buf, size);
					g_self.destination += size;
					g_self.outputLeft -= size;
					ret = 0;
				}
			} else {
				SCE_DBG_ASSERT_LEVEL(0, g_self.compression == 2, 179);
				inflateResult = SceKernelUtilsForDriver_5B9BCD75(slot->buf, NULL, NULL, NULL, &input);
				if (inflateResult >= 0) {
					g_self.stream.pBufEnd = (SceUInt8 *)slot->buf + size;
					g_self.stream.cookie = NULL;
					inflateResult = SceKernelUtilsForDriver_3D74CCDF(g_self.destination,
							g_self.outputLeft, input, NULL, &g_self.stream);
				}
				status = (int)g_self.stream.cookie + (inflateResult < 0 ? 2 : 0);
				slot = &g_self.slot[status & 1];
				ret = status & 2 ? SELF_DECOMPRESSION_ERROR : status;
			}
			slot->state = 3;
		}
complete:
		slot->result = ret;
		sceKernelSetEventFlag(g_self.commEvf, SELF_EVF_DONE);
	}
}

int SelfDecrypterSuspend(SceUInt32 mask)
{
	return sceKernelChangeThreadCpuAffinityMask(g_self.decrypterThread, mask);
}

int SelfDecrypterInit(void)
{
	int ret;

	g_self.slot[1].buf = g_buffer[1];
	g_self.slot[0].buf = g_buffer[0];
	ret = sceKernelCreateEventFlag("SceModuleMgrSelfDecryptComm", 0x3000, SELF_EVF_UNLOCKED,
					NULL);
	g_self.commEvf = ret;
	if (ret < 0)
		return ret;
	ret = sceKernelCreateThread("SceModuleMgrSelfDecrypter", selfDecrypter_thread_entry, 0x6e,
				     0x1000, 0, 0xf, NULL);
	g_self.decrypterThread = ret;
	if (ret < 0)
		return ret;
	g_self.stream.callback = selfDecrypterRefill;
	g_self.stream.size = sizeof(g_self.stream);
	return sceKernelStartThread(ret, 0, NULL);
}

int SelfDecrypterLoadSegment(SelfParser *reader, int index, void *dst, LoadelfTiming *timing)
{
	SceSize filesz = ((Elf32_Phdr *)reader->phdr)[index].p_filesz;
	SceUID fd;

	if (filesz == 0)
		return 0;
	fd = reader->fd;
	if (reader->type == 1) {
		if (reader->fd == BOOTIMAGE_FD && reader->pid == SCE_KERNEL_PROCESS_ID) {
			if (sceLoadcoreIoReadFrom(dst, filesz,
						  SelfParserGetPhdrOffset(reader, index)) != filesz)
				return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			return 0;
		}
	} else if (reader->type == 2) {
		const SceSelfSegmentInfo *segment;
		SceOff offset, position;
		SceSize length, consumed = 0, chunk;
#if FW_DEBUG
		SceUInt32 start = 0, readStart;
#endif
		SceUInt32 now, percent;
		int previous = 1, next, ret, bytesRead;

		g_self.outputLeft = filesz;
		segment = SelfParserGetSegmentInfo(reader, index);
		if (segment == NULL || (segment->length >> 32) != 0)
			return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		length = segment->length;
		if (length == 0)
			return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		offset = segment->offset <= 0x7fffffff ? segment->offset : -0x80000000LL;
		position = sceIoLseek(fd, offset, SCE_SEEK_SET);
		if (position != offset)
			return position < 0 ? (int)position : SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		g_self.reader = reader;
		g_self.segmentIndex = index;
		g_self.destination = dst;
		g_self.segmentSize = length;
		g_self.compression = -1;
		g_self.slot[1].state = -1;
		sceKernelSetEventFlag(g_self.commEvf, SELF_EVF_SLOT1);
#if FW_DEBUG
		if (timing != NULL)
			start = SceThreadmgrForDriver_47F6DE49();
#endif
		for (;;) {
			next = previous ^ 1;
			chunk = length - consumed;
			if (chunk > sizeof(g_buffer[0]))
				chunk = sizeof(g_buffer[0]);
			consumed += chunk;
			g_self.slot[next].size = chunk;
#if FW_DEBUG
			if (timing != NULL)
				readStart = SceThreadmgrForDriver_47F6DE49();
#endif
			bytesRead = sceIoRead(fd, g_self.slot[next].buf, chunk);
			if ((SceSize)bytesRead != chunk) {
				/* The original preserves negative errors and treats a
				 * short or oversized nonnegative read as an execution error. */
				ret = bytesRead < 0 ? bytesRead : 0;
				if (bytesRead >= 0)
					ret = SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
				goto drain;
			}
#if FW_DEBUG
			if (timing != NULL)
				timing->readUsec += SceThreadmgrForDriver_47F6DE49() - readStart;
#endif
			g_self.slot[next].state = 1;
			if (g_self.slot[previous].state == 2) {
				ret = SceSblAuthMgrForKernel_BC422443(g_self.reader->authContext,
						g_self.slot[next].buf, chunk, 0, 0);
				if (ret < 0)
					goto drain;
				g_self.slot[next].state = 2;
			}
			ret = selfDecrypterWaitBlock(previous);
			if (ret < 0)
				return ret;
			sceKernelSetEventFlag(g_self.commEvf, next + 1);
			if (timing != NULL && timing->path != NULL) {
				timing->loadedSize += chunk;
				now = SceThreadmgrForDriver_47F6DE49();
				if (now - timing->start > 1000000 ||
				    (timing->loadedSize >= timing->totalSize && consumed >= length)) {
					percent = (SceUInt64)timing->loadedSize * 100 / timing->totalSize;
					if (percent > 100)
						percent = 100;
					SceSysrootForKernel_C67A1FA9(0, 0, percent, timing->path);
					timing->start = now;
				}
			}
			if (consumed >= length) {
				ret = selfDecrypterWaitBlock(next);
				break;
			}
			previous = next;
		}
		goto finish;
	drain:
		selfDecrypterWaitBlock(previous);
	finish:
#if FW_DEBUG
		if (timing != NULL)
			timing->loadUsec += SceThreadmgrForDriver_47F6DE49() - start;
#endif
		return ret;
	}
	return SCE_KERNEL_ERROR_AUTHFAIL;
}

__attribute__((noinline)) void SelfDecrypterLock(void)
{
	int ret;

	ret = sceKernelWaitEventFlag(g_self.commEvf, SELF_EVF_UNLOCKED, 5, NULL, NULL);
	SCE_DBG_ASSERT_LEVEL(0, ret == 0, 428);
	(void)ret;
}

__attribute__((noinline)) void SelfDecrypterUnlock(void)
{
	int ret;

	ret = sceKernelSetEventFlag(g_self.commEvf, SELF_EVF_UNLOCKED);
	SCE_DBG_ASSERT_LEVEL(0, ret == 0, 435);
	(void)ret;
}
