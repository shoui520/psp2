/*
 * self_parser.c - reading in-memory ELF images in SKBL
 *
 * SKBL's modules are already unpacked in memory, so its parser holds only the
 * buffer range, the ELF header and the program header table, and each segment
 * is copied out of the buffer by memmove.
 */

#include "modulemgr_internal.h"

#define ET_SCE_EXEC		0xfe00

static inline int SelfParserCheckElfHeader(const void *ehdr, SceSize size)
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

int SelfParserOpenFromMem(SelfParser *reader, const void *buffer, SceSize size)
{
	const Elf32_Ehdr *ehdr;
	const char *p = buffer;
	int ret;

	memset(reader, 0, sizeof(*reader));
	if (size >= 0x10 && p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F') {
		ehdr = (const Elf32_Ehdr *)p;
		SCE_DBG_ASSERT_LEVEL(0, 1, 375);	/* in-memory image (fd == BOOTIMAGE_FD) */
		reader->header = (void *)p;
		reader->headerSize = size;
		reader->ehdr = (void *)ehdr;
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

int SelfDecrypterLoadSegment(SelfParser *reader, int index, void *dst, LoadelfTiming *timing)
{
	SceSize filesz = ((Elf32_Phdr *)reader->phdr)[index].p_filesz;
	SceUInt32 offset;

	(void)timing;
	if (filesz == 0)
		return 0;
	offset = SelfParserGetPhdrOffset(reader, index);
	if ((int)offset < 0)
		return offset;
	if (offset + filesz > reader->headerSize)
		return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
	memmove(dst, (const char *)reader->header + offset, filesz);
	return 0;
}
