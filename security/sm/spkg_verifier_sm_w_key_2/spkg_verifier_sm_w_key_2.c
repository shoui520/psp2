/*
 * spkg_verifier_sm_w_key_2.c: SPKG Verifier Secure Module
 * (spkg_verifier_sm_w_key_2.self) command 0xe0002 handler, Certified File
 * header & segment verification, hardware revision floor enforcement, and
 * target/QA policy checks (0x80b09c..0x80bf34, .rodata/.data 0x80ec28..0x80f7c8).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "spkg_verifier_sm_w_key_2.h"

/* ============================================================================
 * .rodata (0x80ec28..0x80f4f8)
 * ============================================================================ */

static const uintptr_t s_spkgHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)spkgHookInvoke,
	0u,
};

static const uintptr_t s_spkgBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u,
};

static const uintptr_t s_spkgVerifierVtable[4] = {
	0u,
	0u,
	(uintptr_t)spkgVerifierDtorD2,
	(uintptr_t)spkgVerifierDtorD0,
};

/* 0x80ec58..0x80f090: 90 hardware revision minimum firmware version & attribute rules */
static const SpkgHwRevRule s_spkgHwRevRules[90] = {
	{ 0xffffffffu, 0xffffffffu, 0xffffffffu },
	{ 0xffffffffu, 0xffffffffu, 0xffffffffu },
	{ 0x00924000u, 0x00924000u, 0x00000001u },
	{ 0x00924000u, 0x00924000u, 0x00000001u },
	{ 0x00996000u, 0x00996000u, 0x0000000fu },
	{ 0x00924000u, 0x00924000u, 0x00000001u },
	{ 0x00924000u, 0x00924000u, 0x00000001u },
	{ 0x00945000u, 0x00945000u, 0x00000001u },
	{ 0x00945000u, 0x00945000u, 0x00000001u },
	{ 0x00945000u, 0x00945000u, 0x00000001u },
	{ 0x00990000u, 0x00990000u, 0x00000001u },
	{ 0x00990000u, 0x00990000u, 0x00000002u },
	{ 0x00990000u, 0x00990000u, 0x00000002u },
	{ 0x00995000u, 0x00995000u, 0x00000004u },
	{ 0x00995000u, 0x00995000u, 0x00000004u },
	{ 0x00995000u, 0x00995000u, 0x00000004u },
	{ 0x00996000u, 0x00996000u, 0x00000004u },
	{ 0x00996000u, 0x00996000u, 0x00000004u },
	{ 0x00996000u, 0x00996000u, 0x00000004u },
	{ 0x00996000u, 0x00996000u, 0x00000008u },
	{ 0x00996000u, 0x00996000u, 0x00000008u },
	{ 0x00996000u, 0x00996000u, 0x00000008u },
	{ 0x01000000u, 0x01000000u, 0x00000008u },
	{ 0x01000000u, 0x01000000u, 0x00000008u },
	{ 0x01000000u, 0x01000000u, 0x00000008u },
	{ 0x01000000u, 0x01000000u, 0x00000008u },
	{ 0x01000000u, 0x01000000u, 0x00000008u },
	{ 0x01000000u, 0x01000000u, 0x00000008u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000010u },
	{ 0x01800000u, 0x01800000u, 0x00000020u },
	{ 0x01800000u, 0x01800000u, 0x00000020u },
	{ 0x01800000u, 0x01800000u, 0x00000020u },
	{ 0x01800000u, 0x01800000u, 0x00000020u },
	{ 0x01800000u, 0x01800000u, 0x00000020u },
	{ 0x01800000u, 0x01800000u, 0x00000020u },
	{ 0x02000000u, 0x02000000u, 0x00000040u },
	{ 0x02000000u, 0x02000000u, 0x00000040u },
	{ 0x02000000u, 0x02000000u, 0x00000040u },
	{ 0x02500000u, 0x02500000u, 0x00000040u },
	{ 0x02500000u, 0x02500000u, 0x00000040u },
	{ 0x02500000u, 0x02500000u, 0x00000040u },
	{ 0x02500000u, 0x02500000u, 0x00000040u },
	{ 0x02500000u, 0x02500000u, 0x00000040u },
	{ 0x02500000u, 0x02500000u, 0x00000040u },
	{ 0x02500000u, 0x02500000u, 0x00000080u },
	{ 0x02500000u, 0x02500000u, 0x00000080u },
	{ 0x02500000u, 0x02500000u, 0x00000080u },
	{ 0x02500000u, 0x02500000u, 0x00000080u },
	{ 0x02500000u, 0x02500000u, 0x00000080u },
	{ 0x02500000u, 0x02500000u, 0x00000080u },
	{ 0x02010000u, 0x02010000u, 0x00000100u },
	{ 0x02100000u, 0x02100000u, 0x00000100u },
	{ 0x02100000u, 0x02100000u, 0x00000100u },
	{ 0x02500000u, 0x02500000u, 0x00000100u },
	{ 0x02500000u, 0x02500000u, 0x00000100u },
	{ 0x02500000u, 0x02500000u, 0x00000200u },
	{ 0x02500000u, 0x02500000u, 0x00000200u },
	{ 0x03100000u, 0x03100000u, 0x00000400u },
	{ 0x03100000u, 0x03100000u, 0x00000400u },
	{ 0x03100000u, 0x03100000u, 0x00000400u },
	{ 0x03100000u, 0x03100000u, 0x00000400u },
	{ 0x03100000u, 0x03100000u, 0x00000400u },
	{ 0x03100000u, 0x03100000u, 0x00000400u },
	{ 0x03100000u, 0x03100000u, 0x00000400u },
	{ 0x03200000u, 0x03200000u, 0x00000800u },
	{ 0x03200000u, 0x03200000u, 0x00000800u },
	{ 0x03100000u, 0x03100000u, 0x00001000u },
	{ 0x03100000u, 0x03100000u, 0x00001000u },
	{ 0x03100000u, 0x03100000u, 0x00001000u },
	{ 0x03100000u, 0x03100000u, 0x00001000u },
	{ 0x03500000u, 0x03500000u, 0x00004000u },
	{ 0x03500000u, 0x03500000u, 0x00004000u },
	{ 0x03500000u, 0x03500000u, 0x00004000u },
	{ 0x03500000u, 0x03500000u, 0x00004000u },
	{ 0x03500000u, 0x03500000u, 0x00004000u },
	{ 0x03500000u, 0x03500000u, 0x00004000u },
	{ 0x03500000u, 0x03500000u, 0x00008000u },
	{ 0x03500000u, 0x03500000u, 0x00008000u },
	{ 0x03500000u, 0x03500000u, 0x00008000u },
	{ 0x03500000u, 0x03500000u, 0x00008000u },
	{ 0x03500000u, 0x03500000u, 0x00008000u },
	{ 0x03500000u, 0x03500000u, 0x00008000u },
};

