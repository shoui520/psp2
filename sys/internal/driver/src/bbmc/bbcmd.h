#pragma once

#include <scetypes.h>
#include <stddef.h>
#include <threadmgr/fast_mutex_object.h>

typedef struct BbcmdControl BbcmdControl;
typedef struct BbcmdRequest BbcmdRequest;
typedef struct BbcmdRequestNode BbcmdRequestNode;
typedef struct BbcmdProtocolContext BbcmdProtocolContext;
typedef struct BbcmdProtocolClient BbcmdProtocolClient;
typedef struct BbcmdCommandEntry BbcmdCommandEntry;

/* The dispatcher copies complete seven-input and six-output argument vectors. */
typedef union BbcmdArgument {
	SceUInt32 value;
	void *pointer;
	SceUInt8 bytes[4];
	SceUInt16 halfwords[2];
} BbcmdArgument;

typedef void (*BbcmdCompletion)(SceUInt32 channel, SceUInt32 command,
			      int result, BbcmdArgument *input,
			      BbcmdArgument *output, void *arg);

void bbcmdManagerCompletion(SceUInt32 channel, SceUInt32 command, int result,
			    BbcmdArgument *input, BbcmdArgument *output, void *arg);

int bbcmdExecute(unsigned int channel, unsigned int command,
		 BbcmdArgument *input, BbcmdArgument *output,
		 BbcmdCompletion completion, void *completionArg);

struct BbcmdRequest {
	SceUInt32 channel;
	SceUInt32 command;
	BbcmdArgument input[7];
	BbcmdArgument output[6];
	BbcmdCompletion completion;
	void *completionArg;
	int (*handler)(BbcmdRequest *request);
	SceUInt32 state;
	int retryTimer;
	void *commandContext;
	int result;
	BbcmdRequestNode *node;
};

struct BbcmdRequestNode {
	BbcmdRequestNode *prev;
	BbcmdRequestNode *next;
	SceUInt32 index;
	SceUInt32 state;
	BbcmdControl *control;
	BbcmdRequest request;
};

struct BbcmdControl {
	SceUInt32 state;
	SceUInt32 channelState[2];
	struct ThreadmgrFastMutex mutex;
	SceUID thread;
	SceUID event;
	SceUInt32 requestedMask[2];
	SceUInt32 completedMask[2];
	SceUInt32 cancelMask;
	SceUInt32 protocolKind;
	BbcmdProtocolContext *protocols[2];
	BbcmdRequestNode nodes[4];
	BbcmdRequestNode *head;
	BbcmdRequestNode *tail;
	BbcmdRequestNode *freeHead;
	SceUInt32 freeCount;
};

_Static_assert(sizeof(BbcmdArgument) == 4, "command argument size");
_Static_assert(sizeof(BbcmdRequest) == 92, "command request size");
_Static_assert(offsetof(BbcmdRequest, retryTimer) == 76, "request retry timer");
_Static_assert(offsetof(BbcmdRequest, commandContext) == 80, "request context");
_Static_assert(sizeof(BbcmdRequestNode) == 112, "command node size");
_Static_assert(offsetof(BbcmdControl, nodes) == 116, "command node pool");
_Static_assert(sizeof(BbcmdControl) == 580, "command controller size");

struct BbcmdCommandEntry {
	SceUInt32 command;
	int (*handler)(BbcmdRequest *request);
};

typedef struct BbcmdClientBinding {
	SceUInt32 requestBits;
	BbcmdProtocolClient *client;
} BbcmdClientBinding;

struct BbcmdProtocolContext {
	void (*initializeHandlers)(BbcmdCommandEntry *commands);
	BbcmdClientBinding bindings[10];
	BbcmdProtocolClient *clientStorage;
};

_Static_assert(sizeof(BbcmdClientBinding) == 8, "protocol client binding size");
_Static_assert(sizeof(BbcmdProtocolContext) == 88, "protocol context size");

extern BbcmdControl *g_bbcmdControl;
extern BbcmdCommandEntry g_bbcmdCommands[52];

void *bbcmdAlloc(SceSize size);
void bbcmdFree(void *ptr);
BbcmdProtocolContext *bbcmdCreateProtocolContext(unsigned int channel);
int bbcmdSetChannelState(unsigned int channel, unsigned int state);
int bbcmdLockMutex(struct ThreadmgrFastMutex *mutex);
int bbcmdUnlockMutex(struct ThreadmgrFastMutex *mutex);

/* bbcmd.c */
int bbcmdSetRequestedMask(unsigned int channel, unsigned int mask);
int bbcmdSetCompletedMask(unsigned int channel, unsigned int mask);
int bbcmdGetCompletedMask(unsigned int channel, unsigned int *mask);
int bbcmdSelectState(SceUInt32 *state, SceUInt32 flags);
void bbcmdStoreStateOne(SceUInt32 *state);
void bbcmdStoreStateOneDuplicate(SceUInt32 *state);
