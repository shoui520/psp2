/*
 * SceGpuEs4User (os0:us/libgpu_es4.suprx): user side of the SGX services.
 *
 * Nearly every export packs an input record, makes one bridge call into
 * gpu_es4.skprx (PVRSRV_BridgeDispatchKM) and unpacks the reply. The layout
 * is that of a PowerVR services client: a connection, device data handles,
 * device memory, sync objects and SGX contexts. Export names are unknown;
 * the comment at each prototype says what it does.
 */

#pragma once

#include <psp2/types.h>
#include <stdint.h>
#include <psp2/kernel/clib.h>
#include <gpu/pvrsrv_client.h>
#include <gpu/gpu_es4_trace.h>

/* the SceGpuEs4ForUser syscall interface */
struct SceGpuBridgePackage {
	uint32_t id;
	uint32_t size;
	void *in;
	uint32_t in_size;
	void *out;
	uint32_t out_size;
	uint32_t h_services;
};

int PVRSRVOpen(void);
int PVRSRVRelease(void);
int PVRSRV_BridgeDispatchKM(uint32_t id, struct SceGpuBridgePackage *pkg);

/* services result codes (the kernel's numbering) */
#define GPU_OK				0x00
#define GPU_ERROR_OUT_OF_MEMORY		0x01
#define GPU_ERROR_INVALID_PARAMS	0x03
#define GPU_ERROR_INIT_FAILURE		0x04
#define GPU_ERROR_RETRY			0x18	/* kernel busy: the bridge call is repeated */
#define GPU_ERROR_DDK_VERSION		0x19
#define GPU_ERROR_BUILD_OPTIONS		0x1a
#define GPU_ERROR_INVALID_FLAGS		0x1d
#define GPU_ERROR_BRIDGE_CALL_FAILED	0x22

/* SCE error codes returned by the sync object wait */
#define SCE_GPU_ERROR_BRIDGE		0x804C0000
#define SCE_GPU_ERROR_WOULD_BLOCK	0x804C0006

/* what the kernel must report to SgxCheckDdk */
#define GPU_DDK_VERSION			0x00010711	/* 1.7.17 */
#define GPU_DDK_BUILD			0x000013a5	/* 5029 */
#define GPU_DDK_BUILD_OPTIONS		0x30322518

#define PVRSRV_DEVICE_TYPE_SGX		7
#define GPU_MAX_DEVICES			1
#define GPU_MAX_HEAPS			32
#define GPU_MAX_TRANSFER_COMMANDS	26
#define GPU_MAX_SYNC_OPS		4

/* SceGpuEs4User_071E62F0 flags */
#define GPU_ALLOC_REQUIRED		0x00000008	/* the call fails without it */
#define GPU_ALLOC_NO_CPU_MAPPING	0x08000000	/* not passed to the kernel */

