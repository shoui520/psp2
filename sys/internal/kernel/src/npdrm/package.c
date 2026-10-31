/* Package implementation; the original source filename is unavailable. */
#include "package_internal.h"
#include <string.h>
#include <sysmem/utils.h>
#include <npdrm/npdrm.h>
#include <sysmem/sblaimgr.h>
#include <acmgr/acmgr.h>
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include <intrmgr/syscall_copy.h>
#include <npdrm/npdrmpackage.h>

#define NPDRM_BE32(value) \
	((((value) & 0xff000000U) >> 24) | (((value) & 0x00ff0000U) >> 8) | \
	 (((value) & 0x0000ff00U) << 8) | (((value) & 0x000000ffU) << 24))
#define NPDRM_BE64(value) \
	(((uint64_t)NPDRM_BE32((uint32_t)(value)) << 32) | \
	 NPDRM_BE32((uint32_t)((value) >> 32)))

enum NpDrmPackageRsaKey {
	NPDRM_RSA_PSP,
	NPDRM_RSA_VITA,
	NPDRM_RSA_LIVE_AREA,
	NPDRM_RSA_ALTERNATE,
};

static const unsigned int gNpDrmPackageRsaModuli[4][64] = {
	{ /* PSP */
		0xb8519dd5, 0x43e51bbf, 0xde1c1acc, 0x2567107c,
		0x45c7c4d0, 0x79f6b9d5, 0xaba75e88, 0xe16aade1,
		0xf1886c43, 0x4018b2cd, 0xaad4e6fb, 0xa659a662,
		0x96565457, 0x7bd3c888, 0xa09dc290, 0x5a7194ae,
		0x8d379ad6, 0x63faa3ad, 0x85dc8913, 0xad0f0d09,
		0x1137b3b2, 0x7fc7d832, 0xacbc234e, 0x8b909202,
		0xace8a719, 0x3d802bc5, 0x2b30a2e5, 0x0ebc22f4,
		0xc1612d1a, 0x31c8e468, 0xa9da2e70, 0xe4afccb9,
		0x99c31ff7, 0xea1a0514, 0x0699257c, 0x625f5336,
		0x4e5d43a1, 0x11718d4a, 0x72f9dd1a, 0xeffe5316,
		0xadcc7026, 0x75c060ad, 0xad97698e, 0x3f169b7e,
		0x01392905, 0xaedd64b4, 0x8bbea8f6, 0x8ed6bf17,
		0xca51d0fb, 0x635ebffb, 0x39963987, 0xff06f69f,
		0xa1f227b8, 0xc3720826, 0x5bfa8ef9, 0x476e7353,
		0xa5d6906f, 0x64c3ba43, 0x83e98f48, 0x395a2aad,
		0xc9899919, 0xd4708d5f, 0x2e3b51a6, 0xbbdb6aa3,
	},
	{ /* Vita */
		0x2813a6c3, 0xdecdd689, 0x6b88b869, 0x7c7eaeae,
		0xdba9a0b3, 0x3f882acf, 0x3a168aa2, 0xf2f557a7,
		0x79d7b4d6, 0x683ea1d0, 0xbbf3a023, 0x70d820c8,
		0x91eee462, 0xe1b13f83, 0x2bc73e2b, 0x18e62b16,
		0x2f63ddd1, 0x22d28d3c, 0x36da79f8, 0xc556f16a,
		0xf522afc9, 0x6664f6c4, 0xa88cf637, 0x49cb0065,
		0x1cd55e04, 0x7a5d4f9c, 0x350cdd8c, 0x139ee661,
		0x1c6b8ae1, 0xf3f51096, 0x0fa2882d, 0x4acef46c,
		0xe40b5f9e, 0xb44247d5, 0x00ae13a0, 0x012807c5,
		0xe657a795, 0x0dc0e4d3, 0xb88d41ee, 0xca4a88bc,
		0x29843914, 0xed9f1027, 0xac6cdcda, 0x890b953a,
		0xa4609ccf, 0x2ab1e680, 0x4550d6f7, 0xb4c77d20,
		0x10fa1ca1, 0xad3bcd46, 0x33eb12c7, 0x61d7571b,
		0x854e38ea, 0x19911288, 0xd5cc4311, 0xc4bbaf63,
		0xefb50784, 0x8b3bad9d, 0xe3bc68fe, 0x731cf624,
		0x2d30274a, 0xd2691b1d, 0xa67c9a04, 0x85d72f79,
	},
	{ /* LiveArea */
		0xe9fdc62d, 0x73895376, 0x8c30a334, 0x84eead72,
		0x7abe861e, 0x4eebeb91, 0x43d69215, 0xa964f3a3,
		0x20af13fb, 0xce387f38, 0x81dc9227, 0xce28806b,
		0xa684814b, 0x4b26af23, 0x26038ef4, 0xb8967c3f,
		0xfca24bd9, 0x1ea2856e, 0xae322e3a, 0x8c440c12,
		0x926b7cff, 0x2ff0d1cc, 0x6671b7cb, 0x0ef24487,
		0x46a70feb, 0xf76f68b6, 0x3a74612a, 0x8a85884d,
		0x15403780, 0x59b3a101, 0xc84ba685, 0x41d950b2,
		0x2106d6b5, 0x80f285ad, 0xb01a56e3, 0xbe152285,
		0xae364816, 0xa5265f93, 0x1756b94d, 0x23eba915,
		0xed1c14d7, 0xa0449ef6, 0xae25d06b, 0x3b840669,
		0x014ff15c, 0xd01a9a5e, 0x9c00c846, 0x944f5abc,
		0x817919b1, 0xa2de3e1f, 0xcdf37ae7, 0xc83517cc,
		0xc93cb2a1, 0x3e30ac7c, 0x7f6c0fe6, 0xb09990b6,
		0x67b37d05, 0xd015bb3e, 0x678ac454, 0x7376208f,
		0x09d740b2, 0x1328c250, 0xcecfc0ca, 0x8e5c3b05,
	},
	{ /* Alternate package signature */
		0x126a85eb, 0x14e4708d, 0x33d327c7, 0xfd8a1f5b,
		0xbe6e05c2, 0xa9193231, 0xd1bc6a9d, 0x4144b3df,
		0x720502b0, 0x581dbac1, 0xbfad7147, 0xbb981b06,
		0x78ce9662, 0x4548444a, 0xc9d8b043, 0x25970052,
		0xc8dd6ba2, 0xf55489af, 0xc2cfc428, 0x1dbdf335,
		0xb84c18ae, 0xe2def46f, 0x195eebdc, 0xaec5b194,
		0xeec3ac5d, 0x2f53671e, 0xcc88ef88, 0x72558134,
		0x57b92bd3, 0x5dcd1e19, 0xeafb0f8e, 0xd61c738f,
		0x163f86a5, 0x46b1ce03, 0xa7e35cd4, 0xd7652eae,
		0x563cc1c8, 0xa3db0087, 0xc3849277, 0xd778a626,
		0xc7ef4812, 0xbb05328e, 0x3692fd5f, 0x6015a806,
		0xe381391e, 0x863526f3, 0x9ab95bbc, 0xad1d6612,
		0xe5cf8ad1, 0x49820839, 0x4d647831, 0xc1a7211d,
		0xc6ac83dd, 0x1ad15db6, 0xb8433049, 0x537764a1,
		0x3653cc2e, 0x28f0ab60, 0x5d6f9e6e, 0x767d3adc,
		0x693d9770, 0xede7d051, 0x294cf882, 0x890ca1f1,
	},
};

/* RSA CRT key material the package signatures are made and unwrapped with. */
const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageRetailRsaKey = {
	.primeP = {
		0x3f579c7d, 0x30b2bf19, 0x892f9ba8, 0x1e66865d,
		0xd6d35fe6, 0x9e5ac7b1, 0x3db81ab2, 0xc7464070,
		0xeeafad74, 0x8dcfc5d7, 0x98114a81, 0xd39d3307,
		0xb3302cce, 0xe46f0ab0, 0xeae682bf, 0xa5589fae,
		0x41d30e6e, 0x8a0c87c0, 0x775ca3a3, 0x3fdac941,
		0x0d2baba8, 0xfb5b56f5, 0x3950fe55, 0xeb352e9f,
		0x367c1790, 0x09602465, 0x20354a86, 0x32835ea0,
		0xc1702c1b, 0x82ae6a37, 0x6b344543, 0xa1878b98,
	},
	.primeQ = {
		0x9a39b74b, 0xb3d19f49, 0xacb223f8, 0xdcaba78c,
		0xa468b436, 0xcc05fcef, 0x2df1e9c9, 0x3af9636b,
		0xc775015e, 0xa53fd614, 0xe6ad550b, 0x4c8ed511,
		0x47de1e49, 0x02289d9b, 0x821d09cb, 0x8eb698e9,
		0xf0efc600, 0x46dde9a0, 0x39dff79c, 0xcdc81121,
		0xcac8d205, 0x5e0c791f, 0x97354b08, 0x1a37b49e,
		0x3af6ccb8, 0x39c42728, 0xc737ef7a, 0xffec8640,
		0xd21188d9, 0x325e4624, 0x3cb832e4, 0xcfb6363a,
	},
	.exponentP = {
		0x18354419, 0x66e9b251, 0x6b58cba6, 0x888bca92,
		0xe9be829d, 0xb3872052, 0x43af5ff3, 0x58faa811,
		0x9ca89ea3, 0x7eaf511f, 0xbd44d8c4, 0x892b1398,
		0xe1b40483, 0xb8cdff86, 0xc88dde7c, 0x72ac52cf,
		0xe69eab7b, 0x10a8ee7a, 0xfc318f60, 0x5f3e0c12,
		0x7b0577d5, 0x1819f868, 0xf9db0e00, 0x38a7db3b,
		0x0b1b2401, 0x6a964000, 0xdba24290, 0x53a61e34,
		0x62ffbd82, 0x27fd3b0f, 0x3a4d941a, 0x3018a60e,
	},
	.exponentQ = {
		0xd6e23661, 0x8a87dc88, 0x6db9ff31, 0x8423b88a,
		0xf6d6d3e5, 0x2d093f7f, 0x17669fd4, 0x491455bf,
		0x9277e3c2, 0x62b4a813, 0x854023e8, 0x366d2fe3,
		0x79450abc, 0x5472b9a1, 0x368b816d, 0x5be5122c,
		0xaf104840, 0x2489edef, 0xfbfc75c5, 0x14dd6b21,
		0x3f4cf64d, 0xdf9d9d27, 0x6dc02e9d, 0x108aedc4,
		0x02da88b8, 0x6a87385c, 0xfd7eb286, 0xeadb844a,
		0x77e5ea83, 0x633539f4, 0x7bf91e01, 0x0608d52a,
	},
	.coefficient = {
		0x055999a3, 0x688a92a7, 0xc4643009, 0x236b4132,
		0xe552aa9a, 0xc3ac9239, 0x335ab621, 0xc7f14d15,
		0x26d19dd8, 0x960e3f77, 0xf3da2cbb, 0xbe105401,
		0x634386ec, 0x7e066ee9, 0x2da63b0a, 0x8af713d4,
		0x80ef722e, 0x138b1b3b, 0xbdfc560f, 0x303d1419,
		0x6529b0b4, 0x54705e25, 0x08bb2d2d, 0x67a01b60,
		0x011d5b23, 0x0e210a17, 0x60cbf992, 0x071095a4,
		0xd46e866c, 0x02168696, 0xee16c69c, 0x406bf63e,
	},
};

