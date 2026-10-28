/* ELF boot-loader helpers, identified by the original loadelf_boot.c diagnostics. */
#include "loadelf_boot.h"
#include "ipl_data.h"
#include "memlmd.h"
#include <elf.h>
#include <pspmoduleinfo.h>

_Static_assert(sizeof(Elf32_Ehdr) == 52, "ELF32 file header");
_Static_assert(sizeof(Elf32_Phdr) == 32, "ELF32 program header");
_Static_assert(sizeof(Elf32_Shdr) == 40, "ELF32 section header");
_Static_assert(sizeof(SceModuleInfo) == 52, "PSP module information");
_Static_assert(sizeof(Elf32_Rel) == 8, "ELF32 relocation");
_Static_assert(sizeof(SceLoadCoreBootSegments) == 64, "IPL segment table");
_Static_assert(sizeof(SceLoadCoreCompressedRelocationHeader) == 4,
	       "compressed relocation header");

u32 GetRelocationBits(u32 value, u32 shift, u32 bits)
{
	u32 mask = (1U << (bits & 31)) - 1;

	return (value >> (shift & 31)) & mask;
}

s32 ShiftRelocationHalfword(u32 value, u32 shift)
{
	return (s32)(s16)value >> (shift & 31);
}

u32 MergeRelocationHalfwords(u32 high, u32 low, u32 shift)
{
	s32 upper = (s32)(high << 16);

	return ((u32)(upper >> (shift & 31)) & 0xFFFF0000) | (low & 0xFFFF);
}

int setGetLengthFunc(int (*function)(u8 *file, SceSize size, SceSize *newSize))
{
	g_getLengthFunc = function;
	return 0;
}

int setPrepareGetLengthFunc(int (*function)(u8 *buffer, SceSize size))
{
	g_prepareGetLengthFunc = function;
	return 0;
}

int setMaskFunc(int (*function)(u32 operation, const u8 *hash))
{
	g_setMaskFunc = function;
	return 0;
}

int setCompareSubTypeFunc(int (*function)(u32 tag))
{
	g_compareSubType = function;
	return 0;
}

int setCompareLatestSubTypeFunc(int (*function)(u32 tag))
{
	g_compareLatestSubType = function;
	return 0;
}

int prepareModuleHeader(u8 *buffer, SceSize size)
{
	if (g_prepareGetLengthFunc == NULL)
		return -1;
	return g_prepareGetLengthFunc(buffer, size) == 0 ? 0 : -1;
}

int compareBootModuleLatestSubType(const PspHeader *header)
{
	const u8 *tag = (const u8 *)&header->tag;
	u32 value = (u32)tag[1] << 16;

	value |= (u32)tag[0] << 24;
	value |= (u32)tag[2] << 8;
	value |= tag[3];
	return memlmdCompareLatestSubType(value);
}

int getBootModuleLength(u8 *file, SceSize size, u32 unused)
{
	PspHeader *header = (PspHeader *)file;
	SceSize newSize = (SceSize)-1;

	if (header->magic != PSP_MAGIC)
		return -1;
	/* The original deliberately ignores this first comparison's result. */
	compareBootModuleLatestSubType(header);
	if (g_prepareGetLengthFunc != NULL &&
	    g_prepareGetLengthFunc(file, size) != 0)
		return -1;
	if (compareBootModuleLatestSubType(header) == 0)
		return -1;
	if (g_setMaskFunc == NULL)
		return SCE_ERROR_KERNEL_NO_DECRYPTOR;
	if (g_setMaskFunc(0, (const u8 *)0xBFC00200) != 0)
		return -1;
	if (g_getLengthFunc == NULL)
		return SCE_ERROR_KERNEL_NO_DECRYPTOR;
	if (g_getLengthFunc(file, size, &newSize) != 0)
		newSize = (SceSize)-1;
	return newSize;
}

