/*
 * SceGpuEs4CoreDump: on a GPU lockup or page fault, SceGpuEs4 calls back
 * here. The module captures the SGX543MP4 registers (the master unit and
 * each core), writes an analysis of the crash to the console and into the
 * dump, and saves the dump (and, if a PA trace runs, the trace as a .pmon
 * file) to host0:, sd0: or ux0:data/.
 */

#pragma once

#include <psp2kern/types.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sce/des.h>
#include "sce/sgxmkif_sce.h"

/* what a captured value holds before the capture */
#define GPU_COREDUMP_POISON		0x5ce0beef

#define GPU_COREDUMP_CORES		4
#define GPU_COREDUMP_DATA_MASTERS	8
#define GPU_COREDUMP_CORE_WINDOW	0x4000	/* core n at (n + 2) * 0x4000 */
#define GPU_COREDUMP_PATH_MAX		0x400

/* a register to capture */
typedef struct GpuCoreDumpReg {
	SceUInt32 offset;
	const char *name;
	SceUInt32 kind;				/* 1 or 2, the same for a whole table; never read */
} GpuCoreDumpReg;

typedef struct GpuCoreDumpMasterValue {
	SceUInt32 offset;
	SceUInt32 value;
} GpuCoreDumpMasterValue;

typedef struct GpuCoreDumpCoreValue {
	SceUInt32 offset;
	SceUInt32 value[GPU_COREDUMP_CORES];
} GpuCoreDumpCoreValue;

typedef struct GpuCoreDumpMasterList {
	SceUInt32 count;
	const GpuCoreDumpReg *regs;
	GpuCoreDumpMasterValue *values;
	SceUInt32 *dumped;
} GpuCoreDumpMasterList;

typedef struct GpuCoreDumpCoreList {
	SceUInt32 count;
	const GpuCoreDumpReg *regs;
	GpuCoreDumpCoreValue *values;
	SceUInt32 *dumped;
} GpuCoreDumpCoreList;

/* a dump segment: the captured values with their count first */
#define GPU_COREDUMP_MASTER_DUMP(n)	struct { SceUInt32 count; GpuCoreDumpMasterValue value[n]; }
#define GPU_COREDUMP_CORE_DUMP(n)	struct { SceUInt32 count; GpuCoreDumpCoreValue value[n]; }

/* the 0x80 values read through the indexed debug port (0x90/0x94 select, 0x70 data) */
#define GPU_COREDUMP_INDEXED		0x80
typedef GPU_COREDUMP_CORE_DUMP(GPU_COREDUMP_INDEXED) GpuCoreDumpIndexed;

/* a process a data master worked for */
typedef struct GpuCoreDumpProcess {
	SceInt64 pid;				/* -1: unknown */
	char name[0x18];
} GpuCoreDumpProcess;

typedef struct GpuCoreDumpHeader {
	SceUInt32 version;			/* 0x20002 */
	SceUInt32 sdkVersion;
	SceUInt32 build;			/* gpuCoreDumpBuild */
	SceUInt32 socRevision;
} GpuCoreDumpHeader;

/* the dump file's segments (DES-encrypted when a key is given) */
typedef struct GpuCoreDumpSegment {
	const void *data;
	SceSize size;
	SceUInt32 type;				/* 0x8130400n */
	const SceUInt8 *iv;			/* 8 bytes, NULL: plain */
	SceUInt64 key;
} GpuCoreDumpSegment;

#define GPU_COREDUMP_SEGMENTS		9

/* text output: into a buffer, or to tty0: */
typedef struct GpuCoreDumpLog {
	char *cur;				/* NULL: the tty */
	char *end;
	SceUID pid;				/* the tty's process, 0: none */
	SceUID fd;
} GpuCoreDumpLog;

/* the crash analysis */
typedef struct GpuCoreDumpDataMaster {
	SceInt64 pid;				/* < 0: none */
	const char *name;
	SceUInt32 reserved;
} GpuCoreDumpDataMaster;

