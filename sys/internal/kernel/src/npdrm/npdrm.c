/*
 * SceNpDrm startup and remaining export placeholders.
 */

#include <skeleton.h>
#include <threadmgr/threadmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <npdrm/npdrm.h>
#include <npdrm/psmdrm.h>
#include <gcauthmgr/sblgcauthmgrdrmbb.h>
#include <gcauthmgr/sblgcauthmgrpsmact.h>
#include <ss_mgr/ss_mgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/aes.h>
#include <sysmem/qafmgr.h>
#include <sysmem/utils.h>
#include <sysmem/sysclib.h>
#include <regmgr/regmgr.h>
#include <marlin_hci/rsa.h>
#include <rtc/rtc.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <authmgr/sblauthmgr.h>
#include "../../../driver/src/msif/msif_ecdsa.h"

struct NpDrmActivationKeyMaterial {
	unsigned char activationKey[16];
	unsigned char keyTableIndexKey[16];
	unsigned char bindingSeed[32];
	unsigned char prime[20];
	unsigned char coefficientA[20];
	unsigned char coefficientB[20];
	unsigned char order[20];
	unsigned char baseX[20];
	unsigned char baseY[20];
	unsigned char encryptionPadding[8];
};

_Static_assert(sizeof(struct NpDrmActivationKeyMaterial) == 192,
	"activation key material ABI");

struct NpDrmActivationCache {
	SceNpDrmActivationData data;
	unsigned char encryptionPadding[8];
};

_Static_assert(sizeof(struct NpDrmActivationCache) == 0x1040,
	"activation cache transport ABI");

struct PsmDrmAesMaterial {
	unsigned char key[16];
	unsigned char iv[16];
};

struct PsmDrmActivationData {
	char magic[8];
	unsigned int version;
	unsigned int flags;
	SceUInt64 accountId;
	unsigned int bindingValidation;
	unsigned int recordValidation;
	SceRtcTick startTime;
	SceRtcTick expirationTime;
	unsigned char activationChecksum[32];
	unsigned char authenticatedHeaderData[176];
	ScePsmDrmKeySet keyset;
	unsigned char rsaSignature[256];
};

union PsmDrmWorkingData {
	struct PsmDrmActivationData activation;
	ScePsmDrmLicense license;
};

struct PsmDrmActivationState {
	unsigned int version;
	unsigned int flags;
	SceUInt64 accountId;
	SceRtcTick startTime;
	SceRtcTick expirationTime;
	struct PsmDrmAesMaterial binding;
};

_Static_assert(sizeof(struct PsmDrmAesMaterial) == 32, "PSM AES material ABI");
_Static_assert(sizeof(struct PsmDrmActivationData) == 1024, "PSM activation ABI");
_Static_assert(sizeof(union PsmDrmWorkingData) == 1024, "PSM working data ABI");
_Static_assert(sizeof(struct PsmDrmActivationState) == 64, "PSM activation state ABI");
_Static_assert(offsetof(struct PsmDrmActivationData, keyset) == 256,
	"PSM activation keyset ABI");
_Static_assert(offsetof(struct PsmDrmActivationState, binding) == 32,
	"PSM activation binding ABI");

/* The activation key material as stored in the module; sceSblAuthMgrGetEKc unwraps it. */
const struct NpDrmActivationKeyMaterial gNpDrmEncryptedActivationKeyMaterial = {
	.activationKey = {
		0x3b, 0x88, 0xe5, 0x12, 0x5f, 0x0a, 0xc0, 0x49,
		0xfe, 0x6c, 0xd9, 0x02, 0x3c, 0x53, 0x32, 0x8a,
	},
	.keyTableIndexKey = {
		0x6c, 0x2a, 0xcd, 0x4e, 0x7f, 0x3a, 0x14, 0x84,
		0x57, 0x31, 0x1b, 0x7c, 0x06, 0x0e, 0xef, 0xfb,
	},
	.bindingSeed = {
		0x8e, 0x78, 0x02, 0x8a, 0x34, 0xb6, 0xfd, 0xb1,
		0xf9, 0xdb, 0xd9, 0x93, 0x5f, 0xbc, 0xeb, 0xe4,
		0xbe, 0x73, 0x0d, 0x21, 0x71, 0x83, 0x5c, 0x37,
		0x2e, 0x64, 0xaf, 0x0b, 0xa5, 0x3a, 0xc2, 0x13,
	},
	.prime = {
		0x15, 0xa3, 0xc2, 0xcc, 0xcf, 0xa8, 0xdc, 0x23, 0x3a, 0x70,
		0x16, 0x2f, 0x69, 0xcd, 0xd4, 0x38, 0x42, 0x65, 0x06, 0x2e,
	},
	.coefficientA = {
		0x60, 0x69, 0xd3, 0xd2, 0x53, 0xc3, 0x57, 0x92, 0x9d, 0x8b,
		0x9d, 0xc8, 0xaa, 0xe1, 0x19, 0xcc, 0xd2, 0x7a, 0xd0, 0x14,
	},
	.coefficientB = {
		0xe8, 0x07, 0x23, 0xad, 0x3f, 0x69, 0xc0, 0x57, 0xb0, 0x6a,
		0x95, 0x3d, 0x75, 0x93, 0x5c, 0xe1, 0x16, 0x3a, 0x28, 0x55,
	},
	.order = {
		0x80, 0x97, 0x1c, 0x55, 0x30, 0x5f, 0xba, 0xcf, 0xf4, 0xf5,
		0x38, 0xf4, 0xd9, 0x98, 0x62, 0xb4, 0x5b, 0xf1, 0xcd, 0xab,
	},
	.baseX = {
		0x1d, 0x89, 0x93, 0xf3, 0xd1, 0xf3, 0x30, 0xa1, 0x10, 0x84,
		0x0c, 0x78, 0x0d, 0x72, 0xf7, 0x51, 0x61, 0x7b, 0xbb, 0x36,
	},
	.baseY = {
		0x03, 0xaf, 0x15, 0xc2, 0x68, 0xfe, 0x61, 0xf8, 0x83, 0x9e,
		0x16, 0xd3, 0x83, 0x90, 0xe3, 0x36, 0x91, 0x21, 0x45, 0x9c,
	},
	.encryptionPadding = {
		0x7c, 0xb0, 0x74, 0xec, 0x1b, 0xda, 0xe9, 0x6f,
	},
};

/* RSA-2048 moduli the act.dat and RIF signatures are checked against (exponent 65537). */
static const unsigned char gNpDrmRsaModuli[4][256] = {
	{ /* activation data */
		0xcb, 0x8d, 0xa0, 0x66, 0xd9, 0xd7, 0xd6, 0xce,
		0x41, 0x15, 0x14, 0x8d, 0x8f, 0xb6, 0x01, 0x18,
		0x08, 0x32, 0x4e, 0x16, 0x88, 0x7e, 0xf0, 0x81,
		0x64, 0xce, 0x07, 0x22, 0x83, 0xf3, 0xe7, 0xb4,
		0xff, 0x61, 0x29, 0x4b, 0xb7, 0xb5, 0xee, 0x8f,
		0x5a, 0x2f, 0x3d, 0x49, 0x48, 0x09, 0xd4, 0x27,
		0x74, 0x75, 0x08, 0x47, 0x78, 0xb3, 0x80, 0xd9,
		0x25, 0xda, 0x88, 0xcf, 0x4b, 0x83, 0xc1, 0xea,
		0x0f, 0x34, 0x60, 0x0d, 0xc0, 0x87, 0xa4, 0x52,
		0x93, 0x80, 0x38, 0x2b, 0xd2, 0x81, 0xb2, 0xb1,
		0x46, 0x0a, 0xab, 0x12, 0x43, 0xf3, 0x59, 0x44,
		0xc9, 0xb1, 0xa1, 0xd8, 0x30, 0x9e, 0x6d, 0xd2,
		0x74, 0x6d, 0x33, 0xc7, 0xa7, 0xd6, 0x3d, 0xb9,
		0x66, 0x48, 0x7c, 0xb5, 0x52, 0x4b, 0x07, 0x23,
		0x0e, 0xb9, 0xd6, 0xdf, 0x2f, 0xcf, 0x0c, 0xa9,
		0x5d, 0x84, 0x18, 0x0d, 0x41, 0xcb, 0x5d, 0x59,
		0x0c, 0xd0, 0x3a, 0x42, 0xf0, 0x90, 0x81, 0x8a,
		0x21, 0xed, 0x8f, 0xc4, 0xfa, 0x28, 0x22, 0x23,
		0xf2, 0x43, 0xc7, 0x2d, 0xa9, 0x50, 0xab, 0x2a,
		0x4c, 0x47, 0x6a, 0x2b, 0xed, 0xa7, 0xfb, 0xa0,
		0xca, 0xf8, 0x3d, 0x6f, 0x42, 0x6b, 0x7e, 0x20,
		0xa6, 0x41, 0x0a, 0x46, 0x34, 0xa5, 0xc4, 0x79,
		0x59, 0xac, 0x30, 0x16, 0xb8, 0xae, 0x25, 0x22,
		0x17, 0xe6, 0x43, 0x7d, 0x80, 0xc1, 0x09, 0x0a,
		0x9a, 0x5f, 0x36, 0x23, 0x7f, 0x50, 0xed, 0xf5,
		0x23, 0xfa, 0xed, 0xb5, 0x51, 0x6e, 0x54, 0xc4,
		0x2a, 0x61, 0x0b, 0xb1, 0x70, 0xa0, 0x4a, 0xe4,
		0xac, 0xb2, 0x3d, 0xd3, 0xa3, 0xdb, 0xb0, 0x3c,
		0x7a, 0x06, 0xe0, 0x08, 0x4e, 0x27, 0x60, 0xcd,
		0x86, 0x4b, 0x02, 0x2c, 0x72, 0x0f, 0x47, 0x68,
		0xc2, 0x93, 0xf0, 0x98, 0xee, 0x54, 0x13, 0x2d,
		0x1b, 0x64, 0xfa, 0x36, 0xa5, 0xe3, 0xcc, 0x9c,
	},
	{ /* RIF on a Vita */
		0x07, 0x75, 0x72, 0xfa, 0x0a, 0x5f, 0x43, 0x63,
		0x31, 0xf3, 0xed, 0x73, 0x2c, 0xf4, 0xd7, 0xb3,
		0x1f, 0x31, 0x0d, 0xe1, 0xcc, 0x64, 0xef, 0xf3,
		0x6a, 0x05, 0xf2, 0xc4, 0x3a, 0xee, 0x7b, 0x1c,
		0x0b, 0x87, 0x08, 0x62, 0x46, 0x8a, 0xf6, 0x3d,
		0x0c, 0xb8, 0x61, 0x9f, 0xc7, 0x04, 0x44, 0xa6,
		0x19, 0xfa, 0xc8, 0xbc, 0xcf, 0x79, 0xa8, 0xbf,
		0x36, 0xb9, 0xea, 0x12, 0x29, 0x5f, 0x8a, 0x13,
		0x99, 0xc2, 0x56, 0xea, 0x3f, 0x60, 0x4c, 0xff,
		0x7e, 0x21, 0xab, 0x33, 0xeb, 0xd2, 0x00, 0xf8,
		0x50, 0xea, 0xb6, 0x11, 0xfd, 0xf9, 0xda, 0xca,
		0xc6, 0x76, 0x18, 0x76, 0xda, 0xb1, 0xf8, 0x17,
		0x27, 0x40, 0xa3, 0x74, 0x38, 0xe9, 0x73, 0x86,
		0xeb, 0xfc, 0x9c, 0x41, 0xb0, 0x93, 0x4f, 0xf9,
		0x4b, 0x95, 0x88, 0xb2, 0x1f, 0xd3, 0xc1, 0x80,
		0xb0, 0x17, 0x1b, 0x2c, 0xec, 0xb9, 0x52, 0xe8,
		0xd5, 0xbd, 0x2a, 0xa9, 0x2f, 0xa6, 0x10, 0xaf,
		0x6b, 0x13, 0xf0, 0xb7, 0xa1, 0x59, 0x3c, 0x18,
		0x6d, 0x23, 0x9f, 0x7f, 0xfc, 0x1e, 0x36, 0x1e,
		0x48, 0xf1, 0x40, 0xbf, 0xc9, 0xb9, 0x1d, 0x5c,
		0x51, 0xc4, 0xb0, 0x6b, 0x9a, 0xe0, 0x01, 0x8f,
		0x17, 0x75, 0xc6, 0x44, 0xee, 0x3c, 0x21, 0x70,
		0x74, 0x28, 0x14, 0x4c, 0x75, 0x95, 0x38, 0x51,
		0xbb, 0xe9, 0xa3, 0x79, 0x0f, 0x73, 0x5a, 0xa4,
		0xd0, 0xe8, 0x2a, 0xd7, 0xdd, 0x8b, 0x2a, 0x36,
		0x16, 0xf2, 0xb5, 0xeb, 0x89, 0x48, 0x91, 0x6b,
		0x63, 0x5b, 0x9a, 0x99, 0x2b, 0xe4, 0x65, 0xc1,
		0xd3, 0x75, 0x1b, 0x36, 0x2c, 0x6d, 0x8c, 0xf9,
		0xbf, 0x62, 0x5d, 0x14, 0xa8, 0x38, 0x47, 0x90,
		0x78, 0x94, 0xb6, 0xe4, 0x12, 0x10, 0x2f, 0xc3,
		0xab, 0x41, 0xd0, 0x64, 0x2e, 0x48, 0xc5, 0xad,
		0x87, 0xe4, 0x5c, 0x57, 0xae, 0x7c, 0x34, 0x8f,
	},
	{ /* RIF of a PSP title */
		0xf3, 0x60, 0xa7, 0x28, 0x04, 0x32, 0x0f, 0xa9,
		0x3b, 0x55, 0x1c, 0xff, 0xb4, 0x1e, 0x50, 0x26,
		0x2d, 0x24, 0x2c, 0x1e, 0x84, 0x59, 0x3c, 0xcd,
		0xa7, 0x59, 0x46, 0xf0, 0xc4, 0xd7, 0x52, 0xe5,
		0x34, 0x35, 0xe6, 0x5c, 0x89, 0xc1, 0xb2, 0xe6,
		0x0b, 0xd4, 0xcf, 0xbf, 0x5c, 0xe2, 0xa9, 0x67,
		0xbf, 0xb8, 0x95, 0x43, 0x2c, 0x85, 0x49, 0x20,
		0x27, 0x82, 0xdb, 0x39, 0x7c, 0x0b, 0xfe, 0xeb,
		0x81, 0xa7, 0x75, 0xd7, 0x94, 0xe0, 0x69, 0x02,
		0xa8, 0xfa, 0xa8, 0x99, 0x5e, 0x37, 0xbb, 0x68,
		0x80, 0x64, 0x75, 0xf7, 0x0d, 0xa8, 0x9d, 0x5c,
		0x60, 0x3c, 0xab, 0xda, 0x63, 0x53, 0xb6, 0x8a,
		0x56, 0x63, 0x44, 0x73, 0x61, 0x45, 0xf4, 0xb5,
		0x89, 0xc6, 0x9e, 0xda, 0x07, 0x02, 0xc5, 0xf8,
		0x4c, 0x4b, 0x81, 0x3c, 0x2e, 0x42, 0x9e, 0x8b,
		0x22, 0xe0, 0x02, 0x08, 0x32, 0x9b, 0x03, 0x66,
		0xb3, 0x0d, 0xa7, 0x2d, 0x61, 0x2b, 0x21, 0x3e,
		0x17, 0xe5, 0x09, 0xec, 0x81, 0x65, 0x3e, 0x71,
		0x3b, 0x6f, 0x87, 0xee, 0x9c, 0xff, 0xea, 0xb3,
		0xe0, 0x77, 0x0c, 0x8f, 0xf0, 0x14, 0x49, 0xba,
		0xed, 0x55, 0x47, 0xf5, 0x92, 0x75, 0x1e, 0xbc,
		0xf7, 0x89, 0x7f, 0xb9, 0x7a, 0x15, 0x55, 0xc7,
		0x9e, 0xb3, 0xfc, 0x6f, 0x75, 0x17, 0x79, 0x71,
		0x5c, 0xb4, 0x5f, 0xd5, 0x7e, 0x2d, 0x59, 0x67,
		0xa5, 0xe3, 0x8b, 0x16, 0xd8, 0xcd, 0x33, 0x22,
		0xd8, 0x9d, 0x1b, 0xf7, 0xd2, 0x53, 0x1c, 0x47,
		0xee, 0x2b, 0x3e, 0xb2, 0xcd, 0xd4, 0xe7, 0xa2,
		0xeb, 0xce, 0xa8, 0xe3, 0x9d, 0x6d, 0x40, 0x69,
		0xd7, 0xfc, 0xb4, 0x67, 0x35, 0xed, 0xb1, 0x4a,
		0x17, 0x90, 0xfc, 0x6f, 0xb9, 0x96, 0x34, 0x6b,
		0x07, 0x76, 0xab, 0x83, 0x45, 0x8c, 0x80, 0x04,
		0xe2, 0xc0, 0x82, 0xb5, 0xd4, 0xb2, 0xe0, 0xa6,
	},
	{ /* PSM activation and license signatures */
		0x67, 0x19, 0xe4, 0x73, 0x01, 0x1b, 0xf7, 0x84,
		0x6e, 0x60, 0x92, 0x75, 0x41, 0xcc, 0x80, 0xd3,
		0x3c, 0x92, 0x1f, 0x2f, 0x34, 0xd9, 0xcd, 0x32,
		0xf4, 0x81, 0x38, 0x92, 0xda, 0x49, 0x9d, 0x1f,
		0xa5, 0x8c, 0x87, 0x7c, 0xe6, 0x84, 0x87, 0xa1,
		0x72, 0x5e, 0x15, 0x61, 0x60, 0xec, 0xb0, 0x24,
		0x19, 0x40, 0x3a, 0x74, 0xc0, 0x1e, 0x49, 0x86,
		0x32, 0x66, 0x41, 0x43, 0x18, 0x40, 0x5e, 0xcf,
		0x27, 0xab, 0xff, 0x26, 0xe2, 0x82, 0x47, 0xe7,
		0x34, 0xbf, 0x87, 0x18, 0x58, 0xfd, 0x8c, 0xaf,
		0x3c, 0xd0, 0x3a, 0xba, 0xd5, 0xf8, 0x22, 0xd6,
		0x2b, 0x0f, 0x69, 0xb7, 0x1a, 0x41, 0x3a, 0x01,
		0x06, 0x16, 0x8e, 0x52, 0x4c, 0x91, 0x1b, 0x0d,
		0xf9, 0x1a, 0x41, 0x9a, 0xb5, 0xa0, 0xa2, 0x96,
		0xb3, 0x88, 0x49, 0xa0, 0xf1, 0x90, 0x5c, 0x0a,
		0x3b, 0x48, 0x2e, 0x0e, 0xc5, 0x4f, 0x12, 0xa9,
		0x9b, 0x42, 0x5a, 0x58, 0xbf, 0x3c, 0x2e, 0xbb,
		0x34, 0x2b, 0x97, 0xc5, 0x97, 0x41, 0x29, 0x26,
		0x9a, 0xf1, 0x66, 0xf4, 0x1e, 0x27, 0xa3, 0x07,
		0xfb, 0x2b, 0xc3, 0x17, 0xd0, 0xcf, 0x06, 0x4b,
		0x01, 0x56, 0xb1, 0xf6, 0xb4, 0x74, 0xac, 0x76,
		0xb8, 0x1d, 0xce, 0x9e, 0xe6, 0x1d, 0x89, 0x37,
		0xf1, 0xb1, 0x8d, 0xa6, 0x58, 0xb1, 0x88, 0x01,
		0xa7, 0x57, 0xac, 0x14, 0xbb, 0x4a, 0xf8, 0x93,
		0x65, 0x10, 0x84, 0x19, 0xcf, 0x37, 0xaa, 0x28,
		0x02, 0x0c, 0xf0, 0x87, 0x2f, 0xcc, 0xf1, 0xa2,
		0xa6, 0xf5, 0x9a, 0x5c, 0xf6, 0xd5, 0x62, 0x14,
		0x59, 0x59, 0x82, 0xb3, 0xe0, 0xcd, 0xbc, 0x5a,
		0xab, 0xce, 0xf7, 0x43, 0x1f, 0xdf, 0x65, 0x74,
		0x58, 0x36, 0xec, 0x84, 0xb6, 0xcd, 0xf6, 0xfa,
		0xb9, 0x5d, 0x93, 0xef, 0x98, 0x76, 0x60, 0x94,
		0x87, 0xd1, 0x19, 0x1c, 0xb2, 0xa7, 0x0c, 0xf3,
	},
};

