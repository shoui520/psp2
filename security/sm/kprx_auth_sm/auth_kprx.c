/*
 * auth_kprx.c: KPRX Authentication Secure Module (kprx_auth_sm.self) main
 * service entry point, mailbox command handlers (0x10001..0x80001) for KPRX
 * SELF header/segment authentication, Signed param.sfo (SPSFO) verification,
 * EKC decryption, DMAC5 keyring slot provisioning, and bind-data decryption
 * (0x80cbf0..0x80e7ee).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "kprx_auth_sm.h"
#include <modulemgr/elf.h>

/* ============================================================================
 * Internal Types
 * ============================================================================ */

typedef struct SceSelfHeader {
	uint32_t magic;				/* 0x00: 0x00454353 ("SCE\0") */
	uint32_t version;			/* 0x04: 3 */
	uint8_t sdkType;			/* 0x08 */
	uint8_t keyRevision;			/* 0x09 */
	uint16_t headerType;			/* 0x0a: 1 = SELF, 6 = SPSFO */
	uint32_t metadataOffset;		/* 0x0c */
	uint64_t headerLen;			/* 0x10 */
	uint64_t dataLen;			/* 0x18 */
	uint64_t selfFilesize;			/* 0x20 */
	uint64_t unknown28;			/* 0x28 */
	uint64_t selfOffset;			/* 0x30 */
	uint64_t appInfoOffset;			/* 0x38 */
	uint64_t elfOffset;			/* 0x40 */
	uint64_t phdrOffset;			/* 0x48 */
	uint64_t shdrOffset;			/* 0x50 */
	uint64_t segmentInfoOffset;		/* 0x58 */
	uint64_t sceVersionOffset;		/* 0x60 */
	uint64_t controlInfoOffset;		/* 0x68 */
	uint64_t controlInfoSize;		/* 0x70 */
	uint64_t padding78;			/* 0x78 */
} SceSelfHeader;

_Static_assert(sizeof(SceSelfHeader) == 0x80, "SceSelfHeader size");

typedef int (*AuthKprxCmdMethod)(void *mgr, void *packetBuf);

typedef struct SmMemberFnPtr {
	AuthKprxCmdMethod fn;			/* 0x00 */
	int32_t adj;				/* 0x04 */
} SmMemberFnPtr;

typedef struct SmHookSpec {
	uint32_t cmdId;				/* 0x00 */
	SmServiceHookNode *node;		/* 0x04 */
	AuthKprxCmdMethod fn;			/* 0x08 */
	int32_t adj;				/* 0x0c */
} SmHookSpec;

typedef struct SelfAuthSubContext {
	SelfAuthContext base;			/* 0x000..0x007 */
	uint8_t aesKeys[48];			/* 0x008..0x037 */
	uint8_t rsaKey[264];			/* 0x038..0x13f */
	uint8_t rsvd140[8];			/* 0x140..0x147 */
	uint8_t customIv[32];			/* 0x148..0x167 */
	uint32_t customIvSize;			/* 0x168 */
	uint32_t rsvd16c;			/* 0x16c */
	CfSegmentDecryptState streamState;	/* 0x170..0x2cf */
	const SceSmScatterList *hdrScatterList;	/* 0x2d0 */
	uint32_t rsvd2d4;			/* 0x2d4 */
	const SceSmScatterList *srcScatterList;	/* 0x2d8 */
	const SceSmScatterList *dstScatterList;	/* 0x2dc */
	uint64_t segOffset;			/* 0x2e0..0x2e7 */
	uint64_t segSize;			/* 0x2e8..0x2ef */
	uint64_t curOffset;			/* 0x2f0..0x2f7 */
	uint64_t rsvd2f8;			/* 0x2f8..0x2ff */
} SelfAuthSubContext;

_Static_assert(sizeof(SelfAuthSubContext) == 0x300, "SelfAuthSubContext size");

typedef struct AuthKprxManager {
	SelfAuthSubContext kprx;		/* 0x000..0x2ff */
	SelfAuthSubContext spsfo;		/* 0x300..0x5ff */
	uint32_t stateFlags;			/* 0x600 */
} AuthKprxManager;

_Static_assert(sizeof(AuthKprxManager) == 0x608, "AuthKprxManager size");

/* Forward declarations of command handlers and helpers */
void authKprxStaticInit(void);
int authKprxHookInvoke(void *closure, void *packetBuf);
int authKprxAddHook(SmServiceContext *srvCtx, uint32_t cmdId,
		AuthKprxManager *mgr, const SmMemberFnPtr *fnPtr,
		SmServiceHookNode **outNode);
void authKprxReadQaFlags(uint32_t *outWords8);
void authKprxMgrInitC2(AuthKprxManager *mgr);
void authKprxMgrCtor(AuthKprxManager *mgr);
void authKprxMgrDtorD2(AuthKprxManager *mgr);
void authKprxMgrDtor(AuthKprxManager *mgr);
int authKprxVerifyHeaderCore(AuthKprxManager *mgr, uint8_t *pkt);
int authKprxCmdVerifyHeader(void *mgr, void *packetBuf);
int authKprxSetupSegmentLoadCore(AuthKprxManager *mgr, uint8_t *pkt);
int authKprxCmdSetupSegment(void *mgr, void *packetBuf);
int authKprxLoadSegmentBlockCore(AuthKprxManager *mgr, uint8_t *pkt);
int authKprxCmdLoadSegment(void *mgr, void *packetBuf);
uint32_t authKprxCheckQaFlagBit0b04(void);
uint32_t authKprxCheckQaFlagBit0f01(void);
int authKprxScatterReadHeader(AuthKprxManager *mgr, SelfAuthSubContext *subCtx,
		uintptr_t dstAddr, const uint64_t *offset, uint32_t size,
		uint32_t *outRead);
int authKprxReadSpsfoHeaderCb(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg);
int authKprxReadKprxHeaderCb(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg);
int authKprxScatterResolveSegment(AuthKprxManager *mgr, SelfAuthSubContext *subCtx,
		const uint64_t *offset, uint32_t size, uint64_t *outSrcAddr,
		uint64_t *outDstAddr, uint32_t *outChunk, uint32_t *outFlag);
int authKprxReadSpsfoSegmentCb(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outChunk, uint32_t *outFlag, void *arg);
int authKprxReadKprxSegmentCb(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outChunk, uint32_t *outFlag, void *arg);
int authKprxValidateSpsfoCommandPacket(AuthKprxManager *mgr, const uint8_t *pkt);
int authKprxVerifySpsfoHeader(AuthKprxManager *mgr, uint8_t *pkt);
int authKprxCheckSpsfoExtHeaderBounds(AuthKprxManager *mgr,
		const uint32_t *extHdr, uint32_t totalScatterSize);
int authKprxLoadSpsfoSegments(AuthKprxManager *mgr, uint8_t *pkt);
int authKprxVerifySpsfoCore(AuthKprxManager *mgr, const void *packetBuf);
int authKprxCmdVerifySpsfo(void *mgr, void *packetBuf);
int authKprxMapRetailSpsfoKeyRev(uint16_t rawRev, uint16_t *outIdx);
int authKprxResolveSpsfoKey(AuthKprxManager *mgr, const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey);
int authKprxResolveSpsfoKeyCb(const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey, void *arg);
int authKprxInitKeys(AuthKprxManager *mgr);
int authKprxIsAllowedAuthId(const uint64_t *authId);
int authKprxResolveKprxKey(SelfAuthSubContext *subCtx, const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey);
int authKprxResolveKprxKeyCb(const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey, void *arg);
int authKprxValidateArmRange(AuthKprxManager *mgr, uint32_t addr, uint32_t size);
int authKprxAes128EcbDecryptWithSyscall(void *buf, uint32_t size, const void *key);
int authKprxCmdDecryptEkc(void *mgr, void *packetBuf);
int authKprxWriteDmac5KeyringSlot(const uint32_t *keyWords, uint32_t wordCount,
		uint32_t slotId);
int authKprxCmdDecryptBindData(void *mgr, void *packetBuf);
int authKprxAes128CbcEncryptWithSyscall(void *buf, uint32_t size,
		const void *key, const void *iv);
int authKprxSha256ThenAes128CbcEncryptWithSyscall(void *buf, uint32_t size,
		const void *key, const void *iv);
int authKprxCmdSetDmac5Key(void *mgr, void *packetBuf);
int authKprxLookupKprxKey(uint16_t keyRev, uint32_t appType, void *outAes48,
		void *outRsa264, void *inOutRootBlock16, uint32_t isRetail);
int authKprxIsRestrictedAppType0dKeyRev(uint32_t keyRev);

/* ============================================================================
 * .rodata (0x8104d0..0x810748)
 * ============================================================================ */

/* 0x8104d0: C++ vtable for concrete hook node (vtable pointer points to +8 = 0x8104d8) */
static const uintptr_t s_authKprxHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)authKprxHookInvoke,
	0u
};

/* 0x8104e0: C++ vtable for abstract hook base node */
static const uintptr_t s_authKprxBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u
};

/* 0x8104f0..0x810748: allowed KPRX AuthIDs for restricted AppType 0x0d keyRevs */
static const uint64_t s_allowedKprxAuthIds[75] = {
	0x2800800000000001ULL, 0x2800800000000002ULL, 0x2800800000000003ULL,
	0x2800800000000013ULL, 0x2800800000000016ULL, 0x2800800000000017ULL,
	0x2800000000000032ULL, 0x2800000000000030ULL, 0x2801000000020001ULL,
	0x2801000000000010ULL, 0x2801000000000011ULL, 0x2801000000000012ULL,
	0x2801000000000013ULL, 0x2801000000000014ULL, 0x2801000000000015ULL,
	0x2801000000000016ULL, 0x2801000000000017ULL, 0x2801000000000018ULL,
	0x2801000000000019ULL, 0x280100000000001aULL, 0x2801000000020002ULL,
	0x280100000000003dULL, 0x280100000000001bULL, 0x2801000000000043ULL,
	0x280100000000003eULL, 0x280100000000001cULL, 0x280100000000001dULL,
	0x280100000000001eULL, 0x280100000000001fULL, 0x2801000000000020ULL,
	0x2801000000020003ULL, 0x2801000000020004ULL, 0x2801000000020005ULL,
	0x2801000000020006ULL, 0x280100000002000bULL, 0x2801000000020007ULL,
	0x280100000002000cULL, 0x2801000000020008ULL, 0x280100000002000dULL,
	0x280100000002000aULL, 0x2801000000020009ULL, 0x280100000002000eULL,
	0x280100000000003fULL, 0x2801000000000021ULL, 0x2801000000000022ULL,
	0x2801000000000023ULL, 0x2801000000000024ULL, 0x2801000000000025ULL,
	0x2801000000000026ULL, 0x2801000000000027ULL, 0x2801000000000028ULL,
	0x2801000000000029ULL, 0x280100000000002aULL, 0x280100000000002bULL,
	0x2801000000000043ULL, 0x280100000000002cULL, 0x280100000000002dULL,
	0x280100000000002eULL, 0x280100000000002fULL, 0x2801000000000040ULL,
	0x2801000000000030ULL, 0x2801000000000031ULL, 0x2801000000000032ULL,
	0x2801000000000033ULL, 0x2801000000000034ULL, 0x2801000000000035ULL,
	0x2801000000000036ULL, 0x2801000000000037ULL, 0x2801000000000038ULL,
	0x2801000000000044ULL, 0x2801000000000041ULL, 0x2801000000000039ULL,
	0x280100000000003aULL, 0x280100000000003bULL, 0x280100000000003cULL,
};


/* ============================================================================
 * .data (0x810914..0x81305c)
 * ============================================================================ */

/* 0x810914..0x810994: 8 mailbox command hook specifications */
static SmHookSpec s_authKprxHookSpecs[8] = {
	{ 0x00010001u, NULL, authKprxCmdVerifyHeader,   0 },
	{ 0x00020001u, NULL, authKprxCmdSetupSegment,   0 },
	{ 0x00030001u, NULL, authKprxCmdLoadSegment,    0 },
	{ 0x00040001u, NULL, authKprxCmdDecryptEkc,     0 },
	{ 0x00050001u, NULL, authKprxCmdSetDmac5Key,    0 },
	{ 0x00060001u, NULL, authKprxCmdSetDmac5Key,    0 },
	{ 0x00070001u, NULL, authKprxCmdDecryptBindData, 0 },
	{ 0x00080001u, NULL, authKprxCmdVerifySpsfo,    0 },
};

