/*
 * ScePerf: Razor CPU DTrace-based activity monitor.
 */

#include "libperf_internal.h"
#include "dtrace_dof_data.h"

#define DTRACEIOC			(('d' << 24) | ('t' << 16) | ('r' << 8))
#define DTRACEIOC_PROVIDER		(DTRACEIOC | 1)
#define DTRACEIOC_AGGCLEAR		(DTRACEIOC | 4)
#define DTRACEIOC_PROBEDESC		(DTRACEIOC | 5)
#define DTRACEIOC_ENABLE		(DTRACEIOC | 6)
#define DTRACEIOC_AGGSNAP		(DTRACEIOC | 7)
#define DTRACEIOC_CONF			(DTRACEIOC | 10)
#define DTRACEIOC_STATUS		(DTRACEIOC | 11)
#define DTRACEIOC_GO			(DTRACEIOC | 12)
#define DTRACEIOC_STOP			(DTRACEIOC | 13)

#define DOF_SECT_OPTDESC		14
#define DTRACEOPT_AGGSIZE		3
#define DOF_SECIDX_NONE			0xFFFFFFFFU
#define DTRACEOPT_UNSET			0xFFFFFFFFFFFFFFFEULL

#define ACTIVITY_MONITOR_NUM_CPUS	4
#define ACTIVITY_MONITOR_NUM_THREADS	3

typedef struct {
	SceUInt8 dofh_ident[16];
	SceUInt32 dofh_flags;
	SceUInt32 dofh_hdrsize;
	SceUInt32 dofh_secsize;
	SceUInt32 dofh_secnum;
	SceUInt64 dofh_secoff;
	SceUInt64 dofh_loadsz;
	SceUInt64 dofh_filesz;
	SceUInt64 dofh_pad;
} DofHdr;

typedef struct {
	SceUInt32 dofs_type;
	SceUInt32 dofs_align;
	SceUInt32 dofs_flags;
	SceUInt32 dofs_entsize;
	SceUInt64 dofs_offset;
	SceUInt64 dofs_size;
} DofSec;

typedef struct {
	SceUInt32 dofo_option;
	SceUInt32 dofo_strtab;
	SceUInt64 dofo_value;
} DofOptDesc;

typedef struct {
	SceUInt32 dtp_id;
	char dtp_provider[64];
	char dtp_mod[64];
	char dtp_func[128];
	char dtp_name[64];
} DTraceProbeDesc;

typedef struct {
	SceUInt64 dtbd_size;
	SceUInt32 dtbd_cpu;
	SceUInt32 dtbd_errors;
	SceUInt64 dtbd_drops;
	SceUInt32 dtbd_data;
	SceUInt32 dtbd_pad;
	SceUInt64 dtbd_oldest;
} DTraceBufDesc;

_Static_assert(sizeof(DTraceBufDesc) == 0x28, "DTraceBufDesc size");

typedef struct {
	SceUInt64 aggId;
	SceUInt64 cpuId;
	SceUInt64 frameIndex;
	SceUInt64 runtime;
	SceUInt64 pad;
} DTraceAggRecord;

static int g_dtraceFd = -1;
static SceInt32 g_activityMonitorRunning;
static SceUInt8 *g_dtraceScratchBuf;
static SceUInt32 g_activityMaxPackets;
static SceUID g_activityThreads[ACTIVITY_MONITOR_NUM_THREADS];
static SceUInt32 g_dtraceScratchSize;
static SceRazorCpuActivityMonitorPacket *g_activityPacketBuf;
static volatile SceInt32 g_activityStopFlag;
static SceUInt32 g_activityPacketCount;

static const SceUInt32 g_dtraceIoctlZero = 0;

static const char g_dtraceProviderSched[0x5C] = "sched";