const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageDebugRsaKey = {
	.primeP = {
		0xe7f76073, 0x40edb893, 0x30797c74, 0x5f5751ae,
		0xf053c3fb, 0xdffe4048, 0x21ce9669, 0xedcf15e7,
		0x32a2652c, 0xe77686ba, 0xcc01203e, 0xa6e57d55,
		0x2422c88c, 0x5d179a2c, 0x4be78cef, 0xe215790e,
		0xe0e7b99f, 0x2e16cdec, 0x57814ca5, 0x6d9877e9,
		0x5cb7d771, 0x3eb44e4b, 0xf551938c, 0x2312ab5c,
		0x97653a01, 0x9a68e2e6, 0x35745037, 0xf906a041,
		0xf02cf2c9, 0x7e61d7ec, 0x182f9d6d, 0xa5728154,
	},
	.primeQ = {
		0xc5cb2ecb, 0xc6e09c7a, 0xafba52ae, 0xfe268339,
		0x2439ec31, 0x07663f8e, 0x482ea463, 0x5b7b7c89,
		0xfd7cd294, 0x877ce6b3, 0xdb7df56a, 0x964f08e1,
		0x4713a301, 0x3d8561a3, 0xa7eb7930, 0x92cd10ce,
		0xfe0f499c, 0xf030941c, 0x463d9157, 0x6c37af1e,
		0x3c744da2, 0xe2f62d5e, 0x34841990, 0x621ff088,
		0x1ab21eaa, 0x797b7f08, 0x35c06809, 0xaf09f99d,
		0x2f9a02b1, 0xf55b860b, 0x99ba3f5a, 0xd4b1af1f,
	},
	.exponentP = {
		0xb9a74c5d, 0x08514215, 0xa725a2a1, 0x587c73a6,
		0x2f656377, 0xdeb78917, 0x4a85d53f, 0xfdf6475f,
		0x3cd00a06, 0x6d73606f, 0xc09ab295, 0x717dd35d,
		0x1a51e981, 0x895b4249, 0xa585741a, 0xeaa4efc5,
		0xad8b77a8, 0x2c4d3ab6, 0xa2d67eca, 0x697e7c38,
		0x7fcf0e2b, 0x4ca60840, 0x74342609, 0xa4d1f909,
		0x5f9cb3bb, 0x3c4e7572, 0xe320fd33, 0x874959cc,
		0x78605c04, 0x5fe0a341, 0x689a656c, 0x4fe3d0eb,
	},
	.exponentQ = {
		0xac911e09, 0x72be2533, 0xbdd76c0c, 0x1595dcda,
		0xd39614c9, 0x8310d3a3, 0x9475d9af, 0x84d8e684,
		0xc28b220f, 0x923262f3, 0x8a3beddd, 0x0a25fd2b,
		0x04323e61, 0x02618498, 0xdb338d7e, 0x965524f1,
		0xdc1d39b0, 0xdd308373, 0xeea26df8, 0x9cbef848,
		0x136a7f1c, 0xdbe29459, 0x69ee3dbc, 0x4bf02bfe,
		0xc4eb2542, 0x8c61f80a, 0x5da59987, 0xd0c07791,
		0x67660a06, 0x905a908c, 0x4e925b1c, 0x5a502880,
	},
	.coefficient = {
		0xdeccca00, 0x4fe35263, 0xb0e3c45a, 0x27da4a06,
		0x086b2847, 0x846ebc5d, 0xfd2d56d7, 0x22a1abec,
		0x8878db19, 0x3783fe4f, 0xeaeab9dc, 0x96f6bc51,
		0x1c2d8a76, 0x4a15a8bb, 0x4f1de956, 0x60249035,
		0x28f7efa1, 0x57794904, 0xeec46ce6, 0x6bbd309b,
		0xa4fb3bc3, 0x9c646a7e, 0xed0066ef, 0x4e61a27e,
		0x38b4ce0b, 0xd0d6a520, 0x7383563e, 0xb52ae1aa,
		0xc049758e, 0x62daa510, 0x0afe1c76, 0x02532212,
	},
};

const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageSigningRsaKey = {
	.primeP = {
		0x8dcdaa61, 0xf5fbcf19, 0xe1f677b6, 0xa4e48589,
		0xc30864fe, 0x03fa6dba, 0x442afa44, 0x71b320b1,
		0xbc407467, 0x05f51b0f, 0xc7c50f45, 0x0c363896,
		0x1b0ecbe6, 0x17059394, 0x27a9b12c, 0xadfff18f,
		0xbf6bb173, 0x50caaaca, 0xdb4b0d5b, 0x134deb30,
		0xd46f0541, 0x2f0c6b6a, 0xbff814df, 0x92764528,
		0x55091812, 0x907c5184, 0x52aceb08, 0xdeb1ce9d,
		0x5e1aaee1, 0xf079c46f, 0xa2c6d2f0, 0xb946286f,
	},
	.primeQ = {
		0x6c2be867, 0x93602654, 0x6b3c7839, 0xdd36ef6b,
		0xb82c5968, 0xcf12c3ad, 0x3604e5f0, 0xd4c75058,
		0xe134e684, 0x5a8fec4c, 0xd61b74d6, 0x972d9c43,
		0x3752bfea, 0xd0a226a4, 0x42d62598, 0xca37cf71,
		0x4ecd05be, 0x4819d592, 0xe5355194, 0x4440a14e,
		0x0b365ab3, 0x1bf6c350, 0x12c09cb8, 0x08cd8535,
		0xfbac4ec5, 0x7e204da9, 0x3047b17a, 0x2c59d248,
		0xa4a5c447, 0x27781d97, 0xc186e807, 0xc5df25ed,
	},
	.exponentP = {
		0x7abde8c1, 0xd5c9570d, 0x8441bb62, 0x709cb06d,
		0xbe3e66d6, 0x4e74e657, 0xae139285, 0xa167ba64,
		0xeb9e6090, 0x0c54a864, 0xef8b9c00, 0x20be5c62,
		0x8fa4cb35, 0x32106f72, 0x3d138c41, 0x07f79099,
		0x7db9bcf0, 0x04d1ce20, 0x02a82cbb, 0x3c0a11df,
		0xe74ea7ef, 0x7b58f6ca, 0x5d3140b5, 0xc6d83dc6,
		0x5fcad3e0, 0x15e49af6, 0x1a356988, 0x19572c0e,
		0x1a4a3a41, 0x6b746a15, 0x075a459c, 0x2c0fd55a,
	},
	.exponentQ = {
		0x063f8703, 0x4fb16c3b, 0x081191e7, 0xdbbdab26,
		0x4be6eb17, 0x524566b7, 0x77841d2f, 0xfca36ccd,
		0xe51fdfd4, 0x22052a96, 0xcff22d0a, 0xfd0fcabc,
		0xf1a21cd8, 0xef1a25a2, 0x58c7de97, 0xc2dcedd4,
		0x967758f2, 0x9d549afe, 0x03d9d0da, 0x9173af29,
		0xe6e38857, 0xda1e844a, 0x62f4939f, 0x3b343cc5,
		0x188e0963, 0xaaa0a8ae, 0xb945471a, 0xda884771,
		0x106d1563, 0x576f811a, 0x42c0841b, 0x4211e7bd,
	},
	.coefficient = {
		0x3832029e, 0x791d676f, 0xb856b832, 0xc5481fe1,
		0x9fc016ef, 0x958f72eb, 0x94bec081, 0x0d097f64,
		0x4f83226d, 0xe67767b6, 0xb37a0e68, 0x29249a42,
		0xb61c6fab, 0x9221fc43, 0x6d7e191a, 0x1c08f6ba,
		0x54f6b2a5, 0x1d4a8aff, 0x1c13eecc, 0xd5de4a06,
		0xaa68b518, 0x68ec0bd0, 0x8d3bf191, 0x04e17667,
		0xccc6470e, 0xa4699f6a, 0x18c7fccc, 0xa27873b2,
		0xad777285, 0xe6faa125, 0xd68e4f4c, 0x8359e4de,
	},
};

