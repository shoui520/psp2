/* SceKrm driver state. Shared by the kernel and boot-image module paths. */
#include "krm_internal.h"

/* The compiler emits these work areas in reverse declaration order. */
#define KRM_RUNTIME(align) __attribute__((section(".bss.krmRuntime"), aligned(align)))
SceUID g_krmProcEventHandler KRM_RUNTIME(4);
KrmEccPublicKey g_krmChallengePublicKey KRM_RUNTIME(4);
KrmEccPrivateKey g_krmChallengePrivateKey KRM_RUNTIME(4);
unsigned int g_krmChallengeNonce[4] KRM_RUNTIME(4);
#if FW_DEBUG
unsigned char g_krmCryptBuffer[4096] KRM_RUNTIME(4);
KrmStateKey g_krmStateKey KRM_RUNTIME(4);
uint32_t g_krmStateIv[4] KRM_RUNTIME(4);
KrmEccPublicKey g_krmStatePublicKey KRM_RUNTIME(4);
KrmEccPrivateKey g_krmStatePrivateKey KRM_RUNTIME(4);
KrmSignedState g_krmEncryptedState KRM_RUNTIME(8);
KrmSignedState g_krmSignedState KRM_RUNTIME(8);
#else
KrmSignedState g_krmSignedState KRM_RUNTIME(8);
KrmEccPublicKey g_krmStatePublicKey KRM_RUNTIME(4);
KrmEccPrivateKey g_krmStatePrivateKey KRM_RUNTIME(4);
KrmStateKey g_krmStateKey KRM_RUNTIME(4);
uint32_t g_krmStateIv[4] KRM_RUNTIME(4);
KrmSignedState g_krmEncryptedState KRM_RUNTIME(8);
unsigned char g_krmCryptBuffer[4096] KRM_RUNTIME(8);
#endif
unsigned int g_krmWorkspacePadding KRM_RUNTIME(4);
KrmEccContext g_krmEccContext KRM_RUNTIME(4);
SceUID g_krmDriverPid KRM_RUNTIME(4);
struct ThreadmgrFastMutex g_krmChallengeMutex KRM_RUNTIME(4);
struct ThreadmgrFastMutex g_krmDriverMutex KRM_RUNTIME(4);
int g_krmChallengeInitialized KRM_RUNTIME(4);
int g_krmWrappingKeyInitialized KRM_RUNTIME(4);
int g_krmKeysInitialized KRM_RUNTIME(4);
unsigned int g_krmDriverState KRM_RUNTIME(4);
unsigned char g_krmRuntimeReserved[448] KRM_RUNTIME(8);
#undef KRM_RUNTIME

