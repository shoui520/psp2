/* ELF and note headers of the core file. */

#include "seg_internal.h"
#include <elf.h>
#include <sysmem/debug.h>
#include <string.h>
#include <stdio.h>

/* the ELF header of an ARM core file with numPhdr program headers */
void utilsElf32MakeEhdr(Elf32_Ehdr *header, SceUInt32 numPhdr)
{
	SCE_DBG_ASSERT(header != NULL, 29);
	memset(header, 0, sizeof(*header));
	header->e_phoff = sizeof(Elf32_Ehdr);
	header->e_ehsize = sizeof(Elf32_Ehdr);
	header->e_phnum = numPhdr;
	header->e_ident[EI_MAG0] = ELFMAG0;
	header->e_ident[EI_MAG1] = ELFMAG1;
	header->e_ident[EI_MAG2] = ELFMAG2;
	header->e_ident[EI_MAG3] = ELFMAG3;
	header->e_type = ET_CORE;
	header->e_machine = EM_ARM;
	header->e_flags = EF_ARM_EABI_VER5;
	header->e_phentsize = sizeof(Elf32_Phdr);
	header->e_ident[EI_CLASS] = ELFCLASS32;
	header->e_ident[EI_DATA] = ELFDATA2LSB;
	header->e_ident[EI_VERSION] = EV_CURRENT;
	header->e_version = EV_CURRENT;
	header->e_ident[EI_OSABI] = 0;
	header->e_ident[EI_ABIVERSION] = 0;
	header->e_ident[9] = 0;
	header->e_ident[10] = 0;
	header->e_ident[11] = 0;
	header->e_ident[12] = 0;
	header->e_ident[13] = 0;
	header->e_ident[14] = 0;
	header->e_ident[15] = 0;
	header->e_shentsize = 0;
	header->e_shnum = 0;
	header->e_shstrndx = 0;
}

void utilsElf32MakePhent(Elf32_Phdr *header, Elf32_Word type, Elf32_Off offset,
	Elf32_Addr vaddr, Elf32_Addr paddr, Elf32_Word filesz, Elf32_Word memsz,
	Elf32_Word flags, Elf32_Word align)
{
	SCE_DBG_ASSERT(header != NULL, 88);
	memset(header, 0, sizeof(*header));
	header->p_type = type;
	header->p_offset = offset;
	header->p_vaddr = vaddr;
	header->p_paddr = paddr;
	header->p_filesz = filesz;
	header->p_memsz = memsz;
	header->p_flags = flags;
	header->p_align = align;
}

/* a PT_NOTE program header for dataSize bytes at fileOffset */
void utilsElf32MakePtnote(Elf32_Phdr *header, SceUInt32 fileOffset, SceSize dataSize)
{
#if FW_DEBUG
	void *lr = __builtin_return_address(0);

	SceDebugForDriver_35A35322(header != NULL, SCE_DBGINFO(112), lr);
	/* utilsElf32MakePhent inlined, its zero fields left to the memset */
	SceDebugForDriver_35A35322(header != NULL, SCE_DBGINFO_FUNC(88, "utilsElf32MakePhent"), lr);
#endif
	memset(header, 0, sizeof(*header));
	header->p_offset = fileOffset;
	header->p_filesz = dataSize;
	header->p_type = PT_NOTE;
	header->p_align = 4;
}

int utilsMakeNoteHeader(CoredumpNoteHeader *header, const char *name,
	SceUInt32 type, SceUInt32 count, SceSize noteSize)
{
	int result;

	SCE_DBG_ASSERT(header != NULL, 159);
	SCE_DBG_ASSERT(name != NULL, 160);
	memset(header, 0, sizeof(*header));
	header->nameSize[0] = sizeof(header->name);
	header->type = type;
	header->descSize = noteSize - 28;
	header->nameSize[1] = 0;
	header->nameSize[2] = 0;
	header->nameSize[3] = 0;
	result = snprintf(header->name, sizeof(header->name), "%s", name);
	header->count = count;
	return result;
}