const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageEncryptedKeyRsaKey = {
	.primeP = {
		0xfe8c4463, 0xe49b3ae3, 0xe0facabf, 0x21a02003,
		0xbc6af94e, 0xd0a57673, 0xcd6a9399, 0x33b2fe10,
		0xf2eebfb4, 0xfe21e09f, 0xe7814746, 0xc3c94e89,
		0x662218eb, 0x591c3584, 0x7e008fdf, 0x9a4aad54,
		0xf6f89383, 0x981d2f74, 0x11b801dc, 0xceaa4dbc,
		0x9225619a, 0x417c041f, 0x714af590, 0x84119161,
		0x9bca99a3, 0x037b1dad, 0xd89cc4a9, 0x152064a2,
		0x985afd23, 0x25c9d4fd, 0xdfecb580, 0xa275011f,
	},
	.primeQ = {
		0xf2d9262d, 0xbd77a955, 0x9e188525, 0x1e71212c,
		0x949bb303, 0x7d02e5fd, 0x725f5763, 0xa99f98be,
		0xea76f0a6, 0x4a8ffd21, 0x2b95161d, 0x8cedf0c5,
		0x0219383a, 0xcc48effb, 0xbac9a0b6, 0x0c397412,
		0xa005cd78, 0x53ebb8ac, 0x1bf43756, 0x6dcc3fe3,
		0xb5d04391, 0x43a7d686, 0xfff98f16, 0xfb7551a2,
		0x61fa73cc, 0x488f356e, 0x9fd4b73c, 0x52604528,
		0xed408d8d, 0x0c672075, 0x796afc31, 0xe4c75da5,
	},
	.exponentP = {
		0x1b36fcbd, 0xdf14d08f, 0xcf240028, 0x07a5ccf5,
		0x750c85b4, 0xbc8949cb, 0xc6e86ca3, 0x818d755f,
		0x9dea917c, 0x6e2ecbd4, 0x2e03e8a7, 0x1b4cf008,
		0x52184ff0, 0x5c87c381, 0x0d8405a6, 0x2f306c36,
		0x0f47df92, 0xa42d3692, 0x1bf19c78, 0x2c4e2f38,
		0xb0f1dda4, 0xf4a4b17d, 0x07e26d64, 0xe6708cd8,
		0x46d95df4, 0xbc3f0093, 0x64d3eaa5, 0xf2efdef4,
		0x9ea0bc0f, 0x3a5f12c0, 0x6d88a03b, 0xa1a8084c,
	},
	.exponentQ = {
		0xf8d58e8d, 0x7c97e1b0, 0x977bc7f5, 0xc5afc624,
		0xea7d74bc, 0xfed7c020, 0x1e679ee4, 0x3e25993d,
		0x657387a6, 0x2ee9a30f, 0xb23338b1, 0xac8aab89,
		0x9bf18b8d, 0xf54cee6d, 0x8729223a, 0x9668aaf4,
		0x570d7d74, 0x8462bace, 0x8893f907, 0xf6acbcc6,
		0x59976865, 0x056eddb6, 0x55d4378d, 0x73beb4a4,
		0x994f3cfc, 0x7dca764a, 0x933199e4, 0x6db489f3,
		0xa13c7bce, 0x4d6b4f49, 0xf6cc3244, 0x56f07064,
	},
	.coefficient = {
		0x44b67962, 0xd73bfa61, 0xcbeddfd6, 0x94b7f614,
		0x7b0e1360, 0xbdfef7c8, 0xf987b1b8, 0x6e0174fb,
		0x25da1983, 0xc8ce8a0c, 0x07525662, 0xa92fce19,
		0xa41681bd, 0x0386d786, 0xa62b051a, 0xa4c8121b,
		0x17e7d2dd, 0x2c183f38, 0x045680f5, 0xc10304c7,
		0x6f5b08c0, 0x83aed02c, 0x3f690e9a, 0x521abb5a,
		0x311c5fd2, 0x65e942c3, 0xa5fad6d1, 0xeb759bcf,
		0x1bfe3d94, 0x3ebd3f4e, 0xb4f39cb9, 0x93d10539,
	},
};

char gNpDrmPackageContentId[48];
struct NpDrmPackageContext gNpDrmPackageContexts[6];
const struct NpDrmPackageRsaCrtMaterial *gNpDrmPackageUnwrapKey;
const struct NpDrmPackageRsaCrtMaterial *gNpDrmPackageSignKey;
SceAesCompactContext gNpDrmPackageAesWorkspaces[6];
int gNpDrmPackageGameExist;

int NpDrmPackageCheckDebugSignature(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	const unsigned char *end;
	volatile unsigned int difference;

	if (size != 20)
		return 0x80871004;
	end = digest + 20;
	difference = 0;
	do {
		difference += (unsigned char)(*signature++ - *digest++);
		difference += (unsigned char)(*signature++ - *digest++);
		difference += (unsigned char)(*signature++ - *digest++);
		difference += (unsigned char)(*signature++ - *digest++);
	} while (digest != end);
	return difference ? 0x80871004 : 0;
}

int NpDrmPackageInitializeContext(struct NpDrmPackageContext *context,
	const struct NpDrmPackageHeader *header, unsigned int size)
{
	struct {
		unsigned char bytes[16];
	} key;
	unsigned char *destination;
	unsigned char *wipe;
	unsigned char *wipeEnd;
	const unsigned char *source;
	unsigned int keyId = (header->extended.keyId >> 24) & 7;

	switch (keyId) {
	case 1:
		key = (typeof(key)) { {
			0x07, 0xf2, 0xc6, 0x82, 0x90, 0xb5, 0x0d, 0x2c,
			0x33, 0x81, 0x8d, 0x70, 0x9b, 0x60, 0xe6, 0x2b,
		} };
		break;
	case 2:
		key = (typeof(key)) { {
			0xe3, 0x1a, 0x70, 0xc9, 0xce, 0x1d, 0xd7, 0x2b,
			0xf3, 0xc0, 0x62, 0x29, 0x63, 0xf2, 0xec, 0xcb,
		} };
		break;
	case 3:
		key = (typeof(key)) { {
			0x42, 0x3a, 0xca, 0x3a, 0x2b, 0xd5, 0x64, 0x9f,
			0x96, 0x86, 0xab, 0xad, 0x6f, 0xd8, 0x80, 0x1f,
		} };
		break;
	case 4:
		key = (typeof(key)) { {
			0xaf, 0x07, 0xfd, 0x59, 0x65, 0x25, 0x27, 0xba,
			0xf1, 0x33, 0x89, 0x66, 0x8b, 0x17, 0xd9, 0xea,
		} };
		break;
	}
	if (keyId == 2 || keyId == 3 || keyId == 4) {
		NpDrmPackageAesInitialize(context->aesContext, key.bytes, sizeof(key));
		NpDrmPackageAesEncrypt(context->aesContext, key.bytes,
			header->main.initialVector);
	}
	NpDrmPackageAesInitialize(context->aesContext, key.bytes, sizeof(key));
	for (destination = context->key, source = key.bytes;
	     destination < context->key + sizeof(context->key);)
		*destination++ = *source++;
	for (destination = (unsigned char *)context->counter,
	     source = header->main.initialVector;
	     destination < (unsigned char *)(context->counter + 2);)
		*destination++ = *source++;
	wipeEnd = key.bytes + sizeof(key);
	for (wipe = key.bytes; wipe < wipeEnd; wipe++)
		*wipe = 0xff;
	for (wipe = key.bytes; wipe < wipeEnd; wipe++)
		*wipe = 0;
	return 0;
}

int NpDrmPackageInitializeCounter(struct NpDrmPackageContext *context,
	const struct NpDrmPackageHeader *header, unsigned int size)
{
	unsigned char counter[16] = { 0 };
	unsigned char *destination;
	const unsigned char *source;
	unsigned int keyId;

	for (destination = counter, source = header->main.digest;
	     destination < counter + sizeof(counter);)
		*destination++ = *source++;
	keyId = (header->extended.keyId >> 24) & 7;
	if (keyId == 2 || keyId == 3) {
		NpDrmPackageAesInitialize(context->aesContext,
			header->main.digest, sizeof(counter));
		NpDrmPackageAesEncrypt(context->aesContext, counter, counter);
	}
	for (destination = (unsigned char *)context->counter, source = counter;
	     destination < (unsigned char *)(context->counter + 2);)
		*destination++ = *source++;
	return 0;
}

int NpDrmPackageCryptData(struct NpDrmPackageContext *context,
	void *buffer, unsigned int size, uint64_t offset, unsigned int flags)
{
	unsigned char *data = buffer;
	uint64_t counter[2];
	unsigned char stream[16];
	uint64_t base;
	unsigned int count;
	unsigned int index;
	int result;

	if (flags & 0x8000) {
		uint64_t end;

		counter[0] = __builtin_bswap64(context->counter[1]) + offset;
		counter[1] = __builtin_bswap64(context->counter[0]);
		end = counter[0] + size;
		if (end != 0 && end < counter[0]) {
			count = size - (unsigned int)end;
			result = NpDrmPackageAesCtr(data, data, count,
				context->key, 128, counter);
			if (result < 0)
				return result;
			data += count;
			size = end;
			counter[0] = 0;
			counter[1] = __builtin_bswap64(context->counter[0]);
		}
		return NpDrmPackageAesCtr(data, data, size,
			context->key, 128, counter);
	}
	for (index = 0; index < sizeof(counter); index++)
		((unsigned char *)counter)[index] =
			((const unsigned char *)context->counter)[index];
	base = __builtin_bswap64(counter[1]);
	while (size != 0) {
		counter[1] = __builtin_bswap64(base + offset);
		NpDrmPackageAesEncrypt(context->aesContext, stream, counter);
		count = size;
		if (count >= sizeof(stream))
			count = sizeof(stream);
		offset++;
		for (index = 0; index < count; index++)
			data[index] ^= stream[index];
		size -= count;
		data += count;
	}
	return 0;
}

int NpDrmPackageInitializePrivateContext(struct NpDrmPackageContext *context,
	const struct NpDrmPackageHeader *header, unsigned int size)
{
	const unsigned char *data = (const void *)header;
	const struct NpDrmPackageMetadataRecord *record;
	const struct NpDrmPackageEncryptedKeyMetadata *encryptedKey = NULL;
	unsigned char counter[16] = { 0 };
	unsigned char key[32] = { 0 };
	unsigned char decrypted[256];
	MarlinRsaPrivateKey privateKey;
	MarlinRsaData output, input;
	unsigned int count, metadataOffset, metadataEnd, recordEnd;
	unsigned int offset, extent, fullHeaderHmacOffset, i;

	for (i = 0; i < sizeof(counter); i++)
		counter[i] = header->main.digest[i];
	if (!(header->extended.keyId & 0x40))
		return 0x80871020;
	fullHeaderHmacOffset = NPDRM_BE32(header->extended.fullHeaderHmacOffset);
	if (!fullHeaderHmacOffset)
		return 0x80871020;
	count = NPDRM_BE32(header->main.metadataCount);
	if (!count)
		return 0x80871020;
	metadataOffset = NPDRM_BE32(header->main.metadataOffset);
	metadataEnd = metadataOffset + NPDRM_BE32(header->main.metadataSize) - 64;
	record = (const void *)(data + metadataOffset);
	for (;;) {
		recordEnd = (unsigned int)((const unsigned char *)record - data) +
			sizeof(*record) + NPDRM_BE32(record->size);
		if (metadataEnd < recordEnd)
			return 0x80871003;
		if (NPDRM_BE32(record->type) == 15)
			encryptedKey = (const void *)record->payload;
		if (--count == 0)
			break;
		record = (const void *)(data + recordEnd);
	}
	if (!encryptedKey)
		return 0x80871020;
	offset = NPDRM_BE32(encryptedKey->offset);
	extent = offset + NPDRM_BE32(encryptedKey->size);
	if (size < extent || extent > fullHeaderHmacOffset)
		return 0x80871020;
	privateKey.primeP = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.primeP;
	privateKey.primeQ = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.primeQ;
	privateKey.exponentP = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.exponentP;
	privateKey.exponentQ = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.exponentQ;
	privateKey.coefficient = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.coefficient;
	output = (MarlinRsaData) { decrypted, 256 };
	input = (MarlinRsaData) { (unsigned char *)data + offset + 256, 256 };
	if (NpDrmRsaDecrypt2048(&output, &input, &privateKey) < 0 || output.size != 32)
		return 0x8087101f;
	for (i = 0; i < sizeof(key); i++)
		key[i] = output.data[i];
	NpDrmPackageAesInitialize(context->aesContext, key, 16);
	NpDrmPackageAesEncrypt(context->aesContext, counter, header->main.digest);
	NpDrmPackageAesInitialize(context->aesContext, counter, 16);
	for (i = 0; i < sizeof(context->key); i++)
		context->key[i] = counter[i];
	for (i = 0; i < sizeof(context->counter); i++)
		((unsigned char *)context->counter)[i] = counter[i];
	return 0;
}

