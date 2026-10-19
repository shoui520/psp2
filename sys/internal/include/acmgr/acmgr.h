/*
 * SceSblACMgr (os0:kd/acmgr.skprx): its exports, each declared once, with acmgr's definition
 * as the prototype, under the link name importers know it by (tools/nids/kernel.yml).
 */
#pragma once
#include <scetypes.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a typed value passed to SceSblACMgr's queries (pfsmgr: type 6, a 2-byte file attribute) */
typedef struct SceSblACMgrValue {		/* name unknown */
	SceUInt32 type;
	unsigned char *value;
	SceSize size;
} SceSblACMgrValue;

/* SceSblACMgrForDriver */
int SceSblACMgrForDriver_0139FC20(SceUID pid);	/* vitasdk: ksceSblACMgrIsWebCoreOrWebKitProcess */
int sceSblACMgrGetFSAttributeByThreadAccessLevel(SceUInt32 mode, int level, SceSblACMgrValue *attr);
int SceSblACMgrForDriver_062CAEB2(SceUID pid);	/* vitasdk: ksceSblACMgrIsAllowedUsbSerial */
int SceSblACMgrForDriver_091F7247(SceUID pid);	/* vitasdk: ksceSblACMgrIsPSMRuntime */
int SceSblACMgrForDriver_0924896F(SceUID pid);
int sceSblACMgrIsKernelProgram(SceUID pid);
int SceSblACMgrForDriver_0B6E6CD7(SceUID pid);	/* vitasdk: ksceSblACMgrIsNotSandboxed */
int sceSblACMgrIsGameProgram(SceUID pid);
int SceSblACMgrForDriver_2A29453C(SceUID pid);	/* vitasdk: ksceSblACMgrIsAllowedVirtualMachine */
int SceSblACMgrForDriver_2E992B02(SceUID pid);
int SceSblACMgrForDriver_3B356B98(SceSblACMgrValue *value, SceUInt32 *mode);
int SceSblACMgrForDriver_426A4E8C(SceUID pid);	/* vitasdk: ksceSblACMgrIsFself */
int sceSblACMgrGetMediaType(const char *path, SceUInt32 *mediaType);
/* the same function, SceSblACMgrForKernel's export */
int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, SceUInt32 *mediaType);
int SceSblACMgrForDriver_456DA7AC(SceUID pid);
int sceSblACMgrSetFSAttributeByAttributeWithMountPoint(SceUID pid, SceSblACMgrValue *attr,
						       const SceSblACMgrValue *source,
						       SceUInt32 mountPoint);
int SceSblACMgrForDriver_4CBD6156(SceUID pid);	/* the process may be debugged */
int SceSblACMgrForDriver_4DB7F512(SceUID pid);	/* vitasdk: ksceSblACMgrIsAllowedExtendedMemory */
int SceSblACMgrForDriver_5C4BC352(SceUID pid);
int SceSblACMgrForDriver_5F9AF49C(SceUID pid, const SceChar8 *path);
int SceSblACMgrForDriver_6210D745(SceUID pid, SceSblACMgrValue *value, SceUInt32 mode);
int SceSblACMgrForDriver_6C5AB07F(SceUID pid);	/* vitasdk: ksceSblACMgrIsNonGameProgram */
int SceSblACMgrForDriver_6D8A88B7(SceUID pid);
int SceSblACMgrForDriver_84604EED(SceUID pid);
int sceSblACMgrIsSystemProgram(SceUID pid);
int SceSblACMgrForDriver_8A54DF3A(SceUID pid);	/* vitasdk: ksceSblACMgrIsNonGameOrGameProgram */
/* vitasdk: ksceSblACMgrIsUpdaterUISetupperOrPkgInstallerSpawn */
int SceSblACMgrForDriver_962CD237(SceUID pid);
int sceSblACMgrGetPaid(SceUID pid, SceUInt64 *paid);
int SceSblACMgrForDriver_991FDC15(SceUID pid);	/* vitasdk: ksceSblACMgrIsSceShell */
int SceSblACMgrForDriver_A27E47A7(SceUID pid);
int SceSblACMgrForDriver_A67E8E5B(SceUID pid);	/* vitasdk: ksceSblACMgrIsSIEApp */
int SceSblACMgrForDriver_A92CD636(SceUID pid);
int SceSblACMgrForDriver_AD717E7A(SceUID pid);	/* the caller may use test app launch */
int SceSblACMgrForDriver_AE1AF154(SceUID pid);
int SceSblACMgrForDriver_B12CEAA8(SceUID pid, SceSblACMgrValue *attr,
				  const SceSblACMgrValue *source);
int SceSblACMgrForDriver_B836CF13(SceUID pid);
int SceSblACMgrForDriver_BE5667C5(SceUID pid, SceSblACMgrValue *attr, SceUInt32 access);
int sceSblACMgrHasCapability(SceUID pid, int bit);
int SceSblACMgrForDriver_C98D82EE(SceUID pid);	/* vitasdk: ksceSblACMgrIsPSMDevAssistant */
int SceSblACMgrForDriver_D0E11C89(SceUID pid);	/* vitasdk: ksceSblACMgrIsMiniSettingsForQA */
int SceSblACMgrForDriver_D7AD8471(SceUID pid, const SceChar8 *path);
int SceSblACMgrForDriver_E79C7A8D(SceUID pid, const SceChar8 *path);
int SceSblACMgrForDriver_E87D1777(void);	/* vitasdk: ksceSblACMgrIsDevelopmentMode */
int SceSblACMgrForDriver_F5AE24AC(SceUID pid);
int SceSblACMgrForDriver_FD00C72A(SceUID pid);	/* vitasdk: ksceSblACMgrIsPspEmu */
int SceSblACMgrForDriver_FF7125DE(SceUID pid, const SceChar8 *path, SceUInt32 *mode);