int checkBootModuleHeader(SceLoadCoreExecFileInfo *execInfo)
{
	const PspHeader *header;
	u32 magic;
	u16 attributes;
	u32 imageSize;
	u16 moduleAttributes;
	u32 decompressedSize;
	u16 execAttributes;

	execInfo->isDecrypted = 0;
	header = (const PspHeader *)execInfo->fileBase;
	magic = header->magic;
	if (magic == SCE_MAGIC)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if (((u32)header & 63) != 0)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if (magic != PSP_MAGIC)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if ((s32)header->modInfoOffset >= 0)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if (header->decryptMode == 0)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	attributes = header->compAttribute;
	if (attributes != 0) {
		if ((attributes & 0xE) != 0)
			return SCE_ERROR_KERNEL_UNSUPPORTED;
		if ((attributes & 0xF0FE) != 0)
			return SCE_ERROR_KERNEL_UNSUPPORTED;
		attributes &= 0xF00;
		if (attributes == 0)
			return SCE_ERROR_KERNEL_UNSUPPORTED;
		if (attributes != 0x200)
			return SCE_ERROR_KERNEL_UNSUPPORTED;
	}
	execInfo->isDecrypted = 1;
	imageSize = header->pspSize;
	moduleAttributes = header->modAttribute;
	execInfo->execSize = imageSize;
	execInfo->modInfoAttribute = moduleAttributes;
	decompressedSize = header->elfSize;
	execAttributes = header->compAttribute;
	execInfo->decSize = decompressedSize;
	execInfo->execAttribute = execAttributes;
	return 0;
}

int decryptBootModule(SceLoadCoreExecFileInfo *execInfo, SceSize *newSize)
{
	PspHeader *header;
	u32 apiType;
	u8 decryptMode;
	int result;
	int (*getLength)(u8 *file, SceSize size, SceSize *newSize);

	if ((execInfo->isDecrypted & 1) == 0)
		return 0;
	apiType = execInfo->apiType;
	header = (PspHeader *)execInfo->fileBase;
	if (apiType != 0)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	decryptMode = header->decryptMode;
	if (decryptMode != 0 && decryptMode != 2)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if (g_prepareGetLengthFunc == NULL || g_getLengthFunc == NULL ||
	    g_setMaskFunc == NULL)
		return SCE_ERROR_KERNEL_NO_DECRYPTOR;
	if (g_prepareGetLengthFunc((u8 *)header, execInfo->execSize) != 0)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if (compareBootModuleLatestSubType(header) == 0)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	result = compareBootModuleLatestSubType(header);
	getLength = g_getLengthFunc;
	if (result != 0) {
		result = g_setMaskFunc(0, (const u8 *)0xBFC00200);
		if (result != 0)
			return result;
		getLength = g_getLengthFunc;
	}
	result = getLength((u8 *)header, execInfo->execSize, newSize);
	/* Sony sets this bit after the callback regardless of its result. */
	execInfo->isDecrypted |= 2;
	return result;
}

SceSize getBootModuleSegmentSpan(const SceLoadCoreExecFileInfo *execInfo,
			       int enabled)
{
	const PspHeader *header;
	u32 minimum;
	u32 maximum = 0;
	int count;
	int index;

	if (enabled == 0)
		return 0;
	header = (const PspHeader *)execInfo->fileBase;
	if (header->magic == PSP_MAGIC) {
		count = (s8)header->nSegments;
		minimum = header->segAddress[0];
		for (index = 0; index < count; index++) {
			u32 address = header->segAddress[index];
			u32 end = address + header->segSize[index];

			if (maximum < end)
				maximum = end;
			if (address < minimum)
				minimum = address;
		}
	} else {
		const Elf32_Ehdr *elf = (const Elf32_Ehdr *)header;
		u32 programOffset = elf->e_phoff;
		const Elf32_Phdr *program;

		count = elf->e_phnum;
		program = (const Elf32_Phdr *)((const u8 *)elf + programOffset);
		/* Sony reads this even with no entries or a non-LOAD first entry. */
		minimum = program->p_vaddr;
		for (index = 0; index < count; index++, program++) {
			if (program->p_type == PT_LOAD) {
				u32 address = program->p_vaddr;
				u32 end = address + program->p_memsz;

				if (maximum < end)
					maximum = end;
				if (address < minimum)
					minimum = address;
			}
		}
	}
	return maximum - minimum;
}

