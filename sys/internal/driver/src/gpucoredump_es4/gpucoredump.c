/*
 * SceGpuEs4CoreDump (Sony's file names unknown; built at -Os): see
 * gpucoredump.h. SceGpuEs4 calls gpuCoreDumpOnLockup when the GPU stops,
 * and gpuCoreDumpStopTrace before that to end a running PA trace. This
 * file captures the registers and saves the dump and the trace.
 */

#include <fw.h>
#include <psp2kern/types.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/dipsw.h>
#include <string.h>
#include "gpucoredump.h"
#include "sce/gpucoredump_es4_imports.h"

#define GPU_COREDUMP_NAME		"SceGpuCoreDump"

/* the .pmon file: two headers, then the trace, then tagged records */
#define GPU_COREDUMP_PMON_MAGIC		0x4e4f4d50	/* "PMON" */
#define GPU_COREDUMP_PMON_CLOCK_MAGIC	0x4e4f4d51	/* "QMON" */
#define GPU_COREDUMP_PMON_HEADER_SIZE	0xe0
#define GPU_COREDUMP_TAG_END		0x1c2b01
#define GPU_COREDUMP_TAG_GPU		0x1c2b02
#define GPU_COREDUMP_TAG_GPU_INFO	0x1c2b03
#define GPU_COREDUMP_TAG_VERSION	0x1c2b04
#define GPU_COREDUMP_TAG_TIME		0x1c2b05
#define GPU_COREDUMP_TAG_CORES		0x1c2b06
#define GPU_COREDUMP_TAG_COUNTERS	0x1c2b08
#define GPU_COREDUMP_TAG_START		0x1c2b00

/* a tagged record of the .pmon file */
typedef struct GpuCoreDumpTag {
	SceUInt32 tag : 24;
	SceUInt32 flags : 8;			/* 0x80 */
	SceUInt32 size;				/* with this header */
} GpuCoreDumpTag;

typedef struct GpuCoreDumpPmonHeader {
	SceUInt32 magic;			/* GPU_COREDUMP_PMON_MAGIC */
	SceUInt32 size;
	SceUInt32 traceSize;			/* the PA buffer size */
	SceUInt32 version;			/* 0x10003 */
	SceUInt32 state;			/* bit 3: the buffer wrapped */
	SceUInt32 position;
	SceUInt32 traceBuffers;			/* 1: the primary buffer */
	SceUInt32 traceControl;			/* PamgrPrimaryTraceConfig.control */
	SceUInt32 base;				/* 0xc0000000 */
	SceUInt32 bufferSize;			/* PamgrPrimaryTraceConfig.size */
	SceUInt32 bufferLimit;			/* PamgrPrimaryTraceConfig.limit */
	struct PamgrGpuTraceConfig gpuTrace;	/* control: 3 */
	SceUInt32 counters[4][GPU_COREDUMP_CORES];	/* per core: 0x90, 0x94, 0x98, 0x9c */
	SceUInt32 reserved[0xb];
} GpuCoreDumpPmonHeader;

typedef struct GpuCoreDumpPmonClock {
	SceUInt32 magic;			/* GPU_COREDUMP_PMON_CLOCK_MAGIC */
	SceUInt32 size;
	SceUInt32 sdkVersion;
	SceUInt32 hz;
} GpuCoreDumpPmonClock;

typedef struct GpuCoreDumpPmonVersion {
	SceUInt32 build;			/* PVRVERSION_BUILD: 5029 */
	SceUInt16 major;
	SceUInt16 minor;
	SceUInt16 revision;
	SceUInt16 reserved;
} GpuCoreDumpPmonVersion;

typedef struct GpuCoreDumpPmonTime {
	SceUInt16 year;
	SceUInt16 month;
	SceUInt16 day;
	SceUInt16 hour;
	SceUInt16 minute;
	SceUInt16 second;
} GpuCoreDumpPmonTime;

SceUInt32 gpuCoreDumpMaxNameLength;
SceUInt32 gpuCoreDumpRenderState[4];
GpuCoreDumpIndexed gpuCoreDumpIndexed;
GpuCoreDumpProcess gpuCoreDumpProcesses[GPU_COREDUMP_DATA_MASTERS];
char gpuCoreDumpText[0x1000];
SceKernelModuleInfo gpuCoreDumpModuleInfo;
GpuCoreDumpHeader gpuCoreDumpHeader;

