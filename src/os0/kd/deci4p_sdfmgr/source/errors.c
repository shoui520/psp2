/* System-error capture and debugger handoff. */
#include "sdfmgr_internal.h"
#include <psp2kern/kernel/excpmgr.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include <sce/excpmgr_cpu.h>

void sdfMgrFormatCpsr(char *buffer, uint32_t cpsr);
void dumpModuleInfo(uint32_t address);

typedef int (*SdfMgrExceptionHook)(SceExcpmgrExceptionContext *context,
				 uint32_t address, uint32_t cpsr);

int sdfMgrPrefetchAbortHandler(SceExcpmgrExceptionContext *context,
			      uint32_t address, uint32_t cpsr)
{
	SceKernelThreadContextInfo thread;
	char status[64];
	SdfMgrCpuState *cpu;
	SdfMgrExceptionHook hook;
	uint32_t fault;
	int result = 2;

	ksceKernelGetThreadContextInfo(&thread);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	fault = readIfsr();
	sdfMgrFormatCpsr(status, cpsr);
	s_sdfMgrState.exceptionDepth = (uint32_t *)ksceExcpmgrGetData();
	if (((fault & 15) | ((fault >> 10) & 1) << 4) != 2 &&
	    thread.process_id == SCE_KERNEL_PROCESS_ID)
		dumpModuleInfo(address);
	hook = (SdfMgrExceptionHook)cpu->indexedHooks[0];
	if (hook)
		result = hook(context, address, cpsr);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
	return result;
}

int sdfMgrDataAbortHandler(SceExcpmgrExceptionContext *context,
			  uint32_t address, uint32_t cpsr)
{
	SceKernelThreadContextInfo thread;
	char status[64];
	SdfMgrCpuState *cpu;
	SdfMgrExceptionHook hook;
	uint32_t fault;
	int result = 2;

	ksceKernelGetThreadContextInfo(&thread);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	fault = readDfsr();
	sdfMgrFormatCpsr(status, cpsr);
	s_sdfMgrState.exceptionDepth = (uint32_t *)ksceExcpmgrGetData();
	if (((fault & 15) | ((fault >> 10) & 1) << 4) != 2 &&
	    thread.process_id == SCE_KERNEL_PROCESS_ID)
		dumpModuleInfo(address);
	hook = (SdfMgrExceptionHook)cpu->indexedHooks[1];
	if (hook)
		result = hook(context, address, cpsr);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
	return result;
}

int sdfMgrUndefinedHandler(SceExcpmgrExceptionContext *context,
			   uint32_t address, uint32_t cpsr)
{
	SceKernelThreadContextInfo thread;
	char status[64];
	SdfMgrCpuState *cpu;
	SdfMgrExceptionHook hook;
	int result = 2;

	ksceKernelGetThreadContextInfo(&thread);
	sdfMgrFormatCpsr(status, cpsr);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	if (thread.process_id == SCE_KERNEL_PROCESS_ID)
		dumpModuleInfo(address);
	hook = (SdfMgrExceptionHook)cpu->indexedHooks[2];
	if (hook)
		result = hook(context, address, cpsr);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
	return result;
}

void dcmpSendStatusSystemError(uint32_t type, SceSize size, const void *source);
void *dcmpGetSystemErrorBuffer(void);
SceSize dcmpGetSystemErrorBufferSize(void);
SceSize SceSysrootForKernel_CD4B84F7(void *buffer, SceSize size);

typedef void (*SdfMgrBusErrorHook)(uint32_t status0, uint32_t status1, uint32_t status2);

void busErrorHandler(uint32_t status0, uint32_t status1, uint32_t status2)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	SdfMgrBusErrorHook hook;
	void *buffer;
	SceSize size;

#if FW_DEBUG
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(2377, 0xfdbcceec, 0x4a0dff49, "busErrorHandler"),
		"****** BusError Intr ******\n", sizeof(SdfMgrCpuState));
#endif
	dcmpSendStatusSystemError(2, 0, NULL);
	buffer = dcmpGetSystemErrorBuffer();
	size = dcmpGetSystemErrorBufferSize();
	size = SceSysrootForKernel_CD4B84F7(buffer, size);
	if (size)
		dcmpSendStatusSystemError(2, size, NULL);
	hook = (SdfMgrBusErrorHook)cpu->indexedHooks[3];
	if (hook)
		hook(status0, status1, status2);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
}
