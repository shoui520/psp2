/*
 * SceRazorHud SDK header (GPU live metrics, trace, and HUD overlay).
 */

#ifndef _SCE_RAZOR_HUD_H
#define _SCE_RAZOR_HUD_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define SCE_RAZOR_GPU_ERROR_NOT_INITIALIZED          0x8008c000
#define SCE_RAZOR_GPU_ERROR_INVALID_ARGUMENT         0x8008c001
#define SCE_RAZOR_GPU_ERROR_INVALID_PERF_MODE        0x8008c002
#define SCE_RAZOR_GPU_ERROR_LIVE_NOT_STARTED         0x8008c003
#define SCE_RAZOR_GPU_ERROR_LIVE_ALREADY_STARTED     0x8008c004
#define SCE_RAZOR_GPU_ERROR_PA_NOT_AVAILABLE         0x8008c005
#define SCE_RAZOR_GPU_ERROR_TRACE_OVERFLOW           0x8008c006
#define SCE_RAZOR_GPU_ERROR_NOT_ALIGNED              0x8008c007
#define SCE_RAZOR_GPU_ERROR_SETTINGS_NOT_FOUND       0x8008c008
#define SCE_RAZOR_GPU_ERROR_INVALID_POINTER          0x8008c009

/* GPU performance modes */
#define SCE_RAZOR_GPU_PERF_MODE_NOT_INITIALIZED      0
#define SCE_RAZOR_GPU_PERF_MODE_NONE                 1
#define SCE_RAZOR_GPU_PERF_MODE_TRACE                2
#define SCE_RAZOR_GPU_PERF_MODE_LIVE                 3
#define SCE_RAZOR_PERF_MODE_HUD                      4
#define SCE_RAZOR_GPU_PERF_MODE_INVALID              5

/* Metrics groups */
#define SCE_RAZOR_GPU_LIVE_METRICS_GROUP_PBUFFER_USAGE 0
#define SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_1    1
#define SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_2    2
#define SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_3    3
#define SCE_RAZOR_GPU_LIVE_METRICS_GROUP_MAX           4

/* Entry types */
#define SCE_RAZOR_LIVE_TRACE_METRIC_ENTRY_TYPE_JOB              0
#define SCE_RAZOR_LIVE_TRACE_METRIC_ENTRY_TYPE_PARAMETER_BUFFER 1
#define SCE_RAZOR_LIVE_TRACE_METRIC_ENTRY_TYPE_FRAME            2

/* Job types */
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_FIRMWARE  0
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_VERTEX1   1
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_FRAGMENT1 2
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_VERTEX2   3
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_FRAGMENT2 4
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_VERTEX3   5
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_FRAGMENT3 6
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_VERTEX0   7
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_TYPE_FRAGMENT0 8

/* Job property flags */
#define SCE_RAZOR_LIVE_TRACE_METRIC_JOB_FLAG_PARTIAL_RENDER (1 << 0)

typedef struct SceRazorGpuLiveEntryHeader {
	SceUInt16 entrySize;
	SceUInt16 entryType;
	SceUInt32 reserved;
} SceRazorGpuLiveEntryHeader;

typedef struct SceRazorGpuLiveEntryJobVertexValues1 {
	SceFloat32 usseVertexProcessing;
} SceRazorGpuLiveEntryJobVertexValues1;

typedef struct SceRazorGpuLiveEntryJobFragmentValues1 {
	SceFloat32 usseFragmentProcessing;
	SceFloat32 usseDependentTextureReadRequest;
	SceFloat32 usseNonDependentTextureReadRequest;
} SceRazorGpuLiveEntryJobFragmentValues1;

typedef struct SceRazorGpuLiveEntryJobVertexValues2 {
	SceUInt32 vdmPrimitivesInput;
	SceUInt32 mtePrimitivesOutput;
	SceUInt32 vdmVerticesInput;
	SceUInt32 mteVerticesOutput;
} SceRazorGpuLiveEntryJobVertexValues2;

typedef struct SceRazorGpuLiveEntryJobFragmentValues2 {
	SceUInt32 rasterizedPixelsBeforeHsr;
	SceUInt32 rasterizedOutputPixels;
	SceUInt32 rasterizedOutputSamples;
} SceRazorGpuLiveEntryJobFragmentValues2;

