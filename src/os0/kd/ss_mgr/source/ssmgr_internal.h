/* Module-private globals shared by the secure service source files. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>
#include <psp2kern/kernel/ssmgr.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>

typedef union SsMgrQaFlags {
	uint32_t words[4];
	uint8_t bytes[16];
} SsMgrQaFlags;

/* Sony's cached DMAC5 keyring entry; unused bytes retain their ABI padding. */
typedef struct SceSblDmac5KeyRing {
	uint8_t key[32];
	int keyId;
	uint8_t valid;
	uint8_t reserved;
	uint8_t age;
	uint8_t keySize;
	uint16_t useCount;
	uint16_t padding;
} SceSblDmac5KeyRing;

_Static_assert(sizeof(SceSblDmac5KeyRing) == 44, "DMAC5 keyring entry ABI");

#define SS_MGR_BSS(name) __attribute__((section(".bss.ssMgr." #name)))
extern void *dmacKeyringRegisters SS_MGR_BSS(dmacKeyringRegisters);
extern SceKernelFastMutex dmacKeyringMutex SS_MGR_BSS(dmacKeyringMutex);
extern SceSblDmac5KeyRing dmacKeyringStorage[32] SS_MGR_BSS(dmacKeyringStorage);
extern uint32_t qafInitialized SS_MGR_BSS(qafInitialized);
extern SsMgrQaFlags qaFlags SS_MGR_BSS(qaFlags);
extern uint32_t qafSysconVersion SS_MGR_BSS(qafSysconVersion);
extern volatile int qafCachedSysconFlag SS_MGR_BSS(qafCachedSysconFlag);
extern volatile int qafSysconFlagInitialized SS_MGR_BSS(qafSysconFlagInitialized);
extern uint32_t qafBaryonVersion SS_MGR_BSS(qafBaryonVersion);
extern SceUID qafSmId SS_MGR_BSS(qafSmId);
extern ScePsCode aimgrPscode SS_MGR_BSS(aimgrPscode);
extern SceOpenPsId aimgrOpenPsId SS_MGR_BSS(aimgrOpenPsId);
extern SceUID aimgrMutex SS_MGR_BSS(aimgrMutex);
extern SceUID aimgrSmId SS_MGR_BSS(aimgrSmId);
extern uint8_t rngSeed[20] SS_MGR_BSS(rngSeed);
extern SceUID rngMutex SS_MGR_BSS(rngMutex);
extern uint8_t rngKey[20] SS_MGR_BSS(rngKey);

/* Imports without a vitasdk declaration. */
int ksceSysrootUtMgrHasNpTestFlag(void);

int qafIsAllowNpTest(void);
int qafLoadToken(uint8_t *token);

/* SBL error codes; Sony's names are not known. */
#define SSMGR_ERROR_INVALID_ARGUMENT	0x800F0716
#define SSMGR_ERROR_NO_QAF		0x800F0726
#define SSMGR_ERROR_NVS			0x800F0705

int ksceSblACIsSystemProgram(int pid);
int nvs_read_data(uint32_t offset, void *buffer, uint32_t size);
int ksceSblQafManagerGetQAFlags(void *flags);
int SceKernelSuspendForDriver_4DF40893(int unk);	/* power lock? (placeholder) */
int SceKernelSuspendForDriver_2BB92967(int unk);	/* power unlock? (placeholder) */
int qafValidateToken(void *token, int service);
int qafSetToken(const void *token);
int qafDeleteToken(void);
int qafGetToken(uint8_t *token);
int nvs_write_data(uint32_t offset, const void *buffer, uint32_t size);
int ksceSysconNvsSetRunMode(int mode);

/* Aimgr secure-processor queries. */
int aimgrReadPscode2(ScePsCode *pscode);
int aimgrReadConsoleId(SceConsoleId *consoleId);
int aimgrReadVisibleId(void *visibleId);
int aimgrInit(void);
int qafInit(void);
int ksceSysconNvsReadData(uint32_t offset, void *buffer, uint32_t size);
int ksceSysconNvsWriteData(uint32_t offset, const void *buffer, uint32_t size);
