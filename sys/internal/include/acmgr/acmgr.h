/* Access-manager dependencies recovered from the module's call sites. */
#pragma once
#include <psp2kern/types.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>

int SceSysrootForDriver_56D85EB0(SceUInt32 authIdLow, SceUInt32 authIdHigh);

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

int SceSblACMgrForDriver_0924896F(SceUID pid);
int SceSblACMgrForDriver_456DA7AC(SceUID pid);
int SceSblACMgrForDriver_4CBD6156(SceUID pid);		/* the process may be debugged */
int SceSblACMgrForDriver_A27E47A7(SceUID pid);
int SceSblACMgrForDriver_A92CD636(SceUID pid);
int SceSblACMgrForDriver_AD717E7A(SceUID pid);		/* the caller may use test app launch */
int SceSblACMgrForDriver_AE1AF154(SceUID pid);
int SceSblACMgrForDriver_B12CEAA8(SceUID pid, SceSblACMgrValue *attr,
				  const SceSblACMgrValue *source);
int SceSblACMgrForDriver_D7AD8471(SceUID pid, const SceChar8 *path);
int SceSblACMgrForDriver_E79C7A8D(SceUID pid, const SceChar8 *path);
int SceSblACMgrForDriver_F5AE24AC(SceUID pid);
int SceSblACMgrForDriver_FF7125DE(SceUID pid, const SceChar8 *path, SceUInt32 *mode);
int SceSblACMgrForKernel_49509A83(SceUID pid);
int SceSblACMgrForKernel_8241AB5C(SceUID pid, int bit);
int ksceSblACIsSystemProgram(SceUID pid);
int ksceSblACMgrGetProcessProgramAuthId2(SceUID pid, SceUInt64 *paid);
int ksceSblACMgrIsAllowedExtendedMemory(SceUID pid);
int ksceSblACMgrIsDevelopmentMode2(void);
int ksceSblACMgrIsGameProgram2(SceUID pid);
int ksceSblACMgrIsNonGameOrGameProgram2(SceUID pid);
int ksceSblACMgrIsNonGameProgram2(SceUID pid);
int ksceSblACMgrIsPSMRuntime(SceUID pid);
int ksceSblACMgrIsRootProgram2(SceUID pid);