/* SceSblACMgrForKernel */
int SceSblACMgrForKernel_02422F1F(SceUID pid);
int SceSblACMgrForKernel_04C0ED3F(SceUID pid);
int SceSblACMgrForKernel_sceSblACMgrIsGameProgram(SceUID pid);
int SceSblACMgrForKernel_06BE9F0F(SceUID pid);
int SceSblACMgrForKernel_0E489631(SceUID pid);
int SceSblACMgrForKernel_11C9158B(SceSblACMgrValue *attr, SceUInt32 *mode);
int SceSblACMgrForKernel_165C3C7A(SceUID pid, const SceChar8 *path);
int SceSblACMgrForKernel_1948E9DB(void);
/* vitasdk: kscePfsACSetFSAttrByMode */
int SceSblACMgrForKernel_2AE6CF27(SceUID pid, SceSblACMgrValue *attr, SceUInt32 mode);
int SceSblACMgrForKernel_30575458(SceUID pid);
int SceSblACMgrForKernel_sceSblACMgrIsKernelProgram(SceUID pid);
int SceSblACMgrForKernel_3388F595(SceUID pid);	/* vitasdk: ksceSblACMgrIsSIEApp2 */
int SceSblACMgrForKernel_356B9139(SceUID pid, const SceChar8 *path, SceUInt32 *mode);
int SceSblACMgrForKernel_384D20FD(SceUID pid);	/* vitasdk: ksceSblACMgrIsAllowedExtendedMemory2 */
int SceSblACMgrForKernel_3F99279F(SceUID pid);	/* vitasdk: ksceSblACMgrIsNonGameProgram2 */
int SceSblACMgrForKernel_410357AF(SceUID pid);
int SceSblACMgrForKernel_47B67F72(SceUID pid);	/* vitasdk: ksceSblACMgrIsPspEmu2 */
int SceSblACMgrForKernel_48F4D5EE(SceUID pid);
int SceSblACMgrForKernel_49509A83(SceUID pid);
int SceSblACMgrForKernel_4C4B7D6B(SceUID pid);
int SceSblACMgrForKernel_570D6AD3(SceUID pid);	/* vitasdk: ksceSblACMgrIsNonGameOrGameProgram2 */
int SceSblACMgrForKernel_5AC59172(SceUID pid);	/* vitasdk: ksceSblACMgrIsFself2 */
int SceSblACMgrForKernel_5E6BA11C(SceUID pid);
int SceSblACMgrForKernel_7529E364(SceUID pid);
int SceSblACMgrForKernel_75AAF981(SceUID pid, SceSblACMgrValue *attr, SceUInt32 access);
int SceSblACMgrForKernel_sceSblACMgrGetPaid(SceUID pid, SceUInt64 *paid);
int SceSblACMgrForKernel_7F294A09(SceUID pid, const SceChar8 *path);
int SceSblACMgrForKernel_8241AB5C(SceUID pid, int bit);
int SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(SceUID pid);
int SceSblACMgrForKernel_96403142(SceUID pid);	/* vitasdk: ksceSblACMgrIsAllowedVirtualMachine2 */
int SceSblACMgrForKernel_966B3738(SceUID pid);	/* vitasdk: ksceSblACMgrIsPSMRuntime2 */
/* vitasdk: ksceSblACMgrIsUpdaterUISetupperOrPkgInstallerSpawn2 */
int SceSblACMgrForKernel_98B28671(SceUID pid);
int SceSblACMgrForKernel_sceSblACMgrHasCapability(SceUID pid, int bit);
int SceSblACMgrForKernel_A50FDA27(SceUID pid);
int SceSblACMgrForKernel_A7C3001D(SceUID pid);	/* vitasdk: ksceSblACMgrIsWebCoreOrWebKitProcess2 */
int SceSblACMgrForKernel_AF6F208E(SceUID pid);	/* vitasdk: ksceSblACMgrIsMiniSettingsForQA2 */
int SceSblACMgrForKernel_BBA13D9C(void);	/* vitasdk: ksceSblACMgrIsDevelopmentMode2 */
int SceSblACMgrForKernel_C90A9216(SceUID pid);	/* vitasdk: ksceSblACMgrIsPSMDevAssistant2 */
int SceSblACMgrForKernel_CCDBB74D(SceUID pid, SceSblACMgrValue *attr,
				  const SceSblACMgrValue *source);
int SceSblACMgrForKernel_DC49E160(SceUID pid);
int SceSblACMgrForKernel_E273ED8C(SceUID pid);
int SceSblACMgrForKernel_F5AD56E4(SceUID pid);	/* vitasdk: ksceSblACMgrIsNotSandboxed2 */
int SceSblACMgrForKernel_F9FEF5F0(SceUID pid);	/* vitasdk: ksceSblACMgrIsSceShell2 */
int SceSblACMgrForKernel_FB2AC5B4(SceUID pid, const SceChar8 *path);
int SceSblACMgrForKernel_FBA1A256(void);

/* SceSblACMgr: the system call */
int _sceSblACMgrIsGameProgram(SceUInt32 *pResult);

#ifdef __cplusplus
}
#endif
