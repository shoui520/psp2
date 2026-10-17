/*
 * SceSblPostSsMgr: the secure services set up after the secure processor's
 * boot (sealed keys, keystones, portability keys, product mode, the secure
 * RTC, licenses, utokens, SPFSO contexts, the firmware loader).
 */

#ifndef POST_SS_MGR_INTERNAL_H
#define POST_SS_MGR_INTERNAL_H

#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu/cache.h>
#include <psp2kern/kernel/ssmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem/heap.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <sysmem/debug.h>
#include <string.h>

int ksceSblAimgrIsToolRev4(void);
int ksceSblAimgrIsPrototypeRev2(void);
int ksceSblAimgrIsToolRev3(void);
int ksceSblAimgrIsToolRev5(void);
int ksceSblAimgrIsPrototypeRev7(void);
int ksceSysconSetMultiCnPort(SceUInt32 port);
int ksceSysconCtrlAccPower(SceUInt32 value);
int ksceSysconCtrlHostOutputViaDongle(SceUInt32 value);
int ksceSysconJigOpenPort(void);
int ksceSysconJigClosePort(void);
int ksceSysconJigSetConfig(SceUInt32 first, SceUInt32 second);
int sceKernelDelayThread(SceUInt32 usec);

#define SCE_SBL_ERROR_UNSUPPORTED		0x800f0002	/* name unknown */
#define SCE_SBL_ERROR_INVALID_ARGUMENT		0x800f0016
#define SCE_SBL_ERROR_BAD_MAGIC			0x800f0024	/* name unknown */
#define SCE_SBL_ERROR_MISMATCH			0x800f0027	/* name unknown: a hash differs */
#define SCE_SBL_ERROR_BAD_VERSION		0x800f0037	/* name unknown */
#define SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT	0x800f1716	/* name unknown */
#define SCE_SBL_ERROR_KEYS_UNAVAILABLE		0x800f1727	/* name unknown */
#define SCE_SBL_ERROR_KEYS_LOCK			0x800f1729	/* name unknown */
#define SCE_SBL_ERROR_PM_JIG			0x800f0a05	/* name unknown */
#define SCE_SBL_ERROR_PM_INVALID_ARGUMENT	0x800f0a16	/* name unknown */
#define SCE_SBL_ERROR_PM_TIMEOUT		0x800f0a23	/* name unknown */
#define SCE_SBL_ERROR_PM_SM			0x800f0a28	/* name unknown */
#define SCE_SBL_ERROR_PM_UNSUPPORTED		0x800f0a25	/* name unknown */
#define SCE_KERNEL_ERROR_NOT_ALLOWED		0x80010058	/* name unknown */

void *ksceSblSsMemset(void *dst, int c, SceSize len);
int ksceSblACIsSystemProgram(SceUID pid);
int ksceSblNvsReadData(SceUInt32 offset, void *buf, SceSize size);
int ksceSblNvsWriteData(SceUInt32 offset, const void *buf, SceSize size);
int SceKernelSuspendForDriver_4DF40893(int unk);
int SceKernelSuspendForDriver_2BB92967(int unk);
int SceSysconForDriver_EBE3262C(void);
int SceSysconForDriver_D2F456DC(void *information);
int SceSysconForDriver_0D0B6D25(void);
int SceSysconForDriver_4BC63A40(void);
int SceSysconForDriver_29CF4335(void);
int SceSysconForDriver_7BFBA09E(SceUInt32 mode, SceUInt32 offset, SceUInt32 size, const void *data);
int SceSysconForDriver_933D813F(SceUInt32 mode, SceUInt32 offset, SceUInt32 size);
int SceSysconForDriver_3C80B529(SceUInt32 offset, SceUInt32 size, void *data);
void *ksceKernelSysrootGetKblParam(void);