/* Base points of the curves the ECDSA signatures use, after the first one. */
static const unsigned char gNpDrmEcdsaBasePoints[3][40] = {
	{
		0x62, 0x27, 0xb0, 0x0a, 0x02, 0x85, 0x6f, 0xb0, 0x41, 0x08,
		0x87, 0x67, 0x19, 0xe0, 0xa0, 0x18, 0x32, 0x91, 0xee, 0xb9,
		0x6e, 0x73, 0x6a, 0xbf, 0x81, 0xf7, 0x0e, 0xe9, 0x16, 0x1b,
		0x0d, 0xde, 0xb0, 0x26, 0x76, 0x1a, 0xff, 0x7b, 0xc8, 0x5b,
	},
	{
		0x1f, 0x07, 0x2b, 0xcc, 0xc1, 0x62, 0xf2, 0xcf, 0xae, 0xa0,
		0xe7, 0xf4, 0xcd, 0xfd, 0x9c, 0xae, 0xc6, 0xc4, 0x55, 0x21,
		0x53, 0x01, 0xf4, 0xe3, 0x70, 0xc3, 0xed, 0xe2, 0xd4, 0xf5,
		0xdb, 0xc3, 0xa7, 0xde, 0x8c, 0xaa, 0xe8, 0xad, 0x5b, 0x7d,
	},
	{
		0x01, 0x21, 0xea, 0x6e, 0xcd, 0xb2, 0x3a, 0x3e, 0x23, 0x75,
		0x67, 0x1c, 0x53, 0x62, 0xe8, 0xe2, 0x8b, 0x1e, 0x78, 0x3b,
		0x1a, 0x27, 0x32, 0x15, 0x8b, 0x8c, 0xed, 0x98, 0x46, 0x6c,
		0x18, 0xa3, 0xac, 0x3b, 0x11, 0x06, 0xaf, 0xb4, 0xec, 0x3b,
	},
};

/* AES key the RIF file names are encrypted with. */
static const unsigned char gNpDrmRifNameKey[16] = {
	0x19, 0xdd, 0x4f, 0xb9, 0x89, 0x48, 0x2b, 0xd4,
	0xcb, 0x9e, 0xc9, 0xc7, 0x9a, 0x2e, 0xfb, 0xd0,
};

/* Key and outputs of the PSM activation check (SceSblGcAuthMgrPsmactForDriver seed). */
static const unsigned char gPsmDrmHmacKey[32] = {
	0xb9, 0x4a, 0x8c, 0x6a, 0x28, 0xd5, 0x82, 0x61,
	0x10, 0xe8, 0x70, 0x8f, 0x6f, 0x4a, 0xc4, 0xe6,
	0x18, 0x60, 0x7c, 0x73, 0xe4, 0xfa, 0x0c, 0x59,
	0x13, 0x25, 0x49, 0x79, 0x48, 0x6e, 0x38, 0xd6,
};

static unsigned char gPsmDrmActivationChecksum[32];

SceUID gNpDrmMutex = -1;
SceUID gPsmDrmMutex = -1;
struct NpDrmActivationKeyMaterial gNpDrmActivationKeyMaterial;
int gNpDrmIsDevelopment;
struct NpDrmActivationCache gNpDrmActivationCache;
union PsmDrmWorkingData gPsmDrmWorkingData;
SceKernelOpenPsId gNpDrmOpenPsId;
int gNpDrmActivationValid;
int gPsmDrmActivationValid;
SceUInt64 gNpDrmAccountId;
int gNpDrmDebugDrmLooseBind;
int gNpDrmDebugUpgradable;
struct PsmDrmAesMaterial gPsmDrmAesMaterial;
struct PsmDrmActivationState gPsmDrmActivationState;

int NpDrmLoadSettings(void);
int NpDrmInitializeActivation(void);
int PsmDrmInitializeActivation(void);
int NpDrmCreateMutex(void);
int PsmDrmCreateMutex(void);
int NpDrmDeleteMutex(void) __attribute__((noinline));
__attribute__((noinline)) void *NpDrmClearMemory(void *buffer, SceSize size);
__attribute__((noinline)) int NpDrmGetAccountId(SceUInt64 *accountId);
__attribute__((noinline)) int NpDrmGetDebugSkuFlag(unsigned int *skuFlag);
int NpDrmEbootSigGenerate(const char *ebootPath, const void *sha256,
	int type, void *signature, unsigned int swVersion, void *buffer,
	unsigned int bufferSize);
int NpDrmVerifyActivationSignature(const SceNpDrmActivationData *data);
int NpDrmVerifyRifSignature(const SceNpDrmLicense *license, int vita);
int PsmDrmLoadActivationFile(void);
__attribute__((noinline)) int NpDrmAesDecrypt(void *buffer, SceSize size,
	const void *key, void **end);
__attribute__((noinline)) int NpDrmAesEncrypt(void *buffer, SceSize size,
	const void *key, void **end);
__attribute__((noinline)) int NpDrmIsLooseAccountBind(void);
__attribute__((noinline)) int NpDrmUpdateAccountId(SceUInt64 accountId);
__attribute__((noinline)) int NpDrmUpdateDebugSettings(void);
__attribute__((noinline)) int NpDrmFormatRifName(char *name, SceSize size,
	SceUInt64 accountId, SceUInt64 kind);
int PsmDrmVerifyActivation(void);
__attribute__((noinline)) int NpDrmAesCbcDecrypt(void *buffer, SceSize size,
	const void *key, const void *iv, void **end);
int NpDrmRsaVerify2048(const MarlinRsaData *signature,
	const MarlinRsaData *digest, const MarlinRsaExponentKey *key,
	int algorithm);

int NpDrmEbootSigGenerateMultiDisc(const char *ebootPath, const void *discInfo,
	void *signature, unsigned int swVersion, void *buffer,
	unsigned int bufferSize);
int NpDrmEbootSigConvert(const char *ebootPath, const void *signature,
	void *newSignature, void *buffer, unsigned int bufferSize);
int NpDrmEbootSigVerify(const char *ebootPath, const void *signature,
	unsigned int signatureSize, void *buffer, unsigned int bufferSize);
int NpDrmPspEbootSigGenerate(const char *ebootPath, const void *sha256,
	void *signature, void *buffer, unsigned int bufferSize);

static int SceNpDrmInit(void *arg)
{
	NpDrmLoadSettings();
	NpDrmInitializeActivation();
	PsmDrmInitializeActivation();
	return 1;
}

int module_start(SceSize arglen, const void *argp)
{
	int ret;

	if (NpDrmCreateMutex() < 0)
		return 2;
	ret = PsmDrmCreateMutex();
	if (ret < 0) {
		NpDrmDeleteMutex();
		return ret;
	}
	sceKernelScheduleWorkTask(0x10023, "SceNpDrmInit", SceNpDrmInit, NULL);
	return 0;
}

int NpDrmRemoveActivationFile(SceUInt64 *accountId)
{
	int permission;
	SceUID fd;
	int ret;

	if (accountId) {
		gNpDrmActivationValid = 0;
		*accountId = 0;
	} else {
		gNpDrmActivationValid = 0;
	}
	permission = sceKernelSetThreadAccessLevel(0x80);
	fd = sceIoOpen("tm0:/npdrm/act.dat", 0x04000003, 6);
	if (fd >= 0) {
		if (accountId && sceIoPread(fd, &gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data), 0) ==
			sizeof(gNpDrmActivationCache.data))
			*accountId = gNpDrmActivationCache.data.account_id;
		NpDrmClearMemory(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache));
		sceIoPwrite(fd, &gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data), 0);
		sceIoClose(fd);
	}
	ret = sceIoRemove("tm0:/npdrm/act.dat");
	sceIoSync("tm0:", 0);
	if (ret == (int)0x80010002)
		ret = 0;
	NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	sceKernelSetThreadAccessLevel(permission);
	return ret;
}

int NpDrmGetRifKey(const SceNpDrmLicense *license, void *klicense,
	int *flags, unsigned int *skuFlags, SceUInt64 *startTime,
	SceUInt64 *expirationTime, int vita)
{
	SceUInt64 accountId;
	unsigned int cartSecret[8];
	SceNpDrmLicense rif;
	short licenseFlags;
	unsigned int drmType;
	unsigned int skuFlag;
	unsigned int version;
	unsigned int licenseType;
	unsigned int debugSkuFlag;
	unsigned int cartBlacklist;
	unsigned int cartIndex;
	int legacy;
	int deviceBound;
	int gameCard;
	int keyFlags;
	int ret;

	if (!klicense || !license)
		return (int)0x80870001;
	NpDrmClearMemory(klicense, 16);
	memcpy(&rif, license, sizeof(rif));
	ret = NpDrmVerifyRifSignature(&rif, vita);
	if (ret < 0)
		return ret;
	licenseFlags = __builtin_bswap16((unsigned short)rif.license_flags);
	drmType = licenseFlags & 0xff;
	gameCard = (licenseFlags & 0x400) != 0;
	deviceBound = (licenseFlags & 0x100) != 0;
	if (__builtin_bswap16((unsigned short)rif.license_type)) {
		legacy = 0;
		skuFlag = __builtin_bswap32(rif.sku_flag);
	} else {
		legacy = 1;
		skuFlag = 0;
	}
	if (gNpDrmIsDevelopment && !NpDrmGetDebugSkuFlag(&debugSkuFlag) &&
		debugSkuFlag)
		skuFlag = debugSkuFlag;
	keyFlags = (licenseFlags & 0x2000) != 0 || skuFlag == 1;
	if (__builtin_bswap64(rif.flags) & 1)
		keyFlags |= 2;
	if (deviceBound)
		keyFlags |= 0x10000;
	if (gameCard)
		keyFlags |= 0x20000;
	version = __builtin_bswap16((unsigned short)rif.version_flags);
	licenseType = __builtin_bswap16((unsigned short)rif.license_type);
	if (skuFlags)
		*skuFlags = skuFlag;
	if (startTime) {
		if (rif.start_time.tick) {
			SceUInt64 time = __builtin_bswap64(rif.start_time.tick);

			if (time && time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
			else
				time = 0x7fffffffffffffffULL;
			*startTime = time;
		} else {
			*startTime = 63113871600000000ULL;
		}
	}
	if (expirationTime) {
		SceUInt64 time = __builtin_bswap64(rif.expiration_time.tick);

		if (time) {
			if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
		} else {
			time = 0x7fffffffffffffffULL;
		}
		*expirationTime = time;
	}
	if ((licenseFlags & 0xfd) && drmType != 13)
		return (int)0x80870005;
	if (deviceBound) {
		if (legacy)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, rif.open_psid, 16))
			return (int)0x80870009;
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(rif.cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(rif.debug_upgradable);
		if (cartIndex) {
			unsigned int bit = cartIndex - 1;

			if (bit > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0,
				&cartBlacklist) < 0 || ((cartBlacklist >> bit) & 1))
				return (int)0x8087000a;
		}
	}
	if (keyFlags & 0x20000) {
		ret = SceSblGcAuthMgrDrmBBForDriver_BB70DDC0(cartSecret);
		if (ret < 0)
			return ret;
	}
	if (flags)
		*flags = keyFlags;
	if (version > 1 || (!licenseType && vita == 1))
		return (int)0x80870005;
	if (drmType == 13 && !vita)
		return (int)0x80870005;
	if (drmType == 13) {
		ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
		if (ret < 0)
			return ret;
	} else {
		ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
		if (ret < 0)
			return ret;
		if (!drmType && !gNpDrmIsDevelopment) {
			ret = (int)0x80870003;
			goto unlock;
		}
	}
	licenseFlags = __builtin_bswap16((unsigned short)rif.license_flags);
	if ((licenseFlags & 0xff) != 0 && (licenseFlags & 0xff) != 13 &&
		!(licenseFlags & 0x400)) {
		accountId = gNpDrmActivationCache.data.account_id;
		if (!gNpDrmActivationValid || !accountId)
			NpDrmGetAccountId(&accountId);
		if (rif.account_id != accountId) {
			ret = (int)0x80870008;
			goto unlock;
		}
		if (!gNpDrmActivationValid) {
			ret = (int)0x80870006;
			goto unlock;
		}
		if (__builtin_bswap16(gNpDrmActivationCache.data.act_type) <
			__builtin_bswap16((unsigned short)rif.version)) {
			ret = (int)0x80870007;
			goto unlock;
		}
	}
	if (!vita && !(keyFlags & 0x20000)) {
		unsigned int index;

		NpDrmAesDecrypt(rif.key_table, 16,
			gNpDrmActivationKeyMaterial.keyTableIndexKey, NULL);
		__builtin_memcpy(&index, &rif.key_table[12], sizeof(index));
		index = __builtin_bswap32(index) & 0x7f;
		NpDrmAesDecrypt(rif.key1, 16,
			gNpDrmActivationCache.data.primary_key_table[index], NULL);
		if (drmType)
			__builtin_memcpy(klicense, rif.key1, 16);
		else
			memset(klicense, 0xaa, 16);
		NpDrmClearMemory(&rif, sizeof(rif));
		ret = 0;
		goto unlock;
	}
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 256);
	__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table, rif.key2, 16);
	memcpy(gNpDrmActivationCache.data.secondary_key_table[6], &rif, 112);
	if (keyFlags & 0x20000) {
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[4],
			cartSecret, 16);
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[5],
			&cartSecret[4], 16);
		if (!vita) {
			__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table,
				gNpDrmActivationCache.data.secondary_key_table[11], 16);
			NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table[11],
				16);
		}
	} else if (drmType == 13) {
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[4],
			gNpDrmActivationKeyMaterial.bindingSeed, 16);
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[5],
			&gNpDrmActivationKeyMaterial.bindingSeed[16], 16);
		ret = sceSblAuthMgrDecBindData(
			gNpDrmActivationCache.data.secondary_key_table, 16,
			gNpDrmActivationCache.data.secondary_key_table[4], 144, 0);
		if (ret < 0)
			goto clear;
		goto copyKey;
	} else {
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[4],
			gNpDrmActivationCache.data.primary_key_table[0], 16);
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[5],
			gNpDrmActivationCache.data.primary_key_table[1], 16);
	}
	ret = sceSblAuthMgrDecBindData(gNpDrmActivationCache.data.secondary_key_table,
		16, gNpDrmActivationCache.data.secondary_key_table[4], 144, 0);
	if (ret < 0)
		goto clear;
	if (!drmType) {
		memset(klicense, 0, 16);
		goto success;
	}
copyKey:
	__builtin_memcpy(klicense, gNpDrmActivationCache.data.secondary_key_table, 16);
success:
	NpDrmClearMemory(&rif, sizeof(rif));
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 256);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return 0;
clear:
	NpDrmClearMemory(&rif, sizeof(rif));
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 256);
unlock:
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return ret;
}

int NpDrmLoadActivationFile(void)
{
	SceUInt64 accountId;
	SceIoStat stat;
	int permission;
	int statPermission;
	SceUID fd;
	int ret;
	int size;

	gNpDrmActivationValid = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	ret = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (ret) {
		ret = (int)0x80870006;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	fd = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, 0);
	if (fd < 0) {
		ret = fd;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	size = sceIoRead(fd, &gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceIoClose(fd);
	if (size != sizeof(gNpDrmActivationCache.data)) {
		sceKernelSetThreadAccessLevel(permission);
		ret = size < 0 ? size : (int)0x80870002;
clear:
		NpDrmClearMemory(&gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data));
		goto out;
	}
	sceKernelSetThreadAccessLevel(permission);
	ret = NpDrmVerifyActivationSignature(&gNpDrmActivationCache.data);
	if (ret < 0) {
		NpDrmClearMemory(&gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data));
		goto out;
	}
	NpDrmGetAccountId(&accountId);
	if (gNpDrmActivationCache.data.account_id == accountId ||
		NpDrmIsLooseAccountBind()) {
		if (__builtin_bswap32(gNpDrmActivationCache.data.version) <= 1 ||
			!memcmp(&gNpDrmOpenPsId,
				gNpDrmActivationCache.data.openpsid, 16)) {
			ret = 0;
			NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table,
				sizeof(gNpDrmActivationCache) -
				offsetof(struct NpDrmActivationCache, data.secondary_key_table));
			NpDrmAesDecrypt(gNpDrmActivationCache.data.primary_key_table,
				sizeof(gNpDrmActivationCache.data.primary_key_table),
				gNpDrmActivationKeyMaterial.activationKey, NULL);
			gNpDrmActivationValid = 1;
			goto out;
		}
		ret = (int)0x80870009;
	} else {
		ret = (int)0x80870008;
	}
	goto clear;
out:
	return ret;
}