/* bridge call IDs */
#define PVRSRV_BRIDGE_ENUM_DEVICES			0x1000
#define PVRSRV_BRIDGE_ACQUIRE_DEVICEINFO		0x1001
#define PVRSRV_BRIDGE_CREATE_DEVMEMCONTEXT		0x1003
#define PVRSRV_BRIDGE_DESTROY_DEVMEMCONTEXT	0x1004
#define PVRSRV_BRIDGE_ALLOC_DEVICEMEM		0x1006
#define PVRSRV_BRIDGE_FREE_DEVICEMEM		0x1007
#define PVRSRV_BRIDGE_CONNECT_SERVICES		0x100c
#define PVRSRV_BRIDGE_DISCONNECT_SERVICES		0x100d
#define PVRSRV_BRIDGE_MAP_MEMORY_TO_GPU			0x101d
#define PVRSRV_BRIDGE_UNMAP_MEMORY_FROM_GPU			0x101e
#define PVRSRV_BRIDGE_CREATE_SHARED_SYNC_INFO		0x101f
#define PVRSRV_BRIDGE_OPEN_SHARED_SYNC_INFO		0x1020
#define PVRSRV_BRIDGE_CLOSE_SHARED_SYNC_INFO		0x1021
#define PVRSRV_BRIDGE_DELETE_SHARED_SYNC_INFO		0x1022
#define PVRSRV_BRIDGE_GET_MISC_INFO		0x102a
#define PVRSRV_BRIDGE_CREATE_SYNC_INFO_MOD_OBJ		0x104d
#define PVRSRV_BRIDGE_DESTROY_SYNC_INFO_MOD_OBJ		0x104e
#define PVRSRV_BRIDGE_MODIFY_PENDING_SYNC_OPS	0x104f
#define PVRSRV_BRIDGE_MODIFY_COMPLETE_SYNC_OPS	0x1050
#define PVRSRV_BRIDGE_SYNC_OPS_FLUSH_TO_MOD_OBJ	0x1053
#define PVRSRV_BRIDGE_ALLOC_SYNC_INFO		0x1055
#define PVRSRV_BRIDGE_FREE_SYNC_INFO		0x1056
#define PVRSRV_BRIDGE_CHECK_SYNC_INFO_MOD_OBJ	0x1057
#define PVRSRV_BRIDGE_REGISTER_MEMBLOCK		0x1058
#define PVRSRV_BRIDGE_UNREGISTER_MEMBLOCK		0x1059
#define PVRSRV_BRIDGE_CHECK_GPU_MAPPING			0x105b
#define PVRSRV_BRIDGE_GET_MAPPED_MEMORY_INFO		0x105c
#define SGX_BRIDGE_GETCLIENTINFO		0x105e
#define SGX_BRIDGE_RELEASECLIENTINFO	0x105f
#define SGX_BRIDGE_GETMISCINFO		0x106c
#define SGX_BRIDGE_CREATE_RENDER_CONTEXT	0x1084
#define SGX_BRIDGE_DESTROY_RENDER_CONTEXT	0x1085
#define SGX_BRIDGE_ADD_RENDER_TARGET	0x1086
#define SGX_BRIDGE_REMOVE_RENDER_TARGET	0x1087
#define SGX_BRIDGE_GET_RENDER_TARGET_DETAILS		0x1088
#define SGX_BRIDGE_GET_RENDER_TARGET_DRIVER_MEMBLOCK		0x1089
#define SGX_BRIDGE_UNSUPPORTED			0x108a	/* SGXUnsupportedBW */
#define SGX_BRIDGE_GET_RENDER_TARGET_INFO	0x108b
#define SGX_BRIDGE_DOKICK		0x108c
#define SGX_BRIDGE_CREATE_TRANSFER_QUEUE	0x108d
#define SGX_BRIDGE_DESTROY_TRANSFER_QUEUE	0x108e
#define SGX_BRIDGE_SUBMIT_TRANSFER_COMMANDS		0x108f
#define SGX_BRIDGE_WAIT_TRANSFER_QUEUE_IDLE		0x1090
#define SGX_BRIDGE_GET_TRACE_INFO		0x1091
#define SGX_BRIDGE_GET_TRACE_COUNTERS		0x1092
#define SGX_BRIDGE_RESET_SYNC_WAITS		0x1093

/* the process' services handle (one per process) */
typedef struct GpuServices {
	SceUInt32 reserved;
	SceUInt32 hServices;
} GpuServices;

typedef struct PVRSRV_DEV_DATA PVRSRV_DEV_DATA;

/* IMG services.h PVRSRV_CONNECTION, in SCE's layout */
typedef struct PVRSRV_CONNECTION {
	GpuServices *services;
	SceUInt32 numDevices;
	PVRSRV_DEVICE_IDENTIFIER device;
	int (*checkDevice)(PVRSRV_DEV_DATA *dev);	/* run once the device data is acquired */
	SceUInt32 reserved;			/* cleared without checkDevice */
	SceUInt32 ui32SrvFlags;			/* SceGpuEs4User_228252A2 (PVRSRVConnect) flags */
} PVRSRV_CONNECTION;