int NpDrmPackageVerifyMainHeader(unsigned char *data, unsigned int size,
	const struct NpDrmPackageExtendedHeader *header)
{
	MarlinRsaExponentKey key;
	MarlinRsaData signatureData;
	MarlinRsaData digestData;
	unsigned int exponent[64];
	unsigned int digest[8];
	unsigned int offset = header->fullHeaderHmacOffset;
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (offset + 384 <= size) {
		unsigned char *signature = data + offset;

		sceSha256Digest(data, offset, digest);
		memset(exponent, 0, sizeof(exponent));
		key.modulus = (unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_VITA];
		key.exponent = exponent;
		exponent[0] = 65537;
		signatureData = (MarlinRsaData) { signature, 256 };
		digestData = (MarlinRsaData) { (unsigned char *)digest, 20 };
		result = NpDrmRsaVerify2048(&signatureData, &digestData,
			&key, 5) >= 0;
	}
	return result;
}


int NpDrmPackageVerifyRsaSignature(const unsigned char *signature,
	const unsigned char *digest, unsigned int *modulus)
{
	MarlinRsaExponentKey key;
	MarlinRsaData signatureData, digestData;
	unsigned int exponent[64];
	int signatureSize = sizeof(exponent);

	memset(exponent, 0, signatureSize);
	key = (MarlinRsaExponentKey) { modulus, exponent };
	exponent[0] = 65537;
	signatureData = (MarlinRsaData) { (unsigned char *)signature, signatureSize };
	digestData = (MarlinRsaData) { (unsigned char *)digest, 20 };
	return NpDrmRsaVerify2048(&signatureData, &digestData, &key, 5) >= 0;
}

int NpDrmPackageCheckRsaSignature(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	if (size != 384)
		return 0x80871004;
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_ALTERNATE]) ?
		0 : 0x80871004;
}

int NpDrmPackageVerifyLiveAreaHeader(unsigned char *data,
	unsigned int size, const struct NpDrmPackageExtendedHeader *header)
{
	unsigned int offset = header->fullHeaderHmacOffset;
	unsigned char digest[32];
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (size >= offset + 384) {
		unsigned char *signature = data + offset;

		sceSha256Digest(data, offset, digest);
		result = NpDrmPackageVerifyRsaSignature(signature, digest,
			(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA]);
	}
	return result;
}

int NpDrmPackageVerifyAlternateSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_ALTERNATE]);
}

int NpDrmPackageCheckLiveAreaSignature(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	if (size != 384)
		return 0x80871004;
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA]) ?
		0 : 0x80871004;
}

int NpDrmPackageVerifyLiveAreaSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA]);
}

int NpDrmPackageVerifyPspSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	MarlinRsaExponentKey key;
	MarlinRsaData signatureData = { (unsigned char *)signature, 256 };
	MarlinRsaData digestData = { (unsigned char *)digest, 20 };
	unsigned int exponent[64];

	memset(exponent, 0, sizeof(exponent));
	key.modulus = (unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_PSP];
	key.exponent = exponent;
	exponent[0] = 65537;
	return NpDrmRsaVerify2048(&signatureData, &digestData, &key, 5) >= 0;
}

int NpDrmPackageVerifyPspSignatures(const unsigned char *signature,
	const unsigned char *digest)
{
	if (SceSblGcAuthMgrPkgForDriver_E459A9A8((const char *)digest,
		(const char *)(signature + 256)) != 0)
		return 0;
	return NpDrmPackageVerifyPspSignature(signature, digest);
}

int NpDrmPackageCheckPspSignatures(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	if (size != 384)
		return 0x80871004;
	if (SceSblGcAuthMgrPkgForDriver_E459A9A8((const char *)digest,
		(const char *)(signature + 256)) != 0)
		return 0x80871004;
	return NpDrmPackageVerifyPspSignature(signature, digest) ?
		0 : 0x80871004;
}

int NpDrmPackageVerifyAlternateHeader(unsigned char *data,
	unsigned int size, const struct NpDrmPackageExtendedHeader *header)
{
	unsigned int offset = header->fullHeaderHmacOffset;
	unsigned char digest[32];
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (size >= offset + 384) {
		unsigned char *signature = data + offset;

		sceSha256Digest(data, offset, digest);
		result = NpDrmPackageVerifyRsaSignature(signature, digest,
			(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_ALTERNATE]);
	}
	return result;
}

int NpDrmPackageCryptDebugData(struct NpDrmPackageContext *context,
	void *buffer, unsigned int size, uint64_t offset, unsigned int flags)
{
	struct NpDrmPackageDebugSeed seed;
	unsigned char digest[20];
	unsigned char *data = buffer;
	unsigned char *cursor = (unsigned char *)&seed;
	unsigned int skip;

	(void)flags;
	while (cursor < (unsigned char *)(&seed + 1))
		*cursor++ = 0;
	seed = *(const struct NpDrmPackageDebugSeed *)((const unsigned char *)context
		+ 0x18);
	skip = seed.hashSuffix[23] & 0x10;
	seed.blockCounter = __builtin_bswap64(offset);
	while (size) {
		unsigned int length = size < 16 ? size : 16;
		unsigned int i;

		sceSha1Digest((unsigned char *)&seed + skip,
			sizeof(seed) - skip, digest);
		seed.blockCounter = __builtin_bswap64(
			__builtin_bswap64(seed.blockCounter) + 1);
		for (i = 0; i < length; i++)
			data[i] ^= digest[i];
		data += length;
		size -= length;
	}
	return 0;
}

int NpDrmPackageVerifyDebugSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	union {
		uint64_t word[2];
		struct {
			unsigned char front[13];
			unsigned char tail[3];
		} bytes;
	} rotated;
	struct NpDrmPackageDebugSeed seed;
	unsigned char output[16];
	unsigned char hash[20];
	unsigned char *cursor = (unsigned char *)&seed;
	volatile unsigned int difference = 0;
	unsigned int skip;
	unsigned int i;

	for (i = 0; i < sizeof(rotated.bytes.front); i++)
		rotated.bytes.front[i] = digest[i + 4];
	for (i = 0; i < sizeof(rotated.bytes.tail); i++)
		rotated.bytes.tail[i] = digest[i + 1];
	while (cursor < (unsigned char *)(&seed + 1))
		*cursor++ = 0;
	seed.firstDigest[0] = rotated.word[0];
	seed.firstDigest[1] = rotated.word[0];
	seed.secondDigest[0] = rotated.word[1];
	seed.secondDigest[1] = rotated.word[1];
	seed.blockCounter = 0;
	for (i = 0; i < sizeof(output); i++)
		output[i] = 0;
	skip = seed.hashSuffix[23] & 0x10;
	sceSha1Digest((unsigned char *)&seed + skip,
		sizeof(seed) - skip, hash);
	seed.blockCounter = __builtin_bswap64(
		__builtin_bswap64(seed.blockCounter) + 1);
	for (i = 0; i < sizeof(output); i++)
		output[i] ^= hash[i];
	for (i = 0; i < sizeof(output); i++)
		difference += (unsigned char)(signature[i] - output[i]);
	return difference == 0;
}

int NpDrmPackageVerifyDebugHeader(unsigned char *data,
	unsigned int size, const struct NpDrmPackageExtendedHeader *header)
{
	unsigned int offset = header->fullHeaderHmacOffset;
	unsigned char digest[20];
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (size >= offset + 384) {
		sceSha1Digest(data, offset, digest);
		result = NpDrmPackageVerifyDebugSignature(data + offset, digest);
	}
	return result;
}

static const struct NpDrmPackageOperations gNpDrmPackageEncryptedDebugOperations = {
	NpDrmPackageVerifyDebugSignature, NpDrmPackageVerifyDebugHeader,
	NpDrmPackageCheckDebugSignature, NpDrmPackageInitializePrivateContext,
	NpDrmPackageCryptData
};
static const struct NpDrmPackageOperations gNpDrmPackageDebugOperations = {
	NpDrmPackageVerifyDebugSignature, NpDrmPackageVerifyDebugHeader,
	NpDrmPackageCheckDebugSignature, NpDrmPackageInitializeCounter,
	NpDrmPackageCryptDebugData
};
static const struct NpDrmPackageOperations gNpDrmPackagePspOperations = {
	NpDrmPackageVerifyPspSignatures, NpDrmPackageVerifyMainHeader,
	NpDrmPackageCheckPspSignatures, NpDrmPackageInitializeContext,
	NpDrmPackageCryptData
};
static const struct NpDrmPackageOperations gNpDrmPackageLiveAreaOperations = {
	NpDrmPackageVerifyLiveAreaSignature, NpDrmPackageVerifyLiveAreaHeader,
	NpDrmPackageCheckLiveAreaSignature, NpDrmPackageInitializeContext,
	NpDrmPackageCryptData
};
static const struct NpDrmPackageRsaCrtMaterial *const gNpDrmPackageRetailRsaKeys[] = {
	&gNpDrmPackageRetailRsaKey, &gNpDrmPackageSigningRsaKey
};
static const struct NpDrmPackageRsaCrtMaterial *const gNpDrmPackageDebugRsaKeys[] = {
	&gNpDrmPackageDebugRsaKey, &gNpDrmPackageSigningRsaKey
};
static const struct NpDrmPackageOperations gNpDrmPackageAlternateOperations = {
	NpDrmPackageVerifyAlternateSignature, NpDrmPackageVerifyAlternateHeader,
	NpDrmPackageCheckRsaSignature, NpDrmPackageInitializeContext,
	NpDrmPackageCryptData
};