/* 0x80f090..0x80f4bc: 89 Console ID product/subcode to hardware revision index mappings */
static const SpkgHwRevMapEntry s_spkgHwRevMap[89] = {
	{ 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x00000000u },
	{ 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x00000001u },
	{ 0x0100u, 0x0100u, 0x0001u, 0x0000u, 0x00000002u },
	{ 0x0101u, 0x0101u, 0x0002u, 0x0000u, 0x00000003u },
	{ 0x0101u, 0x0101u, 0x0004u, 0x0000u, 0x00000004u },
	{ 0x0103u, 0x0111u, 0x0002u, 0x0000u, 0x00000005u },
	{ 0x0101u, 0x0101u, 0x0003u, 0x0000u, 0x00000006u },
	{ 0x0101u, 0x0101u, 0x0005u, 0x0000u, 0x00000007u },
	{ 0x0103u, 0x0111u, 0x0007u, 0x0000u, 0x00000008u },
	{ 0x0103u, 0x0111u, 0x0009u, 0x0000u, 0x00000009u },
	{ 0x0101u, 0x0101u, 0x0009u, 0x0000u, 0x0000000au },
	{ 0x0103u, 0x0111u, 0x000au, 0x0000u, 0x0000000bu },
	{ 0x0102u, 0x0102u, 0x000au, 0x0000u, 0x0000000cu },
	{ 0x0101u, 0x0101u, 0x000bu, 0x0000u, 0x0000000du },
	{ 0x0103u, 0x0111u, 0x000cu, 0x0000u, 0x0000000eu },
	{ 0x0102u, 0x0102u, 0x000cu, 0x0000u, 0x0000000fu },
	{ 0x0101u, 0x0101u, 0x000du, 0x0000u, 0x00000010u },
	{ 0x0103u, 0x0111u, 0x000bu, 0x0000u, 0x00000011u },
	{ 0x0102u, 0x0102u, 0x000bu, 0x0000u, 0x00000012u },
	{ 0x0101u, 0x0101u, 0x000eu, 0x0000u, 0x00000013u },
	{ 0x0103u, 0x0111u, 0x000du, 0x0000u, 0x00000014u },
	{ 0x0102u, 0x0102u, 0x000du, 0x0000u, 0x00000015u },
	{ 0x0101u, 0x0101u, 0x000fu, 0x0000u, 0x00000016u },
	{ 0x0103u, 0x0111u, 0x000fu, 0x0000u, 0x00000017u },
	{ 0x0102u, 0x0102u, 0x000fu, 0x0000u, 0x00000018u },
	{ 0x0101u, 0x0101u, 0x0010u, 0x0000u, 0x00000019u },
	{ 0x0103u, 0x0111u, 0x0010u, 0x0000u, 0x0000001au },
	{ 0x0102u, 0x0102u, 0x0010u, 0x0000u, 0x0000001bu },
	{ 0x0101u, 0x0101u, 0x0401u, 0x0000u, 0x0000001cu },
	{ 0x0103u, 0x0111u, 0x0401u, 0x0000u, 0x0000001du },
	{ 0x0102u, 0x0102u, 0x0401u, 0x0000u, 0x0000001eu },
	{ 0x0101u, 0x0101u, 0x0402u, 0x0000u, 0x0000001fu },
	{ 0x0103u, 0x0111u, 0x0402u, 0x0000u, 0x00000020u },
	{ 0x0102u, 0x0102u, 0x0402u, 0x0000u, 0x00000021u },
	{ 0x0101u, 0x0101u, 0x0403u, 0x0000u, 0x00000022u },
	{ 0x0103u, 0x0111u, 0x0403u, 0x0000u, 0x00000023u },
	{ 0x0102u, 0x0102u, 0x0403u, 0x0000u, 0x00000024u },
	{ 0x0101u, 0x0101u, 0x0011u, 0x0000u, 0x00000025u },
	{ 0x0101u, 0x0101u, 0x0012u, 0x0000u, 0x00000026u },
	{ 0x0103u, 0x0111u, 0x0011u, 0x0000u, 0x00000027u },
	{ 0x0103u, 0x0111u, 0x0012u, 0x0000u, 0x00000028u },
	{ 0x0102u, 0x0102u, 0x0011u, 0x0000u, 0x00000029u },
	{ 0x0102u, 0x0102u, 0x0012u, 0x0000u, 0x0000002au },
	{ 0x0101u, 0x0101u, 0x0404u, 0x0000u, 0x0000002bu },
	{ 0x0103u, 0x0111u, 0x0404u, 0x0000u, 0x0000002cu },
	{ 0x0102u, 0x0102u, 0x0404u, 0x0000u, 0x0000002du },
	{ 0x0101u, 0x0101u, 0x0406u, 0x0000u, 0x0000002eu },
	{ 0x0103u, 0x0111u, 0x0406u, 0x0000u, 0x0000002fu },
	{ 0x0102u, 0x0102u, 0x0406u, 0x0000u, 0x00000030u },
	{ 0x0101u, 0x0101u, 0x0407u, 0x0000u, 0x00000031u },
	{ 0x0103u, 0x0111u, 0x0407u, 0x0000u, 0x00000032u },
	{ 0x0102u, 0x0102u, 0x0407u, 0x0000u, 0x00000033u },
	{ 0x0101u, 0x0101u, 0x0013u, 0x0000u, 0x00000034u },
	{ 0x0101u, 0x0101u, 0x0014u, 0x0000u, 0x00000035u },
	{ 0x0103u, 0x0111u, 0x0013u, 0x0000u, 0x00000036u },
	{ 0x0103u, 0x0111u, 0x0014u, 0x0000u, 0x00000037u },
	{ 0x0102u, 0x0102u, 0x0013u, 0x0000u, 0x00000038u },
	{ 0x0102u, 0x0102u, 0x0014u, 0x0000u, 0x00000039u },
	{ 0x0101u, 0x0101u, 0x0408u, 0x0000u, 0x0000003au },
	{ 0x0103u, 0x0111u, 0x0601u, 0x0000u, 0x0000003bu },
	{ 0x0102u, 0x0102u, 0x0601u, 0x0000u, 0x0000003cu },
	{ 0x0103u, 0x0111u, 0x0602u, 0x0000u, 0x0000003du },
	{ 0x0102u, 0x0102u, 0x0602u, 0x0000u, 0x0000003eu },
	{ 0x0103u, 0x0111u, 0x0201u, 0x0000u, 0x0000003fu },
	{ 0x0102u, 0x0102u, 0x0201u, 0x0000u, 0x00000040u },
	{ 0x0101u, 0x0101u, 0x0603u, 0x0000u, 0x00000041u },
	{ 0x0103u, 0x0111u, 0x0604u, 0x0000u, 0x00000042u },
	{ 0x0102u, 0x0102u, 0x0604u, 0x0000u, 0x00000043u },
	{ 0x0103u, 0x0111u, 0x0605u, 0x0000u, 0x00000044u },
	{ 0x0102u, 0x0102u, 0x0605u, 0x0000u, 0x00000045u },
	{ 0x0103u, 0x0111u, 0x0606u, 0x0000u, 0x00000046u },
	{ 0x0102u, 0x0102u, 0x0606u, 0x0000u, 0x00000047u },
	{ 0x0103u, 0x0111u, 0x0202u, 0x0000u, 0x00000048u },
	{ 0x0102u, 0x0102u, 0x0202u, 0x0000u, 0x00000049u },
	{ 0x0101u, 0x0101u, 0x0409u, 0x0000u, 0x0000004au },
	{ 0x0103u, 0x0111u, 0x040au, 0x0000u, 0x0000004cu },
	{ 0x0102u, 0x0102u, 0x040au, 0x0000u, 0x0000004du },
	{ 0x0101u, 0x0101u, 0x040du, 0x0000u, 0x0000004eu },
	{ 0x0103u, 0x0111u, 0x040du, 0x0000u, 0x0000004fu },
	{ 0x0102u, 0x0102u, 0x040du, 0x0000u, 0x00000050u },
	{ 0x0101u, 0x0101u, 0x040eu, 0x0000u, 0x00000051u },
	{ 0x0103u, 0x0111u, 0x040eu, 0x0000u, 0x00000052u },
	{ 0x0102u, 0x0102u, 0x040eu, 0x0000u, 0x00000053u },
	{ 0x0101u, 0x0101u, 0x0017u, 0x0000u, 0x00000054u },
	{ 0x0101u, 0x0101u, 0x0018u, 0x0000u, 0x00000055u },
	{ 0x0103u, 0x0111u, 0x0017u, 0x0000u, 0x00000056u },
	{ 0x0103u, 0x0111u, 0x0018u, 0x0000u, 0x00000057u },
	{ 0x0102u, 0x0102u, 0x0017u, 0x0000u, 0x00000058u },
	{ 0x0102u, 0x0102u, 0x0018u, 0x0000u, 0x00000059u },
};

