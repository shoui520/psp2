#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sce/bbmc.h>
#include "update_internal.h"

typedef struct BbmcUtilContext {
	SceUID threadId;
	SceUID eventFlagId;
	SceUID resultEventFlagId;
	BbmcSubscribeFeatureIdParam subscriber;
	uint32_t state;
	uint32_t progress;
	uint32_t firmwareVersion;
	uint64_t chunkCount;
	uint64_t chunkIndex;
	uint64_t dataSize;
	const void *buffer;
	unsigned int nodeType;
	UpdateUtilNodeStatus *status;
	uint32_t transferDone;
	uint32_t ready;
	uint32_t complete;
	uint32_t shutdown;
	uint32_t running;
} BbmcUtilContext;

_Static_assert(sizeof(BbmcUtilContext) == 96, "BBMC updater context size");
_Static_assert(offsetof(BbmcUtilContext, subscriber) == 12 &&
	offsetof(BbmcUtilContext, chunkCount) == 40 &&
	offsetof(BbmcUtilContext, buffer) == 64 &&
	offsetof(BbmcUtilContext, running) == 92,
	"BBMC updater context layout");

extern BbmcUtilContext updateBbmcContext;
extern int updateBbmcInitialized;

int SceBbmcForDriver_E83B73CA(int subscriber, unsigned int events);
int SceBbmcForDriver_435858B1(int subscriber);
int BbmcUtilCallback(int notifyId, int notifyCount, int notifyArg, void *userData);
int SceSblUsComUtilCallbackHandler(SceSize args, void *argp);
int BbmcUtilInitialize(void);
int SceBbmcForDriver_E89B746D(int subscriber, uint32_t *hardwareInfo)
	__attribute__((weak));
int SceBbmcForDriver_4281E476(int subscriber, uint32_t *status);
int SceBbmcForDriver_ACB0EC17(int subscriber, const BbmcUpdatePackage *package);
int SceBbmcForDriver_94A31A9C(int subscriber, const BbmcUpdatePackage *package);

int SceBbmcForDriver_7CB72009(int subscriber, uint32_t *version)
	__attribute__((weak));
int BbmcUtilIsAvailable(void);