int allocateBootModuleMemory(SceLoadCoreExecFileInfo *execInfo, SceSize size,
			     int enabled,
			     const SceLoadCoreBootServices *services)
{
	SceSize allocSize;
	SceUID blockId;

	if (enabled == 0) {
		g_maxAllocSize = services->initialMaxAllocSize;
		return 0;
	}
	if (services->allocPartitionMemory == NULL ||
	    services->getBlockHeadAddr == NULL ||
	    services->resizeMemoryBlock == NULL)
		return -1;
	allocSize = g_maxAllocSize;
	if (allocSize < size) {
		g_maxAllocSize = size;
		allocSize = size;
	}
	if (allocSize == 0)
		return -1;
	blockId = services->allocPartitionMemory(1, g_strLoaderCore, 0,
						 allocSize, 0);
	if (blockId < 0)
		return blockId;
	execInfo->decompressionMemId = blockId;
	execInfo->maxAllocSize = g_maxAllocSize;
	execInfo->topAddr = services->getBlockHeadAddr(blockId);
	return 0;
}

int isBootModuleKl4eCompressed(const SceLoadCoreExecFileInfo *execInfo)
{
	if ((execInfo->isDecrypted & 1) == 0)
		return 0;
	return (execInfo->execAttribute >> 9) & 1;
}

int decompressBootModule(SceLoadCoreExecFileInfo *execInfo)
{
	const s8 *file = (const s8 *)execInfo->fileBase;
	int index;
	int result;

	if (execInfo->topAddr == NULL)
		return -1;
	if (isBootModuleKl4eCompressed(execInfo) == 0) {
		execInfo->isCompressed = 0;
		return 0;
	}
	if ((execInfo->execAttribute & 0xF00) != 0x200)
		return SCE_ERROR_KERNEL_UNSUPPORTED;

	for (index = 0; index < 4; index++) {
		if (file[index] == (s8)g_strKL4E[index])
			continue;
		/* The alternate stream starts after a 128-byte prefix. */
		file += 128;
		for (index = 0; index < 4; index++) {
			if (file[index] != (s8)g_strKL4E[index])
				return SCE_ERROR_KERNEL_UNSUPPORTED;
		}
		break;
	}
	result = decompressKl4e(execInfo->topAddr, execInfo->decSize,
			       file + 4, NULL);
	if (result < 0)
		return result;
	if ((SceSize)result != execInfo->decSize)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	execInfo->isCompressed = 1;
	execInfo->fileBase = execInfo->topAddr;
	execInfo->isDecompressed = 1;
	return 0;
}

int checkBootElfSegments(const Elf32_Ehdr *elf,
			 SceLoadCoreExecFileInfo *execInfo)
{
	const Elf32_Phdr *program;
	const Elf32_Phdr *entry;
	u32 minimum;
	u32 maximum = 0;
	int count = elf->e_phnum;
	int index;

	program = (const Elf32_Phdr *)((const u8 *)elf + elf->e_phoff);
	entry = program;
	for (index = 0; index < count; index++, entry++) {
		u32 offset = entry->p_offset;

		if (offset == 0)
			continue;
		if (offset > 0x02000000 ||
		    offset + entry->p_filesz > 0x02000000)
			return SCE_ERROR_KERNEL_UNSUPPORTED;
	}
	while (count > 0 && program->p_type != PT_LOAD) {
		count--;
		program++;
	}
	if (count <= 0 || program->p_type != PT_LOAD)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if ((s32)program->p_paddr >= 0 || program->p_paddr == program->p_vaddr)
		return SCE_ERROR_KERNEL_UNSUPPORTED;

	minimum = program->p_vaddr;
	execInfo->moduleInfoOffset = program->p_paddr & 0x7FFFFFFF;
	/* Only the consecutive LOAD entries starting here contribute. */
	for (index = 0; index < count; index++, program++) {
		u32 address;
		u32 end;
		u32 alignment;

		if (program->p_type != PT_LOAD)
			break;
		address = program->p_vaddr;
		end = address + program->p_memsz;
		alignment = program->p_align;
		if (maximum < end)
			maximum = end;
		if (address < minimum)
			minimum = address;
		if (execInfo->maxSegAlign < alignment)
			execInfo->maxSegAlign = alignment;
	}
	execInfo->largestSegSize = maximum - minimum;
	return 0;
}