int NpDrmPackageProcessHeader(unsigned char *data, unsigned int size,
	const unsigned char *digest, unsigned int flags)
{
	const struct NpDrmPackageHeader *header = (const void *)data;
	const struct NpDrmPackageMainHeader *main;
	const struct NpDrmPackageExtendedHeader *extended;
	struct NpDrmPackageContext *context = NULL;
	const struct NpDrmPackageOperations *operations;
	unsigned char hash[20];
	unsigned int version, revision, metadataOffset, metadataSize;
	unsigned int headerSize, dataSize, hmacOffset, keyId, offset, index;
	int finalize = (flags & 0x200) != 0 || digest != NULL;

	if (!data)
		return 0x80871800;
	main = &header->main;
	extended = &header->extended;
	if ((flags & 0x100) || finalize) {
		index = (unsigned char)flags;
		if (index > 5)
			return 0x8087101c;
		context = &gNpDrmPackageContexts[index];
		if (finalize) {
			int result;

			if (!digest)
				return 0x8087101e;
			if (!context->operations)
				return 0x80871800;
			result = context->operations->checkSignature(data, size, digest);
			memset(context->aesContext, 0, context->aesContextSize);
			*context = (struct NpDrmPackageContext){ NULL };
			return result;
		}
	}
	if (size < 0x8000)
		return 0x80871014;
	if (NPDRM_BE32(main->magic) != 0x7f504b47)
		return 0x80871003;
	revision = NPDRM_BE32((unsigned int)main->revision |
		((unsigned int)main->type << 16));
	version = revision & 0x3fffffff;
	if (!version || version > 2 || (revision & 0xc0000000) == 0xc0000000)
		return 0x80871003;
	metadataOffset = NPDRM_BE32(main->metadataOffset);
	if (metadataOffset <= 191)
		return 0x80871003;
	metadataSize = NPDRM_BE32(main->metadataSize);
	if (metadataSize - 64 > 0x8000 ||
		metadataSize - 64 < 8 * NPDRM_BE32(main->metadataCount))
		return 0x80871003;
	if (version == 1) {
		uint64_t dataOffset = NPDRM_BE64(main->dataOffset);
		unsigned int metadataEnd = metadataOffset + metadataSize;

		if (dataOffset == metadataEnd)
			return NPDRM_BE64(main->totalSize) >= metadataEnd + 96ULL +
				NPDRM_BE64(main->dataSize) ? 0x80871007 : 0x80871003;
		if (metadataEnd >= dataOffset)
			return 0x80871007;
	}
	if (NPDRM_BE32(extended->magic) != 0x7f657874)
		return 0x80871003;
	dataSize = NPDRM_BE32(extended->dataSize);
	if (dataSize < 384 || dataSize > 0x8000)
		return 0x80871003;
	headerSize = NPDRM_BE32(extended->headerSize);
	if (headerSize > 0x8000)
		return 0x80871003;
	hmacOffset = NPDRM_BE32(extended->mainHeaderHmacOffset);
	if (hmacOffset > 0x8000 || hmacOffset != headerSize + 192 ||
		metadataOffset < hmacOffset + dataSize ||
		metadataOffset + metadataSize != NPDRM_BE32(extended->metadataHmacOffset) ||
		NPDRM_BE64(main->dataOffset) < metadataOffset + metadataSize + dataSize ||
		NPDRM_BE64(main->totalSize) < dataSize + 32ULL + NPDRM_BE64(extended->tailOffset) ||
		hmacOffset + 384 > 0x8000)
		return 0x80871003;
	if (!NpDrmPackageIsHardwareAllowed(revision >> 31))
		return 0x80871007;
	keyId = NPDRM_BE32(extended->keyId);
	operations = (int)keyId < 0 ? &gNpDrmPackageEncryptedDebugOperations :
		&gNpDrmPackageDebugOperations;
	if (revision & 0x80000000) {
		operations = &gNpDrmPackagePspOperations;
		if (keyId & 0x10000000)
			operations = &gNpDrmPackageLiveAreaOperations;
		if (keyId & 0x08000000)
			operations = &gNpDrmPackageAlternateOperations;
		if (((keyId & 0x10000000) != 0) != ((keyId & 7) == 3) ||
			((keyId & 0x08000000) != 0) != ((keyId & 7) == 4))
			return 0x80871003;
	}
	sceSha1Digest(data, hmacOffset, hash);
	if (!operations->verifySignature(data + hmacOffset, hash))
		return 0x80871004;
	if (((keyId >> 24) & 7) - 1 > 3)
		return 0x80871006;
	offset = NPDRM_BE32(extended->fullHeaderHmacOffset);
	if (offset) {
		if (offset + dataSize > 0x8000)
			return 0x80871003;
		if (!operations->verifyHeader(data, size, extended))
			return 0x80871004;
	} else {
		offset = NPDRM_BE32(extended->metadataHmacOffset);
		if (offset + dataSize > 0x8000)
			return 0x80871003;
		sceSha1Digest(data + metadataOffset, metadataSize - 64, hash);
		if (!operations->verifySignature(data + offset, hash))
			return 0x80871004;
	}
	if (flags & 0x1000) {
		const struct NpDrmPackageRsaCrtMaterial *const *keys =
			(revision & 0x80000000) ? gNpDrmPackageRetailRsaKeys :
			gNpDrmPackageDebugRsaKeys;
		unsigned int i;

		gNpDrmPackageUnwrapKey = keys[0];
		gNpDrmPackageSignKey = keys[1];
		for (i = 0; i < sizeof(gNpDrmPackageContentId); i++)
			gNpDrmPackageContentId[i] = main->contentId[i];
	}
	if (context) {
		context->operations = operations;
		operations->initializeContext(context, header, size);
	}
	return 0;
}

unsigned int NpDrmPackageTransformData(unsigned char *data, unsigned int size,
	uint64_t offset, unsigned int flags)
{
	const struct NpDrmPackageRsaCrtMaterial *material;
	MarlinRsaPrivateKey privateKey;
	MarlinRsaData output, input;
	unsigned char hash[32];
	unsigned char decrypted[256];
	struct NpDrmPackageUnwrappedKey unwrapped;
	SceNpDrmLicense license;
	unsigned int exponent[64];
	MarlinRsaExponentKey publicKey;
	struct NpDrmPackageContext *context;
	unsigned int index, i;
	volatile unsigned int difference;
	int result;

	if (data == NULL)
		return 0x80871800;
	if (flags & 0x4000) {
		material = gNpDrmPackageUnwrapKey;
		if (!material)
			return 0x80871800;
		gNpDrmPackageUnwrapKey = NULL;
		if (size != 256)
			return 0x80871800;
		privateKey.primeP = (unsigned int *)material->primeP;
		privateKey.primeQ = (unsigned int *)material->primeQ;
		privateKey.exponentP = (unsigned int *)material->exponentP;
		privateKey.exponentQ = (unsigned int *)material->exponentQ;
		privateKey.coefficient = (unsigned int *)material->coefficient;
		output = (MarlinRsaData) { decrypted, 256 };
		input = (MarlinRsaData) { data, 256 };
		if (NpDrmRsaDecrypt2048(&output, &input, &privateKey) < 0 ||
			output.size != sizeof(unwrapped))
			return 0x8087101f;
		for (i = 0; i < sizeof(unwrapped); i++)
			((unsigned char *)&unwrapped)[i] = output.data[i];
		difference = 0;
		for (i = 0; i < sizeof(unwrapped.contentId); i++)
			difference += (unsigned char)(gNpDrmPackageContentId[i] -
				unwrapped.contentId[i]);
		if (difference) {
			memset(&unwrapped, 0, sizeof(unwrapped));
			return 0x8087101d;
		}
		memcpy(data, unwrapped.keyMaterial, sizeof(unwrapped.keyMaterial));
		memset(&unwrapped, 0, sizeof(unwrapped));
		return 0;
	}
	if (flags & 0x2000) {
		material = gNpDrmPackageSignKey;
		if (!material)
			return 0x80871800;
		gNpDrmPackageSignKey = NULL;
		if (size != sizeof(license))
			return 0x80871800;
		memcpy(&license, data, sizeof(license));
		license.version = -1;
		license.license_type = 256;
		license.version_flags = 256;
		license.license_flags = 0;
		license.account_id = 0;
		memcpy(license.content_id, gNpDrmPackageContentId,
			sizeof(license.content_id));
		memset(license.key_table, 0, 32);
		memset(license.ecdsa_signature, 0, sizeof(license.ecdsa_signature));
		memset(license.key2, 0, 32);
		memset(license.open_psid, 0, 60);
		sceSha256Digest(&license, 256, hash);
		privateKey.primeP = (unsigned int *)material->primeP;
		privateKey.primeQ = (unsigned int *)material->primeQ;
		privateKey.exponentP = (unsigned int *)material->exponentP;
		privateKey.exponentQ = (unsigned int *)material->exponentQ;
		privateKey.coefficient = (unsigned int *)material->coefficient;
		output = (MarlinRsaData) { (unsigned char *)license.rsa_signature, 256 };
		input = (MarlinRsaData) { hash, 32 };
		if (NpDrmRsaSign2048(&output, &input, &privateKey, 11) < 0) {
			memset(&license, 0, sizeof(license));
			return 0x8087101e;
		}
		memcpy(data, &license, sizeof(license));
		memset(&license, 0, sizeof(license));
		return 0;
	}
	if (flags & 0x20000) {
		if (size < sizeof(license))
			return 0x80871014;
		sceSha256Digest(data, 256, hash);
		memset(exponent, 0, sizeof(exponent));
		exponent[0] = 65537;
		publicKey.modulus = (unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA];
		publicKey.exponent = exponent;
		output = (MarlinRsaData) { data + 256, 256 };
		input = (MarlinRsaData) { hash, 20 };
		result = NpDrmRsaVerify2048(&output, &input, &publicKey, 5);
		return (result >> 31) & 0x8087101e;
	}
	index = flags & 0xff;
	if (index > 5)
		return 0x8087101c;
	context = &gNpDrmPackageContexts[index];
	if (!context->operations)
		return 0x80871800;
	return context->operations->cryptData(context, data, size, offset >> 4, flags);
}

