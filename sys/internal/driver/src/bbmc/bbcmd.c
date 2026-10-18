/* SceBbmc command subsystem's unconditional failure entry points. */
#include <string.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/debug.h>
#include <threadmgr/fast_mutex_object.h>
#include "bbcmd.h"
#include "bbmc_memory.h"
#include "bbcmd_fp.h"

BbcmdControl *g_bbcmdControl;
static BbcmdControl g_bbcmdStorage;

/* The protocol initializer installs handlers into this original command order. */
BbcmdCommandEntry g_bbcmdCommands[52] = {
	{ .command = 0 },
	{ .command = 1 },
	{ .command = 2 },
	{ .command = 3 },
	{ .command = 4 },
	{ .command = 5 },
	{ .command = 6 },
	{ .command = 7 },
	{ .command = 8 },
	{ .command = 9 },
	{ .command = 10 },
	{ .command = 11 },
	{ .command = 12 },
	{ .command = 13 },
	{ .command = 14 },
	{ .command = 15 },
	{ .command = 16 },
	{ .command = 17 },
	{ .command = 18 },
	{ .command = 19 },
	{ .command = 20 },
	{ .command = 21 },
	{ .command = 22 },
	{ .command = 23 },
	{ .command = 24 },
	{ .command = 25 },
	{ .command = 26 },
	{ .command = 31 },
	{ .command = 36 },
	{ .command = 37 },
	{ .command = 27 },
	{ .command = 38 },
	{ .command = 28 },
	{ .command = 29 },
	{ .command = 39 },
	{ .command = 40 },
	{ .command = 41 },
	{ .command = 34 },
	{ .command = 42 },
	{ .command = 43 },
	{ .command = 44 },
	{ .command = 45 },
	{ .command = 46 },
	{ .command = 47 },
	{ .command = 32 },
	{ .command = 48 },
	{ .command = 49 },
	{ .command = 50 },
	{ .command = 33 },
	{ .command = 51 },
	{ .command = 30 },
	{ .command = 0 },
};

_Static_assert(sizeof(BbcmdCommandEntry) == 8, "command entry size");

extern int bbmcLockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("lockFastMutex");
extern int bbmcUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelUnlockFastMutex");
extern int bbmcInitializeFastMutex(struct ThreadmgrFastMutex *mutex, const char *name,
				 int attr, void *opt) __asm__("sceKernelInitializeFastMutex");
extern int bbmcFinalizeFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelFinalizeFastMutex");

void bbcmdInternalEnterCS(void)
{
	SCE_PANIC(1917);
}

void bbcmdInternalLeaveCS(void)
{
	SCE_PANIC(1941);
}