/* IMG services.h PVRSRV_DEV_DATA */
struct PVRSRV_DEV_DATA {
	PVRSRV_CONNECTION *psConnection;
	SceUInt32 hDevCookie;
};

/* the part of SGX_CLIENT_INFO the client keeps */
typedef struct GpuClientInfo {
	void *pvProcess;
	PVRSRV_MISC_INFO sMiscInfo;
	SceUInt32 asDevData[6];			/* SGX_CLIENT_INFO.asDevData[4..9] */
} GpuClientInfo;

/* the client's view of a render target, as SGX_MISC_INFO_REQUEST_SPM reads it */
typedef struct GpuSpmCounters {
	SceUInt32 reserved[3];
	SceUInt32 ui32NumOutOfMemSignals;
	SceUInt32 ui32NumSPMRenders;
} GpuSpmCounters;

typedef struct GpuRTDataSet {
	SceUInt8 reserved[0x74];
	const GpuSpmCounters *spm;
} GpuRTDataSet;

/* PVRSRVGetMappedMemoryInfo's answer */
typedef struct GpuMappedMemoryInfo {
	void *cpuAddress;
	SceSize size;
	SceUInt32 mmuFlags;
} GpuMappedMemoryInfo;

/* PDS program descriptor of SceGpuEs4User_E98914E4 / _A5E331FA */
typedef struct GpuPdsProgram {
	SceUInt32 program;		/* out: 16-byte aligned program address */
	SceUInt32 size;			/* out: program size */
	SceUInt32 constant[9];
} GpuPdsProgram;

/* gpu_user.c */
extern SceClibMspace g_gpuUserHeap;
extern SceUID g_gpuUserHeapMemBlock;
void *PVRSRVAllocUserModeMem(SceSize size);
void PVRSRVFreeUserModeMem(void *ptr);

/* services.c */
extern GpuServices g_gpuServices;
int GpuServicesConnect(GpuServices **services, SceUInt32 flags);
int GpuServicesDisconnect(GpuServices *services);
int GpuBridgeCall(GpuServices *services, SceUInt32 id, void *in, SceSize inSize, void *out,
		  SceSize outSize);

/* osfunc.c */
void *GpuMemCopyCommands(void *dst, const void *src, SceSize size);
void *PVRSRVMemSet(void *dst, int c, SceSize size);
void *PVRSRVMemCopy(void *dst, const void *src, SceSize size);

/* sgx.c */
int SgxCheckDdk(PVRSRV_DEV_DATA *dev);

/* pvrsrv.c */
extern PVRSRV_CONNECTION g_gpuConnection;
extern int g_gpuConnected;

/* SceGpuEs4User exports (names unknown) */
int SceGpuEs4User_228252A2(PVRSRV_CONNECTION **conn, SceUInt32 flags);	/* connect */
int SceGpuEs4User_A5B14369(PVRSRV_CONNECTION *conn);			/* disconnect */
int SceGpuEs4User_4447D217(PVRSRV_CONNECTION *conn, SceUInt32 *numDevices,
			   PVRSRV_DEVICE_IDENTIFIER *device);	/* enumerate devices */
int SceGpuEs4User_70800B46(PVRSRV_CONNECTION *conn, SceUInt32 index, PVRSRV_DEV_DATA *dev, SceUInt32 type);	/* acquire device data */
int SceGpuEs4User_73E945A1(PVRSRV_CONNECTION *conn, PVRSRV_MISC_INFO *info);	/* misc info */
int SceGpuEs4User_F4735A47(void);					/* release misc info */
int SceGpuEs4User_1E0A9C4A(PVRSRV_DEV_DATA *dev, SceUInt32 *hContext, SceUInt32 *numHeaps,
			   PVRSRV_HEAP_INFO *heaps);				/* create devmem context */
int SceGpuEs4User_B9178277(PVRSRV_DEV_DATA *dev, SceUInt32 hContext);	/* destroy devmem context */
int SceGpuEs4User_071E62F0(PVRSRV_DEV_DATA *dev, SceUInt32 hHeap, SceUInt32 flags, SceUInt32 size,
			   SceUInt32 alignment, SceUInt32 hMemBlockProcRef,
			   PVRSRV_CLIENT_MEM_INFO **memInfo);	/* alloc device memory */
