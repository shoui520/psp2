/* SceSblSsMgr: the secure-service manager's interface (DMAC5, keyrings, random numbers) */
#pragma once
#include <stdint.h>
#include <scetypes.h>
#include <kernel/openpsid.h>
#include <sysmem/memblock.h>
#include <sysmem/pmmgr.h>

/*
 * The console's identifiers (the henkaku wiki's names; no Sony name found). The console ID
 * (IDPS) leads with the PS code's fields; the chassis check's top six bits are its factory code.
 */
typedef struct SceConsoleId {
	SceUInt16 reserved;
	SceUInt16 company_code;
	SceUInt16 product_code;
	SceUInt16 product_sub_code;
	SceUInt8 chassis_check;
	SceUInt8 reserved2[7];
} SceConsoleId;

_Static_assert(sizeof(SceConsoleId) == 0x10, "SceConsoleId");

typedef struct ScePsCode {
	SceUInt16 company_code;
	SceUInt16 product_code;
	SceUInt16 product_sub_code;
	SceUInt16 factory_code;
} ScePsCode;

_Static_assert(sizeof(ScePsCode) == 8, "ScePsCode");

/* the message sceSblSsEncryptWithPortability and DecryptWithPortability transform */
typedef struct ScePortabilityData {
	SceSize msg_size;	/* at most sizeof(msg) */
	SceUInt8 msg[0x20];
} ScePortabilityData;

_Static_assert(sizeof(ScePortabilityData) == 0x24, "ScePortabilityData");

/*
 * The DMAC5 operations' parameter blocks (vitasdk's names; no Sony name found). A user
 * syscall copies its block in, so the layout is the ABI.
 */
typedef struct SceSblDmac5EncDecParam {
	const void *src;
	void *dst;
	SceSize length;
	const void *key;
	SceSize keysize;	/* in bits */
	void *iv;
} SceSblDmac5EncDecParam;

_Static_assert(sizeof(SceSblDmac5EncDecParam) == 0x18, "SceSblDmac5EncDecParam");

/* SHA-256 state and the length hashed so far */
typedef struct SceSblDmac5HashTransformContext {
	SceUInt32 state[8];
	SceUInt64 length;
} SceSblDmac5HashTransformContext;

_Static_assert(sizeof(SceSblDmac5HashTransformContext) == 0x28,
	       "SceSblDmac5HashTransformContext");

typedef struct SceSblDmac5HashTransformParam {
	const void *src;
	void *dst;
	SceSize length;
	const void *key;
	SceSize keysize;	/* in bits */
	void *ctx;		/* a SceSblDmac5HashTransformContext, or a 0x10-byte one */
} SceSblDmac5HashTransformParam;

_Static_assert(sizeof(SceSblDmac5HashTransformParam) == 0x18,
	       "SceSblDmac5HashTransformParam");

/* vitasdk: ksceSblDmac5AesEcbEncWithKeyslot */
int SceSblSsMgrForDriver_01BE0374(const void *src, void *dst, int size, unsigned int key, int keyBits, int coherent);
int sceSblAimgrGetVisibleId(void *visibleId);
/* vitasdk: ksceSblDmac5DesCbcEncWithKeyslot */
int SceSblSsMgrForDriver_05B38698(const void *src, void *dst, int size, unsigned int key, int keyBits, void *iv, int coherent);
/* vitasdk: ksceSblDmac5AesEcbEncNP */
int SceSblSsMgrForDriver_0F7D28AF(const void *src, void *dst, int size, const void *key, int keyBits, int keyId, int coherent);
/* vitasdk: ksceSblDmac5AesCbcDec */
int SceSblSsMgrForDriver_121FA69F(const void *src, void *dst, int size, const void *key, int keyBits, void *iv, int coherent);
/* vitasdk: ksceSblDmac5AesCbcDecNP */
int SceSblSsMgrForDriver_1901CB5E(const void *src, void *dst, int size, const void *key, int keyBits, void *iv, int keyId, int coherent);
/* vitasdk: ksceSblDmac5AesEcbDecNP */
int SceSblSsMgrForDriver_197ACF6F(const void *src, void *dst, int size, const void *key, int keyBits, int keyId, int coherent);