int checkBootElfSections(const Elf32_Ehdr *elf,
			 SceLoadCoreExecFileInfo *execInfo)
{
	int count = elf->e_shnum;
	u32 sectionOffset = elf->e_shoff;
	const Elf32_Shdr *sections;
	int index;

	sections = (const Elf32_Shdr *)((const u8 *)elf + sectionOffset);
	if (count != 0) {
		u32 stringIndex = elf->e_shstrndx;
		u32 nameLimit;

		if (stringIndex != 0 && stringIndex >= (u32)count)
			return SCE_ERROR_KERNEL_UNSUPPORTED;
		nameLimit = sections[stringIndex].sh_size;
		for (index = 0; index < count; index++) {
			const Elf32_Shdr *section = &sections[index];
			u32 offset;
			u32 end;

			if (nameLimit < section->sh_name) {
				Kprintf(g_fmtShNameLimit, index, count, nameLimit,
					section->sh_name);
				return SCE_ERROR_KERNEL_UNSUPPORTED;
			}
			offset = section->sh_offset;
			if (offset > 0x02000000) {
				Kprintf(g_fmtShoff, elf->e_shoff);
				Kprintf(g_fmtShName, index, elf->e_shnum,
					section->sh_name);
				Kprintf(g_fmtShType, index, section->sh_type);
				Kprintf(g_fmtShFlags, index, section->sh_flags);
				Kprintf(g_fmtShOffset, index, section->sh_offset);
				Kprintf(g_fmtShSize, index, section->sh_size);
				Kprintf(g_fmtShAddralign, index,
					section->sh_addralign);
				Kprintf(g_fmtShEntsize, index, section->sh_entsize);
				Kprintf(g_fmtShOffsetLimit, index, 0x02000000,
					section->sh_offset);
				return SCE_ERROR_KERNEL_UNSUPPORTED;
			}
			end = offset + section->sh_size;
			if (end > 0x02000000) {
				Kprintf(g_fmtShOverrun, index, 0x02000000, end);
				return SCE_ERROR_KERNEL_UNSUPPORTED;
			}
		}
	}
	execInfo->textSize = 0;
	execInfo->dataSize = 0;
	execInfo->bssSize = 0;
	if (sectionOffset == 0) {
		const Elf32_Phdr *program;

		count = elf->e_phnum;
		program = (const Elf32_Phdr *)((const u8 *)elf + elf->e_phoff);
		for (index = 0; index < count; index++, program++) {
			u32 size;

			if (program->p_type != PT_LOAD)
				continue;
			size = program->p_filesz;
			if ((program->p_flags & PF_X) != 0)
				execInfo->textSize += size;
			else
				execInfo->dataSize += size;
			if (size < program->p_memsz)
				execInfo->bssSize += program->p_memsz - size;
		}
	} else {
		for (index = 0; index < count; index++) {
			const Elf32_Shdr *section = &sections[index];

			if (section->sh_type == SHT_PROGBITS) {
				if (section->sh_flags == (SHF_ALLOC | SHF_EXECINSTR) ||
				    section->sh_flags == SHF_ALLOC)
					execInfo->textSize += section->sh_size;
				else if (section->sh_flags == (SHF_ALLOC | SHF_WRITE))
					execInfo->dataSize += section->sh_size;
			} else if (section->sh_type == SHT_NOBITS &&
				   section->sh_flags == (SHF_ALLOC | SHF_WRITE)) {
				execInfo->bssSize += section->sh_size;
			}
		}
	}
	return 0;
}

int checkBootElfModule(SceLoadCoreExecFileInfo *execInfo)
{
	const Elf32_Ehdr *elf = (const Elf32_Ehdr *)execInfo->fileBase;
	SceModuleInfo *module;
	u32 entry;
	u32 moduleOffset;
	void *exports;
	void *imports;
	u32 exportsEnd;
	u32 importsEnd;
	u16 attribute;
	u16 combinedAttribute;
	u32 decrypted;
	int result;

	if (elf->e_ident[EI_CLASS] != ELFCLASS32 ||
	    elf->e_ident[EI_DATA] != ELFDATA2LSB)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if (elf->e_type != 0xFFA0 || elf->e_machine != EM_MIPS)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	if (elf->e_phoff < sizeof(Elf32_Ehdr) ||
	    elf->e_shoff - 1 < sizeof(Elf32_Ehdr) - 1)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	result = checkBootElfSegments(elf, execInfo);
	if (result < 0)
		return result;
	result = checkBootElfSections(elf, execInfo);
	if (result < 0)
		return result;
	entry = elf->e_entry;
	moduleOffset = execInfo->moduleInfoOffset;
	execInfo->entryAddr = entry;
	if (moduleOffset == 0)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	module = (SceModuleInfo *)((const u8 *)elf + moduleOffset);
	execInfo->moduleInfo = (void *)module;
	attribute = module->modattribute;
	exports = module->ent_top;
	imports = module->stub_top;
	combinedAttribute = execInfo->modInfoAttribute;
	decrypted = execInfo->isDecrypted & 1;
	exportsEnd = (u32)module->ent_end;
	importsEnd = (u32)module->stub_end;
	combinedAttribute |= attribute & 0xE1FF;
	execInfo->exportsSize = exportsEnd - (u32)exports;
	execInfo->importsSize = importsEnd - (u32)imports;
	execInfo->exportsInfo = exports;
	execInfo->importsInfo = imports;
	execInfo->modInfoAttribute = combinedAttribute;
	if (decrypted != 0 && (combinedAttribute & 0x1E00) != 0x1000)
		return SCE_ERROR_KERNEL_UNSUPPORTED;
	return 0;
}