/* 0x80f4bc..0x80f4d4: 3 SPKG segment specifications to verify/decrypt */
static const SpkgSegmentSpec s_spkgSegmentSpecs[3] = {
	{ 1u, 1u },
	{ 2u, 2u },
	{ 3u, 3u },
};

/* 0x80f4d4..0x80f4f8: rule table pointers indexed by (pkgType - 1) */
static const SpkgHwRevRule *const s_spkgTypeRuleTables[9] = {
	s_spkgHwRevRules,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	s_spkgHwRevRules,
};

/* ============================================================================
 * .data (0x80f68c..0x80f7c8): SPKG Key 2 (AES-256 + IV + RSA-2048 Public Key)
 * ============================================================================ */

static const uint8_t s_spkgMetaKey[48] = {
#if FW_DEBUG
	0xf3u, 0x40u, 0x2fu, 0x35u, 0xf0u, 0xdcu, 0x26u, 0x31u,
	0x70u, 0x42u, 0xf1u, 0x6du, 0x31u, 0x98u, 0x81u, 0x78u,
	0x4du, 0x9bu, 0x7du, 0xddu, 0x7du, 0x5bu, 0xfau, 0xefu,
	0x97u, 0xc7u, 0x8cu, 0x9du, 0x30u, 0x11u, 0x1au, 0xfcu,
	0x7fu, 0xd7u, 0x65u, 0x7cu, 0xc5u, 0xd4u, 0x41u, 0xbbu,
	0x9au, 0xd6u, 0xe1u, 0xcdu, 0x0au, 0xd6u, 0x9bu, 0xeau,
#else
	0x77u, 0xe3u, 0x88u, 0xfbu, 0x9au, 0xc7u, 0x8au, 0x00u,
	0xbau, 0x7au, 0x74u, 0x66u, 0x10u, 0xa0u, 0xdfu, 0x76u,
	0x35u, 0x09u, 0x19u, 0xf5u, 0x41u, 0x97u, 0x4eu, 0x14u,
	0x39u, 0xadu, 0x76u, 0xaau, 0x96u, 0x32u, 0x7cu, 0xe2u,
	0x41u, 0xf2u, 0xd4u, 0xf7u, 0xe5u, 0xb1u, 0x06u, 0x75u,
	0x7au, 0xbcu, 0xf4u, 0x81u, 0x92u, 0xb9u, 0x39u, 0x44u,
#endif
};