static const DTraceProbeDesc g_probeBegin = { 0, "", "", "", "BEGIN" };
static const DTraceProbeDesc g_probeSchedOnCpu = { 0, "sched", "", "", "on-cpu" };
static const DTraceProbeDesc g_probeSchedOffCpu1 = { 0, "sched", "", "", "off-cpu" };
static const DTraceProbeDesc g_probeSchedOffCpu2 = { 0, "sched", "", "", "off-cpu" };
static const DTraceProbeDesc g_probeSchedOffCpu3 = { 0, "sched", "", "", "off-cpu" };
static const DTraceProbeDesc g_probeTick1s = { 0, "", "", "", "tick-1s" };
static const DTraceProbeDesc g_probeEnd = { 0, "", "", "", "END" };
static const DTraceProbeDesc g_probeError = { 0, "dtrace", "", "", "ERROR" };

static const DTraceBufDesc g_aggSnapCpu0 = { 0x20000ULL, 0, 0, 0, 0x2BFC48U, 0, 0 };
static const DTraceBufDesc g_aggSnapCpu1 = { 0x20000ULL, 1, 0, 0, 0x2BFC48U, 0, 0 };
static const DTraceBufDesc g_aggSnapCpu2 = { 0x20000ULL, 2, 0, 0, 0x2BFC48U, 0, 0 };
static const DTraceBufDesc g_aggSnapCpu3 = { 0x20000ULL, 3, 0, 0, 0x2BFC48U, 0, 0 };

static const DTraceBufDesc *const g_aggSnapDescs[ACTIVITY_MONITOR_NUM_CPUS] = {
	&g_aggSnapCpu0,
	&g_aggSnapCpu1,
	&g_aggSnapCpu2,
	&g_aggSnapCpu3,
};

static const DTraceBufDesc g_aggClearCpu3 = { 0x20000ULL, 3, 0, 0, 0x2DFC50U, 0, 0 };
static const DTraceBufDesc g_aggClearCpu0 = { 0ULL, 0, 0, 0, 0x2DFC50U, 0, 0 };
static const DTraceBufDesc g_aggClearCpu1 = { 0ULL, 1, 0, 0, 0x2DFC50U, 0, 0 };
static const DTraceBufDesc g_aggClearCpu2 = { 0ULL, 2, 0, 0, 0x2DFC50U, 0, 0 };

static const DTraceBufDesc *const g_aggClearDescs[ACTIVITY_MONITOR_NUM_CPUS] = {
	&g_aggClearCpu3,
	&g_aggClearCpu0,
	&g_aggClearCpu1,
	&g_aggClearCpu2,
};

static const char *const g_activityThreadNames[ACTIVITY_MONITOR_NUM_THREADS] = {
	"ScePerfForceSchedulerActivity_0",
	"ScePerfForceSchedulerActivity_1",
	"ScePerfForceSchedulerActivity_2",
};

static const SceInt32 g_activityThreadAffinities[ACTIVITY_MONITOR_NUM_THREADS] = {
	0x10000,
	0x20000,
	0x40000,
};

static int forceSchedulerActivityThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	while (g_activityStopFlag == 0)
		sceKernelDelayThread(100000);

	return 0;
}

