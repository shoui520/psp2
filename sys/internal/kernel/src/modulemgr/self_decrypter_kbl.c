/*
 * self_decrypter_kbl.c - reading ELF and SELF images in NSKBL
 *
 * The boot loader's version of self_decrypter.c. It has no threads and no file
 * positioning, so the open reads the file whole, up to 384 KiB, into one static
 * buffer, and each segment is authenticated in that buffer by the auth manager
 * and inflated from it into place.
 */

#include "modulemgr_internal.h"
#include <modulemgr/self.h>

#define ET_SCE_EXEC		0xfe00

/* the file, and the space a misaligned segment is moved to */
#define SELF_BUFFER_SIZE	0x60000
#define SELF_HEADER_SIZE	0x1000
#define SELF_SEGMENT_ALIGN	64

/* name unknown: what a segment the file cannot hold gives (os0: an offset past 2 GiB) */
#define SELF_SEGMENT_INVALID	((int)0x80000000)

static SceUInt8 s_buffer[SELF_BUFFER_SIZE] __attribute__((aligned(SELF_SEGMENT_ALIGN)));

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
	reader->pid = pid;
	reader->header = s_buffer;
	reader->fd = -1;
	reader->authContext = -1;
	ret = sceIoRead(fd, reader->header, SELF_BUFFER_SIZE);
	if (ret < 0)
		return ret;
	reader->fileSize = ret;
	size = (SceSize)ret > SELF_HEADER_SIZE ? SELF_HEADER_SIZE : (SceSize)ret;
	reader->auth = auth;
	reader->headerSize = size;
	p = reader->header;
	if (size < 0x10)
		return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;

	if (p[0] == 'S') {
		if (p[1] != 'C' || p[2] != 'E' || p[3] != '\0')
			return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
		self = (const SceSelfImageHeader *)p;
		reader->fd = fd;
		reader->type = 2;
		left = size - 0x80;
		if (size < 0x80 || self->header.cfHeader.version != 3 ||
		    self->header.cfHeader.category != 1 ||
		    self->header.cfHeader.file_offset - 1 >= 0x1000 ||
		    self->header.elfOffset > left || self->header.phdrOffset > left ||
		    self->header.segmentInfoOffset > left)
			return SCE_KERNEL_ERROR_ILLEGAL_SELF_HEADER;
		ehdr = (const Elf32_Ehdr *)(p + (SceUInt32)self->header.elfOffset);
		reader->ehdr = (void *)ehdr;
		ret = SelfParserCheckElfHeader(ehdr, size - (SceUInt32)self->header.elfOffset);
		if (ret < 0)
			return ret;
		reader->phdr = (void *)(p + (SceUInt32)self->header.phdrOffset);
		phsize = ehdr->e_phnum * sizeof(Elf32_Phdr);
		if (phsize > size - (SceUInt32)self->header.phdrOffset)
			return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
		reader->segmentInfo = (void *)(p + (SceUInt32)self->header.segmentInfoOffset);
		if (phsize > size - (SceUInt32)self->header.segmentInfoOffset)
			return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
		return 0;
	}
	if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F') {
		ehdr = (const Elf32_Ehdr *)p;
		reader->fd = fd;
		reader->ehdr = (void *)ehdr;
		reader->type = 1;
		ret = SelfParserCheckElfHeader(ehdr, size);
		if (ret < 0)
			return ret;
		if (ehdr->e_phnum * sizeof(Elf32_Phdr) <= size - sizeof(*ehdr)) {
			reader->phdr = (void *)(p + ehdr->e_phoff);
			return 0;
		}
	}
	return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
}

int SelfParserClose(SelfParser *reader)
{
	if (reader->type == 2)
		sceSblAuthMgrClose(reader->authContext);
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

/*
 * segment index authenticated in the buffer (moved to a 64-byte boundary first), then
 * copied or inflated to dst; NSKBL keeps no timing
 */
int SelfDecrypterLoadSegment(SelfParser *reader, int index, void *dst, LoadelfTiming *timing)
{
	SceSize filesz = ((Elf32_Phdr *)reader->phdr)[index].p_filesz;
	const SceSelfSegmentInfo *segment;
	SceSize length;
	SceUInt32 offset;
	void *src;
	int ret;

	(void)timing;
	if (filesz == 0)
		return 0;
	if (reader->type != 2)
		return SCE_KERNEL_ERROR_AUTHFAIL;
	segment = SelfParserGetSegmentInfo(reader, index);
	if (segment == NULL)
		return SELF_SEGMENT_INVALID;
	length = segment->length <= 0xffffffff ? segment->length : 0;
	if (segment->offset > 0x7fffffff)
		return SELF_SEGMENT_INVALID;
	offset = segment->offset;
	if (offset > reader->fileSize || length + offset > reader->fileSize) {
		SCE_DBG_PRINTF(6, 67, "too big module to load....\n");
		return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
	}
	src = s_buffer + offset;
	if (offset & (SELF_SEGMENT_ALIGN - 1))
		src = __memmove_chk(s_buffer + SELF_HEADER_SIZE, src, length,
				    SELF_BUFFER_SIZE - SELF_HEADER_SIZE);
	if (!reader->authenticated) {
		ret = sceSblAuthMgrOpen(&reader->authContext);
		if (ret < 0)
			goto setupFailed;
		ret = sceSblAuthMgrAuthHeader(reader->authContext, reader->header,
					      reader->headerSize, reader->auth);
		if (ret < 0)
			goto setupFailed;
		reader->authenticated = 1;
	}
	ret = sceSblAuthMgrSetupAuthSegment(reader->authContext, index);
	if (ret < 0)
		goto setupFailed;
	ret = sceSblAuthMgrAuthSegment(reader->authContext, src, length, 0, 0);
	if (ret < 0) {
		SCE_DBG_PRINTF(11, 86, "authseg failed (0x%08x)\n", ret);
		return ret;
	}
	if (ret == 1) {
		/* stored as it is */
		memcpy(dst, src, length);
		return 0;
	}
	return sceZlibDecompress(dst, filesz, src, NULL);

setupFailed:
	SCE_DBG_PRINTF(11, 79, "setup failed (0x%08x)\n", ret);
	return ret;
}