int SceGpuEs4User_B6CFD213(PVRSRV_DEV_DATA *dev, PVRSRV_CLIENT_MEM_INFO *memInfo);	/* free device memory */
int SceGpuEs4User_56893E98(PVRSRV_DEV_DATA *dev, SceUInt32 hDevMemContext, SceUInt32 hDevMemHeap,
			   SceSize size, SceSize allocationSize, void *cpuAddress, SceUInt32 flags,
			   SceUInt32 *deviceAddress);			/* map memory to the GPU */
int SceGpuEs4User_AD20D1A0(PVRSRV_DEV_DATA *dev, void *cpuAddress, SceUInt32 hDevMemHeap,
			   SceUInt32 waitForIdle);			/* unmap memory from the GPU */
int SceGpuEs4User_0EA7458D(PVRSRV_DEV_DATA *dev, SceUID memBlockUid, SceUInt32 *blockGroupId,
			   SceUInt32 restoreMapping);			/* register a memblock */
int SceGpuEs4User_156A6B70(PVRSRV_DEV_DATA *dev, SceUID memBlockUid);	/* unregister a memblock */
int SceGpuEs4User_9A4A5DAE(PVRSRV_DEV_DATA *dev, void *cpuAddress, GpuMappedMemoryInfo *info);	/* mapped memory info */
int SceGpuEs4User_94286660(PVRSRV_DEV_DATA *dev, SceUInt32 *hSyncInfo);	/* alloc sync info */
int SceGpuEs4User_B26DF3D2(PVRSRV_DEV_DATA *dev, SceUInt32 hSyncInfo);	/* free sync info */
int SceGpuEs4User_416C8813(PVRSRV_CONNECTION *conn, SceUInt32 *hModObj);	/* create sync mod object */
int SceGpuEs4User_7DCCBCAF(PVRSRV_CONNECTION *conn, SceUInt32 hModObj);	/* destroy sync mod object */
int SceGpuEs4User_0D27286F(PVRSRV_CONNECTION *conn, SceUInt32 hModObj, const SceUInt32 *hSyncInfos,
			   int count, SceUInt32 flags, SceUInt32 *writeOps, SceUInt32 *readOps);	/* modify pending sync ops */
int SceGpuEs4User_116449D2(PVRSRV_CONNECTION *conn, SceUInt32 hModObj);	/* modify complete sync ops */
int SceGpuEs4User_AAD85460(PVRSRV_CONNECTION *conn, SceUInt32 hModObj, SceUInt32 wait);	/* flush sync ops to mod object */
int SceGpuEs4User_5C3A6DFE(SceUInt32 hModObj, SceUInt32 timeout);	/* wait for a mod object */
int SceGpuEs4User_B000CA65(PVRSRV_DEV_DATA *dev, GpuClientInfo *info);	/* SGX client info */
int SceGpuEs4User_5273D097(PVRSRV_DEV_DATA *dev, const GpuClientInfo *info);	/* release SGX client info */
int SceGpuEs4User_09BB98B2(PVRSRV_DEV_DATA *dev, SGX_MISC_INFO *info);	/* SGX misc info */
int SceGpuEs4User_8A997DBB(PVRSRV_DEV_DATA *dev, const SGX_CREATERENDERCONTEXT *params,
			   SceUInt32 *hRenderContext,
			   PVRSRV_CLIENT_MEM_INFO **visTestResultMemInfo);	/* create render context */
int SceGpuEs4User_0BFE1685(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderContext, SceUInt32 unused,
			   SceUInt32 forceCleanup);				/* destroy render context */
int SceGpuEs4User_0162A405(PVRSRV_DEV_DATA *dev, const SGX_ADDRENDTARG *target,
			   SceUInt32 *hRenderTarget);				/* add a render target */
int SceGpuEs4User_D90B0707(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderContext,
			   SceUInt32 hRenderTarget);				/* remove a render target */