/* 0x810994: Signed param.sfo (SPSFO) dev/prototype keys (keyRev 0..5) */
static KprxCfKeyEntry s_spsfoDevKeys[6] = {
	{
		{
			0x3au, 0xe9u, 0x7fu, 0xbeu, 0xb7u, 0x71u, 0x0eu, 0xa3u,
			0xc5u, 0xcfu, 0x51u, 0x9fu, 0x0cu, 0x70u, 0x3du, 0xacu,
			0xf0u, 0x64u, 0x3fu, 0xa2u, 0x2eu, 0x74u, 0x4au, 0x1eu,
			0xceu, 0x33u, 0xa1u, 0x10u, 0x19u, 0x32u, 0xe6u, 0xa9u,
			0xdau, 0x69u, 0x04u, 0x7bu, 0xaau, 0xfcu, 0xbdu, 0x94u,
			0x23u, 0xb7u, 0xd5u, 0x26u, 0x15u, 0x5bu, 0xadu, 0x39u,
		},
		{
			0x8fa989dbu, 0x59814227u, 0x1ddcc5e9u, 0x799a0c0eu,
			0xb2ac586cu, 0x240c8d7cu, 0x2962cf97u, 0x15c9531fu,
			0x2a9d698fu, 0x125c1262u, 0xcca13133u, 0x9362e345u,
			0x11a827b9u, 0x6c9ea1adu, 0x0a4b3859u, 0xd7335405u,
			0x7fe039d0u, 0x7ea61f77u, 0x5a6f280bu, 0x675cfdf7u,
			0x0c7d5c80u, 0x2bdbb416u, 0x27d3aec9u, 0xeeb83a37u,
			0x5ae847e3u, 0xcad7ed32u, 0x0fb06ec1u, 0xe7c26482u,
			0xf185e1c8u, 0x904f8593u, 0x63ecb0b2u, 0x8121b313u,
			0xe9cd10a8u, 0x5be3531eu, 0x6f7654c4u, 0x12550d07u,
			0xe16e8a02u, 0xa300ca48u, 0x54c7369bu, 0xc9fe48aau,
			0x047eecd1u, 0x171e977bu, 0xed540d78u, 0x2335e632u,
			0xe1df891fu, 0x1ff6e724u, 0x9265fb86u, 0x7a472fbfu,
			0xc3619a29u, 0x201e5d46u, 0xfa31f673u, 0x8aa9f5f8u,
			0xd75290e7u, 0x817da143u, 0xaf911db4u, 0x85316507u,
			0x76b56b68u, 0xfd18ab7eu, 0x76c6317cu, 0xa49134d2u,
			0x9667451bu, 0x934d7743u, 0x9d003a03u, 0xb50a1b6au,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x79u, 0xdau, 0x5du, 0x9bu, 0x4eu, 0x1cu, 0xc3u, 0x6cu,
			0x4du, 0x1eu, 0xfdu, 0x36u, 0xbau, 0xbcu, 0xa1u, 0xbeu,
			0xb7u, 0x84u, 0xd7u, 0x91u, 0x15u, 0xccu, 0x73u, 0xb2u,
			0xa8u, 0x11u, 0x03u, 0xc7u, 0x37u, 0x0eu, 0x63u, 0xfau,
			0x50u, 0x97u, 0x70u, 0x96u, 0xe2u, 0xb7u, 0x47u, 0x40u,
			0x35u, 0xf6u, 0xb4u, 0xd9u, 0x43u, 0x09u, 0x13u, 0x52u,
		},
		{
			0x7d028edfu, 0x7806bf2au, 0xf23a21d8u, 0xa0172d7cu,
			0xa5216952u, 0x0017e1ccu, 0xf5e879fdu, 0xcac45ea3u,
			0xebf97f96u, 0xda13bf3bu, 0x7acb0a7cu, 0x83af0ee5u,
			0xab011444u, 0xbac79ed7u, 0xceec959fu, 0x44245b41u,
			0xe8d16e55u, 0x8e67f5aeu, 0x1edee2f7u, 0x12ff7879u,
			0xd8fe690eu, 0xfe0a0332u, 0xc495ad86u, 0x5d123172u,
			0x5a4204cau, 0x2ad2556eu, 0x6ad0cc40u, 0x53dd8cf2u,
			0xca86b783u, 0x375f6050u, 0xc724e8c3u, 0xccaee30cu,
			0x52a85518u, 0x93200eebu, 0x57b4a75du, 0xc667b471u,
			0x3202a7b8u, 0x5ccdbd0eu, 0xc62564feu, 0x0eb593c5u,
			0xb7281e23u, 0x8c30fe3du, 0xe1c47b08u, 0x18b2641eu,
			0x6bfcf0c2u, 0xf0ef919eu, 0x2e6af29bu, 0x628a7110u,
			0xb7306d21u, 0xd3b36bd9u, 0x2e2f465cu, 0xf5615d21u,
			0x01646dd2u, 0x9f7190fbu, 0x0c55dcc3u, 0x258352bau,
			0xd0ffba57u, 0xb8be8d67u, 0x4509509bu, 0xc6c0b890u,
			0x46710c7eu, 0x01cf86b0u, 0xad85e2e7u, 0x671d8f1bu,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x3au, 0xe9u, 0x7fu, 0xbeu, 0xb7u, 0x71u, 0x0eu, 0xa3u,
			0xc5u, 0xcfu, 0x51u, 0x9fu, 0x0cu, 0x70u, 0x3du, 0xacu,
			0xf0u, 0x64u, 0x3fu, 0xa2u, 0x2eu, 0x74u, 0x4au, 0x1eu,
			0xceu, 0x33u, 0xa1u, 0x10u, 0x19u, 0x32u, 0xe6u, 0xa9u,
			0xdau, 0x69u, 0x04u, 0x7bu, 0xaau, 0xfcu, 0xbdu, 0x94u,
			0x23u, 0xb7u, 0xd5u, 0x26u, 0x15u, 0x5bu, 0xadu, 0x39u,
		},
		{
			0x8fa989dbu, 0x59814227u, 0x1ddcc5e9u, 0x799a0c0eu,
			0xb2ac586cu, 0x240c8d7cu, 0x2962cf97u, 0x15c9531fu,
			0x2a9d698fu, 0x125c1262u, 0xcca13133u, 0x9362e345u,
			0x11a827b9u, 0x6c9ea1adu, 0x0a4b3859u, 0xd7335405u,
			0x7fe039d0u, 0x7ea61f77u, 0x5a6f280bu, 0x675cfdf7u,
			0x0c7d5c80u, 0x2bdbb416u, 0x27d3aec9u, 0xeeb83a37u,
			0x5ae847e3u, 0xcad7ed32u, 0x0fb06ec1u, 0xe7c26482u,
			0xf185e1c8u, 0x904f8593u, 0x63ecb0b2u, 0x8121b313u,
			0xe9cd10a8u, 0x5be3531eu, 0x6f7654c4u, 0x12550d07u,
			0xe16e8a02u, 0xa300ca48u, 0x54c7369bu, 0xc9fe48aau,
			0x047eecd1u, 0x171e977bu, 0xed540d78u, 0x2335e632u,
			0xe1df891fu, 0x1ff6e724u, 0x9265fb86u, 0x7a472fbfu,
			0xc3619a29u, 0x201e5d46u, 0xfa31f673u, 0x8aa9f5f8u,
			0xd75290e7u, 0x817da143u, 0xaf911db4u, 0x85316507u,
			0x76b56b68u, 0xfd18ab7eu, 0x76c6317cu, 0xa49134d2u,
			0x9667451bu, 0x934d7743u, 0x9d003a03u, 0xb50a1b6au,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x01u, 0xc3u, 0xe0u, 0x20u, 0xd7u, 0xfeu, 0xb0u, 0xf9u,
			0xf4u, 0x94u, 0xe7u, 0x76u, 0xf3u, 0x95u, 0xd3u, 0x77u,
			0x73u, 0x5fu, 0x77u, 0xe1u, 0xdcu, 0x8eu, 0x5eu, 0x67u,
			0x10u, 0x06u, 0xa9u, 0x14u, 0xb9u, 0xe5u, 0x5eu, 0x77u,
			0x2au, 0xacu, 0x92u, 0xb2u, 0x68u, 0x42u, 0xd8u, 0xbfu,
			0xbdu, 0x21u, 0xe3u, 0xd4u, 0x29u, 0x8bu, 0x14u, 0x58u,
		},
		{
			0x545787ddu, 0xf42a3b12u, 0xcfd85f60u, 0x0027bc45u,
			0x03169758u, 0xc7414014u, 0x6520f54fu, 0x0ab191f0u,
			0x55035668u, 0xe5be2b8fu, 0x12aa4c50u, 0x1341c93cu,
			0x7f543f80u, 0x1787092eu, 0x085c4026u, 0xcd9985beu,
			0xf09be46du, 0x2bee5a96u, 0x2bd41dfdu, 0xa3ffcf42u,
			0xf28836f6u, 0x00fc8f82u, 0x344d52e8u, 0xbc6729d4u,
			0x161100c4u, 0xc7fd6e67u, 0xa4063693u, 0x6f768d49u,
			0x45b7d080u, 0x3bb9f65au, 0x24fac0deu, 0xdc9d026du,
			0xa113f935u, 0x82f9cc2eu, 0x53872aeeu, 0x1a6db155u,
			0x0eef2b6du, 0x9abebea8u, 0x08c358a4u, 0x50e52dd6u,
			0x850cc76au, 0xdc909654u, 0x5a7f7540u, 0x4113d24du,
			0xb9943434u, 0xc4919bbcu, 0xa1d6c734u, 0x582ccd6cu,
			0xb2f5aed2u, 0x29f47606u, 0x1cd3359bu, 0x86bd6bbdu,
			0xcd12292eu, 0x819b5d2eu, 0xb89197d0u, 0x7386e235u,
			0x432dab31u, 0x4f78567fu, 0x18971b97u, 0x787ce28fu,
			0x119608ebu, 0xd646efedu, 0xad3a0a8au, 0x0d555bdau,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x3au, 0xe9u, 0x7fu, 0xbeu, 0xb7u, 0x71u, 0x0eu, 0xa3u,
			0xc5u, 0xcfu, 0x51u, 0x9fu, 0x0cu, 0x70u, 0x3du, 0xacu,
			0xf0u, 0x64u, 0x3fu, 0xa2u, 0x2eu, 0x74u, 0x4au, 0x1eu,
			0xceu, 0x33u, 0xa1u, 0x10u, 0x19u, 0x32u, 0xe6u, 0xa9u,
			0xdau, 0x69u, 0x04u, 0x7bu, 0xaau, 0xfcu, 0xbdu, 0x94u,
			0x23u, 0xb7u, 0xd5u, 0x26u, 0x15u, 0x5bu, 0xadu, 0x39u,
		},
		{
			0x8fa989dbu, 0x59814227u, 0x1ddcc5e9u, 0x799a0c0eu,
			0xb2ac586cu, 0x240c8d7cu, 0x2962cf97u, 0x15c9531fu,
			0x2a9d698fu, 0x125c1262u, 0xcca13133u, 0x9362e345u,
			0x11a827b9u, 0x6c9ea1adu, 0x0a4b3859u, 0xd7335405u,
			0x7fe039d0u, 0x7ea61f77u, 0x5a6f280bu, 0x675cfdf7u,
			0x0c7d5c80u, 0x2bdbb416u, 0x27d3aec9u, 0xeeb83a37u,
			0x5ae847e3u, 0xcad7ed32u, 0x0fb06ec1u, 0xe7c26482u,
			0xf185e1c8u, 0x904f8593u, 0x63ecb0b2u, 0x8121b313u,
			0xe9cd10a8u, 0x5be3531eu, 0x6f7654c4u, 0x12550d07u,
			0xe16e8a02u, 0xa300ca48u, 0x54c7369bu, 0xc9fe48aau,
			0x047eecd1u, 0x171e977bu, 0xed540d78u, 0x2335e632u,
			0xe1df891fu, 0x1ff6e724u, 0x9265fb86u, 0x7a472fbfu,
			0xc3619a29u, 0x201e5d46u, 0xfa31f673u, 0x8aa9f5f8u,
			0xd75290e7u, 0x817da143u, 0xaf911db4u, 0x85316507u,
			0x76b56b68u, 0xfd18ab7eu, 0x76c6317cu, 0xa49134d2u,
			0x9667451bu, 0x934d7743u, 0x9d003a03u, 0xb50a1b6au,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0xa8u, 0xe2u, 0xc3u, 0x32u, 0x5bu, 0xc2u, 0x32u, 0x37u,
			0xb0u, 0x66u, 0xd4u, 0xd7u, 0x6cu, 0xb7u, 0x8bu, 0xbbu,
			0x18u, 0xb3u, 0x76u, 0x08u, 0xedu, 0xc0u, 0xc2u, 0xd5u,
			0xe4u, 0x31u, 0xc4u, 0x93u, 0x5bu, 0x77u, 0xceu, 0x8fu,
			0x36u, 0x14u, 0xf7u, 0xddu, 0xa7u, 0xbau, 0x82u, 0x63u,
			0x5fu, 0x37u, 0xd4u, 0x5cu, 0x87u, 0x33u, 0xb4u, 0x78u,
		},
		{
			0x8198e2c3u, 0x52674bf8u, 0xb05a58e1u, 0xd8daac0bu,
			0xb8fe50deu, 0x5811aa38u, 0x0ae94e6au, 0x6e14d83au,
			0x641dfa19u, 0xebf74545u, 0x38405fa5u, 0x6e649d34u,
			0xf4ba0723u, 0x1b51094eu, 0xba6a493bu, 0x88ca141du,
			0xafa7f199u, 0xbc960bf5u, 0x077f8301u, 0xbf98782fu,
			0x07a52f93u, 0x613ff26du, 0x02f6d455u, 0xc3bcdc81u,
			0x6d888d41u, 0x23f24f9au, 0x4357cf57u, 0x86234060u,
			0x83410b3fu, 0xf38a09b3u, 0x3ec19589u, 0xefbe6b93u,
			0xd097b65bu, 0x50a1c932u, 0xf328b502u, 0x3119a05bu,
			0x44360404u, 0x779657f0u, 0x5241b4f5u, 0x90036933u,
			0x227c100cu, 0xd7823ff3u, 0x616a936du, 0x1c1e0eeau,
			0xfb377f2du, 0xaec14b0eu, 0x6b1865f2u, 0x5c70a951u,
			0xfab06398u, 0x2227bff8u, 0x9024d3f5u, 0xfb01912du,
			0xbd75c9a9u, 0x2aa76fb6u, 0x535f84efu, 0x0d70fee3u,
			0xc371cd38u, 0x7fa2add6u, 0xe6bd4f33u, 0x058a0da6u,
			0x759dd2e8u, 0xeaaa15a6u, 0x72b9588cu, 0x555f006du,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
};

/* 0x8110fc: Signed param.sfo (SPSFO) retail keys (mapped from keyRev 1, 3, 5) */
static KprxCfKeyEntry s_spsfoRetailKeys[3] = {
	{
		{
			0x6bu, 0x82u, 0x01u, 0x99u, 0x56u, 0x65u, 0x8cu, 0xb2u,
			0xb0u, 0xb4u, 0x42u, 0xb5u, 0x7au, 0xeau, 0xa2u, 0xfau,
			0x68u, 0x72u, 0xbau, 0xa1u, 0xf0u, 0xf1u, 0x3eu, 0xf6u,
			0x43u, 0x36u, 0x07u, 0x5du, 0x70u, 0x8fu, 0xd9u, 0x17u,
			0x60u, 0x5eu, 0xa7u, 0x32u, 0x7bu, 0xddu, 0x9du, 0x19u,
			0xd9u, 0x3eu, 0x36u, 0x3bu, 0x36u, 0x36u, 0x07u, 0x45u,
		},
		{
			0xe6a37b9bu, 0xc4df13a5u, 0x1589d986u, 0xa5278bffu,
			0x97be6fe4u, 0x71523abeu, 0x5577bab8u, 0x4ccd6bf9u,
			0x230ee28fu, 0x3b7a1d3au, 0x95c26895u, 0x79225e95u,
			0x72228761u, 0xc9cf45d3u, 0xf9e4b668u, 0xd7ade19du,
			0x4a1abf5du, 0xd2f8a5c1u, 0x8c6b7011u, 0x8cff1bdeu,
			0x53a829cbu, 0xd3a8bd6fu, 0xb5147069u, 0x0bf4b20fu,
			0x558b2f0au, 0x63b5291cu, 0xc650d73du, 0x3258c932u,
			0x05ff118au, 0xff7c2606u, 0xe22a1cadu, 0x0aed6959u,
			0x944bd8b4u, 0xda28cffau, 0x088eb22eu, 0x747feb4fu,
			0x2b02ed2au, 0x6d14d75eu, 0x66847e2bu, 0x48bfe6cdu,
			0xf3c240d2u, 0x8f393b23u, 0x4a431ec4u, 0x2cc88744u,
			0xb5eccecbu, 0xa4f0ad40u, 0x0aee24a1u, 0x001fa331u,
			0x17836211u, 0x6c497747u, 0x9937bbcfu, 0xdbcf84a1u,
			0xae38362du, 0x655b2197u, 0xe478aaefu, 0x1d00fa51u,
			0xa5eabdb1u, 0x3d14b384u, 0xe4af73fdu, 0xaa11398eu,
			0x900d2857u, 0x8fb7386bu, 0xedeaac9du, 0xdbc6f4e4u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x8au, 0x5cu, 0x70u, 0x84u, 0x95u, 0x28u, 0xb6u, 0x13u,
			0x42u, 0x02u, 0x77u, 0xbbu, 0x7bu, 0x78u, 0x67u, 0x35u,
			0x1du, 0xe8u, 0x5fu, 0x40u, 0x69u, 0x74u, 0x98u, 0x83u,
			0xd0u, 0x75u, 0x78u, 0xfcu, 0x79u, 0x4du, 0x13u, 0x89u,
			0x90u, 0x4bu, 0x1du, 0x8eu, 0x8fu, 0xfeu, 0x90u, 0xf8u,
			0xf0u, 0x92u, 0xbfu, 0xd7u, 0xf0u, 0x9du, 0x32u, 0xf6u,
		},
		{
			0xcf96ded8u, 0x86c085c7u, 0x1f6dd848u, 0x7bb51dabu,
			0x2651521cu, 0xb9367f76u, 0xf6c427a4u, 0x3d71190au,
			0xba6880f8u, 0x90023f0eu, 0xc08af08bu, 0x28e08570u,
			0xf6260a74u, 0x48c94058u, 0x6ae15351u, 0x6a1e24d9u,
			0xb4f5eae9u, 0x188cf1f1u, 0x73cfead2u, 0x2713f7e8u,
			0xd29ddacau, 0x89c4473eu, 0x959e9636u, 0xd5672750u,
			0xe31d7086u, 0xee4760bdu, 0x186bae1eu, 0x5198c3d0u,
			0x024edbe9u, 0x6bedc271u, 0x756ac5ecu, 0x93554a29u,
			0x39a5be1bu, 0xcc14628fu, 0xbdc1de5bu, 0xff7cf669u,
			0x019da92bu, 0x3966067eu, 0xeac772b2u, 0x8af2dc06u,
			0x28d71646u, 0xafd6cab5u, 0x4ff56c24u, 0xa5d1093cu,
			0x2e3737beu, 0x1a485f15u, 0xed956267u, 0x4f88a371u,
			0x5a94be7eu, 0x1e540cc7u, 0xac850b11u, 0x12819c2bu,
			0x2bf620d7u, 0xd5334de7u, 0xd8040837u, 0x52ec5e5au,
			0xa4cd0fe9u, 0x594bde62u, 0xf00aad1cu, 0xe4e10522u,
			0x7495e5cbu, 0x2daa9177u, 0x716febaau, 0x437909ffu,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0xbeu, 0x0au, 0xdcu, 0xf4u, 0x78u, 0x37u, 0xd2u, 0xa2u,
			0x68u, 0xfcu, 0xa3u, 0xa7u, 0x90u, 0xcau, 0x60u, 0xc3u,
			0xb4u, 0x36u, 0xcbu, 0x76u, 0x73u, 0x6bu, 0x60u, 0x2du,
			0x45u, 0xfau, 0xd6u, 0x00u, 0xe6u, 0x82u, 0x3cu, 0x61u,
			0x8fu, 0x6bu, 0x6eu, 0xe9u, 0x2du, 0x98u, 0xa7u, 0xd7u,
			0xc0u, 0xb5u, 0x59u, 0xd1u, 0x10u, 0x2eu, 0x80u, 0x9bu,
		},
		{
			0x7e06ace1u, 0x85fb698fu, 0x507ec49bu, 0x9cbf1175u,
			0x38527511u, 0x7ef62ec4u, 0xaca6f773u, 0x79b57636u,
			0xc9f4a9b0u, 0x452d6bbeu, 0x37504286u, 0x2301fd87u,
			0x57f1e65fu, 0xc77e4c0du, 0x668ce7bau, 0xa90cd52cu,
			0x17aa5a02u, 0x8071ababu, 0xe8392e68u, 0x6ff02f36u,
			0x47e73e19u, 0xcdca0927u, 0x4f083943u, 0x6c1b24adu,
			0x830a246cu, 0xd99e754fu, 0xcf71aea7u, 0x1bef7787u,
			0x9d99e246u, 0x23d37a1cu, 0x164e81b9u, 0xc19fc62fu,
			0x3c005823u, 0x53ba0dccu, 0x91789b5cu, 0x1ede8b5eu,
			0xb222ddccu, 0xba8f73f0u, 0x79279c99u, 0x53661da1u,
			0x5748317fu, 0xbe1290b2u, 0xc207f0f5u, 0x7d2325c1u,
			0x415114d4u, 0x4320cdebu, 0xb7ea9c0au, 0xc79fc629u,
			0x49235b2au, 0x8267e957u, 0x390d96b2u, 0x1b03fe66u,
			0xa8fac4cdu, 0x0e8566a8u, 0xcc9b47eeu, 0x986f2f0au,
			0x92dc8ee7u, 0xd2951751u, 0x4cb84a56u, 0x30d8853du,
			0x2c069fe9u, 0x13160670u, 0xe9cc39d1u, 0xc1e00ec7u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
};

/* 0x8114b0: AppType 0x0d keyRevs restricted by s_allowedKprxAuthIds */
static uint32_t s_appType0dRestrictedKeyRevs[3] __attribute__((section(".data"))) = { 1u, 2u, 3u };

/* 0x8114bc: KPRX AppType 0x07 CF key entry */
static KprxCfKeyEntry s_kprxAppType07Key = {
	{
		0x74u, 0xf6u, 0xd2u, 0xa1u, 0xd2u, 0xa0u, 0x93u, 0xaeu,
		0x32u, 0xb8u, 0x33u, 0x37u, 0xe0u, 0xaeu, 0x4au, 0xd2u,
		0xe6u, 0xd9u, 0x3bu, 0x03u, 0x4fu, 0x5bu, 0xf3u, 0xb6u,
		0x8du, 0xb7u, 0x71u, 0x31u, 0x88u, 0x33u, 0x10u, 0xd4u,
		0x92u, 0x6au, 0xb5u, 0x5bu, 0xdau, 0xdcu, 0x45u, 0xdbu,
		0xb6u, 0x10u, 0xe9u, 0x0eu, 0x56u, 0xa0u, 0x36u, 0x8cu,
	},
	{
		0xa785f3c2u, 0x5bba3a0cu, 0x748340efu, 0x22b09cc2u,
		0xe2b1b2ddu, 0x17064f4cu, 0xfc7ea34au, 0x4befaa38u,
		0x863c81d1u, 0xb81b0b50u, 0x13fdc6e7u, 0x71e03efdu,
		0xc484c32bu, 0x7df68834u, 0x88b03912u, 0x63724cd3u,
		0x40f71286u, 0x738e212bu, 0x9f93023du, 0x87f03eebu,
		0x1b484f60u, 0x252178d1u, 0x52fe8585u, 0x4790dcdfu,
		0x346f3998u, 0xcacb69dcu, 0x3b8a2d78u, 0x49abb1beu,
		0x0400dc80u, 0xbd775e43u, 0xd6c9feddu, 0x06fa80bbu,
		0x51570b5eu, 0xef236c41u, 0xfc0dc4beu, 0x8650b042u,
		0x7ace1cccu, 0x40c2cff7u, 0x05cefc4du, 0x6fd5bff5u,
		0x73a9ea3fu, 0x35ac3685u, 0x8b020d6eu, 0x49c49840u,
		0xcb96e012u, 0x90d27535u, 0x5ddb060fu, 0x6bb3d3deu,
		0x86f44468u, 0xba53970bu, 0xb7ec8199u, 0x2838db4eu,
		0x1bed6d47u, 0x4d78a9d9u, 0x01c8693bu, 0xe53d8c68u,
		0x05ca2583u, 0xc3d90c16u, 0x37f1be62u, 0xb9e5e040u,
		0x2ba6142cu, 0xa7cd5f92u, 0x06b1b254u, 0x1def42bau,
	},
	0x00000100u,
	{ 0x00u, 0x01u, 0x00u, 0x01u },
	0x00000004u
};

/* 0x8115f8: KPRX AppType 0x0e CF key entry */
static KprxCfKeyEntry s_kprxAppType0eKey = {
	{
		0xd5u, 0x7fu, 0xbbu, 0x5au, 0x9fu, 0x82u, 0x34u, 0x0cu,
		0x15u, 0xd0u, 0x70u, 0x46u, 0x72u, 0x7bu, 0xb0u, 0x49u,
		0x09u, 0x86u, 0x86u, 0x20u, 0x17u, 0xdeu, 0x8au, 0x15u,
		0xebu, 0xd7u, 0xf2u, 0x76u, 0x56u, 0x21u, 0x04u, 0xa4u,
		0x9bu, 0x87u, 0xd6u, 0x84u, 0x66u, 0xcau, 0x50u, 0x8au,
		0xc4u, 0x2fu, 0x2au, 0x90u, 0x1du, 0x29u, 0xd2u, 0x06u,
	},
	{
		0x8475f8cdu, 0xa6f459bcu, 0xa3a51052u, 0x7939a107u,
		0xe1e897f0u, 0xc727a323u, 0x07f7a295u, 0xef70ad15u,
		0x3b858e10u, 0xb748a912u, 0x4bef17b5u, 0x806a1adfu,
		0x09e81327u, 0x0ea52701u, 0x16c5afe4u, 0xa4455f55u,
		0x6516839au, 0xba82d330u, 0xa340347au, 0x455c6217u,
		0xfe746efcu, 0xb7bc5fcau, 0x0741803du, 0xd0a3a914u,
		0xd666b2c3u, 0x2d6547afu, 0xd1dbb58cu, 0x75e76c86u,
		0x275db078u, 0xc74ab34du, 0x7408d3eeu, 0xf5836f1fu,
		0xf8aa5e58u, 0x781ee3cbu, 0x49363349u, 0xde32b16cu,
		0x06db0c96u, 0x597d41b4u, 0x5e7e78e5u, 0x3148f7ceu,
		0x55742e10u, 0x9103df59u, 0x9c89affcu, 0x8500568du,
		0x6e2e9600u, 0x530becbbu, 0x10989aa9u, 0x0f28a24au,
		0x467d663au, 0xdeb93649u, 0x6a50c359u, 0xd22be02du,
		0x35525688u, 0x36ad8812u, 0x0504e516u, 0x1cd375d9u,
		0xb4a61d13u, 0x1d1a8628u, 0x3e2c272du, 0x0dcc0845u,
		0x706f8e68u, 0x329a3764u, 0x5bc52f08u, 0x534bc7e7u,
	},
	0x00000100u,
	{ 0x00u, 0x01u, 0x00u, 0x01u },
	0x00000004u
};

/* 0x811734: KPRX AppType 0x0d CF key entries (keyRev 0..5) */
static KprxCfKeyEntry s_kprxAppType0dKeys[6] = {
	{
		{
			0x32u, 0x2du, 0x70u, 0x6cu, 0xb6u, 0xebu, 0xeau, 0x14u,
			0xdeu, 0xf7u, 0xbfu, 0xe4u, 0x5fu, 0x81u, 0x29u, 0x71u,
			0x34u, 0x7du, 0xc9u, 0x5cu, 0xd7u, 0x69u, 0x7cu, 0x16u,
			0xa7u, 0x1eu, 0xa4u, 0xb2u, 0xa1u, 0xe1u, 0x2cu, 0x0du,
			0x31u, 0xfau, 0x2eu, 0x60u, 0x60u, 0x31u, 0xedu, 0xf3u,
			0x96u, 0x65u, 0xb5u, 0x61u, 0x6eu, 0x9fu, 0x93u, 0x7du,
		},
		{
			0xb1c3f0adu, 0x83484470u, 0xdc5e7306u, 0x6f320b7au,
			0xaf99ad3bu, 0x39236b58u, 0x922cb858u, 0xc53ede1du,
			0xab9db667u, 0x4f67fc66u, 0xdd994e7cu, 0x44926749u,
			0x712c018fu, 0x218ee636u, 0x25daefcfu, 0xec184d7du,
			0xb90b04a7u, 0xb95a2c97u, 0xe7ce0bb3u, 0xad9a3c7cu,
			0xb734bb6cu, 0x156637a8u, 0xd01d049bu, 0xea4abd2eu,
			0x181f49cbu, 0xfc40e618u, 0xa145c1b1u, 0x2cb99349u,
			0x7d20d505u, 0x46ef9f6bu, 0x481a620eu, 0xdd383440u,
			0xa8e7ca25u, 0xbc014093u, 0xad98ec89u, 0xc33614bbu,
			0xebd4c01au, 0xb52b59b5u, 0xa2f8ae48u, 0xbc855558u,
			0x25463f1bu, 0xb8e35be3u, 0xe3b73059u, 0xe438aa6fu,
			0x191db2b1u, 0x7663d660u, 0x6fcfb1fau, 0x6433cb0eu,
			0xd8b4d53bu, 0x632383dfu, 0xaf617b4fu, 0x8f92f8bcu,
			0xf19378c0u, 0xc385140cu, 0x761a9615u, 0xc2495bc2u,
			0x4883ecd0u, 0x0574f6f3u, 0xfc707268u, 0x502bcca1u,
			0x62cfb8c0u, 0x2b855ddeu, 0xdee9ad61u, 0xb37cf1e8u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x54u, 0x27u, 0xf9u, 0x58u, 0x37u, 0x90u, 0x42u, 0xcdu,
			0x9bu, 0x17u, 0x76u, 0x2eu, 0x29u, 0x10u, 0xbau, 0x41u,
			0xc9u, 0xf1u, 0x07u, 0xf3u, 0xfcu, 0x86u, 0xd9u, 0xb7u,
			0x01u, 0x88u, 0x72u, 0x23u, 0x5au, 0x3cu, 0x91u, 0x17u,
			0xbeu, 0x12u, 0xf4u, 0x4cu, 0xf2u, 0x1au, 0xd3u, 0x02u,
			0x6cu, 0xd7u, 0xc5u, 0xe1u, 0x85u, 0x83u, 0xb1u, 0x27u,
		},
		{
			0xd7de0fceu, 0xecc7fab8u, 0x254f6ba6u, 0x4876030fu,
			0x256d13f8u, 0xb9e02cb4u, 0xee72424cu, 0x30630ef3u,
			0x99cba997u, 0x0470aafau, 0x5ef85529u, 0x55d02ad4u,
			0x37496fd1u, 0xf4daad99u, 0x9ad5e735u, 0xff634667u,
			0xcdd2ca72u, 0x61f340b3u, 0xcae74fd0u, 0x896a67c6u,
			0x1c13b094u, 0x5acd3e28u, 0xf55682deu, 0x5f7642ecu,
			0x2ec10879u, 0xc15ca076u, 0x5afe3372u, 0x864fd3acu,
			0x0b7a5361u, 0xe43b77e5u, 0x6053a4d2u, 0x7045ed3eu,
			0x3bb7e820u, 0x2cee2a83u, 0xba92dfa0u, 0x7a20a382u,
			0x597e1397u, 0xfda304e5u, 0x724302e2u, 0x88824464u,
			0xce532180u, 0x15f32af1u, 0x11357bceu, 0xff82d985u,
			0x1e317d5au, 0x190c036fu, 0x78e58c27u, 0x39bbbb15u,
			0x7897c031u, 0xb4ef5142u, 0x7c66347du, 0x3a6db4a7u,
			0x5c718835u, 0x23f16b77u, 0x47aa8b84u, 0xc9e82158u,
			0x740ac8ffu, 0xcd1e81e0u, 0x355f0c3bu, 0xfa546201u,
			0x76e8ac42u, 0x70e154f4u, 0x4e67335eu, 0xfb930255u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x32u, 0x2du, 0x70u, 0x6cu, 0xb6u, 0xebu, 0xeau, 0x14u,
			0xdeu, 0xf7u, 0xbfu, 0xe4u, 0x5fu, 0x81u, 0x29u, 0x71u,
			0x34u, 0x7du, 0xc9u, 0x5cu, 0xd7u, 0x69u, 0x7cu, 0x16u,
			0xa7u, 0x1eu, 0xa4u, 0xb2u, 0xa1u, 0xe1u, 0x2cu, 0x0du,
			0x31u, 0xfau, 0x2eu, 0x60u, 0x60u, 0x31u, 0xedu, 0xf3u,
			0x96u, 0x65u, 0xb5u, 0x61u, 0x6eu, 0x9fu, 0x93u, 0x7du,
		},
		{
			0xb1c3f0adu, 0x83484470u, 0xdc5e7306u, 0x6f320b7au,
			0xaf99ad3bu, 0x39236b58u, 0x922cb858u, 0xc53ede1du,
			0xab9db667u, 0x4f67fc66u, 0xdd994e7cu, 0x44926749u,
			0x712c018fu, 0x218ee636u, 0x25daefcfu, 0xec184d7du,
			0xb90b04a7u, 0xb95a2c97u, 0xe7ce0bb3u, 0xad9a3c7cu,
			0xb734bb6cu, 0x156637a8u, 0xd01d049bu, 0xea4abd2eu,
			0x181f49cbu, 0xfc40e618u, 0xa145c1b1u, 0x2cb99349u,
			0x7d20d505u, 0x46ef9f6bu, 0x481a620eu, 0xdd383440u,
			0xa8e7ca25u, 0xbc014093u, 0xad98ec89u, 0xc33614bbu,
			0xebd4c01au, 0xb52b59b5u, 0xa2f8ae48u, 0xbc855558u,
			0x25463f1bu, 0xb8e35be3u, 0xe3b73059u, 0xe438aa6fu,
			0x191db2b1u, 0x7663d660u, 0x6fcfb1fau, 0x6433cb0eu,
			0xd8b4d53bu, 0x632383dfu, 0xaf617b4fu, 0x8f92f8bcu,
			0xf19378c0u, 0xc385140cu, 0x761a9615u, 0xc2495bc2u,
			0x4883ecd0u, 0x0574f6f3u, 0xfc707268u, 0x502bcca1u,
			0x62cfb8c0u, 0x2b855ddeu, 0xdee9ad61u, 0xb37cf1e8u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0xd2u, 0xa9u, 0xb6u, 0x29u, 0xacu, 0x92u, 0xdeu, 0x0du,
			0x49u, 0x5bu, 0x72u, 0x6cu, 0x69u, 0xc3u, 0x74u, 0x63u,
			0xe2u, 0x22u, 0x2eu, 0x13u, 0x4fu, 0x74u, 0xa1u, 0x1fu,
			0x8du, 0xb9u, 0x62u, 0x09u, 0x55u, 0x3au, 0xf7u, 0xf3u,
			0x10u, 0x5du, 0x2eu, 0x87u, 0x88u, 0x69u, 0x86u, 0x77u,
			0x22u, 0xe4u, 0x78u, 0xa3u, 0x7au, 0xa7u, 0x60u, 0x6fu,
		},
		{
			0xccd033b7u, 0xc0cc3191u, 0x6927f9b4u, 0xfadd2e84u,
			0xf2ca8430u, 0x11582dc5u, 0x68c5df13u, 0xf2d4a751u,
			0x85e14358u, 0xe31fef1au, 0xb7a8d6f8u, 0x7f8514bdu,
			0x8f66ada9u, 0x30941490u, 0x8f088ab8u, 0xfda70267u,
			0xbed40886u, 0x88f67f5au, 0x885be098u, 0x850f91ccu,
			0x3c6da87du, 0x9cc60c18u, 0xfa682796u, 0x0225e1d2u,
			0x855cba2bu, 0x791048b7u, 0xd5074a52u, 0xf675a793u,
			0x258323d1u, 0x4c862ee9u, 0xc3c72decu, 0x55f6d10au,
			0xc22b3b48u, 0x13ff0e08u, 0xb41f9923u, 0x4fef69dcu,
			0xa988c012u, 0xd55ea6feu, 0x722aae77u, 0x11dce43du,
			0xcc8637b5u, 0xcbaba99cu, 0x0d9dc869u, 0x55b02f9bu,
			0x3692adbeu, 0xb5bdc118u, 0xd69b742bu, 0x6c467ee7u,
			0x60f4c3a1u, 0x45eca461u, 0x60f27ce0u, 0xdfe31947u,
			0x565aeb07u, 0x9365209au, 0xb07f4c3du, 0x6dc124e3u,
			0x16bcc60bu, 0x0e81a10fu, 0x746218c1u, 0x25b95138u,
			0x3c888888u, 0x1f9b63d3u, 0x8e79e836u, 0xb5249166u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x32u, 0x2du, 0x70u, 0x6cu, 0xb6u, 0xebu, 0xeau, 0x14u,
			0xdeu, 0xf7u, 0xbfu, 0xe4u, 0x5fu, 0x81u, 0x29u, 0x71u,
			0x34u, 0x7du, 0xc9u, 0x5cu, 0xd7u, 0x69u, 0x7cu, 0x16u,
			0xa7u, 0x1eu, 0xa4u, 0xb2u, 0xa1u, 0xe1u, 0x2cu, 0x0du,
			0x31u, 0xfau, 0x2eu, 0x60u, 0x60u, 0x31u, 0xedu, 0xf3u,
			0x96u, 0x65u, 0xb5u, 0x61u, 0x6eu, 0x9fu, 0x93u, 0x7du,
		},
		{
			0xb1c3f0adu, 0x83484470u, 0xdc5e7306u, 0x6f320b7au,
			0xaf99ad3bu, 0x39236b58u, 0x922cb858u, 0xc53ede1du,
			0xab9db667u, 0x4f67fc66u, 0xdd994e7cu, 0x44926749u,
			0x712c018fu, 0x218ee636u, 0x25daefcfu, 0xec184d7du,
			0xb90b04a7u, 0xb95a2c97u, 0xe7ce0bb3u, 0xad9a3c7cu,
			0xb734bb6cu, 0x156637a8u, 0xd01d049bu, 0xea4abd2eu,
			0x181f49cbu, 0xfc40e618u, 0xa145c1b1u, 0x2cb99349u,
			0x7d20d505u, 0x46ef9f6bu, 0x481a620eu, 0xdd383440u,
			0xa8e7ca25u, 0xbc014093u, 0xad98ec89u, 0xc33614bbu,
			0xebd4c01au, 0xb52b59b5u, 0xa2f8ae48u, 0xbc855558u,
			0x25463f1bu, 0xb8e35be3u, 0xe3b73059u, 0xe438aa6fu,
			0x191db2b1u, 0x7663d660u, 0x6fcfb1fau, 0x6433cb0eu,
			0xd8b4d53bu, 0x632383dfu, 0xaf617b4fu, 0x8f92f8bcu,
			0xf19378c0u, 0xc385140cu, 0x761a9615u, 0xc2495bc2u,
			0x4883ecd0u, 0x0574f6f3u, 0xfc707268u, 0x502bcca1u,
			0x62cfb8c0u, 0x2b855ddeu, 0xdee9ad61u, 0xb37cf1e8u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x32u, 0x2du, 0x70u, 0x6cu, 0xb6u, 0xebu, 0xeau, 0x14u,
			0xdeu, 0xf7u, 0xbfu, 0xe4u, 0x5fu, 0x81u, 0x29u, 0x71u,
			0x34u, 0x7du, 0xc9u, 0x5cu, 0xd7u, 0x69u, 0x7cu, 0x16u,
			0xa7u, 0x1eu, 0xa4u, 0xb2u, 0xa1u, 0xe1u, 0x2cu, 0x0du,
			0x31u, 0xfau, 0x2eu, 0x60u, 0x60u, 0x31u, 0xedu, 0xf3u,
			0x96u, 0x65u, 0xb5u, 0x61u, 0x6eu, 0x9fu, 0x93u, 0x7du,
		},
		{
			0xb1c3f0adu, 0x83484470u, 0xdc5e7306u, 0x6f320b7au,
			0xaf99ad3bu, 0x39236b58u, 0x922cb858u, 0xc53ede1du,
			0xab9db667u, 0x4f67fc66u, 0xdd994e7cu, 0x44926749u,
			0x712c018fu, 0x218ee636u, 0x25daefcfu, 0xec184d7du,
			0xb90b04a7u, 0xb95a2c97u, 0xe7ce0bb3u, 0xad9a3c7cu,
			0xb734bb6cu, 0x156637a8u, 0xd01d049bu, 0xea4abd2eu,
			0x181f49cbu, 0xfc40e618u, 0xa145c1b1u, 0x2cb99349u,
			0x7d20d505u, 0x46ef9f6bu, 0x481a620eu, 0xdd383440u,
			0xa8e7ca25u, 0xbc014093u, 0xad98ec89u, 0xc33614bbu,
			0xebd4c01au, 0xb52b59b5u, 0xa2f8ae48u, 0xbc855558u,
			0x25463f1bu, 0xb8e35be3u, 0xe3b73059u, 0xe438aa6fu,
			0x191db2b1u, 0x7663d660u, 0x6fcfb1fau, 0x6433cb0eu,
			0xd8b4d53bu, 0x632383dfu, 0xaf617b4fu, 0x8f92f8bcu,
			0xf19378c0u, 0xc385140cu, 0x761a9615u, 0xc2495bc2u,
			0x4883ecd0u, 0x0574f6f3u, 0xfc707268u, 0x502bcca1u,
			0x62cfb8c0u, 0x2b855ddeu, 0xdee9ad61u, 0xb37cf1e8u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
};

/* 0x811e9c: KPRX AppType 0x08 dev CF key entries (keyRev 0..5) */
static KprxAppType08KeyEntry s_kprxAppType08DevKeys[6] = {
	{
		{
			0x9du, 0x45u, 0x13u, 0x66u, 0xf5u, 0x2eu, 0x5cu, 0xadu,
			0x43u, 0xbfu, 0xf8u, 0xf8u, 0xe6u, 0xc3u, 0x1bu, 0x66u,
			0x78u, 0x00u, 0x76u, 0x6eu, 0x87u, 0xa2u, 0x3fu, 0x2eu,
			0x77u, 0x68u, 0x28u, 0x35u, 0x15u, 0xa7u, 0xd7u, 0x09u,
		},
		{
			0xf6u, 0xf5u, 0x68u, 0xf9u, 0xd0u, 0x8eu, 0x08u, 0x4bu,
			0x26u, 0xdfu, 0x6au, 0x25u, 0x47u, 0xefu, 0x2bu, 0x9eu,
			0x10u, 0xf0u, 0xc7u, 0x58u, 0x84u, 0x78u, 0x22u, 0xdcu,
			0xafu, 0xe2u, 0x47u, 0x5cu, 0x8au, 0x5au, 0xbdu, 0x57u,
			0xb2u, 0xa2u, 0xb1u, 0x7du, 0x78u, 0x9bu, 0xf7u, 0xb8u,
			0xb5u, 0x1eu, 0x40u, 0x02u, 0x41u, 0x54u, 0x16u, 0xe1u,
		},
		{
			0x83b2a7cbu, 0x4bc3aa1bu, 0x05e628f2u, 0xd8fad8cbu,
			0xd9709288u, 0x50796fcfu, 0xfaf37e89u, 0xcf2944efu,
			0xe53efe19u, 0xae1f2b7cu, 0x8f3d978bu, 0x3735a123u,
			0xe42125cau, 0x82c2b7ddu, 0x74addb26u, 0xcdaae567u,
			0x2c23c220u, 0x70c895f5u, 0x4bcb7063u, 0x8cb53cbcu,
			0xb968e3e6u, 0x31f1114cu, 0xaace98f1u, 0x863abd1bu,
			0xffd07be0u, 0xbc9c0412u, 0x06775eedu, 0x863b79dcu,
			0xd2e9114au, 0xd5a14faau, 0x42899616u, 0x109dc589u,
			0xbde372adu, 0x3ce0c265u, 0x769f77fdu, 0xbeebb40fu,
			0xe714aec4u, 0xed944999u, 0xdbfff6f0u, 0x577aec58u,
			0xdea8f67eu, 0x73177fd2u, 0xff35c1fdu, 0xcee9f6a6u,
			0x60a3bd51u, 0x092144d6u, 0x5af6ed91u, 0x61bb87e7u,
			0x4b9ba869u, 0xefe5135eu, 0x88666a70u, 0xbc66af52u,
			0x81c5fb04u, 0x0a11bd22u, 0xa840136du, 0x2adb81c1u,
			0x09c8eabcu, 0xcd5bc2bdu, 0x4aa96bd0u, 0x33c6accbu,
			0xd392e825u, 0x0aa7be40u, 0xbc161f46u, 0xbf752ab0u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0xd6u, 0xc9u, 0xe2u, 0x53u, 0x33u, 0x2cu, 0xd3u, 0xdcu,
			0x85u, 0x63u, 0xbfu, 0xe3u, 0x9eu, 0x02u, 0x25u, 0x53u,
			0x65u, 0x44u, 0x38u, 0x46u, 0xfbu, 0x89u, 0xeau, 0xf8u,
			0x57u, 0x0cu, 0xf6u, 0x71u, 0xf6u, 0xe0u, 0x5du, 0x86u,
		},
		{
			0xdeu, 0x9cu, 0x27u, 0x97u, 0x85u, 0x99u, 0x06u, 0xe8u,
			0x52u, 0x67u, 0xb6u, 0x10u, 0xf6u, 0x53u, 0x72u, 0x47u,
			0xedu, 0xe5u, 0x8bu, 0x83u, 0x3cu, 0x67u, 0x63u, 0xfeu,
			0xafu, 0xdau, 0xc2u, 0xc4u, 0x4au, 0x39u, 0xf5u, 0x3eu,
			0xcbu, 0x2cu, 0xa0u, 0x4cu, 0xb0u, 0xd6u, 0x3eu, 0xf9u,
			0x88u, 0x56u, 0x23u, 0xe2u, 0x37u, 0xaau, 0xefu, 0xbau,
		},
		{
			0x958a01c1u, 0xe90c5f80u, 0x8b6be1aeu, 0x859664d3u,
			0x2190b285u, 0xc3e25b82u, 0x2c2c0826u, 0xfbe64c8fu,
			0xce9921abu, 0xb8c5164au, 0xae233a69u, 0x4c546f9bu,
			0xb42f1b21u, 0x613f7db6u, 0x4e56945du, 0xd524ddc3u,
			0x54596a90u, 0x5ee14736u, 0x5be1de9fu, 0x944fc1fau,
			0x7c7eb7e5u, 0x6788e079u, 0x7ae4bd30u, 0xe8e2dcdbu,
			0xa79db6d8u, 0xb6b41b07u, 0xa1991e7cu, 0xae9923aau,
			0xb56743b4u, 0xa561381du, 0x7d9406ecu, 0xf65ba7e7u,
			0x836c6557u, 0x268e73afu, 0xb5b55928u, 0x0f5893fdu,
			0x63b64fbeu, 0xfd20df9cu, 0xfc5ecbd6u, 0x2e2c327eu,
			0x3690c7a2u, 0xf0ea6500u, 0xac93c718u, 0x3e999567u,
			0x33ea4d0cu, 0x52513457u, 0x1f2ac691u, 0x7407cacau,
			0xff5a6656u, 0x0c75c8e3u, 0xdcd0fa63u, 0x446d5a8au,
			0xac49c4b3u, 0x8ddffc3cu, 0xcb99ee87u, 0x981202ffu,
			0x5201cdacu, 0xca07596du, 0x7722df66u, 0x778ef19cu,
			0xbcd3c9e6u, 0x1afb0efau, 0x2d165948u, 0xf1b6f194u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x9du, 0x45u, 0x13u, 0x66u, 0xf5u, 0x2eu, 0x5cu, 0xadu,
			0x43u, 0xbfu, 0xf8u, 0xf8u, 0xe6u, 0xc3u, 0x1bu, 0x66u,
			0x78u, 0x00u, 0x76u, 0x6eu, 0x87u, 0xa2u, 0x3fu, 0x2eu,
			0x77u, 0x68u, 0x28u, 0x35u, 0x15u, 0xa7u, 0xd7u, 0x09u,
		},
		{
			0xf6u, 0xf5u, 0x68u, 0xf9u, 0xd0u, 0x8eu, 0x08u, 0x4bu,
			0x26u, 0xdfu, 0x6au, 0x25u, 0x47u, 0xefu, 0x2bu, 0x9eu,
			0x10u, 0xf0u, 0xc7u, 0x58u, 0x84u, 0x78u, 0x22u, 0xdcu,
			0xafu, 0xe2u, 0x47u, 0x5cu, 0x8au, 0x5au, 0xbdu, 0x57u,
			0xb2u, 0xa2u, 0xb1u, 0x7du, 0x78u, 0x9bu, 0xf7u, 0xb8u,
			0xb5u, 0x1eu, 0x40u, 0x02u, 0x41u, 0x54u, 0x16u, 0xe1u,
		},
		{
			0x83b2a7cbu, 0x4bc3aa1bu, 0x05e628f2u, 0xd8fad8cbu,
			0xd9709288u, 0x50796fcfu, 0xfaf37e89u, 0xcf2944efu,
			0xe53efe19u, 0xae1f2b7cu, 0x8f3d978bu, 0x3735a123u,
			0xe42125cau, 0x82c2b7ddu, 0x74addb26u, 0xcdaae567u,
			0x2c23c220u, 0x70c895f5u, 0x4bcb7063u, 0x8cb53cbcu,
			0xb968e3e6u, 0x31f1114cu, 0xaace98f1u, 0x863abd1bu,
			0xffd07be0u, 0xbc9c0412u, 0x06775eedu, 0x863b79dcu,
			0xd2e9114au, 0xd5a14faau, 0x42899616u, 0x109dc589u,
			0xbde372adu, 0x3ce0c265u, 0x769f77fdu, 0xbeebb40fu,
			0xe714aec4u, 0xed944999u, 0xdbfff6f0u, 0x577aec58u,
			0xdea8f67eu, 0x73177fd2u, 0xff35c1fdu, 0xcee9f6a6u,
			0x60a3bd51u, 0x092144d6u, 0x5af6ed91u, 0x61bb87e7u,
			0x4b9ba869u, 0xefe5135eu, 0x88666a70u, 0xbc66af52u,
			0x81c5fb04u, 0x0a11bd22u, 0xa840136du, 0x2adb81c1u,
			0x09c8eabcu, 0xcd5bc2bdu, 0x4aa96bd0u, 0x33c6accbu,
			0xd392e825u, 0x0aa7be40u, 0xbc161f46u, 0xbf752ab0u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x94u, 0x37u, 0x90u, 0xdcu, 0xd6u, 0xafu, 0xe6u, 0xfeu,
			0x4du, 0x06u, 0x04u, 0xd6u, 0x12u, 0x07u, 0xddu, 0x13u,
			0xbau, 0x5au, 0xfcu, 0xb0u, 0x3du, 0x69u, 0xc7u, 0x35u,
			0x32u, 0x51u, 0xd9u, 0x8bu, 0x22u, 0x92u, 0x71u, 0x37u,
		},
		{
			0xeau, 0x4cu, 0x2bu, 0x1bu, 0x33u, 0x3au, 0x98u, 0x38u,
			0x56u, 0xe2u, 0x9au, 0x0eu, 0x41u, 0x83u, 0x4au, 0x15u,
			0xfbu, 0xb2u, 0x67u, 0x3fu, 0xe5u, 0x79u, 0xfau, 0x51u,
			0x5du, 0x09u, 0x22u, 0x85u, 0xaeu, 0x4du, 0x04u, 0x3bu,
			0x02u, 0x30u, 0x64u, 0x71u, 0xf1u, 0x4du, 0x72u, 0xb9u,
			0x82u, 0xd6u, 0xd6u, 0x8cu, 0xd9u, 0xd4u, 0x98u, 0xa6u,
		},
		{
			0xce9960bdu, 0x53d99809u, 0x3018d216u, 0xe4ddaa64u,
			0xcaefdafcu, 0xaaea8cccu, 0xd99e6c12u, 0x38a75210u,
			0x5bd668c7u, 0xbdfc19ccu, 0xbd21e8dfu, 0x94c47a28u,
			0x777beee8u, 0x4d0c63c3u, 0x6056fa14u, 0x1cb8faf5u,
			0xae67182eu, 0x67bcd17fu, 0x2c433542u, 0x959be0d1u,
			0x8df62fe3u, 0x79d91a5du, 0x67980afdu, 0x8294be13u,
			0xc3ac0c11u, 0x8ef70a81u, 0xe655d7dbu, 0xeca2fda3u,
			0x6fa6a7a6u, 0x5a75403fu, 0x0c06b483u, 0x02f47426u,
			0x04fd1883u, 0x8904c9abu, 0xf2a4eb3au, 0x609d1117u,
			0x28581a87u, 0x441b4575u, 0xe6544047u, 0x7f99fb74u,
			0xf5d06a53u, 0xb7c026bdu, 0x338042afu, 0xf46a92ddu,
			0x27117b64u, 0x481ad067u, 0xd0524d40u, 0xdc9dc105u,
			0x910b118cu, 0xf4261442u, 0xb34de159u, 0xd670f26eu,
			0xfc3041d2u, 0x612f0fa0u, 0x3a3387dcu, 0xd183e56bu,
			0xe8f1414cu, 0x3f40bc79u, 0xde7fc7dfu, 0x6301cde5u,
			0x41a916cdu, 0xdb9369edu, 0x9c2d9118u, 0xdf742e00u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x9du, 0x45u, 0x13u, 0x66u, 0xf5u, 0x2eu, 0x5cu, 0xadu,
			0x43u, 0xbfu, 0xf8u, 0xf8u, 0xe6u, 0xc3u, 0x1bu, 0x66u,
			0x78u, 0x00u, 0x76u, 0x6eu, 0x87u, 0xa2u, 0x3fu, 0x2eu,
			0x77u, 0x68u, 0x28u, 0x35u, 0x15u, 0xa7u, 0xd7u, 0x09u,
		},
		{
			0xf6u, 0xf5u, 0x68u, 0xf9u, 0xd0u, 0x8eu, 0x08u, 0x4bu,
			0x26u, 0xdfu, 0x6au, 0x25u, 0x47u, 0xefu, 0x2bu, 0x9eu,
			0x10u, 0xf0u, 0xc7u, 0x58u, 0x84u, 0x78u, 0x22u, 0xdcu,
			0xafu, 0xe2u, 0x47u, 0x5cu, 0x8au, 0x5au, 0xbdu, 0x57u,
			0xb2u, 0xa2u, 0xb1u, 0x7du, 0x78u, 0x9bu, 0xf7u, 0xb8u,
			0xb5u, 0x1eu, 0x40u, 0x02u, 0x41u, 0x54u, 0x16u, 0xe1u,
		},
		{
			0x83b2a7cbu, 0x4bc3aa1bu, 0x05e628f2u, 0xd8fad8cbu,
			0xd9709288u, 0x50796fcfu, 0xfaf37e89u, 0xcf2944efu,
			0xe53efe19u, 0xae1f2b7cu, 0x8f3d978bu, 0x3735a123u,
			0xe42125cau, 0x82c2b7ddu, 0x74addb26u, 0xcdaae567u,
			0x2c23c220u, 0x70c895f5u, 0x4bcb7063u, 0x8cb53cbcu,
			0xb968e3e6u, 0x31f1114cu, 0xaace98f1u, 0x863abd1bu,
			0xffd07be0u, 0xbc9c0412u, 0x06775eedu, 0x863b79dcu,
			0xd2e9114au, 0xd5a14faau, 0x42899616u, 0x109dc589u,
			0xbde372adu, 0x3ce0c265u, 0x769f77fdu, 0xbeebb40fu,
			0xe714aec4u, 0xed944999u, 0xdbfff6f0u, 0x577aec58u,
			0xdea8f67eu, 0x73177fd2u, 0xff35c1fdu, 0xcee9f6a6u,
			0x60a3bd51u, 0x092144d6u, 0x5af6ed91u, 0x61bb87e7u,
			0x4b9ba869u, 0xefe5135eu, 0x88666a70u, 0xbc66af52u,
			0x81c5fb04u, 0x0a11bd22u, 0xa840136du, 0x2adb81c1u,
			0x09c8eabcu, 0xcd5bc2bdu, 0x4aa96bd0u, 0x33c6accbu,
			0xd392e825u, 0x0aa7be40u, 0xbc161f46u, 0xbf752ab0u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x1bu, 0xd1u, 0x6eu, 0x37u, 0xaau, 0x19u, 0x07u, 0x58u,
			0x4au, 0xaeu, 0x75u, 0x5cu, 0x73u, 0x50u, 0x8du, 0x25u,
			0xe8u, 0x49u, 0x52u, 0xbdu, 0x08u, 0x03u, 0x07u, 0x31u,
			0x1eu, 0xe8u, 0xe0u, 0xd2u, 0x77u, 0xd1u, 0x52u, 0x7cu,
		},
		{
			0x44u, 0x31u, 0xfeu, 0x2eu, 0x86u, 0x71u, 0x4fu, 0x5du,
			0x08u, 0xbau, 0x41u, 0x43u, 0xa5u, 0x70u, 0xf8u, 0x28u,
			0xf0u, 0x01u, 0x6eu, 0x27u, 0xffu, 0x7au, 0x6bu, 0xa5u,
			0xc9u, 0x90u, 0x0eu, 0xe8u, 0xc3u, 0xf0u, 0x8cu, 0xc0u,
			0x03u, 0x8du, 0xa5u, 0x28u, 0x73u, 0x13u, 0xb1u, 0x87u,
			0x61u, 0xb4u, 0x0fu, 0x5cu, 0xe1u, 0xabu, 0x5du, 0xe9u,
		},
		{
			0x9d1cf898u, 0x7d4441f8u, 0x11dc4727u, 0x1a3a16abu,
			0x7738749bu, 0x983c1291u, 0xc67dbc52u, 0x09a0bd4du,
			0x5cd852e2u, 0x6ec5ba00u, 0xa5eaf5f1u, 0xef2a41e0u,
			0x8980bd69u, 0x86d00649u, 0xa4baa543u, 0xf2f1543au,
			0x5a2a122bu, 0xd09f703au, 0x55cce6ceu, 0xe3bf9e26u,
			0x50a52b13u, 0xa6dbe828u, 0x809fcdc3u, 0x61385782u,
			0xfad0c2c9u, 0x6b78feb3u, 0x995c58b5u, 0x4222c8cdu,
			0x877f2337u, 0x40cad872u, 0xa8730f0bu, 0x9584c6d5u,
			0x41175619u, 0x15686ee3u, 0x11ad11ebu, 0xa73cf2c7u,
			0xb1e6c50eu, 0xed3c5b14u, 0x79cf79dfu, 0xa5e1b1bau,
			0x2fb62217u, 0x6954b120u, 0xd902c96au, 0xc60e8944u,
			0x5dc211a2u, 0x38f7ac33u, 0xfa6976ccu, 0x94e78e4cu,
			0xba784134u, 0x838e4b29u, 0x321ca16cu, 0x3af1086du,
			0x23a55b60u, 0xd56ea93eu, 0x7c556e80u, 0xd669257bu,
			0xc307c6bfu, 0x4b73ef60u, 0x3a4705f0u, 0x5515d597u,
			0xb7f47c8du, 0x853e08bfu, 0x9aa2285bu, 0xcb304c81u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
};

/* 0x8126c4: 3 * 32-byte EKC decryption keys (command 0x40001) */
static uint8_t s_ekcKeys[96] = {
	0xf8u, 0x55u, 0x7cu, 0x1eu, 0x5fu, 0x0bu, 0x5au, 0x87u,
	0x1eu, 0x88u, 0xe3u, 0x97u, 0x07u, 0xb6u, 0xeau, 0x47u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0xe5u, 0x46u, 0xfcu, 0x0au, 0xb7u, 0x0eu, 0xe7u, 0x29u,
	0x40u, 0x54u, 0xd9u, 0x82u, 0x45u, 0x2bu, 0xc7u, 0x3fu,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x4bu, 0x9du, 0x45u, 0x2bu, 0xcau, 0x18u, 0x38u, 0xf6u,
	0x46u, 0xb1u, 0xb3u, 0xddu, 0x49u, 0x23u, 0x10u, 0x7bu,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

/* 0x812724: 2 * 32-byte AppType 0x08 root keys */
static uint8_t s_appType08RootKeys[64] = {
	0xc1u, 0x03u, 0x68u, 0xbfu, 0x3du, 0x29u, 0x43u, 0xbcu,
	0x6eu, 0x5bu, 0xd0u, 0x5eu, 0x46u, 0xa9u, 0xa7u, 0xb6u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x16u, 0x41u, 0x9du, 0xd3u, 0xbfu, 0xbeu, 0x8bu, 0xdcu,
	0x59u, 0x69u, 0x29u, 0xb7u, 0x2cu, 0xe2u, 0x37u, 0xcdu,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

/* 0x812764: DMAC5 keyseed type 0x00000 key */
static uint8_t s_dmac5KeyType0[32] = {
	0xe1u, 0x22u, 0x13u, 0xb4u, 0x80u, 0x16u, 0xb0u, 0xe9u,
	0x9au, 0xb8u, 0x1fu, 0x8eu, 0xc0u, 0x2au, 0xd4u, 0xa2u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

/* 0x812784: DMAC5 keyseed type 0x10000 key */
static uint8_t s_dmac5KeyType10000[32] = {
	0x90u, 0x5eu, 0x7cu, 0xddu, 0x42u, 0x04u, 0xe2u, 0x53u,
	0xe2u, 0x04u, 0x9bu, 0x5eu, 0x56u, 0x60u, 0xcau, 0x08u,
	0x38u, 0x10u, 0xbau, 0x8cu, 0xfau, 0x0fu, 0xbau, 0xa5u,
	0x8au, 0x83u, 0x48u, 0x60u, 0xdeu, 0xacu, 0xf3u, 0x2fu,
};

/* 0x8127a4: DMAC5 keyseed type 0x10001 key */
static uint8_t s_dmac5KeyType10001[32] = {
	0xb5u, 0xd8u, 0x51u, 0x12u, 0xfeu, 0xdfu, 0xc9u, 0xfdu,
	0x25u, 0xe7u, 0xd1u, 0xdbu, 0x85u, 0x8du, 0x76u, 0xfdu,
	0x04u, 0x1au, 0xfeu, 0xdbu, 0x58u, 0x2bu, 0xebu, 0xb9u,
	0xa5u, 0xe1u, 0x67u, 0xf3u, 0xfdu, 0xddu, 0x2eu, 0x8eu,
};

/* 0x8127c4: Bind data HMAC-SHA256 key (command 0x70001) */
static uint8_t s_bindDataHmacKey[32] = {
	0x90u, 0x1au, 0x84u, 0xfbu, 0x13u, 0xa7u, 0x44u, 0xa3u,
	0x78u, 0xc5u, 0x01u, 0x8au, 0x60u, 0xf5u, 0x8cu, 0x22u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

/* 0x8127e4: DMAC5 keyseed type 0x20000 AES-128-CBC key */
static uint8_t s_dmac5KeyType20000Key[16] = {
	0xa9u, 0xf8u, 0x60u, 0x5bu, 0x3du, 0x15u, 0x80u, 0x0fu,
	0xb6u, 0xf7u, 0x8eu, 0xe0u, 0x4fu, 0x5du, 0xeau, 0x52u,
};

/* 0x8127f4: DMAC5 keyseed type 0x20000 AES-128-CBC IV */
static uint8_t s_dmac5KeyType20000Iv[16] = {
	0xb3u, 0xccu, 0xb2u, 0xceu, 0x0fu, 0xb0u, 0x90u, 0xd7u,
	0x8au, 0xc8u, 0x77u, 0x63u, 0x7bu, 0x32u, 0x47u, 0x3du,
};

/* 0x812804: DMAC5 keyseed type 0x20001 AES-128-CBC key */
static uint8_t s_dmac5KeyType20001Key[32] = {
	0xeau, 0x0bu, 0x8bu, 0x96u, 0x9eu, 0xebu, 0x3du, 0x88u,
	0xa2u, 0x9au, 0xecu, 0x1au, 0xbeu, 0x9eu, 0x96u, 0x1cu,
	0xa2u, 0xfcu, 0x06u, 0x78u, 0x9cu, 0xbcu, 0xfdu, 0x8eu,
	0x8bu, 0x0fu, 0x2cu, 0x09u, 0x61u, 0xcbu, 0x0fu, 0x98u,
};

/* 0x812824: DMAC5 keyseed type 0x20001 AES-128-CBC IV */
static uint8_t s_dmac5KeyType20001Iv[16] = {
	0xfau, 0x9du, 0x31u, 0x5fu, 0xf1u, 0xd9u, 0x6au, 0x5au,
	0xfbu, 0xc5u, 0xe6u, 0x06u, 0x6bu, 0x01u, 0x74u, 0xbdu,
};

/* 0x812834: KPRX AppType 0x08 retail CF key entries (keyRev 0..5) */
static KprxAppType08KeyEntry s_kprxAppType08RetailKeys[6] = {
	{
		{
			0x9du, 0x45u, 0x13u, 0x66u, 0xf5u, 0x2eu, 0x5cu, 0xadu,
			0x43u, 0xbfu, 0xf8u, 0xf8u, 0xe6u, 0xc3u, 0x1bu, 0x66u,
			0x78u, 0x00u, 0x76u, 0x6eu, 0x87u, 0xa2u, 0x3fu, 0x2eu,
			0x77u, 0x68u, 0x28u, 0x35u, 0x15u, 0xa7u, 0xd7u, 0x09u,
		},
		{
			0xf6u, 0xf5u, 0x68u, 0xf9u, 0xd0u, 0x8eu, 0x08u, 0x4bu,
			0x26u, 0xdfu, 0x6au, 0x25u, 0x47u, 0xefu, 0x2bu, 0x9eu,
			0x10u, 0xf0u, 0xc7u, 0x58u, 0x84u, 0x78u, 0x22u, 0xdcu,
			0xafu, 0xe2u, 0x47u, 0x5cu, 0x8au, 0x5au, 0xbdu, 0x57u,
			0xb2u, 0xa2u, 0xb1u, 0x7du, 0x78u, 0x9bu, 0xf7u, 0xb8u,
			0xb5u, 0x1eu, 0x40u, 0x02u, 0x41u, 0x54u, 0x16u, 0xe1u,
		},
		{
			0x83b2a7cbu, 0x4bc3aa1bu, 0x05e628f2u, 0xd8fad8cbu,
			0xd9709288u, 0x50796fcfu, 0xfaf37e89u, 0xcf2944efu,
			0xe53efe19u, 0xae1f2b7cu, 0x8f3d978bu, 0x3735a123u,
			0xe42125cau, 0x82c2b7ddu, 0x74addb26u, 0xcdaae567u,
			0x2c23c220u, 0x70c895f5u, 0x4bcb7063u, 0x8cb53cbcu,
			0xb968e3e6u, 0x31f1114cu, 0xaace98f1u, 0x863abd1bu,
			0xffd07be0u, 0xbc9c0412u, 0x06775eedu, 0x863b79dcu,
			0xd2e9114au, 0xd5a14faau, 0x42899616u, 0x109dc589u,
			0xbde372adu, 0x3ce0c265u, 0x769f77fdu, 0xbeebb40fu,
			0xe714aec4u, 0xed944999u, 0xdbfff6f0u, 0x577aec58u,
			0xdea8f67eu, 0x73177fd2u, 0xff35c1fdu, 0xcee9f6a6u,
			0x60a3bd51u, 0x092144d6u, 0x5af6ed91u, 0x61bb87e7u,
			0x4b9ba869u, 0xefe5135eu, 0x88666a70u, 0xbc66af52u,
			0x81c5fb04u, 0x0a11bd22u, 0xa840136du, 0x2adb81c1u,
			0x09c8eabcu, 0xcd5bc2bdu, 0x4aa96bd0u, 0x33c6accbu,
			0xd392e825u, 0x0aa7be40u, 0xbc161f46u, 0xbf752ab0u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0xb9u, 0x6bu, 0x35u, 0xa4u, 0x42u, 0x0eu, 0xf4u, 0x56u,
			0x5au, 0xaau, 0x19u, 0x99u, 0xa3u, 0x75u, 0x75u, 0xddu,
			0xe5u, 0xe2u, 0x9au, 0x5au, 0x2eu, 0xb5u, 0xb0u, 0xa2u,
			0x4eu, 0xe9u, 0xc1u, 0x1fu, 0x39u, 0x9bu, 0xfcu, 0xa7u,
		},
		{
			0x41u, 0x81u, 0xb2u, 0xdfu, 0x5fu, 0x5du, 0x94u, 0xd3u,
			0xc8u, 0x0bu, 0x7du, 0x86u, 0xeau, 0xcfu, 0x19u, 0x28u,
			0x53u, 0x3au, 0x49u, 0xbau, 0x58u, 0xedu, 0xe2u, 0xb4u,
			0x3cu, 0xdeu, 0xe7u, 0xe5u, 0x72u, 0x56u, 0x8bu, 0xd4u,
			0xb1u, 0x67u, 0x8cu, 0x05u, 0x43u, 0xb6u, 0xc1u, 0x99u,
			0x7bu, 0x63u, 0xa6u, 0xf4u, 0xf3u, 0xc8u, 0xfdu, 0x33u,
		},
		{
			0x5af8c6c2u, 0xccad3c21u, 0xddb6711fu, 0xd17efac2u,
			0x1ea8aeccu, 0x8d46d1a4u, 0xf30e0b16u, 0x4cc63eadu,
			0x45099bb7u, 0xb303fd07u, 0x7044766du, 0x27c95946u,
			0xbe0e51ecu, 0x62727af4u, 0x698a6ee4u, 0x7469161fu,
			0xe134ce11u, 0x42a1e5ddu, 0xd03299d4u, 0x36c9cedau,
			0x2dbcd32au, 0x42508011u, 0xffd46128u, 0xc8dbfa49u,
			0x335d3d85u, 0xf94ad826u, 0xc20308afu, 0x4d84b1b3u,
			0x615d080au, 0xacd144c4u, 0x3c8e9444u, 0x3f097d07u,
			0xacb48a9eu, 0x9491a07bu, 0xf2cadf2du, 0xb2a66323u,
			0x07cfb602u, 0x8c34a61bu, 0xdae0b491u, 0x6403011cu,
			0xb5def526u, 0x7a65533cu, 0x6cfe3a61u, 0xc282f507u,
			0xb68f806eu, 0x2d0e2c89u, 0x58cff398u, 0xcccfb038u,
			0xcca34bafu, 0x043d0561u, 0xb2257c6du, 0x62bdeb7eu,
			0x836bfd75u, 0xab4e93f0u, 0xb238e834u, 0x7aee9d49u,
			0xa67344abu, 0xd6dab530u, 0x57d8a8fdu, 0xa41801a1u,
			0x79334335u, 0x0a3b432eu, 0xd371d83cu, 0xe7fd1f64u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x9du, 0x45u, 0x13u, 0x66u, 0xf5u, 0x2eu, 0x5cu, 0xadu,
			0x43u, 0xbfu, 0xf8u, 0xf8u, 0xe6u, 0xc3u, 0x1bu, 0x66u,
			0x78u, 0x00u, 0x76u, 0x6eu, 0x87u, 0xa2u, 0x3fu, 0x2eu,
			0x77u, 0x68u, 0x28u, 0x35u, 0x15u, 0xa7u, 0xd7u, 0x09u,
		},
		{
			0xf6u, 0xf5u, 0x68u, 0xf9u, 0xd0u, 0x8eu, 0x08u, 0x4bu,
			0x26u, 0xdfu, 0x6au, 0x25u, 0x47u, 0xefu, 0x2bu, 0x9eu,
			0x10u, 0xf0u, 0xc7u, 0x58u, 0x84u, 0x78u, 0x22u, 0xdcu,
			0xafu, 0xe2u, 0x47u, 0x5cu, 0x8au, 0x5au, 0xbdu, 0x57u,
			0xb2u, 0xa2u, 0xb1u, 0x7du, 0x78u, 0x9bu, 0xf7u, 0xb8u,
			0xb5u, 0x1eu, 0x40u, 0x02u, 0x41u, 0x54u, 0x16u, 0xe1u,
		},
		{
			0x83b2a7cbu, 0x4bc3aa1bu, 0x05e628f2u, 0xd8fad8cbu,
			0xd9709288u, 0x50796fcfu, 0xfaf37e89u, 0xcf2944efu,
			0xe53efe19u, 0xae1f2b7cu, 0x8f3d978bu, 0x3735a123u,
			0xe42125cau, 0x82c2b7ddu, 0x74addb26u, 0xcdaae567u,
			0x2c23c220u, 0x70c895f5u, 0x4bcb7063u, 0x8cb53cbcu,
			0xb968e3e6u, 0x31f1114cu, 0xaace98f1u, 0x863abd1bu,
			0xffd07be0u, 0xbc9c0412u, 0x06775eedu, 0x863b79dcu,
			0xd2e9114au, 0xd5a14faau, 0x42899616u, 0x109dc589u,
			0xbde372adu, 0x3ce0c265u, 0x769f77fdu, 0xbeebb40fu,
			0xe714aec4u, 0xed944999u, 0xdbfff6f0u, 0x577aec58u,
			0xdea8f67eu, 0x73177fd2u, 0xff35c1fdu, 0xcee9f6a6u,
			0x60a3bd51u, 0x092144d6u, 0x5af6ed91u, 0x61bb87e7u,
			0x4b9ba869u, 0xefe5135eu, 0x88666a70u, 0xbc66af52u,
			0x81c5fb04u, 0x0a11bd22u, 0xa840136du, 0x2adb81c1u,
			0x09c8eabcu, 0xcd5bc2bdu, 0x4aa96bd0u, 0x33c6accbu,
			0xd392e825u, 0x0aa7be40u, 0xbc161f46u, 0xbf752ab0u,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x32u, 0x84u, 0xe9u, 0x03u, 0x37u, 0x6bu, 0x51u, 0xd6u,
			0x58u, 0x43u, 0x70u, 0x90u, 0x2bu, 0x2cu, 0x31u, 0xdau,
			0xb3u, 0xe2u, 0xebu, 0x33u, 0xecu, 0x4cu, 0xfau, 0x71u,
			0x68u, 0x4du, 0x91u, 0xc4u, 0x8au, 0x98u, 0x7fu, 0xc2u,
		},
		{
			0x27u, 0x0cu, 0xbau, 0x37u, 0x00u, 0x61u, 0xb8u, 0x70u,
			0x77u, 0x67u, 0x2au, 0xdbu, 0x51u, 0x42u, 0xd1u, 0x88u,
			0x44u, 0xaau, 0xedu, 0x35u, 0x2au, 0x9cu, 0xceu, 0xe6u,
			0x36u, 0x02u, 0xb0u, 0xd7u, 0x40u, 0x59u, 0x43u, 0x34u,
			0x1cu, 0xf2u, 0x45u, 0x4fu, 0xbfu, 0x47u, 0xd7u, 0x62u,
			0x21u, 0xb9u, 0x1au, 0xfcu, 0x3bu, 0x60u, 0x8cu, 0x28u,
		},
		{
			0xae46e4d7u, 0xa5554caau, 0xfd2b8673u, 0x07aa8dbfu,
			0x89d05d37u, 0xb4ba3f15u, 0x1fa53562u, 0x4bcf8773u,
			0x9210ed31u, 0xefb1deaau, 0x915ed949u, 0x673c22c8u,
			0x8821ceb8u, 0x1636a3a9u, 0xff96d118u, 0x60dc299cu,
			0xfdb1c9c7u, 0x926e0607u, 0x9e48edc4u, 0xb9f7c83au,
			0x2c5df378u, 0xaf7d2541u, 0x49871599u, 0x011f6febu,
			0xe370ad81u, 0x9df63d07u, 0x7abd875fu, 0xfeaf71fbu,
			0x7377eabau, 0xa42e40a3u, 0xc178868fu, 0x3ff7ec2eu,
			0x3afd31a2u, 0xde03e57au, 0x54c033b8u, 0x1a5c5c48u,
			0x6d91dd28u, 0x5cb974c7u, 0x082563c0u, 0xd849410eu,
			0x5aef7780u, 0xb77c4fd4u, 0xc7245259u, 0x4dfe40c0u,
			0xebdbd638u, 0x3e48d25au, 0xf111bed3u, 0xbddc7c72u,
			0xe831c393u, 0x13b6f653u, 0xc2f1f434u, 0xe5d88daau,
			0x24ba9dfdu, 0x54acae4bu, 0x11e1b874u, 0x0d9c3487u,
			0x74be0a98u, 0x5c9b2896u, 0x39706a5eu, 0x22503885u,
			0xb74c1c95u, 0xbfdbb3f8u, 0x2f1f252cu, 0x97881d4cu,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0x8cu, 0x80u, 0x39u, 0x2eu, 0x96u, 0x63u, 0x99u, 0x31u,
			0xc1u, 0xd0u, 0xb3u, 0x5du, 0x75u, 0x88u, 0x2bu, 0x2fu,
			0x60u, 0x35u, 0x44u, 0x36u, 0x33u, 0x24u, 0x97u, 0xf4u,
			0x25u, 0xb4u, 0x51u, 0x0fu, 0x51u, 0x90u, 0x5bu, 0x5bu,
		},
		{
			0x4du, 0x57u, 0xdfu, 0x95u, 0x49u, 0x35u, 0x0eu, 0x8fu,
			0x82u, 0x3au, 0x1au, 0xdbu, 0xc3u, 0xb0u, 0x79u, 0x05u,
			0x96u, 0xb1u, 0xecu, 0x7au, 0xb6u, 0xc6u, 0x79u, 0xb7u,
			0x68u, 0x33u, 0x7du, 0xd0u, 0x1au, 0x65u, 0x62u, 0x29u,
			0xc5u, 0xb9u, 0x79u, 0x1bu, 0x5cu, 0x6eu, 0xdbu, 0x29u,
			0x31u, 0x29u, 0x40u, 0x47u, 0xdcu, 0x4cu, 0x6du, 0xe7u,
		},
		{
			0xda0398dbu, 0x4d423518u, 0x1cf47230u, 0xf352c230u,
			0xaec45789u, 0xa2807ec8u, 0x1f559995u, 0x30339060u,
			0xdb25e8beu, 0xeb07aed4u, 0xa3613c05u, 0x5205d654u,
			0x844cc17fu, 0x0dd765f6u, 0x5fc34d08u, 0x21de4b57u,
			0x804c5bc4u, 0x7dea45d2u, 0x8e22086eu, 0x76faa2b7u,
			0x821ef469u, 0x8924fdbau, 0xa5225bf7u, 0x0bf9fc23u,
			0xd6985437u, 0x5472f312u, 0x053ecb69u, 0xf443f4d4u,
			0xc447ec79u, 0x9588e37bu, 0xbc3219d2u, 0xd9c6489cu,
			0x5fea1a57u, 0x0bf13b3au, 0xcad43356u, 0x7c39fd0du,
			0xdd96380au, 0xcdf1fa1cu, 0x6aa703dfu, 0xad85d195u,
			0x071e2c63u, 0x3f1487c7u, 0xee75e1c2u, 0x3d331defu,
			0x6eea161eu, 0xf8161dbfu, 0x52c6b108u, 0x1ec799c0u,
			0x9579299au, 0x0074479eu, 0x6bfd088fu, 0x2c08832bu,
			0x24b95563u, 0x09c73ec0u, 0x04c95562u, 0xcdd1e4c8u,
			0x242dde26u, 0xcfd04adau, 0x1646860eu, 0xf8ad3a24u,
			0x7ce9e0e5u, 0x6a3f2b03u, 0x69e00f23u, 0x6f84842eu,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
	{
		{
			0xe8u, 0x3bu, 0xfbu, 0x3fu, 0x85u, 0xeeu, 0xb5u, 0x94u,
			0x52u, 0x01u, 0x10u, 0x14u, 0x74u, 0xabu, 0xaeu, 0xbau,
			0x98u, 0x15u, 0x7du, 0xcdu, 0x38u, 0x5du, 0x56u, 0xd2u,
			0x48u, 0x4cu, 0x60u, 0x9bu, 0xa1u, 0xd3u, 0x87u, 0xc9u,
		},
		{
			0x12u, 0xd6u, 0x4du, 0x01u, 0x72u, 0x49u, 0x52u, 0x26u,
			0x01u, 0x0au, 0x68u, 0x7du, 0xe2u, 0x45u, 0xa7u, 0x3du,
			0xe0u, 0x28u, 0xb3u, 0x56u, 0x1eu, 0x25u, 0xe6u, 0x9bu,
			0xabu, 0xc3u, 0x25u, 0x63u, 0x6fu, 0x3cu, 0xaeu, 0x0au,
			0xf1u, 0x49u, 0xeeu, 0xd1u, 0x75u, 0x7eu, 0x5au, 0x91u,
			0x5bu, 0x24u, 0x30u, 0x97u, 0x95u, 0xbfu, 0xc3u, 0x80u,
		},
		{
			0x2d5a1fd1u, 0x941c4d84u, 0x2f4686fbu, 0x230b3ad5u,
			0xf12b1a3du, 0xf9a30774u, 0xa8ee4b7au, 0xea133ba5u,
			0x4abfe2b6u, 0xf10a497eu, 0x145ebb1cu, 0xedebfebeu,
			0x56dd140au, 0xb9f9c1acu, 0xca088c1au, 0x812d47afu,
			0xcd0274a8u, 0x3351b74eu, 0xb43f3fb7u, 0xb6af3af0u,
			0x029dc290u, 0x84a76a83u, 0xe8eeb0bau, 0x60283372u,
			0x872ed0ddu, 0x521b1851u, 0x69b19b3au, 0x69ff3f43u,
			0x6c9bdd61u, 0x570e0aa6u, 0xffd58938u, 0xe192c130u,
			0xfe896ab3u, 0xe9dbf3f7u, 0x1500867cu, 0x212aecd6u,
			0xa8293076u, 0x95eecfa1u, 0x72decc0bu, 0x6e27fa2eu,
			0x5d098e97u, 0x4fe4f531u, 0x273809feu, 0x2d7cf6fbu,
			0xeaf44f90u, 0x46d83009u, 0x3f8170c7u, 0x131a6f1eu,
			0x62985110u, 0x544ff46cu, 0x17156804u, 0x593fe13bu,
			0xa1a90c3cu, 0xb12bd01bu, 0xd3675973u, 0x4f70ccbfu,
			0x84affbe7u, 0x1714284bu, 0xbde25018u, 0xbccc9e23u,
			0x36d8fb6eu, 0x8499bfeau, 0x868a2c4fu, 0xd352fb2cu,
		},
		0x00000100u,
		{ 0x00u, 0x01u, 0x00u, 0x01u },
		0x00000004u
	},
};

/* ============================================================================
 * .ctors (0x813060) & .bss (0x813080..0x814480)
 * ============================================================================ */

static void (* const s_authKprxCtorList[1])(void)
	__attribute__((section(".ctors"), used)) = {
	authKprxStaticInit
};

/* 0x813080..0x814080: 4 KB KPRX SELF header buffer */
static uint8_t s_kprxHeaderBuf[0x1000]
	__attribute__((section(".bss"), aligned(64)));

/* 0x814080..0x814480: 1 KB Signed param.sfo (SPSFO) header buffer */
static uint8_t s_spsfoHeaderBuf[0x400]
	__attribute__((section(".bss"), aligned(64)));

/* ============================================================================
 * KPRX Authentication Service & Command Handlers (0x80cbf0..0x80e7ee)
 * ============================================================================ */

/* 0x80cbf0 (Sony entry point name sm_main) */
int sm_main(void)
{
	SmServiceContext srvCtx;
	AuthKprxManager mgr;
	int status;
	uint32_t i;

	authKprxMgrCtor(&mgr);
	status = authKprxInitKeys(&mgr);
	if (status == 0) {
		smSrvInit(&srvCtx);
		for (i = 0u; i < 8u; i++) {
			SmMemberFnPtr mfp;

			mfp.fn = s_authKprxHookSpecs[i].fn;
			mfp.adj = s_authKprxHookSpecs[i].adj;
			status = authKprxAddHook(&srvCtx,
					s_authKprxHookSpecs[i].cmdId,
					&mgr, &mfp,
					&s_authKprxHookSpecs[i].node);
			if (status != 0)
				break;
		}
		if (status == 0) {
			int runRet = smSrvRunLoop(&srvCtx);

			if (runRet != 1)
				status = runRet;
		}
		smSrvTeardown(&srvCtx);
	}
	authKprxMgrDtor(&mgr);
	return status;
}

/* 0x80ccac (name ours; C++ static initializer in .ctors) */
void authKprxStaticInit(void)
{
	s_authKprxHookSpecs[0].node = NULL;
	s_authKprxHookSpecs[1].node = NULL;
	s_authKprxHookSpecs[2].node = NULL;
	s_authKprxHookSpecs[3].node = NULL;
	s_authKprxHookSpecs[4].node = NULL;
	s_authKprxHookSpecs[5].node = NULL;
	s_authKprxHookSpecs[6].node = NULL;
	s_authKprxHookSpecs[7].node = NULL;
}

/* 0x80ccd0 (name ours; virtual invoke method on SmServiceHookNode::vtable) */
int authKprxHookInvoke(void *closure, void *packetBuf)
{
	uintptr_t *words = (uintptr_t *)closure;
	uint8_t *obj = (uint8_t *)words[1];
	uintptr_t fnVal = words[2];
	int32_t adj = (int32_t)words[3];
	AuthKprxCmdMethod fn;

	if ((fnVal & 1u) != 0u) {
		uintptr_t vptr = *(const uintptr_t *)(obj + adj);

		fn = *(const AuthKprxCmdMethod *)(vptr + fnVal - 1u);
	} else {
		fn = (AuthKprxCmdMethod)fnVal;
	}
	return fn(obj + adj, packetBuf);
}

/* 0x80cd04 (name ours) */
int authKprxAddHook(SmServiceContext *srvCtx, uint32_t cmdId,
		AuthKprxManager *mgr, const SmMemberFnPtr *fnPtr,
		SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;

	node->cmdId = cmdId;
	node->next = NULL;
	if (&node->vtable != NULL) {
		node->vtable = &s_authKprxHookVtable[2];
		node->obj = mgr;
		node->fn = (void *)(uintptr_t)fnPtr->fn;
		node->adj = fnPtr->adj;
	}
	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80cd7e (name ours) */
void authKprxReadQaFlags(uint32_t *outWords8)
{
	outWords8[0] = REG32(0xe0062140u);
	outWords8[1] = REG32(0xe0062144u);
	outWords8[2] = REG32(0xe0062148u);
	outWords8[3] = REG32(0xe006214cu);
	outWords8[4] = REG32(0xe0062150u);
	outWords8[5] = REG32(0xe0062154u);
	outWords8[6] = REG32(0xe0062158u);
	outWords8[7] = REG32(0xe006215cu);
}

/* 0x80cdee (name ours) */
void authKprxMgrInitC2(AuthKprxManager *mgr)
{
	mgr->stateFlags = 0u;
	memset(&mgr->kprx, 0, sizeof(mgr->kprx));
	memset(&mgr->spsfo, 0, sizeof(mgr->spsfo));
	mgr->kprx.base.hdrBuf = s_kprxHeaderBuf;
	mgr->kprx.base.hdrBufSize = sizeof(s_kprxHeaderBuf);
	mgr->spsfo.base.hdrBuf = s_spsfoHeaderBuf;
	mgr->spsfo.base.hdrBufSize = sizeof(s_spsfoHeaderBuf);
}

/* 0x80ce3c (name ours) */
void authKprxMgrCtor(AuthKprxManager *mgr)
{
	mgr->stateFlags = 0u;
	memset(&mgr->kprx, 0, sizeof(mgr->kprx));
	memset(&mgr->spsfo, 0, sizeof(mgr->spsfo));
	mgr->kprx.base.hdrBuf = s_kprxHeaderBuf;
	mgr->kprx.base.hdrBufSize = sizeof(s_kprxHeaderBuf);
	mgr->spsfo.base.hdrBuf = s_spsfoHeaderBuf;
	mgr->spsfo.base.hdrBufSize = sizeof(s_spsfoHeaderBuf);
}

/* 0x80ce8a (name ours) */
void authKprxMgrDtorD2(AuthKprxManager *mgr)
{
	(void)mgr;
}

/* 0x80ce8c (name ours) */
void authKprxMgrDtor(AuthKprxManager *mgr)
{
	(void)mgr;
}

/* 0x80ce8e (name ours) */
int authKprxVerifyHeaderCore(AuthKprxManager *mgr, uint8_t *pkt)
{
	int64_t expectedFileOffset = 0x1000LL;
	uint8_t cid[16];
	uint8_t metaCopy[0x130];
	SceSelfKblParam kblParam;
	SceSmSelfAuthInfo authInfo;
	uint32_t customIvLen;
	int allowDevOrUnsigned;
	int status;

	aimgrReadConsoleIdAlias1(cid);
	memset(mgr->kprx.customIv, 0, sizeof(mgr->kprx.customIv));
	mgr->kprx.customIvSize = 0u;
	mgr->kprx.hdrScatterList = (const SceSmScatterList *)(pkt + 0x170u);

	if ((uint32_t)mgr->kprx.hdrScatterList->count > 2u) {
		status = SCE_SBL_ERROR_INVALID_VALUE;
		goto cleanup;
	}

	allowDevOrUnsigned = (aimgrIsTest(cid) != 0 ||
			      aimgrIsTool(cid) != 0 ||
			      aimgrIsDex(cid) != 0) ? 1 : 0;

	customIvLen = *(const uint32_t *)(pkt + 0x158u);
	if (customIvLen != 0u) {
		if (customIvLen != 16u) {
			status = SCE_SBL_ERROR_INVALID_VALUE;
			goto cleanup;
		}
		memcpy(mgr->kprx.customIv, pkt + 0x138u, 16u);
		mgr->kprx.customIvSize = customIvLen;
	}

	status = cfAuthenticateAndDecryptHeaderWithKeyCb(&mgr->kprx.base,
			authKprxResolveKprxKeyCb, &mgr->kprx,
			allowDevOrUnsigned, 1,
			authKprxReadKprxHeaderCb, mgr,
			0x600, &expectedFileOffset);
	if (status != 0)
		goto cleanup;

	status = selfVerifySvnAndDecryptMetadata(&mgr->kprx.base);
	if (status != 0)
		goto cleanup;

	memcpy(metaCopy, pkt + 0x40u, sizeof(metaCopy));
	memset(&authInfo, 0, sizeof(authInfo));

	status = selfLoaderWrapperGetAllSelfAuthInfo(&mgr->kprx.base,
			&kblParam, smSyscall8(0u, 0u, 0u),
			&authInfo, (uint32_t *)(metaCopy + 4u));
	if (status != 0)
		goto cleanup;

	status = smSyscall6((uint32_t)(uintptr_t)metaCopy,
			(uint32_t)(uintptr_t)&authInfo, 0u);
	if (status != 0)
		goto cleanup;

	memcpy(pkt + 0xd8u, &kblParam, sizeof(kblParam));

cleanup:
	aimgrClearConsoleId2(cid);
	return status;
}

/* 0x80cfea (name ours; mailbox command 0x10001) */
int authKprxCmdVerifyHeader(void *mgrPtr, void *packetBuf)
{
	AuthKprxManager *mgr = (AuthKprxManager *)mgrPtr;
	uint8_t *pkt = (uint8_t *)packetBuf;
	int status;

	mgr->stateFlags = 0u;
	smSyscall2(0u, 0u, 0u);
	status = authKprxVerifyHeaderCore(mgr, pkt);
	smSyscall3(0u, 0u, 0u);
	if (status == 0)
		mgr->stateFlags |= 1u;
	*(int *)(pkt + 8u) = status;
	return status;
}

/* 0x80d02e (name ours) */
int authKprxSetupSegmentLoadCore(AuthKprxManager *mgr, uint8_t *pkt)
{
	SceCfMetadataSection synthSec;
	const uint8_t *ehdr = NULL;
	const uint32_t *segInfos = NULL;
	const SceCfMetadataSection *sec = NULL;
	uint16_t phnum;
	uint32_t segIdx;
	const uint32_t *segEntry;
	uint32_t compFlag;
	int status;

	status = selfGetElfHeader(&mgr->kprx.base, (const void **)&ehdr);
	if (status != 0)
		return status;

	phnum = *(const uint16_t *)(ehdr + 44u);
	if (phnum > 6u || *(const uint32_t *)(pkt + 0x40u) >= (uint32_t)phnum)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	status = selfGetSegmentInfos(&mgr->kprx.base, (const void **)&segInfos);
	if (status != 0)
		return status;

	segIdx = *(const uint32_t *)(pkt + 0x40u);
	segEntry = segInfos + segIdx * 8u;

	mgr->kprx.srcScatterList = NULL;
	mgr->kprx.dstScatterList = NULL;
	mgr->kprx.segOffset = *(const uint64_t *)&segEntry[0];
	mgr->kprx.segSize = *(const uint64_t *)&segEntry[2];
	mgr->kprx.curOffset = *(const uint64_t *)&segEntry[0];

	synthSec.offset = mgr->kprx.segOffset;
	synthSec.size = mgr->kprx.segSize;
	synthSec.type = 2u;
	synthSec.index = segIdx;
	synthSec.hashAlgorithm = 6u;
	synthSec.hashKeyIndex = 0u;
	synthSec.encryptionAlgorithm = 1u;
	synthSec.keyIndex = 0xffffffffu;
	synthSec.ivIndex = 0xffffffffu;
	synthSec.compressionAlgorithm = segEntry[4];

	if ((int8_t)mgr->kprx.base.hdrBuf[8] >= 0) {
		status = cfStreamSegmentInitByTypeAndIndex(&mgr->kprx.streamState,
				&mgr->kprx.base, 2u, segIdx);
	} else {
		status = cfStreamSegmentInitThunk(&mgr->kprx.streamState,
				&mgr->kprx.base, segIdx, &synthSec);
	}
	if (status != 0)
		return status;

	if ((int8_t)mgr->kprx.base.hdrBuf[8] >= 0) {
		status = cfFindSectionEntryByTypeAndIndex(&mgr->kprx.base,
				2u, segIdx, &sec);
		if (status != 0)
			return status;
		compFlag = sec->compressionAlgorithm;
	} else {
		compFlag = synthSec.compressionAlgorithm;
	}
	*(uint32_t *)(pkt + 0x44u) = compFlag;
	return 0;
}

/* 0x80d138 (name ours; mailbox command 0x20001) */
int authKprxCmdSetupSegment(void *mgrPtr, void *packetBuf)
{
	AuthKprxManager *mgr = (AuthKprxManager *)mgrPtr;
	uint8_t *pkt = (uint8_t *)packetBuf;
	int status = SCE_SBL_ERROR_NOT_INITIALIZED;

	if ((mgr->stateFlags & 1u) != 0u) {
		smSyscall2(0u, 0u, 0u);
		status = authKprxSetupSegmentLoadCore(mgr, pkt);
		if (status != 0)
			smSyscall3(0u, 0u, 0u);
		else
			mgr->stateFlags |= 2u;
	}
	*(int *)(pkt + 8u) = status;
	return status;
}

/* 0x80d18a (name ours) */
int authKprxLoadSegmentBlockCore(AuthKprxManager *mgr, uint8_t *pkt)
{
	uint64_t reqOffset;
	uint64_t reqSize;
	uint32_t srcCount;
	uint32_t dstCount;
	const SceSmScatterList *srcList;
	uint32_t i;
	int status;

	mgr->kprx.srcScatterList = (const SceSmScatterList *)(pkt + 0x40u);
	mgr->kprx.dstScatterList = (const SceSmScatterList *)(pkt + 0x48u);

	srcCount = (uint32_t)mgr->kprx.srcScatterList->count;
	if (srcCount > 65u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	dstCount = (uint32_t)mgr->kprx.dstScatterList->count;
	if (dstCount > 65u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (authKprxValidateArmRange(mgr,
			(uint32_t)(uintptr_t)mgr->kprx.srcScatterList->entries,
			srcCount * sizeof(SceSmRangeEntry)) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}
	if (authKprxValidateArmRange(mgr,
			(uint32_t)(uintptr_t)mgr->kprx.dstScatterList->entries,
			dstCount * sizeof(SceSmRangeEntry)) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	srcList = mgr->kprx.srcScatterList;
	reqSize = 0ULL;
	for (i = 0u; (int32_t)i < srcList->count; i++)
		reqSize += (uint64_t)srcList->entries[i].size;

	if (reqSize > 0x40000ULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	reqOffset = mgr->kprx.curOffset;
	status = cfStreamSegmentUpdateRange(&mgr->kprx.streamState,
			&reqOffset, &reqSize,
			authKprxReadKprxSegmentCb, mgr,
			NULL, NULL);
	if (status != 0)
		return status;

	mgr->kprx.curOffset += reqSize;
	if (mgr->kprx.curOffset == mgr->kprx.segOffset + mgr->kprx.segSize) {
		status = cfStreamSegmentFinal(&mgr->kprx.streamState);
		if (status == 0)
			return 1;
	}
	return status;
}

/* 0x80d2bc (name ours; mailbox command 0x30001) */
int authKprxCmdLoadSegment(void *mgrPtr, void *packetBuf)
{
	AuthKprxManager *mgr = (AuthKprxManager *)mgrPtr;
	uint8_t *pkt = (uint8_t *)packetBuf;
	int status = SCE_SBL_ERROR_NOT_INITIALIZED;

	if ((mgr->stateFlags & 3u) == 3u) {
		status = authKprxLoadSegmentBlockCore(mgr, pkt);
		if (status != 0)
			smSyscall3(0u, 0u, 0u);
		if (status > 0)
			status = 0;
	}
	*(int *)(pkt + 8u) = status;
	return status;
}

/* 0x80d2fa (name ours) */
uint32_t authKprxCheckQaFlagBit0b04(void)
{
	uint32_t qaWords[8];

	authKprxReadQaFlags(qaWords);
	return ((const uint8_t *)qaWords)[11] & 4u;
}

/* 0x80d318 (name ours) */
uint32_t authKprxCheckQaFlagBit0f01(void)
{
	uint32_t qaWords[8];

	authKprxReadQaFlags(qaWords);
	return ((const uint8_t *)qaWords)[15] & 1u;
}

/* 0x80d336 (name ours) */
int authKprxScatterReadHeader(AuthKprxManager *mgr, SelfAuthSubContext *subCtx,
		uintptr_t dstAddr, const uint64_t *offset, uint32_t size,
		uint32_t *outRead)
{
	const SceSmScatterList *list = subCtx->hdrScatterList;
	const SceSmRangeEntry *entries = list->entries;
	int32_t count = list->count;
	uint64_t targetOff = *offset;
	uint64_t curPos = 0ULL;
	uint64_t inEntryOff = 0ULL;
	uint32_t entryAddr = 0u;
	uint32_t entrySize = 0u;
	int32_t idx;

	if (authKprxValidateArmRange(mgr, (uint32_t)(uintptr_t)entries,
			(uint32_t)count * sizeof(SceSmRangeEntry)) == 0) {
		return SCE_SBL_ERROR_INVALID_ADDR;
	}

	for (idx = 0; idx < count; idx++) {
		entryAddr = entries[idx].paddr;
		entrySize = entries[idx].size;
		if (authKprxValidateArmRange(mgr, entryAddr, entrySize) == 0)
			return SCE_SBL_ERROR_INVALID_ADDR;

		if (curPos + (uint64_t)entrySize > targetOff) {
			inEntryOff = targetOff - curPos;
			break;
		}
		curPos += (uint64_t)entrySize;
	}
	if (idx == count)
		return SCE_SBL_ERROR_INVALID_ADDR;

	{
		uint64_t remToRead = (uint64_t)size;
		uint64_t copied = 0ULL;

		for (;;) {
			uint64_t avail = (uint64_t)entrySize - inEntryOff;
			uint32_t chunk = (avail < remToRead) ?
					 (uint32_t)avail : (uint32_t)remToRead;

			memcpy((void *)(dstAddr + (uint32_t)copied),
					(const void *)(uintptr_t)(entryAddr + (uint32_t)inEntryOff),
					chunk);
			remToRead -= (uint64_t)chunk;
			copied += (uint64_t)chunk;
			if (remToRead == 0ULL)
				break;

			idx++;
			if (idx >= count)
				return SCE_SBL_ERROR_INVALID_ADDR;
			entryAddr = entries[idx].paddr;
			entrySize = entries[idx].size;
			inEntryOff = 0ULL;
		}
	}

	*outRead = size;
	return 0;
}

/* 0x80d4a0 (name ours) */
int authKprxReadSpsfoHeaderCb(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg)
{
	AuthKprxManager *mgr = (AuthKprxManager *)arg;
	uint64_t offCopy = *offset;

	return authKprxScatterReadHeader(mgr, &mgr->spsfo, dstAddr,
			&offCopy, size, outRead);
}

/* 0x80d4c8 (name ours) */
int authKprxReadKprxHeaderCb(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg)
{
	AuthKprxManager *mgr = (AuthKprxManager *)arg;
	uint64_t offCopy = *offset;

	return authKprxScatterReadHeader(mgr, &mgr->kprx, dstAddr,
			&offCopy, size, outRead);
}

/* 0x80d4ee (name ours) */
int authKprxScatterResolveSegment(AuthKprxManager *mgr, SelfAuthSubContext *subCtx,
		const uint64_t *offset, uint32_t size, uint64_t *outSrcAddr,
		uint64_t *outDstAddr, uint32_t *outChunk, uint32_t *outFlag)
{
	uint64_t reqOff = *offset;
	uint64_t relFromSegStart;
	uint64_t relFromCurBatch;
	const SceSmScatterList *srcList;
	const SceSmScatterList *dstList;
	uint64_t srcInEntryOff = 0ULL;
	uint32_t srcEntryAddr = 0u;
	uint32_t srcEntrySize = 0u;
	uint64_t dstInEntryOff = 0ULL;
	uint32_t dstEntryAddr = 0u;
	uint32_t dstEntrySize = 0u;
	uint64_t srcAvail;
	uint64_t dstAvail;
	uint32_t chunk;
	int32_t i;

	if (reqOff < subCtx->segOffset)
		return SCE_SBL_ERROR_INVALID_ADDR;
	if (subCtx->segSize < (uint64_t)size)
		return SCE_SBL_ERROR_INVALID_ADDR;

	relFromSegStart = reqOff - subCtx->segOffset;
	if (relFromSegStart > subCtx->segSize - (uint64_t)size ||
	    relFromSegStart > subCtx->segSize) {
		return SCE_SBL_ERROR_INVALID_ADDR;
	}
	if (reqOff < subCtx->curOffset)
		return SCE_SBL_ERROR_INVALID_ADDR;

	srcList = subCtx->srcScatterList;
	if (authKprxValidateArmRange(mgr, (uint32_t)(uintptr_t)srcList->entries,
			(uint32_t)srcList->count * sizeof(SceSmRangeEntry)) == 0) {
		return SCE_SBL_ERROR_INVALID_ADDR;
	}

	relFromCurBatch = reqOff - subCtx->curOffset;
	{
		uint64_t curPos = 0ULL;

		for (i = 0; i < srcList->count; i++) {
			srcEntryAddr = srcList->entries[i].paddr;
			srcEntrySize = srcList->entries[i].size;
			if (authKprxValidateArmRange(mgr, srcEntryAddr, srcEntrySize) == 0)
				return SCE_SBL_ERROR_INVALID_ADDR;
			if (curPos + (uint64_t)srcEntrySize > relFromCurBatch) {
				srcInEntryOff = relFromCurBatch - curPos;
				break;
			}
			curPos += (uint64_t)srcEntrySize;
		}
		if (i == srcList->count)
			return SCE_SBL_ERROR_INVALID_ADDR;
	}

	dstList = subCtx->dstScatterList;
	if (authKprxValidateArmRange(mgr, (uint32_t)(uintptr_t)dstList->entries,
			(uint32_t)dstList->count * sizeof(SceSmRangeEntry)) == 0) {
		return SCE_SBL_ERROR_INVALID_ADDR;
	}

	{
		uint64_t curPos = 0ULL;

		for (i = 0; i < dstList->count; i++) {
			dstEntryAddr = dstList->entries[i].paddr;
			dstEntrySize = dstList->entries[i].size;
			if (authKprxValidateArmRange(mgr, dstEntryAddr, dstEntrySize) == 0)
				return SCE_SBL_ERROR_INVALID_ADDR;
			if (curPos + (uint64_t)dstEntrySize > relFromCurBatch) {
				dstInEntryOff = relFromCurBatch - curPos;
				break;
			}
			curPos += (uint64_t)dstEntrySize;
		}
		if (i == dstList->count)
			return SCE_SBL_ERROR_INVALID_ADDR;
	}

	*outSrcAddr = (uint64_t)srcEntryAddr + srcInEntryOff;
	*outDstAddr = (uint64_t)dstEntryAddr + dstInEntryOff;

	dstAvail = (uint64_t)dstEntrySize - dstInEntryOff;
	chunk = (dstAvail < (uint64_t)size) ? (uint32_t)dstAvail : size;

	srcAvail = (uint64_t)srcEntrySize - srcInEntryOff;
	if (srcAvail < (uint64_t)size && (uint32_t)srcAvail < chunk)
		chunk = (uint32_t)srcAvail;

	*outChunk = chunk;
	*outFlag = 0u;
	return 0;
}

/* 0x80d7b2 (name ours) */
int authKprxReadSpsfoSegmentCb(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outChunk, uint32_t *outFlag, void *arg)
{
	AuthKprxManager *mgr = (AuthKprxManager *)arg;
	uint64_t offCopy = *offset;

	return authKprxScatterResolveSegment(mgr, &mgr->spsfo, &offCopy, size,
			outSrcAddr, outDstAddr, outChunk, outFlag);
}

/* 0x80d7e4 (name ours) */
int authKprxReadKprxSegmentCb(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outChunk, uint32_t *outFlag, void *arg)
{
	AuthKprxManager *mgr = (AuthKprxManager *)arg;
	uint64_t offCopy = *offset;

	return authKprxScatterResolveSegment(mgr, &mgr->kprx, &offCopy, size,
			outSrcAddr, outDstAddr, outChunk, outFlag);
}

/* 0x80d814 (name ours) */
int authKprxValidateSpsfoCommandPacket(AuthKprxManager *mgr, const uint8_t *pkt)
{
	const uint32_t *words = (const uint32_t *)pkt;
	uint32_t count;
	uint32_t entriesAddr;
	uint32_t i;

	if (words[0] != 80u || words[1] != 0x00080001u || words[3] != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	entriesAddr = words[17];
	count = words[16];
	if (entriesAddr == 0u || count == 0u || count > 9u ||
	    words[18] != 0u || words[19] != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	if (authKprxValidateArmRange(mgr, entriesAddr,
			count * sizeof(SceSmRangeEntry)) == 0) {
		return SCE_SBL_ERROR_INVALID_ADDR;
	}

	for (i = 0u; i < count; i++) {
		const SceSmRangeEntry *ent =
			&((const SceSmRangeEntry *)(uintptr_t)entriesAddr)[i];

		if (authKprxValidateArmRange(mgr, ent->paddr, ent->size) == 0)
			return SCE_SBL_ERROR_INVALID_ADDR;
	}
	return 0;
}

/* 0x80d8b2 (name ours) */
int authKprxVerifySpsfoHeader(AuthKprxManager *mgr, uint8_t *pkt)
{
	int64_t expectedFileOffset = 0x400LL;
	uint8_t cid[16];
	int allowDevOrUnsigned;
	int status;

	aimgrReadConsoleIdAlias1(cid);
	mgr->spsfo.hdrScatterList = (const SceSmScatterList *)(pkt + 0x40u);

	if ((uint32_t)mgr->spsfo.hdrScatterList->count > 9u) {
		status = SCE_SBL_ERROR_INVALID_VALUE;
		goto cleanup;
	}

	allowDevOrUnsigned = (aimgrIsTest(cid) != 0 ||
			      aimgrIsTool(cid) != 0 ||
			      aimgrIsDex(cid) != 0) ? 1 : 0;

	status = cfAuthenticateAndDecryptHeaderWithKeyCb(&mgr->spsfo.base,
			authKprxResolveSpsfoKeyCb, mgr,
			allowDevOrUnsigned, 6,
			authKprxReadSpsfoHeaderCb, mgr,
			0, &expectedFileOffset);

cleanup:
	aimgrClearConsoleId2(cid);
	return status;
}

/* 0x80d946 (name ours) */
int authKprxCheckSpsfoExtHeaderBounds(AuthKprxManager *mgr,
		const uint32_t *extHdr, uint32_t totalScatterSize)
{
	const SceSelfHeader *selfHdr = (const SceSelfHeader *)mgr->spsfo.base.hdrBuf;

	if (extHdr[0] == 1u && extHdr[1] == 0x200u) {
		uint64_t totalEnd = selfHdr->headerLen +
				    (uint64_t)extHdr[2] +
				    (uint64_t)extHdr[1];

		if (totalEnd == (uint64_t)totalScatterSize)
			return 0;
	}
	return SCE_SBL_ERROR_BAD_DATA;
}

/* 0x80d99a (name ours) */
int authKprxLoadSpsfoSegments(AuthKprxManager *mgr, uint8_t *pkt)
{
	const SceSmScatterList *scatterList = (const SceSmScatterList *)(pkt + 0x40u);
	const SceSelfHeader *selfHdr;
	const SceCfMetadataSection *sec0 = NULL;
	const SceCfMetadataSection *sec1 = NULL;
	SceCfMetadataSection synthSecs[2];
	uint32_t readBytes = 0u;
	uint8_t extHdrBuf[0x200];
	CfSegmentDecryptState streamState;
	uint32_t totalSize = 0u;
	uint32_t count;
	uint32_t i;
	int status;

	mgr->spsfo.hdrScatterList = scatterList;
	count = (uint32_t)scatterList->count;
	selfHdr = (const SceSelfHeader *)mgr->spsfo.base.hdrBuf;

	if (count > 9u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (authKprxValidateArmRange(mgr, (uint32_t)(uintptr_t)scatterList->entries,
			count * sizeof(SceSmRangeEntry)) == 0) {
		return SCE_SBL_ERROR_INVALID_ADDR;
	}

	for (i = 0u; i < (uint32_t)mgr->spsfo.hdrScatterList->count; i++)
		totalSize += scatterList->entries[i].size;

	if (totalSize - 0x200u > 0x7e00u)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if ((uint64_t)(totalSize - 0x200u) < selfHdr->headerLen)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if ((int8_t)mgr->spsfo.base.hdrBuf[8] >= 0) {
		status = cfFindSectionEntryByTypeAndIndex(&mgr->spsfo.base, 1u, 1u, &sec0);
		if (status != 0)
			return status;
		status = cfFindSectionEntryByTypeAndIndex(&mgr->spsfo.base, 2u, 3u, &sec1);
		if (status != 0)
			return status;
	} else {
		synthSecs[1].offset = selfHdr->headerLen;
		synthSecs[1].size = 0x200ULL;
		synthSecs[1].type = 1u;
		synthSecs[1].index = 1u;
		synthSecs[1].hashAlgorithm = 6u;
		synthSecs[1].hashKeyIndex = 0u;
		synthSecs[1].encryptionAlgorithm = 1u;
		synthSecs[1].keyIndex = 0xffffffffu;
		synthSecs[1].ivIndex = 0xffffffffu;
		synthSecs[1].compressionAlgorithm = 1u;

		synthSecs[0].offset = selfHdr->headerLen + 0x200ULL;
		synthSecs[0].size = (uint64_t)totalSize - (selfHdr->headerLen + 0x200ULL);
		synthSecs[0].type = 1u;
		synthSecs[0].index = 1u;
		synthSecs[0].hashAlgorithm = 6u;
		synthSecs[0].hashKeyIndex = 0u;
		synthSecs[0].encryptionAlgorithm = 1u;
		synthSecs[0].keyIndex = 0xffffffffu;
		synthSecs[0].ivIndex = 0xffffffffu;
		synthSecs[0].compressionAlgorithm = 1u;

		sec0 = &synthSecs[1];
		sec1 = &synthSecs[0];
	}

	if ((sec0->offset <= sec1->offset && sec0->offset + sec0->size > sec1->offset) ||
	    (sec1->offset <= sec0->offset && sec1->offset + sec1->size > sec0->offset)) {
		return SCE_SBL_ERROR_BAD_DATA;
	}
	if (sec0->offset + sec0->size > (uint64_t)totalSize ||
	    sec1->offset + sec1->size > (uint64_t)totalSize ||
	    sec0->size != 0x200ULL) {
		return SCE_SBL_ERROR_BAD_DATA;
	}

	{
		uint64_t sec0Off = sec0->offset;

		status = authKprxScatterReadHeader(mgr, &mgr->spsfo,
				(uintptr_t)extHdrBuf, &sec0Off, 0x200u, &readBytes);
		if (status != 0)
			return status;
	}

	if (authKprxCheckSpsfoExtHeaderBounds(mgr, (const uint32_t *)extHdrBuf, totalSize) != 0)
		return SCE_SBL_ERROR_BAD_DATA;

	{
		struct {
			uint32_t type;
			uint32_t index;
			const SceCfMetadataSection *sec;
		} specs[2];
		uint32_t sIdx;

		memset(specs, 0, sizeof(specs));
		specs[0].type = 1u;
		specs[0].index = 1u;
		specs[0].sec = sec0;
		specs[1].type = 2u;
		specs[1].index = 3u;
		specs[1].sec = sec1;

		for (sIdx = 0u; sIdx < 2u; sIdx++) {
			const SceCfMetadataSection *curSec = specs[sIdx].sec;
			uint64_t curOff;
			uint64_t curSize;

			mgr->spsfo.srcScatterList = scatterList;
			mgr->spsfo.dstScatterList = scatterList;
			mgr->spsfo.segOffset = curSec->offset;
			mgr->spsfo.segSize = curSec->size;
			mgr->spsfo.curOffset = 0ULL;

			if ((int8_t)mgr->spsfo.base.hdrBuf[8] >= 0) {
				status = cfStreamSegmentInitByTypeAndIndex(&streamState,
						&mgr->spsfo.base,
						specs[sIdx].type, specs[sIdx].index);
			} else {
				status = cfStreamSegmentInitThunk(&streamState,
						&mgr->spsfo.base,
						specs[sIdx].index, curSec);
			}
			if (status != 0)
				return status;

			curOff = specs[sIdx].sec->offset;
			curSize = specs[sIdx].sec->size;
			status = cfStreamSegmentUpdateRange(&streamState,
					&curOff, &curSize,
					authKprxReadSpsfoSegmentCb, mgr,
					NULL, NULL);
			if (status != 0)
				return status;

			status = cfStreamSegmentFinal(&streamState);
			if (status != 0)
				return status;
		}
	}

	return 0;
}

/* 0x80dd06 (name ours) */
int authKprxVerifySpsfoCore(AuthKprxManager *mgr, const void *packetBuf)
{
	uint8_t localPkt[80];
	int status;

	memcpy(localPkt, packetBuf, sizeof(localPkt));
	status = authKprxVerifySpsfoHeader(mgr, localPkt);
	if (status == 0) {
		status = authKprxLoadSpsfoSegments(mgr, localPkt);
		if (status == 0)
			status = authKprxValidateSpsfoCommandPacket(mgr, localPkt);
	}
	return status;
}

/* 0x80dd52 (name ours; mailbox command 0x80001) */
int authKprxCmdVerifySpsfo(void *mgrPtr, void *packetBuf)
{
	AuthKprxManager *mgr = (AuthKprxManager *)mgrPtr;
	uint8_t *pkt = (uint8_t *)packetBuf;
	int status;

	smSyscall2(0u, 0u, 0u);
	status = authKprxVerifySpsfoCore(mgr, pkt);
	memset(mgr->spsfo.base.hdrBuf, 0, mgr->spsfo.base.hdrBufSize);
	smSyscall3(0u, 0u, 0u);
	*(int *)(pkt + 8u) = status;
	return status;
}

/* 0x80dd90 (name ours) */
int authKprxMapRetailSpsfoKeyRev(uint16_t rawRev, uint16_t *outIdx)
{
	if (rawRev == 3u) {
		*outIdx = 1u;
		return 0;
	}
	if (rawRev > 3u) {
		if (rawRev == 5u) {
			*outIdx = 2u;
			return 0;
		}
	} else if (rawRev <= 1u) {
		*outIdx = 0u;
		return 0;
	}
	return SCE_SBL_ERROR_INVALID_VALUE;
}

/* 0x80ddc4 (name ours) */
int authKprxResolveSpsfoKey(AuthKprxManager *mgr, const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey)
{
	uint32_t extOff = 0u;
	uint32_t metaOff = 0u;
	const uint8_t *hdr;
	uint16_t rawRev;
	uint16_t keyIdx = 0xffffu;
	const KprxCfKeyEntry *keyTable;
	int status;

	if (&mgr->spsfo == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	hdr = mgr->spsfo.base.hdrBuf;
	status = cfGetExtHeaderAndMetaOffset(&mgr->spsfo.base,
			(const uint8_t **)&extOff, &metaOff);
	if (status != 0)
		return status;
	if (metaOff != 0u)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	rawRev = (uint16_t)(hdr[9] & 0x0fu);
	if (rawRev > 5u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (authKprxCheckQaFlagBit0b04() == 0u) {
		keyIdx = rawRev;
		keyTable = s_spsfoDevKeys;
	} else {
		status = authKprxMapRetailSpsfoKeyRev(rawRev, &keyIdx);
		if (status != 0)
			return status;
		if (keyIdx > 2u)
			return SCE_SBL_ERROR_INVALID_VALUE;
		keyTable = s_spsfoRetailKeys;
	}

	*outMetaKey = keyTable[keyIdx].aesKeys;
	memcpy(mgr->spsfo.rsaKey, keyTable[keyIdx].rsaModulus, 256u);
	memcpy(mgr->spsfo.rsaKey + 256u, keyTable[keyIdx].rsaExponent, 4u);
	*outRootPubKey = mgr->spsfo.rsaKey;
	*outSigType = 5u;
	*outRootKey = NULL;
	return 0;
}

/* 0x80deba (name ours) */
int authKprxResolveSpsfoKeyCb(const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey, void *arg)
{
	return authKprxResolveSpsfoKey((AuthKprxManager *)arg,
			outMetaKey, outRootPubKey, outSigType, outRootKey);
}

/* 0x80ded2 (name ours) */
int authKprxInitKeys(AuthKprxManager *mgr)
{
	(void)mgr;
	smSyscall2(0u, 0u, 0u);
	if (authKprxCheckQaFlagBit0b04() == 0u) {
		memcpy(&s_kprxAppType08RetailKeys[0],
				&s_kprxAppType08DevKeys[0],
				sizeof(KprxAppType08KeyEntry));
		memcpy(s_spsfoRetailKeys, s_spsfoDevKeys,
				sizeof(s_spsfoRetailKeys));
	}
	smSyscall3(0u, 0u, 0u);
	return 0;
}

/* 0x80df0e (name ours) */
int authKprxIsAllowedAuthId(const uint64_t *authId)
{
	uint64_t target = *authId;
	uint32_t i;

	for (i = 0u; i < 75u; i++) {
		if (s_allowedKprxAuthIds[i] == target)
			return 1;
	}
	return 0;
}

/* 0x80df38 (name ours) */
int authKprxResolveKprxKey(SelfAuthSubContext *subCtx, const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey)
{
	uint32_t extOff = 0u;
	uint32_t metaOff = 0u;
	const uint8_t *appInfo = NULL;
	const uint8_t *hdr;
	uint16_t keyRev;
	uint32_t appType;
	uint32_t isRetail;
	int status;

	if (subCtx == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	hdr = subCtx->base.hdrBuf;
	status = cfGetExtHeaderAndMetaOffset(&subCtx->base,
			(const uint8_t **)&extOff, &metaOff);
	if (status != 0)
		return status;
	if (metaOff < 0xc0u)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	status = selfGetAppInfo(&subCtx->base, &appInfo);
	if (status != 0)
		return status;

	keyRev = (uint16_t)(hdr[9] & 0x0fu);
	appType = *(const uint32_t *)(appInfo + 0x0cu);
	isRetail = authKprxCheckQaFlagBit0b04();

	status = authKprxLookupKprxKey(keyRev, appType,
			subCtx->aesKeys, subCtx->rsaKey,
			subCtx->customIv, isRetail);
	if (status != 0)
		return status;

	if (*(const uint32_t *)(appInfo + 0x0cu) == 0x0du &&
	    authKprxIsRestrictedAppType0dKeyRev(keyRev) != 0) {
		uint64_t authId = *(const uint64_t *)appInfo;

		if (authKprxIsAllowedAuthId(&authId) == 0)
			return SCE_SBL_ERROR_NOT_ALLOWED;
	}

	*outMetaKey = subCtx->aesKeys;
	*outRootPubKey = subCtx->rsaKey;
	*outSigType = 5u;
	if (*(const uint32_t *)(appInfo + 0x0cu) == 8u)
		*outRootKey = subCtx->customIv;
	else
		*outRootKey = NULL;
	return status;
}

/* 0x80e024 (name ours) */
int authKprxResolveKprxKeyCb(const uint8_t **outMetaKey,
		const uint8_t **outRootPubKey, uint32_t *outSigType,
		const uint8_t **outRootKey, void *arg)
{
	return authKprxResolveKprxKey((SelfAuthSubContext *)arg,
			outMetaKey, outRootPubKey, outSigType, outRootKey);
}

/* 0x80e03c (name ours) */
int authKprxValidateArmRange(AuthKprxManager *mgr, uint32_t addr, uint32_t size)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} dramBase;

	(void)mgr;
	dramBase.b[0] = REG8(0xe0062260u);
	dramBase.b[1] = REG8(0xe0062261u);
	dramBase.b[2] = REG8(0xe0062262u);
	dramBase.b[3] = REG8(0xe0062263u);

	if (addr >= 0x40200000u) {
		uint64_t dramEnd = (uint64_t)dramBase.w + 0x40000000ULL;

		if ((uint64_t)addr < dramEnd) {
			uint64_t reqEnd = (uint64_t)addr + (uint64_t)size;

			if (reqEnd <= dramEnd)
				return 1;
		}
	} else if (addr <= 0x1effffffu) {
		goto check_sram_low;
	}

	if (addr <= 0x1f007fffu) {
		uint32_t reqEnd = addr + size;

		if (reqEnd >= size && reqEnd <= 0x1f008000u)
			return 1;
	} else if (addr >= 0x1f840000u) {
		if (addr <= 0x1f85ffffu) {
			uint32_t reqEnd = addr + size;

			if (reqEnd >= size && reqEnd <= 0x1f860000u)
				return 1;
		}
		if ((int32_t)addr < 0 && addr <= 0x9fffffffu) {
			uint32_t reqEnd = addr + size;

			if (reqEnd >= size && reqEnd <= 0xa0000000u)
				return 1;
		}
	}

check_sram_low:
	if (addr >= 0x20000000u && addr <= 0x2fffffffu) {
		uint32_t reqEnd = addr + size;

		return (reqEnd >= size && reqEnd <= 0x30000000u);
	}
	return 0;
}

/* 0x80e162 (name ours) */
int authKprxAes128EcbDecryptWithSyscall(void *buf, uint32_t size, const void *key)
{
	uint32_t bmCtrl[2];
	int status = SCE_SBL_ERROR_INVALID_VALUE;

	smSyscall2(0u, 0u, 0u);
	if (buf != NULL && key != NULL && (size & 0x0fu) == 0u) {
		bigmacInitChannel0(bmCtrl);
		status = bigmacStartAes128EcbDecrypt(buf, buf, size, key, bmCtrl);
		if (status == 0)
			status = bigmacWaitChannelDone(bmCtrl);
	}
	smSyscall3(0u, 0u, 0u);
	return status;
}

/* 0x80e1c8 (name ours; mailbox command 0x40001) */
int authKprxCmdDecryptEkc(void *mgrPtr, void *packetBuf)
{
	uint8_t localPkt[0x150];
	uint8_t *pkt = (uint8_t *)packetBuf;
	uint32_t dataSize;
	uint32_t keyIdx;

	(void)mgrPtr;
	memcpy(localPkt, pkt, sizeof(localPkt));

	dataSize = *(const uint32_t *)(localPkt + 0x140u);
	keyIdx = *(const uint32_t *)(localPkt + 0x144u);

	if (*(const uint32_t *)(localPkt + 4u) == 0x00040001u &&
	    (dataSize & 0x0fu) == 0u && dataSize != 0u && dataSize <= 0x100u &&
	    keyIdx < 3u) {
		int status = authKprxAes128EcbDecryptWithSyscall(localPkt + 0x40u,
				dataSize, &s_ekcKeys[keyIdx * 32u]);

		if (status == 0) {
			memcpy(pkt + 0x40u, localPkt + 0x40u,
					*(const uint32_t *)(localPkt + 0x140u));
			*(int *)(pkt + 8u) = 0;
			return 0;
		}
	}

	*(int *)(pkt + 8u) = SCE_SBL_ERROR_INVALID_VALUE;
	return 0;
}

/* 0x80e26a (name ours) */
int authKprxWriteDmac5KeyringSlot(const uint32_t *keyWords, uint32_t wordCount,
		uint32_t slotId)
{
	volatile uint32_t *slotBase;
	uint32_t count;
	uint32_t i;

	smSyscall2(0u, 0u, 0u);
	if (keyWords == NULL || wordCount > 8u || slotId > 31u || slotId == 27u) {
		smSyscall3(0u, 0u, 0u);
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	slotBase = (volatile uint32_t *)(uintptr_t)(0xe04e0000u + (slotId << 5));
	count = wordCount & 0x3fffffffu;
	for (i = 0u; i < count; i++)
		slotBase[i] = keyWords[i];

	smSyscall3(0u, 0u, 0u);
	return 0;
}

/* 0x80e2d6 (name ours; mailbox command 0x70001) */
int authKprxCmdDecryptBindData(void *mgrPtr, void *packetBuf)
{
	AuthKprxManager *mgr = (AuthKprxManager *)mgrPtr;
	uint8_t *pkt = (uint8_t *)packetBuf;
	uint32_t localPkt[22];
	uint32_t bmCtrl[2];
	uint8_t hmacKeyBuf[64];
	uint8_t derivedKeyAndIv[32];
	uint32_t encAddr;
	uint32_t encSize;
	uint32_t seedAddr;
	uint32_t seedSize;
	uint32_t keySlot;

	memcpy(localPkt, pkt, sizeof(localPkt));
	encAddr = localPkt[16];
	encSize = localPkt[17];
	seedAddr = localPkt[18];
	seedSize = localPkt[19];
	keySlot = localPkt[20];

	if (localPkt[1] != 0x00070001u || keySlot != 0u || localPkt[21] != 0u ||
	    (encAddr & 0x3fu) != 0u || (encSize & 0x0fu) != 0u ||
	    encSize == 0u || encSize > 0x1000u ||
	    (seedAddr & 0x3fu) != 0u || (seedSize & 0x0fu) != 0u ||
	    seedSize == 0u || seedSize > 0x1000u) {
		*(int *)(pkt + 8u) = SCE_SBL_ERROR_INVALID_VALUE;
		return 0;
	}

	if (authKprxValidateArmRange(mgr, encAddr, encSize) == 0 ||
	    authKprxValidateArmRange(mgr, seedAddr, seedSize) == 0) {
		*(int *)(pkt + 8u) = SCE_SBL_ERROR_INVALID_ADDR;
		return 0;
	}

	smSyscall2(0u, 0u, 0u);
	if (bigmacInitChannel0(bmCtrl) == 0) {
		memset(hmacKeyBuf, 0, sizeof(hmacKeyBuf));
		memcpy(hmacKeyBuf, &s_bindDataHmacKey[localPkt[20] * 32u], 16u);
		if (bigmacHmacSha256OneShotStart(derivedKeyAndIv,
				(const void *)(uintptr_t)localPkt[18],
				localPkt[19], hmacKeyBuf, 64u, bmCtrl) == 0 &&
		    bigmacWaitChannelDone(bmCtrl) == 0 &&
		    bigmacInitChannel0(bmCtrl) == 0 &&
		    bigmacStartAes128CbcDecrypt(
				(void *)(uintptr_t)localPkt[16],
				(const void *)(uintptr_t)localPkt[16],
				localPkt[17],
				derivedKeyAndIv, derivedKeyAndIv + 16u,
				bmCtrl) == 0 &&
		    bigmacWaitChannelDone(bmCtrl) == 0) {
			*(int *)(pkt + 8u) = 0;
			goto wipe_keys;
		}
	}

	*(int *)(pkt + 8u) = SCE_SBL_ERROR_CRYPTO;

wipe_keys:
	byteMemset(derivedKeyAndIv, 0, sizeof(derivedKeyAndIv));
	byteMemset(hmacKeyBuf, 0, sizeof(hmacKeyBuf));
	smSyscall3(0u, 0u, 0u);
	return 0;
}

/* 0x80e43e (name ours) */
int authKprxAes128CbcEncryptWithSyscall(void *buf, uint32_t size,
		const void *key, const void *iv)
{
	uint8_t localIv[16];
	int status = SCE_SBL_ERROR_INVALID_VALUE;

	if (buf != NULL && ((uintptr_t)buf % 3u) == 0u &&
	    (size & 0x0fu) == 0u && key != NULL && iv != NULL) {
		smSyscall2(0u, 0u, 0u);
		bigmacMemcpy(localIv, iv, sizeof(localIv));
		if (bigmacAes128CbcEncryptSync(buf, buf, size, key, localIv) == 0) {
			smSyscall3(0u, 0u, 0u);
			status = 0;
		} else {
			smSyscall3(0u, 0u, 0u);
			status = SCE_SBL_ERROR_IO;
		}
	}
	return status;
}

/* 0x80e4cc (name ours) */
int authKprxSha256ThenAes128CbcEncryptWithSyscall(void *buf, uint32_t size,
		const void *key, const void *iv)
{
	uint8_t digest[32];
	uint8_t localIv[16];
	int status = SCE_SBL_ERROR_INVALID_VALUE;

	if (buf != NULL && ((uintptr_t)buf % 3u) == 0u &&
	    size == 32u && key != NULL && iv != NULL) {
		smSyscall2(0u, 0u, 0u);
		bigmacMemcpy(localIv, iv, sizeof(localIv));
		if (bigmacSha256(digest, buf, 32u) == 0 &&
		    bigmacAes128CbcEncryptSync(buf, digest, 32u, key, localIv) == 0) {
			smSyscall3(0u, 0u, 0u);
			status = 0;
		} else {
			smSyscall3(0u, 0u, 0u);
			status = SCE_SBL_ERROR_IO;
		}
	}
	return status;
}

/* 0x80e568 (name ours; mailbox commands 0x50001 and 0x60001) */
int authKprxCmdSetDmac5Key(void *mgrPtr, void *packetBuf)
{
	uint8_t *pkt = (uint8_t *)packetBuf;
	uint8_t localPkt[0x150];
	uint32_t workKey[8];
	uint32_t cmdId;
	uint32_t keySize;
	uint32_t keyType;

	(void)mgrPtr;
	memcpy(localPkt, pkt, sizeof(localPkt));
	keySize = *(const uint32_t *)(localPkt + 0x140u);
	cmdId = *(const uint32_t *)(localPkt + 4u);
	keyType = *(const uint32_t *)(localPkt + 0x144u);

	if ((keySize & 0x0fu) != 0u || (cmdId == 0x00050001u && keySize == 0u))
		goto err_invalid;

	if (keyType != 0u && keyType != 0x00010000u &&
	    keyType != 0x00020000u && keyType != 0x00020001u) {
		if (keyType != 0x00010001u || authKprxCheckQaFlagBit0f01() == 0u)
			goto err_invalid;
	}

	memset(workKey, 0, sizeof(workKey));
	cmdId = *(const uint32_t *)(localPkt + 4u);
	if (cmdId == 0x00050001u) {
		int status;

		keySize = *(const uint32_t *)(localPkt + 0x140u);
		if (keySize > 32u)
			goto err_invalid;
		memcpy(workKey, localPkt + 0x40u, keySize);
		keyType = *(const uint32_t *)(localPkt + 0x144u);
		if (keyType == 0u) {
			status = authKprxAes128EcbDecryptWithSyscall(workKey,
					*(const uint32_t *)(localPkt + 0x140u),
					s_dmac5KeyType0);
		} else if (keyType == 0x00010000u) {
			status = authKprxAes128EcbDecryptWithSyscall(workKey,
					*(const uint32_t *)(localPkt + 0x140u),
					s_dmac5KeyType10000);
		} else if (keyType == 0x00010001u) {
			status = authKprxAes128EcbDecryptWithSyscall(workKey,
					*(const uint32_t *)(localPkt + 0x140u),
					s_dmac5KeyType10001);
		} else if (keyType == 0x00020000u) {
			status = authKprxAes128CbcEncryptWithSyscall(workKey,
					*(const uint32_t *)(localPkt + 0x140u),
					s_dmac5KeyType20000Key,
					s_dmac5KeyType20000Iv);
		} else {
			if (keyType != 0x00020001u)
				goto check_prev_status;
			status = authKprxSha256ThenAes128CbcEncryptWithSyscall(
					workKey,
					*(const uint32_t *)(localPkt + 0x140u),
					s_dmac5KeyType20001Key,
					s_dmac5KeyType20001Iv);
		}
		*(int *)(pkt + 8u) = status;

check_prev_status:
		if (*(const int *)(pkt + 8u) != 0)
			return 0;
		keySize = *(const uint32_t *)(localPkt + 0x140u);
	} else if (cmdId == 0x00060001u) {
		byteMemset(workKey, 0, sizeof(workKey));
		keySize = 32u;
	} else {
		goto err_invalid;
	}

	*(int *)(pkt + 8u) = authKprxWriteDmac5KeyringSlot(workKey,
			keySize >> 2,
			*(const uint32_t *)(localPkt + 0x148u));
	return 0;

err_invalid:
	*(int *)(pkt + 8u) = SCE_SBL_ERROR_INVALID_VALUE;
	return 0;
}

/* 0x80e6d0 (name ours) */
int authKprxLookupKprxKey(uint16_t keyRev, uint32_t appType, void *outAes48,
		void *outRsa264, void *inOutRootBlock16, uint32_t isRetail)
{
	const KprxCfKeyEntry *entry;

	if (outAes48 == NULL || outRsa264 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (appType == 8u) {
		uint32_t rootIdx;
		int status;

		if (inOutRootBlock16 == NULL || keyRev > 5u)
			return SCE_SBL_ERROR_INVALID_VALUE;

		rootIdx = ((uint32_t)(keyRev - 2u) <= 3u) ? 1u : 0u;
		status = bigmacAes128EcbDecryptSync(inOutRootBlock16,
				inOutRootBlock16, 16u,
				&s_appType08RootKeys[rootIdx * 32u]);
		if (status != 0)
			return status;

		if (isRetail == 0u) {
			entry = (const KprxCfKeyEntry *)s_kprxAppType08DevKeys[keyRev].aesKeys;
		} else {
			entry = (const KprxCfKeyEntry *)s_kprxAppType08RetailKeys[keyRev].aesKeys;
		}
	} else if (appType == 7u) {
		if (keyRev > 1u)
			return SCE_SBL_ERROR_INVALID_VALUE;
		entry = &s_kprxAppType07Key;
	} else if (appType == 0x0eu) {
		if (keyRev > 1u)
			return SCE_SBL_ERROR_INVALID_VALUE;
		entry = &s_kprxAppType0eKey;
	} else if (appType == 0x0du) {
		if (keyRev > 5u)
			return SCE_SBL_ERROR_INVALID_VALUE;
		entry = &s_kprxAppType0dKeys[keyRev];
	} else {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memcpy(outAes48, entry->aesKeys, 48u);
	memcpy(outRsa264, entry->rsaModulus, 256u);
	memcpy((uint8_t *)outRsa264 + 256u, entry->rsaExponent, 4u);
	return 0;
}

/* 0x80e7d0 (name ours) */
int authKprxIsRestrictedAppType0dKeyRev(uint32_t keyRev)
{
	if (s_appType0dRestrictedKeyRevs[0] == keyRev ||
	    s_appType0dRestrictedKeyRevs[1] == keyRev) {
		return 1;
	}
	return (s_appType0dRestrictedKeyRevs[2] == keyRev);
}