/* name unknown: snapshot active requests, dispatch wakeups, recycle completions. */
int bbcmdWorker(SceSize args, void *argp)
{
	BbcmdControl *control = *(BbcmdControl **)argp;
	SceUInt32 events = 0;
	BbcmdRequestNode *active[4];
	BbcmdRequestNode *node, *next, *tail;
	int count;

	(void)args;
	for (;;) {
		if (sceKernelWaitEventFlag(control->event, 0x8000000f, 1,
					    &events, NULL) < 0 ||
		    (events & 0x80000000) != 0)
			break;
		active[0] = NULL;
		active[1] = NULL;
		active[2] = NULL;
		active[3] = NULL;
		if (bbmcLockFastMutex(&control->mutex) < 0)
			goto lock_failed;
		count = 0;
		for (node = control->head; node != NULL; node = node->next) {
			if (node->state == 1)
				active[count++] = node;
		}
		if (bbmcUnlockFastMutex(&control->mutex) < 0)
			goto unlock_failed;
		if (count > 0) {
			node = active[0];
			if (node->request.handler == NULL)
				return 0;
			if (((events >> node->index) & 1) != 0) {
				sceKernelClearEventFlag(control->event,
							~(1U << node->index));
				node->request.handler(&node->request);
			}
			if (count != 1) {
				node = active[1];
				if (node->request.handler == NULL)
					return 0;
				if (((events >> node->index) & 1) != 0) {
					sceKernelClearEventFlag(control->event,
								~(1U << node->index));
					node->request.handler(&node->request);
				}
				if (count != 2) {
					node = active[2];
					if (node->request.handler == NULL)
						return 0;
					if (((events >> node->index) & 1) != 0) {
						sceKernelClearEventFlag(control->event,
									~(1U << node->index));
						node->request.handler(&node->request);
					}
					if (count != 3) {
						node = active[3];
						if (node->request.handler == NULL)
							return 0;
						if (((events >> node->index) & 1) != 0) {
							sceKernelClearEventFlag(control->event,
										~(1U << node->index));
							node->request.handler(&node->request);
						}
					}
				}
			}
		}
		if (bbmcLockFastMutex(&control->mutex) < 0)
			goto lock_failed;
		for (node = control->head; node != NULL; node = next) {
			next = node->next;
			if (node->state != 2)
				continue;
			if (control->freeCount == 4) {
				if (bbmcUnlockFastMutex(&control->mutex) < 0)
					goto unlock_failed;
				return 0;
			}
			node->state = 0;
			memset(&node->request, 0, sizeof(node->request));
			tail = control->tail;
			if (node != tail) {
				if (node->prev != NULL)
					node->prev->next = node->next;
				if (node->next != NULL)
					node->next->prev = node->prev;
				if (node == control->head) {
					control->head = node->next;
					node->next->prev = NULL;
				}
				tail->next = node;
				node->prev = tail;
				node->next = NULL;
				control->tail = node;
			}
			if (control->freeHead == NULL)
				control->freeHead = node;
			control->freeCount++;
		}
		if (bbmcUnlockFastMutex(&control->mutex) < 0)
			goto unlock_failed;
	}
	return 0;

lock_failed:
	bbcmdInternalEnterCS();
unlock_failed:
	bbcmdInternalLeaveCS();
}

/* name unknown: publish the command owner only after its resources exist. */
int bbcmdInitialize(unsigned int protocolKind)
{
	int ret;

	if (protocolKind != 1) {
		ret = (int)0x80330002;
		goto failed;
	}
	ret = (int)0x80330004;
	if (g_bbcmdControl != NULL)
		goto failed;
	memset(&g_bbcmdStorage, 0, sizeof(g_bbcmdStorage));
	ret = bbmcInitializeFastMutex(&g_bbcmdStorage.mutex, "SceBbcmdCS", 2, NULL);
	if (ret < 0)
		SCE_PANIC_FUNC(1803, "bbcmdInternalInitCS");
	g_bbcmdStorage.protocolKind = 1;
	g_bbcmdStorage.protocols[0] = bbcmdCreateProtocolContext(0);
	g_bbcmdStorage.protocols[1] = bbcmdCreateProtocolContext(1);
	if (g_bbcmdStorage.protocols[0] != NULL &&
	    g_bbcmdStorage.protocols[1] != NULL) {
		g_bbcmdStorage.protocols[0]->initializeHandlers(g_bbcmdCommands);
		g_bbcmdStorage.nodes[0].index = 0;
		g_bbcmdStorage.nodes[0].state = 0;
		g_bbcmdStorage.nodes[1].index = 1;
		g_bbcmdStorage.nodes[1].state = 0;
		g_bbcmdStorage.nodes[2].state = 0;
		g_bbcmdStorage.nodes[2].control = &g_bbcmdStorage;
		g_bbcmdStorage.nodes[3].state = 0;
		g_bbcmdStorage.nodes[3].control = &g_bbcmdStorage;
		g_bbcmdStorage.nodes[0].next = &g_bbcmdStorage.nodes[1];
		g_bbcmdStorage.nodes[2].prev = &g_bbcmdStorage.nodes[1];
		g_bbcmdStorage.nodes[1].next = &g_bbcmdStorage.nodes[2];
		g_bbcmdStorage.nodes[3].prev = &g_bbcmdStorage.nodes[2];
		g_bbcmdStorage.nodes[0].control = &g_bbcmdStorage;
		g_bbcmdStorage.nodes[1].control = &g_bbcmdStorage;
		g_bbcmdStorage.nodes[1].prev = &g_bbcmdStorage.nodes[0];
		g_bbcmdStorage.head = &g_bbcmdStorage.nodes[0];
		g_bbcmdStorage.freeHead = &g_bbcmdStorage.nodes[0];
		g_bbcmdStorage.nodes[2].next = &g_bbcmdStorage.nodes[3];
		g_bbcmdStorage.nodes[2].index = 2;
		g_bbcmdStorage.tail = &g_bbcmdStorage.nodes[3];
		g_bbcmdStorage.nodes[3].index = 3;
		g_bbcmdStorage.freeCount = 4;
		ret = sceKernelCreateThread("SceBbmcBbcmd", bbcmdWorker, 96, 0x2000,
					    0x10000000, 0x80000, NULL);
		if (ret >= 0) {
			g_bbcmdStorage.thread = ret;
			ret = sceKernelCreateEventFlag("SceBbmcBbcmd", 0, 0, NULL);
			if (ret >= 0) {
				g_bbcmdStorage.event = ret;
				g_bbcmdStorage.state = 1;
				g_bbcmdStorage.channelState[0] = 0;
				g_bbcmdStorage.channelState[1] = 0;
				g_bbcmdControl = &g_bbcmdStorage;
				ret = sceKernelStartThread(g_bbcmdStorage.thread, 4,
							   &g_bbcmdControl);
				if (ret >= 0)
					return ret;
				sceKernelDeleteEventFlag(g_bbcmdStorage.event);
			}
			sceKernelDeleteThread(g_bbcmdStorage.thread);
		}
	}
	if (bbmcFinalizeFastMutex(&g_bbcmdStorage.mutex) < 0)
		SCE_PANIC_FUNC(1829, "bbcmdInternalTermCS");
failed:
	g_bbcmdControl = NULL;
	return ret;
}