int NpDrmPackageHashData(const void *data, unsigned int size,
	SceSblDmac5HashTransformContext *context, unsigned int flags)
{
	uint32_t digest[8];
	int result;

	if (data == NULL)
		return 0x80871800;
	digest[0] = digest[1] = digest[2] = digest[3] = 0;
	digest[4] = digest[5] = digest[6] = digest[7] = 0;
	if (flags & 0x10000) {
		result = NpDrmPackageHashTransform(data, digest, size, context, flags);
		context->state[0] = digest[0];
		context->state[1] = digest[1];
		context->state[2] = digest[2];
		context->state[3] = digest[3];
		context->state[4] = digest[4];
		context->state[5] = digest[5];
		context->state[6] = digest[6];
		context->state[7] = digest[7];
	} else {
		result = NpDrmPackageHashTransform(data, NULL, size, context, flags);
	}
	return result;
}

__attribute__((noinline)) int NpDrmPackageSetAesWorkspace(void *workspace,
	unsigned int stride,
	unsigned int count, unsigned int flags)
{
	unsigned int index;
	int result = workspace == NULL;

	if (count <= 5)
		result |= 1;
	if (result)
		return 0x80871800;
	index = flags & 0xff;
	if (index > 5)
		return 0x8087101c;
	gNpDrmPackageContexts[index].aesContext =
		(SceAesCompactContext *)((unsigned char *)workspace + stride * index);
	gNpDrmPackageContexts[index].aesContextSize = stride;
	return result;
}

#define NPDRM_ACT_INFO_BODY(_call) \
	struct NpDrmCheckActDataArgs args; \
	int *kernelActType = actType, *kernelVersionFlag = versionFlag; \
	uint64_t *kernelAccountId = accountId; \
	uint64_t *kernelStartTime, *kernelExpirationTime; \
	int ret, result; \
 \
	ARGS_IN(args, userArgs); \
	if (actType) { \
		kernelActType = __builtin_alloca(sizeof(*kernelActType)); \
		*kernelActType = 0; \
	} \
	if (versionFlag) { \
		kernelVersionFlag = __builtin_alloca(sizeof(*kernelVersionFlag)); \
		*kernelVersionFlag = 0; \
	} \
	if (accountId) { \
		kernelAccountId = __builtin_alloca(sizeof(*kernelAccountId)); \
		*kernelAccountId = 0; \
	} \
	kernelStartTime = args.startTime; \
	if (args.startTime) { \
		kernelStartTime = __builtin_alloca(sizeof(*kernelStartTime)); \
		*kernelStartTime = 0; \
	} \
	kernelExpirationTime = args.expirationTime; \
	if (args.expirationTime) { \
		kernelExpirationTime = __builtin_alloca(sizeof(*kernelExpirationTime)); \
		*kernelExpirationTime = 0; \
	} \
	result = _call(kernelActType, kernelVersionFlag, kernelAccountId, \
		kernelStartTime, kernelExpirationTime); \
	if (actType) { \
		ret = sceKernelCopyToUser(actType, kernelActType, sizeof(*actType)); \
		if (ret < 0) \
			return ret; \
	} \
	if (versionFlag) { \
		ret = sceKernelCopyToUser(versionFlag, kernelVersionFlag, sizeof(*versionFlag)); \
		if (ret < 0) \
			return ret; \
	} \
	if (accountId) { \
		ret = sceKernelCopyToUser(accountId, kernelAccountId, sizeof(*accountId)); \
		if (ret < 0) \
			return ret; \
	} \
	if (args.startTime) { \
		ret = sceKernelCopyToUser(args.startTime, kernelStartTime, \
			sizeof(*args.startTime)); \
		if (ret < 0) \
			return ret; \
	} \
	if (args.expirationTime) \
		return copyResult(result, sceKernelCopyToUser(args.expirationTime, \
			kernelExpirationTime, sizeof(*args.expirationTime))); \
	return result; \

int _sceNpDrmCheckActData(int *actType, int *versionFlag, uint64_t *accountId,
	const struct NpDrmCheckActDataArgs *userArgs)
{
	NPDRM_ACT_INFO_BODY(NpDrmCheckActData);
}

int _sceNpDrmRemoveActData(uint64_t *accountId)
{
	int ret;

	BLOCK_OUT(accountId, sizeof(*accountId), NpDrmRemoveActData(NULL),
		NpDrmRemoveActData((uint64_t *)k));
}

int _sceNpDrmGetRifInfo(const SceNpDrmLicense *license, unsigned int license_size,
	int check_sign, const struct NpDrmGetRifInfoArgs *userArgs)
{
	struct NpDrmGetRifInfoArgs args, kernel;
	SceNpDrmLicense *kernelLicense = NULL;
	int ret, result;

	ARGS_IN(args, userArgs);
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	kernel.content_id = args.content_id;
	if (args.content_id) {
		kernel.content_id = __builtin_alloca(48);
		__builtin_memset(kernel.content_id, 0, 48);
	}
	kernel.account_id = args.account_id;
	if (args.account_id) {
		kernel.account_id = __builtin_alloca(8);
		*kernel.account_id = 0;
	}
	kernel.license_version = args.license_version;
	if (args.license_version) {
		kernel.license_version = __builtin_alloca(4);
		*kernel.license_version = 0;
	}
	kernel.license_flags = args.license_flags;
	if (args.license_flags) {
		kernel.license_flags = __builtin_alloca(4);
		*kernel.license_flags = 0;
	}
	kernel.flags = args.flags;
	if (args.flags) {
		kernel.flags = __builtin_alloca(4);
		*kernel.flags = 0;
	}
	kernel.sku_flags = args.sku_flags;
	if (args.sku_flags) {
		kernel.sku_flags = __builtin_alloca(4);
		*kernel.sku_flags = 0;
	}
	kernel.lic_start_time = args.lic_start_time;
	if (args.lic_start_time) {
		kernel.lic_start_time = __builtin_alloca(8);
		*kernel.lic_start_time = 0;
	}
	kernel.lic_exp_time = args.lic_exp_time;
	if (args.lic_exp_time) {
		kernel.lic_exp_time = __builtin_alloca(8);
		*kernel.lic_exp_time = 0;
	}
	kernel.flags2 = args.flags2;
	if (args.flags2) {
		kernel.flags2 = __builtin_alloca(8);
		*kernel.flags2 = 0;
	}
	result = NpDrmGetRifInfo(kernelLicense, license_size, check_sign,
		kernel.content_id, kernel.account_id, kernel.license_version,
		kernel.license_flags, kernel.flags, kernel.sku_flags,
		kernel.lic_start_time, kernel.lic_exp_time, kernel.flags2);
	if (args.content_id) {
		ret = sceKernelCopyToUser(args.content_id, kernel.content_id, 48);
		if (ret < 0)
			return ret;
	}
	if (args.account_id) {
		ret = sceKernelCopyToUser(args.account_id, kernel.account_id, 8);
		if (ret < 0)
			return ret;
	}
	if (args.license_version) {
		ret = sceKernelCopyToUser(args.license_version, kernel.license_version, 4);
		if (ret < 0)
			return ret;
	}
	if (args.license_flags) {
		ret = sceKernelCopyToUser(args.license_flags, kernel.license_flags, 4);
		if (ret < 0)
			return ret;
	}
	if (args.flags) {
		ret = sceKernelCopyToUser(args.flags, kernel.flags, 4);
		if (ret < 0)
			return ret;
	}
	if (args.sku_flags) {
		ret = sceKernelCopyToUser(args.sku_flags, kernel.sku_flags, 4);
		if (ret < 0)
			return ret;
	}
	if (args.lic_start_time) {
		ret = sceKernelCopyToUser(args.lic_start_time, kernel.lic_start_time, 8);
		if (ret < 0)
			return ret;
	}
	if (args.lic_exp_time) {
		ret = sceKernelCopyToUser(args.lic_exp_time, kernel.lic_exp_time, 8);
		if (ret < 0)
			return ret;
	}
	if (args.flags2)
		return copyResult(result, sceKernelCopyToUser(args.flags2,
			kernel.flags2, 8));
	return result;
}

int _sceNpDrmGetRifName(char *rif_name, uint64_t aid)
{
	int ret;

	BLOCK_OUT(rif_name, 48, NpDrmGetRifName(NULL, aid),
		NpDrmGetRifName((char *)k, aid));
}

int _sceNpDrmGetFixedRifName(char *rif_name, uint64_t aid)
{
	int ret;

	BLOCK_OUT(rif_name, 48, NpDrmGetFixedRifName(NULL, aid),
		NpDrmGetFixedRifName((char *)k, aid));
}

int _sceNpDrmGetRifNameForInstall(char *rif_name,
	const SceNpDrmLicense *license, unsigned int is_fixed)
{
	char *kernelName = rif_name;
	SceNpDrmLicense *kernelLicense = NULL;
	int ret;

	if (rif_name) {
		kernelName = __builtin_alloca(48);
		__builtin_memset(kernelName, 0, 48);
	}
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	ret = NpDrmGetRifNameForInstall(kernelName, kernelLicense, is_fixed);
	if (rif_name)
		return copyResult(ret, sceKernelCopyToUser(rif_name, kernelName, 48));
	return ret;
}

unsigned int _sceNpDrmCheckDrmReset(const void *in_addr, unsigned int size,
	const SceNpDrmCheckDrmResetOpt *pOpt)
{
	SceNpDrmCheckDrmResetOpt args;
	void *kernelInput = (void *)in_addr;
	uint64_t account_id;
	int *pReset;
	int ret;

	ARGS_IN(args, pOpt);
	account_id = args.account_id;
	pReset = args.pReset;
	BLOCK_IN_SIZED(kernelInput, in_addr, args.in_size, 64);
	BLOCK_OUT(pReset, 4,
		NpDrmCheckDrmReset(kernelInput, size, account_id, NULL),
		NpDrmCheckDrmReset(kernelInput, size, account_id, (int *)k));
}

