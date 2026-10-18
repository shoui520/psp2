/*
 * self_decrypter.c - reading ELF and SELF images
 *
 * The SELF path streams the file through the auth manager on a helper
 * thread (double buffered, with partial inflate).  That pipeline is a
 * implemented below with the original auth-manager interface.
 */

#include "modulemgr_internal.h"
#include <modulemgr/self.h>


#define BOOTIMAGE_FD		0x7f7f7f7f	/* reads come from the boot image */

#define ET_SCE_EXEC		0xfe00

/* the communication event flag */
#define SELF_EVF_SLOT0		0x00000001
#define SELF_EVF_SLOT1		0x00000002
#define SELF_EVF_DONE		0x00000008
#define SELF_EVF_UNLOCKED	0x80000000

typedef struct Elf32Ehdr {
	SceUInt8 e_ident[16];
	SceUInt16 e_type;
	SceUInt16 e_machine;
	SceUInt32 e_version;
	SceUInt32 e_entry;
	SceUInt32 e_phoff;
	SceUInt32 e_shoff;
	SceUInt32 e_flags;
	SceUInt16 e_ehsize;
	SceUInt16 e_phentsize;
	SceUInt16 e_phnum;
	SceUInt16 e_shentsize;
	SceUInt16 e_shnum;
	SceUInt16 e_shstrndx;
} Elf32Ehdr;

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

/* partial inflate stream state handed to SceDeflateForDriver */
typedef struct SelfInflateStream {
	SceUInt32 flags;
	const void *inputEnd;
	int slot;
	void *(*refill)(struct SelfInflateStream *stream);
} SelfInflateStream;

static SelfDecrypterSlot g_slot[2];
static SceUID g_commEvf;
static SceUID g_decrypterThread;
static SelfInflateStream g_stream;
static SelfReader *g_reader;
static SceUInt8 *g_destination;
static SceSize g_segmentSize;
static SceSize g_outputLeft;
static SceUInt16 g_segmentIndex;
static SceInt16 g_compression;

/* Auth manager's segment decompression failure (official name unknown). */
#define SELF_DECOMPRESSION_ERROR ((int)0x800f0516)
static SceUInt8 g_buffer[2][0x10000] __attribute__((aligned(0x40)));

int SelfReaderCheckElfHeader(const void *ehdr, SceSize size)
{
	const Elf32Ehdr *e = ehdr;

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
	if (e->e_phentsize != sizeof(Elf32Phdr) ||
	    (e->e_shnum != 0 && e->e_shentsize != 0x28))
		return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	return e->e_phoff <= sizeof(*e) ? 0 : SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
}