static int s_traceKey = -1;

/* SceGpuEs4: ends a running PA trace before the dump, 1 if one was */
static int gpuCoreDumpStopTrace(void)
{
	int key, stopped;

	key = ScePamgrForDriver_0D04B52A(0x7a2077777a207888ULL);
	s_traceKey = key;
	if (key < 0 || !(ScePamgrForDriver_3ACF4316() & 1) || ScePamgrForDriver_88462719(key) != 0) {
		stopped = 0;
	} else {
		ksceKernelPrintf("%s: An active PMON trace has been stopped.\n",
				 gpuCoreDumpModuleInfo.module_name);
		stopped = 1;
	}
	if (s_traceKey >= 0) {
		sceKernelPaUnregister(s_traceKey);
		s_traceKey = -1;
	}
	return stopped;
}

static void gpuCoreDumpCaptureMaster(IMG_VOID **regs, GpuCoreDumpMasterList *list)
{
	SceUInt32 i, offset;

	*list->dumped = list->count;
	for (i = 0; i < list->count; i++) {
		offset = list->regs[i].offset;
		list->values[i].offset = offset;
		list->values[i].value = GPU_REG(regs, offset);
	}
}

static void gpuCoreDumpCaptureCores(IMG_VOID **regs, GpuCoreDumpCoreList *list, int cores)
{
	SceUInt32 i, offset;
	int core;

	*list->dumped = list->count;
	for (i = 0; i < list->count; i++) {
		offset = list->regs[i].offset;
		list->values[i].offset = offset;
		for (core = 0; core != cores; core++)
			list->values[i].value[core] = GPU_CORE_REG(regs, core, offset);
	}
}

static void gpuCoreDumpCoreNameLength(GpuCoreDumpCoreList *list)
{
	const char *name, *p;
	SceUInt32 i;
	int len;

	for (i = 0; i < list->count; i++) {
		name = list->regs[i].name;
		p = name;
		do
			len = p - name;
		while (*p++ != '\0');
		gpuCoreDumpMaxNameLength = (SceUInt32)len < gpuCoreDumpMaxNameLength ? gpuCoreDumpMaxNameLength : len;
	}
}

static void gpuCoreDumpMasterNameLength(GpuCoreDumpMasterList *list)
{
	const char *name, *p;
	SceUInt32 i;
	int len;

	for (i = 0; i < list->count; i++) {
		name = list->regs[i].name;
		p = name;
		do
			len = p - name;
		while (*p++ != '\0');
		gpuCoreDumpMaxNameLength = (SceUInt32)len < gpuCoreDumpMaxNameLength ? gpuCoreDumpMaxNameLength : len;
	}
}