static __attribute__((noinline)) int readDTraceActivityData(int fd)
{
	int ret;
	int cpu;
	int overflow = 0;

	if (g_dtraceScratchSize < 4U)
		return SCE_ERROR_ERRNO_ENOMEM;

	sceClibMemcpy_safe(g_dtraceScratchBuf, &g_dtraceIoctlZero, 4U);
	ret = sceDTraceClientIoctl(fd, DTRACEIOC_STATUS, g_dtraceScratchBuf, g_dtraceScratchSize);
	if (ret < 0)
		return ret;

	for (cpu = 0; cpu < ACTIVITY_MONITOR_NUM_CPUS; cpu++) {
		DTraceBufDesc *desc;
		SceUInt64 numRecords;

		if (g_dtraceScratchSize < sizeof(DTraceBufDesc))
			return SCE_ERROR_ERRNO_ENOMEM;

		sceClibMemcpy_safe(g_dtraceScratchBuf, g_aggSnapDescs[cpu], sizeof(DTraceBufDesc));
		desc = (DTraceBufDesc *)g_dtraceScratchBuf;
		desc->dtbd_data = (SceUInt32)(g_dtraceScratchBuf + 320);

		ret = sceDTraceClientIoctl(fd, DTRACEIOC_AGGSNAP, g_dtraceScratchBuf,
					   g_dtraceScratchSize);
		if (ret < 0)
			return ret;

		numRecords = ((const DTraceBufDesc *)g_dtraceScratchBuf)->dtbd_size /
			     sizeof(DTraceAggRecord);
		if (numRecords != 0) {
			SceUInt32 count = g_activityPacketCount;
			const DTraceAggRecord *rec =
				(const DTraceAggRecord *)(g_dtraceScratchBuf + 320);
			SceUInt64 r;

			for (r = 0; r < numRecords; r++, rec++) {
				SceUInt32 recCpu = (SceUInt32)rec->cpuId;
				SceUInt64 frameIdx = rec->frameIndex;
				SceRazorCpuActivityMonitorPacket *pkt = NULL;
				SceUInt32 i;

				__builtin_prefetch((const SceUInt8 *)rec + 144);

				for (i = 0; i < count; i++) {
					if (g_activityPacketBuf[i].frameIndex == frameIdx &&
					    g_activityPacketBuf[i].cpuId == recCpu) {
						pkt = &g_activityPacketBuf[i];
						break;
					}
				}

				if (pkt == NULL) {
					if (count >= g_activityMaxPackets) {
						overflow = 1;
						break;
					}
					if (recCpu >= ACTIVITY_MONITOR_NUM_CPUS) {
						g_activityPacketCount = count;
						return SCE_PERF_ERROR_BAD_TRACE_DATA;
					}
					pkt = &g_activityPacketBuf[count++];
					pkt->system = 0;
					pkt->idle = 0;
					pkt->user = 0;
					pkt->frameIndex = frameIdx;
					pkt->cpuId = recCpu;
				}

				if (rec->aggId == 2ULL)
					pkt->user += rec->runtime;
				else if (rec->aggId == 3ULL)
					pkt->system += rec->runtime;
				else if (rec->aggId == 1ULL)
					pkt->idle += rec->runtime;
			}
			g_activityPacketCount = count;
		}
	}

	for (cpu = 0; cpu < ACTIVITY_MONITOR_NUM_CPUS; cpu++) {
		DTraceBufDesc *desc;

		if (g_dtraceScratchSize < sizeof(DTraceBufDesc))
			return SCE_ERROR_ERRNO_ENOMEM;

		sceClibMemcpy_safe(g_dtraceScratchBuf, g_aggClearDescs[cpu], sizeof(DTraceBufDesc));
		desc = (DTraceBufDesc *)g_dtraceScratchBuf;
		desc->dtbd_data = (SceUInt32)(g_dtraceScratchBuf + 320);

		ret = sceDTraceClientIoctl(fd, DTRACEIOC_AGGCLEAR, g_dtraceScratchBuf,
					   g_dtraceScratchSize);
		if (ret < 0)
			return ret;
	}

	if (overflow)
		return SCE_ERROR_ERRNO_ENOMEM;
	return 0;
}

int sceRazorCpuGetActivityMonitorTraceBuffer(SceRazorCpuActivityMonitorPacket **pTrace)
{
	int ret;

	if (!g_activityMonitorRunning)
		return SCE_ERROR_ERRNO_EINVAL;

	ret = readDTraceActivityData(g_dtraceFd);
	if (ret >= 0) {
		ret = (int)g_activityPacketCount;
		g_activityPacketCount = 0;
		*pTrace = g_activityPacketBuf;
	}
	return ret;
}