int _sceNpDrmPresetRifProvisionalFlag(SceNpDrmLicense *license)
{
	SceNpDrmLicense *kernelLicense;
	int ret;

	if (!license)
		return NpDrmPresetRifProvisionalFlag(NULL);
	kernelLicense = __builtin_alloca(sizeof(*kernelLicense));
	memset(kernelLicense, 0, sizeof(*kernelLicense));
	ret = sceKernelCopyFromUser(kernelLicense, license, sizeof(*kernelLicense));
	if (ret < 0)
		return ret;
	ret = NpDrmPresetRifProvisionalFlag(kernelLicense);
	return copyResult(ret, sceKernelCopyToUser(license, kernelLicense,
		sizeof(*kernelLicense)));
}

__attribute__((noinline)) int NpDrmRemoveActData(uint64_t *accountId)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = SceNpDrmForDriver_8B85A509(accountId);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmCheckActData(int *actType, int *versionFlag,
	uint64_t *accountId, uint64_t *startTime, uint64_t *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = SceNpDrmForDriver_9265B350(actType, versionFlag, accountId,
		startTime, expirationTime);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetRifName(char *name, uint64_t aid)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = sceNpDrmGetRifName(name, aid);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetFixedRifName(char *name, uint64_t aid)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = sceNpDrmGetFixedRifName(name, aid);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetRifNameForInstall(char *rif_name,
	const SceNpDrmLicense *license, unsigned int is_fixed)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = SceNpDrmForDriver_17573133(rif_name, license, is_fixed);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetRifInfo(const SceNpDrmLicense *license,
	unsigned int license_size, int check_sign, char *content_id,
	uint64_t *account_id, int *license_version, int *license_flags, int *flags,
	unsigned int *sku_flags, uint64_t *lic_start_time, uint64_t *lic_exp_time,
	uint64_t *flags2)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = sceNpDrmGetRifInfo(license, license_size, check_sign, content_id,
		account_id, license_version, license_flags, flags, sku_flags,
		lic_start_time, lic_exp_time, flags2);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmCheckDrmReset(const void *in_addr,
	unsigned int size, uint64_t account_id, int *pReset)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = NpDrmCheckDrmResetForDriver(in_addr, size, account_id, pReset);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmPresetRifProvisionalFlag(SceNpDrmLicense *license)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = SceNpDrmForDriver_C070FE89(license, 1);

	sceSyscallLeave(syscall);
	return ret;
}

int scePsmDrmRemoveActData(uint64_t *accountId)
{
	int ret;

	BLOCK_OUT(accountId, 8, PsmDrmRemoveActData(NULL),
		PsmDrmRemoveActData((uint64_t *)k));
}

int scePsmDrmGetActInfo(int *actType, int *versionFlag, uint64_t *accountId,
	const struct NpDrmCheckActDataArgs *userArgs)
{
	NPDRM_ACT_INFO_BODY(PsmDrmGetActInfo);
}

int scePsmDrmGetRifInfo(const ScePsmDrmLicense *license, char *contentId,
	uint64_t *accountId, const struct NpDrmCheckActDataArgs *userArgs)
{
	struct NpDrmCheckActDataArgs args;
	ScePsmDrmLicense *kernelLicense = NULL;
	char *kernelContentId = contentId;
	uint64_t *kernelAccountId = accountId;
	uint64_t *kernelStartTime, *kernelExpirationTime;
	int ret, result;

	ARGS_IN(args, userArgs);
	kernelExpirationTime = args.expirationTime;
	kernelStartTime = args.startTime;
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	if (contentId) {
		kernelContentId = __builtin_alloca(48);
		__builtin_memset(kernelContentId, 0, 48);
	}
	if (accountId) {
		kernelAccountId = __builtin_alloca(sizeof(*kernelAccountId));
		*kernelAccountId = 0;
	}
	if (kernelStartTime) {
		kernelStartTime = __builtin_alloca(sizeof(*kernelStartTime));
		*kernelStartTime = 0;
	}
	if (kernelExpirationTime) {
		kernelExpirationTime = __builtin_alloca(sizeof(*kernelExpirationTime));
		*kernelExpirationTime = 0;
	}
	result = PsmDrmGetRifInfo(kernelLicense, kernelContentId, kernelAccountId,
		kernelStartTime, kernelExpirationTime);
	if (contentId) {
		ret = sceKernelCopyToUser(contentId, kernelContentId, 48);
		if (ret < 0)
			return ret;
	}
	if (accountId) {
		ret = sceKernelCopyToUser(accountId, kernelAccountId, sizeof(*accountId));
		if (ret < 0)
			return ret;
	}
	if (args.startTime) {
		ret = sceKernelCopyToUser(args.startTime, kernelStartTime,
			sizeof(*args.startTime));
		if (ret < 0)
			return ret;
	}
	if (args.expirationTime)
		return copyResult(result, sceKernelCopyToUser(args.expirationTime,
			kernelExpirationTime, sizeof(*args.expirationTime)));
	return result;
}

int scePsmDrmGetRifName(char *name)
{
	int ret;

	BLOCK_OUT(name, 48, PsmDrmGetRifName(NULL, 0, 0),
		PsmDrmGetRifName((char *)k, 0, 0));
}

int scePsmDrmGetRifNameForInstall(char *name, const ScePsmDrmLicense *license)
{
	char *kernelName = name;
	ScePsmDrmLicense *kernelLicense = NULL;
	int ret;

	if (name) {
		kernelName = __builtin_alloca(48);
		__builtin_memset(kernelName, 0, 48);
	}
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	ret = PsmDrmGetRifNameForInstall(kernelName, kernelLicense);
	if (name)
		return copyResult(ret, sceKernelCopyToUser(name, kernelName, 48));
	return ret;
}

int scePsmDrmGetRifKey(const ScePsmDrmLicense *license_buf,
	ScePsmDrmKeySet *keydata, int *flags, const ScePsmDrmExpireTime *exp_time)
{
	ScePsmDrmExpireTime args;
	ScePsmDrmLicense *kernelLicense = NULL;
	ScePsmDrmKeySet *kernelKeydata = keydata;
	int *kernelFlags = flags;
	SceRtcTick *kernelStartTime, *kernelExpirationTime;
	int ret, result;

	ARGS_IN(args, exp_time);
	kernelStartTime = args.start_date;
	kernelExpirationTime = args.end_date;
	BLOCK_IN(kernelLicense, license_buf, sizeof(*kernelLicense));
	if (keydata) {
		kernelKeydata = __builtin_alloca(sizeof(*kernelKeydata));
		memset(kernelKeydata, 0, sizeof(*kernelKeydata));
	}
	if (flags) {
		kernelFlags = __builtin_alloca(sizeof(*kernelFlags));
		*kernelFlags = 0;
	}
	if (kernelStartTime) {
		kernelStartTime = __builtin_alloca(sizeof(*kernelStartTime));
		kernelStartTime->tick = 0;
	}
	if (kernelExpirationTime) {
		kernelExpirationTime = __builtin_alloca(sizeof(*kernelExpirationTime));
		kernelExpirationTime->tick = 0;
	}
	result = PsmDrmGetRifKey(kernelLicense, kernelKeydata, kernelFlags,
		kernelStartTime, kernelExpirationTime);
	if (keydata) {
		ret = sceKernelCopyToUser(keydata, kernelKeydata, sizeof(*keydata));
		if (ret < 0)
			return ret;
	}
	if (flags) {
		ret = sceKernelCopyToUser(flags, kernelFlags, sizeof(*flags));
		if (ret < 0)
			return ret;
	}
	if (args.start_date) {
		ret = sceKernelCopyToUser(args.start_date, kernelStartTime,
			sizeof(*args.start_date));
		if (ret < 0)
			return ret;
	}
	if (args.end_date)
		return copyResult(result, sceKernelCopyToUser(args.end_date,
			kernelExpirationTime, sizeof(*args.end_date)));
	return result;
}

__attribute__((noinline)) int PsmDrmRemoveActData(uint64_t *accountId)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ScePsmDrmForDriver_4CD5375C(accountId);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetActInfo(int *actType, int *versionFlag,
	uint64_t *accountId, uint64_t *startTime, uint64_t *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ScePsmDrmForDriver_B09003A7(actType, versionFlag, accountId,
		startTime, expirationTime);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifInfo(const ScePsmDrmLicense *license,
	char *contentId, uint64_t *accountId, uint64_t *startTime,
	uint64_t *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ScePsmDrmForDriver_984F9017(license, contentId, accountId,
		startTime, expirationTime);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifName(char *name, int size,
	SceUInt64 accountId)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = PsmDrmGetRifNameForDriver(name, syscall << 16, accountId);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifNameForInstall(char *name,
	const ScePsmDrmLicense *license)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = PsmDrmGetRifNameForInstallForDriver(name, license);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifKey(const ScePsmDrmLicense *license,
	ScePsmDrmKeySet *keydata, int *flags, SceRtcTick *startTime,
	SceRtcTick *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForDriver_091F7247(0)) {
		sceSyscallLeave(syscall);
		return 0x80010058;
	}
	ret = ScePsmDrmForDriver_8C8CFD01(license, keydata, flags, startTime,
		expirationTime);
	sceSyscallLeave(syscall);
	return ret;
}

int _sceNpDrmPackageCheck(const void *buffer, unsigned int size,
	const unsigned char *digest, unsigned int identifier)
{
	unsigned char *kernelDigest = NULL;
	int ret;

	BLOCK_IN(kernelDigest, digest, 32);
	return NpDrmPackageCheckForUser(buffer, size, kernelDigest, identifier);
}

int _sceNpDrmPackageDecrypt(void *buffer, unsigned int size,
	const _sceNpDrmPackageDecrypt_opt *opt)
{
	_sceNpDrmPackageDecrypt_opt args;
	int ret;

	ARGS_IN(args, opt);
	return NpDrmPackageDecryptForUser(buffer, size, args.offset, args.identifier);
}

int _sceNpDrmPackageTransform(void *buffer, unsigned int size,
	SceSblDmac5HashTransformContext *context, unsigned int flags)
{
	SceSblDmac5HashTransformContext *kernelContext;
	int ret, result;

	if (context) {
		kernelContext = __builtin_alloca(sizeof(*kernelContext));
		*kernelContext = (SceSblDmac5HashTransformContext){ 0 };
		ret = sceKernelCopyFromUser(kernelContext, context,
			sizeof(*kernelContext));
		if (ret < 0)
			return ret;
		result = NpDrmPackageTransformForUser(buffer, size, kernelContext,
			flags);
		return copyResult(result, sceKernelCopyToUser(context,
			kernelContext, sizeof(*kernelContext)));
	}
	return NpDrmPackageTransformForUser(buffer, size, NULL, flags);
}

#define NPDRM_PACKAGE_STARTED_BODY(_call) \
	struct NpDrmPackageInstallStartedArgs args; \
	char *kernelMsg = NULL; \
	int ret, len; \
	ARGS_IN(args, opt); \
	len = args.len; \
	BLOCK_IN_SIZED(kernelMsg, msg, args.size, 64); \
	return _call(kind, total, kernelMsg, len)

int _sceNpDrmPackageInstallStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmPackageInstallStarted);
}