static int gpuCoreDumpWriteTag(SceUID fd, SceUInt32 tag, const void *data, SceSize size)
{
	GpuCoreDumpTag hdr;

	hdr.tag = tag;
	hdr.flags = 0x80;
	hdr.size = size + sizeof(hdr);
	if (ksceIoWrite(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		return -1;
	if (data == NULL || size == 0)
		return 0;
	return ksceIoWrite(fd, data, size) == (int)size ? 0 : -1;
}

/* a core register as captured */
int gpuCoreDumpCoreValue(int core, SceUInt32 offset, SceUInt32 *value)
{
	const GpuCoreDumpCoreValue *v;
	SceUInt32 i;

	v = gpuCoreDumpCoreDebugRegs.values;
	for (i = 0; i != gpuCoreDumpCoreDebugRegs.count; i++, v++)
		if (v->offset == offset)
			goto found;
	v = gpuCoreDumpCoreRegs.values;
	for (i = 0; i != gpuCoreDumpCoreRegs.count; i++, v++)
		if (v->offset == offset)
			goto found;
	return 0;
found:
	*value = v->value[core];
	return 1;
}

void gpuCoreDumpCapture(PVRSRV_SGXDEV_INFO *devInfo, int dumpTrace)
{
	IMG_VOID **regs = &devInfo->pvRegsBaseKM;
	SGXMKIF_TA3D_CTL *render;
	GpuCoreDumpAnalysis analysis, textAnalysis;
	GpuCoreDumpPmonHeader pmon;
	GpuCoreDumpPmonClock clock;
	GpuCoreDumpPmonVersion version;
	GpuCoreDumpPmonTime pmonTime;
	GpuTraceCounters counters, saved;
	GpuTraceInfo gpu;
	GpuCoreDumpLog log, processLog;
	SceDateTime time;
	struct PamgrPrimaryTraceConfig trace;
	SceUInt32 gpuInfo[8], value, used, first, mhz;
	char path[GPU_COREDUMP_PATH_MAX], dir[GPU_COREDUMP_PATH_MAX];
	const char *name;
	SceUInt32 soc, i, sel;
	SceUID fd, pid;
	int cores, core, ret;

	cores = (GPU_REG(regs, GPU_COREDUMP_MASTER_CORE) & GPU_COREDUMP_MASTER_CORE_ENABLE_MASK) + 1;
	gpuCoreDumpCaptureMaster(regs, &gpuCoreDumpMasterRegs);
	gpuCoreDumpCaptureMaster(regs, &gpuCoreDumpMasterDebugRegs);
	gpuCoreDumpCaptureCores(regs, &gpuCoreDumpCoreDebugRegs, cores);
	gpuCoreDumpCaptureCores(regs, &gpuCoreDumpCoreRegs, cores);

	gpuCoreDumpIndexed.count = GPU_COREDUMP_INDEXED;
	for (core = 0; core < cores; core++)
		GPU_CORE_REG(regs, core, SCE_SGX_COUNTER_ENABLE) = 1;
	for (i = 0; i != GPU_COREDUMP_INDEXED; i++) {
		sel = i | i << 16;
		gpuCoreDumpIndexed.value[i].offset = i;
		for (core = 0; core < cores; core++) {
			GPU_CORE_REG(regs, core, SCE_SGX_COUNTER_SELECT_FIRST) = sel | SCE_SGX_COUNTER_FIRST_SOURCE;
			GPU_CORE_REG(regs, core, SCE_SGX_COUNTER_SELECT_SECOND) = sel | SCE_SGX_COUNTER_SECOND_SOURCE;
			gpuCoreDumpIndexed.value[i].value[core] = GPU_CORE_REG(regs, core, SCE_SGX_COUNTER_VALUE);
		}
	}

	memset(gpuCoreDumpRenderState, 0, sizeof(gpuCoreDumpRenderState));
	if (devInfo->psKernelSGXTA3DCtlMemInfo != NULL &&
	    (render = devInfo->psKernelSGXTA3DCtlMemInfo->pvLinAddrKM) != NULL) {
		gpuCoreDumpRenderState[0] = render->renderState[0];
		gpuCoreDumpRenderState[1] = render->renderState[1];
		gpuCoreDumpRenderState[2] = render->renderState[2];
		gpuCoreDumpRenderState[3] = render->renderState[3];
	}

	soc = kscePervasiveGetSoCRevision();
	memset(&time, 0, sizeof(time));
	ksceRtcGetCurrentClockLocalTime(&time);
	gpuCoreDumpLogOpen(&log, SCE_KERNEL_ALL_PROCESS_ID);
	gpuCoreDumpHeader.version = 0x20002;
	gpuCoreDumpHeader.sdkVersion = SCE_PSP2_SDK_VERSION;
	gpuCoreDumpHeader.socRevision = soc & 0x1ffff;
	gpuCoreDumpHeader.build = gpuCoreDumpBuild;

	/* first silently, for the process; then to its console */
	analysis.prefix = gpuCoreDumpModuleInfo.module_name;
	analysis.log = NULL;
	gpuCoreDumpAnalyse(devInfo, cores, 1, &analysis);
	pid = analysis.pid;
	if (pid == -1) {
		gpuCoreDumpLogPrintf(&log, "%s: A GPU core dump has occurred in an unknown process.\n",
				     gpuCoreDumpModuleInfo.module_name);
		analysis.log = &log;
		gpuCoreDumpAnalyse(devInfo, cores, 1, &analysis);
	} else {
		name = ksceGUIDName(pid);
		gpuCoreDumpLogPrintf(&log, "%s: A GPU core dump has occurred in process '%s' (0x%08x)\n",
				     gpuCoreDumpModuleInfo.module_name, name != NULL ? name : "unknown", pid);
		gpuCoreDumpLogOpen(&processLog, pid);
		analysis.log = &processLog;
		gpuCoreDumpAnalyse(devInfo, cores, 1, &analysis);
		gpuCoreDumpLogClose(&processLog);
	}

	memset(gpuCoreDumpText, 0, sizeof(gpuCoreDumpText));
	for (i = 0; i != GPU_COREDUMP_DATA_MASTERS; i++) {
		gpuCoreDumpProcesses[i].pid = analysis.dm[i].pid;
		OSMemSet(gpuCoreDumpProcesses[i].name, 0, sizeof(gpuCoreDumpProcesses[i].name));
		if (analysis.dm[i].name != NULL)
			strncpy_s(gpuCoreDumpProcesses[i].name, analysis.dm[i].name,
				  sizeof(gpuCoreDumpProcesses[i].name) - 1);
	}

	/* the analysis again, into the dump */
	gpuCoreDumpLogBuffer(&processLog, gpuCoreDumpText, gpuCoreDumpText + sizeof(gpuCoreDumpText));
	textAnalysis.prefix = "\r";
	textAnalysis.log = &processLog;
	gpuCoreDumpAnalyse(devInfo, cores, 1, &textAnalysis);

	if ((!gpuCoreDumpGetHostPath(path) || gpuCoreDumpSave("host0:", path, "/", &time, dir) != 0) &&
	    gpuCoreDumpSave("host0:", "", "", &time, dir) != 0 &&
	    gpuCoreDumpSave("sd0:", "", "", &time, dir) != 0 &&
	    gpuCoreDumpSave("ux0:", "data/", "", &time, dir) != 0)
		gpuCoreDumpLogPrintf(&log, "%s: Unable to write GPU core dump file\n",
				     gpuCoreDumpModuleInfo.module_name);
	else
		gpuCoreDumpLogPrintf(&log, "%s: GPU core dump file was written to %s.\n\n",
				     gpuCoreDumpModuleInfo.module_name, dir);

	if (!dumpTrace)
		goto out;

	/* the PA trace, as a .pmon file */
	gpuCoreDumpLogPrintf(&log, "%s: Dumping GPU Trace .pmon file on GPU lockup...\n",
			     gpuCoreDumpModuleInfo.module_name);
	memset(&pmon, 0, sizeof(pmon));
	pmon.magic = GPU_COREDUMP_PMON_MAGIC;
	pmon.size = sizeof(pmon);
	pmon.version = 0x10003;
	pmon.traceBuffers = 1;
	pmon.base = 0xc0000000;
	pmon.state = ScePamgrForDriver_3ACF4316();
	pmon.position = ScePamgrForDriver_FB46C3CD();
	ret = ScePamgrForDriver_606C98CD(&trace);
	if (ret != 0)
		goto failed;
	pmon.position = pmon.base + pmon.position - trace.base;
	pmon.bufferLimit = trace.limit;
	pmon.traceControl = trace.control;
	pmon.bufferSize = trace.size;
	pmon.traceSize = trace.size;
	ret = ScePamgrForDriver_02ACD81C(&pmon.gpuTrace);
	if (ret != 0)
		goto failed;
	pmon.gpuTrace.control = 3;
	cores = (GPU_REG(regs, GPU_COREDUMP_MASTER_CORE) & GPU_COREDUMP_MASTER_CORE_ENABLE_MASK) + 1;
	for (core = 0; core < cores; core++) {
		if (gpuCoreDumpCoreValue(core, 0x90, &pmon.counters[0][core]) +
		    gpuCoreDumpCoreValue(core, 0x94, &pmon.counters[1][core]) +
		    gpuCoreDumpCoreValue(core, 0x98, &pmon.counters[2][core]) +
		    gpuCoreDumpCoreValue(core, 0x9c, &pmon.counters[3][core]) != 4)
			goto error;
		pmon.counters[0][core] &= 0x1f7f1f7f;
		pmon.counters[1][core] &= 0x1f7f1f7f;
		pmon.counters[2][core] &= 0x1f7f1f7f;
		pmon.counters[3][core] &= 0x1f7f1f7f;
	}
	if ((!gpuCoreDumpGetHostPath(dir) ||
	     (fd = gpuCoreDumpOpen("host0:", dir, "/", &time, "pmon", path)) < 0) &&
	    (fd = gpuCoreDumpOpen("host0:", "", "", &time, "pmon", path)) < 0 &&
	    (fd = gpuCoreDumpOpen("sd0:", "", "", &time, "pmon", path)) < 0)
		fd = gpuCoreDumpOpen("ux0:", "data/", "", &time, "pmon", path);
	ret = fd;
	if (fd < 0)
		goto failed;
	if (ksceIoWrite(fd, &pmon, sizeof(pmon)) != sizeof(pmon))
		goto error;
	clock.magic = GPU_COREDUMP_PMON_CLOCK_MAGIC;
	clock.size = sizeof(clock);
	clock.sdkVersion = SCE_PSP2_SDK_VERSION;
	kscePowerGetGpuClockFrequencyInternal(&mhz, 0);
	clock.hz = mhz * 1000000;
	if (ksceIoWrite(fd, &clock, sizeof(clock)) != sizeof(clock))
		goto error;

	used = ScePamgrForDriver_019BD60F(0);
	gpuCoreDumpLogPrintf(&log, "%s: Writing trace buffer... 0x%08x bytes\n",
			     gpuCoreDumpModuleInfo.module_name, used);
	gpuCoreDumpLogPrintf(&log, "%s: ", gpuCoreDumpModuleInfo.module_name);
	if (used != 0) {
		/* a wrapped buffer: the older part first */
		if (pmon.state & 8) {
			first = pmon.position - pmon.base;
			gpuCoreDumpWriteTrace(fd, (const SceUInt8 *)(first + 0xc0000000), used - first,
					      &log);
			used = first;
		}
		gpuCoreDumpWriteTrace(fd, (const SceUInt8 *)0xc0000000, used, &log);
	}
	gpuCoreDumpLogPrintf(&log, "\n");

	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_START, NULL, 0);
	SceGpuEs4ForDriver_990338F6(&gpu);
	value = gpu.socRevision;
	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_GPU, &value, sizeof(value));
	gpuInfo[0] = gpu.gpuClockFrequency[0];
	gpuInfo[1] = gpu.gpuClockFrequency[1];
	gpuInfo[2] = gpu.gpuXbarClockFrequency;
	gpuInfo[3] = gpu.armClockFrequency;
	gpuInfo[4] = gpu.timebaseFrequency;
	gpuInfo[5] = gpu.sysClockFrequency;
	gpuInfo[6] = gpu.busClockFrequency;
	gpuInfo[7] = gpu.wirelessState;
	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_GPU_INFO, gpuInfo, sizeof(gpuInfo));
	version.build = 5029;
	version.major = 1;
	version.minor = 7;
	version.revision = 0x11;
	version.reserved = 0;
	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_VERSION, &version, sizeof(version));
	pmonTime.year = time.year;
	pmonTime.month = time.month;
	pmonTime.day = time.day;
	pmonTime.hour = time.hour;
	pmonTime.minute = time.minute;
	pmonTime.second = time.second;
	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_TIME, &pmonTime, sizeof(pmonTime));
	value = 8;
	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_CORES, &value, sizeof(value));
	SceGpuEs4ForDriver_C0C73C1B(&counters);
	memset(&saved, 0, sizeof(saved));
	for (i = 0; i != 8; i++) {
		saved.id[i] = counters.id[i];
		strncpy_s(saved.name[i], counters.name[i], sizeof(saved.name[i]) - 1);
	}
	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_COUNTERS, &saved, sizeof(saved));
	gpuCoreDumpWriteTag(fd, GPU_COREDUMP_TAG_END, NULL, 0);
	ret = ksceIoClose(fd);
	if (ret == 0) {
		gpuCoreDumpLogPrintf(&log, "%s: GPU Trace .pmon file written to %s\n",
				     gpuCoreDumpModuleInfo.module_name, path);
		goto out;
	}
