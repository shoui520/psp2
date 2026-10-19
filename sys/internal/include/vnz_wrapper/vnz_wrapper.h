/*
 * SceCodecEngineWrapper (os0:kd/vnz_wrapper.skprx): SceCodecEngineWrapperForDriver, the codec
 * engine (Venezia, the DSP0 media processor, and the VIP) as other modules call it, each export
 * declared once with its definition as the prototype. The host capture exports are in
 * venezia_debug.h.
 */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/procevent.h>
#include <vnz_wrapper/venezia_rpc.h>

/* a process exit or kill handler a codec driver registers */
typedef int (*CodecEngineProcExitHandler)(SceUID pid, ProcEventParam *param, void *common);

/* codecengine/codecengine.c */
int sceCodecEngineRegisterProcExitHandler(CodecEngineProcExitHandler handler);
int sceCodecEngineRegisterProcKillHandler(CodecEngineProcExitHandler handler);

/* codecengine/memory.c: unmapped memory a process shares with the codec engine */
int sceCodecEngineRegisterUnmapMemory(SceUID pid, SceUIntVAddr address, SceSize size);
int sceCodecEngineUnregisterUnmapMemory(SceUID pid, SceUIntVAddr address, SceSize size);
int sceCodecEngineIsUnmapMemory(SceUID pid, const void *address, SceSize size);
int sceCodecEngineUnmapUserVAtoPA(SceUID pid, SceUIntVAddr address, uintptr_t *physicalAddress);
int sceCodecEngineUnmapPAtoUserVA(SceUID pid, uintptr_t address, SceUIntVAddr *userAddress);
int sceCodecEngineUnmapUserVAtoKernelVA(SceUID pid, const void *address, void **kernelAddress);
int sceCodecEngineUnmapKernelVAtoUserVA(SceUID pid, const void *address, void **userAddress);

/* dsp0_mailbox.c: the DSP's heaps (RPC commands 496 to 502) */
void *SceCodecEngineWrapperForDriver_2DD83BB0(unsigned int heap, SceSize size);
int SceCodecEngineWrapperForDriver_3C5E9CC3(unsigned int heap, void *memory, SceSize size);
void *SceCodecEngineWrapperForDriver_91535820(unsigned int heap, SceSize size);
int SceCodecEngineWrapperForDriver_3A79A935(unsigned int heap, void *memory, SceSize size);
void *SceCodecEngineWrapperForDriver_0F235F44(uintptr_t physical, SceSize size);
uintptr_t SceCodecEngineWrapperForDriver_26FAF6EC(void *memory, SceSize size);

/* venezia/dsp0_rpc.c */
int SceCodecEngineWrapperForDriver_391BD4FD(unsigned int command, const unsigned int *input,
		VeneziaRpcPack pack, VeneziaRpcUnpack unpack);
int SceCodecEngineWrapperForDriver_0D3BB164(unsigned int command, unsigned int argument0,
		unsigned int argument1, unsigned int argument2, unsigned int argument3);
/*
 * SceCodecEngineWrapperForDriver_0D3BB164 for a caller that passes fewer than four argument
 * words: the DSP gets the rest as the caller left the registers (name ours).
 */
int veneziaBlockingRpc(unsigned int command, ...) __asm__("SceCodecEngineWrapperForDriver_0D3BB164");

/* venezia/system.c */
int sceCodecEngineDsp0GetThreadExecGroup(unsigned int thread);

/* venezia/vapa.c: DSP addresses, and the memory an RPC passes */
void *SceCodecEngineWrapperForDriver_F9495589(uintptr_t physical);
uintptr_t SceCodecEngineWrapperForDriver_C7C150AE(const void *address);
int sceCodecEngineDsp0ConvertVirtualToPhysical(VeneziaRpcThunk *thunk, void *base, SceSize size,
		const VeneziaMemoryOption *option);
int sceCodecEngineDsp0ConvertPhysicalToVirtual(VeneziaRpcThunk *thunk);
int sceCodecEngineDsp0AllocCommunicationMemories(VeneziaRpcThunk *thunk);
int sceCodecEngineDsp0FreeCommunicationMemories(VeneziaRpcThunk *thunk);
int sceCodecEngineDsp0MemcpyChain(VeneziaRpcThunk *thunk);

/* venezia/venezia.c: a process's codec use holds off its suspend */
int SceCodecEngineWrapperForDriver_CB8703D8(void);	/* Sony's record: _lockProcessSuspendCore */
int _unlockProcessSuspendCore(void);
int sceCodecEngineDsp0IsProcessSuspended(SceUID pid);

/* vip/hwd0_vapa.c */
int SceCodecEngineWrapperForDriver_FAF590A6(void *destination, const void *source, SceSize size);