/* name unknown: stop the worker before destroying its event and owner. */
int bbcmdFinalize(void)
{
	SceUInt timeout = 10000000;

	if (g_bbcmdControl == NULL)
		return (int)0x80330003;
	sceKernelSetEventFlag(g_bbcmdControl->event, 0x80000000);
	sceKernelWaitThreadEnd(g_bbcmdControl->thread, NULL, &timeout);
	sceKernelDeleteThread(g_bbcmdControl->thread);
	sceKernelDeleteEventFlag(g_bbcmdControl->event);
	if (bbmcFinalizeFastMutex(&g_bbcmdStorage.mutex) < 0)
		SCE_PANIC_FUNC(1829, "bbcmdInternalTermCS");
	memset(g_bbcmdControl, 0, sizeof(*g_bbcmdControl));
	g_bbcmdControl = NULL;
	return 0;
}

/* name unknown: command allocation is available only while its owner exists. */
void *bbcmdAlloc(SceSize size)
{
	if (g_bbcmdControl != NULL)
		return bbmcAlloc(size, 0);
	return NULL;
}

/* name unknown: an inactive owner deliberately leaves the pointer untouched. */
void bbcmdFree(void *ptr)
{
	if (ptr != NULL && g_bbcmdControl != NULL)
		bbmcFree(ptr);
}

extern int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
extern int bbcmdInitializeProtocol(unsigned int command, BbcmdControl *control);
extern int bbcmdFinalizeProtocol(unsigned int command, BbcmdControl *control);
extern int bbcmdOpenChannel(unsigned int command, unsigned int channel);
extern int bbcmdCloseChannel(unsigned int command, unsigned int channel);
extern int bbcmdResetChannel(unsigned int command, unsigned int channel,
			     BbcmdControl *control);