typedef struct SceRazorGpuLiveEntryJobVertexValues3 {
	SceUInt32 bifTaMemoryWrite;
} SceRazorGpuLiveEntryJobVertexValues3;

typedef struct SceRazorGpuLiveEntryJobFragmentValues3 {
	SceUInt32 bifIspParameterFetchMemoryRead;
} SceRazorGpuLiveEntryJobFragmentValues3;

typedef struct SceRazorGpuLiveEntryJobData {
	SceUInt32 reserved[8];
} SceRazorGpuLiveEntryJobData;

typedef union SceRazorGpuLiveEntryJobValues {
	SceRazorGpuLiveEntryJobVertexValues1 vertexValues1;
	SceRazorGpuLiveEntryJobFragmentValues1 fragmentValues1;
	SceRazorGpuLiveEntryJobVertexValues2 vertexValues2;
	SceRazorGpuLiveEntryJobFragmentValues2 fragmentValues2;
	SceRazorGpuLiveEntryJobVertexValues3 vertexValues3;
	SceRazorGpuLiveEntryJobFragmentValues3 fragmentValues3;
	SceRazorGpuLiveEntryJobData reserved;
} SceRazorGpuLiveEntryJobValues;

typedef struct SceRazorGpuLiveEntryJob {
	SceRazorGpuLiveEntryHeader header;
	SceUInt64 startTime;
	SceUInt64 endTime;
	SceUInt8 type;
	SceInt8 core;
	SceUInt16 sceneIndex;
	SceUInt32 frameNumber;
	SceRazorGpuLiveEntryJobValues jobValues;
	SceUInt32 processId;
	SceUInt16 drawCount;
	SceUInt16 flags;
} SceRazorGpuLiveEntryJob;

typedef struct SceRazorGpuLiveEntryParameterBuffer {
	SceRazorGpuLiveEntryHeader header;
	SceUInt64 peakUsageTime;
	SceUInt32 peakUsage;
	SceUInt8 partialRender;
	SceUInt8 vertexJobPaused;
	SceUInt16 reserved;
} SceRazorGpuLiveEntryParameterBuffer;

typedef struct SceRazorGpuLiveEntryFrame {
	SceRazorGpuLiveEntryHeader header;
	SceUInt64 startTime;
	SceUInt32 duration;
	SceUInt32 frameNumber;
	SceUInt32 gpuActiveDuration;
	SceUInt32 reserved;
} SceRazorGpuLiveEntryFrame;

typedef struct SceRazorGpuLiveResultInfo {
	void *resultData;
	SceSize entryCount;
	SceSize overflowCount;
	SceSize bufferSize;
	SceUInt64 startTime;
	SceUInt64 endTime;
	SceUInt32 metricGroup;
	SceUInt32 reserved;
} SceRazorGpuLiveResultInfo;

SceInt32 sceRazorHudReloadSettings(void);
SceInt32 sceRazorGpuPerfGetMode(void);
SceInt32 sceRazorGpuLiveSetMetricsGroup(SceUInt32 metricsGroup);
SceInt32 sceRazorGpuLiveStart(void);
SceInt32 sceRazorGpuLiveStop(void);
SceInt32 sceRazorGpuLiveSetBuffer(void *output, SceSize outputSize, SceRazorGpuLiveResultInfo *results);
SceInt32 sceRazorGpuLiveTriggerDebugDump(void);
SceInt32 sceRazorGpuTraceSetFilename(const char *filename, SceInt32 count);
SceInt32 sceRazorGpuTraceSetMetricsGroup(SceInt32 traceMetricsGroup);
SceInt32 sceRazorGpuTraceTrigger(void);
SceInt32 sceRazorGpuTraceRingBufferSize(void);
SceInt32 sceRazorGpuTraceRingBufferUsed(void);
SceInt32 sceRazorHudSetDisplayEnabled(SceBool displayEnabled);
SceInt32 sceRazorHudSetDisplayFrameCount(SceInt32 frameCount);
SceInt32 sceRazorCpuHudUserMarkerEnableThreadId(SceUID threadID, SceBool enable);
SceInt32 sceRazorCpuHudUserMarkerEnableAllThreads(SceBool enable);
SceInt32 sceRazorCpuHudSetUserMarkerTraceBuffer(void *pBufferBase, SceUInt32 bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_RAZOR_HUD_H */