#define KRM_READ_ONLY __attribute__((section(".rodata.krmPrefix"), aligned(4)))
#if FW_DEBUG
extern const char g_krmKrmFile[12];
#endif
#if FW_DEBUG
const KrmEccPublicKey g_krmPrimaryChallengePublicKey KRM_READ_ONLY = {
	.x = { 0x34fa62de, 0x3cbdda52, 0x377d0660, 0xce523a5b,
		0xed77cc94, 0x8638723e, 0xe75fe09e, 0x53562a82 },
	.y = { 0x5c7dec71, 0xa28d7b16, 0x60f99a42, 0x474fb8f5,
		0x3392d441, 0x088d00f1, 0xcad14211, 0x8f0d7fa0 },
};
const KrmEccPublicKey g_krmAlternateChallengePublicKey KRM_READ_ONLY = {
	.x = { 0x557d6588, 0x5588e451, 0xb80a9d28, 0x352ac827,
		0x525f0bf0, 0x4b69243d, 0xf80616ea, 0x25d918be },
	.y = { 0x3bcc61b7, 0x37ea42ad, 0x95b96793, 0x61a5bee1,
		0x76262318, 0xea9e9575, 0x335d97b5, 0xa9b3596f },
};
const char g_krmKeyRingFile[16] KRM_READ_ONLY = "key-ring.c";
const SceKernelDebugInfo g_krmGenerateRandomDebug KRM_READ_ONLY = {
	.fileHash = 0x3492ea6, .lineHash = 0xd902ce59,
	.funcHash = 0x700c530b, .file = g_krmKeyRingFile,
	.line = 429, .func = g_krmGenerateKeyName,
};
const SceKernelDebugInfo g_krmGenerateEccDebug KRM_READ_ONLY = {
	.fileHash = 0x3492ea6, .lineHash = 0x6c685421,
	.funcHash = 0x700c530b, .file = g_krmKeyRingFile,
	.line = 419, .func = g_krmGenerateKeyName,
};
const char g_krmGenerateKeyName[32] KRM_READ_ONLY = "sceKrmInternalGenerateKey";
const SceKernelDebugInfo g_krmRandomDebug KRM_READ_ONLY = {
	.fileHash = 0x34252f76, .lineHash = 0x56bea0ba,
	.funcHash = 0x98d47fc7, .file = g_krmKrmFile,
	.line = 109, .func = g_krmEncryptAndSignStateName,
};
const SceKernelDebugInfo g_krmTickDebug KRM_READ_ONLY = {
	.fileHash = 0x34252f76, .lineHash = 0x29573a74,
	.funcHash = 0x98d47fc7, .file = g_krmKrmFile,
	.line = 116, .func = g_krmEncryptAndSignStateName,
};
const char g_krmEncryptAndSignStateName[24] KRM_READ_ONLY = "EncryptAndSignState";
const KrmEccPublicKey g_krmAlternateStatePublicKey KRM_READ_ONLY = {
	.x = { 0x2011a333, 0xedfa07e5, 0xcdcbb39e, 0xc0ab8977,
		0x0d3c9112, 0x98dd4df1, 0xbf0d0b76, 0x6a469c00 },
	.y = { 0x60190025, 0x127606eb, 0x7791cae5, 0xe8427be3,
		0xba012886, 0x5fa9452c, 0xe5c094a4, 0x664cbf1f },
};
const KrmEccPublicKey g_krmPrimaryStatePublicKey KRM_READ_ONLY = {
	.x = { 0x13b4101e, 0x36d3220f, 0x52198ba8, 0xeacfd652,
		0x42c97475, 0x56369e08, 0x21e2fd22, 0x4a949edc },
	.y = { 0x29c163d5, 0x399c5b37, 0xd62c57fe, 0xc4bbb634,
		0x1dd51e14, 0x3e6f5347, 0xca91f2cc, 0x60441cf1 },
};
const KrmEccPublicKey g_krmConfigPublicKey KRM_READ_ONLY = {
	.x = { 0x17f64f0c, 0xae44651f, 0xa3432ca8, 0xba4e5460,
		0xe066a0f1, 0x6f8c5895, 0xc3d4177c, 0xa6a1343f },
	.y = { 0xde62aae9, 0xe8a72f45, 0x0a7d32fc, 0x2d22dc45,
		0xa3783ff4, 0xef836b1d, 0x5eca89b7, 0x9be8a731 },
};
const char g_krmKrmFile[12] KRM_READ_ONLY = "krm.c";
const char g_krmDriverFile[12] KRM_READ_ONLY = "driver.c";
const unsigned int g_krmNamePadding KRM_READ_ONLY = 0;
const char g_krmSetPidName[20] KRM_READ_ONLY = "sceKrmDriverSetPID";
const uint32_t g_krmEccInverseCoefficientInfo KRM_READ_ONLY = 0;
const char g_krmProcEventHandlerName[28] KRM_READ_ONLY = "SceKrmProcessEventHandler";
const char g_krmChallengeMutexName[16] KRM_READ_ONLY = "SceKrmMutex2";
const char g_krmGenerateRandomError[48] KRM_READ_ONLY = "%s: Failed to get random bytes with error %08x\n";
const char g_krmGenerateEccError[56] KRM_READ_ONLY = "%s: Failed to create ECC 256 keypair with error %08x\n";
const char g_krmTickError[48] KRM_READ_ONLY = "%s: Failed to get secure tick with error %08x\n";
const char g_krmRandomError[48] KRM_READ_ONLY = "%s: Failed to get random bytes with error %08x\n";
const char g_krmSeedForKey[20] KRM_READ_ONLY = "SceKrmSeedForKey";
const char g_krmDriverMutexName[12] KRM_READ_ONLY = "SceKrmMutex";
const SceKernelDebugInfo g_krmSetPidDebug KRM_READ_ONLY = {
	.fileHash = 0x585b2fa3, .lineHash = 0x67e96bf0,
	.funcHash = 0xbdb9dfa4, .file = g_krmDriverFile,
	.line = 23, .func = g_krmSetPidName,
};
const unsigned int g_krmPrefixPadding KRM_READ_ONLY = 0;
const uint16_t g_krmPowerWindowLimits[6] KRM_READ_ONLY = { 6, 24, 80, 240, 672, 0 };
const unsigned int g_krmPrefixSdkVersion KRM_READ_ONLY = FW_SDK_VERSION;
const unsigned int g_krmPrefixReserved KRM_READ_ONLY = 0;
const char g_krmSeedForIv[16] KRM_READ_ONLY = "SceKrmSeedForIV";
#else
const KrmEccPublicKey g_krmPrimaryChallengePublicKey KRM_READ_ONLY = {
	.x = { 0x34fa62de, 0x3cbdda52, 0x377d0660, 0xce523a5b,
		0xed77cc94, 0x8638723e, 0xe75fe09e, 0x53562a82 },
	.y = { 0x5c7dec71, 0xa28d7b16, 0x60f99a42, 0x474fb8f5,
		0x3392d441, 0x088d00f1, 0xcad14211, 0x8f0d7fa0 },
};
const KrmEccPublicKey g_krmAlternateChallengePublicKey KRM_READ_ONLY = {
	.x = { 0x557d6588, 0x5588e451, 0xb80a9d28, 0x352ac827,
		0x525f0bf0, 0x4b69243d, 0xf80616ea, 0x25d918be },
	.y = { 0x3bcc61b7, 0x37ea42ad, 0x95b96793, 0x61a5bee1,
		0x76262318, 0xea9e9575, 0x335d97b5, 0xa9b3596f },
};
const KrmEccPublicKey g_krmAlternateStatePublicKey KRM_READ_ONLY = {
	.x = { 0x2011a333, 0xedfa07e5, 0xcdcbb39e, 0xc0ab8977,
		0x0d3c9112, 0x98dd4df1, 0xbf0d0b76, 0x6a469c00 },
	.y = { 0x60190025, 0x127606eb, 0x7791cae5, 0xe8427be3,
		0xba012886, 0x5fa9452c, 0xe5c094a4, 0x664cbf1f },
};
const KrmEccPublicKey g_krmPrimaryStatePublicKey KRM_READ_ONLY = {
	.x = { 0x13b4101e, 0x36d3220f, 0x52198ba8, 0xeacfd652,
		0x42c97475, 0x56369e08, 0x21e2fd22, 0x4a949edc },
	.y = { 0x29c163d5, 0x399c5b37, 0xd62c57fe, 0xc4bbb634,
		0x1dd51e14, 0x3e6f5347, 0xca91f2cc, 0x60441cf1 },
};
const KrmEccPublicKey g_krmConfigPublicKey KRM_READ_ONLY = {
	.x = { 0x17f64f0c, 0xae44651f, 0xa3432ca8, 0xba4e5460,
		0xe066a0f1, 0x6f8c5895, 0xc3d4177c, 0xa6a1343f },
	.y = { 0xde62aae9, 0xe8a72f45, 0x0a7d32fc, 0x2d22dc45,
		0xa3783ff4, 0xef836b1d, 0x5eca89b7, 0x9be8a731 },
};
const uint32_t g_krmEccInverseCoefficientInfo KRM_READ_ONLY = 0;
const char g_krmProcEventHandlerName[28] KRM_READ_ONLY = "SceKrmProcessEventHandler";
const char g_krmChallengeMutexName[16] KRM_READ_ONLY = "SceKrmMutex2";
const char g_krmSeedForKey[20] KRM_READ_ONLY = "SceKrmSeedForKey";
const char g_krmDriverMutexName[12] KRM_READ_ONLY = "SceKrmMutex";
const unsigned int g_krmPrefixPadding KRM_READ_ONLY = 0;
const uint16_t g_krmPowerWindowLimits[6] KRM_READ_ONLY = { 6, 24, 80, 240, 672, 0 };
const unsigned int g_krmPrefixSdkVersion KRM_READ_ONLY = FW_SDK_VERSION;
const unsigned int g_krmPrefixReserved KRM_READ_ONLY = 0;
const char g_krmSeedForIv[16] KRM_READ_ONLY = "SceKrmSeedForIV";
#endif
#undef KRM_READ_ONLY