static __attribute__((noinline)) int startDTraceActivityMonitor(void)
{
	int fd;
	int i;

	fd = sceDTraceClientOpen("/dev/dtrace/dtrace", 0);
	if (fd < 0)
		return 2;

	g_dtraceFd = fd;

	do {
		if (g_dtraceScratchSize < 4U)
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_dtraceIoctlZero, 4U);
		sceDTraceClientIoctl(fd, DTRACEIOC_CONF, g_dtraceScratchBuf, g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(g_dtraceProviderSched))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, g_dtraceProviderSched,
				   sizeof(g_dtraceProviderSched));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROVIDER, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeBegin, sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeSchedOnCpu, sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeSchedOffCpu1,
				   sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeSchedOffCpu2,
				   sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeSchedOffCpu3,
				   sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeTick1s, sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeEnd, sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);

		if (g_dtraceScratchSize < sizeof(DTraceProbeDesc))
			break;
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_probeError, sizeof(DTraceProbeDesc));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_PROBEDESC, g_dtraceScratchBuf,
				     g_dtraceScratchSize);
	} while (0);

	if (g_dtraceScratchSize >= sizeof(g_dofActivityMonitorMain)) {
		sceClibMemcpy_safe(g_dtraceScratchBuf, g_dofActivityMonitorMain,
				   sizeof(g_dofActivityMonitorMain));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_ENABLE, g_dtraceScratchBuf,
				     g_dtraceScratchSize);
	}

	if (g_dtraceScratchSize >= sizeof(g_dofActivityMonitorAux)) {
		sceClibMemcpy_safe(g_dtraceScratchBuf, g_dofActivityMonitorAux,
				   sizeof(g_dofActivityMonitorAux));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_ENABLE, g_dtraceScratchBuf,
				     g_dtraceScratchSize);
	}

	if (g_dtraceScratchSize >= sizeof(g_dofActivityMonitorOpt)) {
		sceClibMemcpy_safe(g_dtraceScratchBuf, g_dofActivityMonitorOpt,
				   sizeof(g_dofActivityMonitorOpt));
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_ENABLE, g_dtraceScratchBuf,
				     g_dtraceScratchSize);
	}

	if (g_dtraceScratchSize >= 4U) {
		sceClibMemcpy_safe(g_dtraceScratchBuf, &g_dtraceIoctlZero, 4U);
		sceDTraceClientIoctl(g_dtraceFd, DTRACEIOC_GO, g_dtraceScratchBuf,
				     g_dtraceScratchSize);
	}

	g_activityStopFlag = 0;
	for (i = 0; i < ACTIVITY_MONITOR_NUM_THREADS; i++) {
		SceUID tid = sceKernelCreateThread(g_activityThreadNames[i],
						   forceSchedulerActivityThread,
						   64, 0x100, 0,
						   g_activityThreadAffinities[i], NULL);
		g_activityThreads[i] = tid;
		if (sceKernelStartThread(tid, 0, NULL) < 0)
			break;
	}

	return 0;
}

static inline SceUInt64 getDofMaxAggSize(const SceUInt8 *buf)
{
	const DofHdr *hdr = (const DofHdr *)buf;
	SceUInt32 secNum = hdr->dofh_secnum;
	SceUInt32 i;
	SceUInt64 maxVal = 0;

	for (i = 0; i < secNum; i++) {
		const DofSec *sec = (const DofSec *)(buf + (SceUInt32)hdr->dofh_secoff +
						     i * hdr->dofh_secsize);

		if (sec->dofs_type == DOF_SECT_OPTDESC) {
			SceUInt32 entSize;
			SceUInt64 secSize;
			SceUInt32 off;

			if (sec->dofs_align != 8U)
				return 0;
			entSize = sec->dofs_entsize;
			if (entSize < sizeof(DofOptDesc))
				return 0;
			secSize = sec->dofs_size;
			for (off = 0; off < secSize; off += entSize) {
				const DofOptDesc *opt =
					(const DofOptDesc *)(buf + (SceUInt32)sec->dofs_offset + off);

				if (opt->dofo_strtab != DOF_SECIDX_NONE)
					return 0;
				if (opt->dofo_value == DTRACEOPT_UNSET)
					return 0;
				if (opt->dofo_option == DTRACEOPT_AGGSIZE &&
				    opt->dofo_value > maxVal) {
					maxVal = opt->dofo_value;
				}
			}
		}
	}

	return maxVal;
}