int SceSblSsMgrForDriver_1B14658D(const void *src, void *dst, int size, const void *key, unsigned int keyBits, void *iv, int coherent, uint32_t flags);
int sceSblSsEncryptWithPortability(SceUInt32 key_type, void *iv, ScePortabilityData *src, ScePortabilityData *dst);
/* vitasdk: ksceSblDmac5DesEcbEncWithKeyslot */
int SceSblSsMgrForDriver_37DD5CBF(const void *src, void *dst, int size, unsigned int key, int keyBits, int coherent);
/* vitasdk: ksceSblDmac5Sha1HmacTransform */
int SceSblSsMgrForDriver_6704D985(const void *src, void *dst, int size, const void *key, void *iv, int coherent, uint32_t flags);
/* vitasdk: ksceSblDmac5AesCbcEncNP */
int SceSblSsMgrForDriver_711C057A(const void *src, void *dst, int size, const void *key, int keyBits, void *iv, int keyId, int coherent);
/* vitasdk: ksceSblDmac5Sha256Hmac */
int SceSblSsMgrForDriver_79F38554(const void *src, void *dst, int size, const void *key, void *iv, int coherent, uint32_t flags);
/* vitasdk: ksceSblDmac5AesEcbDec */
int SceSblSsMgrForDriver_7C978BE7(const void *src, void *dst, int size, const void *key, int keyBits, int coherent);
/* vitasdk: ksceSblDmac5AesCtrDec */
int SceSblSsMgrForDriver_7D46768C(const void *src, void *dst, int size, const void *key, int keyBits, void *iv, int coherent);
/* vitasdk: ksceSblDmac5AesCtrEnc */
int SceSblSsMgrForDriver_82B5DCEF(const void *src, void *dst, int size, const void *key, int keyBits, void *iv, int coherent);
/* vitasdk: ksceSblDmac5AesCmacNP */
int SceSblSsMgrForDriver_83B058F5(const void *src, void *dst, SceSize size, const void *key, unsigned int keyBits, void *iv, int keyId, int coherent, uint32_t flags);
/* vitasdk: ksceSblDmac5AesEcbDecWithKeyslot */
int SceSblSsMgrForDriver_8B4700CB(const void *src, void *dst, int size, unsigned int key, int keyBits, int coherent);
/* vitasdk: ksceSblDmac5DesEcbDecWithKeyslot */
int SceSblSsMgrForDriver_8EAFB18A(const void *src, void *dst, int size, unsigned int key, int keyBits, int coherent);
/* vitasdk: ksceSblDmac5DesCbcDecWithKeyslot */
int SceSblSsMgrForDriver_926BCCF0(const void *src, void *dst, int size, unsigned int key, int keyBits, void *iv, int coherent);
/* vitasdk: ksceSblDmac5Sha1HmacNP */
int SceSblSsMgrForDriver_92E37656(const void *src, void *dst, SceSize size, const void *key, void *iv, int keyId, int coherent, uint32_t flags);
/* vitasdk: ksceSblSsDecryptWithPortability */
int SceSblSsMgrForDriver_934DB6B5(SceUInt32 key_type, void *iv, ScePortabilityData *src, ScePortabilityData *dst);
int sceSblAimgrGetPscode2(ScePsCode *pscode);
int sceSblAimgrGetOpenPsId(SceKernelOpenPsId *openPsId);
int SceSblSsMgrForDriver_AC57F4F0(void *result);	/* vitasdk: ksceSblRngGenuineRandomNumber */
/* vitasdk: ksceSblSsCreatePassPhrase */
int SceSblSsMgrForDriver_B8B298FD(const uint32_t *parameters, void *passPhrase);
int SceSblSsMgrForDriver_C38D0CEA(SceUID heap, SceKernelPAVector *vector);
/* vitasdk: ksceSblDmac5AesEcbEnc */
int SceSblSsMgrForDriver_C517770D(const void *src, void *dst, int size, const void *key, int keyBits, int coherent);
/* vitasdk: ksceSblSsMemset */
void *SceSblSsMgrForDriver_CD98CC92(void *dst, int c, SceSize size);
int SceSblSsMgrForDriver_E0DC2587(ScePsCode *pscode);	/* vitasdk: ksceSblAimgrGetPscode */
/* vitasdk: ksceSblDmac5AesCbcEnc */
int SceSblSsMgrForDriver_E6E1AD15(const void *src, void *dst, int size, const void *key, int keyBits, void *iv, int coherent);
/* vitasdk: ksceSblDmac5AesCmacWithKeyslot */
int SceSblSsMgrForDriver_EA6ACB6D(const void *src, void *dst, int size, unsigned int key, unsigned int keyBits, void *iv, int coherent, uint32_t flags);
/* vitasdk: ksceSblDmac5Sha1 */
int SceSblSsMgrForDriver_EB3AF9B5(const void *src, void *dst, int size, void *iv, int coherent, uint32_t flags);
int sceSblAimgrGetConsoleId(SceConsoleId *consoleId);
/* sysmem's sceSblPmMgrGetProductMode (sysmem/pmmgr.h), which SceSblSsMgr exports again */
int SceSblSsMgrForKernel_sceSblPmMgrGetProductMode(SceProductMode *mode);
int SceSblSsMgrForKernel_83D254FF(void *flags);	/* vitasdk: ksceSblQafManagerGetQAFlags */
/* vitasdk: ksceSblQafManagerGetQafName */
int SceSblSsMgrForKernel_E2DD0378(char *name, SceSize size);
int _sceKernelGetOpenPsId(SceKernelOpenPsId *openPsId);
int nvs_read_data(uint32_t offset, void *buffer, uint32_t size);
int nvs_write_data(uint32_t offset, const void *buffer, uint32_t size);
int sceSblDmac5EncDec(SceSblDmac5EncDecParam *param, SceUInt32 command);
int sceSblDmac5EncDecKeyGen(SceSblDmac5EncDecParam *param, int keyId, uint32_t command);
int sceSblDmac5HashTransform(const void *src, void *dst, SceSize size, void *iv, int coherent, uint32_t command, uint32_t flags);
int sceSblDmac5HmacKeyGen(SceSblDmac5HashTransformParam *param, int keyId, uint32_t command, uint32_t flags);
int sceSblDmac5Rnd(void *dst, SceSize size, int coherent);
int sceSblQafManagerDeleteQafTokenForUser(void);
int sceSblQafManagerGetQafNameForUser(char *name, SceSize size);
int sceSblQafManagerIsAllowKernelDebugForUser(void);
int sceSblQafManagerSetQafTokenForUser(void);
int sceSblQafMgrDeleteQafToken2(void);
int sceSblQafMgrGetQafName(char *name, SceSize size);
int sceSblQafMgrGetQafToken(void);
int sceSblQafMgrGetQafToken2(void *token);
int sceSblQafMgrIsAllowAllDebugMenuDisplay(void);
int sceSblQafMgrIsAllowForceUpdate(void);
int sceSblQafMgrIsAllowLimitedDebugMenuDisplay(void);
int sceSblQafMgrIsAllowMinimumDebugMenuDisplay(void);
int sceSblQafMgrIsAllowNonQAPup(void);
int sceSblQafMgrIsAllowNpFullTest(void);
int sceSblQafMgrIsAllowNpTest(void);
int sceSblQafMgrIsAllowRemoteSysmoduleLoad(void);
int sceSblQafMgrIsAllowScreenShotAlways(void);
int sceSblQafMgrSetQafToken2(const void *token);
int sceSblRngPseudoRandomNumber(void *result, SceSize size);
int sceSblSsGetNvsData(uint32_t area, void *buffer, SceSize size);
int sceSblSsInfraAllocatePARangeVector(void *addr, SceSize size, SceUID heap, SceKernelPAVector *vector);
int sceSblSsSetNvsData(uint32_t area, const void *buffer, SceSize size);