#define KRM_PROVISIONED __attribute__((section(".data.krmProvisioned"), aligned(8)))
#if FW_DEBUG
unsigned int g_krmProvisionedChallengeAlternateKeyIv[4] KRM_PROVISIONED = { 0x8681dfc0, 0xcec504b7, 0xbcfeb9b2, 0x275d45ec };
unsigned int g_krmProvisionedChallengePrimaryKeyIv[4] KRM_PROVISIONED = { 0x3564d8e9, 0xb7ba6872, 0x8038ff8c, 0xb306b4fe };
unsigned int g_krmProvisionedChallengeAlternateDigestIv[4] KRM_PROVISIONED = { 0xfffc4c1d, 0x919bc283, 0xc57b0976, 0x7bfeba13 };
unsigned int g_krmProvisionedChallengePrimaryDigestIv[4] KRM_PROVISIONED = { 0xc645fcf8, 0x1ee5a7a0, 0x14f79b64, 0x0fe58da4 };
KrmSha256Digest g_krmProvisionedChallengePrimaryDigest KRM_PROVISIONED = { .words = { 0xce6a23a2, 0x9265f6b9, 0x86cee29f, 0xcc8b81ac, 
		0x0add86c6, 0x4978515d, 0xe78fdf7d, 0xd9f660a2 } };
KrmEccPrivateKey g_krmProvisionedChallengeAlternateKey KRM_PROVISIONED = { .scalar = { 0xc3ae354b, 0x07024fd9, 0x1d971881, 0x596d2871, 
		0xe90f6fa2, 0x528a1158, 0x77942206, 0x75573aed } };
KrmEccPrivateKey g_krmProvisionedChallengePrimaryKey KRM_PROVISIONED = { .scalar = { 0x2e3f6a46, 0x12a85ea7, 0x4be23822, 0x572238c7, 
		0xf2cca18b, 0xecb938fd, 0xa35fa7b1, 0x84b549c8 } };
KrmSha256Digest g_krmProvisionedChallengeAlternateDigest KRM_PROVISIONED = { .words = { 0x348a24b7, 0x1f2c3059, 0x035beb58, 0x6d150fbf, 
		0x99ad7404, 0x7d5f8427, 0xb606d222, 0x127d6392 } };
unsigned int g_krmPrimaryWrappingDigestIv[4] KRM_PROVISIONED = {
	0x021e8e29, 0xc38a1ea2, 0xa70b3ea9, 0x2e413465,
};
KrmSha256Digest g_krmAlternateWrappingDigest KRM_PROVISIONED = {
	.words = { 0xbe4d1022, 0x4a466bd2, 0x03100540, 0xa1c93a8a, 
		0x3d276906, 0x4cba52a7, 0x8c615b11, 0x8868e3cf },
};
KrmKey g_krmWrappingKey KRM_PROVISIONED = {
	.info = { .version = 1, .keyWords = 8, .algorithm = 1, .permissions = 0x20 },
};
KrmSha256Digest g_krmAlternateWrappingKey KRM_PROVISIONED = { .words = { 0xc868d37f, 0xbe13ef65, 0x268c551f, 0xb0fbda38, 
		0x6bf51835, 0x12d23372, 0x98cef823, 0x3aede7ab } };
KrmSha256Digest g_krmPrimaryWrappingDigest KRM_PROVISIONED = { .words = { 0x82a0b161, 0x51ea122a, 0xebce7438, 0x64903384, 
		0x939fff9b, 0xacad4bb8, 0x6c70496a, 0xb2e28bcb } };
unsigned int g_krmProvisionedPadding __attribute__((section(".data.krmProvisioned"), aligned(4))) = 0;
int g_krmDefaultKeyIndex __attribute__((section(".data.krmProvisioned"), aligned(4))) = -1;
unsigned int g_krmAlternateWrappingKeyIv[4] KRM_PROVISIONED = { 0x63f7b47f, 0xdac4641d, 0x8c42dccf, 0x8d453b73 };
unsigned int g_krmPrimaryWrappingKeyIv[4] KRM_PROVISIONED = { 0x68b6bcdf, 0x57356cf9, 0xdba27c51, 0x45b193c3 };
unsigned int g_krmAlternateWrappingDigestIv[4] KRM_PROVISIONED = { 0xdbecf7fe, 0x31a0a82e, 0x5fbf32f1, 0x5b571d9a };
KrmSha256Digest g_krmPrimaryWrappingKey KRM_PROVISIONED = { .words = { 0x44824bc3, 0x6b69bd39, 0x626a2699, 0x5b29c4dd, 
		0x8ec1f49f, 0x60c65c45, 0x1d6f2c8b, 0x853506c9 } };
KrmEccPrivateKey g_krmProvisionedAlternateKey KRM_PROVISIONED = { .scalar = { 0xeb935f8e, 0xb1fcea6e, 0x356a0bac, 0xa443a0ae, 
		0x94fedec7, 0x87577e39, 0xdb247093, 0x7fb4a232 } };
KrmSha256Digest g_krmProvisionedAlternateDigest KRM_PROVISIONED = { .words = { 0x4220cb2a, 0x14a30437, 0x2d356eaa, 0xe786de66, 
		0xb4da22eb, 0xa220fbaa, 0xc57ef1b0, 0x9109aad8 } };
KrmSha256Digest g_krmProvisionedPrimaryDigest KRM_PROVISIONED = { .words = { 0x83924566, 0x846e1574, 0xa3871e92, 0x58f1a79a, 
		0x75e7bb30, 0x224257c9, 0xe1986549, 0x019b373e } };
SceKernelOpenPsId g_krmProvisionedAlternateDigestIv KRM_PROVISIONED = { .id = { 0x7c, 0xd2, 0x95, 0x87, 0xbe, 0xb5, 0xc3, 0x26, 
		0xa6, 0x7f, 0x80, 0x44, 0x39, 0x2f, 0x5e, 0xac } };
SceKernelOpenPsId g_krmProvisionedAlternateKeyIv KRM_PROVISIONED = { .id = { 0x8c, 0x11, 0xb5, 0x35, 0x4c, 0x9b, 0x73, 0xf4, 
		0x30, 0x34, 0xea, 0xcf, 0x78, 0x72, 0x7b, 0x15 } };
SceKernelOpenPsId g_krmProvisionedPrimaryDigestIv KRM_PROVISIONED = { .id = { 0x9b, 0x6f, 0x4f, 0x60, 0x2d, 0x3c, 0x7d, 0x74, 
		0xab, 0x95, 0xc3, 0x60, 0xd2, 0x16, 0x91, 0xf2 } };
SceKernelOpenPsId g_krmProvisionedPrimaryKeyIv KRM_PROVISIONED = { .id = { 0x6f, 0xc6, 0xea, 0x04, 0x60, 0xfe, 0xe1, 0xd2, 
		0x30, 0xca, 0x47, 0x05, 0xed, 0xf7, 0x9f, 0x91 } };