int PsmDrmLoadActivationFile(void)
{
	SceIoStat stat;
	SceUInt64 tick;
	int permission;
	int statPermission;
	SceUID fd;
	int ret;
	int size;

	gPsmDrmActivationValid = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	ret = sceIoGetstat("tm0:/psmdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (ret) {
		ret = (int)0x80870006;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	fd = sceIoOpen("tm0:/psmdrm/act.dat", 0x04000001, 0);
	if (fd < 0) {
		ret = fd;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	size = sceIoRead(fd, &gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	sceIoClose(fd);
	if (size != sizeof(gPsmDrmWorkingData)) {
		sceKernelSetThreadAccessLevel(permission);
		ret = size < 0 ? size : (int)0x80870002;
clear:
		NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
		return ret;
	}
	sceKernelSetThreadAccessLevel(permission);
	ret = PsmDrmVerifyActivation();
	if (ret < 0) {
		NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
		return ret;
	}
	__builtin_memcpy(&gPsmDrmActivationState.binding.key,
		&gPsmDrmWorkingData.activation.keyset, 16);
	__builtin_memcpy(&gPsmDrmActivationState.binding.iv,
		gPsmDrmWorkingData.activation.keyset.hmac_key + 16, 16);
	NpDrmAesCbcDecrypt(&gPsmDrmActivationState.binding,
		sizeof(gPsmDrmActivationState.binding), gPsmDrmAesMaterial.key,
		gPsmDrmAesMaterial.iv, NULL);
	gPsmDrmActivationState.version =
		__builtin_bswap32(gPsmDrmWorkingData.activation.version);
	gPsmDrmActivationState.flags =
		__builtin_bswap32(gPsmDrmWorkingData.activation.flags);
	gPsmDrmActivationState.accountId =
		__builtin_bswap64(gPsmDrmWorkingData.activation.accountId);
	tick = __builtin_bswap64(gPsmDrmWorkingData.activation.startTime.tick);
	if (!tick)
		tick = 0x7fffffffffffffffULL;
	else if (tick != 0x7fffffffffffffffULL)
		tick = tick * 1000 + 62135596800000000ULL;
	gPsmDrmActivationState.startTime.tick = tick;
	tick = __builtin_bswap64(gPsmDrmWorkingData.activation.expirationTime.tick);
	if (!tick)
		tick = 0x7fffffffffffffffULL;
	else if (tick != 0x7fffffffffffffffULL)
		tick = tick * 1000 + 62135596800000000ULL;
	gPsmDrmActivationState.expirationTime.tick = tick;
	NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	gPsmDrmActivationValid = 1;
	return 0;
}

int NpDrmCreateMutex(void)
{
	SceUID uid = sceKernelCreateMutex("SceNpDrm", 0, 0, NULL);

	if (uid >= 0) {
		gNpDrmMutex = uid;
		return 0;
	}
	return uid;
}

int NpDrmInitializeActivation(void)
{
	SceConsoleId consoleId;
	int ret;

	ret = sceSblAimgrGetConsoleId(&consoleId);
	if (ret < 0)
		return ret;
	ret = sceSblAimgrGetOpenPsId(&gNpDrmOpenPsId);
	if (ret < 0)
		return ret;
	gNpDrmIsDevelopment = SceSblAIMgrForDriver_274663A0() || SceSblAIMgrForDriver_F4B98F66();
	ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (ret < 0)
		return ret;
	memcpy(&gNpDrmActivationKeyMaterial, &gNpDrmEncryptedActivationKeyMaterial,
		sizeof(gNpDrmActivationKeyMaterial));
	ret = sceSblAuthMgrGetEKc(&gNpDrmActivationKeyMaterial,
		sizeof(gNpDrmActivationKeyMaterial), 0);
	NpDrmAesEncrypt(&gNpDrmActivationKeyMaterial,
		sizeof(gNpDrmActivationKeyMaterial.activationKey), &consoleId, NULL);
	NpDrmClearMemory(&consoleId, sizeof(consoleId));
	if (ret < 0) {
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return ret;
	}
	if (NpDrmLoadActivationFile() < 0 || !gNpDrmActivationValid)
		NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return 0;
}

int NpDrmDeleteMutex(void)
{
	if (gNpDrmMutex >= 0) {
		sceKernelDeleteMutex(gNpDrmMutex);
		gNpDrmMutex = -1;
	}
	return 0;
}

int SceNpDrmForDriver_116FC0D6(SceUInt64 accountId)
{
	int ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);

	if (ret >= 0) {
		int result = NpDrmUpdateAccountId(accountId);

		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return result;
	}
	return ret;
}

int SceNpDrmForDriver_A91C7443(void)
{
	int ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);

	if (ret >= 0) {
		int result = NpDrmUpdateDebugSettings();

		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return result;
	}
	return ret;
}

int SceNpDrmForDriver_17573133(char *name,
	const SceNpDrmLicense *license, unsigned int isFixed)
{
	SceUInt64 accountId;
	unsigned int value;
	unsigned int licenseType;
	unsigned int drmType;
	unsigned int cartIndex;
	int freeLicense;
	int deviceBound;
	int gameCard;

	if (!license || !name || isFixed > 1)
		return (int)0x80870001;
	licenseType = __builtin_bswap16((unsigned short)license->license_type);
	drmType = __builtin_bswap16((unsigned short)license->license_flags);
	freeLicense = licenseType == 0;
	deviceBound = drmType & 0x100;
	gameCard = (drmType & 0x400) != 0;
	if (gNpDrmIsDevelopment) {
		NpDrmGetDebugSkuFlag(&value);
		licenseType = __builtin_bswap16((unsigned short)license->license_type);
	}
	accountId = license->account_id;
	if ((drmType & 0xfd) && (unsigned char)drmType != 13)
		return (int)0x80870005;
	if (deviceBound) {
		if (freeLicense)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16))
			return (int)0x80870009;
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value) < 0 ||
				((value >> cartIndex) & 1))
				return (int)0x8087000a;
		}
	}
	if (isFixed) {
		if (!licenseType)
			return (int)0x80870005;
		if (gameCard)
			return NpDrmFormatRifName(name, 48, 1, 1);
		if ((unsigned char)drmType == 13 || !accountId)
			return NpDrmFormatRifName(name, 48, 0, 1);
		return NpDrmFormatRifName(name, 48, accountId, 0);
	}
	snprintf(name, 47, "%s.rif", license->content_id);
	return 0;
}

int sceNpDrmGetRifName(char *name, SceUInt64 accountId)
{
	if (!accountId) {
		NpDrmGetAccountId(&accountId);
		if (gNpDrmActivationValid && NpDrmIsLooseAccountBind())
			accountId = gNpDrmActivationCache.data.account_id;
		if (!accountId)
			return (int)0x8087000c;
	}
	return NpDrmFormatRifName(name, 48, accountId, 0);
}

int sceNpDrmGetFixedRifName(char *name, SceUInt64 accountId)
{
	return NpDrmFormatRifName(name, 48, accountId, 1);
}

int SceNpDrmForDriver_742EBAF4(const struct NpDrmActivationCache *activation,
	const void *key)
{
	SceIoStat stat;
	SceUInt64 accountId;
	int permission;
	int cleanupPermission;
	int written;
	int result;
	int verifyResult;

	if (!activation)
		return (int)0x80870001;
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	gNpDrmActivationValid = 0;
	memcpy(&gNpDrmActivationCache, activation,
		sizeof(gNpDrmActivationCache));
	if (key)
		NpDrmAesCbcDecrypt(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache), key, NULL, NULL);
	verifyResult = NpDrmVerifyActivationSignature(&gNpDrmActivationCache.data);
	if (verifyResult < 0) {
		NpDrmClearMemory(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache));
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return verifyResult;
	}
	NpDrmGetAccountId(&accountId);
	if (gNpDrmActivationCache.data.account_id != accountId &&
		!NpDrmIsLooseAccountBind()) {
		result = (int)0x80870008;
		goto clear;
	}
	if (__builtin_bswap32(gNpDrmActivationCache.data.version) > 1 &&
		memcmp(&gNpDrmOpenPsId, gNpDrmActivationCache.data.openpsid, 16)) {
		result = (int)0x80870009;
		goto clear;
	}
	permission = sceKernelSetThreadAccessLevel(0x80);
	if (sceIoGetstat("tm0:/npdrm", &stat) < 0)
		sceIoMkdir("tm0:/npdrm", 6);
	result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000602, 6);
	if (result < 0) {
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	written = sceIoWrite(result, &gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceIoClose(result);
	if (written < (int)sizeof(gNpDrmActivationCache.data)) {
		gNpDrmActivationValid = 0;
		cleanupPermission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000003, 6);
		if (result >= 0) {
			NpDrmClearMemory(&gNpDrmActivationCache,
				sizeof(gNpDrmActivationCache));
			sceIoPwrite(result, &gNpDrmActivationCache.data,
				sizeof(gNpDrmActivationCache.data), 0);
			sceIoClose(result);
		}
		sceIoRemove("tm0:/npdrm/act.dat");
		result = written < 0 ? written : (int)0x80870002;
		sceIoSync("tm0:", 0);
		NpDrmClearMemory(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache));
		sceKernelSetThreadAccessLevel(cleanupPermission);
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	sceKernelSetThreadAccessLevel(permission);
	result = NpDrmLoadActivationFile();
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;

clear:
	NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_8B85A509(SceUInt64 *accountId)
{
	SceIoStat stat;
	int permission;
	int result;
	int ret;

	ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (ret < 0)
		return ret;
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(permission);
	if (result) {
		if (accountId)
			*accountId = 0;
		if (gNpDrmActivationValid) {
			NpDrmClearMemory(&gNpDrmActivationCache,
				sizeof(gNpDrmActivationCache));
			gNpDrmActivationValid = 0;
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return (int)0x80870006;
	}
	result = NpDrmRemoveActivationFile(accountId);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_077926F5(void)
{
	SceIoStat stat;
	SceUInt64 accountId;
	int permission;
	int statPermission;
	int readSize;
	int result;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	gNpDrmActivationValid = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (result) {
		result = (int)0x80870006;
		goto restore;
	}
	result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, 0);
	if (result < 0)
		goto restore;
	readSize = sceIoRead(result, &gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceIoClose(result);
	if (readSize != sizeof(gNpDrmActivationCache.data)) {
		result = readSize < 0 ? readSize : (int)0x80870002;
		goto restore;
	}
	sceKernelSetThreadAccessLevel(permission);
	result = NpDrmVerifyActivationSignature(&gNpDrmActivationCache.data);
	if (result < 0)
		goto clear;
	NpDrmGetAccountId(&accountId);
	if (gNpDrmActivationCache.data.account_id != accountId &&
		!NpDrmIsLooseAccountBind()) {
		result = (int)0x80870008;
		goto clear;
	}
	if (__builtin_bswap32(gNpDrmActivationCache.data.version) > 1 &&
		memcmp(&gNpDrmOpenPsId, gNpDrmActivationCache.data.openpsid, 16)) {
		result = (int)0x80870009;
		goto clear;
	}
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 0x7c0);
	NpDrmAesDecrypt(gNpDrmActivationCache.data.primary_key_table, 0x800,
		gNpDrmActivationKeyMaterial.activationKey, NULL);
	gNpDrmActivationValid = 1;
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return 0;

restore:
	sceKernelSetThreadAccessLevel(permission);
clear:
	NpDrmClearMemory(&gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_D91C3BCE(SceNpDrmActivationData *activation)
{
	SceIoStat stat;
	SceUInt64 accountId;
	int permission;
	int statPermission;
	int readSize;
	int result;

	if (!activation)
		return (int)0x80870001;
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (result) {
		result = (int)0x80870006;
		goto restore;
	}
	result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, 0);
	if (result < 0)
		goto restore;
	readSize = sceIoRead(result, activation, sizeof(*activation));
	sceIoClose(result);
	if (readSize != sizeof(*activation)) {
		result = readSize < 0 ? readSize : (int)0x80870002;
		goto restore;
	}
	sceKernelSetThreadAccessLevel(permission);
	result = NpDrmVerifyActivationSignature(activation);
	if (result < 0)
		goto clear;
	NpDrmGetAccountId(&accountId);
	if (activation->account_id != accountId && !NpDrmIsLooseAccountBind()) {
		result = (int)0x80870008;
		goto clear;
	}
	if (__builtin_bswap32(activation->version) > 1 &&
		memcmp(&gNpDrmOpenPsId, activation->openpsid, 16)) {
		result = (int)0x80870009;
		goto clear;
	}
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return 0;

restore:
	sceKernelSetThreadAccessLevel(permission);
clear:
	NpDrmClearMemory(activation, sizeof(*activation));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_FF63672D(SceNpDrmLicense *license)
{
	unsigned char klicense[16];
	unsigned int value;
	unsigned int licenseType;
	unsigned int drmType;
	unsigned int cartIndex;
	int freeLicense;
	int deviceBound;
	int gameCard;
	int result;

	if (!license)
		return (int)0x80870001;
	licenseType = __builtin_bswap16((unsigned short)license->license_type);
	drmType = __builtin_bswap16((unsigned short)license->license_flags);
	freeLicense = !licenseType;
	deviceBound = drmType & 0x100;
	gameCard = (drmType & 0x400) != 0;
	if (gNpDrmIsDevelopment) {
		NpDrmGetDebugSkuFlag(&value);
		licenseType = __builtin_bswap16((unsigned short)license->license_type);
	}
	if ((drmType & 0xfd) && (unsigned char)drmType != 13)
		return (int)0x80870005;
	if (deviceBound) {
		if (freeLicense)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16))
			return (int)0x80870009;
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value) < 0 ||
				((value >> cartIndex) & 1))
				return (int)0x8087000a;
		}
	}
	if ((unsigned char)drmType == 13 || !(unsigned char)drmType)
		return (int)0x80870005;
	if (!licenseType || !gameCard)
		return 0;
	result = NpDrmGetRifKey(license, klicense, NULL, NULL, NULL, NULL, 1);
	if (result < 0)
		return result;
	NpDrmAesDecrypt(license->key_table, 16, klicense, NULL);
	NpDrmClearMemory(klicense, sizeof(klicense));
	__builtin_memcpy(license->key1, license->key_table, 16);
	NpDrmClearMemory(&license->flags, 0x168);
	return 0;
}

int SceNpDrmForDriver_DACB71F4(const SceNpDrmLicense *license, void *klicense,
	int *flags, SceUInt64 *startTime, SceUInt64 *expirationTime)
{
	return NpDrmGetRifKey(license, klicense, flags, NULL, startTime,
		expirationTime, 0);
}

int SceNpDrmForDriver_C070FE89(SceNpDrmLicense *license,
	unsigned int provisional)
{
	union {
		SceRtcTick currentTime;
		unsigned int skuFlag;
	} value;
	SceInt64 flags;
	SceUInt64 oldProvisional;
	SceUInt64 startTime;
	SceUInt64 expirationTime;
	unsigned int licenseType;
	unsigned int drmType;
	unsigned int cartIndex;
	int result;

	if (!license || provisional > 1)
		return (int)0x80870001;
	flags = license->flags;
	oldProvisional = __builtin_bswap32((unsigned int)(flags >> 32)) & 1;
	if (!provisional) {
		license->flags = flags & ~0x0100000000000000LL;
		result = NpDrmVerifyRifSignature(license, 1);
		if (result < 0) {
			if (oldProvisional) {
				flags = license->flags;
				flags |= 0x0100000000000000LL;
				license->flags = flags;
			}
		} else {
			result = 0;
		}
		return result;
	}
	drmType = __builtin_bswap16((unsigned short)license->license_flags);
	licenseType = __builtin_bswap16((unsigned short)license->license_type);
	if (gNpDrmIsDevelopment)
		NpDrmGetDebugSkuFlag(&value.skuFlag);
	if (license->start_time.tick) {
		startTime = __builtin_bswap64(license->start_time.tick);
		if (startTime && startTime != 0x7fffffffffffffffULL)
			startTime = startTime * 1000 + 62135596800000000ULL;
		else
			startTime = 0x7fffffffffffffffULL;
	} else {
		startTime = 63113871600000000ULL;
	}
	expirationTime = __builtin_bswap64(license->expiration_time.tick);
	if (expirationTime) {
		if (expirationTime != 0x7fffffffffffffffULL)
			expirationTime = expirationTime * 1000 + 62135596800000000ULL;
	} else {
		expirationTime = 0x7fffffffffffffffULL;
	}
	if ((drmType & 0xfd) && (unsigned char)drmType != 13)
		return (int)0x80870005;
	if (drmType & 0x100) {
		if (!licenseType)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16))
			return (int)0x80870009;
	}
	if (drmType & 0x400) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value.skuFlag) < 0 ||
				((value.skuFlag >> cartIndex) & 1))
				return (int)0x8087000a;
		}
	}
	if (expirationTime == 0x7fffffffffffffffULL &&
		(SceRtcForDriver_401C0954(&value.currentTime) ||
		value.currentTime.tick < startTime)) {
		license->flags |= 0x0100000000000000ULL;
		result = NpDrmVerifyRifSignature(license, 1);
		if (result < 0) {
			if (!oldProvisional)
				license->flags &= ~0x0100000000000000ULL;
			return result;
		}
	}
	return 0;
}

int SceNpDrmForDriver_723322B5(const SceNpDrmLicense *license, void *klicense,
	int *flags, unsigned int *skuFlags, SceUInt64 *startTime, SceUInt64 *expirationTime)
{
	return NpDrmGetRifKey(license, klicense, flags, skuFlags, startTime, expirationTime, 1);
}