static const uint32_t s_spkgRsaModulus[64] = {
#if FW_DEBUG
	0xf3d12f98u, 0xfa224657u, 0xecdb5b49u, 0x9ca255efu,
	0x661d7fa5u, 0x58f253fau, 0x53669572u, 0x8f32146du,
	0xa572bb8cu, 0x2c1e1237u, 0xe76318edu, 0x38fb066eu,
	0xe63e5bc0u, 0x8694eb88u, 0x89f33730u, 0x90af955cu,
	0xccb72657u, 0x552f6cceu, 0x3330f6b8u, 0x80170783u,
	0x604f492au, 0x965cfb30u, 0x6e99a0bbu, 0x424aff40u,
	0xd1c8ba0fu, 0x1aaa52ebu, 0x57c31b1cu, 0x72065ee9u,
	0x36906613u, 0x576c26aeu, 0x64af99d7u, 0xfd769cf6u,
	0xa581b31eu, 0x39e9bca4u, 0x37ec0f5au, 0x3a83b140u,
	0xc988368bu, 0xfe847c63u, 0x7e711430u, 0x1b2d7e4du,
	0xdcbc32bdu, 0xd4283f38u, 0xaa21a34cu, 0x52900e69u,
	0x2d7a214eu, 0x13e97244u, 0xfaeda2beu, 0xa22f2270u,
	0x44ce76a1u, 0x7dffc132u, 0xc368bd83u, 0x9780b87fu,
	0x2db1f67fu, 0x0a52ce33u, 0xe35c3857u, 0x7a390971u,
	0x93f25dc7u, 0x1577dd50u, 0x8ede9607u, 0xe7c2908au,
	0x59fbd9a3u, 0x28424b0du, 0xb0717310u, 0x33922715u,
#else
	0x37d91eb1u, 0x672d07e6u, 0xe5aaf141u, 0xe7378c30u,
	0xe893b8e5u, 0xc0c47430u, 0x58daf6b2u, 0x08a5cf95u,
	0x330ab1f8u, 0xbe32d6a4u, 0x43766004u, 0x670e6865u,
	0x1bc3dc70u, 0xa7d0e6f0u, 0xf3e6b20cu, 0x6ec31ee6u,
	0xb0e45dadu, 0xa82c6b2fu, 0xf5fd5665u, 0xcb2d69e6u,
	0x8fad21c0u, 0xe21d700bu, 0x91235667u, 0xaf6c7179u,
	0x8ffe0083u, 0xf9bf5277u, 0x7cc72df3u, 0x8cb34dccu,
	0x5820675cu, 0xcc739795u, 0x10e25feau, 0xacccd978u,
	0xc2940515u, 0x6f21de58u, 0x4db84787u, 0xd880384au,
	0xe6c4c4fau, 0xfaa4728du, 0x8c92cdaeu, 0x0261d8ebu,
	0xf74c9be4u, 0xc86ef999u, 0x6e518ca9u, 0x78259318u,
	0xb37d021eu, 0xb8c43854u, 0x37ca5418u, 0x9321b6eeu,
	0x699cb903u, 0xa8ffd32au, 0xab2a3507u, 0x6a6d7f6bu,
	0xe2928b0au, 0xa8125c74u, 0x944b9da2u, 0xb707b048u,
	0x76aef96fu, 0x89e52d44u, 0xbe837987u, 0x5506dafeu,
	0x67a6d326u, 0xbd80c507u, 0x49916e3bu, 0x9d923932u,
#endif
};

static const uint8_t s_spkgRsaExponent[4] = {
	0x00u, 0x01u, 0x00u, 0x01u,
};

/* ============================================================================
 * .bss (0x810000..0x816010)
 * ============================================================================ */

static uint8_t s_spkgHeapPool[0x4000] __attribute__((aligned(16)));
static SpkgVerifier s_spkgVerifier;
static uint8_t s_spkgHdrBuf[0x2000] __attribute__((aligned(16)));

/* ============================================================================
 * Service Entry & Command 0xe0002 Handler (0x80b09c..0x80b18e)
 * ============================================================================ */

/* 0x80b09c */
int sm_main(void)
{
	SmServiceContext srvCtx;
	SmServiceHookNode *node;
	int status = SCE_SBL_ERROR_SPKG_INVALID_VALUE;

	smHeapInit(s_spkgHeapPool, sizeof(s_spkgHeapPool));
	smSrvInit(&srvCtx);
	node = smSrvAllocNode(&srvCtx);
	if (node != NULL) {
		node->cmdId = SCE_SBL_SPKG_CMD_VERIFY;
		node->next = NULL;
		if (&node->vtable != NULL) {
			node->vtable = &s_spkgHookVtable[2];
			node->handler = (void *)spkgCmdVerifyHeaderAndSegments;
		}
		node->next = srvCtx.head;
		srvCtx.head = node;
		status = 0;
		smSrvRunLoop(&srvCtx);
	}
	smSrvTeardown(&srvCtx);
	return status;
}

/* 0x80b112 (name ours) */
void spkgHookInvoke(void *self, void *commBuf)
{
	void (**fnPtr)(void *) = (void (**)(void *))((uint8_t *)self + 4u);

	(*fnPtr)(commBuf);
}

/* 0x80b126 (name ours) */
void spkgStaticInitOrDtor(int isInit)
{
	if (isInit == 1)
		spkgVerifierCtorC1(&s_spkgVerifier);
	else if (isInit == 0)
		spkgVerifierDtorD2(&s_spkgVerifier);
}

/* 0x80b146 (name ours; mailbox command 0xe0002) */
void spkgCmdVerifyHeaderAndSegments(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	int status;

	status = spkgVerifierPrepare(&s_spkgVerifier,
			(SpkgVerifyRequest *)(void *)(pkt + 0x40u));
	if (status == 0)
		status = spkgVerifierExecute(&s_spkgVerifier);
	spkgVerifierCleanup(&s_spkgVerifier);
	*(int *)(void *)(pkt + 8u) = status;
}

/* 0x80b182 (name ours) */
void spkgStaticInitThunk(void)
{
	spkgStaticInitOrDtor(1);
}