KrmEccPrivateKey g_krmProvisionedPrimaryKey KRM_PROVISIONED = { .scalar = { 0xf5b21708, 0x61adf4c7, 0xb4552304, 0x719fad33, 
		0xaa42f8ee, 0x2a56b6b0, 0x21ad5d03, 0x857fdb7a } };
#else
unsigned int g_krmProvisionedChallengeAlternateKeyIv[4] KRM_PROVISIONED = { 0x8681dfc0, 0xcec504b7, 0xbcfeb9b2, 0x275d45ec };
unsigned int g_krmProvisionedChallengePrimaryKeyIv[4] KRM_PROVISIONED = { 0x3564d8e9, 0xb7ba6872, 0x8038ff8c, 0xb306b4fe };
unsigned int g_krmProvisionedChallengeAlternateDigestIv[4] KRM_PROVISIONED = { 0xfffc4c1d, 0x919bc283, 0xc57b0976, 0x7bfeba13 };
unsigned int g_krmProvisionedChallengePrimaryDigestIv[4] KRM_PROVISIONED = { 0xc645fcf8, 0x1ee5a7a0, 0x14f79b64, 0x0fe58da4 };
KrmSha256Digest g_krmProvisionedChallengePrimaryDigest KRM_PROVISIONED = { .words = { 0xce6a23a2, 0x9265f6b9, 0x86cee29f, 0xcc8b81ac, 
		0x0add86c6, 0x4978515d, 0xe78fdf7d, 0xd9f660a2 } };
KrmEccPrivateKey g_krmProvisionedChallengeAlternateKey KRM_PROVISIONED = { .scalar = { 0xc3ae354b, 0x07024fd9, 0x1d971881, 0x596d2871, 
		0xe90f6fa2, 0x528a1158, 0x77942206, 0x75573aed } };
KrmEccPrivateKey g_krmProvisionedChallengePrimaryKey KRM_PROVISIONED = { .scalar = { 0x2e3f6a46, 0x12a85ea7, 0x4be23822, 0x572238c7, 
		0xf2cca18b, 0xecb938fd, 0xa35fa7b1, 0x84b549c8 } };
KrmSha256Digest g_krmProvisionedChallengeAlternateDigest KRM_PROVISIONED = { .words = { 0x348a24b7, 0x1f2c3059, 0x035beb58, 0x6d150fbf, 
		0x99ad7404, 0x7d5f8427, 0xb606d222, 0x127d6392 } };
unsigned int g_krmPrimaryWrappingDigestIv[4] KRM_PROVISIONED = {
	0x021e8e29, 0xc38a1ea2, 0xa70b3ea9, 0x2e413465,
};
KrmSha256Digest g_krmAlternateWrappingDigest KRM_PROVISIONED = {
	.words = { 0xbe4d1022, 0x4a466bd2, 0x03100540, 0xa1c93a8a, 
		0x3d276906, 0x4cba52a7, 0x8c615b11, 0x8868e3cf },
};
KrmKey g_krmWrappingKey KRM_PROVISIONED = {
	.info = { .version = 1, .keyWords = 8, .algorithm = 1, .permissions = 0x20 },
};
KrmSha256Digest g_krmAlternateWrappingKey KRM_PROVISIONED = { .words = { 0xc868d37f, 0xbe13ef65, 0x268c551f, 0xb0fbda38, 
		0x6bf51835, 0x12d23372, 0x98cef823, 0x3aede7ab } };
KrmSha256Digest g_krmPrimaryWrappingKey KRM_PROVISIONED = { .words = { 0x44824bc3, 0x6b69bd39, 0x626a2699, 0x5b29c4dd, 
		0x8ec1f49f, 0x60c65c45, 0x1d6f2c8b, 0x853506c9 } };
unsigned int g_krmProvisionedPadding __attribute__((section(".data.krmProvisioned"), aligned(4))) = 0;
int g_krmDefaultKeyIndex __attribute__((section(".data.krmProvisioned"), aligned(4))) = -1;
KrmSha256Digest g_krmPrimaryWrappingDigest KRM_PROVISIONED = { .words = { 0x82a0b161, 0x51ea122a, 0xebce7438, 0x64903384, 
		0x939fff9b, 0xacad4bb8, 0x6c70496a, 0xb2e28bcb } };
unsigned int g_krmAlternateWrappingKeyIv[4] KRM_PROVISIONED = { 0x63f7b47f, 0xdac4641d, 0x8c42dccf, 0x8d453b73 };
unsigned int g_krmPrimaryWrappingKeyIv[4] KRM_PROVISIONED = { 0x68b6bcdf, 0x57356cf9, 0xdba27c51, 0x45b193c3 };
unsigned int g_krmAlternateWrappingDigestIv[4] KRM_PROVISIONED = { 0xdbecf7fe, 0x31a0a82e, 0x5fbf32f1, 0x5b571d9a };
SceKernelOpenPsId g_krmProvisionedPrimaryKeyIv KRM_PROVISIONED = { .id = { 0x6f, 0xc6, 0xea, 0x04, 0x60, 0xfe, 0xe1, 0xd2, 
		0x30, 0xca, 0x47, 0x05, 0xed, 0xf7, 0x9f, 0x91 } };
KrmEccPrivateKey g_krmProvisionedAlternateKey KRM_PROVISIONED = { .scalar = { 0xeb935f8e, 0xb1fcea6e, 0x356a0bac, 0xa443a0ae, 
		0x94fedec7, 0x87577e39, 0xdb247093, 0x7fb4a232 } };
KrmEccPrivateKey g_krmProvisionedPrimaryKey KRM_PROVISIONED = { .scalar = { 0xf5b21708, 0x61adf4c7, 0xb4552304, 0x719fad33, 
		0xaa42f8ee, 0x2a56b6b0, 0x21ad5d03, 0x857fdb7a } };
SceKernelOpenPsId g_krmProvisionedPrimaryDigestIv KRM_PROVISIONED = { .id = { 0x9b, 0x6f, 0x4f, 0x60, 0x2d, 0x3c, 0x7d, 0x74, 
		0xab, 0x95, 0xc3, 0x60, 0xd2, 0x16, 0x91, 0xf2 } };
SceKernelOpenPsId g_krmProvisionedAlternateDigestIv KRM_PROVISIONED = { .id = { 0x7c, 0xd2, 0x95, 0x87, 0xbe, 0xb5, 0xc3, 0x26, 
		0xa6, 0x7f, 0x80, 0x44, 0x39, 0x2f, 0x5e, 0xac } };