failed:
	if (ret == 0)
		goto out;
error:
	gpuCoreDumpLogPrintf(&log, "%s: Error, unable to write GPU Trace .pmon file.\n\n",
			     gpuCoreDumpModuleInfo.module_name);
out:
	gpuCoreDumpLogClose(&log);
}

/* SceGpuEs4: the GPU stopped */
static void gpuCoreDumpOnLockup(int dumpTrace)
{
	PvrsrvDeviceIdentifier ids[1], *sgx;
	PVRSRV_DEVICE_NODE *node = NULL;
	SceUInt32 count, i;

	if (PVRSRVEnumerateDevicesKM(&count, ids) != 0)
		return;
	sgx = NULL;
	for (i = 0; i != count; i++)
		if (ids[i].cls == PVRSRV_DEVICE_CLASS_3D && ids[i].type == PVRSRV_DEVICE_TYPE_SGX)
			sgx = &ids[i];
	if (sgx == NULL || PVRSRVAcquireDeviceDataKM(sgx->index, 0, (void **)&node) != 0 || node == NULL)
		return;
	if (node->pvDevice == NULL)
		ksceKernelPrintf("%s: psDevInfo is NULL\n", gpuCoreDumpModuleInfo.module_name);
	else
		gpuCoreDumpCapture(node->pvDevice, dumpTrace);
}