void moveBootSegmentWords(const u32 *source, u32 *destination, SceSize size)
{
	const u32 *end;
	SceSize bytes;

	if (source == destination)
		return;
	bytes = size & ~3U;
	end = (const u32 *)((u32)source + bytes);
	if ((u32)destination < (u32)source) {
		while ((u32)source < (u32)end)
			*destination++ = *source++;
	} else {
		destination = (u32 *)((u32)destination + bytes);
		while ((u32)source < (u32)end) {
			end--;
			destination--;
			*destination = *end;
		}
	}
}

int ApplyPspRelSection(const SceLoadCoreExecFileInfo *execInfo,
		      const SceLoadCoreBootSegments *segments,
		      const Elf32_Rel *relocations, int count)
{
	int processed = 0;

	while (processed < count) {
		u32 info = relocations->r_info;
		u32 type = (u8)info;
		int offsetSegment = (info >> 8) & 0xFF;
		int addressSegment = info >> 16;
		u32 offset;
		u8 *target;
		u32 base;
		u32 value;
		int advance = 1;

		if (offsetSegment >= (s32)execInfo->numSegments ||
		    addressSegment >= (s32)execInfo->numSegments)
			goto internalError;
		offset = relocations->r_offset;
		if (offset >= segments->fileSize[offsetSegment])
			goto internalError;
		target = (u8 *)((u32)segments->source[offsetSegment] + offset);
		base = (u32)segments->destination[addressSegment];
		switch (type) {
		case R_MIPS_NONE:
			break;
		case R_MIPS_16:
		case R_MIPS_LO16:
			__builtin_memcpy(&value, target, sizeof(value));
			value = (value & 0xFFFF0000) |
				((base + (u32)(s32)(s16)value) & 0xFFFF);
			__builtin_memcpy(target, &value, sizeof(value));
			break;
		case R_MIPS_32:
			__builtin_memcpy(&value, target, sizeof(value));
			value += base;
			__builtin_memcpy(target, &value, sizeof(value));
			break;
		case R_MIPS_26: {
			u32 address;

			__builtin_memcpy(&value, target, sizeof(value));
			address = (value & 0x03FFFFFF) << 2;
			address |= (offset + (u32)segments->destination[offsetSegment]) &
				   0xF0000000;
			address += base;
			value = (value & 0xFC000000) | ((address >> 2) & 0x03FFFFFF);
			__builtin_memcpy(target, &value, sizeof(value));
			break;
		}
		case R_MIPS_HI16: {
			const Elf32_Rel *low;
			u32 high;
			u32 sum;
			u32 upper;
			int segment;
			int index;

			__builtin_memcpy(&value, target, sizeof(value));
			high = value << 16;
			while (processed + advance < count &&
			       (u8)relocations[advance].r_info == R_MIPS_HI16)
				advance++;
			/* Sony reads the following record even when the HI16 run ends the list. */
			low = &relocations[advance];
			segment = (low->r_info >> 8) & 0xFF;
			target = (u8 *)((u32)segments->source[segment] + low->r_offset);
			__builtin_memcpy(&value, target, sizeof(value));
			sum = high + (u32)(s32)(s16)value + base;
			upper = (((sum >> 15) + 1) >> 1) & 0xFFFF;
			for (index = 0; index < advance; index++) {
				const Elf32_Rel *relocation = &relocations[index];

				segment = (relocation->r_info >> 8) & 0xFF;
				target = (u8 *)((u32)segments->source[segment] +
					       relocation->r_offset);
				__builtin_memcpy(&value, target, sizeof(value));
				value = (value & 0xFFFF0000) | upper;
				__builtin_memcpy(target, &value, sizeof(value));
			}
			break;
		}
		case R_MIPS_LITERAL:
			Kprintf(g_strStars);
			Kprintf(g_strCannotLoad);
			Kprintf(g_fmtBadRelocType, type);
			break;
		default:
			Kprintf(g_strStars);
			Kprintf(g_strCannotLoad);
			Kprintf(g_fmtBadRelocType, type);
			return SCE_ERROR_KERNEL_UNSUPPORTED;
		}
		processed += advance;
		relocations += advance;
	}
	return 0;

internalError:
	Kprintf(g_fmtInternalError, g_strApplyPspRelSection);
	return SCE_ERROR_ERROR;
}

