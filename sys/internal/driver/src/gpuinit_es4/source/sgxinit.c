/*
 * SceGpuEs4Init: the SGX init server. For each SGX device it allocates the
 * microkernel's control blocks and buffers, loads and links its programs,
 * builds the register scripts and hands everything to SceGpuEs4
 * (DevInitSGXPart2KM); then it finishes the services' initialisation.
 */

#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/kernel/aimgr.h>
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "sce/gpuinit_es4.h"
#include "sce/perfmon_regs.h"
#include "../../../../../../contrib/img-sgx-ddk-1.7.862890/services4/srvkm/hwdefs/sgx543_v1.164defs.h"
#include "../../../../../../contrib/img-sgx-ddk-1.7.862890/services4/srvkm/hwdefs/sgxmpdefs.h"
#include "programs.h"

/* Debug records use Sony's relative source path, including its directories. */
#undef __FILE_NAME__
#define __FILE_NAME__ "devices/sgx/sgxinit.c"

/* services.h */
#define PVRSRV_MAX_CLIENT_HEAPS		32
#define HEAP_IDX(id)			((id) & ((1 << 24) - 1))

/* heap indices, IMG's from sgxapi_km.h */
#define SGX_KERNEL_CODE_HEAP_ID		2
#define SGX_KERNEL_DATA_HEAP_ID		3
#define SGX_PIXELSHADER_HEAP_ID		4
#define SGX_VERTEXSHADER_HEAP_ID	5
#define SGX_PDSPIXEL_CODEDATA_HEAP_ID	6
/* and SCE's own */
#define SGX_USE_CODE_BASE9_HEAP_ID	0x12
#define SGX_USE_CODE_BASE8_HEAP_ID	0x13
#define SGX_PERFMON_HEAP_ID		0x15

/* the data master in a USE code base register */
#define USE_CODE_BASE_DM(dm)		((dm) << EUR_CR_USE_CODE_BASE_DM_00_SHIFT)

/* memory info attributes of the allocations */
#define SGX_KERNEL_DATA_ATTRIBS		0x40804f
#define SGX_KERNEL_CTL_ATTRIBS		0x42804f
#define SGX_KERNEL_BUFFER_ATTRIBS	0x40800f
#define SGX_KERNEL_CODE_ATTRIBS		0x408009

#define SGX_PERFMON_USER_VADDR		0x8000000
#define SGX_PERFMON_KERNEL_VADDR	0x8001000
#define SGX_PERFMON_PAGE_SIZE		0x1000	/* a user page, then a kernel page */
#define SGX_BOOKMARK_VADDR		0x8002000
#define SGX_BOOKMARK_SIZE		0x8000
#define SGX_BOOKMARK_FILL		0x7a207123

/* a script being built */
typedef struct SgxScriptBuild {
	IMG_UINT32 ui32MaxLen;
	IMG_UINT32 ui32CurrPos;
	IMG_BOOL bOutOfSpace;
	SGX_INIT_COMMAND *psCommands;
} SgxScriptBuild;

/* the heaps the init server uses, out of SGX_BRIDGE_INFO_FOR_SRVINIT */
typedef struct SgxInitHeaps {
	IMG_DEV_PHYADDR sPDDevPAddr;
	PVRSRV_HEAP_INFO *kernelCode;
	PVRSRV_HEAP_INFO *kernelData;
	PVRSRV_HEAP_INFO *pixelShader;
	PVRSRV_HEAP_INFO *vertexShader;
	PVRSRV_HEAP_INFO *useCodeBase9;
	PVRSRV_HEAP_INFO *useCodeBase8;
	PVRSRV_HEAP_INFO *perfmon;
	PVRSRV_HEAP_INFO *pdsPixel;
} SgxInitHeaps;

static SceKernelModuleInfo moduleInfo __attribute__((aligned(16)));

SGX_INIT_COMMAND *nextScriptCommand(SgxScriptBuild *script) /* name unknown */
{
	IMG_UINT32 pos = script->ui32CurrPos;
	SGX_INIT_COMMAND *command;

	if (pos >= script->ui32MaxLen)
		script->bOutOfSpace = IMG_TRUE;
	if (script->bOutOfSpace)
		return NULL;
	command = &script->psCommands[pos];
	script->ui32CurrPos = pos + 1;
	return command;
}

void scriptWriteSGXReg(SgxScriptBuild *script, IMG_UINT32 offset,
			      IMG_UINT32 value) /* name unknown */
{
	SGX_INIT_COMMAND *command = nextScriptCommand(script);

	if (command) {
		command->sWriteHWReg.eOp = SGX_INIT_OP_WRITE_HW_REG;
		command->sWriteHWReg.ui32Offset = offset;
		command->sWriteHWReg.ui32Value = value;
	}
}

/* patch the 32-bit immediate of a USE limm instruction */
void patchUSEImmediate(IMG_UINT32 *instruction, IMG_UINT32 value) /* name unknown */
{
	instruction[0] = (instruction[0] & 0xffe00000) | (value & 0x1fffff);
	instruction[1] = (instruction[1] & 0xfffc0e0f) | ((value >> 17) & 0x1f0) |
			 ((value >> 26) << 12);
}

