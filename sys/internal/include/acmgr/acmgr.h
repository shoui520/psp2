/* Access-manager dependencies recovered from the module's call sites. */
#pragma once
#include <psp2kern/types.h>

int ksceKernelSysrootGetThreadAccessLevel(void);
int ksceSysrootGetSelfAuthInfo(SceUID pid, void *authInfo);
int SceSysrootForDriver_26AA237C(void);
int SceSysrootForDriver_E2515A08(void);
int SceSysrootForDriver_56D85EB0(SceUInt32 authIdLow, SceUInt32 authIdHigh);
int SceQafMgrForDriver_694D1096(void);
int ksceSblQafMgrIsAllowSystemAppDebug(void);

/* Driver C90A9216 / kernel C98D82EE, recovered one-PID query. */
int ksceSblACMgrIsPSMDevAssistant(SceUID pid);

/* a typed value passed to SceSblACMgr's queries (pfsmgr: type 6, a 2-byte file attribute) */
typedef struct SceSblACMgrValue {		/* name unknown */
	SceUInt32 type;
	unsigned char *value;
	SceSize size;
} SceSblACMgrValue;

int SceSblACMgrForDriver_6210D745(SceUID pid, SceSblACMgrValue *value, SceUInt32 mode);
int SceSblACMgrForDriver_3B356B98(SceSblACMgrValue *value, SceUInt32 *mode);
int SceSblACMgrForDriver_BE5667C5(SceUID pid, SceSblACMgrValue *attr, SceUInt32 access);