int getRelocationSegmentBits(u32 value)
{
	u32 bits;

	for (bits = 1; bits <= 32; bits++) {
		if ((1U << (bits & 31)) >= value)
			return bits;
	}
	return SCE_ERROR_ERROR;
}

int isCompressedRelocationHigh16(u32 type)
{
	return type == 6;
}

int applyCompressedBootRelocations(const SceLoadCoreExecFileInfo *execInfo,
				   const SceLoadCoreBootSegments *segments,
				   u32 numSegments, const u8 *data,
				   SceSize size, u32 flags)
{
	const SceLoadCoreCompressedRelocationHeader *header;
	const u8 *cursor;
	u32 end = (u32)data + size;
	u32 offsetSegment = 0;
	u32 offset = 0;
	u16 lowAddend = 0;
	u32 flagBits;
	u32 typeBits;
	u32 flagCount;
	u32 typeCount;
	u32 previousType;
	u32 index;
	u8 value;
	int segmentBits;

	segmentBits = getRelocationSegmentBits(numSegments);
	if (segmentBits < 0)
		return segmentBits;
	header = (const SceLoadCoreCompressedRelocationHeader *)data;
	if (header->formatVersion != 0)
		return SCE_ERROR_ERROR;
	cursor = (const u8 *)(header + 1);
	value = *cursor;
	flagCount = value;
	typeBits = header->typeBits;
	flagBits = header->flagBits;
	/* Each table includes its length byte as entry zero. */
	for (index = 0; index < flagCount; index++) {
		g_relocFlags[index] = value;
		cursor++;
		value = *cursor;
	}
	typeCount = value;
	for (index = 0; index < typeCount; index++) {
		g_relocTypes[index] = value;
		cursor++;
		if (index + 1 < typeCount)
			value = *cursor;
	}
	previousType = typeCount;
	while ((u32)cursor < end) {
		u32 token = *(const u16 *)cursor;
		const u8 *next = cursor + sizeof(u16);
		u32 flagIndex = GetRelocationBits(token, 0, flagBits);
		u32 control;
		u32 typeIndex;
		u32 type;
		u32 addressSegment;
		u32 base;
		u32 word;
		u8 *target;

		if (flagIndex >= flagCount)
			return SCE_ERROR_ERROR;
		control = g_relocFlags[flagIndex];
		if ((control & 1) == 0) {
			offsetSegment = GetRelocationBits(token, flagBits, segmentBits);
			if (offsetSegment >= numSegments)
				return SCE_ERROR_ERROR;
			offset = (u32)((s32)token >> ((segmentBits + flagBits) & 31));
			if ((control & 6) != 0) {
				if ((control & 6) != 4)
					return SCE_ERROR_ERROR;
				__builtin_memcpy(&offset, next, sizeof(offset));
				next += sizeof(offset);
			}
			cursor = next;
			continue;
		}
		typeIndex = GetRelocationBits(token, segmentBits + flagBits, typeBits);
		if (typeIndex >= typeCount)
			return SCE_ERROR_ERROR;
		addressSegment = GetRelocationBits(token, flagBits, segmentBits);
		if (offsetSegment >= numSegments || addressSegment >= numSegments)
			return SCE_ERROR_ERROR;
		type = g_relocTypes[typeIndex];
		switch (control & 6) {
		case 0:
			offset += (u32)ShiftRelocationHalfword(token,
					 typeBits + segmentBits + flagBits);
			break;
		case 2:
			offset += MergeRelocationHalfwords(token, *(const u16 *)next,
					 typeBits + segmentBits + flagBits);
			next += sizeof(u16);
			break;
		case 4:
			__builtin_memcpy(&offset, next, sizeof(offset));
			next += sizeof(offset);
			break;
		default:
			return SCE_ERROR_ERROR;
		}
		if (offset >= segments->fileSize[offsetSegment])
			return SCE_ERROR_ERROR;
		switch (control & 0x38) {
		case 0:
			lowAddend = 0;
			break;
		case 8:
			if (isCompressedRelocationHigh16(previousType) == 0)
				lowAddend = 0;
			break;
		case 16:
			lowAddend = *(const u16 *)next;
			next += sizeof(u16);
			break;
		default:
			return SCE_ERROR_ERROR;
		}
		target = (u8 *)((u32)segments->source[offsetSegment] + offset);
		base = (u32)segments->destination[addressSegment];
		switch (type) {
		case 0:
			break;
		case 1:
		case 2:
		case 3: {
			u32 address;
			u32 opcode;

			__builtin_memcpy(&word, target, sizeof(word));
			address = (word & 0x03FFFFFF) << 2;
			address |= (offset + (u32)segments->destination[offsetSegment]) &
				   0xF0000000;
			address += base;
			opcode = word & 0xFC000000;
			if (type == 2)
				opcode = 0x08000000;
			else if (type == 3)
				opcode = 0x0C000000;
			word = opcode | ((address >> 2) & 0x03FFFFFF);
			__builtin_memcpy(target, &word, sizeof(word));
			break;
		}
		case 4:
		case 7:
			__builtin_memcpy(&word, target, sizeof(word));
			word = (word & 0xFFFF0000) | ((base + (u32)(s32)(s16)word) & 0xFFFF);
			__builtin_memcpy(target, &word, sizeof(word));
			break;
		case 5:
			__builtin_memcpy(&word, target, sizeof(word));
			word += base;
			__builtin_memcpy(target, &word, sizeof(word));
			break;
		case 6: {
			u32 sum;
			u32 upper;

			__builtin_memcpy(&word, target, sizeof(word));
			sum = (word << 16) + (u32)(s32)(s16)lowAddend + base;
			upper = (((sum >> 15) + 1) >> 1) & 0xFFFF;
			word = (word & 0xFFFF0000) | upper;
			__builtin_memcpy(target, &word, sizeof(word));
			break;
		}
		default:
			return SCE_ERROR_ERROR;
		}
		previousType = type;
		cursor = next;
	}
	return 0;
}