int sceNpDrmGetRifInfo(const SceNpDrmLicense *license, unsigned int licenseSize,
	int checkSign, char *contentId, SceUInt64 *accountId, int *licenseVersion,
	int *drmType, int *flags, unsigned int *skuFlags, SceUInt64 *startTime,
	SceUInt64 *expirationTime, SceUInt64 *flags2)
{
	union {
		SceUInt64 accountId;
		unsigned int skuFlag;
	} value;
	unsigned int licenseFlags;
	unsigned int type;
	unsigned int skuFlag;
	unsigned int resultFlags;
	unsigned int cartIndex;
	SceUInt64 time;
	int shortLicense;
	int deviceBound;
	int gameCard;
	int result;

	if (!license)
		return (int)0x80870001;
	if (licenseSize <= 151) {
		result = (int)0x80870004;
		type = 0;
		goto finishValidation;
	}
	if (checkSign) {
		result = NpDrmVerifyRifSignature(license,
			licenseSize >= 512 && license->license_type != 0);
		if (result < 0) {
			type = 0;
			goto finishValidation;
		}
	}
	licenseFlags = __builtin_bswap16((unsigned short)license->license_flags);
	type = (unsigned char)licenseFlags;
	deviceBound = (licenseFlags & 0x100) != 0;
	gameCard = (licenseFlags & 0x400) != 0;
	shortLicense = !__builtin_bswap16((unsigned short)license->license_type) ||
		licenseSize < 512;
	skuFlag = shortLicense ? 0 : __builtin_bswap32(license->sku_flag);
	if (gNpDrmIsDevelopment && !NpDrmGetDebugSkuFlag(&value.skuFlag) &&
		value.skuFlag)
		skuFlag = value.skuFlag;
	resultFlags = (licenseFlags & 0x2000) != 0 || skuFlag == 1;
	if (__builtin_bswap64(license->flags) & 1)
		resultFlags |= 2;
	if (deviceBound)
		resultFlags |= 0x10000;
	if (gameCard)
		resultFlags |= 0x20000;
	if (contentId)
		__builtin_memcpy(contentId, license->content_id, 48);
	if (licenseVersion)
		*licenseVersion = __builtin_bswap16((unsigned short)license->version);
	if (accountId)
		*accountId = license->account_id;
	if (flags)
		*flags = resultFlags;
	if (skuFlags)
		*skuFlags = skuFlag;
	if (startTime) {
		if (license->start_time.tick) {
			time = __builtin_bswap64(license->start_time.tick);
			if (time) {
				if (time != 0x7fffffffffffffffULL)
					time = time * 1000 + 62135596800000000ULL;
			} else {
				time = 0x7fffffffffffffffULL;
			}
		} else {
			time = 63113871600000000ULL;
		}
		*startTime = time;
	}
	if (expirationTime) {
		time = __builtin_bswap64(license->expiration_time.tick);
		if (time) {
			if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
		} else {
			time = 0x7fffffffffffffffULL;
		}
		*expirationTime = time;
	}
	if (flags2) {
		if (!shortLicense || ((licenseFlags & 0x1000) &&
			license->expiration_time.tick && licenseSize > 159))
			*flags2 = __builtin_bswap64(license->flags) & ~1ULL;
		else
			*flags2 = 0;
	}
	result = (int)0x80870005;
	if ((licenseFlags & 0xfd) && type != 13)
		goto finishValidation;
	if (deviceBound) {
		if (shortLicense)
			goto finishValidation;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16)) {
			result = (int)0x80870009;
			goto finishValidation;
		}
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0) {
			result = (int)0x8087000a;
			goto finishValidation;
		}
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				goto finishValidation;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value.skuFlag) < 0 ||
				((value.skuFlag >> cartIndex) & 1)) {
				result = (int)0x8087000a;
				goto finishValidation;
			}
		}
	}
	result = 0;
finishValidation:
	if (drmType)
		*drmType = type;
	if (result)
		return result;
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	result = 0;
	if (!type && !gNpDrmIsDevelopment) {
		result = (int)0x80870003;
		goto unlock;
	}
	licenseFlags = __builtin_bswap16((unsigned short)license->license_flags);
	if ((unsigned char)licenseFlags && (unsigned char)licenseFlags != 13 &&
		!(licenseFlags & 0x400)) {
		value.accountId = gNpDrmActivationCache.data.account_id;
		if (!gNpDrmActivationValid || !value.accountId)
			NpDrmGetAccountId(&value.accountId);
		if (license->account_id != value.accountId)
			result = (int)0x80870008;
		else if (!gNpDrmActivationValid)
			result = (int)0x80870006;
		else if (__builtin_bswap16((unsigned short)gNpDrmActivationCache.data.act_type) <
			__builtin_bswap16((unsigned short)license->version))
			result = (int)0x80870007;
	}
unlock:
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int NpDrmCheckDrmResetForDriver(const void *in_addr, unsigned int size,
	SceUInt64 account_id, int *pReset)
{
	const unsigned char *input = in_addr;
	SceIoStat stat;
	SceUInt64 activationAccountId;
	SceUInt64 currentAccountId;
	unsigned int activationType;
	unsigned int mode;
	int permission;
	int readPermission;
	int result;
	int readSize;
	SceUID fd;

	if (size - 2 > 62 || !input)
		return (int)0x80870001;
	mode = input[1];
	if (pReset)
		*pReset = 0;
	if (!input[0] || sceKernelLockMutex(gNpDrmMutex, 1, NULL) < 0)
		return 0;
	if (!gNpDrmActivationValid) {
		if (NpDrmLoadActivationFile() < 0)
			gNpDrmActivationValid = 0;
		else if (gNpDrmActivationValid)
			goto activationLoaded;
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
		sceKernelSetThreadAccessLevel(permission);
		if (!result) {
			readPermission = sceKernelSetThreadAccessLevel(0x80);
			permission = sceKernelSetThreadAccessLevel(0x80);
			result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
			sceKernelSetThreadAccessLevel(permission);
			if (!result) {
				fd = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, result);
				if (fd >= 0) {
					readSize = sceIoRead(fd, &gNpDrmActivationCache,
						sizeof(gNpDrmActivationCache));
					sceIoClose(fd);
					if (readSize == sizeof(gNpDrmActivationCache)) {
						sceKernelSetThreadAccessLevel(readPermission);
						goto activationLoaded;
					}
				}
			}
			sceKernelSetThreadAccessLevel(readPermission);
		}
		NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	}