/* a deferred placeholder stays a call, as the routine it stands for is */
#define SCE_ERROR_ERRNO_ENOENT		0x80010002
#define SCE_ERROR_ERRNO_EIO		0x80010005
#define SCE_ERROR_ERRNO_EEXIST		0x80010011
#define SCE_ERROR_ERRNO_EAGAIN		0x80010059	/* name unknown */

#define POST_SS_DEFERRED	__attribute__((noinline))

/* the self authentication the module's secure modules (pm_sm, act_sm, utoken_sm) run with */
#define POST_SS_SM_AUTH_INFO	{								\
	.program_authority_id = 0x2808000000000001ULL,			\
	.capability = {							\
		0x80, 0x00, 0x00, 0x00, 0xc0, 0x00, 0xf0, 0x00,		\
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,		\
	},								\
	.attribute = {							\
		0x80, 0x09, 0x80, 0x03, 0x00, 0x00, 0xc3, 0x00,		\
		0x00, 0x00, 0x80, 0x09, 0x80, 0x00, 0x00, 0x00,		\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		\
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,		\
	},								\
}

/* post_ss_mgr_startstop.c */
extern SceUID g_postSsMgrHeap;			/* name unknown */
extern SceKernelFastMutex g_postSsMgrKeysMutex;	/* name unknown */
extern SceKernelFastMutex g_postSsMgrSdMutex;	/* name unknown */
extern SceKernelFastMutex g_postSsMgrCloudMutex;	/* name unknown */

/* a keystone (names unknown) */
typedef struct ScePostSsKeystone {
	char magic[8];
	SceUInt16 version;
	SceUInt16 type;
	SceUInt8 reserved[4];
	SceUInt8 nonce[0x10];
	SceUInt8 passcodeHmac[0x20];
	SceUInt8 hmac[0x20];
} ScePostSsKeystone;

/* sealedkey.c */
int getSealedKeySecret(void *encKey, void *secret);

/* keystone.c */
int postSsMgrCheckKeystone(const ScePostSsKeystone *keystone);	/* name unknown */

/* portability.c (names unknown) */
int postSsMgrDecryptKeySet(const void *const *encA, const void *ivA, const void *const *encB,
			   const void *ivB, SceSize count, void *outA, void *outB, void **keys);
void *postSsMgrGetKeySetA(void);
void *postSsMgrGetKeySetB(void);
int postSsMgrLoadCloudKeys(void);

/* pm_mode.c */
int pmMgrIsNoSmMode(void);			/* name unknown */
int pmMgrGetProductMode(SceUInt8 *mode);	/* name unknown */
int pmMgrGetSmProductMode(SceUInt8 *mode);	/* name unknown */

/* pm_mgr.c */
extern SceUInt8 g_pmProductMode;		/* name unknown */
int sceSblPmMgrInit(void);
int sceSblPmMgrSetProductModeCommon(int mode, int useSm);
int ksceSblPostSsMgrExecutePmSmF00dCommand(int on);
int _ksceSblPostSsMgrExecutePmSmF00dCommand(SceUInt32 flags);
int ksceSblPostSsMgrExecutePmSmF00dCommand8(SceUInt8 *mode);
int sceSblPmMgrAuthEtoI(void);

/* pm_syscon_jig.c */
int pm_auth_common(int type, const char *path);
int pm_auth2_common(int type);
int invoke_pm_secure_module(const char *path);			/* name unknown */
int stop_pm_secure_module(void);					/* name unknown */
int pmSmGetMode(SceUInt32 *state, SceUInt32 *data);	/* name unknown */
int pmSmSetMode(SceUInt32 *state, SceUInt32 *data, int mode);	/* name unknown */
int pmMgrSetNvsMode(int mode, void *nvs);		/* name unknown */

int sceSblRtcMgrInit(void);		/* name unknown */
int sceSblLicMgrInit(void);		/* name unknown */
int sceSblUtMgrInit(void);		/* name unknown */
int sceSblFwLoaderInit(void);		/* name unknown */
int sceSblRsaSetRandom(int (*random)(void *buf, SceSize size));	/* name unknown */

#endif