typedef struct GpuCoreDumpAnalysis {
	SceUInt32 coreFlags;			/* GPU_COREDUMP_CORE_* of the core being analysed */
	SceUInt32 flags;			/* GPU_COREDUMP_SUMMARY_* */
	SceUID pid;				/* the faulting process, -1: unknown */
	GpuCoreDumpLog *log;
	const char *prefix;
	SceUInt32 reserved;
	GpuCoreDumpDataMaster dm[GPU_COREDUMP_DATA_MASTERS];
} GpuCoreDumpAnalysis;

/* GpuCoreDumpAnalysis.flags: what the summary says */
#define GPU_COREDUMP_SUMMARY_BANNER	0x01
#define GPU_COREDUMP_SUMMARY_PARTIAL	0x02
#define GPU_COREDUMP_SUMMARY_PAGE_FAULT	0x04
#define GPU_COREDUMP_SUMMARY_UNKNOWN	0x40

/* GpuCoreDumpAnalysis.coreFlags: bits 0-1 survive from core to core */
#define GPU_COREDUMP_CORE_KEEP		0x03
#define GPU_COREDUMP_CORE_VERTEX	0x08
#define GPU_COREDUMP_CORE_FRAGMENT	0x10

/* a condition on a register: sets flags (and prints) when it holds */
typedef struct GpuCoreDumpRule {
	SceUInt32 source;			/* 0: register, 1: indexed */
	SceUInt32 cond;				/* 1: all mask bits set, 2: not all; 0 ends */
	SceUInt32 offset;
	SceUInt32 mask;
	const char *message;			/* NULL: silent */
	SceUInt32 flags;
	SceUInt32 enable;			/* the analysis mask bits it runs for */
} GpuCoreDumpRule;