PVRSRV_ERROR allocKernelDataZeroed(IMG_HANDLE device, PVRSRV_PER_PROCESS_DATA *process,
					  PVRSRV_HEAP_INFO *heap, IMG_SIZE_T size,
					  IMG_SIZE_T alignment,
					  PVRSRV_KERNEL_MEM_INFO **memInfo) /* name unknown */
{
	PVRSRV_KERNEL_MEM_INFO *mem;
	PVRSRV_ERROR ret;

	ret = PVRSRVAllocDeviceMemKM(device, process, heap->hDevMemHeap, SGX_KERNEL_DATA_ATTRIBS,
				     size, alignment, 1, memInfo);
	if (ret != PVRSRV_OK)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	mem = *memInfo;
	OSMemSet(mem->pvLinAddrKM, 0, mem->uAllocSize);
	return ret;
}

PVRSRV_ERROR allocKernelBufferZeroed(IMG_HANDLE device, PVRSRV_PER_PROCESS_DATA *process,
					    PVRSRV_HEAP_INFO *heap, IMG_SIZE_T size,
					    IMG_SIZE_T alignment,
					    PVRSRV_KERNEL_MEM_INFO **memInfo) /* name unknown */
{
	PVRSRV_KERNEL_MEM_INFO *mem;
	PVRSRV_ERROR ret;

	ret = PVRSRVAllocDeviceMemKM(device, process, heap->hDevMemHeap, SGX_KERNEL_BUFFER_ATTRIBS,
				     size, alignment, 1, memInfo);
	if (ret != PVRSRV_OK)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	mem = *memInfo;
	OSMemSet(mem->pvLinAddrKM, 0, mem->uAllocSize);
	return ret;
}

/* add a USE code address, relative to its code base, to a PDS DOUTU word */
void setUSEExecutionAddress(IMG_UINT32 *doutu, IMG_UINT32 address,
				   IMG_UINT32 codeBase) /* name unknown */
{
	IMG_UINT32 offset = address - codeBase;

	*doutu |= ((offset << 1) & 0x00fffff0) | (((offset >> 23) + 15) & 15);
}

static void sceGpuMapPARegisters(IMG_HANDLE device, PVRSRV_HEAP_INFO *heap)
{
	SceKernelAllocMemBlockKernelOpt opt;
	IMG_UINT32 *bookmark = NULL;
	GpuHeap *gpuHeap;
	GpuMmuContext *context;
	void *perfmon;
	SceUInt32 flags;
	SceUID uid;
	int ret;

	if (!device || !heap || !heap->hDevMemHeap)
		return;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x200000;
	uid = ksceKernelAllocMemBlock("SceRazorGpuBmkWar", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW,
				      SGX_BOOKMARK_SIZE, &opt);
	if (uid < 0) {
		SCE_DBG_PRINTF(0, 5304, "%s: Failed to allocate %d bytes for SceRazorGpuBmkWar with error %08x.\n",
			       __func__, SGX_BOOKMARK_SIZE, uid);
		return;
	}
	ret = ksceKernelGetMemBlockBase(uid, (void **)&bookmark);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 5311, "%s: Failed to get memory block base for SceRazorGpuBmkWar with error %08x.\n",
			       __func__, ret);
		goto free;
	}
	for (unsigned i = 0; i < SGX_BOOKMARK_SIZE / 4; i++)
		bookmark[i] = SGX_BOOKMARK_FILL;

	if (ksceSblAimgrIsTool() && ksceSblACMgrIsDevelopmentMode()) {
		perfmon = (void *)PERFMON_REG_BASE;
		flags = 0x80000008;
	} else {
		perfmon = bookmark;
		flags = 0;
	}
	gpuHeap = heap->hDevMemHeap;
	context = &gpuHeap->memoryContext->mmu;
	ret = ksceGpuMmuMapMemory(context, SGX_PERFMON_USER_VADDR, perfmon, SGX_PERFMON_PAGE_SIZE, flags);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 5369, "%s: Failed to map user-land Perfmon registers with error %08x.\n", __func__, ret);
		goto unmap;
	}
	ret = ksceGpuMmuMapMemory(context, SGX_PERFMON_KERNEL_VADDR,
				  (SceUInt8 *)perfmon + SGX_PERFMON_PAGE_SIZE, SGX_PERFMON_PAGE_SIZE, flags | 0x10);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 5377, "%s: Failed to map kernel-land Perfmon registers with error %08x.\n", __func__, ret);
		goto unmap;
	}

	flags = 0;
	if (ksceSblAimgrIsTool() && ksceSblACMgrIsDevelopmentMode())
		flags = 8;
	ret = ksceGpuMmuMapMemory(context, SGX_BOOKMARK_VADDR, bookmark, SGX_BOOKMARK_SIZE, flags);
	if (ret >= 0)
		return;
	SCE_DBG_PRINTF(0, 5388, "%s: Failed to map 32K for GPU Bookmark emit workaround with error %08x.\n", __func__, ret);
unmap:
	ksceGpuMmuUnmapMemory(context, SGX_PERFMON_USER_VADDR, SGX_PERFMON_PAGE_SIZE);
	ksceGpuMmuUnmapMemory(context, SGX_PERFMON_KERNEL_VADDR, SGX_PERFMON_PAGE_SIZE);
	ksceGpuMmuUnmapMemory(context, SGX_BOOKMARK_VADDR, SGX_BOOKMARK_SIZE);
free:
	ksceKernelFreeMemBlock(uid);
}