/* 0x80b188 (name ours) */
void spkgStaticDtorThunk(void)
{
	spkgStaticInitOrDtor(0);
}

/* ============================================================================
 * Keyring & ARM Address Range Validation Helpers (0x80b292..0x80b3b0)
 * ============================================================================ */

/* 0x80b292 (name ours) */
void spkgReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	uint32_t off = slotIndex * 0x20u;

	outWords8[0] = REG32(0xe0058000u + off);
	outWords8[1] = REG32(0xe0058004u + off);
	outWords8[2] = REG32(0xe0058008u + off);
	outWords8[3] = REG32(0xe005800cu + off);
	outWords8[4] = REG32(0xe0058010u + off);
	outWords8[5] = REG32(0xe0058014u + off);
	outWords8[6] = REG32(0xe0058018u + off);
	outWords8[7] = REG32(0xe005801cu + off);
}

/* 0x80b2dc (name ours) */
int spkgValidateArmRange(uint32_t addr, uint32_t size)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} dramSize;

	dramSize.b[0] = REG8(0xe0062260u);
	dramSize.b[1] = REG8(0xe0062261u);
	dramSize.b[2] = REG8(0xe0062262u);
	dramSize.b[3] = REG8(0xe0062263u);

	if (addr >= 0x40200000u) {
		uint64_t dramEnd = (uint64_t)dramSize.w + 0x40000000ULL;

		if ((uint64_t)addr < dramEnd) {
			uint64_t reqEnd = (uint64_t)addr + (uint64_t)size;

			if (reqEnd <= dramEnd)
				return 1;
		}
	} else if (addr <= 0x1effffffu) {
		return 0;
	}

	if (addr <= 0x1f007fffu) {
		uint32_t reqEnd = addr + size;

		if (reqEnd >= size && reqEnd <= 0x1f008000u)
			return 1;
	} else if (addr >= 0x1f840000u && addr <= 0x1f85ffffu) {
		uint32_t reqEnd = addr + size;

		return (reqEnd >= size && reqEnd <= 0x1f860000u);
	}
	return 0;
}

/* ============================================================================
 * SpkgVerifier Class Methods (0x80b3b0..0x80bf34)
 * ============================================================================ */

/* 0x80b3b0 (name ours) */
void spkgVerifierDtorD2(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
}

/* 0x80b3b8 (name ours) */
void spkgVerifierDtorD0(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
	operatorDelete(v);
}

/* 0x80b3c2 (name ours) */
void spkgVerifierCtorC2(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
	v->req = NULL;
	v->isPrepared = 0u;
}

/* 0x80b3d4 (name ours) */
void spkgVerifierCtorC1(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
	v->req = NULL;
	v->isPrepared = 0u;
}

/* 0x80b3e6 (name ours) */
void spkgVerifierDtorD1(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
}

/* 0x80b3ee (name ours) */
void spkgVerifierCleanup(SpkgVerifier *v)
{
	(void)v;
}

/* 0x80b3f0 (name ours) */
int spkgCheckCfHeader(SpkgVerifier *v, const void *cfHdr)
{
	const uint32_t *words = (const uint32_t *)cfHdr;
	uint16_t category;

	(void)v;
	if (cfHdr == NULL)
		return 0;
	if ((words[0] & 0x00ffffffu) != 0x00454353u || words[1] != 3u)
		return 0;

	category = *(const uint16_t *)((const uint8_t *)cfHdr + 10u);
	return (category == 3u || category == 5u);
}

/* 0x80b420 (name ours) */
int spkgCheckExtHeaderMagic(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	(void)v;
	if (extHdr == NULL)
		return 0;
	return (extHdr->magic == 4u);
}

/* 0x80b42e (name ours) */
int spkgCheckMinVersionFloor(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	uint32_t slotBuf[8];
	uint8_t cid[16];
	int status = 0;

	(void)v;
	aimgrClearConsoleId2(cid);
	if (extHdr->pkgType == 1u || extHdr->pkgType == 9u) {
		uint32_t minSysver;
		uint16_t prodCode;
		uint16_t subCode;
		int isTestSub4 = 0;

		spkgReadKeyringSlot32(0x50fu, slotBuf);
		minSysver = slotBuf[0];
		spkgReadKeyringSlot32(0x509u, slotBuf);

		prodCode = (uint16_t)be16tohSm(*(const uint16_t *)((const uint8_t *)slotBuf + 4u));
		if (prodCode == 0x0101u) {
			subCode = (uint16_t)be16tohSm(*(const uint16_t *)((const uint8_t *)slotBuf + 6u));
			if (subCode == 0x0004u)
				isTestSub4 = 1;
		}
		if (aimgrIsTool(cid) == 0 && !isTestSub4 && extHdr->version < minSysver)
			status = SCE_SBL_ERROR_SPKG_VERSION_FLOOR;
	}
	aimgrClearConsoleId(cid);
	return status;
}

/* 0x80b4c8 (name ours) */
int spkgReturnZero(void)
{
	return 0;
}

/* 0x80b4cc (name ours) */
int spkgFindRangeEntry(const SceSmRangeEntry *entries, uint32_t count,
		const uint64_t *targetOffset, uint32_t *outIdx,
		uint32_t *outAccumLow, uint64_t *outInEntryOffset)
{
	uint64_t target = *targetOffset;
	uint64_t accum = 0ULL;
	uint32_t i;

	for (i = 0u; i < count; i++) {
		uint64_t nextAccum = accum + (uint64_t)entries[i].size;

		if (nextAccum > target) {
			*outIdx = i;
			*outAccumLow = (uint32_t)accum;
			*outInEntryOffset = target - accum;
			return 0;
		}
		accum = nextAccum;
	}

	*outAccumLow = (uint32_t)accum;
	return SCE_SBL_ERROR_SPKG_OUT_OF_RANGE;
}