int SelfReaderOpen(SelfReader *reader, SceUID pid, SceUID fd, ModuleAuthInfo *auth)
{
	const SceSelfImageHeader *self;
	const Elf32Ehdr *ehdr;
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
		ret = ksceIoRead(fd, reader->header, 0x1000);
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
		ehdr = (const Elf32Ehdr *)(p + (SceUInt32)self->header.elfOffset);
		reader->ehdr = (void *)ehdr;
		ret = SelfReaderCheckElfHeader(ehdr, size - (SceUInt32)self->header.elfOffset);
		if (ret < 0)
			goto fail;
		reader->phdr = (void *)(p + (SceUInt32)self->header.phdrOffset);
		phsize = ehdr->e_phnum * sizeof(Elf32Phdr);
		if (phsize > size - (SceUInt32)self->header.phdrOffset)
			goto bad_elf;
		reader->segmentInfo = (void *)(p + (SceUInt32)self->header.segmentInfoOffset);
		if (phsize > size - (SceUInt32)self->header.segmentInfoOffset)
			goto bad_elf;
		SelfDecrypterLock();
		return 0;
	}
	if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F') {
		ehdr = (const Elf32Ehdr *)p;
		reader->fd = fd;
		reader->ehdr = (void *)ehdr;
		reader->type = 1;
		ret = SelfReaderCheckElfHeader(ehdr, size);
		if (ret < 0)
			goto fail;
		if (ehdr->e_phnum * sizeof(Elf32Phdr) <= size - sizeof(*ehdr)) {
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

int SelfReaderClose(SelfReader *reader)
{
	if (reader->heap != NULL) {
		ModulemgrFree(reader->pid, reader->heap);
		reader->heap = NULL;
	}
	if (reader->type == 2) {
		ksceSblAuthMgrClose(reader->authContext);
		SelfDecrypterUnlock();
	}
	return 0;
}

int SelfReaderGetType(SelfReader *reader)
{
	if (reader->ehdr == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
	return ((Elf32Ehdr *)reader->ehdr)->e_type;
}

SceUInt16 SelfReaderGetPhnum(SelfReader *reader)
{
	return ((Elf32Ehdr *)reader->ehdr)->e_phnum;
}

SceUInt32 SelfReaderGetEntry(SelfReader *reader)
{
	return ((Elf32Ehdr *)reader->ehdr)->e_entry;
}

void *SelfReaderGetSegmentInfo(SelfReader *reader, SceUInt32 index)
{
	if (reader->ehdr == NULL)
		return NULL;
	if (reader->segmentInfo != NULL && index < ((Elf32Ehdr *)reader->ehdr)->e_phnum)
		return (SceSelfSegmentInfo *)reader->segmentInfo + index;
	return NULL;
}

SceUInt32 SelfReaderGetSegmentOffset(SelfReader *reader, SceUInt32 index)
{
	Elf32Phdr *phdr;

	if (reader->ehdr != NULL && reader->phdr != NULL &&
	    index < ((Elf32Ehdr *)reader->ehdr)->e_phnum) {
		phdr = (Elf32Phdr *)reader->phdr + index;
		if (phdr != NULL)
			return phdr->p_offset;
	}
	return SCE_KERNEL_ERROR_ILLEGAL_SELF_HEADER;
}

SceUInt64 SelfReaderGetProgramAuthId(SelfReader *reader)
{
	return ((SceSelfImageHeader *)reader->header)->appInfo.authId;
}

SceUInt64 SelfReaderGetProgramVersion(SelfReader *reader)
{
	return ((SceSelfImageHeader *)reader->header)->appInfo.version;
}

int selfDecrypterWaitBlock(int slot)
{
	int ret;

	ret = sceKernelWaitEventFlag(g_commEvf, SELF_EVF_DONE, 5, NULL, NULL);
	return ret < 0 ? ret : g_slot[slot].result;
}

void *selfDecrypterRefill(SelfInflateStream *stream)
{
	SelfDecrypterSlot *slot;
	SceSize size;
	SceUInt32 bits;
	int ret;

	g_slot[stream->slot].result = 0;
	g_slot[stream->slot].state = 3;
	sceKernelSetEventFlag(g_commEvf, SELF_EVF_DONE);
	sceKernelWaitEventFlag(g_commEvf, SELF_EVF_SLOT0 | SELF_EVF_SLOT1,
				5, &bits, NULL);
	g_stream.slot = (bits & SELF_EVF_SLOT0) ^ 1;
	slot = &g_slot[g_stream.slot];
	size = slot->size;
	if (slot->state < 2) {
		ret = ksceSblAuthMgrLoadBlock(g_reader->authContext, slot->buf, size);
		if (ret < 0)
			return NULL;
		slot->state = 2;
	}
	g_stream.inputEnd = (void *)(((SceUInt32)slot->buf + size + 3) & ~3);
	return slot->buf;
}

int selfDecrypter_thread_entry(SceSize args, void *argp)
{
	SelfDecrypterSlot *slot;
	SelfReader *reader;
	const void *input;
	SceUInt32 bits, status;
	SceSize size;
	SceUInt16 index;
	int ret = 0, inflateResult;

	(void)args;
	(void)argp;
	for (;;) {
		sceKernelWaitEventFlag(g_commEvf, SELF_EVF_SLOT0 | SELF_EVF_SLOT1,
					5, &bits, NULL);
		slot = &g_slot[(bits & SELF_EVF_SLOT0) ^ 1];
		if (slot->state < 0) {
			reader = g_reader;
			index = g_segmentIndex;
			if (!reader->authenticated) {
				ret = ksceSblAuthMgrOpen(&reader->authContext);
				if (ret < 0)
					goto complete;
				ret = ksceSblAuthMgrAuthHeader(reader->authContext,
						reader->header, reader->headerSize, reader->auth);
				if (ret < 0)
					goto complete;
				reader->authenticated = 1;
			}
			ret = ksceSblAuthMgrSetupAuthSegment(reader->authContext, index);
			if (ret >= 0)
				g_compression = ret;
		} else {
			size = slot->size;
			if (ret < 0)
				goto complete;
			if (slot->state < 2) {
				ret = ksceSblAuthMgrLoadBlock(g_reader->authContext, slot->buf, size);
				slot->state = 2;
				if (ret < 0)
					goto complete;
			}
			if (g_compression == 1) {
				if (g_outputLeft < size) {
					ret = SELF_DECOMPRESSION_ERROR;
				} else {
					memcpy(g_destination, slot->buf, size);
					g_destination += size;
					g_outputLeft -= size;
					ret = 0;
				}
			} else {
				SCE_DBG_ASSERT_LEVEL(0, g_compression == 2, 179);
				inflateResult = SceKernelUtilsForDriver_5B9BCD75(slot->buf, NULL, NULL, NULL, &input);
				if (inflateResult >= 0) {
					g_stream.inputEnd = (SceUInt8 *)slot->buf + size;
					g_stream.slot = 0;
					inflateResult = SceKernelUtilsForDriver_3D74CCDF(g_destination,
							g_outputLeft, input, NULL, &g_stream);
				}
				status = g_stream.slot + (inflateResult < 0 ? 2 : 0);
				slot = &g_slot[status & 1];
				ret = status & 2 ? SELF_DECOMPRESSION_ERROR : status;
			}
			slot->state = 3;
		}
complete:
		slot->result = ret;
		sceKernelSetEventFlag(g_commEvf, SELF_EVF_DONE);
	}
}

int SelfDecrypterSuspend(SceUInt32 mask)
{
	return sceKernelChangeThreadCpuAffinityMask(g_decrypterThread, mask);
}

int SelfDecrypterInit(void)
{
	int ret;

	g_slot[1].buf = g_buffer[1];
	g_slot[0].buf = g_buffer[0];
	ret = sceKernelCreateEventFlag("SceModuleMgrSelfDecryptComm", 0x3000, SELF_EVF_UNLOCKED,
					NULL);
	g_commEvf = ret;
	if (ret < 0)
		return ret;
	ret = sceKernelCreateThread("SceModuleMgrSelfDecrypter", selfDecrypter_thread_entry, 0x6e,
				     0x1000, 0, 0xf, NULL);
	g_decrypterThread = ret;
	if (ret < 0)
		return ret;
	g_stream.refill = selfDecrypterRefill;
	g_stream.flags = 0x10;
	return sceKernelStartThread(ret, 0, NULL);
}

int SelfReaderLoadSegment(SelfReader *reader, int index, void *dst, LoadelfTiming *timing)
{
	SceSize filesz = ((Elf32Phdr *)reader->phdr)[index].p_filesz;

	if (filesz == 0)
		return 0;
	if (reader->type == 1) {
		if (reader->fd == BOOTIMAGE_FD && reader->pid == KERNEL_PID) {
			if (sceLoadcoreIoReadFrom(dst, filesz,
						  SelfReaderGetSegmentOffset(reader, index)) != filesz)
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

		g_outputLeft = filesz;
		segment = SelfReaderGetSegmentInfo(reader, index);
		if (segment == NULL || segment->length == 0 || segment->length >> 32)
			return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		length = segment->length;
		offset = segment->offset <= 0x7fffffff ? segment->offset : -0x80000000LL;
		position = ksceIoLseek(reader->fd, offset, SCE_SEEK_SET);
		if (position != offset)
			return position < 0 ? (int)position : SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		g_segmentIndex = index;
		g_compression = -1;
		g_slot[1].state = -1;
		g_reader = reader;
		g_destination = dst;
		g_segmentSize = length;
		sceKernelSetEventFlag(g_commEvf, SELF_EVF_SLOT1);
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
			g_slot[next].size = chunk;
#if FW_DEBUG
			if (timing != NULL)
				readStart = SceThreadmgrForDriver_47F6DE49();
#endif
			bytesRead = ksceIoRead(reader->fd, g_slot[next].buf, chunk);
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
			g_slot[next].state = 1;
			if (g_slot[previous].state == 2) {
				ret = ksceSblAuthMgrLoadBlock(g_reader->authContext,
						g_slot[next].buf, chunk);
				if (ret < 0)
					goto drain;
				g_slot[next].state = 2;
			}
			ret = selfDecrypterWaitBlock(previous);
			if (ret < 0)
				return ret;
			sceKernelSetEventFlag(g_commEvf, next + 1);
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

void SelfDecrypterLock(void)
{
	int ret;

	ret = sceKernelWaitEventFlag(g_commEvf, SELF_EVF_UNLOCKED, 5, NULL, NULL);
	SCE_DBG_ASSERT_LEVEL(0, ret == 0, 428);
	(void)ret;
}

void SelfDecrypterUnlock(void)
{
	int ret;

	ret = sceKernelSetEventFlag(g_commEvf, SELF_EVF_UNLOCKED);
	SCE_DBG_ASSERT_LEVEL(0, ret == 0, 435);
	(void)ret;
}