activationLoaded:
	activationType = __builtin_bswap16((unsigned short)gNpDrmActivationCache.data.act_type);
	activationAccountId = gNpDrmActivationCache.data.account_id;
	if (!gNpDrmActivationValid) {
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
		sceKernelSetThreadAccessLevel(permission);
		if (!result && gNpDrmActivationCache.data.account_id) {
			NpDrmGetAccountId(&currentAccountId);
			if (gNpDrmActivationCache.data.account_id != currentAccountId)
				NpDrmIsLooseAccountBind();
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return 0;
	}
	mode &= 0xc0;
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	if (mode == 0x80) {
		if (!activationType)
			return 0;
	} else if (mode == 0xc0) {
		return 0;
	}
	if (activationAccountId != account_id && NpDrmIsLooseAccountBind())
		return 0;
	if (pReset)
		*pReset = 1;
	if (sceKernelLockMutex(gNpDrmMutex, 1, NULL) < 0)
		goto retryRemoval;
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(permission);
	if (result) {
		activationAccountId = 0;
		if (gNpDrmActivationValid) {
			NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
			gNpDrmActivationValid = 0;
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		goto retryRemoval;
	}
	result = NpDrmRemoveActivationFile(&activationAccountId);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	if (result >= 0)
		return 0;
retryRemoval:
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(permission);
	if (result) {
		activationAccountId = 0;
		if (gNpDrmActivationValid) {
			NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
			gNpDrmActivationValid = 0;
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return (int)0x80870006;
	}
	result = NpDrmRemoveActivationFile(&activationAccountId);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result < 0 ? result : 0;
}

int SceNpDrmForDriver_90B1A6D3(const char *ebootPath, const void *sha256,
	void *signature, int swVersion)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigGenerate(ebootPath, sha256, 2,
			signature, swVersion, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_6D9223E1(const char *ebootPath, const void *sha256,
	void *signature, int swVersion)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigGenerate(ebootPath, sha256, 3,
			signature, swVersion, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_39A7A666(const char *ebootPath, const void *discInfo,
	void *signature, int swVersion)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigGenerateMultiDisc(ebootPath, discInfo,
			signature, swVersion, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_A29B75F9(const char *ebootPath, const void *signature,
	void *newSignature)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigConvert(ebootPath, signature,
			newSignature, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_7A319692(const char *ebootPath, const void *signature)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigVerify(ebootPath, signature, 512,
			gNpDrmActivationCache.data.secondary_key_table, 0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_EF387FC4(const char *ebootPath, const void *sha256,
	void *signature)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmPspEbootSigGenerate(ebootPath, sha256,
			signature, gNpDrmActivationCache.data.secondary_key_table, 0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

/*
 * Signature checks. The activation data carries an ECDSA (P-224) signature and an
 * RSA-2048 one; a license carries a 256-byte RSA signature, and the PSP variant an
 * ECDSA signature over its first 112 bytes.
 */
struct NpDrmRsaVerifyArgs {
	const void *data;
	SceSize size;
	const void *signature;
	int keyType;
};

struct NpDrmEcdsaVerifyArgs {
	const void *digest;
	const void *signature;
	int curve;
};

int NpDrmRsaVerify(const struct NpDrmRsaVerifyArgs *args)
{
	unsigned char digest[32];
	unsigned char signature[256];
	unsigned char modulus[256];
	unsigned int exponent[64];
	MarlinRsaData digestData;
	MarlinRsaData signatureData;
	MarlinRsaExponentKey key;
	unsigned int error;
	int result;

	memset(exponent, 0, sizeof(exponent));
	exponent[0] = 65537;
	result = sceSha256Digest(args->data, args->size, digest);
	if (result < 0)
		return result;
	memcpy(signature, args->signature, sizeof(signature));
	digestData.data = digest;
	digestData.size = sizeof(digest);
	signatureData.data = signature;
	signatureData.size = sizeof(signature);
	switch (args->keyType) {
	case 2:
		error = 0x80870004;
		memcpy(modulus, gNpDrmRsaModuli[1], sizeof(modulus));
		break;
	case 4:
	case 5:
		error = 0x808f0104;
		memcpy(modulus, gNpDrmRsaModuli[3], sizeof(modulus));
		break;
	case 0:
	case 1:
	case 3:
		error = 0x80870004;
		memcpy(modulus, gNpDrmRsaModuli[args->keyType == 3 ? 2 : 0],
			sizeof(modulus));
		break;
	default:
		return (int)0x80870001;
	}
	key.modulus = (unsigned int *)modulus;
	key.exponent = exponent;
	result = (NpDrmRsaVerify2048(&signatureData, &digestData, &key, 11) >> 31) &
		error;
	return result;
}

int NpDrmEcdsaVerify(const struct NpDrmEcdsaVerifyArgs *args)
{
	unsigned char signature[40];
	unsigned char basePoint[40];
	struct MsifEcdsaSignature ecdsaSignature;
	struct MsifEcdsaPoint publicKey;
	struct MsifEcdsaCurve curve;
	int result;

	if (args->curve < 0 || args->curve > 2)
		return (int)0x80870001;
	memcpy(signature, args->signature, sizeof(signature));
	memcpy(basePoint, gNpDrmEcdsaBasePoints[args->curve],
		sizeof(basePoint));
	ecdsaSignature.r = signature;
	ecdsaSignature.s = signature + 20;
	publicKey.x = basePoint;
	publicKey.y = basePoint + 20;
	curve.prime = gNpDrmActivationKeyMaterial.prime;
	curve.coefficientA = gNpDrmActivationKeyMaterial.coefficientA;
	curve.coefficientB = gNpDrmActivationKeyMaterial.coefficientB;
	curve.order = gNpDrmActivationKeyMaterial.order;
	curve.baseX = gNpDrmActivationKeyMaterial.baseX;
	curve.baseY = gNpDrmActivationKeyMaterial.baseY;
	result = (MsifEcdsaVerify224(&ecdsaSignature, args->digest, &publicKey,
		&curve) >> 31) & 0x80870004;
	return result;
}

int NpDrmVerifyActivationSignature(const SceNpDrmActivationData *data)
{
	union {
		struct NpDrmRsaVerifyArgs rsa;
		struct NpDrmEcdsaVerifyArgs ecdsa;
	} args;
	unsigned char digest[20];

	if (!data)
		return (int)0x80870001;
	if (sceSha1Digest(data, offsetof(SceNpDrmActivationData, ecdsa_signature),
			digest) >= 0 &&
	    (args.ecdsa.digest = digest,
	     args.ecdsa.signature = data->ecdsa_signature,
	     args.ecdsa.curve = 0,
	     (int)SceThreadmgrForDriver_E54FD746(0x2000,
						  (int (*)(void *))NpDrmEcdsaVerify,
						  &args) >= 0) &&
	    ((args.rsa.data = data,
	      args.rsa.size = offsetof(SceNpDrmActivationData, rsa_signature),
	      args.rsa.signature = data->rsa_signature,
	      args.rsa.keyType = 0,
	      (int)SceThreadmgrForDriver_E54FD746(0x2000,
						   (int (*)(void *))NpDrmRsaVerify,
						   &args) >= 0) ||
	     __builtin_bswap32(data->version) <= 1))
		return 0;
	return (int)0x80870004;
}

int NpDrmVerifyRifSignature(const SceNpDrmLicense *license, int vita)
{
	union {
		SceNpDrmLicense license;
		unsigned char bytes[0x200];
	} buffer;
	unsigned char digest[20];
	union {
		struct NpDrmRsaVerifyArgs rsa;
		struct NpDrmEcdsaVerifyArgs ecdsa;
	} args;
	unsigned int flags;
	int keyType;

	if (!license)
		return (int)0x80870001;
	flags = __builtin_bswap16((unsigned short)license->license_flags);
	if (flags & 0x100)
		vita = 1;
	if (flags & 0x400) {
		if (license->license_flags) {
			memset(&buffer, 0, sizeof(buffer));
			memcpy(&buffer, license, sizeof(buffer));
			keyType = 3;
			goto verify;
		}
	} else if (license->license_flags) {
		if (!vita) {
			if (sceSha1Digest(license, 112, digest) < 0)
				return (int)0x80870004;
			args.ecdsa.digest = digest;
			args.ecdsa.signature = license->ecdsa_signature;
			args.ecdsa.curve = 0;
			return (SceThreadmgrForDriver_E54FD746(0x2000,
					(int (*)(void *))NpDrmEcdsaVerify,
					&args) >> 31) & 0x80870004;
		}
		memset(&buffer, 0, sizeof(buffer));
		memcpy(&buffer, license, sizeof(buffer));
		keyType = 1;
		buffer.license.flags &= ~0x01000000ULL;
		goto verify;
	}
	memset(&buffer, 0, sizeof(buffer));
	memcpy(&buffer, license, sizeof(buffer));
	keyType = 2;
verify:
	args.rsa.data = &buffer;
	args.rsa.size = 256;
	args.rsa.signature = license->rsa_signature;
	args.rsa.keyType = keyType;
	if (SceThreadmgrForDriver_E54FD746(0x2000, (int (*)(void *))NpDrmRsaVerify,
					   &args) < 0)
		return __builtin_bswap16((unsigned short)license->license_type) ?
			(int)0x80870004 : (int)0x80870005;
	return 0;
}

void *NpDrmClearMemory(void *buffer, SceSize size)
{
	return memset(buffer, 0, size);
}

__attribute__((noinline)) int NpDrmAesEncryptBlocks(void *buffer, SceSize size, const void *key,
	void **end)
{
	SceAesCompactContext context;
	unsigned char *current = buffer;
	unsigned char *last = current + (size & ~0xf);

	if (end)
		*end = last;
	SceKernelUtilsForDriver_EDA97D6D(&context, 128, 128, key);
	while (current < last) {
		SceKernelUtilsForDriver_302947B6(&context, current, current);
		current += 16;
	}
	memset(&context, 0, sizeof(context));
	return 0;
}

__attribute__((noinline)) int NpDrmAesDecryptBlocks(void *buffer, SceSize size, const void *key,
	void **end)
{
	SceAesCompactContext context;
	unsigned char *current = buffer;
	unsigned char *last = current + (size & ~0xf);

	if (end)
		*end = last;
	SceKernelUtilsForDriver_72408E29(&context, 128, 128, key);
	while (current < last) {
		SceKernelUtilsForDriver_E39CD272(&context, current, current);
		current += 16;
	}
	memset(&context, 0, sizeof(context));
	return 0;
}

int NpDrmAesEncrypt(void *buffer, SceSize size, const void *key, void **end)
{
	return NpDrmAesEncryptBlocks(buffer, size, key, end);
}

int NpDrmAesDecrypt(void *buffer, SceSize size, const void *key, void **end)
{
	return NpDrmAesDecryptBlocks(buffer, size, key, end);
}

/* AES-CBC decryption workspace: the AES context followed by the chaining block. */
struct NpDrmAesCbcContext {
	SceAesCompactContext aes;
	unsigned char chainingBlock[32];
};

typedef unsigned char NpDrmAesVector
	__attribute__((vector_size(16), aligned(1), may_alias));

/* Decrypts whole 16-byte blocks in place; the 32-byte chaining block keeps two
 * ciphertext blocks so a wider block size can be handled in one pass. */
int NpDrmAesCbcDecryptWorker(void *buffer, SceSize size, const void *key,
	const void *iv, void **end)
{
	struct NpDrmAesCbcContext context __attribute__((aligned(8)));
	unsigned char zeroIv[16];
	unsigned char plainBlock[32] __attribute__((aligned(8)));
	unsigned char *current = buffer;
	unsigned char *last = current + (size & ~0xf);
	unsigned int blockBytes;
	unsigned int index;
	NpDrmAesVector plain, ciphertext;

	if (end)
		*end = last;
	if (!iv) {
		memset(zeroIv, 0, sizeof(zeroIv));
		iv = zeroIv;
	}
	if (SceKernelUtilsForDriver_72408E29(&context.aes, 128, 128, key) >= 0 &&
	    context.aes.blockWords * 4)
		memcpy(context.chainingBlock, iv, context.aes.blockWords * 4);
	while (current < last) {
		SceKernelUtilsForDriver_E39CD272(&context.aes, current, plainBlock);
		blockBytes = context.aes.blockWords * 4;
		if (blockBytes) {
			if (blockBytes > 15) {
				plain = *(NpDrmAesVector *)plainBlock ^
					*(NpDrmAesVector *)context.chainingBlock;
				ciphertext = *(const NpDrmAesVector *)current;
				*(NpDrmAesVector *)context.chainingBlock = ciphertext;
				*(NpDrmAesVector *)current = plain;
				index = (blockBytes >> 4) << 4;
				if (blockBytes >> 4 != 1) {
					plain = *(NpDrmAesVector *)(plainBlock + 16) ^
						*(NpDrmAesVector *)(context.chainingBlock + 16);
					ciphertext = *(const NpDrmAesVector *)(current + 16);
					*(NpDrmAesVector *)(context.chainingBlock + 16) = ciphertext;
					*(NpDrmAesVector *)(current + 16) = plain;
				}
				if (index != blockBytes) {
					do {
						unsigned char chain = context.chainingBlock[index];
						unsigned char decrypted = plainBlock[index];

						context.chainingBlock[index] = current[index];
						current[index++] = chain ^ decrypted;
					} while (blockBytes > index);
				}
			} else {
				for (index = 0; index < blockBytes; index++) {
					unsigned char chain = context.chainingBlock[index];
					unsigned char encrypted = current[index];

					current[index] = chain ^ plainBlock[index];
					context.chainingBlock[index] = encrypted;
				}
			}
		}
		current += 16;
	}
	memset(&context, 0, sizeof(context));
	return 0;
}

__attribute__((noinline)) int NpDrmAesCbcDecrypt(void *buffer, SceSize size,
	const void *key, const void *iv, void **end)
{
	return NpDrmAesCbcDecryptWorker(buffer, size, key, iv, end);
}

int NpDrmLoadSettings(void)
{
	SceUInt64 accountId = 0;
	int debugUpgradable;
	int looseBind;
	int result;

	if (sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", &accountId,
			       sizeof(accountId)) < 0) {
		accountId = 0;
	}
	gNpDrmAccountId = accountId;
	if (sceRegMgrGetKeyInt("/CONFIG/NP", "debug_upgradable",
			       &debugUpgradable) < 0) {
		debugUpgradable = 0;
	}
	gNpDrmDebugUpgradable = debugUpgradable;
	result = SceQafMgrForDriver_DEC6DF4E();
	gNpDrmDebugDrmLooseBind = result == 1 &&
		(result = sceRegMgrGetKeyInt("/CONFIG/NP2",
					    "debug_drm_loose_bind",
					    &looseBind)) == 0 && looseBind == 1;
	return result;
}

int NpDrmUpdateAccountId(SceUInt64 accountId)
{
	SceUInt64 stored = accountId;

	if (!accountId &&
	    sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", &stored,
			       sizeof(stored)) >= 0)
		accountId = stored;
	gNpDrmAccountId = accountId;
	return 0;
}

int NpDrmUpdateDebugSettings(void)
{
	int debugUpgradable;
	int looseBind;

	if (sceRegMgrGetKeyInt("/CONFIG/NP", "debug_upgradable",
			       &debugUpgradable) < 0)
		debugUpgradable = 0;
	gNpDrmDebugUpgradable = debugUpgradable;
	gNpDrmDebugDrmLooseBind = SceQafMgrForDriver_DEC6DF4E() == 1 &&
		sceRegMgrGetKeyInt("/CONFIG/NP2", "debug_drm_loose_bind",
				   &looseBind) == 0 && looseBind == 1;
	return 0;
}

int NpDrmGetAccountId(SceUInt64 *accountId)
{
	if (!accountId)
		return (int)0x80870001;
	*accountId = gNpDrmAccountId;
	return 0;
}

int NpDrmGetDebugSkuFlag(unsigned int *skuFlag)
{
	if (!skuFlag)
		return (int)0x80870001;
	*skuFlag = gNpDrmDebugUpgradable;
	return 0;
}

int NpDrmIsLooseAccountBind(void)
{
	return gNpDrmDebugDrmLooseBind;
}

int NpDrmFormatRifName(char *name, SceSize size, SceUInt64 accountId,
	SceUInt64 kind)
{
	union {
		struct {
			SceUInt64 kind;
			SceUInt64 accountId;
		} plain;
		unsigned char bytes[16];
	} rif;

	if (!name)
		return (int)0x80870001;
	rif.plain.kind = kind;
	rif.plain.accountId = accountId;
	NpDrmAesEncryptBlocks(rif.bytes, sizeof(rif.bytes), gNpDrmRifNameKey,
		NULL);
	snprintf(name, size, "%016llx%016llx.rif",
		 (unsigned long long)__builtin_bswap64(rif.plain.kind),
		 (unsigned long long)__builtin_bswap64(rif.plain.accountId));
	return 0;
}

/*
 * The PSM license checks run the same two workers with the PSM key (key type 4/5) on a
 * 768-byte message followed by its 768-byte signature.
 */
__attribute__((noinline)) int NpDrmPsmVerifyActData(void *buffer)
{
	struct NpDrmRsaVerifyArgs args;

	if (!buffer)
		return (int)0x808f0101;
	args.data = buffer;
	args.size = 768;
	args.signature = (unsigned char *)buffer + 768;
	args.keyType = 4;
	return SceThreadmgrForDriver_E54FD746(0x2000,
		(int (*)(void *))NpDrmRsaVerify, &args);
}

__attribute__((noinline)) int NpDrmPsmVerifyRif(void *buffer)
{
	struct NpDrmRsaVerifyArgs args;

	if (!buffer)
		return (int)0x808f0101;
	args.data = buffer;
	args.size = 768;
	args.signature = (unsigned char *)buffer + 768;
	args.keyType = 5;
	return SceThreadmgrForDriver_E54FD746(0x2000,
		(int (*)(void *))NpDrmRsaVerify, &args);
}

int NpDrmEcdsaVerifyData(const void *data, SceSize size, const void *signature,
	int curve)
{
	struct NpDrmEcdsaVerifyArgs args;
	unsigned char digest[20];
	int result;

	result = sceSha1Digest(data, size, digest);
	if (result < 0)
		return result;
	args.digest = digest;
	args.signature = signature;
	args.curve = curve;
	return SceThreadmgrForDriver_E54FD746(0x2000,
		(int (*)(void *))NpDrmEcdsaVerify, &args);
}

int PsmDrmCreateMutex(void)
{
	SceUID uid = sceKernelCreateMutex("ScePsmDrm", 0, 0, NULL);

	if (uid >= 0) {
		gPsmDrmMutex = uid;
		return 0;
	}
	return uid;
}

int PsmDrmInitializeActivation(void)
{
	unsigned char seed[128];
	int result;

	result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrPsmactForDriver_39222A58(seed);
	if (result >= 0)
		result = sceSha256Digest(seed, sizeof(seed),
			gPsmDrmActivationChecksum);
	if (result >= 0)
		result = sceHmacSha256Digest(gPsmDrmHmacKey,
			sizeof(gPsmDrmHmacKey), seed, sizeof(seed),
			&gPsmDrmAesMaterial);
	if (result < 0) {
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return result;
	}
	NpDrmClearMemory(seed, sizeof(seed));
	if (PsmDrmLoadActivationFile() < 0 || !gPsmDrmActivationValid)
		NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	return 0;
}

int PsmDrmVerifyActivation(void)
{
	SceUInt64 accountId;
	int result;

	NpDrmGetAccountId(&accountId);
	result = NpDrmPsmVerifyActData(&gPsmDrmWorkingData);
	if (result < 0)
		return result;
	if (memcmp(&gPsmDrmWorkingData, "PSM-ACT", 8) ||
	    __builtin_bswap32(gPsmDrmWorkingData.activation.version) != 1)
		return (int)0x808f0105;
	if (__builtin_bswap32(gPsmDrmWorkingData.activation.bindingValidation))
		return (int)0x808f0109;
	if (__builtin_bswap32(gPsmDrmWorkingData.activation.recordValidation) ||
	    __builtin_bswap64(gPsmDrmWorkingData.activation.expirationTime.tick) <
	    __builtin_bswap64(gPsmDrmWorkingData.activation.startTime.tick))
		return (int)0x808f0105;
	if (__builtin_bswap64(gPsmDrmWorkingData.license.account_id) != accountId)
		return (int)0x808f0108;
	if (timingsafe_memcmp(gPsmDrmWorkingData.license.activation_checksum,
			      gPsmDrmActivationChecksum, 32))
		return (int)0x808f0109;
	return 0;
}

int ScePsmDrmForDriver_791198CE(void)
{
	int result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);

	if (result < 0)
		return result;
	result = PsmDrmLoadActivationFile();
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	return result;
}

int ScePsmDrmForDriver_B09003A7(int *version, int *flags,
	SceUInt64 *accountId, SceUInt64 *startTime, SceUInt64 *expirationTime)
{
	int result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);

	if (result < 0)
		return result;
	if (!gPsmDrmActivationValid) {
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return (int)0x808f0106;
	}
	if (version)
		*version = (int)gPsmDrmActivationState.version;
	if (flags)
		*flags = (int)gPsmDrmActivationState.flags;
	if (accountId)
		*accountId = gPsmDrmActivationState.accountId;
	if (startTime)
		*startTime = gPsmDrmActivationState.startTime.tick;
	if (expirationTime)
		*expirationTime = gPsmDrmActivationState.expirationTime.tick;
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	return result;
}

int ScePsmDrmForDriver_4CD5375C(SceUInt64 *accountId)
{
	int permission;
	int removeResult;
	int result;
	int fd;

	result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	if (accountId)
		*accountId = 0;
	gPsmDrmActivationValid = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	fd = sceIoOpen("tm0:/psmdrm/act.dat", 0x04000003, 6);
	if (fd >= 0) {
		if (accountId &&
		    sceIoPread(fd, &gPsmDrmWorkingData,
			       sizeof(gPsmDrmWorkingData), 0) ==
			    sizeof(gPsmDrmWorkingData))
			*accountId = __builtin_bswap64(
				gPsmDrmWorkingData.license.account_id);
		NpDrmClearMemory(&gPsmDrmWorkingData,
			sizeof(gPsmDrmWorkingData));
		sceIoPwrite(fd, &gPsmDrmWorkingData,
			sizeof(gPsmDrmWorkingData), 0);
		sceIoClose(fd);
	}
	removeResult = sceIoRemove("tm0:/psmdrm/act.dat");
	sceIoSync("tm0:", 0);
	if (removeResult == (int)0x80010002)
		removeResult = 0;
	NpDrmClearMemory(&gPsmDrmActivationState, sizeof(gPsmDrmActivationState));
	NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	return removeResult;
}

int PsmDrmGetRifNameForDriver(char *name, int size, SceUInt64 accountId)
{
	return NpDrmFormatRifName(name, 0x30, accountId, 0);
}

int ScePsmDrmForDriver_CB73E9D3(void *activation, const char *key)
{
	SceIoStat stat;
	int permission;
	int cleanupPermission;
	int written;
	int result;
	int fd;

	if (!activation)
		return (int)0x808f0101;
	result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	gPsmDrmActivationValid = 0;
	memcpy(&gPsmDrmWorkingData, activation, sizeof(gPsmDrmWorkingData));
	if (key)
		NpDrmAesCbcDecrypt(&gPsmDrmWorkingData,
			sizeof(gPsmDrmWorkingData), key, NULL, NULL);
	result = PsmDrmVerifyActivation();
	if (result < 0) {
		NpDrmClearMemory(&gPsmDrmWorkingData,
			sizeof(gPsmDrmWorkingData));
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return result;
	}
	permission = sceKernelSetThreadAccessLevel(0x80);
	if (sceIoGetstat("tm0:/psmdrm", &stat) < 0)
		sceIoMkdir("tm0:/psmdrm", 6);
	fd = sceIoOpen("tm0:/psmdrm/act.dat", 0x04000602, 6);
	if (fd < 0) {
		sceKernelSetThreadAccessLevel(permission);
		NpDrmClearMemory(&gPsmDrmWorkingData,
			sizeof(gPsmDrmWorkingData));
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return fd;
	}
	written = sceIoWrite(fd, &gPsmDrmWorkingData,
		sizeof(gPsmDrmWorkingData));
	sceIoClose(fd);
	if (written < (int)sizeof(gPsmDrmWorkingData)) {
		gPsmDrmActivationValid = 0;
		cleanupPermission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoOpen("tm0:/psmdrm/act.dat", 0x04000003, 6);
		if (result >= 0) {
			NpDrmClearMemory(&gPsmDrmWorkingData,
				sizeof(gPsmDrmWorkingData));
			sceIoPwrite(result, &gPsmDrmWorkingData,
				sizeof(gPsmDrmWorkingData), 0);
			sceIoClose(result);
		}
		sceIoRemove("tm0:/psmdrm/act.dat");
		result = ~(written >> 31) & 0x80870002;
		if (written < 0)
			result = written;
		sceIoSync("tm0:", 0);
		NpDrmClearMemory(&gPsmDrmActivationState,
			sizeof(gPsmDrmActivationState));
		NpDrmClearMemory(&gPsmDrmWorkingData,
			sizeof(gPsmDrmWorkingData));
		sceKernelSetThreadAccessLevel(cleanupPermission);
		sceKernelSetThreadAccessLevel(permission);
		NpDrmClearMemory(&gPsmDrmWorkingData,
			sizeof(gPsmDrmWorkingData));
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return result;
	}
	sceKernelSetThreadAccessLevel(permission);
	result = PsmDrmLoadActivationFile();
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	return result;
}

/*
 * Reads the PBP an eboot signature is computed over: the SHA-224 of the PBP head, the
 * SHA-224 of the region after DATA.PSAR, and (when a digest is given) the SHA-256 of the
 * whole file, which must equal it. The 256-byte signature is read into the caller's
 * buffer at the end and the file descriptor stays open.
 */
int NpDrmEbootSigRead(int fd, const void *unused, SceInt64 size, const void *sha256,
	void *headDigest, void *dataDigest, char *buffer, SceSize bufferSize)
{
	unsigned char header[40];
	unsigned char digest[32];
	SceSha224Context sha224Context;
	SceSha256Context sha256Context;
	SceInt64 dataOffset;
	SceInt64 position;
	SceInt64 limit;
	SceInt64 end;
	SceSize chunk;
	unsigned int index;
	int read;
	int result;

	(void)unused;
	read = sceIoPread(fd, buffer, bufferSize, 0);
	if (read < 0)
		return read;
	if (read <= 39)
		return (int)0x80870005;
	for (index = 0; index < sizeof(header) / sizeof(unsigned int); index++)
		((unsigned int *)header)[index] = ((const unsigned int *)buffer)[index];
	dataOffset = (int)*(int *)(header + 36);
	if (dataOffset + 255 >= size)
		return (int)0x80870005;
	if (*(unsigned int *)header != 0x50425000)
		return (int)0x80870005;
	if (dataOffset < read) {
		read = (int)dataOffset;
		position = dataOffset;
	} else {
		position = read;
	}
	limit = *(unsigned int *)(header + 12);
	if (limit > 0x400)
		limit = 0x400;
	if (position < limit)
		return (int)0x80870005;
	result = sceSha224Digest(buffer, limit, headDigest);
	if (result < 0)
		return result;
	if (sha256) {
		result = sceSha256BlockInit(&sha256Context);
		if (result < 0)
			return result;
		result = sceSha256BlockUpdate(&sha256Context, buffer, read);
		if (result < 0)
			return result;
		while (position < dataOffset) {
			read = sceIoPread(fd, buffer, bufferSize, position);
			if (read <= 0)
				break;
			chunk = read;
			if (dataOffset < position + read)
				chunk = (SceSize)((int)dataOffset - position);
			result = sceSha256BlockUpdate(&sha256Context, buffer, chunk);
			if (result < 0)
				return result;
			position += chunk;
		}
		if (position < dataOffset)
			return read ? read : (int)0x80870002;
	}
	result = sceSha224BlockInit(&sha224Context);
	if (result < 0)
		return result;
	end = (dataOffset + 0x1c003f) & ~(SceInt64)0x3f;
	if (end < size)
		size = end;
	limit = dataOffset + 0x1c0000;
	if (limit < size)
		size = limit;
	if (dataOffset < size) {
		position = dataOffset;
		for (;;) {
			read = sceIoPread(fd, buffer, bufferSize, position);
			if (read <= 0)
				break;
			chunk = read;
			if (size < position + read)
				chunk = (SceSize)(size - position);
			if (sha256) {
				result = sceSha256BlockUpdate(&sha256Context, buffer,
					chunk);
				if (result < 0)
					return result;
			}
			result = sceSha224BlockUpdate(&sha224Context, buffer, chunk);
			if (result < 0)
				return result;
			position += chunk;
			if (position >= size)
				goto dataDone;
		}
		return read ? read : (int)0x80870002;
	}
dataDone:
	result = sceSha224BlockResult(&sha224Context, dataDigest);
	if (result < 0)
		return result;
	if (sha256) {
		while (position < size) {
			read = sceIoPread(fd, buffer, bufferSize, position);
			if (read <= 0)
				break;
			chunk = read;
			if (size < position + read)
				chunk = (SceSize)(size - position);
			result = sceSha256BlockUpdate(&sha256Context, buffer, chunk);
			if (result < 0)
				return result;
			position += chunk;
		}
		if (position < size)
			return read ? read : (int)0x80870002;
		result = sceSha256BlockResult(&sha256Context, digest);
		if (result < 0)
			return result;
		if (timingsafe_memcmp(digest, sha256, 32))
			return (int)0x80870004;
	}
	read = sceIoPread(fd, buffer, 0x100, dataOffset);
	if (read == 0x100)
		return 0;
	return read < 0 ? read : (int)0x80870005;
}

/* The 512-byte signature block an NP-DRM eboot carries instead of a PBP signature. */
struct NpDrmEbootSignature {
	char magic[8];
	unsigned int version;
	unsigned int type;
	unsigned char info[48];
	SceUInt64 accountId;
	SceUInt64 tick;
	SceUInt64 fileSize;
	unsigned int systemVersion;
	unsigned int discCount;
	unsigned char info2[48];
	unsigned char reserved[0xc8];
	unsigned char headSignature[56];
	unsigned char dataSignature[56];
	unsigned char blockSignature[56];
};

_Static_assert(sizeof(struct NpDrmEbootSignature) == 0x200,
	"eboot signature block ABI");
_Static_assert(offsetof(struct NpDrmEbootSignature, headSignature) == 0x158,
	"eboot head signature ABI");
_Static_assert(offsetof(struct NpDrmEbootSignature, blockSignature) == 0x1c8,
	"eboot block signature ABI");

int NpDrmEbootSigGenerate(const char *ebootPath, const void *sha256, int type,
	void *signature, unsigned int swVersion, void *buffer,
	unsigned int bufferSize)
{
	struct NpDrmEbootSignature block;
	SceIoStat stat;
	SceRtcTick tick;
	SceUInt64 accountId;
	unsigned char headDigest[32];
	unsigned char dataDigest[32];
	unsigned int systemVersion;
	unsigned int size;
	unsigned int index;
	int result;
	int fd;

	size = bufferSize;
	if (bufferSize & 0x3f)
		size = bufferSize & ~0x3f;
	if (!sha256 || !ebootPath)
		return (int)0x80870001;
	if (!buffer || !signature || size < 0x400 || (unsigned int)(type - 2) > 1)
		return (int)0x80870001;
	memset(&block, 0, sizeof(block));
	systemVersion = SceSysrootForDriver_67AAB627();
	result = NpDrmGetAccountId(&accountId);
	if (result < 0)
		return result;
	result = SceRtcForDriver_401C0954(&tick);
	if (result < 0)
		return result;
	block.systemVersion = systemVersion < swVersion ? swVersion :
		systemVersion;
	block.accountId = accountId;
	block.tick = tick.tick;
	result = sceIoGetstat(ebootPath, &stat);
	if (result < 0)
		return result;
	result = sceIoOpen(ebootPath, 1, 0x186);
	if (result < 0)
		return result;
	fd = result;
	result = NpDrmEbootSigRead(fd, dataDigest, stat.st_size, sha256,
		headDigest, dataDigest, buffer, size);
	if (result < 0) {
		sceIoClose(fd);
		return result;
	}
	sceIoClose(fd);
	block.version = 1;
	block.type = type;
	block.fileSize = stat.st_size;
	if (type == 3) {
		if (memcmp(buffer, "PSISOIMG", 8))
			return (int)0x80870005;
		memcpy(block.magic, "NPPS1SIG", 8);
	} else {
		if (memcmp(buffer, "NPUMDIMG", 8))
			return (int)0x80870005;
		memcpy(block.magic, "NPUMDSIG", 8);
		memcpy(block.info, (char *)buffer + 16, sizeof(block.info));
	}
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(headDigest,
		block.headSignature, 1);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(dataDigest,
		block.dataSignature, 1);
	if (result < 0)
		return result;
	result = sceSha224Digest(&block,
		offsetof(struct NpDrmEbootSignature, blockSignature), headDigest);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(headDigest,
		block.blockSignature, 1);
	if (result < 0)
		return result;
	memcpy(signature, &block, sizeof(block));
	return 0;
}

/* The 256-byte signature block an NP-DRM PSP eboot (NPUMDIMG) carries. */
struct NpDrmPspEbootSignature {
	char magic[8];
	unsigned int version;
	unsigned int reserved;
	unsigned char info[0x48];
	unsigned char headSignature[56];
	unsigned char dataSignature[56];
	unsigned char blockSignature[56];
};

_Static_assert(sizeof(struct NpDrmPspEbootSignature) == 0x100,
	"PSP eboot signature block ABI");

int NpDrmPspEbootSigGenerate(const char *ebootPath, const void *sha256,
	void *signature, void *buffer, unsigned int bufferSize)
{
	struct NpDrmPspEbootSignature block;
	SceIoStat stat;
	unsigned char headDigest[32];
	unsigned char dataDigest[32];
	unsigned int size;
	unsigned int index;
	int result;
	int fd;

	size = bufferSize;
	if (bufferSize & 0x3f)
		size = bufferSize & ~0x3f;
	if (!sha256 || !ebootPath)
		return (int)0x80870001;
	if (!buffer || !signature || size < 0x400)
		return (int)0x80870001;
	result = sceIoGetstat(ebootPath, &stat);
	if (result < 0)
		return result;
	result = sceIoOpen(ebootPath, 1, 0x186);
	if (result < 0)
		return result;
	fd = result;
	result = NpDrmEbootSigRead(fd, sha256, stat.st_size, sha256,
		headDigest, dataDigest, buffer, size);
	if (result < 0) {
		sceIoClose(fd);
		return result;
	}
	sceIoClose(fd);
	if (memcmp(buffer, "NPUMDIMG", 8))
		return (int)0x80870005;
	result = NpDrmEcdsaVerifyData(buffer, 216, (char *)buffer + 216, 2);
	if (result < 0)
		return result;
	memset(&block, 0, sizeof(block));
	for (index = 0; index < 64 / sizeof(unsigned int); index++)
		((unsigned int *)&block)[index] = ((const unsigned int *)buffer)[index];
	block.reserved = 0;
	memcpy(block.magic, "NPUMDSIG", 8);
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(headDigest,
		block.headSignature, 0);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(dataDigest,
		block.dataSignature, 0);
	if (result < 0)
		return result;
	result = sceSha224Digest(&block,
		offsetof(struct NpDrmPspEbootSignature, blockSignature), headDigest);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(headDigest,
		block.blockSignature, 0);
	if (result < 0)
		return result;
	memcpy(signature, &block, sizeof(block));
	NpDrmClearMemory(&block, sizeof(block));
	NpDrmClearMemory(headDigest, 0x40);
	return result;
}

/*
 * Multi-disc variant of the reader: the SHA-224 covers the head and one 768 KiB region
 * per disc (the disc starts given by the caller), and the SHA-256 of the whole file is
 * not checked here (the caller verifies the disc block instead).
 */
int NpDrmEbootSigReadMultiDisc(int fd, const void *unused, SceInt64 size,
	unsigned int discCount, const SceInt64 *discOffsets, void *headDigest,
	void *dataDigest, char *buffer, SceSize bufferSize)
{
	unsigned char header[40];
	SceSha224Context sha224Context;
	SceInt64 dataOffset;
	SceInt64 position;
	SceInt64 limit;
	SceInt64 end;
	SceSize chunk;
	unsigned int index;
	int read;
	int result;

	(void)unused;
	read = sceIoPread(fd, buffer, bufferSize, 0);
	if (read < 0)
		return read;
	if (read <= 39)
		return (int)0x80870005;
	for (index = 0; index < sizeof(header) / sizeof(unsigned int); index++)
		((unsigned int *)header)[index] = ((const unsigned int *)buffer)[index];
	dataOffset = (int)*(int *)(header + 36);
	if (*(unsigned int *)header != 0x50425000)
		return (int)0x80870005;
	position = read;
	if (dataOffset < read)
		position = dataOffset;
	limit = *(unsigned int *)(header + 12);
	if (limit > 0x400)
		limit = 0x400;
	if (position < limit)
		return (int)0x80870005;
	result = sceSha224Digest(buffer, limit, headDigest);
	if (result < 0)
		return result;
	result = sceSha224BlockInit(&sha224Context);
	if (result < 0)
		return result;
	result = sceSha224BlockUpdate(&sha224Context, buffer, limit);
	if (result < 0)
		return result;
	end = dataOffset + 0xc0000;
	if (discOffsets[0] >= dataOffset && discOffsets[0] < end)
		end = discOffsets[0];
	position = dataOffset;
	while (position < end) {
		read = sceIoPread(fd, buffer, bufferSize, position);
		if (read < 0)
			return read;
		chunk = read;
		if (end < position + read)
			chunk = (SceSize)(end - position);
		result = sceSha224BlockUpdate(&sha224Context, buffer, chunk);
		if (result < 0)
			return result;
		position += chunk;
	}
	for (index = 0; index < discCount; index++) {
		limit = discOffsets[index];
		end = limit + 0xc0000;
		if (end < 0)
			return (int)0x80870005;
		if (size < end)
			end = size;
		if (index + 1 < discCount && discOffsets[index + 1] >= limit &&
		    discOffsets[index + 1] < end)
			end = discOffsets[index + 1];
		for (position = limit; position < end; ) {
			read = sceIoPread(fd, buffer, bufferSize, position);
			if (read < 0)
				return read;
			chunk = read;
			if (end < position + read)
				chunk = (SceSize)(end - position);
			result = sceSha224BlockUpdate(&sha224Context, buffer, chunk);
			if (result < 0)
				return result;
			position += chunk;
		}
	}
	result = sceSha224BlockResult(&sha224Context, dataDigest);
	if (result < 0)
		return result;
	read = sceIoPread(fd, buffer, 0x100, dataOffset);
	if (read == 0x100)
		return 0;
	return read < 0 ? read : (int)0x80870005;
}

/*
 * Multi-disc eboot (MULTISIG): the caller supplies a 200-byte block describing the
 * discs, its own signature and the per-disc offsets; the signature block copies them.
 */
int NpDrmEbootSigGenerateMultiDisc(const char *ebootPath, const void *discInfo,
	void *signature, unsigned int swVersion, void *buffer,
	unsigned int bufferSize)
{
	struct NpDrmEbootSignature block;
	SceIoStat stat;
	SceRtcTick tick;
	SceUInt64 accountId;
	const SceInt64 *offsets;
	unsigned char discDigest[32];
	unsigned char headDigest[32];
	unsigned char dataDigest[32];
	unsigned int systemVersion;
	unsigned int count;
	unsigned int index;
	unsigned int size;
	int result;
	int fd;

	size = bufferSize;
	if (bufferSize & 0x3f)
		size = bufferSize & ~0x3f;
	if (!discInfo || !ebootPath)
		return (int)0x80870001;
	if (!buffer || !signature || size < 0x400)
		return (int)0x80870001;
	memset(&block, 0, sizeof(block));
	systemVersion = SceSysrootForDriver_67AAB627();
	result = NpDrmGetAccountId(&accountId);
	if (result < 0)
		return result;
	result = SceRtcForDriver_401C0954(&tick);
	if (result < 0)
		return result;
	block.systemVersion = systemVersion < swVersion ? swVersion :
		systemVersion;
	block.accountId = accountId;
	block.tick = tick.tick;
	result = sceSha224Digest(discInfo, 200, discDigest);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(discDigest,
		(const char *)discInfo + 200, 1000);
	if (result < 0)
		return result;
	count = *(const unsigned int *)((const char *)discInfo + 52);
	if (count - 1 > 5)
		return (int)0x80870005;
	result = sceIoGetstat(ebootPath, &stat);
	if (result < 0)
		return result;
	if (*(const SceUInt64 *)((const char *)discInfo + 56) !=
	    (SceUInt64)stat.st_size)
		return (int)0x80870005;
	result = sceIoOpen(ebootPath, 1, 0x186);
	if (result < 0)
		return result;
	fd = result;
	offsets = (const SceInt64 *)((const char *)discInfo + 64);
	for (index = 0; index < count; index++) {
		if (offsets[index] < 0 || (SceUInt64)stat.st_size <
		    (SceUInt64)offsets[index]) {
			sceIoClose(fd);
			return (int)0x80870001;
		}
	}
	result = NpDrmEbootSigReadMultiDisc(fd, dataDigest, stat.st_size, count,
		offsets, headDigest, dataDigest, buffer, size);
	if (result < 0) {
		sceIoClose(fd);
		return result;
	}
	sceIoClose(fd);
	memcpy(block.magic, "MULTISIG", 8);
	block.version = 1;
	block.type = 4;
	block.fileSize = stat.st_size;
	block.discCount = count;
	for (index = 0; index < sizeof(block.info) / sizeof(unsigned int); index++) {
		((unsigned int *)block.info)[index] =
			((const unsigned int *)discInfo)[index];
		((unsigned int *)block.info2)[index] =
			((const unsigned int *)discInfo)[index + 16];
	}
	memcpy(block.dataSignature, (const char *)discInfo + 144,
		sizeof(block.dataSignature));
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(headDigest,
		block.headSignature, 1);
	if (result < 0)
		return result;
	result = sceSha224Digest(&block,
		offsetof(struct NpDrmEbootSignature, blockSignature), headDigest);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(headDigest,
		block.blockSignature, 1);
	if (result < 0)
		return result;
	memcpy(signature, &block, sizeof(block));
	return 0;
}

/*
 * Converts a PSP-format eboot signature (NPUMDSIG, 256 bytes) into the 512-byte NP
 * block: the old block and both of its digests are verified first, then the new block is
 * signed with the retail key.
 */
int NpDrmEbootSigConvert(const char *ebootPath, const void *signature,
	void *newSignature, void *buffer, unsigned int bufferSize)
{
	const struct NpDrmPspEbootSignature *old = signature;
	struct NpDrmEbootSignature block;
	SceSha224Context sha224Context;
	SceIoStat stat;
	unsigned char header[40];
	unsigned char headDigest[32];
	unsigned char dataDigest[32];
	unsigned char blockDigest[32];
	SceRtcTick tick;
	SceUInt64 accountId;
	SceInt64 dataOffset;
	SceInt64 position;
	SceInt64 limit;
	SceInt64 end;
	SceSize chunk;
	unsigned int systemVersion;
	unsigned int size;
	unsigned int index;
	int read;
	int result;
	int fd;

	size = bufferSize;
	if (bufferSize & 0x3f)
		size = bufferSize & ~0x3f;
	if (!signature || !ebootPath)
		return (int)0x80870001;
	if (!buffer || !newSignature || size < 0x400)
		return (int)0x80870001;
	if (old->reserved != 0 || memcmp(old->magic, "NPUMDSIG", 8))
		return (int)0x80870005;
	memset(&block, 0, sizeof(block));
	systemVersion = SceSysrootForDriver_67AAB627();
	result = NpDrmGetAccountId(&accountId);
	if (result < 0)
		return result;
	result = SceRtcForDriver_401C0954(&tick);
	if (result < 0)
		return result;
	block.systemVersion = systemVersion;
	block.accountId = accountId;
	block.tick = tick.tick;
	result = sceIoGetstat(ebootPath, &stat);
	if (result < 0)
		return result;
	result = sceIoOpen(ebootPath, 1, 0x186);
	if (result < 0)
		return result;
	fd = result;
	read = sceIoPread(fd, buffer, size, 0);
	if (read < 0) {
		result = read;
		goto close;
	}
	if (read <= 39) {
		result = (int)0x80870005;
		goto close;
	}
	for (index = 0; index < sizeof(header) / sizeof(unsigned int); index++)
		((unsigned int *)header)[index] = ((const unsigned int *)buffer)[index];
	dataOffset = (int)*(int *)(header + 36);
	limit = *(unsigned int *)(header + 12);
	if (limit > 0x400)
		limit = 0x400;
	position = read;
	if (dataOffset < read)
		position = (int)dataOffset;
	if (dataOffset + 255 >= stat.st_size ||
	    *(unsigned int *)header != 0x50425000 || position < limit) {
		result = (int)0x80870005;
		goto close;
	}
	result = sceSha224Digest(buffer, limit, headDigest);
	if (result < 0)
		goto close;
	result = sceSha224BlockInit(&sha224Context);
	if (result < 0)
		goto close;
	end = dataOffset + 0x1c0000;
	if (stat.st_size < end)
		end = stat.st_size;
	limit = (dataOffset + 0x1c003f) & ~(SceInt64)0x3f;
	if (limit < end)
		end = limit;
	position = dataOffset;
	while (position < end) {
		read = sceIoPread(fd, buffer, size, position);
		if (read <= 0)
			break;
		chunk = read;
		if (end < position + read)
			chunk = (SceSize)(end - position);
		result = sceSha224BlockUpdate(&sha224Context, buffer, chunk);
		if (result < 0)
			goto close;
		position += chunk;
	}
	if (position < end) {
		result = read ? read : (int)0x80870002;
		goto close;
	}
	result = sceSha224BlockResult(&sha224Context, dataDigest);
	if (result < 0)
		goto close;
	read = sceIoPread(fd, buffer, 0x100, dataOffset);
	if (read != 0x100) {
		result = ~(read >> 31) & 0x80870005;
		if (read < 0)
			result = read;
		goto close;
	}
	sceIoClose(fd);
	if (memcmp(old->info, (char *)buffer + 16, 0x30)) {
		result = (int)0x80870005;
		goto out;
	}
	result = sceSha224Digest(old, 200, blockDigest);
	if (result < 0)
		goto out;
	result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(blockDigest,
		old->blockSignature, 0);
	if (result < 0)
		goto out;
	result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(headDigest,
		old->headSignature, 0);
	if (result < 0)
		goto out;
	result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(dataDigest,
		old->dataSignature, 0);
	if (result < 0)
		goto out;
	memcpy(block.magic, "NPUMDSIG", 8);
	block.version = 1;
	block.type = 1;
	block.fileSize = stat.st_size;
	memcpy(block.info, old->info, 0x30);
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(headDigest,
		block.headSignature, 1);
	if (result < 0)
		goto out;
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(dataDigest,
		block.dataSignature, 1);
	if (result < 0)
		goto out;
	result = sceSha224Digest(&block,
		offsetof(struct NpDrmEbootSignature, blockSignature), blockDigest);
	if (result < 0)
		goto out;
	result = SceSblGcAuthMgrDrmBBForDriver_050DC6DF(blockDigest,
		block.blockSignature, 1);
	if (result < 0)
		goto out;
	memcpy(newSignature, &block, sizeof(block));
	result = 0;
	goto out;

close:
	sceIoClose(fd);
out:
	return result;
}

/*
 * Verifies the eboot signature block an NP-DRM eboot carries: a PSP-format 256-byte block
 * (type field 0) checked against the file itself, or the 512-byte NP block (types 1-4),
 * including the MULTISIG multi-disc form.
 */
int NpDrmEbootSigVerify(const char *ebootPath, const void *signature,
	unsigned int signatureSize, void *buffer, unsigned int bufferSize)
{
	const struct NpDrmEbootSignature *block = signature;
	const struct NpDrmPspEbootSignature *psp = signature;
	SceSha224Context sha224Context;
	SceIoStat stat;
	unsigned char header[40];
	unsigned char blockDigest[32];
	unsigned char headDigest[32];
	unsigned char dataDigest[32];
	SceUInt64 accountId;
	const SceInt64 *offsets;
	SceInt64 dataOffset;
	SceInt64 position;
	SceInt64 limit;
	SceInt64 end;
	SceSize chunk;
	unsigned int count;
	unsigned int index;
	unsigned int size;
	int read;
	int result;
	int fd;

	size = bufferSize;
	if (bufferSize & 0x3f)
		size = bufferSize & ~0x3f;
	if (size < 0x400)
		return (int)0x80870001;
	if (block->type == 0) {
		if (signatureSize != 256)
			return (int)0x80870001;
		if (memcmp(psp->magic, "NPUMDSIG", 8))
			return (int)0x80870005;
		result = sceSha224Digest(psp, 200, blockDigest);
		if (result < 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(blockDigest,
			psp->blockSignature, 0);
		if (result < 0)
			return result;
		result = sceIoGetstat(ebootPath, &stat);
		if (result < 0)
			return result;
		result = sceIoOpen(ebootPath, 1, 0x186);
		if (result < 0)
			return result;
		fd = result;
		result = NpDrmEbootSigRead(fd, headDigest, stat.st_size, NULL,
			&stat, (char *)&stat + 16, buffer, size);
		sceIoClose(fd);
		if (result < 0)
			return result;
		if (memcmp(psp->info, (char *)buffer + 16, 0x30))
			return (int)0x80870005;
		result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(&stat,
			psp->headSignature, 0);
		if (result < 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7((char *)&stat + 16,
			psp->dataSignature, 0);
		return result & (result >> 31);
	}
	if (signatureSize != 512)
		return (int)0x80870001;
	result = sceSha224Digest(block,
		offsetof(struct NpDrmEbootSignature, blockSignature), blockDigest);
	if (result < 0)
		return result;
	result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(blockDigest,
		block->blockSignature, 1);
	if (result < 0)
		return result;
	result = sceIoGetstat(ebootPath, &stat);
	if (result < 0)
		return result;
	if (block->fileSize != (SceUInt64)stat.st_size)
		return (int)0x80870005;
	result = sceIoOpen(ebootPath, 1, 0x186);
	if (result < 0)
		return result;
	fd = result;
	if (block->type == 4) {
		count = block->discCount;
		offsets = (const SceInt64 *)block->info2;
		if (count - 1 > 5) {
			result = (int)0x80870001;
		} else {
			for (index = 0; index < count; index++) {
				if (offsets[index] < 0 ||
				    (SceUInt64)stat.st_size <
				    (SceUInt64)offsets[index]) {
					result = (int)0x80870001;
					break;
				}
			}
			if (index == count)
				result = NpDrmEbootSigReadMultiDisc(fd,
					dataDigest, stat.st_size, count, offsets,
					NULL, dataDigest, buffer, size);
		}
	} else {
		read = sceIoPread(fd, buffer, size, 0);
		if (read < 0) {
			result = read;
		} else if (read <= 39) {
			result = (int)0x80870005;
		} else {
			memcpy(header, buffer, sizeof(header));
			dataOffset = (int)*(int *)(header + 36);
			limit = *(unsigned int *)(header + 12);
			if (limit > 0x400)
				limit = 0x400;
			position = read;
			if (dataOffset < read)
				position = (int)dataOffset;
			if (dataOffset + 255 >= stat.st_size ||
			    *(unsigned int *)header != 0x50425000 ||
			    position < limit) {
				result = (int)0x80870005;
			} else {
				result = sceSha224Digest(buffer, limit,
					headDigest);
				if (result >= 0)
					result = sceSha224BlockInit(
						&sha224Context);
				end = dataOffset + 0x1c0000;
				if (stat.st_size < end)
					end = stat.st_size;
				limit = (dataOffset + 0x1c003f) &
					~(SceInt64)0x3f;
				if (limit < end)
					end = limit;
				position = dataOffset;
				while (result >= 0 && position < end) {
					read = sceIoPread(fd, buffer, size,
						position);
					if (read <= 0)
						break;
					chunk = read;
					if (end < position + read)
						chunk = (SceSize)(end - position);
					result = sceSha224BlockUpdate(
						&sha224Context, buffer, chunk);
					position += chunk;
				}
				if (result >= 0 && position < end)
					result = read ? read :
						(int)0x80870002;
				if (result >= 0)
					result = sceSha224BlockResult(
						&sha224Context, dataDigest);
				if (result >= 0)
					read = sceIoPread(fd, buffer, 0x100,
						dataOffset);
				if (result >= 0 && read != 0x100)
					result = read < 0 ? read :
						(int)0x80870005;
			}
		}
	}
	sceIoClose(fd);
	if (result < 0)
		return result;
	if (block->type == 4) {
		result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(dataDigest,
			block->dataSignature, 1000);
		if (result < 0)
			return result;
	} else {
		if (block->type == 1 || block->type == 2) {
			if (memcmp(block->magic, "NPUMDSIG", 8) ||
			    memcmp(block->info, (char *)buffer + 16, 0x30))
				return (int)0x80870005;
		} else if (block->type == 3 &&
			   memcmp(block->magic, "NPPS1SIG", 8)) {
			return (int)0x80870005;
		}
		result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(headDigest,
			block->headSignature, 1);
		if (result < 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_4B506BE7(dataDigest,
			block->dataSignature, 1);
		if (result < 0)
			return result;
	}
	result = NpDrmGetAccountId(&accountId);
	if (result < 0)
		return result;
	if (block->accountId != accountId && !NpDrmIsLooseAccountBind())
		return (int)0x80870008;
	if (SceSysrootForDriver_67AAB627() < block->systemVersion)
		return (int)0x8087000b;
	return 0;
}

/*
 * RSA-2048 on the bignum library npdrm shares with msif_ecdsa.c (whose MsifInteger* and
 * MsifMontgomery* routines are the primitives the Marlin code below calls). The same
 * client code exists in post_ss_mgr/rsa.c; it is kept here because npdrm carries exactly
 * one copy of the primitives, shared with the ECDSA code, so that file cannot be linked
 * in without adding a second one.
 */
#define NPDRM_RSA_WORDS	0x40
#define NPDRM_RSA_BYTES	0x100

int rsaEncodeDigest(unsigned char *output, int size, const MarlinRsaData *digest,
	int algorithm);
void MarlinSquareIntegerWords(unsigned int *output, const unsigned int *input,
	int count);
void MarlinMontgomeryPower(unsigned int *output, const unsigned int *input,
	const unsigned int *exponent, const unsigned int *modulus, int count,
	unsigned int inverse);

int rsaCreateSignature(void *signature, const void *hash, const void *key,
		       int words, SceSize size, int type)	/* name unknown */
{
	MarlinRsaData *output = signature;
	const MarlinRsaData *digest = hash;
	const MarlinRsaPrivateKey *privKey = key;
	unsigned int block[64];
	unsigned char *front;
	unsigned char *back;
	unsigned int *first;
	unsigned int *last;
	int half;
	int lastByteOffset;
	int remaining;
	int result;

	if (rsaEncodeDigest((unsigned char *)block, size, digest, type) == -1)
		return -1;
	lastByteOffset = 4 * (words - 1);
	front = (unsigned char *)block;
	half = words / 2;
	result = 0;
	back = front + lastByteOffset;
	if (half > 0) {
		last = block + words;
		first = block;
		remaining = half;
		do {
			unsigned int high = back[3] | (back[2] << 8) |
				(back[1] << 16) | ((unsigned int)back[0] << 24);
			unsigned int low = front[3] | (front[2] << 8) |
				(front[1] << 16) | ((unsigned int)front[0] << 24);

			back -= 4;
			front += 4;
			*first++ = high;
			--remaining;
			*--last = low;
		} while (remaining != 0);
	}
	if (MarlinRsaPrivateOperation(block, block, privKey, words) == -1)
		return -1;
	front = output->data;
	back = front + lastByteOffset;
	if (half > 0) {
		last = block + words;
		first = block;
		remaining = half;
		do {
			unsigned int high = *--last;
			unsigned int low = *first++;

			front[3] = high;
			front[0] = high >> 24;
			front[1] = high >> 16;
			front[2] = high >> 8;
			back[0] = low >> 24;
			--remaining;
			back[1] = low >> 16;
			front += 4;
			back[2] = low >> 8;
			back[3] = low;
			back -= 4;
		} while (remaining != 0);
	}
	output->size = size;
	return result;
}

int rsaVerifySignature(const void *signature, const void *hash, const void *key,
		       int words, SceSize size, int type)	/* name unknown */
{
	const MarlinRsaData *sig = signature;
	const MarlinRsaData *digest = hash;
	const MarlinRsaExponentKey *pubKey = key;
	unsigned int decoded[64];
	unsigned int encoded[64];
	unsigned char *front;
	unsigned char *back;
	unsigned int *first;
	unsigned int *last;
	int half;
	int lastByteOffset;
	int remaining;
	int index;
	int result;

	if (sig->size != (int)size)
		return -1;
	front = sig->data;
	lastByteOffset = 4 * (words - 1);
	half = words / 2;
	result = 0;
	back = front + lastByteOffset;
	if (half > 0) {
		first = encoded;
		last = encoded + words;
		remaining = half;
		do {
			unsigned int high = back[3] | (back[2] << 8) |
				(back[1] << 16) | ((unsigned int)back[0] << 24);
			unsigned int low = front[3] | (front[2] << 8) |
				(front[1] << 16) | ((unsigned int)front[0] << 24);

			back -= 4;
			front += 4;
			*first++ = high;
			--remaining;
			*--last = low;
		} while (remaining != 0);
	}
	if (MarlinRsaExponentOperation(decoded, encoded, pubKey, words) == -1)
		return -1;
	front = (unsigned char *)decoded;
	back = front + lastByteOffset;
	if (half > 0) {
		last = decoded + words;
		first = decoded;
		remaining = half;
		do {
			unsigned int high = *--last;
			unsigned int low = *first++;

			front[3] = high;
			front[0] = high >> 24;
			front[1] = high >> 16;
			front[2] = high >> 8;
			back[0] = low >> 24;
			--remaining;
			back[1] = low >> 16;
			front += 4;
			back[2] = low >> 8;
			back[3] = low;
			back -= 4;
		} while (remaining != 0);
	}
	if (rsaEncodeDigest((unsigned char *)encoded, size, digest, type) == -1)
		return -1;
	first = decoded;
	index = result;
	if (words <= 0)
		return -1;
	do {
		if (*first != encoded[index])
			break;
		++index;
		++first;
	} while (index < words);
	if (words <= 0 || words != index)
		return -1;
	return result;
}

int rsaEncodeDigest(unsigned char *output, int size, const MarlinRsaData *digest, int algorithm)
{
	/* DER DigestInfo: SEQUENCE, AlgorithmIdentifier/OID, OCTET STRING. */
	unsigned char digestInfo[19];
	unsigned char *destination;
	const unsigned char *source;
	int digestSize;
	int prefixSize;
	int paddingSize;
	int remaining;
	int index;
	int result = 0;

	switch (algorithm) {
	case 2: /* MD2, 1.2.840.113549.2.2 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x20;
		digestInfo[3] = 0x0c;
		digestInfo[5] = 0x08;
		digestInfo[6] = 0x2a;
		digestInfo[9] = 0x86;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestSize = 16;
		digestInfo[10] = 0xf7;
		prefixSize = 18;
		digestInfo[11] = 0x0d;
		digestInfo[12] = 0x02;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x05;
		digestInfo[15] = 0x00;
		digestInfo[16] = 0x04;
		digestInfo[17] = 16;
		break;
	case 4: /* MD5, 1.2.840.113549.2.5 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x20;
		digestInfo[3] = 0x0c;
		digestInfo[5] = 0x08;
		digestInfo[6] = 0x2a;
		digestInfo[9] = 0x86;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestSize = 16;
		digestInfo[10] = 0xf7;
		prefixSize = 18;
		digestInfo[11] = 0x0d;
		digestInfo[12] = 0x02;
		digestInfo[13] = 0x05;
		digestInfo[14] = 0x05;
		digestInfo[15] = 0x00;
		digestInfo[16] = 0x04;
		digestInfo[17] = 16;
		break;
	case 5: /* SHA-1, 1.3.14.3.2.26 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x21;
		digestInfo[3] = 0x09;
		digestInfo[5] = 0x05;
		digestInfo[6] = 0x2b;
		digestInfo[7] = 0x0e;
		digestSize = 20;
		digestInfo[8] = 0x03;
		prefixSize = 15;
		digestInfo[9] = 0x02;
		digestInfo[10] = 0x1a;
		digestInfo[11] = 0x05;
		digestInfo[12] = 0x00;
		digestInfo[13] = 0x04;
		digestInfo[14] = 20;
		break;
	case 11: /* SHA-256, 2.16.840.1.101.3.4.2.1 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x31;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[14] = 0x01;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 32;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 32;
		break;
	case 12: /* SHA-384, 2.16.840.1.101.3.4.2.2 */
		digestInfo[4] = 0x06;
		digestInfo[0] = 0x30;
		digestInfo[1] = 0x41;
		digestInfo[2] = 0x30;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestSize = 48;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 48;
		break;
	case 13: /* SHA-512, 2.16.840.1.101.3.4.2.3 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x51;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 64;
		digestInfo[14] = 0x03;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 64;
		break;
	case 14: /* SHA-224, 2.16.840.1.101.3.4.2.4 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x2d;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 28;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x04;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 28;
		break;
	default:
		return -1;
	}
	if (digest->size != digestSize)
		return -1;
	paddingSize = size - (prefixSize + digestSize) - 3;
	if (size < prefixSize + digestSize + 11)
		return -1;
	output[0] = 0;
	output[1] = 1;
	destination = output + 2;
	if (paddingSize > 0) {
		unsigned char *padding = destination;

		remaining = paddingSize;
		do {
			--remaining;
			*padding++ = 0xff;
		} while (remaining != 0);
	}
	destination += paddingSize;
	output[paddingSize + 2] = 0;
	++destination;
	if (prefixSize > 0) {
		unsigned char *prefix = destination;

		source = digestInfo;
		remaining = prefixSize;
		do {
			unsigned char byte = *source++;

			--remaining;
			*prefix++ = byte;
		} while (remaining != 0);
	}
	if (digestSize > 0) {
		destination += prefixSize;
		index = 0;
		do {
			*destination++ = digest->data[index];
			++index;
		} while (index < digestSize);
	}
	return result;
}

__attribute__((cold)) int MarlinRsaExponentOperation(unsigned int *output, const unsigned int *input,
			       const MarlinRsaExponentKey *key, int count)
{
	unsigned int temporary[128];
	unsigned int inverse;
	unsigned int mask;
	unsigned int bit;
	int index;
	int remaining;

	if (count <= 0 || count > 64)
		return -1;
	if ((key->modulus[count - 1] & 0x80000000) == 0)
		return -1;
	if ((key->modulus[0] & 1) == 0)
		return -1;
	if (!MsifIntegerGreaterThan(key->modulus, input, count))
		return -1;
	if (count > 0) {
		index = 0;
		remaining = count;
		do {
			unsigned int word = *input++;

			remaining--;
			temporary[index] = 0;
			temporary[count + index] = word;
			index++;
		} while (remaining != 0);
	}
	inverse = 1;
	for (mask = 2; mask != 0; mask *= 2) {
		bit = (inverse * -key->modulus[0]) & mask;
		inverse |= bit;
	}
	MsifIntegerRemainder(temporary, temporary, 2 * count,
				    key->modulus, count);
	MarlinMontgomeryPower(temporary, temporary, key->exponent,
			      key->modulus, count, inverse);
	MsifMontgomeryReduce(output, temporary, count,
			       key->modulus, count, inverse);
	return 0;
}

__attribute__((cold)) int MarlinRsaPrivateOperation(unsigned int *output, const unsigned int *input,
			      const MarlinRsaPrivateKey *key, int count)
{
	unsigned int residueP[32];
	unsigned int residueQ[32];
	unsigned int product[64];
	int half;
	unsigned int *temporary;
	unsigned int inverseP;
	unsigned int inverseQ;
	unsigned int maskP;
	unsigned int maskQ;
	unsigned int bit;
	unsigned int carry;
	int index;

	if (count <= 0 || count > 64)
		return -1;
	half = count / 2;
	temporary = product + half;
	MsifIntegerMultiply(product, key->primeP, half, key->primeQ, half);
	if ((product[count - 1] & key->primeP[half - 1] &
	     key->primeQ[half - 1] & 0x80000000) == 0)
		return -1;
	if ((product[0] & key->primeP[0] & key->primeQ[0] & 1) == 0)
		return -1;
	if (!MsifIntegerGreaterThan(product, input, count))
		return -1;
	for (index = 0; index < half; index++)
		product[index] = 0;
	inverseQ = 1;
	maskQ = 2;
	inverseP = 1;
	maskP = 2;
	do {
		bit = (inverseQ * -key->primeQ[0]) & maskQ;
		maskQ *= 2;
		inverseQ |= bit;
	} while (maskQ != 0);
	MsifIntegerRemainder(temporary, input, count, key->primeQ, half);
	MsifIntegerRemainder(temporary, product, count, key->primeQ, half);
	MarlinMontgomeryPower(residueQ, temporary, key->exponentQ,
			      key->primeQ, half, inverseQ);
	MsifMontgomeryReduce(residueQ, residueQ, half, key->primeQ,
			       half, inverseQ);
	do {
		bit = (inverseP * -key->primeP[0]) & maskP;
		maskP *= 2;
		inverseP |= bit;
	} while (maskP != 0);
	MsifIntegerRemainder(temporary, input, count, key->primeP, half);
	MsifIntegerRemainder(temporary, product, count, key->primeP, half);
	MarlinMontgomeryPower(residueP, temporary, key->exponentP,
			      key->primeP, half, inverseP);
	for (index = 0; index < half; index++)
		temporary[index] = residueQ[index];
	MsifIntegerRemainder(temporary, product, count, key->primeP, half);
	if (MsifIntegerSubtract(residueP, residueP, temporary, half, 0))
		MsifIntegerAdd(residueP, residueP, key->primeP, half, 0);
	MsifMontgomeryMultiply(residueP, residueP, key->coefficient,
				 key->primeP, half, inverseP);
	MsifIntegerMultiply(product, residueP, half, key->primeQ, half);
	carry = MsifIntegerAdd(output, residueQ, product, half, 0);
	for (index = half; index < count; index++) {
		unsigned int word = product[index];
		unsigned int sum = carry + word;

		output[index] = sum;
		carry = (word & ~sum) >> 31;
	}
	return 0;
}


__attribute__((cold)) void MarlinMontgomeryPower(unsigned int *output, const unsigned int *input,
			   const unsigned int *exponent, const unsigned int *modulus,
			   int count, unsigned int inverse)
{
	unsigned int shiftedExponent[64];
	unsigned int accumulator[65];
	unsigned int square[128];
	unsigned int top;
	unsigned int carry;
	unsigned int leadingZeros;
	unsigned int shift;
	unsigned int *cursor;
	int usedWords;
	int squareCount;
	int zeros;
	int index;

	if (count <= 0 || count > 64)
		return;
	squareCount = 2 * count;
	top = modulus[count - 1];
	carry = 0;
	for (index = 0; index < count; index++) {
		unsigned int word = modulus[index];
		unsigned int negative = -word - carry;

		accumulator[index] = negative;
		carry = (word | negative) >> 31;
	}
	if ((top & 0x80000000) == 0)
		MsifIntegerRemainder(accumulator, accumulator, count,
					    modulus, count);
	leadingZeros = 0;
	zeros = 32;
	for (index = count - 1; index >= 0 && zeros == 32; index--) {
		unsigned int word = exponent[index];
		int upperHalf = ((int)-(word >> 16) >> 16) & 16;
		int upperByte;
		int upperNibble;
		int upperPair;

		word >>= upperHalf;
		upperByte = ((int)(word - 256) >> 16) & 8;
		word <<= upperByte;
		upperNibble = ((int)(word - 4096) >> 16) & 4;
		word <<= upperNibble;
		upperPair = ((int)(word - 0x4000) >> 16) & 2;
		word <<= upperPair;
		zeros = 16 - upperHalf;
		zeros += upperByte;
		zeros += upperNibble;
		zeros += upperPair;
		zeros -= (word >> 14) & ~(word >> 15);
		zeros += 2;
		leadingZeros += zeros;
	}
	usedWords = count - (leadingZeros >> 5);
	shift = leadingZeros & 31;
	MsifIntegerShiftLeft(shiftedExponent, exponent, usedWords, shift);
	cursor = shiftedExponent;
	while (usedWords > 0) {
		unsigned int bit = 0;

		for (index = 0; index < usedWords; index++) {
			unsigned int word = cursor[index];

			cursor[index] = (bit & 1) | (word * 2);
			bit = word >> 31;
		}
		shift++;
		if (shift >= 32) {
			usedWords--;
			cursor++;
			shift &= 31;
		}
		MarlinSquareIntegerWords(square, accumulator, count);
		MsifMontgomeryReduce(accumulator, square, squareCount,
				       modulus, count, inverse);
		if (bit != 0)
			MsifMontgomeryMultiply(accumulator, accumulator, input,
						 modulus, count, inverse);
	}
	for (index = 0; index < count; index++)
		*output++ = accumulator[index];
}

__attribute__((cold)) void MarlinSquareIntegerWords(unsigned int *output,
			      const unsigned int *input, int count)
{
	unsigned int product[64];
	unsigned int diagonal[129];
	unsigned int crossProducts[128];
	unsigned int *cursor;
	const unsigned int *first;
	const unsigned int *second;
	int index;
	int remaining;
	int total;

	if (count <= 0 || count > 64)
		return;
	remaining = count - 1;
	cursor = diagonal;
	first = input;
	for (index = count; index > 0; index--) {
		unsigned int word = *first++;
		unsigned short low = word;
		unsigned int high = word >> 16;
		unsigned int lowSquare = low * low;
		unsigned int cross = low * high;
		unsigned int middle = cross + (lowSquare >> 16);

		cursor[0] = lowSquare + (cross << 17);
		cursor[1] = high * high + (middle >> 16)
			+ (((unsigned short)middle + cross) >> 16);
		cursor += 2;
	}
	total = 2 * count;
	cursor = crossProducts;
	index = total;
	if (index > 0) {
		do {
			index--;
			*cursor++ = 0;
		} while (index != 0);
	}
	cursor = crossProducts + 1;
	first = input;
	second = input + 1;
	if (remaining > 0) {
		do {
			MsifIntegerMultiplyWord(product, second, remaining, *first++);
			MsifIntegerAdd(cursor, cursor, product, remaining + 1, 0);
			remaining--;
			cursor += 2;
			second++;
		} while (remaining != 0);
	}
	MsifIntegerShiftLeft(crossProducts + 1, crossProducts + 1,
				    total - 1, 1);
	MsifIntegerAdd(output, diagonal, crossProducts, total, 0);
}

/* Private-key operation followed by the PKCS#1 00 02 padding strip. */
int NpDrmRsaDecryptData(MarlinRsaData *output, const MarlinRsaData *input,
	const MarlinRsaPrivateKey *key, int words, int bytes)
{
	unsigned int block[64];
	unsigned char *front;
	unsigned char *back;
	unsigned char *cursor;
	unsigned char *end;
	int half;
	int lastByteOffset;
	int length;
	int index;

	if (input->size != bytes)
		return -1;
	lastByteOffset = 4 * (words - 1);
	front = input->data;
	back = front + lastByteOffset;
	half = words / 2;
	for (index = 0; index < half; index++) {
		unsigned int high = back[3] | (back[2] << 8) | (back[1] << 16) |
			((unsigned int)back[0] << 24);
		unsigned int low = front[3] | (front[2] << 8) |
			(front[1] << 16) | ((unsigned int)front[0] << 24);

		back -= 4;
		front += 4;
		block[index] = high;
		block[words - 1 - index] = low;
	}
	if (MarlinRsaPrivateOperation(block, block, key, words) == -1)
		return -1;
	front = (unsigned char *)block;
	back = front + lastByteOffset;
	for (index = 0; index < half; index++) {
		unsigned int high = block[words - 1 - index];
		unsigned int low = block[index];

		front[3] = high;
		front[0] = high >> 24;
		front[1] = high >> 16;
		front[2] = high >> 8;
		back[0] = low >> 24;
		back[1] = low >> 16;
		back[2] = low >> 8;
		back[3] = low;
		front += 4;
		back -= 4;
	}
	if (front[0] != 0 || front[1] != 2)
		return -1;
	cursor = front + 2;
	end = front + bytes;
	while (cursor < end && *cursor)
		cursor++;
	if (cursor >= end || cursor - (front + 2) <= 7)
		return -1;
	length = (int)(end - cursor);
	cursor++;
	for (index = 0; index < length - 1; index++)
		output->data[index] = cursor[index];
	output->size = length - 1;
	return 0;
}

int NpDrmRsaDecrypt2048(MarlinRsaData *output, const MarlinRsaData *input,
	const MarlinRsaPrivateKey *key)
{
	return NpDrmRsaDecryptData(output, input, key, NPDRM_RSA_WORDS,
		NPDRM_RSA_BYTES);
}

int NpDrmRsaSign2048(MarlinRsaData *output, const MarlinRsaData *input,
	const MarlinRsaPrivateKey *key, int algorithm)
{
	return rsaCreateSignature(output, input, key, NPDRM_RSA_WORDS,
		NPDRM_RSA_BYTES, algorithm);
}

int NpDrmRsaVerify2048(const MarlinRsaData *signature,
	const MarlinRsaData *digest, const MarlinRsaExponentKey *key,
	int algorithm)
{
	return rsaVerifySignature(signature, digest, key, NPDRM_RSA_WORDS,
		NPDRM_RSA_BYTES, algorithm);
}

/*
 * Both PSM RIF entry points check a PSM-RIF license block: the caller's license is copied
 * to the working buffer, its PSM signature verified, then the same magic/version/validity/
 * account/checksum checks run and the requested fields are returned.
 */
int PsmDrmGetRifNameForInstallForDriver(char *name, const void *license)
{
	SceUInt64 accountId;
	SceUInt64 licenseAccountId;
	int result;

	if (!license)
		return (int)0x808f0101;
	result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	memcpy(&gPsmDrmWorkingData, license, sizeof(gPsmDrmWorkingData));
	NpDrmGetAccountId(&accountId);
	result = NpDrmPsmVerifyRif(&gPsmDrmWorkingData);
	if (result >= 0) {
		if (memcmp(&gPsmDrmWorkingData, "PSM-RIF", 8) ||
		    __builtin_bswap32(gPsmDrmWorkingData.activation.version) != 1)
			result = (int)0x808f0105;
		else if (__builtin_bswap32(
				 gPsmDrmWorkingData.activation.bindingValidation))
			result = (int)0x808f0109;
		else if (__builtin_bswap32(
				 gPsmDrmWorkingData.activation.recordValidation) ||
			 __builtin_bswap64(
				 gPsmDrmWorkingData.activation.expirationTime.tick) <
			 __builtin_bswap64(
				 gPsmDrmWorkingData.activation.startTime.tick) ||
			 __builtin_bswap32(gPsmDrmWorkingData.activation.flags))
			result = (int)0x808f0105;
		else if (__builtin_bswap64(
				 gPsmDrmWorkingData.license.account_id) != accountId)
			result = (int)0x808f0108;
		else if (timingsafe_memcmp(
				 gPsmDrmWorkingData.license.activation_checksum,
				 gPsmDrmActivationChecksum, 32))
			result = (int)0x808f0109;
	} else if (result == (int)0x808f0104) {
		NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return result;
	}
	if (!memcmp(&gPsmDrmWorkingData, "PSM-RIF", 8))
		licenseAccountId = __builtin_bswap64(
			gPsmDrmWorkingData.license.account_id);
	else
		licenseAccountId = 0;
	NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	if (result)
		return result;
	return NpDrmFormatRifName(name, 0x30, licenseAccountId, 0);
}

int ScePsmDrmForDriver_984F9017(const ScePsmDrmLicense *license,
	char *contentId, SceUInt64 *accountId, SceUInt64 *startTime,
	SceUInt64 *expirationTime)
{
	SceUInt64 currentAccountId;
	SceUInt64 time;
	int result;

	if (!license)
		return (int)0x808f0101;
	result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	memcpy(&gPsmDrmWorkingData, license, sizeof(gPsmDrmWorkingData));
	NpDrmGetAccountId(&currentAccountId);
	result = NpDrmPsmVerifyRif(&gPsmDrmWorkingData);
	if (result >= 0) {
		if (memcmp(&gPsmDrmWorkingData, "PSM-RIF", 8) ||
		    __builtin_bswap32(gPsmDrmWorkingData.activation.version) != 1)
			result = (int)0x808f0105;
		else if (__builtin_bswap32(
				 gPsmDrmWorkingData.activation.bindingValidation))
			result = (int)0x808f0109;
		else if (__builtin_bswap32(
				 gPsmDrmWorkingData.activation.recordValidation) ||
			 __builtin_bswap64(
				 gPsmDrmWorkingData.activation.expirationTime.tick) <
			 __builtin_bswap64(
				 gPsmDrmWorkingData.activation.startTime.tick) ||
			 __builtin_bswap32(gPsmDrmWorkingData.activation.flags))
			result = (int)0x808f0105;
		else if (__builtin_bswap64(
				 gPsmDrmWorkingData.license.account_id) !=
			 currentAccountId)
			result = (int)0x808f0108;
		else if (timingsafe_memcmp(
				 gPsmDrmWorkingData.license.activation_checksum,
				 gPsmDrmActivationChecksum, 32))
			result = (int)0x808f0109;
	} else if (result == (int)0x808f0104) {
		NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return result;
	}
	if (!memcmp(&gPsmDrmWorkingData, "PSM-RIF", 8)) {
		if (contentId)
			strncpy(contentId, gPsmDrmWorkingData.license.content_id,
				0x30);
		if (accountId)
			*accountId = __builtin_bswap64(
				gPsmDrmWorkingData.license.account_id);
		if (startTime) {
			time = __builtin_bswap64(
				gPsmDrmWorkingData.activation.startTime.tick);
			if (!time)
				time = 0x7fffffffffffffffULL;
			else if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
			*startTime = time;
		}
		if (expirationTime) {
			time = __builtin_bswap64(
				gPsmDrmWorkingData.activation.expirationTime.tick);
			if (!time)
				time = 0x7fffffffffffffffULL;
			else if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
			*expirationTime = time;
		}
	}
	NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_FC84CA1A(void)
{
	return NpDrmIsLooseAccountBind() != 0;
}

int SceNpDrmForDriver_FE7B17B6(const SceNpDrmLicense *license,
	unsigned int size)
{
	int vita;

	if (size >= 0x200)
		vita = (unsigned short)license->license_type != 0;
	else
		vita = 0;
	return NpDrmVerifyRifSignature(license, vita);
}

int SceNpDrmForDriver_B6CA3A2C(const char *ebootPath, const void *signature)
{
	int result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);

	if (result < 0)
		return result;
	result = NpDrmEbootSigVerify(ebootPath, signature, 0x100,
		gNpDrmActivationCache.data.secondary_key_table, 0x7c0);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

/* PSP encrypted-data ("\0PSPEDAT") header a PS1 eboot carries. */
struct NpDrmPspEdatHeader {
	SceUInt64 magic;		/* "\0PSPEDAT" */
	int flags;
	unsigned short dataOffset;
	unsigned short reserved;
	unsigned char data[0x48];
	unsigned char signature[0x28];
	unsigned char reserved2[0x10];
};

_Static_assert(sizeof(struct NpDrmPspEdatHeader) == 0x90,
	"PSP encrypted data header ABI");
_Static_assert(offsetof(struct NpDrmPspEdatHeader, signature) == 0x58,
	"PSP encrypted data signature ABI");

/* gcauthmgr's exports this file calls; declared in its own header when it has one. */
int SceSblGcAuthMgrDrmBBForDriver_4C5DE1AA(const void *license, const void *key,
	const void *secret, void *output);
int SceSblGcAuthMgrDrmBBForDriver_4FE89ADB(const void *key, const void *pgd,
	unsigned int pgdSize, void *output, unsigned int outputSize);

int SceNpDrmForDriver_4E321BDE(const SceNpDrmLicense *license, const char *doc,
	unsigned int size, void *key)
{
	struct NpDrmPspEdatHeader header;
	unsigned char klicense[16];
	unsigned char contentKey[16];
	int result;

	if (!key || !doc)
		return (int)0x80870001;
	NpDrmClearMemory(key, 16);
	if (size <= 0x8f)
		return (int)0x80870005;
	memcpy(&header, doc, sizeof(header));
	if (header.magic != 0x5441444550535000ULL)
		return (int)0x80870005;
	NpDrmClearMemory(klicense, 16);
	if (header.flags >= 0) {
		result = NpDrmEcdsaVerifyData(&header, 0x58,
			(char *)&header + 0x58, 1);
		if (result < 0)
			return result;
		if (!(header.flags & 0x1000000)) {
			if (!license)
				return (int)0x80870001;
			result = NpDrmGetRifKey(license, klicense, NULL, NULL,
				NULL, NULL, 1);
			if (result < 0)
				return result;
		}
	}
	result = SceSblGcAuthMgrDrmBBForDriver_4C5DE1AA(&header, klicense, 0,
		contentKey);
	if (result < 0)
		return result;
	if (size < header.dataOffset)
		return (int)0x80870005;
	result = SceSblGcAuthMgrDrmBBForDriver_4FE89ADB(contentKey,
		doc + header.dataOffset, size - header.dataOffset, key, 8);
	return result < 0 ? result : 0;
}

int SceNpDrmForDriver_9265B350(int *actType, int *versionFlag,
	SceUInt64 *accountId, SceUInt64 *startTime, SceUInt64 *expirationTime)
{
	SceIoStat stat;
	SceUInt64 currentAccountId;
	unsigned int version;
	int permission;
	int readPermission;
	int read;
	int status;
	int result;
	int fd;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	status = result;
	if (!gNpDrmActivationValid) {
		status = NpDrmLoadActivationFile();
		if (status < 0)
			gNpDrmActivationValid = 0;
		else if (gNpDrmActivationValid)
			goto activationLoaded;
		permission = sceKernelSetThreadAccessLevel(0x80);
		read = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
		sceKernelSetThreadAccessLevel(permission);
		if (read == 0) {
			permission = sceKernelSetThreadAccessLevel(0x80);
			readPermission = sceKernelSetThreadAccessLevel(0x80);
			read = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
			sceKernelSetThreadAccessLevel(readPermission);
			if (read == 0) {
				fd = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001,
					0);
				if (fd >= 0) {
					read = sceIoRead(fd,
						&gNpDrmActivationCache,
						sizeof(gNpDrmActivationCache));
					sceIoClose(fd);
					if (read ==
					    (int)sizeof(gNpDrmActivationCache)) {
						sceKernelSetThreadAccessLevel(
							permission);
						goto activationLoaded;
					}
				}
			}
			sceKernelSetThreadAccessLevel(permission);
		}
		NpDrmClearMemory(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache));
	}
activationLoaded:
	if (actType)
		*actType = __builtin_bswap16((unsigned short)
			gNpDrmActivationCache.data.act_type);
	if (versionFlag)
		*versionFlag = __builtin_bswap16((unsigned short)
			gNpDrmActivationCache.data.version_flag);
	if (accountId)
		*accountId = gNpDrmActivationCache.data.account_id;
	if (startTime)
		*startTime = 0;
	if (expirationTime)
		*expirationTime = 0;
	version = __builtin_bswap32(gNpDrmActivationCache.data.version);
	if (version > 1) {
		if (startTime) {
			SceUInt64 time = __builtin_bswap64(
				gNpDrmActivationCache.data.start_time.tick);

			if (!time)
				time = 0x7fffffffffffffffULL;
			else if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
			*startTime = time;
		}
		if (expirationTime) {
			SceUInt64 time = __builtin_bswap64(
				gNpDrmActivationCache.data.expiration_time.tick);

			if (!time)
				time = 0x7fffffffffffffffULL;
			else if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
			*expirationTime = time;
		}
	} else {
		if (startTime)
			*startTime = 0;
		if (expirationTime)
			*expirationTime = 0x7fffffffffffffffULL;
	}
	if (gNpDrmActivationValid) {
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return 0;
	}
	permission = sceKernelSetThreadAccessLevel(0x80);
	read = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(permission);
	if (read != 0) {
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return (int)0x80870006;
	}
	if (gNpDrmActivationCache.data.account_id) {
		NpDrmGetAccountId(&currentAccountId);
		if (gNpDrmActivationCache.data.account_id != currentAccountId &&
		    !NpDrmIsLooseAccountBind())
			status = (int)0x80870008;
		else if (version > 1)
			status = memcmp(&gNpDrmOpenPsId,
				gNpDrmActivationCache.data.openpsid, 0x10) ?
				(int)0x80870009 : 0;
		else
			status = 0;
	}
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	result = ~(status >> 31) & 0x80870004;
	if (status < 0)
		result = status;
	return result;
}

int ScePsmDrmForDriver_8C8CFD01(const ScePsmDrmLicense *license,
	ScePsmDrmKeySet *keydata, int *flags, SceRtcTick *startTime,
	SceRtcTick *expirationTime)
{
	SceUInt64 accountId;
	SceUInt64 time;
	int result;

	if (!keydata || !license)
		return (int)0x808f0101;
	result = sceKernelLockMutex(gPsmDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	if (!gPsmDrmActivationValid) {
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return (int)0x808f0106;
	}
	memcpy(&gPsmDrmWorkingData, license, sizeof(gPsmDrmWorkingData));
	NpDrmGetAccountId(&accountId);
	result = NpDrmPsmVerifyRif(&gPsmDrmWorkingData);
	if (result < 0)
		goto unlock;
	if (memcmp(&gPsmDrmWorkingData, "PSM-RIF", 8) ||
	    __builtin_bswap32(gPsmDrmWorkingData.activation.version) != 1)
		result = (int)0x808f0105;
	else if (__builtin_bswap32(gPsmDrmWorkingData.activation.bindingValidation))
		result = (int)0x808f0109;
	else if (__builtin_bswap32(gPsmDrmWorkingData.activation.recordValidation) ||
		 __builtin_bswap64(
			 gPsmDrmWorkingData.activation.expirationTime.tick) <
		 __builtin_bswap64(
			 gPsmDrmWorkingData.activation.startTime.tick) ||
		 __builtin_bswap32(gPsmDrmWorkingData.activation.flags))
		result = (int)0x808f0105;
	else if (__builtin_bswap64(gPsmDrmWorkingData.license.account_id) !=
		 accountId)
		result = (int)0x808f0108;
	else if (timingsafe_memcmp(
			 gPsmDrmWorkingData.license.activation_checksum,
			 gPsmDrmActivationChecksum, 32))
		result = (int)0x808f0109;
	if (result) {
		sceKernelUnlockMutex(gPsmDrmMutex, 1);
		return result;
	}
	NpDrmAesCbcDecrypt(&gPsmDrmWorkingData.license.keyset,
		sizeof(gPsmDrmWorkingData.license.keyset),
		gPsmDrmActivationState.binding.key,
		gPsmDrmActivationState.binding.iv, NULL);
	NpDrmAesCbcDecrypt(&gPsmDrmWorkingData.license.keyset, 0x20,
		gPsmDrmAesMaterial.key, gPsmDrmAesMaterial.iv, NULL);
	memcpy(keydata, &gPsmDrmWorkingData.license.keyset, sizeof(*keydata));
	if (flags)
		*flags = gPsmDrmActivationState.flags;
	if (startTime) {
		time = __builtin_bswap64(
			gPsmDrmWorkingData.activation.startTime.tick);
		if (!time)
			time = 0x7fffffffffffffffULL;
		else if (time != 0x7fffffffffffffffULL)
			time = time * 1000 + 62135596800000000ULL;
		startTime->tick = time;
		if (time < gPsmDrmActivationState.startTime.tick)
			startTime->tick = gPsmDrmActivationState.startTime.tick;
	}
	if (expirationTime) {
		time = __builtin_bswap64(
			gPsmDrmWorkingData.activation.expirationTime.tick);
		if (!time)
			time = 0x7fffffffffffffffULL;
		else if (time != 0x7fffffffffffffffULL)
			time = time * 1000 + 62135596800000000ULL;
		expirationTime->tick = time;
		if (gPsmDrmActivationState.expirationTime.tick < time)
			expirationTime->tick =
				gPsmDrmActivationState.expirationTime.tick;
	}
	result = 0;
unlock:
	sceKernelUnlockMutex(gPsmDrmMutex, 1);
	return result;
}