KrmSha256Digest g_krmProvisionedAlternateDigest KRM_PROVISIONED = { .words = { 0x4220cb2a, 0x14a30437, 0x2d356eaa, 0xe786de66, 
		0xb4da22eb, 0xa220fbaa, 0xc57ef1b0, 0x9109aad8 } };
KrmSha256Digest g_krmProvisionedPrimaryDigest KRM_PROVISIONED = { .words = { 0x83924566, 0x846e1574, 0xa3871e92, 0x58f1a79a, 
		0x75e7bb30, 0x224257c9, 0xe1986549, 0x019b373e } };
SceKernelOpenPsId g_krmProvisionedAlternateKeyIv KRM_PROVISIONED = { .id = { 0x8c, 0x11, 0xb5, 0x35, 0x4c, 0x9b, 0x73, 0xf4, 
		0x30, 0x34, 0xea, 0xcf, 0x78, 0x72, 0x7b, 0x15 } };
#endif
#undef KRM_PROVISIONED

/* Internal names unknown; the debug build identifies this file as driver.c. */
int KrmDriverLock(void)
{
	return lockFastMutex(&g_krmDriverMutex);
}

int KrmDriverUnlock(void)
{
	return sceKernelUnlockFastMutex(&g_krmDriverMutex);
}

void sceKrmDriverSetPID(SceUID pid)
{
#if FW_DEBUG
	if (SceDebugForDriver_374B7868() > 0) {
		int valid = 1;

		if (__builtin_expect(g_krmDriverPid != -1, 0))
			valid = pid == -1;
		SceDebugForDriver_FD753E7A(1, valid,
			&g_krmSetPidDebug,
			__builtin_return_address(0));
	}
#endif
	g_krmDriverPid = pid;
}

SceUID KrmDriverGetPID(void)
{
	return g_krmDriverPid;
}

unsigned int KrmDriverGetAuthorization(SceUID pid)
{
	return SceSblACMgrForDriver_AE1AF154(pid);
}

void KrmDriverSetState(void)
{
	g_krmDriverState = 1;
}

unsigned int KrmDriverGetState(void)
{
	return g_krmDriverState;
}

void KrmDriverClearState(void)
{
	g_krmDriverState = 0;
}

int KrmDriverInitialize(void)
{
	int ret;

	g_krmDriverState = 0;
	g_krmDriverPid = -1;
	ret = sceKernelInitializeFastMutex(&g_krmDriverMutex,
					   g_krmDriverMutexName, 0, NULL);
	return ret < 0 ? ret : 0;
}

typedef unsigned char KrmAesVector
	__attribute__((vector_size(16), aligned(1), may_alias));

/* Name unknown: decrypt CBC blocks, retaining ciphertext before an in-place store. */
int KrmAesCbcDecrypt(const unsigned char *src, unsigned char *dst,
		     SceSize size, unsigned int keyBits, const void *key,
		     unsigned char *iv)
{
	KrmAesCbcContext ctx __attribute__((aligned(8)));
	unsigned char plainBlock[32] __attribute__((aligned(8)));
	const unsigned char *start = src;
	unsigned int i, blockBytes;
	KrmAesVector plain, ciphertext;
	int ret;

	if (!dst || !src || (size << 28) || !iv || !key ||
	    ((keyBits & ~0x40U) != 0x80 && keyBits != 0x100))
		return (int)0x80a00000;
	memset(&ctx, 0, sizeof(ctx));
	ret = KrmAesInitDecrypt(&ctx.aes, 128, keyBits, key);
	if (ret >= 0 && ctx.aes.blockWords * 4)
		memcpy(ctx.chainingBlock, iv, ctx.aes.blockWords * 4);
	if (size) {
		do {
			KrmAesDecryptBlock(&ctx.aes, src, plainBlock);
			blockBytes = ctx.aes.blockWords * 4;
			if (blockBytes) {
				unsigned int vectorEligible = ((uintptr_t)src >= (uintptr_t)(dst + 16)) |
					((uintptr_t)dst >= (uintptr_t)(src + 16));

				vectorEligible = blockBytes > 15 ? vectorEligible & 1 : 0;
				if (vectorEligible) {
					plain = *(KrmAesVector *)plainBlock ^
						*(KrmAesVector *)ctx.chainingBlock;
					ciphertext = *(const KrmAesVector *)src;
					*(KrmAesVector *)ctx.chainingBlock = ciphertext;
					*(KrmAesVector *)dst = plain;
					i = (blockBytes >> 4) << 4;
					if (blockBytes >> 4 != 1) {
						plain = *(KrmAesVector *)(plainBlock + 16) ^
							*(KrmAesVector *)(ctx.chainingBlock + 16);
						__builtin_prefetch(src + 64);
						ciphertext = *(const KrmAesVector *)(src + 16);
						*(KrmAesVector *)(ctx.chainingBlock + 16) = ciphertext;
						*(KrmAesVector *)(dst + 16) = plain;
					}
					if (i != blockBytes) {
						do {
							unsigned char chain = ctx.chainingBlock[i];
							unsigned char decrypted = plainBlock[i];

							ctx.chainingBlock[i] = src[i];
							dst[i++] = chain ^ decrypted;
						} while (blockBytes > i);
					}
				} else {
					i = 0;
					do {
						unsigned char chain = ctx.chainingBlock[i];
						unsigned char encrypted = src[i];

						dst[i] = chain ^ plainBlock[i];
						ctx.chainingBlock[i++] = encrypted;
					} while (i != blockBytes);
				}
			}
			src += 16;
			dst += 16;
		} while ((SceSize)(src - start) < size);
	}
	__builtin_memcpy(iv, ctx.chainingBlock, 16);
	return 0;
}