/* Validate command arguments and queue a complete request. */
int bbcmdExecute(unsigned int channel, unsigned int command,
		  BbcmdArgument *input, BbcmdArgument *output,
		  BbcmdCompletion completion, void *completionArg)
{
	BbcmdControl *control;
	BbcmdRequestNode *node;
	int (*handler)(BbcmdRequest *request);
	unsigned int index;
	int ret;

	if (g_bbcmdControl == NULL)
		return (int)0x80330003;
	if (bbmcLockFastMutex(&g_bbcmdControl->mutex) < 0)
		goto lock_failed;
	control = g_bbcmdControl;
	if (channel > 3)
		goto invalid_argument;
	switch (command) {
	case 0: {
		unsigned int mode = input[0].value;
		unsigned int selected = input[1].value;
		BbmcControl *manager;
		int canceled = 0;

		if (mode > 1)
			goto invalid_argument;
		manager = bbmcGetControl();
		if (bbmcLockFastMutex(&control->mutex) < 0)
			goto lock_failed;
		ret = 0;
		for (node = control->head; node != NULL; node = node->next) {
			if (node->state != 1)
				continue;
			node->control->cancelMask |= 1U << node->index;
			ret = sceKernelSetEventFlag(node->control->event,
						    1U << node->index);
			if ((mode == 1 && selected == node->request.command) || ret < 0)
				goto cancel_unlock;
			canceled = 1;
		}
		if (bbmcUnlockFastMutex(&control->mutex) < 0)
			goto unlock_failed;
		if (!canceled)
			ret = bbmcSendSimpleMessage(manager, 55);
		goto done;
cancel_unlock:
		if (bbmcUnlockFastMutex(&control->mutex) < 0)
			goto unlock_failed;
		goto done;
	}
	case 1:
		if (control->state == 0)
			goto invalid_state;
		if (control->protocolKind != 1)
			goto invalid_protocol;
		ret = bbcmdInitializeProtocol(1, control);
		goto done;
	case 2:
		if (control->state <= 1)
			goto invalid_state;
		if (control->protocolKind != 1)
			goto invalid_protocol;
		ret = bbcmdFinalizeProtocol(2, control);
		goto done;
	case 3:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 0)
			goto invalid_state;
		if (control->protocolKind != 1)
			goto invalid_protocol;
		ret = bbcmdOpenChannel(3, channel);
		goto done;
	case 4:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 1)
			goto invalid_state;
		if (control->protocolKind != 1)
			goto invalid_protocol;
		ret = bbcmdCloseChannel(4, channel);
		goto done;
	case 5:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 1)
			goto invalid_state;
		break;
	case 6:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 2)
			goto invalid_state;
		break;
	case 7:
		if (channel != 0)
			goto invalid_argument;
		if (control->channelState[0] != 3)
			goto invalid_state;
		break;
	case 8:
		if (channel != 0)
			goto invalid_argument;
		if (control->channelState[0] != 4)
			goto invalid_state;
		break;
	case 9:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 2)
			goto invalid_state;
		if (input[0].value > 127)
			goto invalid_argument;
		control->requestedMask[channel] = input[0].value;
		break;
	case 10:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		break;
	case 11:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 5) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			break;
		if (input[0].value > 1 || output[1].pointer == NULL ||
		    output[2].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 12:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 5) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (input[0].pointer == NULL || input[1].value > 9)
			goto invalid_argument;
		break;
	case 13:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 5) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (input[0].pointer == NULL || input[1].value > 36 ||
		    input[2].pointer == NULL || input[3].value > 9)
			goto invalid_argument;
		break;
	case 14:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (output[0].pointer == NULL ||
		    ((output[1].pointer == NULL || output[2].pointer == NULL) &&
		     input[0].value != 2))
			goto invalid_argument;
		break;
	case 15:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 16) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (input[0].value > 64 || output[0].pointer == NULL ||
		    output[1].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 16:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 16) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if ((input[0].value > 64 && !(input[0].value & 0xffff0000)) ||
		    output[0].pointer == NULL || output[1].pointer == NULL ||
		    g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 17:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 16) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (input[0].value > 64 && !(input[0].value & 0xffff0000))
			goto invalid_argument;
		break;
	case 18:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 16) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (input[0].pointer == NULL ||
		    (input[2].value != 6 && input[2].value != 0))
			goto invalid_argument;
		break;
	case 19:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 32) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (output[1].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 20:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 32) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 21:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 32) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 22:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 32) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 23:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 32) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 24:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 8) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (output[1].pointer == NULL || output[3].pointer == NULL ||
		    output[0].pointer == NULL || output[2].pointer == NULL ||
		    g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 25:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 8) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (output[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 26:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 8) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		if (input[0].pointer == NULL)
			goto invalid_argument;
		break;
	case 27:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[0].pointer == NULL || output[1].pointer == NULL ||
		    g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 28:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 29:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		break;
	case 30:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 2) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 31:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 16) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 32:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[0].value != 0 &&
		    (input[1].value == 0 || g_bbcmdControl == NULL))
			goto invalid_argument;
		break;
	case 33:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 4) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 34:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 36:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[3].pointer == NULL || input[1].value == 0)
			goto invalid_argument;
		break;
	case 37:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[0].value > 5)
			goto invalid_argument;
		break;
	case 38:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output == NULL || input == NULL || input[1].value == 0 ||
		    g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 39:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[1].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 40:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 41:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 42:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 43:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		break;
	case 44:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 45:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[0].value > 3)
			goto invalid_argument;
		break;
	case 46:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 47:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[0].pointer == NULL || output[0].pointer == NULL ||
		    g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 49:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (output[0].pointer == NULL || g_bbcmdControl == NULL)
			goto invalid_argument;
		break;
	case 50:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3)
			goto invalid_state;
		if (input[0].value == 0 && input[1].value > 1)
			goto invalid_argument;
		break;
	case 51:
		if (channel > 1)
			goto invalid_argument;
		if (control->channelState[channel] != 3 ||
		    !(control->requestedMask[channel] & 4) ||
		    control->requestedMask[channel] != control->completedMask[channel])
			goto invalid_state;
		break;
	case 48:
		if (channel > 1)
			goto invalid_argument;
		if (control->protocolKind != 1)
			goto invalid_protocol;
		ret = bbcmdResetChannel(48, channel, control);
		goto done;
	default:
		goto invalid_argument;
	}
	for (index = 0; index < 52; index++) {
		if (g_bbcmdCommands[index].command == command)
			break;
	}
	if (index == 52 || g_bbcmdCommands[index].handler == NULL)
		goto invalid_argument;
	handler = g_bbcmdCommands[index].handler;
	if (control->freeCount == 0) {
		ret = (int)0x80330005;
		goto done;
	}
	node = control->freeHead;
	node->request.channel = channel;
	node->request.command = command;
	node->request.result = 0;
	if (input != NULL)
		__builtin_memcpy(node->request.input, input, sizeof(node->request.input));
	if (output != NULL)
		__builtin_memcpy(node->request.output, output, sizeof(node->request.output));
	node->request.node = node;
	node->request.state = 0;
	node->request.completion = completion;
	node->request.commandContext = NULL;
	node->state = 1;
	node->request.completionArg = completionArg;
	node->request.handler = handler;
	control->freeHead = node->next;
	control->freeCount--;
	ret = sceKernelSetEventFlag(control->event, 1U << node->index);
	goto done;