/* 0x80b542 (name ours) */
int spkgReadSegmentCallback(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outChunk, uint32_t *outFlag, void *arg)
{
	SpkgStreamCbContext *cbCtx = (SpkgStreamCbContext *)arg;
	SpkgVerifyRequest *req = cbCtx->req;
	uint64_t inEntryOff = 0ULL;
	uint32_t accumLow = 0u;
	uint32_t entryIdx = 0xffffffffu;
	const SceSmRangeEntry *ent = NULL;
	uint32_t paddr;
	uint32_t entrySize;
	uint32_t endAddr;
	uint64_t resolvedAddr;
	uint64_t remInEntry;

	if (req->listCount > 481u)
		return SCE_SBL_ERROR_SPKG_INVALID_VALUE;

	if (req->mode == 1ULL) {
		uint64_t remOff = *offset;
		const SpkgScatterListDesc *curDesc = NULL;

		while (cbCtx->listIdx < (uint64_t)req->listCount) {
			curDesc = &req->u.lists[(uint32_t)cbCtx->listIdx];
			if (spkgFindRangeEntry(curDesc->entries,
					curDesc->byteSize >> 3, &remOff,
					&entryIdx, &accumLow, &inEntryOff) == 0) {
				break;
			}
			remOff -= (uint64_t)accumLow;
			cbCtx->listIdx++;
		}
		if (cbCtx->listIdx == (uint64_t)req->listCount ||
		    curDesc == NULL || entryIdx == 0xffffffffu) {
			return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
		}
		ent = &curDesc->entries[entryIdx];
	} else {
		if (spkgFindRangeEntry(req->u.inlineEntries,
				req->inlineEntryCount, offset,
				&entryIdx, &accumLow, &inEntryOff) != 0) {
			return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
		}
		ent = &req->u.inlineEntries[entryIdx];
	}

	paddr = ent->paddr;
	entrySize = ent->size;
	endAddr = paddr + entrySize;
	if (endAddr < paddr)
		return SCE_SBL_ERROR_SPKG_INVALID_ADDR;

	if (paddr <= 0x0005ffffu) {
		if (endAddr > 0x00040000u)
			return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
	} else if (paddr <= 0x003fffffu) {
		if (endAddr > 0x00300000u)
			return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
	} else if (paddr <= 0x0081ffffu) {
		if (endAddr > 0x00800000u)
			return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
	} else if (paddr > 0xdfffffffu || endAddr > 0xdfffffffu) {
		return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
	}

	if (paddr <= 0x401fffffu && endAddr > 0x40000000u)
		return SCE_SBL_ERROR_INVALID_ADDR;

	resolvedAddr = (uint64_t)paddr + inEntryOff;
	*outSrcAddr = resolvedAddr;
	*outDstAddr = resolvedAddr;

	remInEntry = (uint64_t)entrySize - inEntryOff;
	*outChunk = (remInEntry < (uint64_t)size) ? (uint32_t)remInEntry : size;
	*outFlag = 0u;

	if (cbCtx->segType == 1u) {
		if (cbCtx->segIdx == 1u && *outChunk == 64u) {
			void *dst = &req->decryptedExtHdr1;

			memcpy(dst, (const void *)(uintptr_t)*outSrcAddr, 64u);
			*outDstAddr = (uint64_t)(int64_t)(intptr_t)dst;
			*outSrcAddr = (uint64_t)(int64_t)(intptr_t)dst;
		}
	} else if (cbCtx->segType == 2u) {
		if (cbCtx->segIdx == 2u && *outChunk == 64u) {
			void *dst = req->decryptedSeg2;

			memcpy(dst, (const void *)(uintptr_t)*outSrcAddr, 64u);
			*outDstAddr = (uint64_t)(int64_t)(intptr_t)dst;
			*outSrcAddr = (uint64_t)(int64_t)(intptr_t)dst;
		}
	}
	return 0;
}

/* 0x80b7b0 (name ours) */
int spkgCopyFromScatterEntries(const SceSmRangeEntry *entries, uint32_t count,
		uintptr_t dstAddr, const uint64_t *offset, uint32_t size,
		uint32_t *outRead)
{
	uint64_t inEntryOff = 0ULL;
	uint32_t accumLow = 0u;
	uint32_t entryIdx = 0u;
	uint64_t remToRead = (uint64_t)size;
	uint64_t copied = 0ULL;
	uint32_t paddr;
	uint32_t entrySize;

	if (spkgFindRangeEntry(entries, count, offset,
			&entryIdx, &accumLow, &inEntryOff) != 0) {
		return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
	}

	paddr = entries[entryIdx].paddr;
	entrySize = entries[entryIdx].size;
	for (;;) {
		uint64_t avail;
		uint32_t chunk;

		if (spkgValidateArmRange(paddr, entrySize) == 0)
			return SCE_SBL_ERROR_SPKG_INVALID_VALUE;

		avail = (uint64_t)entrySize - inEntryOff;
		chunk = (avail < remToRead) ? (uint32_t)avail : (uint32_t)remToRead;
		cfCopyBytes((void *)(dstAddr + (uint32_t)copied),
				(const void *)(uintptr_t)(paddr + (uint32_t)inEntryOff),
				chunk, NULL);

		remToRead -= (uint64_t)chunk;
		copied += (uint64_t)chunk;
		inEntryOff = 0ULL;
		if (remToRead == 0ULL)
			break;

		entryIdx++;
		if (entryIdx >= count)
			break;
		paddr = entries[entryIdx].paddr;
		entrySize = entries[entryIdx].size;
	}

	*outRead = (uint32_t)copied;
	return 0;
}