/* Name unknown: encrypt CBC blocks and return the final chaining block. */
int KrmAesCbcEncrypt(const unsigned char *src, unsigned char *dst,
		     SceSize size, unsigned int keyBits, const void *key,
		     unsigned char *iv)
{
	KrmAesCbcContext ctx __attribute__((aligned(8)));
	unsigned char plainBlock[32] __attribute__((aligned(8)));
	const unsigned char *start = src;
	unsigned int i, blockBytes;
	int ret;

	if (!dst || !src || (size << 28) || !iv || !key ||
	    ((keyBits & ~0x40U) != 0x80 && keyBits != 0x100))
		return (int)0x80a00000;
	memset(&ctx, 0, sizeof(ctx));
	ret = KrmAesInitEncrypt(&ctx.aes, 128, keyBits, key);
	if (ret >= 0) {
		blockBytes = ctx.aes.blockWords * 4;
		if (blockBytes)
			memcpy(ctx.chainingBlock, iv, blockBytes);
	}
	if (size) {
		blockBytes = ctx.aes.blockWords * 4;
		do {
			unsigned int vectorCount = blockBytes >> 4;
			const unsigned char *nextSrc = src + 16;

			i = vectorCount << 4;
			if (!vectorCount || blockBytes <= 15) {
				i = 0;
				goto xorTail;
			}
			{
				const KrmAesVector *vectorSrc = (const KrmAesVector *)src;
				KrmAesVector input = *vectorSrc++;

				nextSrc = (const unsigned char *)vectorSrc;
				*(KrmAesVector *)plainBlock =
					input ^
					*(KrmAesVector *)ctx.chainingBlock;
				if (vectorCount > 1) {
					__builtin_prefetch(src + 48);
					*(KrmAesVector *)(plainBlock + 16) =
						*vectorSrc ^
						*(KrmAesVector *)(ctx.chainingBlock + 16);
				}
			}
xorTail:
			if (i != blockBytes) {
#define KRM_CBC_XOR_BYTE(n) \
				plainBlock[i + (n)] = src[i + (n)] ^ \
					ctx.chainingBlock[i + (n)]; \
				if (i + (n) + 1 >= blockBytes) \
					goto encryptBlock
				KRM_CBC_XOR_BYTE(0);
				KRM_CBC_XOR_BYTE(1);
				KRM_CBC_XOR_BYTE(2);
				KRM_CBC_XOR_BYTE(3);
				KRM_CBC_XOR_BYTE(4);
				KRM_CBC_XOR_BYTE(5);
				KRM_CBC_XOR_BYTE(6);
				KRM_CBC_XOR_BYTE(7);
				KRM_CBC_XOR_BYTE(8);
				KRM_CBC_XOR_BYTE(9);
				KRM_CBC_XOR_BYTE(10);
				KRM_CBC_XOR_BYTE(11);
				KRM_CBC_XOR_BYTE(12);
				KRM_CBC_XOR_BYTE(13);
				KRM_CBC_XOR_BYTE(14);
#undef KRM_CBC_XOR_BYTE
				plainBlock[i + 15] = src[i + 15] ^
					ctx.chainingBlock[i + 15];
			}
encryptBlock:
			KrmAesEncryptBlock(&ctx.aes, plainBlock, dst);
			blockBytes = ctx.aes.blockWords * 4;
			if (blockBytes)
				memcpy(ctx.chainingBlock, dst, blockBytes);
			src = nextSrc;
			dst += 16;
		} while ((SceSize)(src - start) < size);
	}
	__builtin_memcpy(iv, ctx.chainingBlock, 16);
	return 0;
}

/* Names unknown: single-block wrappers deliberately ignore service errors. */
int KrmAesDecrypt(const void *src, void *dst, unsigned int keyBits,
		  const void *key)
{
	SceAesCompactContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	KrmAesInitDecrypt(&ctx, 128, keyBits, key);
	KrmAesDecryptBlock(&ctx, src, dst);
	return 0;
}

int KrmAesEncrypt(const void *src, void *dst, unsigned int keyBits,
		  const void *key)
{
	SceAesCompactContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	KrmAesInitEncrypt(&ctx, 128, keyBits, key);
	KrmAesEncryptBlock(&ctx, src, dst);
	return 0;
}

/* Names unknown: the driver accepts only the 128-bit MAC key. */
__attribute__((noinline)) int KrmComputeMac(const void *src, SceSize size, void *mac,
		  SceSize keySize, const void *key)
{
	if (keySize != 16)
		return (int)0x80a00000;
	return SceKernelUtilsForDriver_1DF3792A(128, 128, key, src, size, mac);
}

int KrmVerifyMac(const void *src, SceSize size, const void *mac,
		 SceSize keySize, const void *key)
{
	unsigned char calculatedMac[16];
	int ret;

	if (keySize != 16)
		return (int)0x80a00000;
	ret = SceKernelUtilsForDriver_1DF3792A(128, 128, key, src, size,
					    calculatedMac);
	if (ret == 0)
		ret = timingsafe_memcmp(mac, calculatedMac, 16) ?
			(int)0x80a00008 : 0;
	return ret;
}

/* Names unknown: driver wrappers preserve local copies across ECC calls. */
int KrmDriverVerifySignature(const KrmEccSignature *signature, const void *src,
			     SceSize size, const KrmEccPublicKey *publicKey)
{
	KrmEccSignature signatureCopy;
	KrmEccPublicKey publicKeyCopy;

	if (!signature || !src || !publicKey)
		return (int)0x80a00000;
	__builtin_memcpy(&signatureCopy, signature, sizeof(signatureCopy));
	__builtin_memcpy(&publicKeyCopy, publicKey, sizeof(publicKeyCopy));
	return KrmEccVerifyMessage(src, size, &publicKeyCopy, &signatureCopy,
				 &g_krmEccContext);
}

int KrmDriverSignMessage(KrmEccSignature *signature, const void *src,
			 SceSize size, const KrmEccPrivateKey *privateKey)
{
	KrmEccPrivateKey privateKeyCopy;
	KrmEccSignature signatureCopy;
	int ret;

	if (!signature || !src || !privateKey)
		return (int)0x80a00000;
	__builtin_memcpy(&privateKeyCopy, privateKey, sizeof(privateKeyCopy));
	ret = KrmEccSignMessage(src, size, &privateKeyCopy, &signatureCopy,
				&g_krmEccContext);
	if (ret == 0)
		__builtin_memcpy(signature, &signatureCopy, sizeof(signatureCopy));
	return ret;
}

int KrmDriverGenerateKeyPair(KrmEccPublicKey *publicKey,
			    KrmEccPrivateKey *privateKey)
{
	KrmEccPrivateKey privateKeyCopy;
	KrmEccPublicKey publicKeyCopy;
	int ret;

	ret = KrmEccGenerateKeys(&publicKeyCopy, &privateKeyCopy, &g_krmEccContext);
	if (__builtin_expect(ret != 0, 1))
		return (int)0x80a00008;
	__builtin_memcpy(publicKey, &publicKeyCopy, sizeof(publicKeyCopy));
	__builtin_memcpy(privateKey, &privateKeyCopy, sizeof(privateKeyCopy));
	return ret;
}

