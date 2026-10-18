/* Private segment-interface registration records. */

#pragma once

#include <coredump/elf.h>
#include <scetypes.h>

typedef struct SegIf SegIf;
typedef struct SegIfMethod SegIfMethod;

typedef struct SegIfMethodPair {
	struct SegIf *segment;
	const struct SegIfMethod *methods;
	SceUInt32 segmentType;
} SegIfMethodPair;

struct SegIfMethod {
	int (*init)(SegIf *segment);
	int (*calcSize)(SegIf *segment);
	int (*writePhdr)(SegIf *segment, SceOff fileOffset);
	int (*writeSegs)(SegIf *segment);
};

struct SegIf {
	const SegIfMethod *methods;
	SceUInt32 numPhdr;
	SceSize dataSize;
	SceSize writtenSize;
	SceUInt32 segmentType;
};

typedef struct FileSegIf {
	SegIf base;
	SceUInt32 numFiles;
} FileSegIf;

_Static_assert(sizeof(FileSegIf) == 24, "file segment interface size");

typedef struct ProcessSegment {
	SegIf segment;
	char path[256];
	SceSize pathSize;
} ProcessSegment;

_Static_assert(sizeof(ProcessSegment) == 280, "process segment size");
_Static_assert(__builtin_offsetof(ProcessSegment, path) == 20, "process path");
_Static_assert(__builtin_offsetof(ProcessSegment, pathSize) == 276,
	"process path size");

typedef struct GpuSegIf {
	SegIf base;
	SceSize gpuInfoSize;
} GpuSegIf;

_Static_assert(sizeof(GpuSegIf) == 24, "GPU segment interface size");

typedef struct SummarySegIf {
	SegIf base;
	const SegIfMethodPair *pairs;
	SceUInt32 numSegments;
} SummarySegIf;

_Static_assert(sizeof(SummarySegIf) == 28, "summary segment interface size");

typedef struct CoredumpNoteHeader {
	SceUInt8 nameSize[4];	/* little-endian, written byte by byte */
	SceUInt32 descSize;
	SceUInt32 type;
	char name[16];
	SceUInt32 count;
} CoredumpNoteHeader;

typedef struct CoredumpTtyNote {
	CoredumpNoteHeader header;
	SceUInt32 reserved;
	SceSize logSize;
} CoredumpTtyNote;

_Static_assert(sizeof(CoredumpTtyNote) == 40, "TTY note size");

_Static_assert(sizeof(SegIfMethodPair) == 12, "segment method pair size");
_Static_assert(sizeof(SegIfMethod) == 16, "segment methods size");
_Static_assert(sizeof(SegIf) == 20, "segment interface size");
_Static_assert(sizeof(CoredumpNoteHeader) == 32, "coredump note header size");

int segWriteSegs(struct SegIf *segment);
int segInit(SegIf *segment);
int segCalcSize(SegIf *segment);
void segGetSize(SegIf *segment, SceUInt32 *numPhdr, SceSize *dataSize);
int segInitBase(SegIf *segment);
int segWritePhdr(SegIf *segment, SceOff fileOffset);
int segWritePtNoteHdr(SegIf *segment, SceOff fileOffset);
int segWritePaddingData(SegIf *segment, SceSize writtenSize);
int utilsMakeNoteHeader(CoredumpNoteHeader *header, const char *name,
	SceUInt32 type, SceUInt32 count, SceSize noteSize);
void utilsElf32MakeEhdr(Elf32_Ehdr *header, SceUInt32 numPhdr);
void utilsElf32MakePhent(Elf32_Phdr *header, Elf32_Word type, Elf32_Off offset,
	Elf32_Addr vaddr, Elf32_Addr paddr, Elf32_Word filesz, Elf32_Word memsz,
	Elf32_Word flags, Elf32_Word align);
void utilsElf32MakePtnote(Elf32_Phdr *header, SceUInt32 fileOffset,
	SceSize dataSize);