invalid_protocol:
	ret = (int)0x80330001;
	goto done;
invalid_state:
	ret = (int)0x80330006;
	goto done;
invalid_argument:
	ret = (int)0x80330002;
done:
	if (bbmcUnlockFastMutex(&g_bbcmdControl->mutex) < 0)
		goto unlock_failed;
	return ret;
lock_failed:
	bbcmdInternalEnterCS();
unlock_failed:
	bbcmdInternalLeaveCS();
}

/* name unknown: canonical mutex boundary with the command owner's failure path. */
int bbcmdLockMutex(struct ThreadmgrFastMutex *mutex)
{
	int ret = bbmcLockFastMutex(mutex);

	if (ret < 0)
		bbcmdInternalEnterCS();
	return ret;
}

/* name unknown: canonical mutex boundary with the command owner's failure path. */
int bbcmdUnlockMutex(struct ThreadmgrFastMutex *mutex)
{
	int ret = bbmcUnlockFastMutex(mutex);

	if (ret < 0)
		bbcmdInternalLeaveCS();
	return ret;
}

/* name unknown: change the channel's requested service mask under its mutex. */
int bbcmdSetRequestedMask(unsigned int channel, unsigned int mask)
{
	int invalid = channel > 1;

	if (channel <= 1)
		invalid = mask > 127;
	if (invalid)
		return (int)0x80330002;
	if (bbmcLockFastMutex(&g_bbcmdControl->mutex) < 0)
		bbcmdInternalEnterCS();
	g_bbcmdControl->requestedMask[channel] = mask;
	if (bbmcUnlockFastMutex(&g_bbcmdControl->mutex) < 0)
		bbcmdInternalLeaveCS();
	return 0;
}