/* a field to print when the core flags hold */
typedef struct GpuCoreDumpField {
	SceUInt32 require;			/* GpuCoreDumpAnalysis.coreFlags bits */
	SceUInt32 source;			/* 0: register, 1: indexed */
	SceUInt32 offset;
	SceUInt32 mask;
	SceUInt32 shift;
	const char *name;			/* NULL ends */
	void (*print)(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
} GpuCoreDumpField;

typedef struct GpuCoreDumpMessage {
	SceUInt32 flag;				/* 0 ends */
	const char *message;
} GpuCoreDumpMessage;

/* the units a page fault names */
typedef struct GpuCoreDumpSubunit {
	SceUInt32 id;
	const char *name;			/* NULL ends */
} GpuCoreDumpSubunit;

typedef struct GpuCoreDumpRequester {
	SceUInt32 flags;			/* for GpuCoreDumpAnalysis.coreFlags */
	const GpuCoreDumpSubunit *subunits;
	const char *name;			/* NULL ends */
} GpuCoreDumpRequester;

typedef struct GpuCoreDumpRange {
	SceUInt32 base;
	SceSize size;
	const char *name;			/* NULL ends */
} GpuCoreDumpRange;

/* the writer of the dump file (gpucoredump.c) */
typedef struct GpuCoreDumpWriter {
	DesContext cipher;
	SceUInt8 iv[8];
	SceUID fd;
	SceSize blockLength;
	SceSize length;
	int encrypt;
	int error;
	SceUInt32 reserved;
	SceUInt8 block[8];
	SceUInt8 buf[0x200];
} GpuCoreDumpWriter;

/* the SGX registers, through PVRSRV_SGXDEV_INFO.pvRegsBaseKM */
#define GPU_REG(regs, offset)		(*(volatile SceUInt32 *)((SceUInt8 *)*(regs) + (offset)))
#define GPU_CORE_REG(regs, core, offset) \
	GPU_REG(regs, (offset) + ((core) + 2) * GPU_COREDUMP_CORE_WINDOW)

/* EUR_CR_MASTER_CORE (sgxmpdefs.h): the number of enabled cores, less one */
#define GPU_COREDUMP_MASTER_CORE		0x4000
#define GPU_COREDUMP_MASTER_CORE_ENABLE_MASK	0x3

/* each core's debug bus, read through a select pair (not in the IMG headers) */
#define GPU_COREDUMP_DEBUG_DATA		0x70
#define GPU_COREDUMP_DEBUG_ENABLE	0x78
#define GPU_COREDUMP_DEBUG_SELECT0	0x90
#define GPU_COREDUMP_DEBUG_SELECT1	0x94

/* gpucoredump_tables.c */
extern GpuCoreDumpMasterList gpuCoreDumpMasterRegs;
extern GpuCoreDumpCoreList gpuCoreDumpCoreRegs;
extern GpuCoreDumpCoreList gpuCoreDumpCoreDebugRegs;
extern GpuCoreDumpMasterList gpuCoreDumpMasterDebugRegs;
extern const SceUInt8 gpuCoreDumpIvA[8];
extern const SceUInt8 gpuCoreDumpIvB[8];
extern const GpuCoreDumpField gpuCoreDumpFields[];
extern const GpuCoreDumpMessage gpuCoreDumpMessages[];
extern const GpuCoreDumpRule gpuCoreDumpMasterRules[];
extern const GpuCoreDumpRule gpuCoreDumpCoreRules[];
extern const GpuCoreDumpSegment gpuCoreDumpSegments[GPU_COREDUMP_SEGMENTS];
extern const GpuCoreDumpRequester gpuCoreDumpMasterRequesters[];
extern const GpuCoreDumpRequester gpuCoreDumpCoreRequesters[];
extern const char *const gpuCoreDumpPageFaultTypes[3];
extern const SceUInt32 gpuCoreDumpMasterDmRegs[GPU_COREDUMP_DATA_MASTERS];
extern const SceUInt32 gpuCoreDumpCoreDmRegs[GPU_COREDUMP_DATA_MASTERS];
extern const GpuCoreDumpRange gpuCoreDumpRanges[];
extern const SceUInt32 gpuCoreDumpBuild;

/* gpucoredump.c: the storage the tables point at */
extern SceUInt32 gpuCoreDumpMaxNameLength;
extern SceUInt32 gpuCoreDumpRenderState[4];
extern GpuCoreDumpIndexed gpuCoreDumpIndexed;
extern GpuCoreDumpProcess gpuCoreDumpProcesses[GPU_COREDUMP_DATA_MASTERS];
extern char gpuCoreDumpText[0x1000];
extern SceKernelModuleInfo gpuCoreDumpModuleInfo;
extern GpuCoreDumpHeader gpuCoreDumpHeader;

void gpuCoreDumpPrintString(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintSamples(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintValue(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintTile(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintTileSwapped(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintUnsigned(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintUssePipe3(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintUssePipe2(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintUssePipe1(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintUssePipe0(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);
void gpuCoreDumpPrintDrawNumber(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value);

/* across the files */
void gpuCoreDumpResetMaster(GpuCoreDumpMasterList *list);
void gpuCoreDumpResetCore(GpuCoreDumpCoreList *list);
int gpuCoreDumpGetHostPath(char *path);
SceUID gpuCoreDumpOpen(const char *device, const char *dir, const char *sub, const SceDateTime *time,
			      const char *ext, char *path);
int gpuCoreDumpSave(const char *device, const char *dir, const char *sub, const SceDateTime *time,
			   char *path);
int gpuCoreDumpWriteTrace(SceUID fd, const SceUInt8 *data, SceSize size, GpuCoreDumpLog *log);
int gpuCoreDumpCoreValue(int core, SceUInt32 offset, SceUInt32 *value);
void gpuCoreDumpCapture(PVRSRV_SGXDEV_INFO *devInfo, int dumpTrace);
int gpuCoreDumpIndexedValue(SceUInt32 core, SceUInt32 index, SceUInt32 *value);
int gpuCoreDumpMasterValue(SceUInt32 offset, SceUInt32 *value);
SceUInt32 *gpuCoreDumpGetRenderState(void);
const char *gpuCoreDumpRangeName(SceUInt32 address);
void gpuCoreDumpAnalyse(PVRSRV_SGXDEV_INFO *devInfo, int cores, SceUInt32 enable,
			       GpuCoreDumpAnalysis *analysis);
int gpuCoreDumpWriteSegments(SceUID fd, const GpuCoreDumpSegment *seg, int count);
void gpuCoreDumpLogOpen(GpuCoreDumpLog *log, SceUID pid);
void gpuCoreDumpLogBuffer(GpuCoreDumpLog *log, char *buf, char *end);
void gpuCoreDumpLogClose(GpuCoreDumpLog *log);
void gpuCoreDumpLogPrintf(GpuCoreDumpLog *log, const char *fmt, ...);