int KrmDriverEncodeEcc(const KrmEccPrivateKey *value, KrmEccEncodedBlock *encoded,
		       const KrmEccPublicKey *publicKey)
{
	KrmEccPrivateKey valueCopy;
	KrmEccPublicKey publicKeyCopy;
	KrmEccEncodedBlock encodedCopy;
	int ret;

	__builtin_memcpy(&publicKeyCopy, publicKey, sizeof(publicKeyCopy));
	__builtin_memcpy(&valueCopy, value, sizeof(valueCopy));
	ret = KrmEccEncode(&publicKeyCopy, &valueCopy, &encodedCopy, &g_krmEccContext);
	if (ret == 0)
		memcpy(encoded, &encodedCopy, sizeof(encodedCopy));
	return ret;
}

int KrmDriverDecodeEcc(const KrmEccEncodedBlock *encoded, KrmEccPrivateKey *value,
		       const KrmEccPrivateKey *privateKey)
{
	KrmEccPrivateKey privateKeyCopy;
	KrmEccPrivateKey valueCopy;
	KrmEccEncodedBlock encodedCopy;
	int ret;

	__builtin_memcpy(&privateKeyCopy, privateKey, sizeof(privateKeyCopy));
	memcpy(&encodedCopy, encoded, sizeof(encodedCopy));
	ret = KrmEccDecode(&privateKeyCopy, &encodedCopy, &valueCopy, &g_krmEccContext);
	if (ret == 0)
		__builtin_memcpy(value, &valueCopy, sizeof(valueCopy));
	return ret;
}

int KrmDriverSignDigest(KrmEccSignature *signature, const void *digest,
			const KrmEccPrivateKey *privateKey)
{
	KrmEccPrivateKey privateKeyCopy;
	KrmEccSignature signatureCopy;
	int ret;

	memset(&signatureCopy, 0, sizeof(signatureCopy));
	__builtin_memcpy(&privateKeyCopy, privateKey, sizeof(privateKeyCopy));
	ret = KrmEccSignDigest(digest, &privateKeyCopy, &signatureCopy, &g_krmEccContext);
	if (ret == 0)
		__builtin_memcpy(signature, &signatureCopy, sizeof(signatureCopy));
	return ret;
}

int KrmDriverVerifyDigest(const KrmEccSignature *signature, const void *digest,
			  const KrmEccPublicKey *publicKey)
{
	KrmEccSignature signatureCopy;
	KrmEccPublicKey publicKeyCopy;

	__builtin_memcpy(&signatureCopy, signature, sizeof(signatureCopy));
	__builtin_memcpy(&publicKeyCopy, publicKey, sizeof(publicKeyCopy));
	return KrmEccVerifyDigest(digest, &publicKeyCopy, &signatureCopy,
				&g_krmEccContext);
}

int KrmDriverValidatePublicKey(const KrmEccPublicKey *publicKey)
{
	KrmEccPublicKey point;

	KrmEccInitialize(&g_krmEccContext);
	KrmEccImportPoint(publicKey, &point);
	return KrmEccValidatePoint(&point, &g_krmEccContext) ? (int)0x80a00008 : 0;
}

int KrmDriverGetRandomBytes(unsigned char *dst, unsigned int size)
{
	unsigned char block[64];
	unsigned int count;
	int ret = 0;

	while (size) {
		count = size < sizeof(block) ? size : sizeof(block);
		ret = SceSblSsMgrForDriver_AC57F4F0(block);
		if (ret < 0)
			break;
		memcpy(dst, block, count);
		dst += count;
		size -= count;
	}
	memset(block, 0, sizeof(block));
	return ret;
}

int KrmDriverSha256(void *digest, const void *src, SceSize size)
{
	SceSha256Context ctx;

	SceKernelUtilsForDriver_D909FA2C(&ctx);
	SceKernelUtilsForDriver_236A9097(&ctx, src, size);
	SceKernelUtilsForDriver_4899CD4B(&ctx, digest);
	return 0;
}

int KrmDriverCopyFromUser(void *dst, const void *src, SceSize size)
{
	return sceKernelCopyFromUser(dst, src, size);
}

int KrmDriverCopyToUser(void *dst, const void *src, SceSize size)
{
	return sceKernelCopyToUser(dst, src, size);
}

int KrmDriverIsAccessible(const void *addr, SceSize size, unsigned int flags)
{
	unsigned int low = (flags & 1) ? 16 : 0;
	unsigned int type = (flags & 1) ? 48 : 32;

	if (!(flags & 2))
		type = low;
	return sceKernelIsAccessibleRange(type, addr, size) == 0;
}

unsigned int KrmDriverGetVersion(void)
{
#if FW_DEBUG
	return FW_SDK_VERSION;
#else
	return FW_SDK_VERSION | 0x11;
#endif
}

int KrmDriverGetOpenPsId(SceKernelOpenPsId *id)
{
	SceKernelOpenPsId localId;
	int ret;

	if (!id)
		return (int)0x80a00000;
	ret = sceSblAimgrGetOpenPsId(&localId);
	if (ret == 0)
		*id = localId;
	return ret;
}

int KrmDriverDecryptPortable(const void *iv, const void *src, void *dst)
{
	SceKernelOpenPsId localIv;
	ScePortabilityData input;
	ScePortabilityData output;
	int ret;

	__builtin_memcpy(&localIv, iv, sizeof(localIv));
	input.msg_size = sizeof(input.msg);
	__builtin_memcpy(input.msg, src, sizeof(input.msg));
	output.msg_size = sizeof(output.msg);
	__builtin_memset(output.msg, 0, sizeof(output.msg));
	ret = SceSblSsMgrForDriver_934DB6B5(11, &localIv, &input, &output);
	if (ret == 0)
		__builtin_memcpy(dst, output.msg, sizeof(output.msg));
	__builtin_memset(&localIv, 0, sizeof(localIv));
	__builtin_memset(&input, 0, sizeof(input));
	__builtin_memset(&output, 0, sizeof(output));
	return ret;
}

int KrmDriverGetNetworkTick(uint64_t *tick)
{
	SceRtcTick localTick;

	if (!tick)
		return (int)0x80a00000;
	if (SceRtcForDriver_F1517B38(&localTick) < 0)
		return (int)0x80a00009;
	*tick = localTick.tick;
	return 0;
}

int KrmDriverInitializeService(void)
{
	return 0;
}

int KrmDriverCheckExpiration(const KrmKeyInfo *info)
{
	uint64_t tick;
	int ret;

	if (!info)
		return (int)0x80a00000;
	ret = info->permissions & 8;
	if (ret) {
		ret = KrmDriverGetNetworkTick(&tick);
		if (ret >= 0) {
			ret = (int)0x80a00006;
			if (info->expirationTick >= tick)
				ret = 0;
		}
	}
	return ret;
}