int sceRazorCpuStartActivityMonitor(void *pBufferBase, SceUInt32 bufferSize)
{
	SceUInt64 aggMain;
	SceUInt64 aggAux;
	SceUInt64 aggOpt;
	SceUInt64 maxAgg;
	SceUInt32 aligned;
	SceUInt32 scratchSize;
	int ret;

	g_activityMonitorRunning = 0;
	if (pBufferBase == NULL || ((SceUIntPtr)pBufferBase & 7U) != 0)
		return SCE_ERROR_ERRNO_EINVAL;

	g_dtraceScratchBuf = (SceUInt8 *)pBufferBase;
	if (bufferSize < sizeof(g_dofActivityMonitorMain))
		return SCE_ERROR_ERRNO_ENOMEM;

	sceClibMemcpy_safe(g_dtraceScratchBuf, g_dofActivityMonitorMain,
			   sizeof(g_dofActivityMonitorMain));
	aggMain = getDofMaxAggSize(g_dtraceScratchBuf);

	sceClibMemcpy_safe(g_dtraceScratchBuf, g_dofActivityMonitorAux,
			   sizeof(g_dofActivityMonitorAux));
	aggAux = getDofMaxAggSize(g_dtraceScratchBuf);

	maxAgg = (aggAux > aggMain) ? aggAux : aggMain;

	sceClibMemcpy_safe(g_dtraceScratchBuf, g_dofActivityMonitorOpt,
			   sizeof(g_dofActivityMonitorOpt));
	aggOpt = getDofMaxAggSize(g_dtraceScratchBuf);

	if (aggOpt > maxAgg)
		maxAgg = aggOpt;

	aligned = ((SceUInt32)maxAgg + 7U) & ~7U;
	scratchSize = aligned + 40U;
	g_dtraceScratchSize = scratchSize;
	if (bufferSize < aligned + 200U)
		return SCE_ERROR_ERRNO_ENOMEM;

	g_activityPacketBuf = (SceRazorCpuActivityMonitorPacket *)((SceUInt8 *)pBufferBase +
								   scratchSize);
	g_activityMaxPackets = (bufferSize - scratchSize) /
			       sizeof(SceRazorCpuActivityMonitorPacket);

	ret = startDTraceActivityMonitor();
	if (ret >= 0) {
		g_activityMonitorRunning = 1;
		return 0;
	}
	return ret;
}

int sceRazorCpuStopActivityMonitor(void)
{
	int fd = g_dtraceFd;
	int ret;
	int i;
	int exitStatus;

	if (fd == -1)
		return SCE_ERROR_ERRNO_EINVAL;

	if (g_dtraceScratchSize < 4U)
		return SCE_ERROR_ERRNO_ENOMEM;

	sceClibMemcpy_safe(g_dtraceScratchBuf, &g_dtraceIoctlZero, 4U);
	ret = sceDTraceClientIoctl(fd, DTRACEIOC_STOP, g_dtraceScratchBuf,
				   g_dtraceScratchSize);
	if (ret < 0)
		return ret;

	ret = sceDTraceClientClose(g_dtraceFd);
	if (ret < 0)
		return ret;

	g_activityStopFlag = 1;
	for (i = 0; i < ACTIVITY_MONITOR_NUM_THREADS; i++) {
		sceKernelWaitThreadEnd(g_activityThreads[i], &exitStatus, NULL);
		sceKernelDeleteThread(g_activityThreads[i]);
	}

	g_dtraceFd = -1;
	return 0;
}