int gpuCoreDumpIndexedValue(SceUInt32 core, SceUInt32 index, SceUInt32 *value)
{
	if (index < GPU_COREDUMP_INDEXED && core < GPU_COREDUMP_CORES) {
		*value = gpuCoreDumpIndexed.value[index].value[core];
		return 1;
	}
	return 0;
}

/* a master register as captured */
int gpuCoreDumpMasterValue(SceUInt32 offset, SceUInt32 *value)
{
	const GpuCoreDumpMasterValue *v;
	SceUInt32 i;

	v = gpuCoreDumpMasterRegs.values;
	for (i = 0; i != gpuCoreDumpMasterRegs.count; i++, v++)
		if (v->offset == offset)
			goto found;
	v = gpuCoreDumpMasterDebugRegs.values;
	for (i = 0; i != gpuCoreDumpMasterDebugRegs.count; i++, v++)
		if (v->offset == offset)
			goto found;
	return 0;
found:
	*value = v->value;
	return 1;
}

SceUInt32 *gpuCoreDumpGetRenderState(void)
{
	return gpuCoreDumpRenderState;
}

int module_start(SceSize args, void *argp)
{
	GpuCoreDumpCoreValue *v;
	int i;

	if (ksceKernelCheckDipsw(0xd4) == 0)
		return SCE_KERNEL_START_NO_RESIDENT;
	if (ksceKernelGetModuleInfoByAddr(&gpuCoreDumpModuleInfo, &gpuCoreDumpModuleInfo) < 0)
		for (i = 0; i != sizeof(GPU_COREDUMP_NAME); i++)
			gpuCoreDumpModuleInfo.module_name[i] = GPU_COREDUMP_NAME[i];
	gpuCoreDumpResetMaster(&gpuCoreDumpMasterRegs);
	gpuCoreDumpResetMaster(&gpuCoreDumpMasterDebugRegs);
	gpuCoreDumpResetCore(&gpuCoreDumpCoreDebugRegs);
	gpuCoreDumpResetCore(&gpuCoreDumpCoreRegs);
	gpuCoreDumpIndexed.count = GPU_COREDUMP_INDEXED;
	for (v = gpuCoreDumpIndexed.value; v != gpuCoreDumpIndexed.value + GPU_COREDUMP_INDEXED; v++) {
		v->offset = GPU_COREDUMP_POISON;
		v->value[0] = 0;
		v->value[1] = 0;
		v->value[2] = 0;
		v->value[3] = 0;
	}
	gpuCoreDumpMaxNameLength = 0;
	gpuCoreDumpMasterNameLength(&gpuCoreDumpMasterRegs);
	gpuCoreDumpMasterNameLength(&gpuCoreDumpMasterDebugRegs);
	gpuCoreDumpCoreNameLength(&gpuCoreDumpCoreDebugRegs);
	gpuCoreDumpCoreNameLength(&gpuCoreDumpCoreRegs);
	SceGpuEs4ForDriver_26B03DCE(gpuCoreDumpStopTrace, gpuCoreDumpOnLockup);
	return SCE_KERNEL_START_SUCCESS;
}