#define NPDRM_PACKAGE_FINISHED_BODY(_call) \
	SyscallSizeArgs args; \
	char *kernelMsg = NULL; \
	int ret; \
	ARGS_IN(args, opt); \
	BLOCK_IN_SIZED(kernelMsg, msg, args.size, 64); \
	return _call(result, kernelMsg, len)

int _sceNpDrmPackageInstallFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmPackageInstallFinished);
}

int _sceNpDrmPackageUninstallStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmPackageUninstallStarted);
}

int _sceNpDrmPackageUninstallFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmPackageUninstallFinished);
}

int _sceNpDrmSaveDataInstallStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmSaveDataInstallStarted);
}

int _sceNpDrmSaveDataInstallFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmSaveDataInstallFinished);
}

int _sceNpDrmSaveDataFormatStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmSaveDataFormatStarted);
}

int _sceNpDrmSaveDataFormatFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmSaveDataFormatFinished);
}

int NpDrmPackageCheckOnStack(void *argp)
{
	const struct NpDrmPackageCheckArgs *args = argp;

	return NpDrmPackageProcessHeader(args->data, args->size, args->digest,
		args->flags);
}

int NpDrmPackageDecryptOnStack(void *argp)
{
	const struct NpDrmPackageDecryptArgs *args = argp;

	return NpDrmPackageTransformData(args->data, args->size, args->offset,
		args->flags);
}

__attribute__((noinline)) int NpDrmPackageAesInitialize(
	SceAesCompactContext *context, const void *key, unsigned int keySize)
{
	return SceKernelUtilsForDriver_EDA97D6D(context, 128, keySize * 8, key);
}

__attribute__((noinline)) int NpDrmPackageAesEncrypt(
	SceAesCompactContext *context, void *output, const void *input)
{
	return SceKernelUtilsForDriver_302947B6(context, input, output);
}

__attribute__((noinline)) int NpDrmPackageAesCtr(const void *input,
	void *output, unsigned int size, const void *key, unsigned int keyBits,
	void *counter)
{
	return SceSblSsMgrForDriver_82B5DCEF(input, output, size, key, keyBits, counter, 1);
}

int NpDrmPackageHashTransform(const void *input,
	void *digest, unsigned int size, SceSblDmac5HashTransformContext *context,
	unsigned int flags)
{
	unsigned int command = (int)(flags << 1) < 0 ? 3 : 0x13;
	unsigned int mode;

	if (context->length == 0)
		mode = digest ? 0 : 0x400;
	else
		mode = digest ? 0x800 : 0xc00;
	return sceSblDmac5HashTransform(input, digest, size, context,
		1, command, mode);
}

__attribute__((noinline)) int NpDrmPackageIsHardwareAllowed(unsigned int retail)
{
	if (retail) {
		if (SceSblAIMgrForDriver_D78B04A2())
			return 1;
		if (SceSblPostSsMgrForDriver_9D2E2D39())
			return 1;
		return SceQafMgrForDriver_AE033133() != 0;
	}
	if (SceSblAIMgrForDriver_F4B98F66())
		return 1;
	return SceSblAIMgrForDriver_274663A0() != 0;
}

__attribute__((noinline)) int NpDrmPackageCheckForUser(const void *buffer,
	unsigned int size, const unsigned char *digest, unsigned int identifier)
{
	unsigned int syscall = sceSyscallEnter();
	uint64_t authId = 0;
	struct NpDrmPackageCheckArgs args;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	SceUID mapId;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = 0x80010058;
		goto out;
	}
	ret = sceSblACMgrGetPaid(0, &authId);
	if (ret < 0)
		goto out;
	if (authId != UINT64_C(0x2800000000000001) &&
		authId - UINT64_C(0x280000000000002d) >= 2 &&
		authId != UINT64_C(0x2800000000000039)) {
		ret = 0x8001000d;
		goto out;
	}
	if (!size) {
		ret = 0x80010016;
		goto out;
	}
	ret = sceKernelIsAccessibleRangeProc(0, 0x10, buffer, size);
	if (ret < 0)
		goto out;
	mapId = sceKernelUserMap("SceNpDrmPackageChk", 1, buffer, size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (mapId < 0) {
		ret = mapId;
		goto out;
	}
	args.data = (unsigned char *)kernelPage + kernelOffset;
	ret = NpDrmPackageSetAesWorkspace(gNpDrmPackageAesWorkspaces,
		sizeof(gNpDrmPackageAesWorkspaces[0]), 6, identifier);
	if (!ret) {
		args.size = size;
		args.digest = digest;
		args.flags = identifier;
		ret = SceThreadmgrForDriver_E54FD746(0x2000, NpDrmPackageCheckOnStack, &args);
	}
	sceKernelUserUnmap(mapId, buffer, size);
out:
	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmPackageDecryptForUser(void *buffer,
	unsigned int size, int64_t offset, unsigned int identifier)
{
	unsigned int syscall = sceSyscallEnter();
	uint64_t authId = 0;
	struct NpDrmPackageDecryptArgs args;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	SceUID mapId;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = 0x80010058;
		goto out;
	}
	ret = sceSblACMgrGetPaid(0, &authId);
	if (ret < 0)
		goto out;
	if (authId != UINT64_C(0x2800000000000001) &&
		authId - UINT64_C(0x280000000000002d) >= 2 &&
		authId != UINT64_C(0x2800000000000039)) {
		ret = 0x8001000d;
		goto out;
	}
	if (!size) {
		ret = 0x80010016;
		goto out;
	}
	ret = sceKernelIsAccessibleRangeProc(0, 0x30, buffer, size);
	if (ret < 0)
		goto out;
	mapId = sceKernelUserMap("SceNpDrmPackageDec", 3, buffer, size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (mapId < 0) {
		ret = mapId;
		goto out;
	}
	args.data = (unsigned char *)kernelPage + kernelOffset;
	args.size = size;
	args.offset = offset;
	args.flags = identifier;
	args.reserved = 0;
	ret = SceThreadmgrForDriver_E54FD746(0x2000, NpDrmPackageDecryptOnStack, &args);
	sceKernelUserUnmap(mapId, buffer, size);
out:
	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmPackageTransformForUser(void *buffer,
	unsigned int size, SceSblDmac5HashTransformContext *context,
	unsigned int flags)
{
	unsigned int syscall = sceSyscallEnter();
	uint64_t authId = 0;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	SceUID mapId;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = 0x80010058;
		goto out;
	}
	ret = sceSblACMgrGetPaid(0, &authId);
	if (ret < 0)
		goto out;
	if (authId != UINT64_C(0x2800000000000001) &&
		authId - UINT64_C(0x280000000000002d) >= 2 &&
		authId != UINT64_C(0x2800000000000039)) {
		ret = 0x8001000d;
		goto out;
	}
	if (!size) {
		ret = 0x80010016;
		goto out;
	}
	ret = sceKernelIsAccessibleRangeProc(0, 0x10, buffer, size);
	if (ret < 0)
		goto out;
	mapId = sceKernelUserMap("SceNpDrmPackageTrans", 1, buffer, size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (mapId < 0) {
		ret = mapId;
		goto out;
	}
	ret = NpDrmPackageHashData((unsigned char *)kernelPage + kernelOffset,
		size, context, flags);
	sceKernelUserUnmap(mapId, buffer, size);
out:
	sceSyscallLeave(syscall);
	return ret;
}

int SceNpDrmForDriver_3BFD2850(int value)
{
	gNpDrmPackageGameExist = value;
	return 0;
}

int sceNpDrmPackageIsGameExist(void)
{
	return gNpDrmPackageGameExist;
}

#define NPDRM_PACKAGE_PROGRESS_BODY(_call) \
	unsigned int syscall = sceSyscallEnter(); \
	uint64_t authId = 0; \
	int ret; \
	if (!sceSblACMgrIsSystemProgram(0)) { \
		ret = 0x80010058; \
		goto out; \
	} \
	ret = sceSblACMgrGetPaid(0, &authId); \
	if (ret < 0) \
		goto out; \
	if (authId == UINT64_C(0x2800000000000001)) \
		goto notify; \
	if (authId - UINT64_C(0x280000000000002d) < 2) \
		goto notify; \
	if (authId != UINT64_C(0x2800000000000039)) { \
		ret = 0x8001000d; \
		goto out; \
	} \
notify: \
	ret = (_call); \
out: \
	sceSyscallLeave(syscall); \
	return ret

__attribute__((noinline)) int NpDrmPackageInstallStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(
		SceSysrootForDriver_70A60BBA(kind, total, msg, len));
}

int sceNpDrmPackageInstallOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_79458ACB(kind, percent));
}

__attribute__((noinline)) int NpDrmPackageInstallFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_D03741B8(result, msg, len));
}

__attribute__((noinline)) int NpDrmPackageUninstallStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(
		SceSysrootForDriver_56C4C949(kind, total, msg, len));
}

int sceNpDrmPackageUninstallOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_7998F5B5(kind, percent));
}

__attribute__((noinline)) int NpDrmPackageUninstallFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_0F59B369(result, msg, len));
}

__attribute__((noinline)) int NpDrmSaveDataInstallStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_97E2FB77(kind, total, msg, len));
}

int sceNpDrmSaveDataInstallOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_F0C91CD3(kind, percent));
}

__attribute__((noinline)) int NpDrmSaveDataInstallFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_E384DD93(result, msg, len));
}

__attribute__((noinline)) int NpDrmSaveDataFormatStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_E8EBF179(kind, total, msg, len));
}

int sceNpDrmSaveDataFormatOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_1BA0C6C1(kind, percent));
}

__attribute__((noinline)) int NpDrmSaveDataFormatFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_D04C7723(result, msg, len));
}