int prepareBootSegments(SceLoadCoreExecFileInfo *execInfo,
			SceLoadCoreBootSegments *segments)
{
	u8 *file = execInfo->fileBase;
	const Elf32_Ehdr *elf = (const Elf32_Ehdr *)file;
	const Elf32_Phdr *first = (const Elf32_Phdr *)(file + elf->e_phoff);
	const Elf32_Phdr *relocation;
	int remaining = elf->e_phnum;
	int count = 1;
	int result;
	int i;

	while (remaining > 0 && first->p_type != PT_LOAD) {
		--remaining;
		++first;
	}
	segments->destination[0] = execInfo->topAddr;
	segments->source[0] = file + first->p_offset;
	segments->fileSize[0] = first->p_filesz;
	segments->alignment[0] = first->p_align;
	if (remaining <= 1)
		return SCE_ERROR_KERNEL_UNSUPPORTED;

	while (first[count].p_type == PT_LOAD) {
		const Elf32_Phdr *program = &first[count];

		segments->destination[count] = execInfo->topAddr + program->p_vaddr;
		segments->source[count] = file + program->p_offset;
		segments->fileSize[count] = program->p_filesz;
		segments->alignment[count] = program->p_align;
		if (++count >= remaining)
			return SCE_ERROR_KERNEL_UNSUPPORTED;
	}
	relocation = &first[count];
	if ((u32)(relocation->p_type - 0x700000A0) >= 2)
		return SCE_ERROR_KERNEL_UNSUPPORTED;

	execInfo->numSegments = count;
	execInfo->segmentAddr[0] = (u32)segments->destination[0];
	execInfo->segmentSize[0] = first->p_memsz;
	execInfo->segmentAlign[0] = segments->alignment[0];
	for (i = 1; i < 4; ++i) {
		if (i < count) {
			execInfo->segmentAddr[i] = (u32)segments->destination[i];
			execInfo->segmentSize[i] = first[i].p_memsz;
			execInfo->segmentAlign[i] = segments->alignment[i];
		} else {
			execInfo->segmentAlign[i] = 0;
			execInfo->segmentAddr[i] = 0;
			execInfo->segmentSize[i] = 0;
		}
	}
	execInfo->entryAddr += (u32)segments->destination[0];
	execInfo->exportsInfo = (void *)((u32)execInfo->exportsInfo +
					(u32)segments->destination[0]);
	execInfo->importsInfo = (void *)((u32)execInfo->importsInfo +
					(u32)segments->destination[0]);
	if (relocation->p_type == 0x700000A0) {
		result = ApplyPspRelSection(execInfo, segments,
			(const Elf32_Rel *)(file + relocation->p_offset),
			relocation->p_filesz >> 3);
	} else if (relocation->p_type == 0x700000A1) {
		result = applyCompressedBootRelocations(execInfo, segments, count,
			file + relocation->p_offset, relocation->p_filesz,
			relocation->p_flags);
	} else {
		return SCE_ERROR_ERROR;
	}
	if (result < 0)
		return result;
	execInfo->moduleInfo = (void *)((u32)execInfo->topAddr - first->p_offset +
					execInfo->moduleInfoOffset);
	return 0;
}