int KrmDriverEncrypt(const KrmKey *key, unsigned char *iv, const void *src,
		     SceSize size, void *dst, SceSize dstSize)
{
	uint64_t tick;
	int ret;

	if (!key || !src)
		return (int)0x80a00000;
	if (!dst)
		return (int)0x80a00000;
	if (!(key->info.permissions & 0x40))
		return (int)0x80a00006;
	if (key->info.permissions & 8) {
		ret = KrmDriverGetNetworkTick(&tick);
		if (ret < 0)
			return ret;
		if (key->info.expirationTick < tick)
			return (int)0x80a00006;
	}
	switch (key->info.algorithm) {
	case 3:
		if (size == dstSize)
			return KrmAesEncrypt(src, dst, 32 * key->info.keyWords,
					     key->material.symmetric);
		break;
	case 2:
		if (size != dstSize)
			return (int)0x80a00000;
		if (!iv)
			return (int)0x80a00000;
		return KrmAesCbcEncrypt(src, dst, size, 32 * key->info.keyWords,
				       key->material.symmetric, iv);
	case 1:
		if (key->info.keyWords == 8 && dstSize == 128 && size == 32)
			return KrmDriverEncodeEcc(src, dst, &key->material.ecc.publicKey);
		break;
	default:
		return (int)0x80a00007;
	}
	return (int)0x80a00000;
}

int KrmDriverDecrypt(const KrmKey *key, unsigned char *iv, const void *src,
		     SceSize size, void *dst, SceSize dstSize)
{
	uint64_t tick;
	int ret;

	if (!key || !src)
		return (int)0x80a00000;
	if (!dst)
		return (int)0x80a00000;
	if (!(key->info.permissions & 0x20))
		return (int)0x80a00006;
	if (key->info.permissions & 8) {
		ret = KrmDriverGetNetworkTick(&tick);
		if (ret < 0)
			return ret;
		if (key->info.expirationTick < tick)
			return (int)0x80a00006;
	}
	switch (key->info.algorithm) {
	case 2:
		if (size != dstSize)
			return (int)0x80a00000;
		if (!iv)
			return (int)0x80a00000;
		return KrmAesCbcDecrypt(src, dst, size, 32 * key->info.keyWords,
				       key->material.symmetric, iv);
	case 3:
		if (size == dstSize)
			return KrmAesDecrypt(src, dst, 32 * key->info.keyWords,
					     key->material.symmetric);
		break;
	case 1:
		if (key->info.keyWords == 8 && dstSize == 32 && size == 128)
			return KrmDriverDecodeEcc(src, dst, &key->material.ecc.privateKey);
		break;
	default:
		return (int)0x80a00007;
	}
	return (int)0x80a00000;
}

int KrmDriverSign(const KrmKey *key, const void *src, SceSize size, void *signature,
		SceSize signatureSize)
{
	uint64_t tick;
	int ret;

	if (!key || !src)
		return (int)0x80a00000;
	if (!signature)
		return (int)0x80a00000;
	if (!(key->info.permissions & 0x80))
		return (int)0x80a00006;
	if (key->info.permissions & 8) {
		ret = KrmDriverGetNetworkTick(&tick);
		if (ret < 0)
			return ret;
		if (key->info.expirationTick < tick)
			return (int)0x80a00006;
	}
	switch (key->info.algorithm) {
	default:
		ret = (int)0x80a00007;
		break;
	case 1:
		if (key->info.keyWords != 8)
			return (int)0x80a00000;
		if (signatureSize != 64)
			return (int)0x80a00000;
		ret = KrmDriverSignMessage(signature, src, size, &key->material.ecc.privateKey);
		break;
	case 4:
		if (key->info.keyWords != 4)
			return (int)0x80a00000;
		if (signatureSize != 16)
			return (int)0x80a00000;
		ret = KrmComputeMac(src, size, signature, signatureSize, key->material.symmetric);
		break;
	}
	return ret;
}

int KrmDriverVerify(const KrmKey *key, const void *src, SceSize size, const void *signature,
		SceSize signatureSize)
{
	uint64_t tick;
	int ret;

	if (!key || !src)
		return (int)0x80a00000;
	if (!signature)
		return (int)0x80a00000;
	if (!(key->info.permissions & 0x100))
		return (int)0x80a00006;
	if (key->info.permissions & 8) {
		ret = KrmDriverGetNetworkTick(&tick);
		if (ret < 0)
			return ret;
		if (key->info.expirationTick < tick)
			return (int)0x80a00006;
	}
	switch (key->info.algorithm) {
	case 1:
		if (key->info.keyWords != 8)
			return (int)0x80a00000;
		if (signatureSize != 64)
			return (int)0x80a00000;
		return KrmDriverVerifySignature(signature, src, size, &key->material.ecc.publicKey);
	case 4:
		if (key->info.keyWords != 4)
			return (int)0x80a00000;
		if (signatureSize != 16)
			return (int)0x80a00000;
		return KrmVerifyMac(src, size, signature, signatureSize, key->material.symmetric);
	default:
		return (int)0x80a00007;
	}
}

int KrmDriverSignHash(const KrmKey *key, const void *src, SceSize size, void *signature,
		SceSize signatureSize)
{
	uint64_t tick;
	int ret;

	if (!src || !key || !signature)
		return (int)0x80a00000;
	if (!(key->info.permissions & 0x80))
		return (int)0x80a00006;
	if (key->info.permissions & 8) {
		ret = KrmDriverGetNetworkTick(&tick);
		if (ret < 0)
			return ret;
		if (key->info.expirationTick < tick)
			return (int)0x80a00006;
	}
	if (key->info.algorithm == 1) {
		if (key->info.keyWords == 8 && signatureSize == 64 && size == 32)
			return KrmDriverSignDigest(signature, src, &key->material.ecc.privateKey);
	} else {
		return (int)0x80a00007;
	}
	return (int)0x80a00000;
}

int KrmDriverVerifyHash(const KrmKey *key, const void *src, SceSize size, const void *signature,
		SceSize signatureSize)
{
	uint64_t tick;
	int ret;

	if (!src || !key || !signature)
		return (int)0x80a00000;
	if (!(key->info.permissions & 0x100))
		return (int)0x80a00006;
	if (key->info.permissions & 8) {
		ret = KrmDriverGetNetworkTick(&tick);
		if (ret < 0)
			return ret;
		if (key->info.expirationTick < tick)
			return (int)0x80a00006;
	}
	if (key->info.algorithm == 1) {
		if (key->info.keyWords == 8 && signatureSize == 64 && size == 32)
			return KrmDriverVerifyDigest(signature, src, &key->material.ecc.publicKey);
	} else {
		return (int)0x80a00007;
	}
	return (int)0x80a00000;
}