PVRSRV_ERROR SGXInitialisePart2KM(PVRSRV_PER_PROCESS_DATA *process,
								  const PVRSRV_DEVICE_IDENTIFIER *devId)
{
	static const IMG_UINT32 cores[] = {0, 2, 4, 6};
	IMG_HANDLE device;
	SgxInitHeaps heaps;
	SGX_BRIDGE_INFO_FOR_SRVINIT *info;
	SGX_BRIDGE_INIT_INFO *init;
	SGXMKIF_TA3D_CTL *ta3dCtl;
	PVRSRV_KERNEL_MEM_INFO *mem, *microkernel;
	SGX_INIT_COMMAND *command;
	SgxScriptBuild part1 = {SGX_MAX_INIT_COMMANDS, 0, IMG_FALSE, NULL};
	SgxScriptBuild part2 = {SGX_MAX_INIT_COMMANDS, 0, IMG_FALSE, NULL};
	SgxScriptBuild deinit = {SGX_MAX_DEINIT_COMMANDS, 0, IMG_FALSE, NULL};
	IMG_UINT32 doutu[3];			/* the PDS DOUTU words of program2 */
	IMG_UINT32 eventUSE[3];			/* program3 to program5 */
	IMG_UINT32 startUSE;			/* program6 */
	IMG_UINT32 entryUSE[4];			/* program6, then program7 */
	IMG_UINT32 *program, *data;
	IMG_UINT32 pixelPDSExec, corePDSExec, coreMask, revision, i;
	PVRSRV_ERROR ret;

	ret = PVRSRVAcquireDeviceDataKM(devId->ui32DeviceIndex, PVRSRV_DEVICE_TYPE_UNKNOWN, &device);
	if (ret != PVRSRV_OK)
		return ret;
	info = OSAllocMem(sizeof(*info));
	if (!info)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	ret = SGXGetInfoForSrvinitKM(device, info);
	if (ret != PVRSRV_OK)
		goto freeInfo;

	OSMemSet(&heaps, 0, sizeof(heaps));
	heaps.sPDDevPAddr = info->sPDDevPAddr;
	for (i = 0; i < PVRSRV_MAX_CLIENT_HEAPS; i++) {
		PVRSRV_HEAP_INFO *heap = &info->asHeapInfo[i];

		switch (HEAP_IDX(heap->ui32HeapID)) {
		case SGX_KERNEL_CODE_HEAP_ID:
			heaps.kernelCode = heap;
			break;
		case SGX_KERNEL_DATA_HEAP_ID:
			heaps.kernelData = heap;
			break;
		case SGX_PIXELSHADER_HEAP_ID:
			heaps.pixelShader = heap;
			break;
		case SGX_VERTEXSHADER_HEAP_ID:
			heaps.vertexShader = heap;
			break;
		case SGX_PDSPIXEL_CODEDATA_HEAP_ID:
			heaps.pdsPixel = heap;
			break;
		case SGX_USE_CODE_BASE9_HEAP_ID:
			heaps.useCodeBase9 = heap;
			break;
		case SGX_USE_CODE_BASE8_HEAP_ID:
			heaps.useCodeBase8 = heap;
			break;
		case SGX_PERFMON_HEAP_ID:
			heaps.perfmon = heap;
			break;
		}
	}

	init = OSAllocMem(sizeof(*init));
	if (!init) {
		ret = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto freeInfo;
	}
	part1.psCommands = init->sScripts.asInitCommandsPart1;
	part2.psCommands = init->sScripts.asInitCommandsPart2;
	deinit.psCommands = init->sScripts.asDeinitCommands;

	init->ui32ClientBuildOptions = 0x30322518;
	init->sSGXStructSizes.ui32Sizeof_2DCMD = 0xe8;
	init->sSGXStructSizes.ui32Sizeof_2DCMD_SHARED = 0x70;
	init->sSGXStructSizes.ui32Sizeof_CMDTA = 0x2a4;
	init->sSGXStructSizes.ui32Sizeof_CMDTA_SHARED = 0x1e0;
	init->sSGXStructSizes.ui32Sizeof_TRANSFERCMD = 0x4c0;
	init->sSGXStructSizes.ui32Sizeof_TRANSFERCMD_SHARED = 0xdc;
	init->sSGXStructSizes.ui32Sizeof_3DREGISTERS = 0x104;
	init->sSGXStructSizes.ui32Sizeof_HWPBDESC = 0x3c;
	init->sSGXStructSizes.ui32Sizeof_HWRENDERCONTEXT = 0x6c;
	init->sSGXStructSizes.ui32Sizeof_HWRENDERDETAILS = 0x220;
	init->sSGXStructSizes.ui32Sizeof_HWRTDATA = 0x84;
	init->sSGXStructSizes.ui32Sizeof_HWRTDATASET = 0x9c;
	init->sSGXStructSizes.ui32Sizeof_HWTRANSFERCONTEXT = 0xb4;
	init->sSGXStructSizes.ui32Sizeof_HOST_CTL = sizeof(struct SGXMKIF_HOST_CTL);
	init->sSGXStructSizes.ui32Sizeof_COMMAND = sizeof(SGXMKIF_COMMAND);
	init->ui32DevData8 = 0;
	init->ui32PixelShaderHeapBase = heaps.pixelShader->sDevVAddrBase.uiAddr;
	init->ui32DevData4 = 2;
	init->ui32EDMTaskReg0 = 0xb10;
	init->ui32EDMTaskReg1 = 0xb1c;
	init->ui32ClkGateCtl = 0x2aaaaa;
	init->ui32ClkGateCtl2 = 0xa8a8aaa;
	init->ui32ClkGateStatusReg = EUR_CR_CLKGATESTATUS;
	init->ui32ClkGateStatusMask = 0xffff77ff;
	init->ui32MasterClkGateStatusReg = 0x4008;
	init->ui32MasterClkGateStatusMask = 15;
	init->ui32MasterClkGateStatus2Reg = 0x4024;
	init->ui32MasterClkGateStatus2Mask = 30;

	/* the kernel CCB, its control blocks and the microkernel's control blocks */
	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelData->hDevMemHeap, SGX_KERNEL_DATA_ATTRIBS,
				   0x2000, 0x1000, 1, &init->hKernelCCBMemInfo) != PVRSRV_OK)
		goto allocFail;
	init->pvKernelCCB = init->hKernelCCBMemInfo->pvLinAddrKM;
	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelData->hDevMemHeap, SGX_KERNEL_DATA_ATTRIBS,
				   8, 0x1000, 1, &init->hKernelCCBCtlMemInfo) != PVRSRV_OK)
		goto allocFail;
	init->pvKernelCCBCtl = init->hKernelCCBCtlMemInfo->pvLinAddrKM;
	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelData->hDevMemHeap, SGX_KERNEL_DATA_ATTRIBS,
				   4, 0x10, 1, &init->hKernelCCBEventKickerMemInfo) != PVRSRV_OK ||
	    PVRSRVAllocDeviceMemKM(device, process, heaps.kernelData->hDevMemHeap, SGX_KERNEL_CTL_ATTRIBS,
				   sizeof(SGXMKIF_TA3D_CTL), 0x20, 1, &init->hKernelSGXTA3DCtlMemInfo) != PVRSRV_OK ||
	    PVRSRVAllocDeviceMemKM(device, process, heaps.kernelData->hDevMemHeap, SGX_KERNEL_CTL_ATTRIBS,
				   sizeof(struct SGXMKIF_HOST_CTL), 0x20, 1, &init->hKernelSGXHostCtlMemInfo) != PVRSRV_OK)
		goto allocFail;

	mem = init->hKernelSGXTA3DCtlMemInfo;
	OSMemSet(mem->pvLinAddrKM, 0, sizeof(SGXMKIF_TA3D_CTL));
	ta3dCtl = mem->pvLinAddrKM;
	ta3dCtl->sTA3DCtlDevAddr = init->hKernelSGXTA3DCtlMemInfo->sDevVAddr;
	ta3dCtl->sHostCtlDevAddr = init->hKernelSGXHostCtlMemInfo->sDevVAddr;
	ta3dCtl->ui32Mode = 2;
	ta3dCtl->sKernelCCBCtlDevAddr = init->hKernelCCBCtlMemInfo->sDevVAddr;
	ta3dCtl->sKernelCCBDevAddr = init->hKernelCCBMemInfo->sDevVAddr;
	ta3dCtl->sPDDevPAddr = heaps.sPDDevPAddr;
	OSMemSet(init->hKernelSGXHostCtlMemInfo->pvLinAddrKM, 0, sizeof(struct SGXMKIF_HOST_CTL));

	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelData->hDevMemHeap, SGX_KERNEL_CTL_ATTRIBS,
				   0x60, 0x20, 1, &init->hKernelSGXMiscMemInfo) != PVRSRV_OK)
		goto allocFail;
	OSMemSet(init->hKernelSGXMiscMemInfo->pvLinAddrKM, 0, 0x60);

	/* the TA and 3D signature buffers: the register count, then the registers */
	ret = allocKernelDataZeroed(device, process, heaps.kernelData, 0x418, 0x20,
				    &init->hKernelTASigBufferMemInfo);
	if (ret != PVRSRV_OK)
		goto freeInit;
	ret = allocKernelDataZeroed(device, process, heaps.kernelData, 0x724, 0x20,
				    &init->hKernel3DSigBufferMemInfo);
	if (ret != PVRSRV_OK)
		goto freeInit;
	data = init->hKernelTASigBufferMemInfo->pvLinAddrKM;
	data[0] = 4;
	data[1] = 0;
	data[2] = 0x900;
	data[3] = 0x904;
	data[4] = 0x908;
	data[5] = 0x90c;
	data = init->hKernel3DSigBufferMemInfo->pvLinAddrKM;
	data[0] = 7;
	data[1] = 0;
	data[2] = 0x910;
	data[3] = 0x914;
	data[4] = 0x918;
	data[5] = 0x91c;
	data[6] = 0x920;
	data[7] = 0x924;
	data[8] = 0x30c;
	ta3dCtl->sTASigBufferDevAddr = init->hKernelTASigBufferMemInfo->sDevVAddr;
	ta3dCtl->s3DSigBufferDevAddr = init->hKernel3DSigBufferMemInfo->sDevVAddr;

	init->aui32HostKickAddr[SGXMKIF_CMD_TA] = 0xf6;
	init->aui32HostKickAddr[SGXMKIF_CMD_TRANSFER] = 0xdc;
	init->aui32HostKickAddr[SGXMKIF_CMD_2D] = 0xc1;
	init->aui32HostKickAddr[SGXMKIF_CMD_POWER] = 0x8a;
	init->aui32HostKickAddr[SGXMKIF_CMD_CONTEXTSUSPEND] = 0xa1;
	init->aui32HostKickAddr[SGXMKIF_CMD_CLEANUP] = 0x113;
	init->aui32HostKickAddr[SGXMKIF_CMD_GETMISCINFO] = 0x151;
	init->aui32HostKickAddr[SGXMKIF_CMD_PROCESS_QUEUES] = 0x130;
	init->aui32HostKickAddr[SGXMKIF_CMD_DATABREAKPOINT] = 0x18e;
	init->aui32HostKickAddr[SGXMKIF_CMD_SETHWPERFSTATUS] = 0x131;

	/* the microkernel and its PDS programs, in the kernel code heap */
	startUSE = 0;
	entryUSE[0] = entryUSE[1] = entryUSE[2] = entryUSE[3] = 0;
	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap, SGX_KERNEL_CODE_ATTRIBS,
				   sizeof(program0), 8, 1, &microkernel) != PVRSRV_OK)
		goto allocFail;
	OSMemCopy(microkernel->pvLinAddrKM, program0, sizeof(program0));
	program = microkernel->pvLinAddrKM;
	program[0x1bf0 / 4] |= 0x11e8;
	init->hKernelProgramMemInfo[0] = microkernel;

	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap, SGX_KERNEL_CODE_ATTRIBS,
				   sizeof(program1), 8, 1, &init->hKernelProgramMemInfo[1]) != PVRSRV_OK)
		goto allocFail;
	mem = init->hKernelProgramMemInfo[1];
	OSMemCopy(mem->pvLinAddrKM, program1, sizeof(program1));
	data = mem->pvLinAddrKM;
	data[0x70 / 4] |= ((mem->sDevVAddr.uiAddr + 0x40 - heaps.kernelCode->sDevVAddrBase.uiAddr) >> 3) & 0xfffff;

	doutu[0] = 0x8000000;
	doutu[1] = 1;
	doutu[2] = 0;
	setUSEExecutionAddress(&doutu[0], mem->sDevVAddr.uiAddr, heaps.kernelCode->sDevVAddrBase.uiAddr);
	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap, SGX_KERNEL_CODE_ATTRIBS,
				   sizeof(program2), 0x10, 1, &init->hKernelProgramMemInfo[2]) != PVRSRV_OK)
		goto allocFail;
	mem = init->hKernelProgramMemInfo[2];
	OSMemCopy(mem->pvLinAddrKM, program2, sizeof(program2));
	data = mem->pvLinAddrKM;
	data[8] = doutu[0];
	data[9] = doutu[1];
	data[2] = doutu[2];
	data[0] = 0;
	data[1] = 0x100;
	data[4] = init->hKernelSGXTA3DCtlMemInfo->sDevVAddr.uiAddr;
	data[5] = 0x200;
	pixelPDSExec = mem->sDevVAddr.uiAddr & 0xfffffff0;

	/* a failure here does not stop the programs that follow */
	eventUSE[0] = eventUSE[1] = eventUSE[2] = 0;
	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap, SGX_KERNEL_CODE_ATTRIBS,
				   sizeof(program3), 0x10, 1, &init->hKernelProgramMemInfo[3]) == PVRSRV_OK) {
		data = init->hKernelProgramMemInfo[3]->pvLinAddrKM;
		OSMemCopy(data, program3, sizeof(program3));
		setUSEExecutionAddress(&eventUSE[0], microkernel->sDevVAddr.uiAddr + 0xd10,
				       heaps.kernelCode->sDevVAddrBase.uiAddr);
		data[0] = eventUSE[0] | 0x8000000;
		data[1] = 1;
		data[2] = 0;
		if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap, SGX_KERNEL_CODE_ATTRIBS,
					   sizeof(program4), 0x10, 1, &init->hKernelProgramMemInfo[4]) == PVRSRV_OK) {
			data = init->hKernelProgramMemInfo[4]->pvLinAddrKM;
			OSMemCopy(data, program4, sizeof(program4));
			setUSEExecutionAddress(&eventUSE[1], microkernel->sDevVAddr.uiAddr + 0xda0,
					       heaps.kernelCode->sDevVAddrBase.uiAddr);
			data[0] = init->hKernelSGXTA3DCtlMemInfo->sDevVAddr.uiAddr;
			data[1] = 0x3f;
			data[4] = eventUSE[1];
			data[5] = 1;
			data[2] = 0;
			if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap,
						   SGX_KERNEL_CODE_ATTRIBS, sizeof(program5), 0x10, 1,
						   &init->hKernelProgramMemInfo[5]) == PVRSRV_OK) {
				data = init->hKernelProgramMemInfo[5]->pvLinAddrKM;
				OSMemCopy(data, program5, sizeof(program5));
				setUSEExecutionAddress(&eventUSE[2], microkernel->sDevVAddr.uiAddr + 0xdd0,
						       heaps.kernelCode->sDevVAddrBase.uiAddr);
				data[0] = eventUSE[2] | 0x8000000;
				data[1] = 1;
				data[2] = 0;
			}
		}
	}

	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap, SGX_KERNEL_CODE_ATTRIBS,
				   sizeof(program6), 0x10, 1, &init->hKernelProgramMemInfo[6]) != PVRSRV_OK)
		goto allocFail;
	mem = init->hKernelProgramMemInfo[6];
	OSMemCopy(mem->pvLinAddrKM, program6, sizeof(program6));
	setUSEExecutionAddress(&startUSE, microkernel->sDevVAddr.uiAddr + 0x30,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	data = mem->pvLinAddrKM;
	data[0] = 0;
	data[1] = 0x100;
	data[4] = 1;
	data[5] = 0;
	data[10] = startUSE | 0x8000000;
	setUSEExecutionAddress(&entryUSE[0], microkernel->sDevVAddr.uiAddr + 0x3580,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	setUSEExecutionAddress(&entryUSE[1], microkernel->sDevVAddr.uiAddr + 0x8ff0,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	setUSEExecutionAddress(&entryUSE[2], microkernel->sDevVAddr.uiAddr + 0xafb0,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	setUSEExecutionAddress(&entryUSE[3], microkernel->sDevVAddr.uiAddr + 0xecf0,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	data[6] = entryUSE[0] | 0x8000000;
	data[7] = entryUSE[1] | 0x8000000;
	data[3] = entryUSE[2] | 0x8000000;
	data[2] = entryUSE[3] | 0x8000000;

	entryUSE[0] = entryUSE[1] = entryUSE[2] = entryUSE[3] = 0;
	setUSEExecutionAddress(&entryUSE[0], microkernel->sDevVAddr.uiAddr + 0x3610,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	setUSEExecutionAddress(&entryUSE[1], microkernel->sDevVAddr.uiAddr + 0x9100,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	setUSEExecutionAddress(&entryUSE[2], microkernel->sDevVAddr.uiAddr + 0xb0d0,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	setUSEExecutionAddress(&entryUSE[3], microkernel->sDevVAddr.uiAddr + 0xed60,
			       heaps.kernelCode->sDevVAddrBase.uiAddr);
	if (PVRSRVAllocDeviceMemKM(device, process, heaps.kernelCode->hDevMemHeap, SGX_KERNEL_CODE_ATTRIBS,
				   sizeof(program7), 0x10, 1, &init->hKernelProgramMemInfo[7]) != PVRSRV_OK)
		goto allocFail;
	mem = init->hKernelProgramMemInfo[7];
	OSMemCopy(mem->pvLinAddrKM, program7, sizeof(program7));
	data = mem->pvLinAddrKM;
	data[0] = 0;
	data[6] = 0x240100;
	data[3] = 0x8000f;
	data[4] = entryUSE[2] | 0x8000000;
	data[8] = entryUSE[0] | 0x8000000;
	data[16] = entryUSE[1] | 0x8000000;
	data[5] = data[9] = data[13] = data[17] = 1;
	data[2] = 0;
	data[12] = entryUSE[3] | 0x8000000;
	data[7] = 0x420000;

	/* link the microkernel to its PDS programs */
	program = microkernel->pvLinAddrKM;
	patchUSEImmediate(&program[0x79b8 / 4], 0x2800900);
	patchUSEImmediate(&program[0x79d0 / 4], init->hKernelProgramMemInfo[7]->sDevVAddr.uiAddr >> 4);
	patchUSEImmediate(&program[0x7500 / 4], 0x2800900);
	patchUSEImmediate(&program[0x7510 / 4], init->hKernelProgramMemInfo[7]->sDevVAddr.uiAddr >> 4);
	patchUSEImmediate(&program[0xd10 / 4], 0x1001000);
	patchUSEImmediate(&program[0xd20 / 4], init->hKernelProgramMemInfo[4]->sDevVAddr.uiAddr >> 4);
	patchUSEImmediate(&program[0xd28 / 4], init->hKernelSGXTA3DCtlMemInfo->sDevVAddr.uiAddr);
	patchUSEImmediate(&program[0xd68 / 4], 0x800900);
	patchUSEImmediate(&program[0xd70 / 4], init->hKernelProgramMemInfo[5]->sDevVAddr.uiAddr >> 4);
	patchUSEImmediate(&program[0xdf0 / 4], init->hKernelProgramMemInfo[6]->sDevVAddr.uiAddr & 0xfffffff0);
	patchUSEImmediate(&program[0xe00 / 4], 6);

	/* the clear clip workaround (BRN31542); Sony ignores these failures */
	allocKernelDataZeroed(device, process, heaps.kernelData, 0x80, 0x80,
			      &init->hKernelClearClipWAVDMStreamMemInfo);
	OSMemCopy(init->hKernelClearClipWAVDMStreamMemInfo->pvLinAddrKM, program8, sizeof(program8));
	allocKernelDataZeroed(device, process, heaps.kernelData, 0x60, 0x80,
			      &init->hKernelClearClipWAIndexStreamMemInfo);
	data = init->hKernelClearClipWAIndexStreamMemInfo->pvLinAddrKM;
	for (i = 0; i < 24; i++)
		data[i] = i;
	allocKernelDataZeroed(device, process, heaps.kernelData, 0x200, 0x80,
			      &init->hKernelClearClipWAPDSMemInfo);
	OSMemCopy(init->hKernelClearClipWAPDSMemInfo->pvLinAddrKM, program9, sizeof(program9));
	allocKernelDataZeroed(device, process, heaps.kernelData, 0x200, 0x80,
			      &init->hKernelClearClipWAUSEMemInfo);
	OSMemCopy(init->hKernelClearClipWAUSEMemInfo->pvLinAddrKM, program10, sizeof(program10));
	allocKernelBufferZeroed(device, process, heaps.kernelData, 0x14000, 0x100000,
				&init->hKernelClearClipWAParamMemInfo);
	allocKernelBufferZeroed(device, process, heaps.kernelData, 0x1000, 0x1000,
				&init->hKernelClearClipWAPMPTMemInfo);
	allocKernelBufferZeroed(device, process, heaps.kernelData, 0x8000, 0x1000,
				&init->hKernelClearClipWATPCMemInfo);
	allocKernelBufferZeroed(device, process, heaps.kernelData, 0xc000, 0x1000,
				&init->hKernelClearClipWAPSGRgnHdrMemInfo);

	data = init->hKernelClearClipWAVDMStreamMemInfo->pvLinAddrKM;
	data[0] |= init->hKernelClearClipWAPDSMemInfo->sDevVAddr.uiAddr >> 4;
	data[5] |= (init->hKernelClearClipWAPDSMemInfo->sDevVAddr.uiAddr + 0x50) >> 4;
	data[7] |= (init->hKernelClearClipWAPDSMemInfo->sDevVAddr.uiAddr + 0xa0) >> 4;
	data[3] |= init->hKernelClearClipWAIndexStreamMemInfo->sDevVAddr.uiAddr;
	data = init->hKernelClearClipWAPDSMemInfo->pvLinAddrKM;
	data[8] = (((init->hKernelClearClipWAUSEMemInfo->sDevVAddr.uiAddr >> 3) & 0xfffff) << 4) | 0x100000e;
	data[28] = ((((init->hKernelClearClipWAUSEMemInfo->sDevVAddr.uiAddr + 0x80) >> 3) & 0xfffff) << 4) | 0x100000e;
	data[48] = ((((init->hKernelClearClipWAUSEMemInfo->sDevVAddr.uiAddr + 0x118) >> 3) & 0xfffff) << 4) | 0x100000e;
	program = init->hKernelProgramMemInfo[0]->pvLinAddrKM;
	patchUSEImmediate(&program[0xf30 / 4], init->hKernelClearClipWAUSEMemInfo->sDevVAddr.uiAddr >> 23 << 23);
	patchUSEImmediate(&program[0x1058 / 4], init->hKernelClearClipWAPSGRgnHdrMemInfo->sDevVAddr.uiAddr);

	corePDSExec = init->hKernelProgramMemInfo[3]->sDevVAddr.uiAddr & 0xfffffff0;
	revision = kscePervasiveGetSoCRevision();
	if ((revision & 0x1ffff) < 0x20)
		ksceKernelPrintf("[%s]:Invalid SoC revision (%X.%01X)\n", moduleInfo.module_name,
				 (revision & 0x1ffff) >> 4, revision & 15);

	/* part 1: enable the cores and their clocks */
	scriptWriteSGXReg(&part1, EUR_CR_MASTER_CORE, 3);
	scriptWriteSGXReg(&part1, 0x4020, 0x2a9);
	coreMask = 0;
	for (i = 0; i < 4; i++)
		coreMask |= 2 << cores[i];
	scriptWriteSGXReg(&part1, 0x4004, coreMask);
	command = nextScriptCommand(&part1);
	if (!command)
		goto outOfSpace;
	command->eOp = SGX_INIT_OP_HALT;

	/* part 2: events, code bases and the microkernel's PDS programs */
	scriptWriteSGXReg(&part2, EUR_CR_EVENT_HOST_ENABLE, 0x4000);
	scriptWriteSGXReg(&part2, EUR_CR_EVENT_HOST_ENABLE2, 0xc00);
	scriptWriteSGXReg(&part2, 0x13c, 0);
	scriptWriteSGXReg(&part2, 0xa7c, 0);
	scriptWriteSGXReg(&part2, 0xa80, 0);
	scriptWriteSGXReg(&part2, 0xa74, 0x208200);
	init->ui32DevData6 = 0x180;
	scriptWriteSGXReg(&part2, 0xabc, 0x40);
	init->ui32DevData7 = 0x8c0;
	scriptWriteSGXReg(&part2, 0xab8, 0);
	scriptWriteSGXReg(&part2, 0x800, 0x10);
	scriptWriteSGXReg(&part2, 0x818, 0);
	scriptWriteSGXReg(&part2, 0x804, 0x5e0);
	scriptWriteSGXReg(&part2, 0x814, 0xffff);
	scriptWriteSGXReg(&part2, 0xa00, 0x7c000);
	SCE_DBG_ASSERT(1, 4236);
	for (i = 0; i < 4; i++)
		scriptWriteSGXReg(&part2, EUR_CR_USE_CODE_BASE_4 + i * 4,
				  ((heaps.pixelShader->sDevVAddrBase.uiAddr + i * 0x800000) >> 6) |
				  USE_CODE_BASE_DM(1));
	for (i = 0; i < 4; i++)
		scriptWriteSGXReg(&part2, EUR_CR_USE_CODE_BASE_0 + i * 4,
				  (heaps.vertexShader->sDevVAddrBase.uiAddr + i * 0x800000) >> 6);
	scriptWriteSGXReg(&part2, EUR_CR_USE_CODE_BASE_9,
			  (heaps.useCodeBase9->sDevVAddrBase.uiAddr >> 6) | USE_CODE_BASE_DM(1));
	scriptWriteSGXReg(&part2, EUR_CR_USE_CODE_BASE_8, heaps.useCodeBase8->sDevVAddrBase.uiAddr >> 6);
	for (i = 0; i < 5; i++)
		scriptWriteSGXReg(&part2, EUR_CR_USE_CODE_BASE_10 + i * 4, 0);
	scriptWriteSGXReg(&part2, EUR_CR_USE_CODE_BASE_15,
			  (heaps.kernelCode->sDevVAddrBase.uiAddr >> 6) | USE_CODE_BASE_DM(3));
	scriptWriteSGXReg(&part2, 0xb30, 0x100);
	scriptWriteSGXReg(&part2, EUR_CR_EVENT_KICKER,
			  init->hKernelCCBEventKickerMemInfo->sDevVAddr.uiAddr);
	scriptWriteSGXReg(&part2, 0xa68, pixelPDSExec);
	scriptWriteSGXReg(&part2, 0xa6c, 3);
	scriptWriteSGXReg(&part2, 0xa70, 0x12001);
	scriptWriteSGXReg(&part2, 0x8a68, corePDSExec);
	scriptWriteSGXReg(&part2, 0x8a6c, 1);
	scriptWriteSGXReg(&part2, 0x8a70, 0x12001);
	scriptWriteSGXReg(&part2, 0x8a58, 0x2200c000);
	scriptWriteSGXReg(&part2, 0x4a58, 0x804300f);
	init->ui32CacheControl = 6;
	scriptWriteSGXReg(&part2, 0x4808, 4);
	scriptWriteSGXReg(&part2, 0x4144, 0x103e8);
	scriptWriteSGXReg(&part2, 0x414c, 0);
	corePDSExec = (init->hKernelProgramMemInfo[1]->sDevVAddr.uiAddr + 0x70 -
		       heaps.kernelCode->sDevVAddrBase.uiAddr) >> 3;
	scriptWriteSGXReg(&part2, 0xcba0, corePDSExec);
	scriptWriteSGXReg(&part2, 0x10ba0, corePDSExec);
	scriptWriteSGXReg(&part2, 0x14ba0, corePDSExec);
	scriptWriteSGXReg(&part2, 0x8ba0, (init->hKernelProgramMemInfo[0]->sDevVAddr.uiAddr + 0x1bd0 -
					   heaps.kernelCode->sDevVAddrBase.uiAddr) >> 3);
	scriptWriteSGXReg(&part2, 0xbb4, 0);
	command = nextScriptCommand(&part2);
	if (!command)
		goto outOfSpace;
	command->eOp = SGX_INIT_OP_HALT;

	/* deinit: stop the PDS programs, events and clocks */
	scriptWriteSGXReg(&deinit, 0xa58, 0);
	scriptWriteSGXReg(&deinit, 0xa68, 0);
	scriptWriteSGXReg(&deinit, 0xa6c, 0);
	scriptWriteSGXReg(&deinit, 0xa70, 0);
	scriptWriteSGXReg(&deinit, EUR_CR_USE_CODE_BASE_15, 0);
	scriptWriteSGXReg(&deinit, EUR_CR_EVENT_HOST_ENABLE, 0);
	scriptWriteSGXReg(&deinit, EUR_CR_EVENT_HOST_ENABLE2, 0);
	scriptWriteSGXReg(&deinit, EUR_CR_CLKGATECTL, 0);
	scriptWriteSGXReg(&deinit, EUR_CR_CLKGATECTL2, 0);
	command = nextScriptCommand(&deinit);
	if (!command)
		goto outOfSpace;
	command->eOp = SGX_INIT_OP_HALT;

	sceGpuMapPARegisters(device, heaps.perfmon);
	ret = DevInitSGXPart2KM(process, device, init);
	goto freeInit;

outOfSpace:
	ret = PVRSRV_ERROR_INSUFFICIENT_SPACE_FOR_COMMAND;
	goto freeInit;
allocFail:
	ret = PVRSRV_ERROR_OUT_OF_MEMORY;
freeInit:
	OSFreeMem(init);
freeInfo:
	OSFreeMem(info);
	return ret;
}

int module_start(SceSize args, void *argp)
{
	static const char fallbackName[] = "SceGpuInit";
	PVRSRV_DEVICE_IDENTIFIER devId;
	PVRSRV_PER_PROCESS_DATA *process;
	IMG_UINT32 count, pid, i;
	PVRSRV_ERROR ret, final;

	if (ksceKernelGetModuleInfoByAddr(&moduleInfo, &moduleInfo) < 0) {
		for (i = 0; i < sizeof(fallbackName); i++)
			moduleInfo.module_name[i] = fallbackName[i];
	}
	if (!OSProcHasPrivSrvInit() ||
	    PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_RUNNING) ||
	    PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_RAN))
		return PVRSRV_ERROR_INIT_FAILURE;

	pid = OSGetCurrentProcessIDKM();
	ret = PVRSRVPerProcessDataConnect(pid, 0);
	if (ret != PVRSRV_OK) {
		ksceKernelPrintf("%s: Error %08x with %s\n", "SrvInit", ret, "PVRSRVPerProcessDataConnect");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	process = PVRSRVPerProcessData(pid);
	if (!process) {
		ksceKernelPrintf("%s: Failed to get process data\n", "SrvInit");
		ret = PVRSRV_ERROR_INVALID_PERPROC;
	} else {
		process->bInitProcess = IMG_TRUE;
		ret = PVRSRVEnumerateDevicesKM(&count, &devId);
		if (ret != PVRSRV_OK) {
			PVRSRVPerProcessDataDisconnect(pid);
			ksceKernelPrintf("%s: Error %08x with %s\n", "SrvInit", ret, "PVRSRVEnumerateDevicesKM");
		} else {
			for (i = 0; i < count; i++) {
				const PVRSRV_DEVICE_IDENTIFIER *dev = &devId + i;

				if (dev->eDeviceClass != PVRSRV_DEVICE_CLASS_3D ||
				    dev->eDeviceType != PVRSRV_DEVICE_TYPE_SGX)
					continue;
				ret = SGXInitialisePart2KM(process, dev);
				if (ret != PVRSRV_OK)
					break;
			}
		}
	}
	/* Sony clears it even when there is no process data */
	process->bInitProcess = IMG_FALSE;
	PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_RUNNING, IMG_FALSE);
	PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_RAN, IMG_TRUE);
	final = PVRSRVFinaliseSystem(ret == PVRSRV_OK);
	PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL, final == PVRSRV_OK && ret == PVRSRV_OK);
	return final != PVRSRV_OK ? SCE_KERNEL_START_FAILED : SCE_KERNEL_START_NO_RESIDENT;
}
