/* DMAC5 operation state shared by the manager and its keyring helpers. */
#pragma once
#include <stdint.h>
#include <dmacmgr/dmacmgr.h>

/* Name unknown: the first 40 bytes hold an IV or incremental hash state. */
typedef struct Dmac5Context {
	uint8_t state[40];
	uint32_t command;
	uint32_t keyRingIndex;
	uint32_t dataCoherency;
	uint32_t ivCoherency;
	SceKernelDmaOpId operation;
	uint32_t reserved;
} Dmac5Context;

_Static_assert(sizeof(Dmac5Context) == 64, "DMAC5 context ABI");

int dmac5ValidateReservedKey(unsigned int key, uint32_t *index);
int dmac5SetKeySize(uint32_t *command, unsigned int bits);

/* dmac5mgr.c */
int sceSblDmac5Init(void);
int dmac5AllocOperation(void);
int dmac5HashCrypt(Dmac5Context *context, const void *src, void *dst,
	SceSize size, uint32_t flags, const void *key);
int dmac5ClearKeyring(void);	/* clears the cached DMAC5 keyring on resume */
int dmac5LockUserBuffers(const void *src, SceSize srcSize, void *dst, SceSize dstSize);
int dmac5UnlockUserBuffers(const void *src, SceSize srcSize, void *dst, SceSize dstSize);
int dmac5LoadIv(Dmac5Context *context, const void *iv);
int dmac5StoreIv(Dmac5Context *context, void *iv);

/* dmac5mgr_key.c */
int dmac5SetExternalKey(SceKernelDmaOpDirectParam *param, const void *key, uint32_t command);
int dmac5SetKeyRingIndex(SceKernelDmaOpDirectParam *param, unsigned int keyRingIndex);
