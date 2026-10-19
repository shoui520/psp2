/*
 * SceSblPostSsMgr: the secure services set up after the secure processor's
 * boot (sealed keys, keystones, portability keys, product mode, the secure
 * RTC, licenses, utokens, SPFSO contexts, the firmware loader).
 */

#ifndef POST_SS_MGR_INTERNAL_H
#define POST_SS_MGR_INTERNAL_H

#include <scetypes.h>
#include <kernel/error.h>
#include <sysmem/cpu.h>
#include <ss_mgr/ss_mgr.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/debug.h>
#include <string.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/suspend.h>
#include <acmgr/acmgr.h>

#define SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT	0x800f1716	/* name unknown */
#define SCE_SBL_ERROR_KEYS_UNAVAILABLE		0x800f1727	/* name unknown */
#define SCE_SBL_ERROR_KEYS_LOCK			0x800f1729	/* name unknown */
#define SCE_SBL_ERROR_PM_JIG			0x800f0a05	/* name unknown */
#define SCE_SBL_ERROR_PM_INVALID_ARGUMENT	0x800f0a16	/* name unknown */
#define SCE_SBL_ERROR_PM_TIMEOUT		0x800f0a23	/* name unknown */
#define SCE_SBL_ERROR_PM_SM			0x800f0a28	/* name unknown */
#define SCE_SBL_ERROR_PM_UNSUPPORTED		0x800f0a25	/* name unknown */
#define SCE_SBL_ERROR_RTC_NO_CP			0x800f1002	/* name unknown: no CP clock */

/* a deferred placeholder stays a call, as the routine it stands for is */
#define POST_SS_DEFERRED	__attribute__((noinline))

/* the self authentication the module's secure modules (pm_sm, act_sm, utoken_sm) run with */
#define POST_SS_SM_AUTH_INFO	{								\
	.programAuthorityId = 0x2808000000000001ULL,			\
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

/* pm_sm, which keeps the product mode where the syscon cannot */
#define PM_SM_PATH			"os0:sm/pm_sm.self"
#define SYSCON_STATUS_NO_SM_MODE	0x40	/* name unknown */

/* pm_mode.c */
int pmMgrIsNoSmMode(void);			/* name unknown */
int pmMgrGetProductMode(SceUInt8 *mode);	/* name unknown */
int pmMgrGetSmProductMode(SceUInt8 *mode);	/* name unknown */

/* pm_mgr.c */
extern SceUInt8 g_pmProductMode;		/* name unknown */
int sceSblPmMgrInit(void);
int sceSblPmMgrSetProductModeCommon(int mode, int useSm);

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

/* rsa.c */
int postSsCopyKey(void *dst, SceSize count, const void *src, SceSize size);

#endif