/* name unknown: store the completed seven-bit service mask. */
int bbcmdSetCompletedMask(unsigned int channel, unsigned int mask)
{
	if (channel > 1 || mask > 127)
		return (int)0x80330002;
	g_bbcmdControl->completedMask[channel] = mask;
	return 0;
}

/* name unknown: retrieve the completed mask from a live controller. */
int bbcmdGetCompletedMask(unsigned int channel, unsigned int *mask)
{
	int invalid;

	if (g_bbcmdControl == NULL)
		return (int)0x80330002;
	invalid = mask == NULL;
	invalid |= channel > 1;
	if (invalid)
		return (int)0x80330002;
	*mask = g_bbcmdControl->completedMask[channel];
	return 0;
}

/* name unknown: set one of the original five channel lifecycle states. */
int bbcmdSetChannelState(unsigned int channel, unsigned int state)
{
	if (g_bbcmdControl == NULL)
		return (int)0x80330002;
	if (channel > 1 || state > 4)
		return (int)0x80330002;
	g_bbcmdControl->channelState[channel] = state;
	return 0;
}

/* name unknown: select the next command state from the enabled state families. */
int bbcmdSelectState(SceUInt32 *state, SceUInt32 flags)
{
	SceUInt32 current = *state;

	if (current > 21 || flags - 1 > 126)
		return (int)0x80330002;
	if ((flags & 1) != 0 && current <= 3) {
		int ret = 0;

		if (current == 2)
			*state = 3;
		else
			*state = 1;
		return ret;
	} else if ((flags & 2) != 0 && current <= 7) {
		*state = 6;
	} else if ((flags & 4) != 0 && current <= 11) {
		int ret = 0;

		if (current == 10)
			*state = 11;
		else
			*state = 9;
		return ret;
	} else if ((flags & 8) != 0 && current <= 13) {
		*state = 13;
	} else if ((flags & 16) != 0 && current <= 15) {
		*state = 15;
	} else if ((flags & 32) != 0 && current <= 18) {
		*state = 18;
	} else {
		*state = 22;
	}
	return 0;
}

/* names unknown: two retained, unreferenced originals perform the same store. */
void bbcmdStoreStateOne(SceUInt32 *state)
{
	*state = 1;
}

void bbcmdStoreStateOneDuplicate(SceUInt32 *state)
{
	*state = 1;
}

/* name unknown: clamp the exponent and truncate the encoded significand. */
volatile BbcmdFloatEncoding *bbcmdEncodeClampedFloat(volatile BbcmdFloatEncoding *output,
					  const BbcmdDoubleEncoding *input)
{
	int clamped = -126;
	SceUInt8 exponentLow = input->exponentSignificand;
	SceUInt8 top = input->signExponent;
	SceUInt8 sign = top & 0x80;
	int exponent = ((exponentLow | (top << 8)) >> 4) & 0x7ff;
	SceUInt8 middle, low, high, exponentByte;

	exponent -= 1023;
	if (exponent >= 127)
		exponent = 127;
	if (exponent >= clamped)
		clamped = exponent;
	output->signExponent = clamped + 127;
	output->significandExponent = (input->exponentSignificand << 4) |
				     (input->significandHigh >> 4);
	middle = (input->significandHigh << 4) | (input->significandMiddle >> 4);
	output->significandMiddle = middle;
	low = (SceUInt8)((input->significandMiddle << 4) |
			(input->significandLow >> 4)) >> 1;
	output->significandLow = low;
	if ((middle & 1) != 0)
		output->significandLow = low | 0x80;
	high = output->significandExponent;
	output->significandMiddle = middle >> 1;
	if ((high & 1) != 0)
		output->significandMiddle = (middle >> 1) | 0x80;
	exponentByte = output->signExponent;
	output->significandExponent = high >> 1;
	if ((exponentByte & 1) != 0)
		output->significandExponent = (high >> 1) | 0x80;
	output->signExponent = sign | (exponentByte >> 1);
	return output;
}

_Static_assert(sizeof(BbcmdDoubleEncoding) == 8, "double encoding size");
_Static_assert(sizeof(BbcmdFloatEncoding) == 4, "float encoding size");