/* 0x80b8aa (name ours) */
int spkgReadHeaderCallback(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg)
{
	SpkgVerifyRequest *req = *(SpkgVerifyRequest **)arg;

	if (req->listCount > 481u)
		return SCE_SBL_ERROR_SPKG_INVALID_VALUE;

	if (req->mode == 1ULL) {
		uint64_t remOff = *offset;
		uint32_t totalRead = 0u;
		uint32_t i;

		for (i = 0u; i < req->listCount; i++) {
			const SpkgScatterListDesc *desc = &req->u.lists[i];
			uint32_t listRead = 0u;
			int status;

			if (spkgValidateArmRange((uint32_t)(uintptr_t)desc->entries,
					desc->byteSize) == 0) {
				return SCE_SBL_ERROR_SPKG_INVALID_VALUE;
			}
			status = spkgCopyFromScatterEntries(desc->entries,
					desc->byteSize >> 3,
					dstAddr + totalRead, &remOff,
					size - totalRead, &listRead);
			if (status != 0) {
				remOff -= (uint64_t)listRead;
			} else {
				totalRead += listRead;
				if (totalRead == size)
					break;
			}
		}
		if (i == req->listCount)
			return SCE_SBL_ERROR_SPKG_INVALID_ADDR;

		*outRead = totalRead;
		return 0;
	}

	return spkgCopyFromScatterEntries(req->u.inlineEntries,
			req->inlineEntryCount, dstAddr, offset, size, outRead);
}

/* 0x80b9b0 (name ours) */
int spkgReadPlainHeaderBytes(void *dst, SpkgVerifyRequest *req,
		uint32_t offset, uint32_t size)
{
	uint64_t off64 = (uint64_t)offset;
	uint32_t readBytes = 0u;
	SpkgVerifyRequest *reqPtr = req;
	int status;

	if (dst == NULL || req == NULL || size == 0u)
		return SCE_SBL_ERROR_SPKG_INVALID_VALUE;

	status = spkgReadHeaderCallback((uintptr_t)dst, &off64, size,
			&readBytes, &reqPtr);
	if (status == 0 && readBytes != size)
		return SCE_SBL_ERROR_SPKG_INVALID_ADDR;
	return status;
}

/* 0x80b9f8 (name ours) */
int spkgValidateUnencryptedHeaders(SpkgVerifier *v, SpkgVerifyRequest *req)
{
	uint8_t cfHdr[48];
	SpkgExtHeader extHdr;
	uint32_t extHdrOff;
	int status;

	if (req == NULL)
		return SCE_SBL_ERROR_SPKG_INVALID_VALUE;

	memset(cfHdr, 0, sizeof(cfHdr));
	memset(&extHdr, 0, sizeof(extHdr));

	status = spkgReadPlainHeaderBytes(cfHdr, req, 0u, sizeof(cfHdr));
	if (status != 0)
		return status;
	if (spkgCheckCfHeader(v, cfHdr) == 0)
		return SCE_SBL_ERROR_SPKG_BAD_HEADER;

	extHdrOff = *(const uint32_t *)(const void *)(cfHdr + 0x10u);
	status = spkgReadPlainHeaderBytes(&extHdr, req, extHdrOff, sizeof(extHdr));
	if (status != 0)
		return status;
	if (extHdr.magic != 4u)
		return SCE_SBL_ERROR_SPKG_BAD_HEADER;
	if (spkgCheckMinVersionFloor(v, &extHdr) != 0)
		return SCE_SBL_ERROR_SPKG_VERSION_FLOOR;

	return 0;
}

/* 0x80ba8e (name ours) */
int spkgVerifierPrepare(SpkgVerifier *v, SpkgVerifyRequest *req)
{
	if (req == NULL)
		return SCE_SBL_ERROR_SPKG_INVALID_VALUE;
	if (spkgValidateUnencryptedHeaders(v, req) != 0)
		return SCE_SBL_ERROR_SPKG_PREPARE_FAILED;

	v->req = req;
	v->isPrepared = 1u;
	return 0;
}

/* 0x80baca (name ours) */
int spkgCheckTargetCompatibility(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	uint8_t cid[16];
	int status = SCE_SBL_ERROR_SPKG_TARGET_MISMATCH;

	(void)v;
	aimgrClearConsoleId2(cid);
	if (extHdr->targetFlags == 0u) {
		status = 0;
		goto done;
	}

	if (aimgrIsRetailOrChord(cid) != 0) {
		if ((extHdr->targetFlags & 0x40000000u) != 0u) {
			status = 0;
			goto done;
		}
	}
	if (aimgrIsDex(cid) != 0) {
		if ((extHdr->targetFlags & 0x20000000u) != 0u) {
			status = 0;
			goto done;
		}
	}
	if (aimgrIsTest(cid) != 0 || aimgrIsTool(cid) != 0) {
		if ((extHdr->targetFlags & 0x10000000u) != 0u) {
			status = 0;
			goto done;
		}
	}
	if (aimgrIsTest(cid) != 0 || aimgrIsTool(cid) != 0) {
		if (spkgIsQaFlagSet() != 0u && (extHdr->qaFlags & 1u) != 0u) {
			status = 0;
			goto done;
		}
	}
	if (aimgrIsDex(cid) != 0) {
		if ((extHdr->targetFlags & 0x40000000u) != 0u &&
		    spkgIsQaFlagSet() != 0u &&
		    (extHdr->qaFlags & 1u) != 0u) {
			status = 0;
		}
	}

done:
	aimgrClearConsoleId(cid);
	return status;
}

/* 0x80bb96 (name ours) */
int spkgLookupHardwareRevEntry(SpkgVerifier *v, uint32_t *outIdx)
{
	uint8_t decodedCid[16];
	uint8_t rawCid[16];
	uint32_t ruleIdx = 0u;
	int status;

	(void)v;
	aimgrClearConsoleId2(rawCid);
	status = aimgrGetConsoleId(rawCid, decodedCid);
	if (status == 0) {
		if (aimgrIsTool(rawCid) != 0) {
			ruleIdx = 1u;
		} else {
			uint16_t prodCode = *(const uint16_t *)(const void *)(decodedCid + 4u);
			uint16_t subCode = *(const uint16_t *)(const void *)(decodedCid + 6u);
			uint32_t i;

			for (i = 0u; i < 88u; i++) {
				if (prodCode >= s_spkgHwRevMap[i].minProdCode &&
				    prodCode <= s_spkgHwRevMap[i].maxProdCode &&
				    subCode == s_spkgHwRevMap[i].subCode) {
					ruleIdx = s_spkgHwRevMap[i].ruleIdx;
					break;
				}
			}
		}
	}
	*outIdx = ruleIdx;
	aimgrClearConsoleId(rawCid);
	return status;
}