void clearBootWords(u32 *destination, u32 count)
{
	while (count != 0) {
		--count;
		*destination++ = 0;
	}
}

void clearBootMemory(u8 *destination, SceSize size)
{
	while ((u32)destination & 3) {
		*destination++ = 0;
		--size;
	}
	clearBootWords((u32 *)destination, (size + 3) >> 2);
}

void copyBootSegments(const SceLoadCoreExecFileInfo *execInfo,
		      const SceLoadCoreBootSegments *segments)
{
	const Elf32_Ehdr *elf = (const Elf32_Ehdr *)execInfo->fileBase;
	const Elf32_Phdr *programs = (const Elf32_Phdr *)
		(execInfo->fileBase + elf->e_phoff);
	const Elf32_Phdr *last = (const Elf32_Phdr *)((u32)programs +
		(execInfo->numSegments << 5) - sizeof(*programs));
	u8 *bss = NULL;
	SceSize bssSize = 0;
	u32 i;

	if (last->p_filesz < last->p_memsz) {
		bssSize = last->p_memsz - last->p_filesz;
		bss = execInfo->topAddr + last->p_vaddr + last->p_filesz;
	}
	for (i = 0; i < execInfo->numSegments; ++i)
		moveBootSegmentWords((const u32 *)segments->source[i],
			(u32 *)segments->destination[i], segments->fileSize[i]);
	if (bssSize != 0)
		clearBootMemory(bss, bssSize);
}

int resizeBootModuleMemory(SceLoadCoreExecFileInfo *execInfo, int enabled,
			   const SceLoadCoreBootServices *services)
{
	s32 delta;
	int result;

	if (enabled == 0 || execInfo->isCompressed == 0)
		return 0;
	delta = (s32)(execInfo->largestSegSize - execInfo->maxAllocSize);
	if (delta > 0)
		delta = (delta + 255) & ~255;
	result = services->resizeMemoryBlock(execInfo->decompressionMemId,
					     0, delta);
	if (result < 0)
		return result;
	execInfo->maxAllocSize = execInfo->largestSegSize;
	return 0;
}

int loadBootExecutable(u8 *file, SceLoadCoreExecFileInfo *execInfo, int mode,
		       const SceLoadCoreBootServices *services)
{
	SceLoadCoreBootSegments segments;
	SceSize newSize;
	SceSize span;
	int result;

	if (file == NULL || execInfo == NULL || services == NULL ||
	    mode < 0 || mode >= 2)
		return (int)0x800200D2;
	execInfo->fileBase = file;
	result = checkBootModuleHeader(execInfo);
	if (result < 0)
		return result;
	span = getBootModuleSegmentSpan(execInfo, mode);
	result = decryptBootModule(execInfo, &newSize);
	if (result < 0)
		return result;
	result = allocateBootModuleMemory(execInfo, span, mode, services);
	if (result < 0)
		return result;
	result = decompressBootModule(execInfo);
	if (result < 0)
		return result;
	result = checkBootElfModule(execInfo);
	if (result < 0)
		return result;
	result = prepareBootSegments(execInfo, &segments);
	if (result < 0)
		return result;
	copyBootSegments(execInfo, &segments);
	result = resizeBootModuleMemory(execInfo, mode, services);
	return result < 0 ? result : 0;
}
