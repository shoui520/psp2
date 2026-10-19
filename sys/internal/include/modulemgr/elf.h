/*
 * The ELF32 records, as the System V ABI names them: the file header and the program headers
 * of the modules the module manager loads and of the core files coredump and crashdump
 * write. No SDK header or debug record names Sony's own; the names are the ABI's, and the
 * SCE values the ones Sony's tools print.
 */
#pragma once

#include <stdint.h>

typedef uint32_t	Elf32_Addr;
typedef uint16_t	Elf32_Half;
typedef uint32_t	Elf32_Off;
typedef int32_t		Elf32_Sword;
typedef uint32_t	Elf32_Word;

/* e_ident */
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_OSABI	7
#define EI_ABIVERSION	8
#define EI_PAD		9
#define EI_NIDENT	16

#define ELFMAG0		0x7f
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'

#define ELFCLASSNONE	0
#define ELFCLASS32	1
#define ELFCLASS64	2

#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

#define EV_NONE		0
#define EV_CURRENT	1

/* e_type */
#define ET_NONE		0
#define ET_REL		1
#define ET_EXEC		2
#define ET_DYN		3
#define ET_CORE		4
#define ET_SCE_FIXEDEXEC	0xfe04

/* e_machine */
#define EM_ARM		40

/* e_flags: the ARM EABI version */
#define EF_ARM_EABIMASK		0xff000000
#define EF_ARM_EABI_VER5	0x05000000

typedef struct {
	unsigned char	e_ident[EI_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

/* p_type */
#define PT_NULL		0
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SCE_RELA	0x60000000
#define PT_SCE_COMMENT	0x6fffff00
#define PT_SCE_VERSION	0x6fffff01

/* p_flags */
#define PF_X		0x1
#define PF_W		0x2
#define PF_R		0x4

typedef struct {
	Elf32_Word	p_type;
	Elf32_Off	p_offset;
	Elf32_Addr	p_vaddr;
	Elf32_Addr	p_paddr;
	Elf32_Word	p_filesz;
	Elf32_Word	p_memsz;
	Elf32_Word	p_flags;
	Elf32_Word	p_align;
} Elf32_Phdr;

_Static_assert(sizeof(Elf32_Ehdr) == 0x34, "ELF32 file header");
_Static_assert(sizeof(Elf32_Phdr) == 0x20, "ELF32 program header");