int SceGpuEs4User_CCEE0336(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderTarget,
			   SGX_RENDER_TARGET_DETAILS *details);			/* render target details */
int SceGpuEs4User_804BA13D(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderTarget, SceUID *dataMemblockUid,
			   SceUInt32 *useExternalUid);				/* render target driver memblock */
int SceGpuEs4User_9E8E2957(PVRSRV_DEV_DATA *dev, SceUInt32 arg, SceUInt32 *result);	/* unsupported */
int SceGpuEs4User_83D972DF(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderTarget,
			   SGX_RENDER_TARGET_INFO *info);			/* render target info */
int SceGpuEs4User_F0CCE9DB(PVRSRV_DEV_DATA *dev, const SGX_KICKTA *kick,
			   SGX_KICKTA_OUTPUT *output);				/* kick a scene */
int SceGpuEs4User_BDE2B9E1(PVRSRV_DEV_DATA *dev, SceUInt32 reservedParameter,
			   const GpuTransferMemoryContext *memoryContext,
			   SceUInt32 *hContext);				/* create a transfer queue */
int SceGpuEs4User_8ED7D698(PVRSRV_DEV_DATA *dev, SceUInt32 hContext, SceUInt32 forceCleanup);	/* destroy a transfer queue */
int SceGpuEs4User_5564C1BE(const SceUInt32 *commands, SceUInt32 numCommands, PVRSRV_DEV_DATA *dev,
			   SceUInt32 hContext, SceUInt32 hSyncInfo, SceUInt32 sourceSyncEnable,
			   SceUInt32 destinationSyncEnable,
			   const GpuTransferStatus *status);			/* submit transfer commands */
int SceGpuEs4User_7E965369(PVRSRV_DEV_DATA *dev, SceUInt32 hContext);	/* wait for an idle transfer queue */
int SceGpuEs4User_768482F6(PVRSRV_DEV_DATA *dev, GpuTraceInfo *info);	/* trace info */
int SceGpuEs4User_C0D16CE5(PVRSRV_DEV_DATA *dev, GpuTraceCounters *counters);	/* trace counters */
int SceGpuEs4User_9D5C4326(PVRSRV_DEV_DATA *dev);				/* reset sync waits */
void *SceGpuEs4User_7AF40138(SceSize size);				/* user heap allocation */
void SceGpuEs4User_1BE5985A(void *ptr);					/* user heap free */
SceUInt32 *SceGpuEs4User_E98914E4(GpuPdsProgram *pds, SceUInt32 *buffer, int textured,
				  int perspective, int variant);		/* PDS pixel program */
SceUInt32 *SceGpuEs4User_A5E331FA(GpuPdsProgram *pds, void *buffer);	/* PDS state update program */
SceUInt32 *SceGpuEs4User_1A575DFD(SceUInt32 *out);			/* USSE: end of program */
SceUInt32 *SceGpuEs4User_86FC3202(SceUInt32 *out);			/* USSE: fixed 4-instruction sequence */
SceUInt32 *SceGpuEs4User_53CACDAB(SceUInt32 *out, const SceUInt32 *address, SceUInt32 count);	/* USSE: load sequence */

/* SceGpuEs4UserForVsh exports (names unknown) */
int SceGpuEs4UserForVsh_3C09BFCA(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey, SceUInt32 hSyncInfo);	/* create shared sync */
int SceGpuEs4UserForVsh_8A27AB0A(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey, SceUInt32 *hSyncInfo);	/* open shared sync */
int SceGpuEs4UserForVsh_0A9556D9(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey, SceUInt32 hSyncInfo);	/* close shared sync */
int SceGpuEs4UserForVsh_010DA2CB(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey);	/* delete shared sync */
int SceGpuEs4UserForVsh_3DC96093(PVRSRV_DEV_DATA *dev, SceUInt32 hDevMemContext,
				 SceUInt32 deviceAddress, SceSize size,
				 SceUInt32 access);				/* check a GPU mapping */
