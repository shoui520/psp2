/* Sony venezia/dsp0_rpc.c: RPC history and diagnostics. */
#undef __FILE_NAME__
#define __FILE_NAME__ "venezia/dsp0_rpc.c"
#include <sce/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include "venezia_internal.h"

static inline VeneziaRpcArg *_getRpcArg(unsigned int channel)
{
#if FW_DEBUG
	ksceKernelAssert(channel <= 15, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	return g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
}

int _sceVeneziaRpcGetHistory(VeneziaRpcHistoryEntry *history)
{
	unsigned int count, i, channel, previous;
	VeneziaRpcArg *arg, *previousArg;

#if FW_DEBUG
	ksceKernelAssert(history != NULL, SCE_DBGINFO_FUNC(923, "_sceVeneziaRpcGetHistory"),
			 __builtin_return_address(0));
#endif
	if (!g_veneziaRpcState.memory)
		return 0;
	unsigned int channels[8] __attribute__((aligned(8))) = {0, 1, 2, 3, 4, 5, 6, 7};
	count = 8;
	do {
		for (i = 1; i < count; i++) {
			channel = channels[i];
			arg = _getRpcArg(channel);
			previous = channels[i - 1];
			previousArg = _getRpcArg(previous);
			if (arg->sequence < previousArg->sequence) {
				channels[i - 1] = channel;
				channels[i] = previous;
			}
		}
	} while (--count != 2);
	for (i = 0; i < 8; i++) {
		channel = channels[i];
		arg = _getRpcArg(channel);
#if FW_DEBUG
		ksceKernelAssert(arg != NULL, SCE_DBGINFO_FUNC(946, "_sceVeneziaRpcGetHistory"),
				 __builtin_return_address(0));
#endif
		history[i].resultFlags = arg->resultFlags;
		history[i].pid = arg->pid;
		history[i].channel = channel;
		history[i].command = arg->command;
		history[i].arguments[0] = arg->arguments[0];
		history[i].arguments[1] = arg->arguments[1];
		history[i].arguments[2] = arg->arguments[2];
		history[i].arguments[3] = arg->arguments[3];
		history[i].result = arg->result;
	}
	return 0;
}

void _sceVeneziaRpcPrintBlockingCalls(SceUID pid)
{
#if FW_DEBUG
	VeneziaRpcArg *arg;
	unsigned int channel;

	if (!g_veneziaRpcState.memory)
		return;
	for (channel = 0; channel < 8; channel++) {
		arg = _getRpcArg(channel);
		ksceKernelAssert(arg != NULL,
			 SCE_DBGINFO_FUNC(969, "_sceVeneziaRpcPrintBlockingCalls"),
			 __builtin_return_address(0));
		if (!(arg->resultFlags & 2) && arg->pid == pid)
			SCE_DBG_PRINTF_FUNC(0, 975, "_sceVeneziaRpcPrintBlockingCalls",
				"Warning: Codec Engine is waiting for suspend... (0x%08X, %2u, %3u)\n",
				pid, channel, arg->command & 0x1ff);
	}
#endif
}

/* name unknown: invoke the dedicated DSP debug channel. */
int callVeneziaDebugRpcThunk(unsigned int command, const unsigned int *input,
			    VeneziaRpcPack pack, VeneziaRpcUnpack unpack)
{
	return _callDebugThunk(command, input, pack, unpack);
}

/* name unknown: invoke the dedicated DSP debug channel. */
int callVeneziaDebugRpc(unsigned int command, unsigned int argument0,
		unsigned int argument1, unsigned int argument2, unsigned int argument3)
{
	unsigned int arguments[4] __attribute__((aligned(8)));

	arguments[0] = argument0;
	arguments[1] = argument1;
	arguments[2] = argument2;
	arguments[3] = argument3;
	return _callDebugThunk(command, arguments, NULL, NULL);
}

#if FW_DEBUG
/* name unknown: debug channel permits available. */
unsigned int getRpcFreeDebugChannelCount(void)
{
	return g_veneziaRpcState.freeDebugChannels;
}
#endif

#if FW_VERSION >= 0x04000000
extern int finalizeVeneziaRpcCommunicationMemory(void);
#else
extern int finalizeVeneziaRpcCommunicationMemory(void);
#endif

int _sceVeneziaStoreContext(void)
{
	if (g_veneziaRpcState.memory)
		memcpy(g_veneziaRpcState.savedCommunicationMemory,
		       g_veneziaRpcState.memory, sizeof(VeneziaRpcMemory));
	return finalizeVeneziaRpcCommunicationMemory();
}

#if FW_VERSION >= 0x04000000
extern int enableVeneziaMailboxInterrupts(void);
extern int initVeneziaRpcCommunicationMemory(void);
#else
extern int enableVeneziaMailboxInterrupts(void);
extern int initVeneziaRpcCommunicationMemory(void);
#endif

int _sceVeneziaLoadContext(void)
{
	int ret;

	ret = enableVeneziaMailboxInterrupts();
	if (ret >= 0) {
		ret = initVeneziaRpcCommunicationMemory();
		if (ret >= 0 && g_veneziaRpcState.memory)
			memcpy(g_veneziaRpcState.memory,
			       g_veneziaRpcState.savedCommunicationMemory, sizeof(VeneziaRpcMemory));
	}
	return ret;
}