/* 0x80bc1e (name ours) */
uint32_t spkgGetHardwareAttrMask(SpkgVerifier *v, uint32_t idx)
{
	(void)v;
	if (idx < 90u)
		return s_spkgHwRevRules[idx].attrMask;
	return 0u;
}

/* 0x80bc3a (name ours) */
int spkgIsVersionBelowHwMin(SpkgVerifier *v, uint32_t pkgType,
		uint32_t pkgVer, uint32_t defaultMinVer)
{
	uint32_t ruleIdx = 0u;
	const SpkgHwRevRule *rules;
	uint32_t minVer;

	if (pkgType - 1u > 8u)
		return 0;

	rules = s_spkgTypeRuleTables[pkgType - 1u];
	if (rules == NULL)
		return 0;

	if (spkgLookupHardwareRevEntry(v, &ruleIdx) != 0)
		return 1;

	minVer = rules[ruleIdx].minVer;
	if (minVer == 0xffffffffu)
		minVer = defaultMinVer;
	return (pkgVer < minVer);
}

/* 0x80bc92 (name ours) */
int spkgCheckPkgVersionAndChassis(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	uint32_t slotBuf[8];
	uint32_t slot50b[8];
	uint8_t cid[16];
	uint32_t defaultMinVer = 0u;
	uint8_t allowDowngradeBit;
	int status = 0;

	aimgrClearConsoleId2(cid);
	if (extHdr->pkgType == 1u) {
		spkgReadKeyringSlot32(0x518u, slotBuf);
		defaultMinVer = slotBuf[0];
	} else if (extHdr->pkgType == 9u) {
		spkgReadKeyringSlot32(0x50eu, slotBuf);
		defaultMinVer = slotBuf[0];
	}

	spkgReadKeyringSlot32(0x50bu, slot50b);
	allowDowngradeBit = (uint8_t)slot50b[0];

	if (extHdr->pkgType == 1u || extHdr->pkgType == 9u) {
		spkgReadKeyringSlot32(0x50au, slot50b);
		if ((((const uint8_t *)slot50b)[15] & 2u) == 0u &&
		    aimgrIsRetailOrChord(cid) != 0 &&
		    extHdr->version < defaultMinVer) {
			if ((allowDowngradeBit & 1u) == 0u ||
			    (extHdr->qaFlags & 2u) == 0u) {
				status = SCE_SBL_ERROR_SPKG_HW_REV_FLOOR;
				goto done;
			}
		}
		if (spkgIsVersionBelowHwMin(v, extHdr->pkgType,
				extHdr->version, defaultMinVer) != 0) {
			status = SCE_SBL_ERROR_SPKG_HW_REV_FLOOR;
		}
	}

done:
	aimgrClearConsoleId(cid);
	return status;
}

/* 0x80bd56 (name ours) */
int spkgCheckFactoryUpdatePolicy(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	uint32_t slot519[8];

	(void)v;
	if ((extHdr->qaFlags & 1u) != 0u && spkgIsQaFlagSet() == 0u) {
		spkgReadKeyringSlot32(0x519u, slot519);
		if (((slot519[0] >> 5) | slot519[0]) & 1u)
			return 0;
		spkgReadKeyringSlot32(0x519u, slot519);
		if (((slot519[0] >> 6) & 1u) == 0u)
			return SCE_SBL_ERROR_SPKG_FACTORY_DENIED;
	}
	return 0;
}

/* 0x80bdb0 (name ours) */
int spkgVerifierExecute(SpkgVerifier *v)
{
	SelfAuthContext cfCtx;
	SpkgVerifyRequest *reqPtr;
	int64_t expectedHeaderLen = 0x400LL;
	uint8_t rsaKey[260];
	uint32_t i;
	int status;

	if (v->isPrepared == 0u)
		return SCE_SBL_ERROR_SPKG_PREPARE_FAILED;

	cfCtx.hdrBuf = s_spkgHdrBuf;
	cfCtx.hdrBufSize = sizeof(s_spkgHdrBuf);
	memcpy(rsaKey, s_spkgRsaModulus, 256u);
	memcpy(rsaKey + 256u, s_spkgRsaExponent, 4u);
	reqPtr = v->req;

	status = selfInitAndVerifyHeader(&cfCtx, s_spkgMetaKey, rsaKey, 5u,
			0, 3u, spkgReadHeaderCallback, &reqPtr,
			0, &expectedHeaderLen);
	if (status != 0)
		return status;

	for (i = 0u; i < 3u; i++) {
		SpkgStreamCbContext cbCtx;
		const SceCfMetadataSection *sec = NULL;

		memset(&cbCtx, 0, sizeof(cbCtx));
		cbCtx.req = v->req;
		cfGetSectionEntry(&cfCtx, i, &sec);
		cbCtx.segType = sec->type;
		cbCtx.segIdx = sec->index;

		status = selfDecryptAndLoadSegment(&cfCtx,
				s_spkgSegmentSpecs[i].segType,
				s_spkgSegmentSpecs[i].segIdx,
				spkgReadSegmentCallback, &cbCtx,
				NULL, NULL);
		if (status != 0)
			return status;
	}

	if (&v->req->decryptedExtHdr1 == NULL ||
	    v->req->decryptedExtHdr1.magic != 4u ||
	    spkgCheckMinVersionFloor(v, &v->req->decryptedExtHdr1) != 0) {
		return SCE_SBL_ERROR_SPKG_PREPARE_FAILED;
	}

	status = spkgCheckTargetCompatibility(v, &v->req->decryptedExtHdr1);
	if (status == 0) {
		status = spkgCheckFactoryUpdatePolicy(v, &v->req->decryptedExtHdr1);
		if (status == 0)
			status = spkgCheckPkgVersionAndChassis(v, &v->req->decryptedExtHdr1);
	}
	return status;
}

/* 0x80bf10 (name ours) */
uint32_t spkgIsQaFlagSet(void)
{
	uint32_t slot50a[8];

	spkgReadKeyringSlot32(0x50au, slot50a);
	return ((const uint8_t *)slot50a)[15] & 1u;
}
