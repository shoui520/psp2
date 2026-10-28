/*
 * update_service_sm.c: PlayStation Vita Update Service Secure Module
 * (os0:sm/update_service_sm.self, SceSblUpdateServiceSm).
 *
 * Implements the 13-command system software update service:
 *   - Command 0x00010002: Verify SCESLB2 update list package & cache entry hashes
 *   - Command 0x00020002: Verify scatter-gather entry HMAC-SHA256 against update list
 *   - Command 0x00030002: Decrypt & verify 4096-byte "ENC IS" package
 *   - Command 0x00040002: Verify & decrypt SPKG Certified File header and segments
 *   - Command 0x00050002: Execute Bigmac AES-128-CBC LLI scatter-gather transfer
 *   - Command 0x00060002: Encrypt & write SNVS sectors (1..31)
 *   - Command 0x00070002: Read & decrypt SNVS sectors (1..31)
 *   - Command 0x00080002: Update SNVS sector 0 status words
 *   - Command 0x00090002: Read SNVS sector 0 status words
 *   - Command 0x000a0002: Decrypt & verify 1024-byte "ENC ID" package
 *   - Command 0x000b0002: SNVS sector challenge/response/commit protocol (ops 0..3)
 *   - Command 0x000c0002: SNVS sector 0 header challenge/response/commit protocol (ops 0..3)
 *   - Command 0x000d0002: Key slot 15 Auth1/Auth2 subcmd 4 protocol & keyring load (ops 0..4)
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "update_service_sm.h"

/* ============================================================================
 * .rodata & .data Constants and Keys (0x8124c0..0x813c5c)
 * ============================================================================ */

static const uintptr_t s_updHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)updHookInvoke,
	0u,
};

static const uintptr_t s_updBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u,
};

static const UpdEncRsaPubKey s_updEncIdRsaPubKey = {
	.modulus = {
#if FW_DEBUG
		0xb3u, 0xf1u, 0xd9u, 0xb6u, 0xc3u, 0x67u, 0x00u, 0x47u,
		0xe1u, 0xbeu, 0xccu, 0xb0u, 0xaau, 0x9eu, 0x6cu, 0xdeu,
		0xb9u, 0xc3u, 0x5au, 0x89u, 0x75u, 0x8au, 0xbeu, 0xf7u,
		0x08u, 0x36u, 0x6eu, 0x6fu, 0x67u, 0x49u, 0xa9u, 0xdcu,
		0x67u, 0x72u, 0x88u, 0x2bu, 0x44u, 0x28u, 0xf5u, 0x2du,
		0x15u, 0x49u, 0x9fu, 0x14u, 0x7bu, 0x57u, 0xb7u, 0x3du,
		0x38u, 0x42u, 0xb9u, 0x96u, 0x9cu, 0x6eu, 0x95u, 0x1au,
		0xd3u, 0x95u, 0xe6u, 0x70u, 0x6eu, 0x7au, 0x55u, 0x5cu,
		0x08u, 0x09u, 0x81u, 0x3du, 0x10u, 0x19u, 0xd7u, 0x29u,
		0x8bu, 0x77u, 0xebu, 0x37u, 0x4fu, 0x93u, 0x93u, 0xf5u,
		0x97u, 0x58u, 0x7cu, 0x85u, 0x0cu, 0x87u, 0x34u, 0x25u,
		0x92u, 0xe3u, 0x93u, 0x31u, 0x04u, 0x14u, 0x84u, 0x9cu,
		0x3fu, 0x2eu, 0x44u, 0x48u, 0xd5u, 0x00u, 0x0fu, 0x9fu,
		0x60u, 0xccu, 0xbfu, 0x40u, 0xd1u, 0x9du, 0x5fu, 0xabu,
		0xdbu, 0x90u, 0xfeu, 0x57u, 0x3cu, 0x4du, 0x22u, 0x5cu,
		0xf3u, 0x20u, 0xecu, 0x5du, 0x6bu, 0xc7u, 0xcau, 0x68u,
		0x69u, 0xcdu, 0x04u, 0x79u, 0x48u, 0xe8u, 0xc0u, 0x92u,
		0x15u, 0x78u, 0x7au, 0xefu, 0xfbu, 0x82u, 0x93u, 0x40u,
		0x24u, 0x0cu, 0xf8u, 0x63u, 0x5bu, 0x9eu, 0x73u, 0xc7u,
		0x8fu, 0x77u, 0x76u, 0x7eu, 0x74u, 0xb9u, 0x7au, 0x6du,
		0x4cu, 0xe5u, 0xb9u, 0x94u, 0xf5u, 0x81u, 0x33u, 0x87u,
		0xf1u, 0xf4u, 0x7du, 0x4fu, 0xf5u, 0x0du, 0x7du, 0x79u,
		0x0eu, 0x60u, 0x94u, 0x2cu, 0x60u, 0x40u, 0xbau, 0xc5u,
		0x8bu, 0x96u, 0xfbu, 0xbcu, 0x2du, 0x09u, 0xf4u, 0xbbu,
		0x23u, 0xdbu, 0x20u, 0xd7u, 0x63u, 0xc4u, 0x11u, 0x6du,
		0x06u, 0x35u, 0x7fu, 0x8fu, 0x84u, 0x4du, 0x96u, 0xb0u,
		0x2du, 0x9au, 0x3cu, 0xb8u, 0x1du, 0xa0u, 0x12u, 0x81u,
		0x1eu, 0xbdu, 0x93u, 0x7du, 0xb4u, 0x96u, 0xf7u, 0x2bu,
		0x61u, 0x42u, 0xe0u, 0xa1u, 0xbeu, 0x0au, 0xd0u, 0x9bu,
		0xfeu, 0x99u, 0xabu, 0xd3u, 0x32u, 0xdbu, 0x2eu, 0xdau,
		0x56u, 0x52u, 0xa6u, 0x40u, 0x04u, 0xd8u, 0x28u, 0x17u,
		0x6au, 0x7cu, 0x7cu, 0x3cu, 0x47u, 0x42u, 0x8du, 0xabu,
#else
		0xd5u, 0x1cu, 0xb2u, 0xfau, 0x3au, 0xa3u, 0xb7u, 0xcdu,
		0x8cu, 0xe9u, 0x42u, 0x09u, 0x2au, 0x83u, 0x0cu, 0x15u,
		0xb2u, 0x9cu, 0xa5u, 0x65u, 0xc4u, 0x11u, 0x61u, 0xa5u,
		0xd4u, 0x70u, 0x55u, 0x70u, 0x6bu, 0xeau, 0x3eu, 0xceu,
		0x27u, 0xeeu, 0x4du, 0x7fu, 0x1eu, 0xa2u, 0x0eu, 0x5du,
		0x03u, 0x92u, 0x0cu, 0x18u, 0x5au, 0xb9u, 0x22u, 0xdfu,
		0xb2u, 0xf8u, 0x00u, 0xecu, 0x60u, 0x85u, 0x3eu, 0xdcu,
		0xc2u, 0x9fu, 0xe1u, 0xb0u, 0x86u, 0x55u, 0x3cu, 0x0eu,
		0x9cu, 0x81u, 0x32u, 0x65u, 0x92u, 0x49u, 0x91u, 0xaeu,
		0xffu, 0x2eu, 0xf6u, 0xbau, 0xa2u, 0xceu, 0xc3u, 0xb5u,
		0xb0u, 0xb8u, 0x87u, 0x0du, 0x17u, 0x5cu, 0x77u, 0xa0u,
		0x72u, 0x12u, 0xe2u, 0x15u, 0x3bu, 0x1cu, 0x55u, 0xe0u,
		0x56u, 0xa5u, 0x84u, 0x79u, 0x02u, 0x91u, 0x42u, 0xfbu,
		0x31u, 0xf3u, 0xc1u, 0xdfu, 0x13u, 0x12u, 0x7du, 0xa7u,
		0x0du, 0x23u, 0x0bu, 0x43u, 0x44u, 0xd0u, 0xa0u, 0xd3u,
		0xeeu, 0xcbu, 0x2bu, 0x0cu, 0x7au, 0xe9u, 0x72u, 0xb4u,
		0xa4u, 0x57u, 0x59u, 0xdfu, 0x59u, 0xdcu, 0xecu, 0x25u,
		0xb7u, 0xebu, 0x76u, 0xdau, 0x0cu, 0xbfu, 0x3fu, 0x2cu,
		0x74u, 0xe2u, 0x42u, 0xbdu, 0x48u, 0xb9u, 0x9fu, 0x68u,
		0x8du, 0x17u, 0xd3u, 0x0cu, 0x02u, 0x10u, 0x3fu, 0xa2u,
		0xd2u, 0xbfu, 0x99u, 0x49u, 0x70u, 0x4cu, 0x30u, 0x12u,
		0x3cu, 0x20u, 0xd5u, 0x7eu, 0x91u, 0xecu, 0x91u, 0xdeu,
		0x36u, 0x2bu, 0x54u, 0x8bu, 0x8bu, 0x06u, 0xdeu, 0xd9u,
		0xe4u, 0x1cu, 0x3fu, 0x99u, 0x19u, 0xc9u, 0x5bu, 0xdau,
		0xc0u, 0x4fu, 0xadu, 0x45u, 0x6bu, 0xa9u, 0x06u, 0xa2u,
		0x3bu, 0xf9u, 0xcau, 0x57u, 0x7bu, 0xf4u, 0x63u, 0x67u,
		0xecu, 0x9bu, 0xb7u, 0x93u, 0xe3u, 0x07u, 0x21u, 0x6eu,
		0x7eu, 0x1cu, 0x0fu, 0xabu, 0xeau, 0x4fu, 0xc2u, 0x4bu,
		0x09u, 0xaeu, 0x56u, 0x7au, 0xccu, 0xf8u, 0x31u, 0xc7u,
		0x81u, 0x0eu, 0xedu, 0x04u, 0xa0u, 0x4bu, 0x27u, 0x5cu,
		0xd9u, 0xc5u, 0xbbu, 0x2du, 0x2du, 0x6fu, 0x7fu, 0x62u,
		0x33u, 0xebu, 0x00u, 0x8du, 0xe9u, 0x3cu, 0xc0u, 0xbbu,
#endif
	},
	.modulusBytes = 0x00000100u,
	.exponent = { 0x00u, 0x01u, 0x00u, 0x01u },
	.exponentBytes = 0x00000004u,
};

static const UpdEncRsaPubKey s_updEncIsRsaPubKey = {
	.modulus = {
#if FW_DEBUG
		0xbbu, 0x1eu, 0x13u, 0x96u, 0x36u, 0x48u, 0xb5u, 0xecu,
		0x36u, 0xecu, 0x4du, 0xddu, 0x00u, 0xddu, 0x37u, 0x52u,
		0x53u, 0xb4u, 0x7au, 0x3fu, 0xd4u, 0x52u, 0xbbu, 0x67u,
		0xf1u, 0xfbu, 0x8bu, 0x09u, 0x67u, 0xddu, 0xabu, 0x08u,
		0xaau, 0x82u, 0x45u, 0x62u, 0x00u, 0xccu, 0x90u, 0x06u,
		0xd2u, 0x7du, 0xd0u, 0x01u, 0x44u, 0xf3u, 0x6cu, 0x5cu,
		0x98u, 0xefu, 0x4au, 0x2cu, 0x53u, 0x07u, 0x4cu, 0x65u,
		0x49u, 0x78u, 0xf4u, 0x9au, 0xe8u, 0x2eu, 0x25u, 0x89u,
		0x88u, 0x7eu, 0x2du, 0xf2u, 0x4eu, 0x74u, 0x8bu, 0xdbu,
		0x1bu, 0x2eu, 0xe2u, 0x22u, 0xf8u, 0xc6u, 0xecu, 0x6eu,
		0x0du, 0xf3u, 0xc3u, 0x26u, 0x66u, 0x20u, 0x98u, 0x82u,
		0xc0u, 0x08u, 0xd6u, 0x6au, 0xeeu, 0x35u, 0xf1u, 0x8fu,
		0xd8u, 0xa8u, 0x10u, 0xb8u, 0x86u, 0xb1u, 0xa9u, 0x26u,
		0xc3u, 0xc9u, 0x69u, 0x06u, 0x48u, 0x2fu, 0xabu, 0x7bu,
		0x10u, 0x3eu, 0xf7u, 0x69u, 0x00u, 0xfeu, 0x7cu, 0x1du,
		0xf1u, 0x30u, 0x41u, 0x05u, 0x13u, 0x98u, 0xcau, 0xe8u,
		0x83u, 0xb8u, 0x48u, 0x50u, 0x69u, 0xf9u, 0x8cu, 0xb7u,
		0x8cu, 0xd2u, 0x75u, 0xc9u, 0x67u, 0x14u, 0xdbu, 0x22u,
		0xc9u, 0x1cu, 0xf1u, 0x9bu, 0x4bu, 0xe2u, 0xc7u, 0x7au,
		0xc2u, 0x28u, 0xc9u, 0x3du, 0xfau, 0x0bu, 0x09u, 0xdau,
		0xadu, 0xe4u, 0xacu, 0xfdu, 0xa2u, 0x6du, 0x66u, 0xdfu,
		0xfdu, 0x18u, 0xddu, 0xe5u, 0x88u, 0x95u, 0xa3u, 0x4cu,
		0xa6u, 0x37u, 0x7cu, 0x25u, 0x94u, 0x60u, 0x3fu, 0xe4u,
		0x1eu, 0x0cu, 0x50u, 0x62u, 0x15u, 0x07u, 0x61u, 0xd8u,
		0x73u, 0xfbu, 0x53u, 0x94u, 0xd1u, 0x03u, 0x2fu, 0xc0u,
		0x92u, 0x9fu, 0x9eu, 0xd6u, 0xc5u, 0xbeu, 0x05u, 0x1bu,
		0x03u, 0x9cu, 0x6cu, 0x5bu, 0xf5u, 0xd9u, 0x2bu, 0xebu,
		0xc3u, 0xecu, 0x83u, 0xecu, 0x06u, 0xe3u, 0xccu, 0x7bu,
		0x5eu, 0xc1u, 0xbcu, 0xe4u, 0x45u, 0x62u, 0x27u, 0x41u,
		0xcdu, 0x60u, 0xbeu, 0x2du, 0xdcu, 0xf8u, 0x38u, 0x48u,
		0xbcu, 0x86u, 0xb3u, 0xebu, 0xf9u, 0x87u, 0xc4u, 0x38u,
		0x32u, 0x0bu, 0x6du, 0xeau, 0xd9u, 0xd4u, 0xeeu, 0x27u,
#else
		0xd1u, 0x5fu, 0x54u, 0x27u, 0x2au, 0x76u, 0xd0u, 0x2au,
		0xe6u, 0x43u, 0x3cu, 0xdau, 0x56u, 0xb2u, 0x94u, 0x6du,
		0xd2u, 0xc9u, 0xfau, 0xf9u, 0xb0u, 0x3au, 0xdfu, 0x82u,
		0x50u, 0x5bu, 0x48u, 0xdau, 0x87u, 0x16u, 0x3cu, 0x82u,
		0xebu, 0xeau, 0xc1u, 0x51u, 0x84u, 0xc9u, 0x4fu, 0x35u,
		0x3du, 0x9au, 0xa5u, 0x34u, 0x3fu, 0xeeu, 0xb9u, 0x4eu,
		0x2fu, 0x11u, 0x52u, 0x1bu, 0xe5u, 0x76u, 0x1du, 0x5cu,
		0x35u, 0xb7u, 0x7au, 0x2du, 0xdfu, 0x74u, 0x8cu, 0x28u,
		0x48u, 0x91u, 0xabu, 0x2fu, 0x25u, 0xb9u, 0xd5u, 0x9bu,
		0x4bu, 0x91u, 0x2eu, 0x1du, 0x61u, 0xe1u, 0xc5u, 0x25u,
		0x4cu, 0x52u, 0x0bu, 0x27u, 0x4eu, 0xbau, 0xecu, 0xacu,
		0x17u, 0x98u, 0xd1u, 0x55u, 0xaeu, 0x66u, 0xd4u, 0x8eu,
		0x6eu, 0xa3u, 0xfbu, 0x5fu, 0xf0u, 0x3bu, 0xf0u, 0x54u,
		0xf0u, 0x4bu, 0xd4u, 0x5au, 0xa7u, 0xb5u, 0xe8u, 0x3cu,
		0xbfu, 0x19u, 0xe0u, 0xfbu, 0x91u, 0xeeu, 0x99u, 0x50u,
		0x42u, 0xa3u, 0x16u, 0x91u, 0x67u, 0x0au, 0x27u, 0x94u,
		0xddu, 0x71u, 0xaeu, 0xf1u, 0xdcu, 0xa9u, 0x91u, 0xc1u,
		0x04u, 0x55u, 0x0au, 0xc6u, 0x04u, 0xdau, 0xb2u, 0xe7u,
		0x97u, 0x5eu, 0x86u, 0xd2u, 0xa6u, 0xa5u, 0xc1u, 0xaeu,
		0x10u, 0x2bu, 0xaau, 0xc1u, 0x9au, 0xb9u, 0x19u, 0x3eu,
		0x30u, 0x7du, 0x41u, 0x02u, 0x7au, 0xd9u, 0x7du, 0x81u,
		0xedu, 0xd2u, 0x4eu, 0x7du, 0x08u, 0x35u, 0xbau, 0xbau,
		0x1bu, 0xc3u, 0x58u, 0xf7u, 0x48u, 0x8au, 0xa0u, 0x34u,
		0x9bu, 0xd7u, 0xcbu, 0xd7u, 0xb0u, 0xceu, 0x6eu, 0x93u,
		0x29u, 0xd5u, 0x9cu, 0x7cu, 0x7du, 0xebu, 0xb9u, 0x9bu,
		0x58u, 0xfcu, 0x3eu, 0x3eu, 0x1bu, 0x87u, 0x1fu, 0x4fu,
		0xf7u, 0x45u, 0x90u, 0xccu, 0xb4u, 0xe0u, 0x30u, 0xfau,
		0x0bu, 0xc0u, 0x88u, 0x9fu, 0x16u, 0xe1u, 0xfcu, 0x1au,
		0x7bu, 0x1cu, 0xdbu, 0xf6u, 0xbbu, 0x83u, 0x21u, 0xe1u,
		0x4eu, 0x33u, 0x34u, 0x33u, 0xcdu, 0x8eu, 0x7cu, 0xc7u,
		0x6fu, 0xa9u, 0xcfu, 0x54u, 0x9eu, 0xd5u, 0xa9u, 0x6au,
		0xa9u, 0xdeu, 0xecu, 0x24u, 0x90u, 0xffu, 0x86u, 0x61u,
#endif
	},
	.modulusBytes = 0x00000100u,
	.exponent = { 0x00u, 0x01u, 0x00u, 0x01u },
	.exponentBytes = 0x00000004u,
};

static const uint8_t s_updEncIdKeyMaterial[0x2a0] = {
#if FW_DEBUG
	0xabu, 0x70u, 0x97u, 0x35u, 0x6fu, 0xddu, 0x49u, 0xd8u,
	0x38u, 0x78u, 0x54u, 0x01u, 0x67u, 0xf0u, 0xc4u, 0xadu,
	0x85u, 0x53u, 0x7cu, 0x5au, 0x56u, 0xbdu, 0x15u, 0xdfu,
	0x0eu, 0xb5u, 0xf7u, 0xf0u, 0xd9u, 0xe2u, 0x76u, 0xe6u,
	0xb7u, 0xfcu, 0x10u, 0x26u, 0xb8u, 0x56u, 0xb5u, 0x25u,
	0xd0u, 0x0fu, 0x51u, 0xabu, 0xa7u, 0x9bu, 0x3fu, 0x22u,
	0xfbu, 0x57u, 0xa6u, 0x5au, 0xebu, 0x41u, 0x42u, 0x13u,
	0x6fu, 0x99u, 0xe3u, 0xeau, 0xf1u, 0x53u, 0xb5u, 0x5au,
	0xf3u, 0x74u, 0x7au, 0x33u, 0xa8u, 0x7du, 0xf8u, 0x51u,
	0xaeu, 0x65u, 0xbbu, 0xaau, 0xa8u, 0x3eu, 0x60u, 0xefu,
	0x60u, 0x83u, 0xabu, 0xa2u, 0x20u, 0xa5u, 0x2du, 0x0au,
	0x28u, 0xe8u, 0x0au, 0xefu, 0x56u, 0xe8u, 0x1du, 0x06u,
	0xa4u, 0x11u, 0x9bu, 0x89u, 0xefu, 0xb5u, 0x2cu, 0x7eu,
	0xd6u, 0xc2u, 0x59u, 0x8bu, 0xdcu, 0x5fu, 0xe7u, 0x68u,
	0xc9u, 0x18u, 0x04u, 0x2fu, 0x5du, 0x59u, 0x2bu, 0x65u,
	0x42u, 0xd8u, 0xf0u, 0xceu, 0x78u, 0x92u, 0x8au, 0xe3u,
	0x9au, 0xe1u, 0x5cu, 0x1du, 0x4du, 0x13u, 0x7du, 0x97u,
	0x12u, 0x94u, 0x7du, 0x39u, 0x56u, 0x50u, 0xa1u, 0xa0u,
	0x44u, 0x4cu, 0x3fu, 0x29u, 0x2bu, 0xfbu, 0x89u, 0x3eu,
	0x41u, 0xc4u, 0xedu, 0x64u, 0xebu, 0x30u, 0x10u, 0xe8u,
	0xadu, 0x6au, 0xfcu, 0x47u, 0x88u, 0x5eu, 0xb5u, 0xeau,
	0x1eu, 0xe7u, 0xc1u, 0xbeu, 0x60u, 0xdcu, 0xd0u, 0x44u,
	0xabu, 0x65u, 0x9du, 0xefu, 0xf0u, 0xc5u, 0x99u, 0x95u,
	0x64u, 0x52u, 0x21u, 0x1eu, 0xbcu, 0x10u, 0x97u, 0xfcu,
	0x51u, 0xf8u, 0xe8u, 0xcdu, 0x46u, 0xeeu, 0x59u, 0x97u,
	0x45u, 0xeau, 0x1fu, 0xbfu, 0x4bu, 0x50u, 0xd0u, 0xc5u,
	0x21u, 0xcfu, 0xd1u, 0x7au, 0x2fu, 0x9bu, 0x19u, 0x78u,
	0x1fu, 0x2bu, 0x66u, 0xe2u, 0xecu, 0x66u, 0xa4u, 0x4fu,
	0xc0u, 0xf0u, 0xc4u, 0x21u, 0x88u, 0x5cu, 0x25u, 0x21u,
	0x1eu, 0xb7u, 0x9bu, 0xf6u, 0x20u, 0xbdu, 0xc3u, 0xbeu,
	0xabu, 0x99u, 0xccu, 0xdau, 0xcdu, 0xecu, 0x21u, 0x55u,
	0x34u, 0x6cu, 0x9du, 0x7eu, 0x03u, 0x4fu, 0x28u, 0x88u,
	0x14u, 0x94u, 0xe4u, 0x10u, 0x02u, 0x09u, 0x04u, 0x51u,
	0xc4u, 0xb8u, 0x84u, 0x64u, 0xdbu, 0x4eu, 0x03u, 0x05u,
	0x1eu, 0x94u, 0xe5u, 0x50u, 0x8eu, 0x98u, 0x02u, 0x8bu,
	0xbcu, 0xe2u, 0x8du, 0x34u, 0x01u, 0x6eu, 0x81u, 0xc6u,
	0x67u, 0x95u, 0x2bu, 0xcbu, 0x85u, 0x9au, 0xeau, 0x69u,
	0x58u, 0x69u, 0x24u, 0x55u, 0xc0u, 0x56u, 0x76u, 0xb0u,
	0x27u, 0xabu, 0x39u, 0x33u, 0x21u, 0x45u, 0xccu, 0xc3u,
	0x63u, 0x63u, 0xe2u, 0x91u, 0x6au, 0x92u, 0xe4u, 0xccu,
	0xc9u, 0x1au, 0x49u, 0x33u, 0x24u, 0xd3u, 0x0bu, 0x4bu,
	0x00u, 0x96u, 0x22u, 0x42u, 0xd8u, 0x68u, 0xa2u, 0x61u,
	0x83u, 0x24u, 0xdcu, 0x39u, 0x26u, 0x21u, 0xb4u, 0x43u,
	0x84u, 0xa1u, 0x10u, 0xa5u, 0x79u, 0x40u, 0x6au, 0xebu,
	0x0bu, 0x1au, 0x72u, 0x43u, 0x19u, 0x7fu, 0x1fu, 0x42u,
	0x44u, 0x3bu, 0xf1u, 0x10u, 0xc6u, 0x16u, 0x71u, 0x32u,
	0xa9u, 0x45u, 0x82u, 0x06u, 0x5bu, 0x24u, 0x43u, 0x30u,
	0xbau, 0x60u, 0xbcu, 0xc3u, 0x5au, 0x1du, 0xf2u, 0xbfu,
	0x27u, 0x92u, 0x47u, 0x41u, 0x78u, 0x1du, 0x3eu, 0xb7u,
	0xb0u, 0xc2u, 0xe1u, 0x26u, 0x47u, 0xdbu, 0x8eu, 0x9cu,
	0x26u, 0xbfu, 0x1du, 0xcau, 0x8fu, 0xabu, 0x94u, 0x70u,
	0x37u, 0x8au, 0x48u, 0x83u, 0xf6u, 0xe3u, 0x27u, 0xb2u,
	0xc1u, 0xe5u, 0x75u, 0xfbu, 0x7fu, 0xfbu, 0x6cu, 0x89u,
	0x93u, 0x00u, 0xbeu, 0xefu, 0x46u, 0x8fu, 0xdeu, 0xd7u,
	0x7au, 0x5du, 0x2eu, 0x91u, 0x4bu, 0xb3u, 0xc7u, 0x7au,
	0x5du, 0x8eu, 0x35u, 0x96u, 0xc1u, 0xf9u, 0x53u, 0xecu,
	0xb4u, 0xc4u, 0xc6u, 0x9eu, 0xbdu, 0x02u, 0x51u, 0xbeu,
	0x71u, 0x6du, 0xcbu, 0xc8u, 0xb9u, 0x53u, 0x2cu, 0x4bu,
	0x61u, 0x3du, 0xc9u, 0x33u, 0x29u, 0x74u, 0x83u, 0x7au,
	0xf6u, 0x67u, 0x60u, 0xe7u, 0x8fu, 0xf9u, 0x80u, 0x2bu,
	0x48u, 0x1eu, 0xa4u, 0x48u, 0xb4u, 0x07u, 0xe0u, 0xbfu,
	0x3cu, 0xa7u, 0xe5u, 0x2cu, 0xcau, 0x97u, 0xe2u, 0xa9u,
	0x92u, 0xe0u, 0xc6u, 0xa9u, 0x28u, 0xb8u, 0x24u, 0xb7u,
	0xd7u, 0x17u, 0xf3u, 0x87u, 0xe7u, 0xc2u, 0x45u, 0xb7u,
	0x8bu, 0x16u, 0x56u, 0x7au, 0x36u, 0xb1u, 0x98u, 0x51u,
	0xadu, 0x29u, 0x8eu, 0x0fu, 0x48u, 0x4au, 0x4eu, 0xd8u,
	0xefu, 0x99u, 0x43u, 0x23u, 0x1du, 0xffu, 0xeau, 0xe3u,
	0xe4u, 0x5eu, 0x6au, 0x70u, 0xfeu, 0x24u, 0x5cu, 0x22u,
	0x4du, 0xe6u, 0x5cu, 0x2cu, 0x62u, 0xe3u, 0xf1u, 0x65u,
	0x80u, 0xa4u, 0x35u, 0x15u, 0x8eu, 0x41u, 0x21u, 0xe2u,
	0x23u, 0x63u, 0xf4u, 0xe2u, 0x44u, 0x4bu, 0xb5u, 0x81u,
	0xecu, 0xafu, 0xf8u, 0x5du, 0x3au, 0x2cu, 0x9au, 0x1bu,
	0x3eu, 0xb8u, 0x5fu, 0x69u, 0xf4u, 0x93u, 0xb4u, 0x6du,
	0x27u, 0xeau, 0x3eu, 0x0bu, 0xf1u, 0xeau, 0x04u, 0x47u,
	0x64u, 0x67u, 0x82u, 0xceu, 0xacu, 0xaeu, 0x54u, 0xccu,
	0xddu, 0x92u, 0x4eu, 0xafu, 0xd2u, 0xe7u, 0xd6u, 0xa5u,
	0x8cu, 0xf2u, 0xc3u, 0xa2u, 0x26u, 0xf4u, 0x08u, 0x4cu,
	0x81u, 0x45u, 0xc6u, 0x24u, 0x39u, 0xeau, 0x9bu, 0x3bu,
	0x71u, 0x6bu, 0x8cu, 0x06u, 0x17u, 0xa4u, 0x3fu, 0xdeu,
	0xb4u, 0x29u, 0x14u, 0x6du, 0x23u, 0x47u, 0x9eu, 0x4du,
	0xf4u, 0x84u, 0x18u, 0x83u, 0xf5u, 0x9cu, 0x1du, 0x04u,
	0xe0u, 0xb2u, 0x97u, 0xc1u, 0x1cu, 0xb6u, 0xa9u, 0xebu,
	0x6cu, 0x41u, 0x30u, 0xb5u, 0xb4u, 0x19u, 0x3eu, 0x0cu,
	0x0bu, 0x60u, 0xe9u, 0xe3u, 0xb0u, 0xa0u, 0xdau, 0x14u,
#else
	0xacu, 0x5au, 0xb4u, 0x90u, 0x78u, 0x67u, 0xfeu, 0x80u,
	0x17u, 0x07u, 0x2cu, 0x98u, 0x28u, 0x0du, 0xc3u, 0x11u,
	0xacu, 0xdfu, 0xa9u, 0x42u, 0x46u, 0xd3u, 0x19u, 0x70u,
	0xb7u, 0x92u, 0x5fu, 0xf7u, 0xa2u, 0x6bu, 0x00u, 0x36u,
	0x21u, 0x5eu, 0x25u, 0x2bu, 0x99u, 0x0cu, 0xb3u, 0x4cu,
	0x1eu, 0xb9u, 0x1du, 0x6bu, 0xc4u, 0x06u, 0xccu, 0xbfu,
	0x13u, 0xe8u, 0x44u, 0xfdu, 0xa6u, 0xa2u, 0x97u, 0xcbu,
	0xb0u, 0x29u, 0x08u, 0xafu, 0x00u, 0xddu, 0xaau, 0xb4u,
	0xabu, 0x79u, 0x46u, 0xb2u, 0xb4u, 0x40u, 0xfau, 0xa8u,
	0x65u, 0x76u, 0x9du, 0xf4u, 0x48u, 0x95u, 0xc7u, 0xefu,
	0x28u, 0xc6u, 0xf5u, 0xe8u, 0x04u, 0x69u, 0xdfu, 0x24u,
	0xdau, 0x78u, 0x10u, 0x93u, 0xe6u, 0x78u, 0x46u, 0xbbu,
	0x1bu, 0x5au, 0xfcu, 0xc3u, 0x47u, 0xc4u, 0x2fu, 0x2au,
	0xaau, 0x69u, 0x7eu, 0xc7u, 0x16u, 0x76u, 0xe0u, 0x43u,
	0xb9u, 0x07u, 0xa8u, 0x7bu, 0x8fu, 0x23u, 0x3cu, 0xd8u,
	0xd5u, 0xd2u, 0x27u, 0x78u, 0x6au, 0x06u, 0xbau, 0xa8u,
	0x5au, 0x8bu, 0xe4u, 0xd5u, 0x6eu, 0xccu, 0xa2u, 0x9eu,
	0x04u, 0xb3u, 0xe2u, 0x79u, 0xb3u, 0x7du, 0xa4u, 0x26u,
	0x76u, 0x02u, 0x10u, 0x5au, 0x4eu, 0xd0u, 0x7eu, 0x93u,
	0xebu, 0xb5u, 0xd0u, 0x02u, 0x50u, 0x6fu, 0x23u, 0xeau,
	0x99u, 0xdfu, 0x30u, 0xa3u, 0x7du, 0xe8u, 0xeeu, 0x9cu,
	0x26u, 0x18u, 0x3cu, 0x4bu, 0xecu, 0xe9u, 0x8fu, 0xd8u,
	0xaau, 0xbbu, 0xe2u, 0xccu, 0x79u, 0x53u, 0xb7u, 0x29u,
	0xd0u, 0x0au, 0xc2u, 0xc0u, 0x72u, 0x56u, 0x12u, 0xbdu,
	0xd1u, 0xfau, 0x02u, 0xc5u, 0x19u, 0x5fu, 0x69u, 0xa0u,
	0x2fu, 0x04u, 0x94u, 0x12u, 0xb7u, 0xfdu, 0x38u, 0xccu,
	0x2au, 0xe4u, 0xb6u, 0xc5u, 0x67u, 0x19u, 0xfeu, 0x92u,
	0xfeu, 0x5fu, 0x4fu, 0xd3u, 0x00u, 0x60u, 0x8cu, 0xa4u,
	0xf9u, 0xffu, 0x39u, 0xbeu, 0x7eu, 0x68u, 0xc5u, 0x27u,
	0x2du, 0xf4u, 0xe0u, 0x53u, 0xb9u, 0x13u, 0xa5u, 0xb7u,
	0xd6u, 0x77u, 0xf4u, 0xfau, 0x9cu, 0xf7u, 0x41u, 0xdfu,
	0xfdu, 0xfau, 0x1fu, 0x25u, 0x4au, 0x6du, 0x21u, 0x98u,
	0x65u, 0x7eu, 0x30u, 0x70u, 0x73u, 0x96u, 0xa0u, 0x17u,
	0xf9u, 0xbdu, 0x6au, 0x4du, 0x3au, 0x13u, 0x52u, 0x63u,
	0xefu, 0xd6u, 0xc4u, 0x8fu, 0x6au, 0xaau, 0x1au, 0xb3u,
	0x2eu, 0xa1u, 0x28u, 0x45u, 0xf8u, 0x87u, 0x2au, 0xe2u,
	0xa1u, 0x14u, 0x32u, 0xe5u, 0xf6u, 0xbeu, 0xb0u, 0x2eu,
	0xa8u, 0x21u, 0x59u, 0x3eu, 0x30u, 0x71u, 0x74u, 0x20u,
	0x58u, 0xdau, 0xa3u, 0xfcu, 0xceu, 0x2bu, 0xa3u, 0x7du,
	0x5bu, 0xceu, 0x4cu, 0xc9u, 0xdeu, 0x0fu, 0x27u, 0x46u,
	0x82u, 0x05u, 0xc6u, 0xd4u, 0x4bu, 0x71u, 0x8eu, 0x66u,
	0x39u, 0xbdu, 0xabu, 0x18u, 0x75u, 0x40u, 0x31u, 0x04u,
	0xe0u, 0x18u, 0x6bu, 0xbdu, 0xc2u, 0x38u, 0x8fu, 0xc5u,
	0xa5u, 0xf5u, 0x8au, 0x05u, 0xa8u, 0xf8u, 0x3cu, 0x7eu,
	0xf0u, 0x9bu, 0x9fu, 0x21u, 0x0du, 0x99u, 0xcdu, 0xa8u,
	0x24u, 0x8cu, 0xabu, 0x94u, 0xe0u, 0xd2u, 0xfbu, 0x5du,
	0x42u, 0xa8u, 0x69u, 0xb3u, 0xbbu, 0x12u, 0x9au, 0xf0u,
	0x2bu, 0xc2u, 0x5bu, 0x25u, 0x9du, 0x3au, 0xa0u, 0x42u,
	0x80u, 0x6fu, 0xe6u, 0x89u, 0x1eu, 0x87u, 0xe6u, 0xb0u,
	0xd6u, 0xf1u, 0xa0u, 0x0du, 0x14u, 0xfcu, 0x31u, 0x3eu,
	0x33u, 0x30u, 0x5au, 0x02u, 0xaau, 0x85u, 0xdau, 0x0au,
	0x59u, 0xd9u, 0x5au, 0x33u, 0xa0u, 0x55u, 0xf1u, 0x75u,
	0x09u, 0xa4u, 0xd7u, 0x58u, 0x41u, 0x0eu, 0x09u, 0x82u,
	0x5eu, 0x12u, 0xdeu, 0xaeu, 0x41u, 0xa6u, 0xe6u, 0x66u,
	0xcfu, 0x61u, 0x88u, 0x05u, 0x41u, 0xd7u, 0xa8u, 0x92u,
	0xe4u, 0xbau, 0xcau, 0xf0u, 0xaau, 0xd3u, 0x42u, 0x94u,
	0xebu, 0x52u, 0x53u, 0x1fu, 0xe6u, 0xf9u, 0x28u, 0x22u,
	0x74u, 0x7bu, 0x6bu, 0xafu, 0xa5u, 0x8au, 0x92u, 0xe0u,
	0x62u, 0x7bu, 0x35u, 0xceu, 0x1eu, 0x66u, 0x0eu, 0xa2u,
	0x50u, 0x3bu, 0xeau, 0x62u, 0xa8u, 0xe3u, 0x21u, 0x88u,
	0xcau, 0x4eu, 0x92u, 0x11u, 0x31u, 0x09u, 0xc9u, 0x7cu,
	0x58u, 0xfdu, 0xd9u, 0xb3u, 0x98u, 0x01u, 0x88u, 0xcdu,
	0x4fu, 0x51u, 0xccu, 0xecu, 0xf6u, 0x27u, 0xb7u, 0xc7u,
	0xd8u, 0x2du, 0x9du, 0xafu, 0x37u, 0x78u, 0xd6u, 0xa9u,
	0xb5u, 0xebu, 0x6cu, 0xbeu, 0x0eu, 0xe6u, 0x47u, 0x37u,
	0xebu, 0x4fu, 0x30u, 0x6bu, 0x4fu, 0x1au, 0x19u, 0x32u,
	0x9du, 0xd2u, 0xcau, 0x90u, 0xe1u, 0x10u, 0x38u, 0x8eu,
	0x13u, 0x93u, 0x90u, 0x3eu, 0x37u, 0xb0u, 0x48u, 0x29u,
	0x54u, 0x32u, 0xd8u, 0xd6u, 0x4bu, 0x0fu, 0x24u, 0xfeu,
	0xc3u, 0x45u, 0x51u, 0x86u, 0x29u, 0x82u, 0x64u, 0x71u,
	0x03u, 0xf1u, 0x6eu, 0x47u, 0xaau, 0x98u, 0x9fu, 0xd1u,
	0xa1u, 0x50u, 0x5fu, 0xe6u, 0x40u, 0xf5u, 0x92u, 0xebu,
	0x69u, 0xcbu, 0x48u, 0xe2u, 0xfau, 0x54u, 0x4fu, 0x34u,
	0x33u, 0x73u, 0xc3u, 0x62u, 0x7eu, 0x9fu, 0xb8u, 0x36u,
	0x1bu, 0x5bu, 0x49u, 0x4fu, 0xa4u, 0x0fu, 0xb7u, 0x77u,
	0x90u, 0x37u, 0xf5u, 0x75u, 0x94u, 0x67u, 0x02u, 0x44u,
	0x1eu, 0x73u, 0x6fu, 0x39u, 0xe4u, 0x3du, 0x1fu, 0x77u,
	0x6bu, 0x9fu, 0xf4u, 0x3au, 0x1cu, 0x82u, 0x68u, 0x11u,
	0xceu, 0x11u, 0xb8u, 0x49u, 0x08u, 0xc4u, 0x80u, 0xb0u,
	0xa8u, 0x93u, 0xc2u, 0x15u, 0xc8u, 0x43u, 0xc2u, 0xb9u,
	0x19u, 0xf3u, 0x1du, 0x8au, 0xf7u, 0x28u, 0xb3u, 0x08u,
	0x44u, 0x89u, 0xa0u, 0x2au, 0x6eu, 0x8cu, 0xf1u, 0x77u,
	0x01u, 0xb1u, 0x90u, 0xa3u, 0xd4u, 0x13u, 0x70u, 0xffu,
	0x2bu, 0xa1u, 0x24u, 0xa6u, 0x89u, 0x78u, 0xa7u, 0x68u,
#endif
};

static const uint8_t s_updEncIsKeyMaterial[0x2a0] = {
#if FW_DEBUG
	0xabu, 0x70u, 0x97u, 0x35u, 0x6fu, 0xddu, 0x49u, 0xd8u,
	0x38u, 0x78u, 0x54u, 0x01u, 0x67u, 0xf0u, 0xc4u, 0xadu,
	0x85u, 0x53u, 0x7cu, 0x5au, 0x56u, 0xbdu, 0x15u, 0xdfu,
	0x0eu, 0xb5u, 0xf7u, 0xf0u, 0xd9u, 0xe2u, 0x76u, 0xe6u,
	0xe5u, 0xf7u, 0x06u, 0xffu, 0x09u, 0x6du, 0xcdu, 0xa4u,
	0xc1u, 0x94u, 0xf3u, 0x92u, 0x42u, 0x4du, 0xc8u, 0x93u,
	0x48u, 0x65u, 0x5du, 0x7au, 0x19u, 0xf7u, 0x27u, 0xfeu,
	0x99u, 0x2au, 0x5eu, 0x6bu, 0xceu, 0x98u, 0x8au, 0x78u,
	0x8fu, 0xd4u, 0xa8u, 0xc5u, 0xd9u, 0xb9u, 0x9eu, 0xecu,
	0xccu, 0x16u, 0xb1u, 0x07u, 0xe2u, 0xafu, 0x47u, 0x81u,
	0xacu, 0x7fu, 0x82u, 0xd1u, 0x8au, 0x33u, 0x0eu, 0xa3u,
	0x8eu, 0x8bu, 0xa5u, 0x0cu, 0x53u, 0x1bu, 0x72u, 0x71u,
	0xa3u, 0x8au, 0x0eu, 0x13u, 0x73u, 0x13u, 0x6eu, 0x63u,
	0x4eu, 0x66u, 0x72u, 0xc9u, 0x99u, 0x36u, 0x3fu, 0x1au,
	0x82u, 0xf3u, 0x23u, 0xe3u, 0x76u, 0x6fu, 0x36u, 0x44u,
	0x33u, 0x66u, 0xbfu, 0x8au, 0x03u, 0xcfu, 0xc2u, 0x1au,
	0xadu, 0xbeu, 0x1du, 0x58u, 0x9fu, 0x55u, 0x54u, 0x5eu,
	0xefu, 0x7eu, 0x24u, 0x35u, 0xafu, 0x18u, 0x46u, 0xe4u,
	0xdfu, 0xd6u, 0x6du, 0x1au, 0xebu, 0x62u, 0xc6u, 0x7au,
	0x65u, 0x5cu, 0x0eu, 0x65u, 0x07u, 0x6au, 0x94u, 0xe2u,
	0x1bu, 0x15u, 0x87u, 0x88u, 0x17u, 0xaeu, 0x50u, 0x55u,
	0x5cu, 0x2bu, 0x51u, 0x38u, 0xcau, 0x6au, 0xd6u, 0x36u,
	0xf9u, 0x8cu, 0x8bu, 0xf0u, 0x44u, 0x2au, 0x94u, 0x18u,
	0x4cu, 0x54u, 0x2au, 0x30u, 0x1fu, 0xe8u, 0x81u, 0x1du,
	0xf3u, 0xa0u, 0xa2u, 0x96u, 0xb1u, 0xffu, 0xc0u, 0x53u,
	0xbcu, 0x77u, 0x9bu, 0x68u, 0x74u, 0x93u, 0xa8u, 0xd5u,
	0x9du, 0x1du, 0xbcu, 0xa7u, 0x37u, 0x88u, 0xcau, 0xd9u,
	0xa0u, 0x23u, 0x34u, 0xdau, 0x89u, 0x86u, 0x84u, 0x88u,
	0x27u, 0x8bu, 0x38u, 0xc2u, 0xa8u, 0x43u, 0xb8u, 0xbcu,
	0x7au, 0x63u, 0x42u, 0xa9u, 0x25u, 0x45u, 0x7du, 0x2cu,
	0x7cu, 0xcbu, 0x30u, 0xf2u, 0xa4u, 0x9cu, 0x22u, 0xf7u,
	0xdau, 0x65u, 0x71u, 0xe6u, 0xafu, 0xdfu, 0x0fu, 0xbfu,
	0xd3u, 0x6au, 0xeeu, 0xceu, 0x88u, 0x3fu, 0x53u, 0x09u,
	0x59u, 0x1du, 0xa3u, 0xc1u, 0xe1u, 0xc2u, 0xadu, 0xd7u,
	0x33u, 0xfau, 0x20u, 0x4fu, 0x12u, 0x35u, 0xbau, 0x7eu,
	0xd0u, 0xc3u, 0x46u, 0x96u, 0x80u, 0xecu, 0x69u, 0xd3u,
	0xbdu, 0xe2u, 0xdeu, 0xadu, 0xf2u, 0xc7u, 0xfdu, 0x74u,
	0xdcu, 0xdcu, 0xbau, 0x2au, 0xc4u, 0x54u, 0xf8u, 0x7cu,
	0x1du, 0xeeu, 0x20u, 0xd2u, 0x31u, 0x6cu, 0x06u, 0x30u,
	0x8cu, 0x22u, 0x3eu, 0x3eu, 0x3fu, 0x60u, 0x73u, 0x16u,
	0xf7u, 0x6au, 0x62u, 0xe4u, 0x0au, 0xe5u, 0xa8u, 0x7au,
	0x3du, 0x85u, 0x46u, 0x15u, 0x46u, 0x6bu, 0xd3u, 0x13u,
	0x99u, 0xd7u, 0x96u, 0x39u, 0x0cu, 0x06u, 0x70u, 0x0cu,
	0x07u, 0x69u, 0x3eu, 0x46u, 0x50u, 0x93u, 0x4eu, 0x6au,
	0xa9u, 0x85u, 0x29u, 0xafu, 0x9eu, 0xb0u, 0xbbu, 0xd1u,
	0x95u, 0xb9u, 0xd2u, 0xbbu, 0xa6u, 0x5fu, 0xb1u, 0x65u,
	0x23u, 0x44u, 0xb7u, 0x22u, 0x96u, 0x0eu, 0x9cu, 0xc3u,
	0xc5u, 0x11u, 0x18u, 0xc1u, 0xd3u, 0xc6u, 0xd0u, 0xb3u,
	0xecu, 0x30u, 0x64u, 0x7au, 0xa2u, 0xc5u, 0x30u, 0xceu,
	0x54u, 0x4au, 0xe1u, 0x37u, 0x38u, 0x54u, 0x24u, 0x2au,
	0x25u, 0x14u, 0x74u, 0x82u, 0x29u, 0x6cu, 0x54u, 0x6fu,
	0xa8u, 0xf2u, 0x6du, 0x36u, 0x2au, 0xfeu, 0xb4u, 0x2fu,
	0x01u, 0xbcu, 0x15u, 0xa5u, 0xc7u, 0x42u, 0xb4u, 0xd7u,
	0xf4u, 0x2cu, 0x9cu, 0x17u, 0x4fu, 0xd7u, 0x90u, 0x30u,
	0xdbu, 0x87u, 0x80u, 0x59u, 0x4eu, 0xcfu, 0xe4u, 0xf6u,
	0x5cu, 0x99u, 0xf4u, 0x9eu, 0x39u, 0xdbu, 0xc0u, 0xc8u,
	0x28u, 0xe3u, 0xe8u, 0xbeu, 0xb1u, 0xe9u, 0x33u, 0xecu,
	0xeeu, 0x57u, 0x4au, 0xe2u, 0xd9u, 0x38u, 0xafu, 0x68u,
	0x17u, 0x9du, 0xdbu, 0x94u, 0x7du, 0x1du, 0x45u, 0xecu,
	0xe4u, 0x1cu, 0x13u, 0x67u, 0x3au, 0xe8u, 0xf7u, 0x9cu,
	0xe4u, 0xaau, 0xdau, 0x53u, 0x64u, 0x5du, 0x56u, 0x89u,
	0x0fu, 0xb2u, 0x5bu, 0x9bu, 0x80u, 0x0bu, 0xbau, 0x09u,
	0x38u, 0xadu, 0x23u, 0xb7u, 0x38u, 0xb2u, 0x66u, 0xeeu,
	0x9eu, 0xa0u, 0x98u, 0x77u, 0xd1u, 0x9fu, 0x53u, 0xb5u,
	0xdbu, 0x6cu, 0x07u, 0xabu, 0x7cu, 0x4au, 0xdbu, 0xacu,
	0x46u, 0x3cu, 0x95u, 0x8fu, 0x77u, 0x47u, 0x00u, 0x18u,
	0x74u, 0x52u, 0x99u, 0xc1u, 0xfbu, 0x64u, 0xd8u, 0x28u,
	0xbau, 0xe1u, 0x0du, 0xe4u, 0x6du, 0x4eu, 0x05u, 0x47u,
	0xdfu, 0x6eu, 0xdfu, 0xd4u, 0x8cu, 0xfau, 0x3cu, 0xdfu,
	0x8bu, 0x6au, 0xc4u, 0x76u, 0xd1u, 0xb9u, 0x14u, 0xb7u,
	0x2bu, 0x10u, 0xf0u, 0xf5u, 0x84u, 0x1bu, 0x53u, 0xf2u,
	0x6cu, 0xa4u, 0xe0u, 0xa0u, 0x0bu, 0x30u, 0xeeu, 0xd6u,
	0x2au, 0x7du, 0x19u, 0xdbu, 0x12u, 0xbdu, 0x79u, 0xe9u,
	0xc4u, 0x6fu, 0x1eu, 0x94u, 0x6du, 0xd2u, 0x1bu, 0xb1u,
	0xa2u, 0xcdu, 0x4bu, 0x93u, 0x87u, 0x41u, 0x9eu, 0xf5u,
	0x4fu, 0x02u, 0xaau, 0xdeu, 0xd4u, 0xc1u, 0x6fu, 0xd9u,
	0x75u, 0x02u, 0xc6u, 0xebu, 0x95u, 0x35u, 0x29u, 0x84u,
	0x6eu, 0x12u, 0x9eu, 0xf7u, 0x8du, 0x81u, 0x04u, 0xdfu,
	0x58u, 0xb4u, 0x99u, 0x68u, 0x0fu, 0x33u, 0xf2u, 0xa3u,
	0x92u, 0xeau, 0xd3u, 0x19u, 0x12u, 0x39u, 0xaau, 0x10u,
	0x3fu, 0xb6u, 0xb5u, 0xe0u, 0x3bu, 0xf0u, 0x2eu, 0x45u,
	0xb9u, 0x7bu, 0x98u, 0x81u, 0x0au, 0x23u, 0xf1u, 0x72u,
	0x7du, 0xa7u, 0x8au, 0x2au, 0x9du, 0xf0u, 0xa0u, 0x15u,
	0x28u, 0x91u, 0xa9u, 0x68u, 0x67u, 0x7au, 0x91u, 0x5bu,
#else
	0xbfu, 0xd5u, 0xeau, 0x9fu, 0x91u, 0xaeu, 0x9au, 0xf2u,
	0x35u, 0x65u, 0xe5u, 0x34u, 0xc4u, 0x82u, 0x3bu, 0x72u,
	0xceu, 0x16u, 0x93u, 0x7eu, 0x97u, 0xa3u, 0x34u, 0x9fu,
	0x14u, 0x3cu, 0x8fu, 0xb6u, 0xaau, 0x21u, 0x95u, 0x28u,
	0x21u, 0x5eu, 0x25u, 0x2bu, 0x99u, 0x0cu, 0xb3u, 0x4cu,
	0x1eu, 0xb9u, 0x1du, 0x6bu, 0xc4u, 0x06u, 0xccu, 0xbfu,
	0x13u, 0xe8u, 0x44u, 0xfdu, 0xa6u, 0xa2u, 0x97u, 0xcbu,
	0xb0u, 0x29u, 0x08u, 0xafu, 0x00u, 0xddu, 0xaau, 0xb4u,
	0xabu, 0x79u, 0x46u, 0xb2u, 0xb4u, 0x40u, 0xfau, 0xa8u,
	0x65u, 0x76u, 0x9du, 0xf4u, 0x48u, 0x95u, 0xc7u, 0xefu,
	0x28u, 0xc6u, 0xf5u, 0xe8u, 0x04u, 0x69u, 0xdfu, 0x24u,
	0xdau, 0x78u, 0x10u, 0x93u, 0xe6u, 0x78u, 0x46u, 0xbbu,
	0x1bu, 0x5au, 0xfcu, 0xc3u, 0x47u, 0xc4u, 0x2fu, 0x2au,
	0xaau, 0x69u, 0x7eu, 0xc7u, 0x16u, 0x76u, 0xe0u, 0x43u,
	0xb9u, 0x07u, 0xa8u, 0x7bu, 0x8fu, 0x23u, 0x3cu, 0xd8u,
	0xd5u, 0xd2u, 0x27u, 0x78u, 0x6au, 0x06u, 0xbau, 0xa8u,
	0x5au, 0x8bu, 0xe4u, 0xd5u, 0x6eu, 0xccu, 0xa2u, 0x9eu,
	0x04u, 0xb3u, 0xe2u, 0x79u, 0xb3u, 0x7du, 0xa4u, 0x26u,
	0x76u, 0x02u, 0x10u, 0x5au, 0x4eu, 0xd0u, 0x7eu, 0x93u,
	0xebu, 0xb5u, 0xd0u, 0x02u, 0x50u, 0x6fu, 0x23u, 0xeau,
	0x99u, 0xdfu, 0x30u, 0xa3u, 0x7du, 0xe8u, 0xeeu, 0x9cu,
	0x26u, 0x18u, 0x3cu, 0x4bu, 0xecu, 0xe9u, 0x8fu, 0xd8u,
	0xaau, 0xbbu, 0xe2u, 0xccu, 0x79u, 0x53u, 0xb7u, 0x29u,
	0xd0u, 0x0au, 0xc2u, 0xc0u, 0x72u, 0x56u, 0x12u, 0xbdu,
	0xd1u, 0xfau, 0x02u, 0xc5u, 0x19u, 0x5fu, 0x69u, 0xa0u,
	0x2fu, 0x04u, 0x94u, 0x12u, 0xb7u, 0xfdu, 0x38u, 0xccu,
	0x2au, 0xe4u, 0xb6u, 0xc5u, 0x67u, 0x19u, 0xfeu, 0x92u,
	0xfeu, 0x5fu, 0x4fu, 0xd3u, 0x00u, 0x60u, 0x8cu, 0xa4u,
	0xf9u, 0xffu, 0x39u, 0xbeu, 0x7eu, 0x68u, 0xc5u, 0x27u,
	0x2du, 0xf4u, 0xe0u, 0x53u, 0xb9u, 0x13u, 0xa5u, 0xb7u,
	0xd6u, 0x77u, 0xf4u, 0xfau, 0x9cu, 0xf7u, 0x41u, 0xdfu,
	0xfdu, 0xfau, 0x1fu, 0x25u, 0x4au, 0x6du, 0x21u, 0x98u,
	0x65u, 0x7eu, 0x30u, 0x70u, 0x73u, 0x96u, 0xa0u, 0x17u,
	0xf9u, 0xbdu, 0x6au, 0x4du, 0x3au, 0x13u, 0x52u, 0x63u,
	0xefu, 0xd6u, 0xc4u, 0x8fu, 0x6au, 0xaau, 0x1au, 0xb3u,
	0x2eu, 0xa1u, 0x28u, 0x45u, 0xf8u, 0x87u, 0x2au, 0xe2u,
	0xa1u, 0x14u, 0x32u, 0xe5u, 0xf6u, 0xbeu, 0xb0u, 0x2eu,
	0xa8u, 0x21u, 0x59u, 0x3eu, 0x30u, 0x71u, 0x74u, 0x20u,
	0x58u, 0xdau, 0xa3u, 0xfcu, 0xceu, 0x2bu, 0xa3u, 0x7du,
	0x5bu, 0xceu, 0x4cu, 0xc9u, 0xdeu, 0x0fu, 0x27u, 0x46u,
	0x82u, 0x05u, 0xc6u, 0xd4u, 0x4bu, 0x71u, 0x8eu, 0x66u,
	0x39u, 0xbdu, 0xabu, 0x18u, 0x75u, 0x40u, 0x31u, 0x04u,
	0xe0u, 0x18u, 0x6bu, 0xbdu, 0xc2u, 0x38u, 0x8fu, 0xc5u,
	0xa5u, 0xf5u, 0x8au, 0x05u, 0xa8u, 0xf8u, 0x3cu, 0x7eu,
	0xf0u, 0x9bu, 0x9fu, 0x21u, 0x0du, 0x99u, 0xcdu, 0xa8u,
	0x24u, 0x8cu, 0xabu, 0x94u, 0xe0u, 0xd2u, 0xfbu, 0x5du,
	0x42u, 0xa8u, 0x69u, 0xb3u, 0xbbu, 0x12u, 0x9au, 0xf0u,
	0x2bu, 0xc2u, 0x5bu, 0x25u, 0x9du, 0x3au, 0xa0u, 0x42u,
	0x80u, 0x6fu, 0xe6u, 0x89u, 0x1eu, 0x87u, 0xe6u, 0xb0u,
	0xd6u, 0xf1u, 0xa0u, 0x0du, 0x14u, 0xfcu, 0x31u, 0x3eu,
	0x33u, 0x30u, 0x5au, 0x02u, 0xaau, 0x85u, 0xdau, 0x0au,
	0x59u, 0xd9u, 0x5au, 0x33u, 0xa0u, 0x55u, 0xf1u, 0x75u,
	0x09u, 0xa4u, 0xd7u, 0x58u, 0x41u, 0x0eu, 0x09u, 0x82u,
	0x5eu, 0x12u, 0xdeu, 0xaeu, 0x41u, 0xa6u, 0xe6u, 0x66u,
	0xcfu, 0x61u, 0x88u, 0x05u, 0x41u, 0xd7u, 0xa8u, 0x92u,
	0xe4u, 0xbau, 0xcau, 0xf0u, 0xaau, 0xd3u, 0x42u, 0x94u,
	0xebu, 0x52u, 0x53u, 0x1fu, 0xe6u, 0xf9u, 0x28u, 0x22u,
	0x74u, 0x7bu, 0x6bu, 0xafu, 0xa5u, 0x8au, 0x92u, 0xe0u,
	0x62u, 0x7bu, 0x35u, 0xceu, 0x1eu, 0x66u, 0x0eu, 0xa2u,
	0x50u, 0x3bu, 0xeau, 0x62u, 0xa8u, 0xe3u, 0x21u, 0x88u,
	0xcau, 0x4eu, 0x92u, 0x11u, 0x31u, 0x09u, 0xc9u, 0x7cu,
	0x58u, 0xfdu, 0xd9u, 0xb3u, 0x98u, 0x01u, 0x88u, 0xcdu,
	0x4fu, 0x51u, 0xccu, 0xecu, 0xf6u, 0x27u, 0xb7u, 0xc7u,
	0xd8u, 0x2du, 0x9du, 0xafu, 0x37u, 0x78u, 0xd6u, 0xa9u,
	0xb5u, 0xebu, 0x6cu, 0xbeu, 0x0eu, 0xe6u, 0x47u, 0x37u,
	0xebu, 0x4fu, 0x30u, 0x6bu, 0x4fu, 0x1au, 0x19u, 0x32u,
	0x9du, 0xd2u, 0xcau, 0x90u, 0xe1u, 0x10u, 0x38u, 0x8eu,
	0x13u, 0x93u, 0x90u, 0x3eu, 0x37u, 0xb0u, 0x48u, 0x29u,
	0x54u, 0x32u, 0xd8u, 0xd6u, 0x4bu, 0x0fu, 0x24u, 0xfeu,
	0xc3u, 0x45u, 0x51u, 0x86u, 0x29u, 0x82u, 0x64u, 0x71u,
	0x03u, 0xf1u, 0x6eu, 0x47u, 0xaau, 0x98u, 0x9fu, 0xd1u,
	0xa1u, 0x50u, 0x5fu, 0xe6u, 0x40u, 0xf5u, 0x92u, 0xebu,
	0x69u, 0xcbu, 0x48u, 0xe2u, 0xfau, 0x54u, 0x4fu, 0x34u,
	0x33u, 0x73u, 0xc3u, 0x62u, 0x7eu, 0x9fu, 0xb8u, 0x36u,
	0x1bu, 0x5bu, 0x49u, 0x4fu, 0xa4u, 0x0fu, 0xb7u, 0x77u,
	0x90u, 0x37u, 0xf5u, 0x75u, 0x94u, 0x67u, 0x02u, 0x44u,
	0x1eu, 0x73u, 0x6fu, 0x39u, 0xe4u, 0x3du, 0x1fu, 0x77u,
	0x6bu, 0x9fu, 0xf4u, 0x3au, 0x1cu, 0x82u, 0x68u, 0x11u,
	0xceu, 0x11u, 0xb8u, 0x49u, 0x08u, 0xc4u, 0x80u, 0xb0u,
	0xa8u, 0x93u, 0xc2u, 0x15u, 0xc8u, 0x43u, 0xc2u, 0xb9u,
	0x19u, 0xf3u, 0x1du, 0x8au, 0xf7u, 0x28u, 0xb3u, 0x08u,
	0x44u, 0x89u, 0xa0u, 0x2au, 0x6eu, 0x8cu, 0xf1u, 0x77u,
	0x01u, 0xb1u, 0x90u, 0xa3u, 0xd4u, 0x13u, 0x70u, 0xffu,
	0x2bu, 0xa1u, 0x24u, 0xa6u, 0x89u, 0x78u, 0xa7u, 0x68u,
#endif
};

static const uint8_t s_updListHmacKeys[10][64] = {
#if FW_DEBUG
	{
		0x2bu, 0x53u, 0xcau, 0x32u, 0x0eu, 0xc2u, 0x65u, 0xd7u,
		0x53u, 0x71u, 0xfeu, 0x43u, 0x13u, 0xa6u, 0x20u, 0xbdu,
		0xdfu, 0x2au, 0x0au, 0xa4u, 0xfeu, 0x35u, 0xc9u, 0xc8u,
		0x50u, 0x73u, 0x9bu, 0x2cu, 0xe1u, 0x0eu, 0x6du, 0x21u,
		0xb4u, 0x0eu, 0xddu, 0xe2u, 0x83u, 0xe7u, 0x8eu, 0xa9u,
		0x4au, 0xccu, 0x7eu, 0xf1u, 0x9cu, 0x3cu, 0xd9u, 0x20u,
		0xaeu, 0xe1u, 0xc3u, 0xbfu, 0x29u, 0x89u, 0xb5u, 0x41u,
		0x20u, 0x0bu, 0x4fu, 0x22u, 0xc3u, 0x04u, 0x3bu, 0xe1u,
	},
	{
		0x2au, 0x27u, 0x25u, 0xf8u, 0xf9u, 0xd0u, 0x37u, 0xfbu,
		0x4cu, 0xedu, 0xfdu, 0x08u, 0x98u, 0xddu, 0x0cu, 0x84u,
		0xe2u, 0x16u, 0xbeu, 0x22u, 0xb8u, 0xbau, 0xd8u, 0xe0u,
		0xe2u, 0xacu, 0xf3u, 0xe8u, 0x26u, 0x06u, 0x30u, 0xd8u,
		0x26u, 0xe9u, 0x83u, 0x3cu, 0x84u, 0x3du, 0xe2u, 0x11u,
		0x1du, 0xccu, 0xd4u, 0xdbu, 0xd5u, 0x4au, 0x99u, 0x30u,
		0xd6u, 0x6cu, 0xbfu, 0xf3u, 0x03u, 0x1bu, 0x6eu, 0xbbu,
		0x00u, 0x89u, 0xb1u, 0x37u, 0x8eu, 0x6cu, 0x7au, 0xd5u,
	},
	{
		0x95u, 0x1au, 0x4bu, 0xb7u, 0xedu, 0x8fu, 0x8au, 0xe8u,
		0x11u, 0xf5u, 0x62u, 0xbau, 0x62u, 0xe2u, 0x1cu, 0x53u,
		0x6fu, 0x7cu, 0x0cu, 0x4au, 0x63u, 0xb1u, 0x48u, 0x6au,
		0x44u, 0x60u, 0xa2u, 0x3cu, 0x7cu, 0x00u, 0xebu, 0x6du,
		0xb9u, 0x72u, 0xb7u, 0xafu, 0xe6u, 0x08u, 0x35u, 0x3fu,
		0x83u, 0xdcu, 0xabu, 0x3fu, 0x56u, 0x63u, 0x75u, 0xf8u,
		0x20u, 0xc7u, 0xacu, 0x0du, 0xdcu, 0x6cu, 0x0fu, 0xbcu,
		0x3eu, 0x08u, 0xaau, 0x06u, 0x86u, 0x0eu, 0xbdu, 0xeau,
	},
	{
		0x7du, 0x64u, 0x59u, 0x83u, 0x37u, 0x4du, 0xa9u, 0x03u,
		0x8du, 0x31u, 0xcau, 0xf9u, 0xbdu, 0xb9u, 0x82u, 0xa9u,
		0x1fu, 0xafu, 0x21u, 0x17u, 0xf6u, 0x75u, 0x84u, 0xddu,
		0xd4u, 0x7fu, 0x55u, 0x4cu, 0x3eu, 0xedu, 0x35u, 0xc4u,
		0x9du, 0x41u, 0x76u, 0x84u, 0xc7u, 0x52u, 0xfdu, 0x38u,
		0x3cu, 0x86u, 0x9cu, 0x5au, 0xa1u, 0xdbu, 0xaau, 0x84u,
		0xaau, 0x71u, 0x1bu, 0x4eu, 0x90u, 0x3au, 0xa2u, 0x7bu,
		0x22u, 0x10u, 0xdeu, 0xecu, 0xddu, 0x9bu, 0x88u, 0x14u,
	},
	{
		0xb9u, 0xf3u, 0x98u, 0x92u, 0x58u, 0x37u, 0xc9u, 0x7bu,
		0xb9u, 0x93u, 0xe2u, 0x0au, 0xb9u, 0x38u, 0x44u, 0x4au,
		0x5bu, 0x12u, 0x03u, 0x38u, 0xc9u, 0x6eu, 0x5au, 0x0eu,
		0xc0u, 0x52u, 0xc7u, 0x34u, 0x7eu, 0x56u, 0xe1u, 0xc8u,
		0x84u, 0x5eu, 0x1cu, 0x07u, 0x76u, 0xf8u, 0x61u, 0xdcu,
		0x60u, 0x9cu, 0xceu, 0x1au, 0xbau, 0x45u, 0xcdu, 0xc9u,
		0x81u, 0x0au, 0x2au, 0x9cu, 0x55u, 0x04u, 0x02u, 0x4eu,
		0x89u, 0x70u, 0x4fu, 0x7au, 0x3eu, 0x45u, 0x09u, 0x85u,
	},
	{
		0xadu, 0x3au, 0x10u, 0x1cu, 0x67u, 0xc5u, 0x10u, 0x11u,
		0x84u, 0x13u, 0xc0u, 0x3cu, 0xf7u, 0x33u, 0xc2u, 0xa7u,
		0xfdu, 0x48u, 0x58u, 0xfeu, 0x3eu, 0xaeu, 0x4cu, 0x4eu,
		0x59u, 0xa9u, 0x08u, 0x32u, 0x9au, 0x19u, 0xacu, 0x33u,
		0x45u, 0xc3u, 0x39u, 0x6au, 0x75u, 0x03u, 0x7fu, 0x90u,
		0x81u, 0x34u, 0xbeu, 0x8au, 0x15u, 0x60u, 0xeeu, 0x4du,
		0xe8u, 0x0eu, 0xd7u, 0x1bu, 0xdeu, 0x06u, 0x71u, 0xb6u,
		0xdfu, 0x9fu, 0x47u, 0x44u, 0x07u, 0xc2u, 0xa3u, 0x49u,
	},
	{
		0x71u, 0x2eu, 0x6cu, 0x16u, 0x19u, 0x1du, 0xbcu, 0xb0u,
		0x16u, 0x6au, 0xf4u, 0x60u, 0x04u, 0xb7u, 0xdau, 0x1eu,
		0xa7u, 0x6fu, 0x5au, 0x69u, 0x42u, 0xaau, 0xc2u, 0x5cu,
		0x6au, 0xe6u, 0x43u, 0x0au, 0x3bu, 0x26u, 0x24u, 0x18u,
		0x41u, 0xd3u, 0x66u, 0x43u, 0xa6u, 0xf5u, 0x60u, 0x25u,
		0xd0u, 0xd9u, 0x4eu, 0x05u, 0x84u, 0xe7u, 0xdeu, 0x32u,
		0xf8u, 0x16u, 0x20u, 0x80u, 0x7du, 0x05u, 0x84u, 0xd5u,
		0x46u, 0x96u, 0x60u, 0x91u, 0xf6u, 0x8cu, 0x02u, 0x1du,
	},
	{
		0x8bu, 0x85u, 0xbfu, 0xabu, 0x9cu, 0xb7u, 0xbeu, 0x2au,
		0xd3u, 0xcfu, 0x06u, 0x71u, 0x51u, 0xdbu, 0x4eu, 0xf5u,
		0xeeu, 0x0fu, 0x65u, 0xabu, 0xc7u, 0x00u, 0xffu, 0xc3u,
		0x98u, 0xcfu, 0xedu, 0x39u, 0xb9u, 0x2du, 0x3fu, 0xe6u,
		0xbau, 0x44u, 0x7fu, 0x7bu, 0xfeu, 0x85u, 0x2du, 0xa7u,
		0x53u, 0x91u, 0x54u, 0xa4u, 0x3bu, 0x30u, 0xf0u, 0x48u,
		0x49u, 0x10u, 0x2bu, 0x4fu, 0xc4u, 0xeau, 0xdeu, 0xf0u,
		0xdfu, 0xeau, 0x45u, 0xa3u, 0x1cu, 0x6au, 0x67u, 0xf9u,
	},
	{
		0x73u, 0x84u, 0xa2u, 0x99u, 0x4cu, 0xa8u, 0x80u, 0xacu,
		0xbfu, 0xb2u, 0xf1u, 0x8du, 0x03u, 0x2eu, 0xb5u, 0xe2u,
		0x25u, 0x79u, 0x2eu, 0x81u, 0xa1u, 0xecu, 0x3eu, 0x62u,
		0x95u, 0xebu, 0xc0u, 0x9eu, 0xd6u, 0x69u, 0xb7u, 0x80u,
		0x24u, 0x5cu, 0x96u, 0x2bu, 0xc7u, 0x6au, 0x6cu, 0xf9u,
		0x48u, 0x2fu, 0x58u, 0xd6u, 0x43u, 0x0cu, 0xc0u, 0xfeu,
		0xeau, 0xd1u, 0x53u, 0x69u, 0x3du, 0xa1u, 0x50u, 0xd9u,
		0x38u, 0x1cu, 0xf4u, 0xedu, 0xd8u, 0x56u, 0x44u, 0x1du,
	},
	{
		0x6eu, 0xe3u, 0xddu, 0x61u, 0x88u, 0xc6u, 0x9du, 0x57u,
		0xa1u, 0x8bu, 0x86u, 0xf4u, 0x7au, 0xe5u, 0x8du, 0x83u,
		0x99u, 0xa7u, 0x0eu, 0x44u, 0xc0u, 0xc7u, 0x67u, 0x12u,
		0xcfu, 0x98u, 0x3fu, 0x98u, 0xa7u, 0x05u, 0x8eu, 0xbdu,
		0xcbu, 0x6fu, 0xbau, 0x6du, 0x04u, 0x99u, 0x90u, 0x05u,
		0xcau, 0x34u, 0x07u, 0xc3u, 0x5bu, 0x0eu, 0xc8u, 0x9cu,
		0x42u, 0x0fu, 0xb2u, 0x77u, 0xd3u, 0x4bu, 0x5cu, 0xb9u,
		0x55u, 0x31u, 0x4au, 0x73u, 0x2eu, 0x98u, 0x0du, 0xf3u,
	},
#else
	{
		0x73u, 0x3du, 0x85u, 0x8au, 0x2au, 0x39u, 0xbau, 0x30u,
		0x58u, 0xffu, 0x16u, 0x08u, 0x33u, 0xd1u, 0xb9u, 0x18u,
		0x04u, 0xd4u, 0xb8u, 0x69u, 0xb0u, 0x49u, 0xc7u, 0xd9u,
		0xf5u, 0xcdu, 0xcdu, 0x8cu, 0x71u, 0x19u, 0x58u, 0x22u,
		0xebu, 0x1eu, 0x48u, 0xceu, 0xfbu, 0x38u, 0x34u, 0x24u,
		0x33u, 0x20u, 0x6du, 0x08u, 0x2cu, 0xd5u, 0xdbu, 0x03u,
		0x95u, 0xd7u, 0x22u, 0xcbu, 0xc1u, 0x82u, 0xebu, 0xa7u,
		0x75u, 0xabu, 0xfdu, 0xd6u, 0x34u, 0xdfu, 0xe7u, 0xdau,
	},
	{
		0x67u, 0x6du, 0x13u, 0x18u, 0x29u, 0xe3u, 0x4fu, 0xbdu,
		0xcfu, 0x47u, 0xa8u, 0xafu, 0xa4u, 0x16u, 0xfcu, 0xb6u,
		0xb1u, 0x07u, 0x6bu, 0xe1u, 0x5du, 0xe8u, 0xaau, 0x94u,
		0xfeu, 0x10u, 0x2au, 0x6bu, 0x08u, 0xd9u, 0xa4u, 0xddu,
		0xc2u, 0x73u, 0x53u, 0x07u, 0x13u, 0x9bu, 0x09u, 0x49u,
		0xd7u, 0x88u, 0xaeu, 0x03u, 0x65u, 0xedu, 0xb2u, 0x41u,
		0x80u, 0x20u, 0x94u, 0x0du, 0x51u, 0x9fu, 0xceu, 0x9du,
		0x04u, 0xebu, 0x86u, 0x8fu, 0x13u, 0xa5u, 0x66u, 0x2fu,
	},
	{
		0x67u, 0x4cu, 0xd9u, 0x97u, 0xe5u, 0x3fu, 0x1bu, 0x50u,
		0xecu, 0x33u, 0x79u, 0x32u, 0x31u, 0xe1u, 0xabu, 0x26u,
		0x30u, 0xdfu, 0x29u, 0xfeu, 0x3fu, 0x2du, 0xb1u, 0xf8u,
		0x8eu, 0x27u, 0x7au, 0x0cu, 0x3fu, 0x52u, 0x54u, 0x23u,
		0x52u, 0xa5u, 0x73u, 0x18u, 0x8bu, 0xa8u, 0x24u, 0x9bu,
		0x01u, 0xfbu, 0xd1u, 0x4du, 0x7fu, 0x67u, 0xb4u, 0x7au,
		0x4bu, 0x8cu, 0x46u, 0x22u, 0xa0u, 0x9au, 0xa2u, 0xa6u,
		0xa4u, 0x97u, 0xaeu, 0xc4u, 0xb8u, 0x2cu, 0x5du, 0xe0u,
	},
	{
		0xc7u, 0xbfu, 0xb2u, 0x87u, 0x30u, 0xd0u, 0x31u, 0xa9u,
		0x63u, 0xa4u, 0xa6u, 0xa5u, 0x65u, 0x0eu, 0xa8u, 0xb5u,
		0x86u, 0x23u, 0x73u, 0x39u, 0x72u, 0x52u, 0xc7u, 0x24u,
		0x3eu, 0xf6u, 0xeau, 0xe6u, 0x3du, 0xd2u, 0x7du, 0x5fu,
		0x6eu, 0xa1u, 0x75u, 0xc1u, 0x7fu, 0xebu, 0x82u, 0x11u,
		0xe7u, 0x28u, 0x3bu, 0xb4u, 0xebu, 0xe1u, 0xa2u, 0x54u,
		0x5eu, 0x4eu, 0x7bu, 0xaeu, 0xbfu, 0xb5u, 0x0au, 0x11u,
		0xb1u, 0x06u, 0x97u, 0x14u, 0xacu, 0x8fu, 0x38u, 0xbfu,
	},
	{
		0xbau, 0xa3u, 0xdcu, 0x7du, 0x11u, 0xcau, 0x82u, 0xecu,
		0x2eu, 0x0cu, 0xd2u, 0xd6u, 0x49u, 0x7cu, 0x7cu, 0x37u,
		0xa8u, 0xabu, 0x88u, 0xe9u, 0xafu, 0x47u, 0xe5u, 0x70u,
		0x9au, 0x80u, 0x9bu, 0xb5u, 0x7eu, 0x99u, 0xb3u, 0x8fu,
		0xfcu, 0xbeu, 0x74u, 0x2fu, 0xbcu, 0xc4u, 0x47u, 0xe2u,
		0xd8u, 0x3bu, 0xafu, 0x68u, 0x3fu, 0x30u, 0xc9u, 0xb7u,
		0xd5u, 0xd8u, 0xf2u, 0x56u, 0x70u, 0xceu, 0xb2u, 0x8du,
		0xfdu, 0xc5u, 0x94u, 0xc8u, 0xc0u, 0xa7u, 0x17u, 0x9fu,
	},
	{
		0xebu, 0x7du, 0x09u, 0x3eu, 0x4au, 0xfau, 0xc5u, 0x47u,
		0x3eu, 0x05u, 0x0du, 0xd8u, 0x8eu, 0xbeu, 0xfdu, 0x4cu,
		0x50u, 0x13u, 0xe5u, 0x48u, 0x85u, 0x6fu, 0x50u, 0x5bu,
		0x4bu, 0xcbu, 0x05u, 0xe7u, 0x44u, 0xc3u, 0x08u, 0x6fu,
		0xe7u, 0x76u, 0x94u, 0x66u, 0xaau, 0x38u, 0x40u, 0x48u,
		0xa0u, 0x37u, 0xa6u, 0x46u, 0x6au, 0x44u, 0xcdu, 0x15u,
		0xdeu, 0x41u, 0xb9u, 0x78u, 0x0bu, 0x07u, 0x09u, 0xa1u,
		0xf4u, 0xb5u, 0x75u, 0x76u, 0x02u, 0xceu, 0xf6u, 0x93u,
	},
	{
		0xe0u, 0x54u, 0xa0u, 0x1fu, 0xe2u, 0xe2u, 0xc6u, 0xcbu,
		0x1du, 0x03u, 0x49u, 0x53u, 0x69u, 0xbbu, 0x62u, 0x7fu,
		0x21u, 0x47u, 0x61u, 0x23u, 0x0au, 0xefu, 0x5cu, 0x87u,
		0x16u, 0x3du, 0x41u, 0x63u, 0x79u, 0xd7u, 0x73u, 0x65u,
		0x80u, 0xecu, 0x66u, 0xb8u, 0xffu, 0x84u, 0x5du, 0x4eu,
		0x34u, 0x20u, 0xc0u, 0x48u, 0x4eu, 0xf7u, 0xf0u, 0x04u,
		0x13u, 0x63u, 0x44u, 0xabu, 0xdbu, 0xa6u, 0xb4u, 0x5cu,
		0xa1u, 0x2bu, 0xa4u, 0x19u, 0x2fu, 0xaau, 0x80u, 0x2eu,
	},
	{
		0x37u, 0x75u, 0xaau, 0x32u, 0xc1u, 0x9eu, 0xc2u, 0x6fu,
		0xb2u, 0x71u, 0x4au, 0xfbu, 0x2du, 0xbau, 0x75u, 0x17u,
		0x7eu, 0x7bu, 0x01u, 0xb4u, 0x68u, 0xe6u, 0xd6u, 0xfau,
		0x8cu, 0xaau, 0x89u, 0x36u, 0x8du, 0x01u, 0x3fu, 0x6cu,
		0x15u, 0xb8u, 0x1fu, 0x3cu, 0x45u, 0x28u, 0xedu, 0xa7u,
		0xa2u, 0x64u, 0x84u, 0xe1u, 0xc3u, 0x20u, 0xfcu, 0xedu,
		0xa8u, 0xfbu, 0x7au, 0x8fu, 0x70u, 0xccu, 0x61u, 0xd4u,
		0x31u, 0x58u, 0x29u, 0xedu, 0x88u, 0x5fu, 0xe1u, 0x18u,
	},
	{
		0xf6u, 0xd9u, 0x03u, 0x05u, 0xd6u, 0x6cu, 0xd6u, 0x06u,
		0x24u, 0x5du, 0x62u, 0x62u, 0x0au, 0xcdu, 0x4eu, 0xbeu,
		0xc4u, 0xb6u, 0x55u, 0x54u, 0xe8u, 0x83u, 0xe5u, 0x27u,
		0xefu, 0x82u, 0x0fu, 0x9du, 0x12u, 0x97u, 0x88u, 0x82u,
		0xfeu, 0x39u, 0x0cu, 0xcfu, 0x24u, 0xccu, 0xd9u, 0x5du,
		0x5bu, 0x53u, 0x26u, 0x0bu, 0xb9u, 0x62u, 0x6fu, 0x33u,
		0x39u, 0x44u, 0x97u, 0x9cu, 0x07u, 0x85u, 0x12u, 0x12u,
		0x07u, 0x70u, 0x47u, 0x06u, 0xf0u, 0x89u, 0x8eu, 0x16u,
	},
	{
		0x33u, 0x33u, 0xcfu, 0x90u, 0xe0u, 0x27u, 0xa8u, 0xf2u,
		0x1du, 0x87u, 0x8fu, 0x71u, 0xa9u, 0x4du, 0x24u, 0xfdu,
		0x24u, 0x5bu, 0xd7u, 0xf7u, 0xceu, 0x8au, 0x9au, 0xbbu,
		0x55u, 0xacu, 0x7eu, 0x03u, 0x5au, 0xdfu, 0x9fu, 0x7bu,
		0xd4u, 0xfcu, 0x39u, 0x08u, 0x54u, 0xa7u, 0xbfu, 0x30u,
		0x27u, 0x58u, 0xb1u, 0x06u, 0x68u, 0xa4u, 0x01u, 0xbdu,
		0x01u, 0x7au, 0x97u, 0xafu, 0xddu, 0x4cu, 0x74u, 0x89u,
		0x5bu, 0x59u, 0xbau, 0x07u, 0xc2u, 0x5eu, 0x87u, 0xa5u,
	},
#endif
};

static const uint8_t s_spkgMetaKey[48] = {
#if FW_DEBUG
	0x23u, 0xf1u, 0xd5u, 0x25u, 0x24u, 0x42u, 0x66u, 0xe6u,
	0xdau, 0x7au, 0x52u, 0xdau, 0x94u, 0x46u, 0x31u, 0x83u,
	0x01u, 0xeeu, 0x8cu, 0xc5u, 0x8du, 0x54u, 0x90u, 0x1au,
	0xe9u, 0x4du, 0x93u, 0x01u, 0x0fu, 0x7du, 0xeeu, 0x6bu,
	0x37u, 0x21u, 0xf7u, 0xc0u, 0x5du, 0xe5u, 0xf5u, 0x5eu,
	0xccu, 0x39u, 0xbdu, 0xdbu, 0x4au, 0x6cu, 0x58u, 0x5du,
#else
	0x2eu, 0x6fu, 0x47u, 0x51u, 0xd1u, 0x5bu, 0x06u, 0xc5u,
	0x1fu, 0x57u, 0x2au, 0x93u, 0x06u, 0xe5u, 0x2du, 0xd7u,
	0x00u, 0x7eu, 0xa5u, 0x6au, 0x31u, 0xd4u, 0x59u, 0xecu,
	0x6du, 0x36u, 0x81u, 0xabu, 0x08u, 0x62u, 0x55u, 0x01u,
	0xb3u, 0xd5u, 0x41u, 0xa5u, 0x68u, 0x75u, 0x1du, 0xf8u,
	0xf4u, 0x83u, 0x3bu, 0xabu, 0x4eu, 0xfeu, 0x05u, 0x37u,
#endif
};

static const uint32_t s_spkgRsaModulus[64] = {
#if FW_DEBUG
	0x914e82bdu, 0xae612f1fu, 0xb9a196c7u, 0xa581d130u,
	0x799cb427u, 0x5994468fu, 0xd33e6082u, 0xaa0afe2fu,
	0xe862bf3du, 0x9d585858u, 0x1729a981u, 0x39f1ccf8u,
	0xd837650eu, 0xdc56fc3du, 0xeb817dc1u, 0xe8fcf7e5u,
	0x972300afu, 0x86f1ebf9u, 0x61a4b47eu, 0xabe8f5adu,
	0xcb2721ffu, 0xaa7e46a6u, 0xad195eccu, 0xba9012bdu,
	0xd1144e82u, 0x8d433a44u, 0x0aad19fdu, 0x3c8f9c18u,
	0x1d7341b6u, 0x0f5d17a5u, 0xc004184du, 0x34f34dd7u,
	0x3bd5c9c1u, 0x92e8dfb8u, 0x5709f758u, 0x2bb70de3u,
	0x684a4039u, 0xbc47c5b8u, 0xe1937aecu, 0x1cbd408eu,
	0x443f3bcdu, 0x9ce3f888u, 0xcc252e22u, 0x0b0dd437u,
	0x863ee69au, 0x6fb2edf9u, 0x5da0241eu, 0x3a68aeb3u,
	0xd65ffd56u, 0xed30b52fu, 0x4e1b895fu, 0x205b6a54u,
	0xe4e9b44cu, 0x849ec878u, 0x0b11339cu, 0xfda1888cu,
	0xd5835e12u, 0x245214cau, 0xf645f18au, 0x22b15081u,
	0x578f30feu, 0x2002459au, 0x47475c87u, 0xe15249a9u,
#else
	0xb51f60bbu, 0xf08de4d4u, 0xb2357937u, 0x176f9575u,
	0x88a81256u, 0xb32f1f24u, 0x5224eec5u, 0x8de1d04cu,
	0x2be732edu, 0x425521f2u, 0xf7425975u, 0x64584909u,
	0xa1ce23acu, 0x68f8848du, 0xd8d1cba2u, 0x3386ac08u,
	0x8312899bu, 0x7316a0cau, 0x582659b5u, 0x8431195au,
	0xb1e794e3u, 0xef6a72c5u, 0x63dd11adu, 0x68bc36c0u,
	0x1352c53du, 0xdc5dc418u, 0xf356f2d3u, 0xc3386121u,
	0xe53c8407u, 0xfed43d63u, 0x2045b63eu, 0x2bf096b6u,
	0x6aaec3ebu, 0x7975af44u, 0xc9511e8du, 0x4ee4e0b0u,
	0xbfd06296u, 0xc9a1c710u, 0xaa46432cu, 0x960e86efu,
	0x2ea47290u, 0x48853b36u, 0xbd99a321u, 0x15da0f34u,
	0x8fea8ec6u, 0x782e925eu, 0xf82029e9u, 0x99e95f9fu,
	0x76a14294u, 0xa003a95bu, 0x3e937accu, 0xdc8fd339u,
	0xd732986cu, 0x6e31047bu, 0xa1c1609bu, 0xa3f7388fu,
	0x70cb6571u, 0x32c038a4u, 0x5d57e225u, 0x3ffb959fu,
	0xd958b6cbu, 0xc2d6dec0u, 0xa1030e3au, 0x99fed450u,
#endif
};

static const uint8_t * const g_updEncIdAesKey = s_updEncIdKeyMaterial;
static const uint8_t * const g_updEncIdAesIv = s_updEncIdKeyMaterial + 0x10u;
static const uint8_t * const g_updEncIdPrivData __attribute__((used)) =
	s_updEncIdKeyMaterial + 0x20u;
static const UpdEncRsaPubKey * const g_updEncIdRsaPubKeyPtr __attribute__((used)) =
	&s_updEncIdRsaPubKey;

static const uint8_t * const g_updEncIsAesKey = s_updEncIsKeyMaterial;
static const uint8_t * const g_updEncIsAesIv = s_updEncIsKeyMaterial + 0x10u;
static const uint8_t * const g_updEncIsPrivData __attribute__((used)) =
	s_updEncIsKeyMaterial + 0x20u;
static const UpdEncRsaPubKey * const g_updEncIsRsaPubKeyPtr __attribute__((used)) =
	&s_updEncIsRsaPubKey;

/* 0x812c70: Default 64-byte IV buffer for Command 5 Bigmac LLI transfer */
static const uint8_t s_updDmaDefaultIv[64] = {
	0xfdu, 0x4fu, 0xa8u, 0xfau, 0x4fu, 0xe7u, 0x94u, 0x30u,
	0xa0u, 0xcau, 0x30u, 0x5cu, 0x88u, 0xe5u, 0x24u, 0xddu,
};

/* 0x813a78: Mutable 64-byte IV buffer for Command 5 Bigmac LLI transfer */
static uint8_t s_updDmaWorkIv[64] = {
	0xfdu, 0x4fu, 0xa8u, 0xfau, 0x4fu, 0xe7u, 0x94u, 0x30u,
	0xa0u, 0xcau, 0x30u, 0x5cu, 0x88u, 0xe5u, 0x24u, 0xddu,
};

/* 0x812cc4: 16-byte zero IV for Auth1 */
static const uint8_t s_pmZeroIvAuth[16] = { 0 };

/* 0x812cd4: 64-byte key entry for slot 15 (0x0f) */
static uint8_t s_pmKeyEntry15[64] = {
	0x50u, 0xe4u, 0xc3u, 0xa7u, 0x72u, 0x64u, 0x16u, 0x7cu,
	0x40u, 0x9cu, 0x72u, 0xa9u, 0xb5u, 0x7au, 0x86u, 0x09u,
	0x9eu, 0x34u, 0x08u, 0x7cu, 0x48u, 0x98u, 0x5bu, 0x4bu,
	0x35u, 0x1au, 0x63u, 0x57u, 0x2du, 0x9bu, 0x48u, 0x1bu,
	0xebu, 0xe3u, 0x46u, 0x0du, 0x84u, 0xa4u, 0x17u, 0x54u,
	0xacu, 0x44u, 0x13u, 0x68u, 0xcfu, 0x02u, 0x00u, 0xd8u,
	0xc8u, 0x6bu, 0x51u, 0xfbu, 0x01u, 0x9au, 0x20u, 0x7fu,
	0x32u, 0x11u, 0x8eu, 0x55u, 0x46u, 0x2du, 0x50u, 0x08u,
};

/* 0x812d18: 16-byte zero IV for SNVS */
static const uint8_t s_pmZeroIv16[16] = { 0 };

/* 0x812d28: PmSnvsCipher vtable */
static const void * const s_pmSnvsCipherVtable[4] = {
	NULL,
	NULL,
	(const void *)pmSnvsCipherDtorD2,
	(const void *)pmSnvsCipherDtorD0,
};

/* 0x812d38: Bitwise-inverted "SNVS" magic */
static const uint8_t s_pmInvMagicSnvs[4] = {
	0xacu, 0xb1u, 0xa9u, 0xacu,
};

/* 0x812d3c: 16-byte zero IV for Auth2 subcmd 4 */
static const uint8_t s_pmZeroIvSubcmd[16] = { 0 };

/* 0x812d4c: 16-byte constant challenge block for Auth2 subcmd 4 */
static const uint8_t s_pmSubcmd4Magic[16] = {
	0xb0u, 0x11u, 0x03u, 0xb0u, 0x62u, 0x38u, 0x32u, 0xd6u,
	0x25u, 0x40u, 0xb5u, 0x63u, 0x33u, 0xd6u, 0xe1u, 0x1du,
};

/* 0x813ab8: Key slot pointer table (only slot 15 populated) */
static uint8_t *s_pmKeyTable[16] = {
	[15] = s_pmKeyEntry15,
};

typedef int (*PmAesEcbFn)(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
static PmAesEcbFn s_pmXtsEcbEncryptFn;
static PmAesEcbFn s_pmXtsEcbSecondFn;

/* 0x812d5c: Update list HMAC-SHA256 key table (1-based index 1..10) */
static const uint8_t * const s_updListHmacKeyTable[11] = {
	(const uint8_t *)(uintptr_t)10u,
	s_updListHmacKeys[0],
	s_updListHmacKeys[1],
	s_updListHmacKeys[2],
	s_updListHmacKeys[3],
	s_updListHmacKeys[4],
	s_updListHmacKeys[5],
	s_updListHmacKeys[6],
	s_updListHmacKeys[7],
	s_updListHmacKeys[8],
	s_updListHmacKeys[9],
};

/* 0x812d88: SCESLB2 magic */
static const char s_updSlb2Magic[8] = "SCESLB2";

/* 0x813010: UpdListVerifier vtable */
static const uintptr_t s_updListVerifierVtable[4] = {
	0u,
	0u,
	(uintptr_t)updListVerifierDtorD2,
	(uintptr_t)updListVerifierDtorD0,
};

/* 0x813020 & 0x813028: ENC IS / ENC ID package magic strings */
static const char s_updEncIsMagic[8] = "ENC IS";
static const char s_updEncIdMagic[8] = "ENC ID";

/* 0x813030: SpkgVerifier vtable */
static const uintptr_t s_spkgVerifierVtable[4] = {
	0u,
	0u,
	(uintptr_t)spkgVerifierDtorD2,
	(uintptr_t)spkgVerifierDtorD0,
};

/* 0x813040..0x813478: 90 hardware revision minimum firmware version & attribute rules */
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

/* 0x813478..0x8138a4: 89 Console ID product/subcode to hardware revision index mappings */
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

/* 0x8138a4..0x8138bc: 3 SPKG segment specifications to verify/decrypt */
static const SpkgSegmentSpec s_spkgSegmentSpecs[3] = {
	{ 1u, 1u },
	{ 2u, 2u },
	{ 3u, 3u },
};

/* 0x8138bc..0x8138e0: rule table pointers indexed by (pkgType - 1) */
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

static const uint8_t s_spkgRsaExponent[4] = {
	0x00u, 0x01u, 0x00u, 0x01u,
};

/* ============================================================================
 * .bss Globals (0x814000..0x81c7e0)
 * ============================================================================ */

static uint8_t s_updHeapPool[0x4000] __attribute__((aligned(16)));
static UpdListVerifier s_updListVerifier;
static uint8_t s_updListVerified;
static SpkgVerifier s_spkgVerifier;

/* Key slot 15 Auth1 / Auth2 protocol state (0x818200..0x81833f) */
static uint8_t s_updAuthInitHdr[40];
static uint8_t s_updAuthChallengeIn[40];
static uint32_t s_updAuthChallengeReady;
static uint8_t s_updAuthRngNonce[8];
static uint8_t s_updAuthChallengeOut[40];
static uint8_t s_updAuthVerifyIn[40];
static uint8_t s_updAuthSessionKey[32];
static uint8_t s_updAuthSubcmd4Out[40];
static uint8_t s_updAuthSubcmd4In[40];

/* SNVS sector transfer scratch buffer (0x818340..0x81873f) */
static uint8_t s_pmSnvsScratch[992];

/* SNVS cipher instance and keyring buffers (0x818740..0x8187bf) */
static PmSnvsCipher s_pmSnvsCipher;
static uint32_t s_pmSnvsInitialized;
static uint8_t s_pmSnvsKey1[32];
static uint8_t s_pmSnvsKey2[32];
static uint8_t s_pmSnvsKey3[32];

#if FW_VERSION >= 0x04000000
/* 0x8187c0: 4096-byte ENC IS work buffer (BSS on 4.00, stack on 3.63/3.65) */
static uint8_t s_updEncIsWorkBuf[0x1000];
#endif

/* 0x81a7e0: 8192-byte SPKG Certified File header buffer */
static uint8_t s_spkgHdrBuf[0x2000] __attribute__((aligned(16)));

static void (* const s_updInitArray[])(void)
	__attribute__((section(".ctors"), used)) = {
	updListStaticInitThunk,
	spkgStaticInitThunk,
	pmStaticInitThunk,
};

static void (* const s_updFiniArray[])(void)
	__attribute__((section(".dtors"), used)) = {
	updListStaticDtorThunk,
	spkgStaticDtorThunk,
	pmStaticDtorThunk,
};

/* ============================================================================
 * Service Entry & Top-Level Command Handlers (0x80b09c..0x80bea1)
 * ============================================================================ */

/* 0x80b09c */
int sm_main(void)
{
	SmServiceContext srvCtx;
	SmServiceHookNode *nodes[13];
	void (*fn)(void *);
	int status = SCE_SBL_ERROR_UPD_INVALID_VALUE;

	smHeapInit(s_updHeapPool, sizeof(s_updHeapPool));
	smSrvInit(&srvCtx);

	fn = updCmdVerifyUpdateList;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_VERIFY_UPDATE_LIST,
			&fn, &nodes[0]) != 0)
		goto cleanup;

	fn = updCmdCheckEntryInUpdateList;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_CHECK_UPDATE_ENTRY,
			&fn, &nodes[1]) != 0)
		goto cleanup;

	fn = updCmdVerifyEncIs;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_VERIFY_ENC_IS,
			&fn, &nodes[2]) != 0)
		goto cleanup;

	fn = updCmdVerifyEncId;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_VERIFY_ENC_ID,
			&fn, &nodes[3]) != 0)
		goto cleanup;

	fn = spkgCmdVerifyHeaderAndSegments;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_VERIFY_SPKG,
			&fn, &nodes[4]) != 0)
		goto cleanup;

	fn = updCmdDmaCopyScatterList;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_DMA_COPY_SCATTER,
			&fn, &nodes[5]) != 0)
		goto cleanup;

	fn = updCmdSnvsWriteSectors;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_SNVS_WRITE_SECTORS,
			&fn, &nodes[6]) != 0)
		goto cleanup;

	fn = updCmdSnvsReadSectors;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_SNVS_READ_SECTORS,
			&fn, &nodes[7]) != 0)
		goto cleanup;

	fn = updCmdSnvsWriteStatus;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_SNVS_WRITE_STATUS,
			&fn, &nodes[8]) != 0)
		goto cleanup;

	fn = updCmdSnvsReadStatus;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_SNVS_READ_STATUS,
			&fn, &nodes[9]) != 0)
		goto cleanup;

	fn = updCmdSnvsSectorProtocol;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_SNVS_SECTOR_PROTOCOL,
			&fn, &nodes[10]) != 0)
		goto cleanup;

	fn = updCmdSnvsHeaderProtocol;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_SNVS_HEADER_PROTOCOL,
			&fn, &nodes[11]) != 0)
		goto cleanup;

	fn = updCmdAuthKeySlot15;
	if (updRegisterSimpleHook(&srvCtx, SCE_SBL_UPD_CMD_AUTH_KEYSLOT_15,
			&fn, &nodes[12]) != 0)
		goto cleanup;

	status = smSrvRunLoop(&srvCtx);

cleanup:
	smSrvTeardown(&srvCtx);
	return status;
}

/* 0x80b26c (name ours) */
void updHookInvoke(void *self, void *commBuf)
{
	void (**fnPtr)(void *) = (void (**)(void *))((uint8_t *)self + 4u);

	(*fnPtr)(commBuf);
}

/* 0x80b280 (name ours) */
int updRegisterSimpleHook(SmServiceContext *srvCtx, uint32_t cmdId,
		void (**fnPtr)(void *), SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;

	node->cmdId = cmdId;
	node->next = NULL;
	if (&node->vtable != NULL) {
		node->vtable = &s_updHookVtable[2];
		node->handler = (void *)*fnPtr;
	}
	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b2e6 (name ours) */
void updListStaticInitOrDtor(int isInit)
{
	if (isInit == 1)
		updListVerifierCtorC1(&s_updListVerifier);
	else if (isInit == 0)
		updListVerifierDtorD2(&s_updListVerifier);
}

/* 0x80b306 (name ours; mailbox command 0x00010002) */
void updCmdVerifyUpdateList(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	int status;

	s_updListVerified = 0u;
	status = updListSetFlags(&s_updListVerifier, 0x401u);
	if (status == 0) {
		status = updListVerifyPackage(&s_updListVerifier,
				(SpkgVerifyRequest *)(void *)(pkt + 0x40u));
		if (status == 0)
			s_updListVerified = 1u;
	}
	*(int *)(void *)(pkt + 8u) = status;
}

/* 0x80b348 (name ours; mailbox command 0x00020002) */
void updCmdCheckEntryInUpdateList(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;

	if (s_updListVerified == 0u) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_NOT_READY;
		return;
	}
	*(int *)(void *)(pkt + 8u) = updListVerifyEntry(&s_updListVerifier,
			(const SpkgVerifyRequest *)(const void *)(pkt + 0x40u));
}

/* 0x80b380 (name ours; mailbox command 0x00030002) */
void updCmdVerifyEncIs(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	UpdEncPackageVerifier verifier;
	uint8_t nonceStorage[96];
	void *rawAlloc;
	uint8_t *pkgBuf;
	uint8_t *nonceBuf;
	int status;

	updEncIsCtorC1(&verifier);
	rawAlloc = smHeapMalloc(0x103fu);
	if (rawAlloc == NULL) {
		status = SCE_SBL_ERROR_UPD_NO_MEMORY;
		goto done;
	}

	pkgBuf = (uint8_t *)(((uintptr_t)rawAlloc + 63u) & ~(uintptr_t)63u);
	status = updEncIsCopyFromScatter(&verifier, pkgBuf,
			(const UpdEncScatterRequest *)(const void *)(pkt + 0x40u),
			0u, 0x1000u);
	if (status != 0)
		goto free_buf;

	nonceBuf = (uint8_t *)(((uintptr_t)nonceStorage + 63u) & ~(uintptr_t)63u);
	memcpy(nonceBuf, pkt + 0x48u, 32u);

	if (*(const uint32_t *)(const void *)(pkgBuf + 0x14u) != 1u) {
		status = SCE_SBL_ERROR_UPD_INVALID_VALUE;
		goto free_buf;
	}

	status = updEncSetRsaPubKey(&verifier, &s_updEncIsRsaPubKey);
	if (status != 0)
		goto free_buf;

	status = updEncSetAesKeyIv(&verifier, g_updEncIsAesKey, g_updEncIsAesIv);
	if (status != 0)
		goto free_buf;

	status = updVerifyEncIsPackage(&verifier, nonceBuf, 32u, pkgBuf, NULL);

free_buf:
	smHeapFree(rawAlloc);
done:
	*(int *)(void *)(pkt + 8u) = status;
	updEncIsDtorD1(&verifier);
}

/* 0x80b462 (name ours; mailbox command 0x000a0002) */
void updCmdVerifyEncId(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	UpdEncPackageVerifier verifier;
	uint8_t nonceStorage[96];
	void *rawAlloc;
	uint8_t *pkgBuf;
	uint8_t *nonceBuf;
	int status;

	updEncIdCtorC1(&verifier);
	rawAlloc = smHeapMalloc(0x43fu);
	if (rawAlloc == NULL) {
		status = SCE_SBL_ERROR_UPD_NO_MEMORY;
		goto done;
	}

	pkgBuf = (uint8_t *)(((uintptr_t)rawAlloc + 63u) & ~(uintptr_t)63u);
	status = updEncIdCopyFromScatter(&verifier, pkgBuf,
			(const UpdEncScatterRequest *)(const void *)(pkt + 0x40u),
			0u, 0x400u);
	if (status != 0)
		goto free_buf;

	nonceBuf = (uint8_t *)(((uintptr_t)nonceStorage + 63u) & ~(uintptr_t)63u);
	memcpy(nonceBuf, pkt + 0x48u, 32u);

	if (*(const uint32_t *)(const void *)(pkgBuf + 0x10u) != 1u) {
		status = SCE_SBL_ERROR_UPD_INVALID_VALUE;
		goto free_buf;
	}

	status = updEncSetRsaPubKey(&verifier, &s_updEncIdRsaPubKey);
	if (status != 0)
		goto free_buf;

	status = updEncSetAesKeyIv(&verifier, g_updEncIdAesKey, g_updEncIdAesIv);
	if (status != 0)
		goto free_buf;

	status = updVerifyEncIdPackage(&verifier, nonceBuf, 32u, pkgBuf, NULL);

free_buf:
	smHeapFree(rawAlloc);
done:
	*(int *)(void *)(pkt + 8u) = status;
	updEncIdDtorD1(&verifier);
}

/* 0x80b544 (name ours) */
void updListStaticInitThunk(void)
{
	updListStaticInitOrDtor(1);
}

/* 0x80b54a (name ours) */
void updListStaticDtorThunk(void)
{
	updListStaticInitOrDtor(0);
}

/* 0x80b550 (name ours) */
void spkgStaticInitOrDtor(int isInit)
{
	if (isInit == 1)
		spkgVerifierCtorC1(&s_spkgVerifier);
	else if (isInit == 0)
		spkgVerifierDtorD2(&s_spkgVerifier);
}

/* 0x80b570 (name ours; mailbox command 0x00040002) */
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

/* 0x80b5ac (name ours) */
void spkgStaticInitThunk(void)
{
	spkgStaticInitOrDtor(1);
}

/* 0x80b5b2 (name ours) */
void spkgStaticDtorThunk(void)
{
	spkgStaticInitOrDtor(0);
}

/* 0x80b5b8 (name ours) */
int updDmaValidateArmRange(uint32_t addr, uint32_t size)
{
	return spkgValidateArmRange(addr, size);
}

/* 0x80b68c (name ours) */
int updDmaCountScatterEntries(const SpkgVerifyRequest *req, uint32_t *outCount)
{
	uint32_t total = 0u;
	uint32_t i;

	if (req == NULL || req->listCount > 481u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	if (req->mode == 1ULL) {
		for (i = 0u; i < req->listCount; i++)
			total += req->u.lists[i].byteSize >> 3;
	} else {
		total = req->inlineEntryCount;
	}
	*outCount = total;
	return 0;
}

/* 0x80b6d2 (name ours) */
int updDmaBuildLliChain(const SpkgVerifyRequest *req, const void *ivPtr,
		BigmacLliDesc *lliBuf, uint32_t count)
{
	int status;
	uint32_t i;

	if (req == NULL || lliBuf == NULL || count == 0u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	status = bigmacInitLliChain(lliBuf, count, 0x2000u, 8u, ivPtr);
	if (status != 0)
		return status;

	if (req->listCount > 481u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	if (req->mode == 1ULL) {
		BigmacLliDesc *curDesc = lliBuf;

		for (i = 0u; i < req->listCount; i++) {
			const SpkgScatterListDesc *list = &req->u.lists[i];
			uint32_t nEntries;
			uint32_t j;

			if (updDmaValidateArmRange((uint32_t)(uintptr_t)list->entries,
					list->byteSize) == 0) {
				return SCE_SBL_ERROR_UPD_INVALID_VALUE;
			}
			nEntries = list->byteSize >> 3;
			for (j = 0u; j < nEntries; j++) {
				uint32_t paddr = list->entries[j].paddr;
				uint32_t sz = list->entries[j].size;

				curDesc->dst = paddr;
				curDesc->src = paddr;
				curDesc->size = sz;
				if (updDmaValidateArmRange(paddr, sz) == 0)
					return SCE_SBL_ERROR_UPD_INVALID_VALUE;
				curDesc++;
			}
		}
	} else {
		for (i = 0u; i < req->listCount; i++) {
			uint32_t paddr = req->u.inlineEntries[i].paddr;
			uint32_t sz = req->u.inlineEntries[i].size;

			lliBuf[i].dst = paddr;
			lliBuf[i].src = paddr;
			lliBuf[i].size = sz;
			if (updDmaValidateArmRange(paddr, sz) == 0)
				return SCE_SBL_ERROR_UPD_INVALID_VALUE;
		}
	}
	return 0;
}

/* 0x80b7da (name ours) */
int updDmaExecuteLliChain(BigmacLliDesc *lliBuf)
{
	uint32_t bmCtrl[2];

	if (bigmacInitChannel0(bmCtrl) != 0)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;
	if (bigmacStartAes128CbcEncryptLli(lliBuf, bmCtrl) != 0)
		return SCE_SBL_ERROR_UPD_DMA_IO;
	if (bigmacWaitChannelDone(bmCtrl) != 0)
		return SCE_SBL_ERROR_UPD_DMA_IO;
	return 0;
}

/* 0x80b826 (name ours; mailbox command 0x00050002) */
void updCmdDmaCopyScatterList(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	const SpkgVerifyRequest *req =
		(const SpkgVerifyRequest *)(const void *)(pkt + 0x40u);
	uint32_t entryCount = 0u;
	void *rawAlloc;
	BigmacLliDesc *lliBuf;
	int status;

	memcpy(s_updDmaWorkIv, s_updDmaDefaultIv, sizeof(s_updDmaWorkIv));
	status = updDmaCountScatterEntries(req, &entryCount);
	if (status != 0)
		goto done;

	rawAlloc = smHeapMalloc((entryCount << 5) + 31u);
	if (rawAlloc == NULL) {
		status = SCE_SBL_ERROR_UPD_NO_MEMORY;
		goto done;
	}

	lliBuf = (BigmacLliDesc *)(((uintptr_t)rawAlloc + 31u) & ~(uintptr_t)31u);
	status = updDmaBuildLliChain(req, s_updDmaWorkIv, lliBuf, entryCount);
	if (status == 0)
		status = updDmaExecuteLliChain(lliBuf);

	smHeapFree(rawAlloc);
done:
	*(int *)(void *)(pkt + 8u) = status;
}

/* 0x80b8a8 (name ours) */
void updSnvsTransferSectors(void *commBuf, uint32_t isRead)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t startSector;
	uint32_t sectorCount;
	int status;

	pmEnsureSnvsCryptoInitialized();
	memset(s_pmSnvsScratch, 0, sizeof(s_pmSnvsScratch));

	startSector = *(const uint32_t *)(const void *)(pkt + 0x40u);
	sectorCount = *(const uint32_t *)(const void *)(pkt + 0x44u);
	if (startSector - 1u > 29u || sectorCount > 31u ||
	    startSector + sectorCount >= 32u) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
		return;
	}

	memcpy(s_pmSnvsScratch, pkt + 0x48u, sectorCount << 5);
	if (isRead == 0u) {
		status = pmEncryptSnvsSectors(startSector, pkt + 0x48u,
				s_pmSnvsScratch, sectorCount);
	} else if (isRead == 1u) {
		status = pmDecryptSnvsSectors(startSector, pkt + 0x48u,
				s_pmSnvsScratch, sectorCount);
	} else {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
		return;
	}

	if (status != 0) {
		*(int *)(void *)(pkt + 8u) = status;
		return;
	}

	memcpy(pkt + 0x48u, s_pmSnvsScratch, sectorCount << 5);
	*(int *)(void *)(pkt + 8u) = 0;
}

/* 0x80b964 (name ours) */
void updReadSnvsCounter2240(uint8_t *outCounter4)
{
	outCounter4[0] = REG8(0xe0062240u);
	outCounter4[1] = REG8(0xe0062241u);
	outCounter4[2] = REG8(0xe0062242u);
	outCounter4[3] = REG8(0xe0062243u);
}

/* 0x80b99c (name ours; mailbox command 0x00060002) */
void updCmdSnvsWriteSectors(void *commBuf)
{
	updSnvsTransferSectors(commBuf, 0u);
}

/* 0x80b9a2 (name ours; mailbox command 0x00070002) */
void updCmdSnvsReadSectors(void *commBuf)
{
	updSnvsTransferSectors(commBuf, 1u);
}

/* 0x80b9a8 (name ours; mailbox command 0x00080002) */
void updCmdSnvsWriteStatus(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t sector0Buf[32];
	uint32_t curWord0;
	uint32_t curWord1;
	int status;

	pmEnsureSnvsCryptoInitialized();
	memcpy(sector0Buf, pkt + 0x48u, 32u);
	status = pmReadSnvsSector0(&curWord0, &curWord1, sector0Buf);
	if (status != 0) {
		*(int *)(void *)(pkt + 8u) = status;
		return;
	}

	status = pmWriteSnvsSector0(pkt + 0x40u, pkt + 0x44u, sector0Buf);
	if (status == 0)
		memcpy(pkt + 0x48u, sector0Buf, 32u);
	*(int *)(void *)(pkt + 8u) = status;
}

/* 0x80ba24 (name ours; mailbox command 0x00090002) */
void updCmdSnvsReadStatus(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t sector0Buf[32];
	uint32_t word0;
	uint32_t word1;
	int status;

	pmEnsureSnvsCryptoInitialized();
	memcpy(sector0Buf, pkt + 0x48u, 32u);
	status = pmReadSnvsSector0(&word0, &word1, sector0Buf);
	if (status == 0) {
		memcpy(pkt + 0x40u, &word0, 4u);
		memcpy(pkt + 0x44u, &word1, 4u);
	}
	*(int *)(void *)(pkt + 8u) = status;
}

/* 0x80ba9e (name ours; mailbox command 0x000b0002) */
void updCmdSnvsSectorProtocol(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t op = *(const uint32_t *)(const void *)(pkt + 0x40u);
	uint32_t sectorIdx = *(const uint32_t *)(const void *)(pkt + 0x44u);
	uint32_t hwCounter = 0u;
	uint32_t unwrappedSector = 0u;
	uint32_t savedCounter;
	int status;

	pmEnsureSnvsCryptoInitialized();
	updReadSnvsCounter2240((uint8_t *)(void *)&hwCounter);
	savedCounter = hwCounter;
	memset(s_pmSnvsScratch, 0, sizeof(s_pmSnvsScratch));

	if (op == 0u) {
		status = pmBuildSnvsMacChallenge(hwCounter, sectorIdx,
				pkt + 0x68u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return;
		}
		*(int *)(void *)(pkt + 8u) = 0;
		return;
	}

	if (op == 1u) {
		status = pmVerifyAndUnwrapSnvsSectorWrite(&hwCounter,
				&unwrappedSector, s_pmSnvsScratch, 32u,
				pkt + 0x98u);
		if (savedCounter != hwCounter) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
			return;
		}
		hwCounter = pmIncrementSnvsCounter(savedCounter);
		memset(pkt + 0x98u, 0, 48u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return;
		}
		if (sectorIdx != unwrappedSector) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
			return;
		}
		status = pmDecryptSnvsSectors(sectorIdx, s_pmSnvsScratch,
				pkt + 0x48u, 1u);
		*(int *)(void *)(pkt + 8u) = status;
		return;
	}

	if (op == 2u) {
		status = pmEncryptSnvsSectors(sectorIdx, pkt + 0x48u,
				s_pmSnvsScratch, 1u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return;
		}
		status = pmWrapSnvsSectorReadResponse(hwCounter, sectorIdx,
				s_pmSnvsScratch, 32u, pkt + 0x68u);
		*(int *)(void *)(pkt + 8u) = status;
		return;
	}

	if (op == 3u) {
		status = pmVerifySnvsCommitPacket(&hwCounter, &unwrappedSector,
				pkt + 0x98u);
		if (savedCounter != hwCounter) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
			return;
		}
		hwCounter = pmIncrementSnvsCounter(savedCounter);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return;
		}
		memset(pkt + 0x98u, 0, 48u);
		if (sectorIdx == unwrappedSector) {
			*(int *)(void *)(pkt + 8u) = 0;
			return;
		}
	}

	*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
}

/* 0x80bbce (name ours; mailbox command 0x000c0002) */
void updCmdSnvsHeaderProtocol(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t sector0Buf[32];
	uint32_t op = *(const uint32_t *)(const void *)(pkt + 0x40u);
	uint32_t hwCounter = 0u;
	uint32_t unwrappedSector = 0u;
	uint32_t savedCounter;
	int status;

	pmEnsureSnvsCryptoInitialized();
	updReadSnvsCounter2240((uint8_t *)(void *)&hwCounter);
	savedCounter = hwCounter;
	memset(s_pmSnvsScratch, 0, sizeof(s_pmSnvsScratch));

	if (op == 0u) {
		status = pmBuildSnvsMacChallenge(hwCounter, 0u, pkt + 0x50u);
		*(int *)(void *)(pkt + 8u) = status;
		return;
	}

	if (op == 1u) {
		status = pmVerifyAndUnwrapSnvsSectorWrite(&hwCounter,
				&unwrappedSector, sector0Buf, 32u,
				pkt + 0x80u);
		if (savedCounter != hwCounter) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
			return;
		}
		hwCounter = pmIncrementSnvsCounter(savedCounter);
		memset(pkt + 0x80u, 0, 48u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return;
		}
		if (unwrappedSector != 0u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
			return;
		}
		status = pmReadSnvsSector0(pkt + 0x48u, pkt + 0x4cu,
				sector0Buf);
		*(int *)(void *)(pkt + 8u) = status;
		return;
	}

	if (op == 2u) {
		status = pmWriteSnvsSector0(pkt + 0x48u, pkt + 0x4cu,
				sector0Buf);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return;
		}
		status = pmWrapSnvsSectorReadResponse(hwCounter, 0u,
				sector0Buf, 32u, pkt + 0x50u);
		*(int *)(void *)(pkt + 8u) = status;
		return;
	}

	if (op == 3u) {
		status = pmVerifySnvsCommitPacket(&hwCounter, &unwrappedSector,
				pkt + 0x80u);
		if (savedCounter != hwCounter) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
			return;
		}
		hwCounter = pmIncrementSnvsCounter(savedCounter);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return;
		}
		memset(pkt + 0x80u, 0, 48u);
		if (unwrappedSector == 0u) {
			*(int *)(void *)(pkt + 8u) = 0;
			return;
		}
	}

	*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_UPD_INVALID_VALUE;
}

/* 0x80bcfa (name ours; mailbox command 0x000d0002) */
void updCmdAuthKeySlot15(void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t *req = pkt + 0x40u;
	uint32_t op = *(const uint32_t *)(const void *)req;

	switch (op) {
	case 0u:
		if (pmBuildAuth1InitHeader(s_updAuthInitHdr, 15u) == 0)
			memcpy(req + 0x30u, s_updAuthInitHdr, 40u);
		break;

	case 1u:
		memcpy(s_updAuthChallengeIn, req + 8u, 40u);
		s_updAuthChallengeReady = 1u;
		if (pmGetRandomBytes(s_updAuthRngNonce, 8u) != 0) {
			s_updAuthChallengeReady = 0u;
			break;
		}
		if (pmBuildAuth1Challenge(s_updAuthChallengeOut,
				s_updAuthChallengeIn, 15u, 1u,
				s_updAuthRngNonce) != 0) {
			s_updAuthChallengeReady = 0u;
			break;
		}
		memcpy(req + 0x30u, s_updAuthChallengeOut, 40u);
		break;

	case 2u:
		if (s_updAuthChallengeReady == 0u)
			break;
		memcpy(s_updAuthVerifyIn, req + 8u, 40u);
		(void)pmVerifyAuth1Response(s_updAuthVerifyIn, 15u, 1u,
				s_updAuthChallengeIn + 8u,
				s_updAuthRngNonce, s_updAuthSessionKey);
		break;

	case 3u:
		if (pmBuildSubcmd4Challenge(s_updAuthSubcmd4Out, 1u,
				s_updAuthSessionKey) == 0) {
			memcpy(req + 0x30u, s_updAuthSubcmd4Out, 40u);
		}
		break;

	case 4u:
		memcpy(s_updAuthSubcmd4In, req + 8u, 40u);
		if (pmVerifySubcmd4Response(s_updAuthSubcmd4In, 1u,
				s_updAuthSessionKey) == 0) {
			const uint32_t *sk =
				(const uint32_t *)(const void *)s_updAuthSessionKey;
			uint32_t rngWord;
			union {
				uint8_t b[4];
				uint32_t w;
			} u;

			REG32(0xe0030000u) = sk[0];
			REG32(0xe0030004u) = sk[1];
			REG32(0xe0030008u) = sk[2];
			REG32(0xe003000cu) = sk[3];
			REG32(0xe0030010u) = sk[4];
			REG32(0xe0030014u) = sk[5];
			REG32(0xe0030018u) = sk[6];
			REG32(0xe003001cu) = sk[7];
			REG32(0xe0030020u) = 0x511u;

			rngWord = REG32(0xe005003cu);
			u.b[0] = (uint8_t)rngWord;
			u.b[1] = (uint8_t)(rngWord >> 8);
			u.b[2] = (uint8_t)(rngWord >> 16);
			u.b[3] = (uint8_t)(rngWord >> 24);
			REG32(0xe0030000u) = u.w;
			REG32(0xe0030020u) = 0x512u;
		}
		break;

	default:
		break;
	}

	*(int *)(void *)(pkt + 8u) = 0;
}
/* ============================================================================
 * Key Slot Validation & SNVS Sector Engine (0x80bfa6..0x80d0bb)
 * ============================================================================ */

/* 0x80bfa6 (name ours) */
int pmCheckKeyId(uint32_t keyId)
{
	if (keyId != 15u)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if (s_pmKeyTable[15] == NULL)
		return (int)0x800f0028u;
	return 0;
}

/* 0x80bfc6 (name ours) */
void pmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	volatile const uint32_t *slot =
		(volatile const uint32_t *)(0xe0058000u + (slotIndex << 5));

	outWords8[0] = slot[0];
	outWords8[1] = slot[1];
	outWords8[2] = slot[2];
	outWords8[3] = slot[3];
	outWords8[4] = slot[4];
	outWords8[5] = slot[5];
	outWords8[6] = slot[6];
	outWords8[7] = slot[7];
}

/* 0x80c010 (name ours) */
void pmStaticInitOrDtor(int isInit)
{
	if (isInit == 1)
		pmSnvsCipherCtorC1(&s_pmSnvsCipher);
	else if (isInit == 0)
		pmSnvsCipherDtorD2(&s_pmSnvsCipher);
}

/* 0x80c030 (name ours) */
int pmEnsureSnvsCryptoInitialized(void)
{
	int status = 0;

	if (s_pmSnvsInitialized == 0u) {
		memset(s_pmSnvsKey1, 0, 32u);
		memset(s_pmSnvsKey2, 0, 32u);
		memset(s_pmSnvsKey3, 0, 32u);
		pmReadKeyringSlot32(0x502u, (uint32_t *)(void *)s_pmSnvsKey1);
		pmReadKeyringSlot32(0x503u, (uint32_t *)(void *)s_pmSnvsKey2);
		pmReadKeyringSlot32(0x504u, (uint32_t *)(void *)s_pmSnvsKey3);
		status = pmSnvsCipherInit(&s_pmSnvsCipher,
				s_pmSnvsKey1, s_pmSnvsKey2, s_pmSnvsKey3,
				128u, 128u);
		if (status == 0)
			s_pmSnvsInitialized = 1u;
	}
	return status;
}

/* 0x80c0ac (name ours) */
int pmReadSnvsSector0(void *outWord0, void *outWord1, const void *inSector32)
{
	uint8_t decBuf[32];
	uint8_t macCheck[32];
	uint8_t savedMac[16];
	int status;

	if (s_pmSnvsInitialized == 0u || outWord0 == NULL ||
	    outWord1 == NULL || inSector32 == NULL) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memset(decBuf, 0, sizeof(decBuf));
	status = pmSnvsDecryptSectors(&s_pmSnvsCipher, inSector32,
			decBuf, 32u, 0u);
	if (status != 0)
		return status;

	memcpy(macCheck, decBuf, 32u);
	memset(savedMac, 0, sizeof(savedMac));
	status = pmSnvsSignRecordCmac(&s_pmSnvsCipher, macCheck);
	if (status != 0)
		return status;

	if (timingsafeBcmp(savedMac, decBuf + 16u, 16u) != 0)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	memcpy(outWord0, macCheck + 8u, 4u);
	memcpy(outWord1, macCheck + 12u, 4u);
	return 0;
}

/* 0x80c15c (name ours) */
int pmValidateSectorRange(uint32_t sectorCount, uint32_t startSector)
{
	if (sectorCount - 1u > 30u || startSector > 31u)
		return 0;
	return (startSector + sectorCount < 32u);
}

/* 0x80c178 (name ours) */
int pmDecryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount)
{
	if (s_pmSnvsInitialized == 0u || dst == NULL || src == NULL ||
	    pmValidateSectorRange(startSector, sectorCount) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}
	return pmSnvsDecryptSectors(&s_pmSnvsCipher, src, dst,
			sectorCount * 32u, startSector);
}

/* 0x80c1ce (name ours) */
uint32_t pmIsSnvsInitialized(void)
{
	return s_pmSnvsInitialized;
}

/* 0x80c1d4 (name ours) */
PmSnvsCipher *pmGetSnvsCipher(void)
{
	return &s_pmSnvsCipher;
}

/* 0x80c1da (name ours) */
const uint8_t *pmGetZeroIv16(void)
{
	return s_pmZeroIv16;
}

/* 0x80c1e0 (name ours) */
int pmBuildSnvsMacChallenge(uint32_t counter, uint32_t sectorIdx,
		uint8_t *outChallenge16)
{
	uint32_t slot516[8];
	uint32_t slot511[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint32_t byteOff;
	uint16_t sum = 0u;
	uint32_t i;
	int status;

	pmReadKeyringSlot32(0x516u, slot516);
	if (slot516[0] == 1u)
		return SCE_SBL_ERROR_NOT_INITIALIZED;

	byteOff = sectorIdx * 32u;
	if (outChallenge16 == NULL || byteOff > 992u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(outChallenge16, 0, 16u);
	outChallenge16[0] = (uint8_t)byteOff;
	outChallenge16[1] = (uint8_t)(byteOff >> 8);
	outChallenge16[2] = 32u;
	outChallenge16[3] = 0u;
	outChallenge16[4] = (uint8_t)counter;
	outChallenge16[5] = (uint8_t)(counter >> 8);
	outChallenge16[6] = (uint8_t)(counter >> 16);
	outChallenge16[7] = (uint8_t)(counter >> 24);

	for (i = 0u; i < 13u; i++)
		sum = (uint16_t)(sum + outChallenge16[i]);
	sum = (uint16_t)~sum;
	outChallenge16[14] = (uint8_t)sum;
	outChallenge16[15] = (uint8_t)(sum >> 8);

	pmReadKeyringSlot32(0x511u, slot511);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, s_pmZeroIv16, 16u);
	status = bigmacStartAes128CbcEncrypt(outChallenge16, outChallenge16,
			16u, slot511, iv, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80c2e2 (name ours) */
int pmVerifyAndUnwrapSnvsSectorWrite(uint32_t *inOutCounter,
		uint32_t *outSectorIdx, uint8_t *outSectorData32,
		uint32_t sectorSize, uint8_t *inOutPacket48)
{
	uint32_t slot511[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint16_t sum = 0u;
	uint16_t pktSum;
	uint32_t pktCounter;
	uint16_t pktByteOff;
	uint32_t i;
	int status;

	if (inOutCounter == NULL || outSectorIdx == NULL ||
	    outSectorData32 == NULL || inOutPacket48 == NULL ||
	    sectorSize != 32u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	pmReadKeyringSlot32(0x511u, slot511);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, s_pmZeroIv16, 16u);
	status = bigmacStartAes128CbcDecrypt(inOutPacket48, inOutPacket48,
			48u, slot511, iv, bmCtrl);
	if (status != 0)
		return status;
	status = bigmacWaitChannelDone(bmCtrl);
	if (status != 0)
		return status;

	for (i = 0u; i < 45u; i++)
		sum = (uint16_t)(sum + inOutPacket48[i]);
	sum = (uint16_t)~sum;
	pktSum = (uint16_t)inOutPacket48[46] | ((uint16_t)inOutPacket48[47] << 8);
	if (pktSum != sum)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	pktCounter = (uint32_t)inOutPacket48[4] |
		     ((uint32_t)inOutPacket48[5] << 8) |
		     ((uint32_t)inOutPacket48[6] << 16) |
		     ((uint32_t)inOutPacket48[7] << 24);
	if (pktCounter != *inOutCounter)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	*inOutCounter = pktCounter;
	pktByteOff = (uint16_t)inOutPacket48[0] | ((uint16_t)inOutPacket48[1] << 8);
	*outSectorIdx = (uint32_t)(pktByteOff >> 5);
	memcpy(outSectorData32, inOutPacket48 + 14u, 32u);
	return status;
}

/* 0x80c3ee (name ours) */
uint32_t pmIncrementSnvsCounter(uint32_t counter)
{
	uint32_t nextCounter = counter + 1u;

	REG32(0xe0030000u) = nextCounter;
	REG32(0xe0030020u) = 0x512u;
	return nextCounter;
}

/* 0x80c424 (name ours) */
void pmStaticInitThunk(void)
{
	pmStaticInitOrDtor(1);
}

/* 0x80c42a (name ours) */
void pmStaticDtorThunk(void)
{
	pmStaticInitOrDtor(0);
}

/* 0x80c430 (name ours) */
void pmSnvsCipherDtorD2(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
}

/* 0x80c438 (name ours) */
void pmSnvsCipherDtorD0(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
	operatorDelete(c);
}

/* 0x80c442 (name ours) */
void pmSnvsCipherCtorC2(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
	c->dataKey = NULL;
	c->tweakKey = NULL;
	c->cmacKey = NULL;
	c->dataKeyBits = 0u;
	c->cmacKeyBits = 0u;
}

/* 0x80c460 (name ours) */
void pmSnvsCipherCtorC1(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
	c->dataKey = NULL;
	c->tweakKey = NULL;
	c->cmacKey = NULL;
	c->dataKeyBits = 0u;
	c->cmacKeyBits = 0u;
}

/* 0x80c47e (name ours) */
void pmSnvsCipherDtorD1(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
}

/* 0x80c486 (name ours) */
int pmSnvsCipherInit(PmSnvsCipher *c, const uint8_t *dataKey,
		const uint8_t *tweakKey, const uint8_t *cmacKey,
		uint32_t dataKeyBits, uint32_t cmacKeyBits)
{
	if (dataKey == NULL || tweakKey == NULL || cmacKey == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if (dataKeyBits != 128u && dataKeyBits != 256u)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if (cmacKeyBits != 128u && cmacKeyBits != 256u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	c->dataKey = dataKey;
	c->tweakKey = tweakKey;
	c->cmacKey = cmacKey;
	c->dataKeyBits = dataKeyBits;
	c->cmacKeyBits = cmacKeyBits;
	return 0;
}

/* 0x80c4d8 (name ours) */
int pmSnvsSignRecordCmac(const PmSnvsCipher *c, uint8_t *inOutRec32)
{
	uint8_t mac[16];
	uint8_t msg[64];
	uint32_t bmCtrl[2];
	int status;

	if (inOutRec32 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(mac, 0, sizeof(mac));
	memset(msg, 0, sizeof(msg));
	memcpy(msg, inOutRec32, 32u);
	memset(msg + 16u, 0, 16u);

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(mac, msg, 64u, c->cmacKey,
			c->cmacKeyBits >> 3, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0)
			memcpy(inOutRec32 + 16u, mac, 16u);
	}
	return status;
}

/* 0x80c588 (name ours) */
int pmFormatSectorTweak(const PmSnvsCipher *c, uint32_t sectorIdx,
		uint64_t *outTweak16)
{
	uint64_t val;

	(void)c;
	if (outTweak16 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(outTweak16, 0, 16u);
	val = (uint64_t)sectorIdx << 1;
	le64tohSm(outTweak16, &val);
	return 0;
}

/* 0x80c5d4 (name ours) */
int pmSnvsDecryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector)
{
	uint64_t tweak[2];
	int status;

	if (c->dataKey == NULL || c->tweakKey == NULL ||
	    dst == NULL || src == NULL || (totalBytes & 0x0fu) != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	status = pmFormatSectorTweak(c, startSector, tweak);
	if (status == 0) {
		if (pmXtsDecryptMultiSectors((const uint8_t *)(const void *)tweak,
				c->dataKey, c->tweakKey, c->dataKeyBits,
				totalBytes, 16u,
				(const uint8_t *)src, (uint8_t *)dst) != 0) {
			status = SCE_SBL_ERROR_UPD_VERIFY_FAILED;
		}
	}
	return status;
}

/* 0x80c668 (name ours) */
void pmSnvsCipherNoop1(void)
{
}

/* 0x80c66a (name ours) */
int pmXtsDecryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst)
{
	uint32_t bmCtrl[2];
	uint8_t tweak[16];
	uint8_t prevTweak[16];
	uint8_t scratch[16];
	uint32_t offset = 0u;
	uint32_t i;

	if (sectorBytes < 16u)
		return -1;

	bigmacInitChannel0(bmCtrl);
	if (keyBits == 128u) {
		s_pmXtsEcbEncryptFn = bigmacStartAes128EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes128EcbDecrypt;
	} else if (keyBits == 256u) {
		s_pmXtsEcbEncryptFn = bigmacStartAes256EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes256EcbDecrypt;
	} else {
		return -1;
	}

	if (s_pmXtsEcbEncryptFn(tweak, tweakIv, 16u, tweakKey, bmCtrl) != 0)
		return -1;
	bigmacWaitChannelDone(bmCtrl);

	while (offset + 16u <= sectorBytes) {
		uint8_t carry = 0u;

		for (i = 0u; i < 16u; i++)
			dst[offset + i] = src[offset + i] ^ tweak[i];

		if (s_pmXtsEcbSecondFn(dst + offset, dst + offset,
				16u, dataKey, bmCtrl) != 0) {
			return -1;
		}
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset + i] ^= tweak[i];

		for (i = 0u; i < 16u; i++) {
			uint8_t nextCarry = tweak[i] >> 7;

			tweak[i] = (uint8_t)((tweak[i] << 1) + carry);
			carry = nextCarry;
		}
		if (carry != 0u)
			tweak[0] ^= 0x87u;

		offset += 16u;
	}

	if (offset < sectorBytes) {
		uint32_t rem = sectorBytes - offset;
		uint8_t carry = 0u;

		memcpy(prevTweak, tweak, 16u);
		if ((tweak[0] & 1u) != 0u) {
			tweak[0] ^= 0x87u;
			carry = 1u;
		}
		for (i = 16u; i > 0u; i--) {
			uint8_t b = prevTweak[i - 1u];
			uint8_t nextCarry = b & 1u;

			prevTweak[i - 1u] = (uint8_t)((carry << 7) | (b >> 1));
			carry = nextCarry;
		}

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = src[offset - 16u + i] ^ tweak[i];

		if (s_pmXtsEcbSecondFn(dst + offset - 16u, dst + offset - 16u,
				16u, dataKey, bmCtrl) != 0) {
			return -1;
		}
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] ^= tweak[i];

		for (i = 0u; i < rem; i++) {
			scratch[i] = src[offset + i] ^ prevTweak[i];
			dst[offset + i] = dst[offset - 16u + i];
		}
		for (; i < 16u; i++)
			scratch[i] = dst[offset - 16u + i] ^ prevTweak[i];

		if (s_pmXtsEcbSecondFn(scratch, scratch, 16u, dataKey, bmCtrl) != 0)
			return -1;
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = scratch[i] ^ prevTweak[i];
	}

	return 0;
}

/* 0x80c8f4 (name ours) */
int pmIncrementTweak128(uint8_t *tweak, uint32_t len)
{
	uint32_t i;

	for (i = 0u; i < len; i++) {
		if (tweak[i] != 0xffu) {
			tweak[i]++;
			break;
		}
		tweak[i] = 0u;
	}
	return 0;
}

/* 0x80c918 (name ours) */
int pmXtsDecryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst)
{
	uint8_t curTweak[16];
	uint32_t sectorBytes = wordsPerSector << 4;
	uint32_t numSectors = (totalBytes / sectorBytes) +
			      ((totalBytes % sectorBytes) != 0u ? 1u : 0u);
	uint32_t remBytes = totalBytes;
	uint32_t offset = 0u;
	uint32_t i;

	memcpy(curTweak, tweakIv, 16u);
	for (i = 0u; i < numSectors; i++) {
		uint32_t chunk = (i == numSectors - 1u) ? remBytes : sectorBytes;

		pmXtsDecryptSector(tweakKey, dataKey, curTweak, keyBits,
				chunk, src + offset, dst + offset);
		offset += chunk;
		pmIncrementTweak128(curTweak, 16u);
		remBytes -= sectorBytes;
	}
	return 0;
}

/* 0x80c9de (name ours) */
void pmReadHardwareKey2220(uint32_t *outWords8)
{
	outWords8[0] = REG32(0xe0062220u);
	outWords8[1] = REG32(0xe0062224u);
	outWords8[2] = REG32(0xe0062228u);
	outWords8[3] = REG32(0xe006222cu);
	outWords8[4] = REG32(0xe0062230u);
	outWords8[5] = REG32(0xe0062234u);
	outWords8[6] = REG32(0xe0062238u);
	outWords8[7] = REG32(0xe006223cu);
}

/* 0x80ca4e (name ours) */
int pmEncryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount)
{
	if (pmIsSnvsInitialized() == 0u || src == NULL || dst == NULL ||
	    pmValidateSectorRange(startSector, sectorCount) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}
	return pmSnvsEncryptSectors(pmGetSnvsCipher(), src, dst,
			sectorCount * 32u, startSector);
}

/* 0x80caac (name ours) */
int pmWriteSnvsSector0(const void *inWord0, const void *inWord1,
		void *outSector32)
{
	uint8_t rec[32];
	uint8_t enc[32];
	uint32_t i;
	int status;

	if (pmIsSnvsInitialized() == 0u || inWord0 == NULL ||
	    inWord1 == NULL || outSector32 == NULL) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memset(rec, 0, sizeof(rec));
	memcpy(rec, s_pmInvMagicSnvs, 4u);
	for (i = 0u; i < 4u; i++)
		rec[i] = (uint8_t)~rec[i];
	rec[4] = 1u;
	memset(rec + 5u, 0, 3u);
	memcpy(rec + 8u, inWord0, 4u);
	memcpy(rec + 12u, inWord1, 4u);

	status = pmSnvsSignRecordCmac(pmGetSnvsCipher(), rec);
	if (status != 0)
		return status;

	memset(enc, 0, sizeof(enc));
	status = pmSnvsEncryptSectors(pmGetSnvsCipher(), rec, enc, 32u, 0u);
	if (status == 0)
		memcpy(outSector32, enc, 32u);
	return status;
}

/* 0x80cb8a (name ours) */
int pmWrapSnvsSectorReadResponse(uint32_t counter, uint32_t sectorIdx,
		const void *sectorData32, uint32_t sectorSize,
		uint8_t *outPacket48)
{
	uint32_t hwKey[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint32_t byteOff = sectorIdx * 32u;
	uint16_t sum = 0u;
	uint32_t i;
	int status;

	if (sectorSize != 32u || outPacket48 == NULL || byteOff > 992u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(outPacket48, 0, 48u);
	outPacket48[0] = (uint8_t)byteOff;
	outPacket48[1] = (uint8_t)(byteOff >> 8);
	outPacket48[2] = (uint8_t)sectorSize;
	outPacket48[3] = 0u;
	outPacket48[4] = (uint8_t)counter;
	outPacket48[5] = (uint8_t)(counter >> 8);
	outPacket48[6] = (uint8_t)(counter >> 16);
	outPacket48[7] = (uint8_t)(counter >> 24);
	memcpy(outPacket48 + 14u, sectorData32, 32u);

	for (i = 0u; i < 45u; i++)
		sum = (uint16_t)(sum + outPacket48[i]);
	sum = (uint16_t)~sum;
	outPacket48[46] = (uint8_t)sum;
	outPacket48[47] = (uint8_t)(sum >> 8);

	pmReadHardwareKey2220(hwKey);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, pmGetZeroIv16(), 16u);
	status = bigmacStartAes128CbcEncrypt(outPacket48, outPacket48,
			48u, hwKey, iv, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80cc88 (name ours) */
int pmVerifySnvsCommitPacket(uint32_t *inOutCounter, uint32_t *outSectorIdx,
		uint8_t *inOutPacket16)
{
	uint32_t hwKey[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint16_t sum = 0u;
	uint16_t pktSum;
	uint32_t pktCounter;
	uint16_t pktByteOff;
	uint32_t i;
	int status;

	if (inOutCounter == NULL || outSectorIdx == NULL || inOutPacket16 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	pmReadHardwareKey2220(hwKey);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, pmGetZeroIv16(), 16u);
	status = bigmacStartAes128CbcDecrypt(inOutPacket16, inOutPacket16,
			16u, hwKey, iv, bmCtrl);
	if (status != 0)
		return status;
	status = bigmacWaitChannelDone(bmCtrl);
	if (status != 0)
		return status;

	for (i = 0u; i < 13u; i++)
		sum = (uint16_t)(sum + inOutPacket16[i]);
	sum = (uint16_t)~sum;
	pktSum = (uint16_t)inOutPacket16[14] | ((uint16_t)inOutPacket16[15] << 8);
	if (pktSum != sum)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	pktCounter = (uint32_t)inOutPacket16[4] |
		     ((uint32_t)inOutPacket16[5] << 8) |
		     ((uint32_t)inOutPacket16[6] << 16) |
		     ((uint32_t)inOutPacket16[7] << 24);
	if (pktCounter != *inOutCounter)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	*inOutCounter = pktCounter;
	pktByteOff = (uint16_t)inOutPacket16[0] | ((uint16_t)inOutPacket16[1] << 8);
	*outSectorIdx = (uint32_t)(pktByteOff >> 5);
	return status;
}

/* 0x80cd72 (name ours) */
int pmSnvsEncryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector)
{
	uint64_t tweak[2];
	int status;

	if (c->dataKey == NULL || c->tweakKey == NULL ||
	    src == NULL || dst == NULL || (totalBytes & 0x0fu) != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	status = pmFormatSectorTweak(c, startSector, tweak);
	if (status == 0) {
		if (pmXtsEncryptMultiSectors((const uint8_t *)(const void *)tweak,
				c->dataKey, c->tweakKey, c->dataKeyBits,
				totalBytes, 16u,
				(const uint8_t *)src, (uint8_t *)dst) != 0) {
			status = SCE_SBL_ERROR_UPD_VERIFY_FAILED;
		}
	}
	return status;
}

/* 0x80ce08 (name ours) */
int pmXtsEncryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst)
{
	uint32_t bmCtrl[2];
	uint8_t tweak[16];
	uint8_t scratch[16];
	uint32_t offset = 0u;
	uint32_t i;

	if (sectorBytes < 16u)
		return -1;

	bigmacInitChannel0(bmCtrl);
	if (keyBits == 128u) {
		s_pmXtsEcbEncryptFn = bigmacStartAes128EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes128EcbDecrypt;
	} else if (keyBits == 256u) {
		s_pmXtsEcbEncryptFn = bigmacStartAes256EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes256EcbDecrypt;
	} else {
		return -1;
	}

	if (s_pmXtsEcbEncryptFn(tweak, tweakIv, 16u, tweakKey, bmCtrl) != 0)
		return -1;
	bigmacWaitChannelDone(bmCtrl);

	while (offset + 16u <= sectorBytes) {
		uint8_t carry = 0u;

		for (i = 0u; i < 16u; i++)
			dst[offset + i] = src[offset + i] ^ tweak[i];

		if (s_pmXtsEcbEncryptFn(dst + offset, dst + offset,
				16u, dataKey, bmCtrl) != 0) {
			return -1;
		}
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset + i] ^= tweak[i];

		for (i = 0u; i < 16u; i++) {
			uint8_t nextCarry = tweak[i] >> 7;

			tweak[i] = (uint8_t)((tweak[i] << 1) + carry);
			carry = nextCarry;
		}
		if (carry != 0u)
			tweak[0] ^= 0x87u;

		offset += 16u;
	}

	if (offset < sectorBytes) {
		uint32_t rem = sectorBytes - offset;

		for (i = 0u; i < rem; i++) {
			scratch[i] = src[offset + i] ^ tweak[i];
			dst[offset + i] = dst[offset - 16u + i];
		}
		for (; i < 16u; i++)
			scratch[i] = dst[offset - 16u + i] ^ tweak[i];

		if (s_pmXtsEcbEncryptFn(scratch, scratch, 16u, dataKey, bmCtrl) != 0)
			return -1;
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = scratch[i] ^ tweak[i];
	}

	return 0;
}

/* 0x80cff4 (name ours) */
int pmXtsEncryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst)
{
	uint8_t curTweak[16];
	uint32_t sectorBytes = wordsPerSector << 4;
	uint32_t numSectors = (totalBytes / sectorBytes) +
			      ((totalBytes % sectorBytes) != 0u ? 1u : 0u);
	uint32_t remBytes = totalBytes;
	uint32_t offset = 0u;
	uint32_t i;

	memcpy(curTweak, tweakIv, 16u);
	for (i = 0u; i < numSectors; i++) {
		uint32_t chunk = (i == numSectors - 1u) ? remBytes : sectorBytes;

		pmXtsEncryptSector(tweakKey, dataKey, curTweak, keyBits,
				chunk, src + offset, dst + offset);
		offset += chunk;
		pmIncrementTweak128(curTweak, 16u);
		remBytes -= sectorBytes;
	}
	return 0;
}

/* ============================================================================
 * Auth1 & Auth2 Subcmd 4 Protocol Helpers (0x80d0bc..0x80d675)
 * ============================================================================ */

/* 0x80d0bc (name ours) */
int pmConstTimeCompare(const uint8_t *a, const uint8_t *b, uint32_t len)
{
	uint32_t i;
	int res = 0;

	for (i = 0u; i < len; i++) {
		if (a[i] != b[i])
			res = -1;
	}
	return res;
}

/* 0x80d0da (name ours) */
int pmResolveKeySlot(uint32_t keyId, uint32_t variant, uint32_t *outSlot)
{
	int status = pmCheckKeyId(keyId);

	if (status == 0) {
		if (keyId == 6u && variant != 1u)
			*outSlot = 14u;
		else
			*outSlot = keyId;
	}
	return status;
}

/* 0x80d10e (name ours) */
int pmBuildAuth1InitHeader(uint8_t *out40, uint32_t keyId)
{
	int status = pmCheckKeyId(keyId);

	if (status == 0) {
		out40[0] = 48u;
		out40[1] = 0u;
		out40[2] = 0u;
		out40[3] = (uint8_t)keyId;
		memset(out40 + 4u, 0, 4u);
		memset(out40 + 8u, 0, 32u);
	}
	return status;
}

/* 0x80d15e (name ours) */
int pmBuildAuth1Challenge(uint8_t *out40, const uint8_t *in40,
		uint32_t keyId, uint32_t variant,
		const uint8_t *serverNonce8)
{
	uint32_t slot;
	int status;

	status = pmCheckKeyId(keyId);
	if (status != 0)
		return status;

	if (in40[0] != 48u || in40[1] != 1u)
		return (int)0x800f0024u;

	if (in40[2] >= 6u) {
		if (in40[2] == 16u)
			return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
		return (int)0x800f0024u;
	}
	if (in40[2] != 0u)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	if (in40[3] != keyId)
		return (int)0x800f0024u;

	out40[0] = 48u;
	out40[1] = 2u;
	out40[2] = 0u;
	out40[3] = (uint8_t)keyId;
	*(uint32_t *)(void *)(out40 + 4u) = variant;

	slot = (keyId == 6u && variant != 1u) ? 14u : keyId;
	memcpy(out40 + 8u, serverNonce8, 8u);
	memcpy(out40 + 16u, in40 + 8u, 8u);
	memcpy(out40 + 24u, s_pmKeyTable[slot] + 48u, 16u);

	if (pmAes128CbcEncrypt(out40 + 8u, out40 + 8u, 32u,
			s_pmKeyTable[slot], s_pmZeroIvAuth) != 0) {
		return SCE_SBL_ERROR_IO;
	}
	return 0;
}

/* 0x80d252 (name ours) */
int pmVerifyAuth1Response(const uint8_t *in40, uint32_t keyId,
		uint32_t variant, const uint8_t *expectedClientNonce8,
		const uint8_t *expectedServerNonce8, uint8_t *outSessionKey16)
{
	uint8_t decBuf[32];
	uint32_t slot;
	int status;

	status = pmCheckKeyId(keyId);
	if (status != 0)
		return status;

	if (in40[0] != 48u || in40[1] != 3u)
		return (int)0x800f0024u;

	if (in40[2] >= 6u) {
		if (in40[2] == 16u)
			return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
		return (int)0x800f0024u;
	}
	if (in40[2] != 0u)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	if (in40[3] != keyId)
		return (int)0x800f0024u;

	slot = (keyId == 6u && variant != 1u) ? 14u : keyId;
	if (pmAes128CbcDecrypt(decBuf, in40 + 8u, 32u,
			s_pmKeyTable[slot] + 16u, s_pmZeroIvAuth) != 0) {
		return SCE_SBL_ERROR_IO;
	}

	if (pmConstTimeCompare(decBuf, expectedClientNonce8, 8u) != 0 ||
	    pmConstTimeCompare(decBuf + 8u, expectedServerNonce8, 8u) != 0) {
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	}

	if (outSessionKey16 != NULL) {
		memcpy(outSessionKey16, expectedClientNonce8, 8u);
		memcpy(outSessionKey16 + 8u, expectedServerNonce8, 8u);
		if (pmAes128CbcEncrypt(outSessionKey16, outSessionKey16, 16u,
				s_pmKeyTable[slot] + 32u, s_pmZeroIvAuth) != 0) {
			return SCE_SBL_ERROR_IO;
		}
	}
	return 0;
}

/* 0x80d386 (name ours) */
int pmClearKeySlot(uint32_t keyId, uint32_t variant)
{
	uint32_t slot;
	int status;

	status = pmCheckKeyId(keyId);
	if (status != 0)
		return status;

	slot = (keyId == 6u && variant != 1u) ? 14u : keyId;
	if (slot > 15u)
		return (int)0x800f0022u;
	if (s_pmKeyTable[slot] == NULL)
		return (int)0x800f0002u;

	memset(s_pmKeyTable[slot], 0, 64u);
	return 0;
}

/* 0x80d3e4 (name ours) */
int pmAes128CbcEncrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv)
{
	uint8_t localIv[16];

	memcpy(localIv, iv, 16u);
	if (bigmacAes128CbcEncryptSync(dst, src, len, key, localIv) != 0)
		return SCE_SBL_ERROR_IO;
	return 0;
}

/* 0x80d446 (name ours) */
int pmAes128CbcDecrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv)
{
	uint8_t localIv[16];

	memcpy(localIv, iv, 16u);
	if (bigmacAes128CbcDecryptSync(dst, src, len, key, localIv) != 0)
		return SCE_SBL_ERROR_IO;
	return 0;
}

/* 0x80d4a8 (name ours) */
int pmGetRandomBytes(uint8_t *dst, uint32_t len)
{
	uint32_t offset = 0u;

	while (len != 0u) {
		uint32_t chunk = (len < 5u) ? len : 4u;
		uint32_t word = REG32(0xe005003cu);

		memcpy(dst + offset, &word, chunk);
		len -= chunk;
		offset += chunk;
	}
	return 0;
}

/* 0x80d500 (name ours) */
int pmBuildSubcmd4Challenge(uint8_t *out40, uint32_t variant,
		const uint8_t *sessionKey16)
{
	uint32_t slot = 0xffffffffu;

	out40[0] = 48u;
	out40[1] = 4u;
	out40[2] = 0u;
	out40[3] = 15u;
	if (pmResolveKeySlot(15u, variant, &slot) != 0 || slot != 15u)
		return SCE_SBL_ERROR_UPD_KEY_NOT_FOUND;

	memset(out40 + 4u, 0, 4u);
	memcpy(out40 + 8u, s_pmSubcmd4Magic, 16u);
	memset(out40 + 24u, 0, 16u);
	if (pmAes128CbcEncrypt(out40 + 8u, out40 + 8u, 32u,
			sessionKey16, s_pmZeroIvSubcmd) != 0) {
		return SCE_SBL_ERROR_IO;
	}
	return 0;
}

/* 0x80d59a (name ours) */
int pmVerifySubcmd4Response(const uint8_t *in40, uint32_t variant,
		const uint8_t *sessionKey16)
{
	uint8_t decBuf[32];
	uint32_t slot = 0xffffffffu;

	if (in40[0] != 48u || in40[1] != 5u)
		return (int)0x800f0024u;

	if (in40[2] >= 6u) {
		if (in40[2] == 16u)
			return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
		return (int)0x800f0024u;
	}
	if (in40[2] != 0u)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	if (in40[3] != 15u)
		return (int)0x800f0024u;

	if (pmResolveKeySlot(15u, variant, &slot) != 0 || slot != 15u)
		return SCE_SBL_ERROR_UPD_KEY_NOT_FOUND;

	if (pmAes128CbcDecrypt(decBuf, in40 + 8u, 32u,
			sessionKey16, s_pmZeroIvSubcmd) != 0) {
		return SCE_SBL_ERROR_IO;
	}

	if (pmConstTimeCompare(decBuf, s_pmSubcmd4Magic, 16u) != 0)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;

	return 0;
}

/* ============================================================================
 * UpdListVerifier Class Methods (0x80d676..0x80dd77)
 * ============================================================================ */

/* 0x80d676 (name ours) */
int updListValidateArmRange(uint32_t addr, uint32_t size)
{
	return spkgValidateArmRange(addr, size);
}

/* 0x80d74a (name ours) */
void updListVerifierDtorD2(UpdListVerifier *v)
{
	v->vptr = &s_updListVerifierVtable[2];
	if (v->entryIds != NULL) {
		smHeapFree(v->entryIds);
		v->entryIds = NULL;
	}
	if (v->entryHashes != NULL) {
		smHeapFree(v->entryHashes);
		v->entryHashes = NULL;
	}
}

/* 0x80d782 (name ours) */
void updListVerifierDtorD0(UpdListVerifier *v)
{
	updListVerifierDtorD2(v);
	operatorDelete(v);
}

/* 0x80d7c0 (name ours) */
void updListVerifierCtorC2(UpdListVerifier *v)
{
	v->vptr = &s_updListVerifierVtable[2];
	v->keyIdx = 0u;
	v->keyType = 0u;
	v->flags = 0u;
	v->entryIds = NULL;
	v->entryHashes = NULL;
	memset(v->hmacKey, 0, sizeof(v->hmacKey));
	memset(&v->header, 0, sizeof(v->header));
	memset(v->headerHmac, 0, sizeof(v->headerHmac));
}

/* 0x80d812 (name ours) */
void updListVerifierCtorC1(UpdListVerifier *v)
{
	updListVerifierCtorC2(v);
}

/* 0x80d864 (name ours) */
void updListVerifierDtorD1(UpdListVerifier *v)
{
	updListVerifierDtorD2(v);
}

/* 0x80d89c (name ours) */
int updListSelectHmacKeyWithFlags(UpdListVerifier *v, uint32_t keyType,
		uint32_t keyIdx, uint32_t flags)
{
	if (keyType != 2u || keyIdx - 1u >= 10u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	v->keyType = keyType;
	v->keyIdx = keyIdx;
	v->flags = flags;
	memcpy(v->hmacKey, s_updListHmacKeyTable[keyIdx], 64u);
	return 0;
}

/* 0x80d8e0 (name ours) */
int updListSetFlags(UpdListVerifier *v, uint32_t flags)
{
	v->flags = flags;
	return 0;
}

/* 0x80d8e8 (name ours) */
int updListSelectHmacKey(UpdListVerifier *v, uint32_t keyType,
		uint32_t keyIdx)
{
	if (keyType != 2u || keyIdx - 1u >= 10u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	v->keyType = keyType;
	v->keyIdx = keyIdx;
	memcpy(v->hmacKey, s_updListHmacKeyTable[keyIdx], 64u);
	return 0;
}

/* 0x80d928 (name ours) */
int updListCheckMagic(UpdListVerifier *v, const UpdSlb2Header *hdr)
{
	(void)v;
	return (timingsafeBcmp(hdr->magic, s_updSlb2Magic, 7u) == 0);
}

/* 0x80d942 (name ours) */
int updListCheckHeaderMagicAndBit(UpdListVerifier *v, const UpdSlb2Header *hdr)
{
	if (updListCheckMagic(v, hdr) == 0)
		return 0;
	return (hdr->magic[7] & 1u);
}

/* 0x80d964 (name ours) */
int updListGetHeaderBit(UpdListVerifier *v, const UpdSlb2Header *hdr)
{
	(void)v;
	return (hdr->magic[7] & 1u);
}

/* 0x80d96e (name ours) */
int updListIsKeyType2(UpdListVerifier *v, uint32_t keyType)
{
	(void)v;
	return (keyType == 2u);
}

/* 0x80d974 (name ours) */
int updListIsKeyIdxValid(UpdListVerifier *v, uint32_t keyIdx)
{
	(void)v;
	return (keyIdx - 1u < 10u);
}

/* 0x80d97a (name ours) */
const uint8_t *updListFindEntryHash(const UpdListVerifier *v,
		const uint64_t *entryId)
{
	uint64_t target = *entryId;
	uint64_t count;
	uint64_t i;

	if (v->entryIds == NULL || v->entryHashes == NULL)
		return NULL;

	count = (uint64_t)v->header.entryCountLow |
		((uint64_t)v->header.entryCountHigh << 32);
	for (i = 0ULL; i < count; i++) {
		const uint64_t *curId =
			(const uint64_t *)(const void *)(v->entryIds + (uint32_t)i * 32u);

		if (*curId == target)
			return v->entryHashes + (uint32_t)i * 64u + 8u;
	}
	return NULL;
}

/* 0x80d9e2 (name ours) */
int updListReadScatterBytes(UpdListVerifier *v, void *dst,
		SpkgVerifyRequest *req, uint32_t offset, uint32_t size)
{
	(void)v;
	if (dst == NULL || req == NULL || size == 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	return spkgReadPlainHeaderBytes(dst, req, offset, size);
}

/* 0x80da06 (name ours) */
int updListLoadHeaderAndTables(UpdListVerifier *v, SpkgVerifyRequest *req)
{
	uint32_t idsBytes;
	uint32_t hashesBytes;
	int status;

	if (req == NULL)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	status = updListReadScatterBytes(v, &v->header, req, 0u,
			sizeof(v->header));
	if (status != 0)
		return status;

	if (updListCheckHeaderMagicAndBit(v, &v->header) == 0)
		return SCE_SBL_ERROR_UPD_BAD_HEADER;

	if (v->header.entryCountHigh != 0u || v->header.entryCountLow > 512u)
		return SCE_SBL_ERROR_UPD_BAD_HEADER;

	idsBytes = v->header.entryCountLow * 32u;
	if (v->entryIds != NULL) {
		smHeapFree(v->entryIds);
		v->entryIds = NULL;
	}
	v->entryIds = (uint8_t *)smHeapMalloc(idsBytes);
	if (v->entryIds == NULL)
		return SCE_SBL_ERROR_UPD_NO_MEMORY;

	status = updListReadScatterBytes(v, v->entryIds, req,
			sizeof(v->header), idsBytes);
	if (status != 0)
		return status;

	hashesBytes = v->header.entryCountLow * 64u;
	if (v->entryHashes != NULL) {
		smHeapFree(v->entryHashes);
		v->entryHashes = NULL;
	}
	v->entryHashes = (uint8_t *)smHeapMalloc(hashesBytes);
	if (v->entryHashes == NULL)
		return SCE_SBL_ERROR_UPD_NO_MEMORY;

	status = updListReadScatterBytes(v, v->entryHashes, req,
			sizeof(v->header) + idsBytes, hashesBytes);
	if (status != 0)
		return status;

	return updListReadScatterBytes(v, v->headerHmac, req,
			sizeof(v->header) + idsBytes + hashesBytes,
			sizeof(v->headerHmac));
}

/* 0x80db08 (name ours) */
int updListVerifyScatterHmac(CfHmacContext *hmacCtx,
		const SpkgVerifyRequest *req, const uint8_t *hmacKey,
		uint32_t keyLen, const uint8_t *expectedHmac,
		uint32_t skipTailBytes)
{
	uint8_t digest[32];
	uint32_t totalEntries;
	uint32_t processedEntries = 0u;

	if (req == NULL)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	memset(digest, 0, sizeof(digest));
	totalEntries = req->inlineEntryCount;
	if (totalEntries == 0u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	if (bigmacHmacSha256Init(hmacCtx, hmacKey, keyLen) != 0)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	if (req->listCount > 481u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	if (req->mode == 1ULL) {
		uint32_t listIdx;

		for (listIdx = 0u; listIdx < req->listCount; listIdx++) {
			const SpkgScatterListDesc *desc = &req->u.lists[listIdx];
			uint32_t count = desc->byteSize >> 3;
			uint32_t i;

			if (updListValidateArmRange(
					(uint32_t)(uintptr_t)desc->entries,
					desc->byteSize) == 0) {
				return SCE_SBL_ERROR_UPD_INVALID_VALUE;
			}
			for (i = 0u; i < count; i++) {
				uint32_t paddr = desc->entries[i].paddr;
				uint32_t size = desc->entries[i].size;
				uint32_t isLast;

				if (paddr == 0u)
					continue;
				if (updListValidateArmRange(paddr, size) == 0)
					return SCE_SBL_ERROR_UPD_INVALID_VALUE;

				processedEntries++;
				if (processedEntries == totalEntries) {
					if (size < skipTailBytes)
						return SCE_SBL_ERROR_UPD_INVALID_VALUE;
					size -= skipTailBytes;
				}
				isLast = (processedEntries == totalEntries) ? 1u : 0u;
				if (bigmacSha256Update(hmacCtx,
						(const void *)(uintptr_t)paddr,
						size, isLast) != 0) {
					return SCE_SBL_ERROR_UPD_BAD_DIGEST;
				}
				if (processedEntries == totalEntries)
					break;
			}
		}
	} else {
		uint32_t i;

		for (i = 0u; i < totalEntries; i++) {
			uint32_t paddr = req->u.inlineEntries[i].paddr;
			uint32_t size = req->u.inlineEntries[i].size;
			uint32_t isLast;

			if (updListValidateArmRange(paddr, size) == 0)
				return SCE_SBL_ERROR_UPD_INVALID_VALUE;

			processedEntries++;
			if (processedEntries == totalEntries) {
				if (size < skipTailBytes)
					return SCE_SBL_ERROR_UPD_INVALID_VALUE;
				size -= skipTailBytes;
			}
			isLast = (processedEntries == totalEntries) ? 1u : 0u;
			if (bigmacSha256Update(hmacCtx,
					(const void *)(uintptr_t)paddr,
					size, isLast) != 0) {
				return SCE_SBL_ERROR_UPD_BAD_DIGEST;
			}
		}
	}

	if (bigmacSha256Final(digest, hmacCtx) != 0)
		return SCE_SBL_ERROR_UPD_BAD_DIGEST;

	if (timingsafeBcmp(digest, expectedHmac, 32u) != 0)
		return SCE_SBL_ERROR_UPD_BAD_DIGEST;

	return 0;
}

/* 0x80dca6 (name ours) */
int updListVerifyEntry(UpdListVerifier *v, const SpkgVerifyRequest *req)
{
	uint64_t entryId;
	const uint8_t *expectedHmac;

	if (req == NULL)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;
	if (v->entryIds == NULL || v->entryHashes == NULL)
		return SCE_SBL_ERROR_UPD_NOT_READY;

	entryId = (uint64_t)req->rsvd00[0] | ((uint64_t)req->rsvd00[1] << 32);
	expectedHmac = updListFindEntryHash(v, &entryId);
	if (expectedHmac == NULL)
		return SCE_SBL_ERROR_UPD_NOT_READY;

	if (updListVerifyScatterHmac(&v->hmacCtx, req, v->hmacKey,
			sizeof(v->hmacKey), expectedHmac, 0u) != 0) {
		return SCE_SBL_ERROR_UPD_BAD_DIGEST;
	}
	return 0;
}

/* 0x80dd16 (name ours) */
int updListVerifyPackage(UpdListVerifier *v, SpkgVerifyRequest *req)
{
	uint8_t cid[16];
	int status;

	aimgrClearConsoleId2(cid);
	if (req == NULL) {
		status = SCE_SBL_ERROR_UPD_INVALID_VALUE;
		goto done;
	}

	status = updListLoadHeaderAndTables(v, req);
	if (status != 0)
		goto done;

	status = updListSelectHmacKey(v, v->header.keyType, v->header.keyIdx);
	if (status != 0)
		goto done;

	status = updListVerifyScatterHmac(&v->hmacCtx, req, v->hmacKey,
			sizeof(v->hmacKey), v->headerHmac, 32u);

done:
	aimgrClearConsoleId(cid);
	return status;
}

/* ============================================================================
 * UpdEncPackageVerifier Class Methods (0x80dd78..0x80e747)
 * ============================================================================ */

/* 0x80dd78 (name ours) */
int updEncIsValidateArmRange(uint32_t addr, uint32_t size)
{
	return spkgValidateArmRange(addr, size);
}

/* 0x80de4c (name ours) */
void updEncIsCtorC2(UpdEncPackageVerifier *v)
{
	updEncBaseCtorC2(v);
}

/* 0x80de50 (name ours) */
void updEncIsCtorC1(UpdEncPackageVerifier *v)
{
	updEncBaseCtorC2(v);
}

/* 0x80de54 (name ours) */
void updEncIsDtorD2(UpdEncPackageVerifier *v)
{
	updEncBaseDtorD2(v);
}

/* 0x80de58 (name ours) */
void updEncIsDtorD1(UpdEncPackageVerifier *v)
{
	updEncBaseDtorD2(v);
}

/* 0x80de5c (name ours) */
int updEncIsNotSupported(void)
{
	return SCE_SBL_ERROR_NOT_SUPPORTED;
}

/* 0x80de66 (name ours) */
int updVerifyEncIsPackage(UpdEncPackageVerifier *v, const void *nonceBuf,
		uint32_t nonceLen, const uint8_t *pkgBuf, void *outDecrypted)
{
	uint32_t bmCtrl[2];
	uint8_t hashBuf[32];
#if FW_VERSION < 0x04000000
	uint8_t s_updEncIsWorkBuf[0x1000];
#endif

	bigmacInitChannel0(bmCtrl);
	if (nonceBuf == NULL || nonceLen != 32u || pkgBuf == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (v->hasRsaPubKey == 0u || v->hasAesKeyIv == 0u)
		return SCE_SBL_ERROR_NOT_INITIALIZED;

	memcpy(s_updEncIsWorkBuf, pkgBuf, 32u);
	if (bigmacStartAes128CbcDecrypt(s_updEncIsWorkBuf + 32u,
			pkgBuf + 32u, 4064u, v->aesKey, v->aesIv, bmCtrl) != 0) {
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	}
	bigmacWaitChannelDone(bmCtrl);

	if (timingsafeBcmp(s_updEncIsWorkBuf, s_updEncIsMagic, 7u) != 0 ||
	    s_updEncIsWorkBuf[7] != 1u ||
	    *(const uint32_t *)(const void *)(s_updEncIsWorkBuf + 8u) != 1u) {
		return (int)0x800f0024u;
	}

	if (bigmacStartSha256(hashBuf, nonceBuf, 32u, bmCtrl) != 0)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	bigmacWaitChannelDone(bmCtrl);

	if (updEncVerifyRsa2048Sig(v, hashBuf, 32u,
			s_updEncIsWorkBuf + 32u, 256u) != 0) {
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	}

	if (bigmacStartSha256(hashBuf, s_updEncIsWorkBuf, 3840u, bmCtrl) != 0)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	bigmacWaitChannelDone(bmCtrl);

	if (updEncVerifyRsa2048Sig(v, hashBuf, 32u,
			s_updEncIsWorkBuf + 3840u, 256u) != 0) {
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	}

	if (outDecrypted != NULL)
		memcpy(outDecrypted, s_updEncIsWorkBuf, 4096u);

	return 0;
}

/* 0x80dfc8 (name ours) */
int updEncIsCopyFromScatter(UpdEncPackageVerifier *v, void *dst,
		const UpdEncScatterRequest *req, uint32_t offset, uint32_t size)
{
	uint8_t *dstBytes = (uint8_t *)dst;
	uint32_t remSize = size;
	uint32_t curOffset = 0u;

	(void)v;
	if (dst == NULL || req == NULL || size == 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (req->listCount > 494u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	if (req->modeLow == 1u && req->modeHigh == 0u) {
		uint32_t listIdx;

		for (listIdx = 0u; listIdx < req->listCount; listIdx++) {
			const SpkgScatterListDesc *desc = &req->u.lists[listIdx];
			uint32_t count = desc->byteSize >> 3;
			uint32_t i;

			if (updEncIsValidateArmRange(
					(uint32_t)(uintptr_t)desc->entries,
					desc->byteSize) == 0) {
				return SCE_SBL_ERROR_UPD_INVALID_VALUE;
			}
			for (i = 0u; i < count; i++) {
				uint32_t paddr = desc->entries[i].paddr;
				uint32_t entrySize = desc->entries[i].size;

				if (updEncIsValidateArmRange(paddr, entrySize) == 0)
					return SCE_SBL_ERROR_UPD_INVALID_VALUE;

				if (offset >= curOffset) {
					if (curOffset + entrySize >= offset) {
						uint32_t inOff = offset - curOffset;
						uint32_t chunk = entrySize - inOff;

						if (chunk > remSize)
							chunk = remSize;
						memcpy(dstBytes,
							(const void *)(uintptr_t)(paddr + inOff),
							chunk);
						dstBytes += chunk;
						remSize -= chunk;
					}
				} else if (remSize != 0u) {
					uint32_t chunk = (entrySize < remSize) ?
						entrySize : remSize;

					memcpy(dstBytes,
						(const void *)(uintptr_t)paddr,
						chunk);
					dstBytes += chunk;
					remSize -= chunk;
				}
				if (remSize == 0u)
					return 0;
				curOffset += entrySize;
			}
		}
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	{
		uint32_t i;

		for (i = 0u; i < req->inlineEntryCount; i++) {
			uint32_t paddr = req->u.inlineEntries[i].paddr;
			uint32_t entrySize = req->u.inlineEntries[i].size;

			if (updEncIsValidateArmRange(paddr, entrySize) == 0)
				return SCE_SBL_ERROR_INVALID_VALUE;

			if (offset >= curOffset) {
				if (curOffset + entrySize >= offset) {
					uint32_t inOff = offset - curOffset;
					uint32_t chunk = entrySize - inOff;

					if (chunk > remSize)
						chunk = remSize;
					memcpy(dstBytes,
						(const void *)(uintptr_t)(paddr + inOff),
						chunk);
					dstBytes += chunk;
					remSize -= chunk;
				}
			} else if (remSize != 0u) {
				uint32_t chunk = (entrySize < remSize) ?
					entrySize : remSize;

				memcpy(dstBytes,
					(const void *)(uintptr_t)paddr,
					chunk);
				dstBytes += chunk;
				remSize -= chunk;
			}
			if (remSize == 0u)
				return 0;
			curOffset += entrySize;
		}
	}
	return SCE_SBL_ERROR_INVALID_VALUE;
}

/* 0x80e178 (name ours) */
int updEncIdValidateArmRange(uint32_t addr, uint32_t size)
{
	return spkgValidateArmRange(addr, size);
}

/* 0x80e24c (name ours) */
int updEncIdWaitChannelDone(const uint32_t *ctrl)
{
	uint32_t status = 0u;

	do {
		bigmacGetChannelStatus(ctrl, &status);
	} while ((status & 1u) != 0u);

	return (status == 0u) ? 0 : -1;
}

/* 0x80e278 (name ours) */
void updEncIdCtorC2(UpdEncPackageVerifier *v)
{
	updEncBaseCtorC2(v);
}

/* 0x80e27c (name ours) */
void updEncIdCtorC1(UpdEncPackageVerifier *v)
{
	updEncBaseCtorC2(v);
}

/* 0x80e280 (name ours) */
void updEncIdDtorD2(UpdEncPackageVerifier *v)
{
	updEncBaseDtorD2(v);
}

/* 0x80e284 (name ours) */
void updEncIdDtorD1(UpdEncPackageVerifier *v)
{
	updEncBaseDtorD2(v);
}

/* 0x80e288 (name ours) */
int updEncIdNotSupported(void)
{
	return SCE_SBL_ERROR_NOT_SUPPORTED;
}

/* 0x80e292 (name ours) */
int updVerifyEncIdPackage(UpdEncPackageVerifier *v, const void *nonceBuf,
		uint32_t nonceLen, const uint8_t *pkgBuf, void *outDecrypted)
{
	uint32_t bmCtrl[2];
	uint8_t workBuf[0x400];
	uint8_t hashBuf[32];

	bigmacInitChannel0(bmCtrl);
	if (nonceBuf == NULL || nonceLen != 32u || pkgBuf == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (v->hasRsaPubKey == 0u || v->hasAesKeyIv == 0u)
		return SCE_SBL_ERROR_NOT_INITIALIZED;

	memcpy(workBuf, pkgBuf, 32u);
	if (bigmacStartAes128CbcDecrypt(workBuf + 32u,
			pkgBuf + 32u, 992u, v->aesKey, v->aesIv, bmCtrl) != 0) {
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	}
	updEncIdWaitChannelDone(bmCtrl);

	if (timingsafeBcmp(workBuf, s_updEncIdMagic, 7u) != 0 ||
	    workBuf[7] != 1u ||
	    *(const uint32_t *)(const void *)(workBuf + 8u) != 1u) {
		return (int)0x800f0024u;
	}

	if (bigmacStartSha256(hashBuf, nonceBuf, 32u, bmCtrl) != 0)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	updEncIdWaitChannelDone(bmCtrl);

	if (updEncVerifyRsa2048Sig(v, hashBuf, 32u,
			workBuf + 32u, 256u) != 0) {
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	}

	if (bigmacStartSha256(hashBuf, workBuf, 288u, bmCtrl) != 0)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	updEncIdWaitChannelDone(bmCtrl);

	if (updEncVerifyRsa2048Sig(v, hashBuf, 32u,
			workBuf + 768u, 256u) != 0) {
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	}

	if (outDecrypted != NULL)
		memcpy(outDecrypted, workBuf, 1024u);

	return 0;
}

/* 0x80e3f4 (name ours) */
int updEncIdCopyFromScatter(UpdEncPackageVerifier *v, void *dst,
		const UpdEncScatterRequest *req, uint32_t offset, uint32_t size)
{
	return updEncIsCopyFromScatter(v, dst, req, offset, size);
}

/* 0x80e5ae (name ours) */
void updEncBaseCtorC2(UpdEncPackageVerifier *v)
{
	v->hasPrivData = 0u;
	v->hasRsaPubKey = 0u;
	v->hasAesKeyIv = 0u;
}

/* 0x80e5be (name ours) */
void updEncBaseCtorC1(UpdEncPackageVerifier *v)
{
	v->hasPrivData = 0u;
	v->hasRsaPubKey = 0u;
	v->hasAesKeyIv = 0u;
}

/* 0x80e5ce (name ours) */
void updEncBaseDtorD2(UpdEncPackageVerifier *v)
{
	(void)v;
}

/* 0x80e5d0 (name ours) */
void updEncBaseDtorD1(UpdEncPackageVerifier *v)
{
	(void)v;
}

/* 0x80e5d2 (name ours) */
int updEncSetAesKeyIv(UpdEncPackageVerifier *v, const uint8_t *key16,
		const uint8_t *iv16)
{
	if (key16 == NULL || iv16 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memcpy(v->aesKey, key16, 16u);
	memcpy(v->aesIv, iv16, 16u);
	v->hasAesKeyIv = 1u;
	return 0;
}

/* 0x80e610 (name ours) */
int updEncSetPrivData(UpdEncPackageVerifier *v, const uint8_t *priv640)
{
	if (priv640 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memcpy(v->privData, priv640, 640u);
	v->hasPrivData = 1u;
	return 0;
}

/* 0x80e640 (name ours) */
int updEncSetRsaPubKey(UpdEncPackageVerifier *v, const UpdEncRsaPubKey *pubKey)
{
	if (pubKey == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memcpy(&v->rsaPubKey, pubKey, sizeof(v->rsaPubKey));
	v->hasRsaPubKey = 1u;
	return 0;
}

/* 0x80e670 (name ours) */
int updEncBaseNotSupported(void)
{
	return SCE_SBL_ERROR_NOT_SUPPORTED;
}

/* 0x80e67a (name ours) */
int updEncVerifyRsa2048Sig(UpdEncPackageVerifier *v, const uint8_t *hash32,
		uint32_t hashLen, const uint8_t *sig256, uint32_t sigLen)
{
	uint32_t bmCtrl[2];
	uint32_t bignumState;
	uint8_t workBuf[256];
	BignumPublicKey key;
	int status;

	bigmacInitChannel0(bmCtrl);
	bignumInitState(&bignumState);
	memset(workBuf, 0, sizeof(workBuf));

	if (v->hasRsaPubKey == 0u)
		return SCE_SBL_ERROR_NOT_INITIALIZED;

	if (hash32 == NULL || hashLen == 0u || sig256 == NULL || sigLen != 256u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	key.modulus = (const uint32_t *)(const void *)v->rsaPubKey.modulus;
	key.modulusWords = v->rsaPubKey.modulusBytes >> 2;
	key.exponent = (const uint32_t *)(const void *)v->rsaPubKey.exponent;
	key.exponentWords = v->rsaPubKey.exponentBytes >> 2;

	do {
		status = verifyRsaPkcs1v15Step(workBuf, sig256, &key,
				hash32, 4u, &bignumState);
		if (status != SCE_SBL_ERROR_BUSY)
			break;
		bignumWaitInterrupt();
	} while (1);

	if (status < 0)
		return SCE_SBL_ERROR_UPD_VERIFY_FAILED;
	return 0;
}

/* ============================================================================
 * SpkgVerifier Class Methods (0x80f61c..0x810309)
 * ============================================================================ */

/* 0x80f61c (name ours) */
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

/* 0x80f666 (name ours) */
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

/* 0x80f73a (name ours) */
void spkgVerifierDtorD2(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
}

/* 0x80f742 (name ours) */
void spkgVerifierDtorD0(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
	operatorDelete(v);
}

/* 0x80f74c (name ours) */
void spkgVerifierCtorC2(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
	v->req = NULL;
	v->isPrepared = 0u;
}

/* 0x80f75e (name ours) */
void spkgVerifierCtorC1(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
	v->req = NULL;
	v->isPrepared = 0u;
}

/* 0x80f770 (name ours) */
void spkgVerifierDtorD1(SpkgVerifier *v)
{
	v->vptr = &s_spkgVerifierVtable[2];
}

/* 0x80f778 (name ours) */
void spkgVerifierCleanup(SpkgVerifier *v)
{
	(void)v;
}

/* 0x80f77a (name ours) */
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

/* 0x80f7aa (name ours) */
int spkgCheckExtHeaderMagic(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	(void)v;
	if (extHdr == NULL)
		return 0;
	return (extHdr->magic == 4u);
}

/* 0x80f7b8 (name ours) */
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
			status = SCE_SBL_ERROR_UPD_VERSION_FLOOR;
	}
	aimgrClearConsoleId(cid);
	return status;
}

/* 0x80f852 (name ours) */
int spkgCheckExtHeaderField30(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	(void)v;
	if (extHdr == NULL)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	if (extHdr->pkgType == 1u || extHdr->pkgType == 9u ||
	    extHdr->pkgType == 10u || extHdr->pkgType == 21u ||
	    extHdr->pkgType == 22u) {
		if (extHdr->rsvd30[0] == 0u)
			return SCE_SBL_ERROR_UPD_VERSION_FLOOR;
	}
	return 0;
}

/* 0x80f88c (name ours) */
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
	return SCE_SBL_ERROR_UPD_OUT_OF_RANGE;
}

/* 0x80f902 (name ours) */
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
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

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
			return SCE_SBL_ERROR_UPD_INVALID_ADDR;
		}
		ent = &curDesc->entries[entryIdx];
	} else {
		if (spkgFindRangeEntry(req->u.inlineEntries,
				req->inlineEntryCount, offset,
				&entryIdx, &accumLow, &inEntryOff) != 0) {
			return SCE_SBL_ERROR_UPD_INVALID_ADDR;
		}
		ent = &req->u.inlineEntries[entryIdx];
	}

	paddr = ent->paddr;
	entrySize = ent->size;
	endAddr = paddr + entrySize;
	if (endAddr < paddr)
		return SCE_SBL_ERROR_UPD_INVALID_ADDR;

	if (paddr <= 0x0005ffffu) {
		if (endAddr > 0x00040000u)
			return SCE_SBL_ERROR_UPD_INVALID_ADDR;
	} else if (paddr <= 0x003fffffu) {
		if (endAddr > 0x00300000u)
			return SCE_SBL_ERROR_UPD_INVALID_ADDR;
	} else if (paddr <= 0x0081ffffu) {
		if (endAddr > 0x00800000u)
			return SCE_SBL_ERROR_UPD_INVALID_ADDR;
	} else if (paddr > 0xdfffffffu || endAddr > 0xdfffffffu) {
		return SCE_SBL_ERROR_UPD_INVALID_ADDR;
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

/* 0x80fb70 (name ours) */
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
		return SCE_SBL_ERROR_UPD_INVALID_ADDR;
	}

	paddr = entries[entryIdx].paddr;
	entrySize = entries[entryIdx].size;
	for (;;) {
		uint64_t avail;
		uint32_t chunk;

		if (spkgValidateArmRange(paddr, entrySize) == 0)
			return SCE_SBL_ERROR_UPD_INVALID_VALUE;

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

/* 0x80fc6a (name ours) */
int spkgReadHeaderCallback(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg)
{
	SpkgVerifyRequest *req = *(SpkgVerifyRequest **)arg;

	if (req->listCount > 481u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

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
				return SCE_SBL_ERROR_UPD_INVALID_VALUE;
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
			return SCE_SBL_ERROR_UPD_INVALID_ADDR;

		*outRead = totalRead;
		return 0;
	}

	return spkgCopyFromScatterEntries(req->u.inlineEntries,
			req->inlineEntryCount, dstAddr, offset, size, outRead);
}

/* 0x80fd70 (name ours) */
int spkgReadPlainHeaderBytes(void *dst, SpkgVerifyRequest *req,
		uint32_t offset, uint32_t size)
{
	uint64_t off64 = (uint64_t)offset;
	uint32_t readBytes = 0u;
	SpkgVerifyRequest *reqPtr = req;
	int status;

	if (dst == NULL || req == NULL || size == 0u)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	status = spkgReadHeaderCallback((uintptr_t)dst, &off64, size,
			&readBytes, &reqPtr);
	if (status == 0 && readBytes != size)
		return SCE_SBL_ERROR_UPD_INVALID_ADDR;
	return status;
}

/* 0x80fdb8 (name ours) */
int spkgValidateUnencryptedHeaders(SpkgVerifier *v, SpkgVerifyRequest *req)
{
	uint8_t cfHdr[48];
	SpkgExtHeader extHdr;
	uint32_t extHdrOff;
	int status;

	if (req == NULL)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;

	memset(cfHdr, 0, sizeof(cfHdr));
	memset(&extHdr, 0, sizeof(extHdr));

	status = spkgReadPlainHeaderBytes(cfHdr, req, 0u, sizeof(cfHdr));
	if (status != 0)
		return status;
	if (spkgCheckCfHeader(v, cfHdr) == 0)
		return SCE_SBL_ERROR_UPD_BAD_HEADER;

	extHdrOff = *(const uint32_t *)(const void *)(cfHdr + 0x10u);
	status = spkgReadPlainHeaderBytes(&extHdr, req, extHdrOff, sizeof(extHdr));
	if (status != 0)
		return status;
	if (extHdr.magic != 4u)
		return SCE_SBL_ERROR_UPD_BAD_HEADER;
	if (spkgCheckMinVersionFloor(v, &extHdr) != 0 ||
	    spkgCheckExtHeaderField30(v, &extHdr) != 0) {
		return SCE_SBL_ERROR_UPD_VERSION_FLOOR;
	}

	return 0;
}

/* 0x80fe58 (name ours) */
int spkgVerifierPrepare(SpkgVerifier *v, SpkgVerifyRequest *req)
{
	if (req == NULL)
		return SCE_SBL_ERROR_UPD_INVALID_VALUE;
	if (spkgValidateUnencryptedHeaders(v, req) != 0)
		return SCE_SBL_ERROR_UPD_PREPARE_FAILED;

	v->req = req;
	v->isPrepared = 1u;
	return 0;
}

/* 0x80fe94 (name ours) */
int spkgCheckTargetCompatibility(SpkgVerifier *v, const SpkgExtHeader *extHdr)
{
	uint8_t cid[16];
	int status = SCE_SBL_ERROR_UPD_TARGET_MISMATCH;

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

/* 0x80ff60 (name ours) */
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

/* 0x80ffe8 (name ours) */
uint32_t spkgGetHardwareAttrMask(SpkgVerifier *v, uint32_t idx)
{
	(void)v;
	if (idx < 90u)
		return s_spkgHwRevRules[idx].attrMask;
	return 0u;
}

/* 0x810004 (name ours) */
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

/* 0x81005c (name ours) */
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
				status = SCE_SBL_ERROR_UPD_HW_REV_FLOOR;
				goto done;
			}
		}
		if (spkgIsVersionBelowHwMin(v, extHdr->pkgType,
				extHdr->version, defaultMinVer) != 0) {
			status = SCE_SBL_ERROR_UPD_HW_REV_FLOOR;
		}
	}

done:
	aimgrClearConsoleId(cid);
	return status;
}

/* 0x810120 (name ours) */
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
			return SCE_SBL_ERROR_UPD_BAD_DIGEST;
	}
	return 0;
}

/* 0x81017a (name ours) */
int spkgVerifierExecute(SpkgVerifier *v)
{
	SelfAuthContext cfCtx;
	SpkgVerifyRequest *reqPtr;
	int64_t expectedHeaderLen = 0x400LL;
	uint8_t rsaKey[260];
	uint32_t i;
	int status;

	if (v->isPrepared == 0u)
		return SCE_SBL_ERROR_UPD_PREPARE_FAILED;

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
	    spkgCheckMinVersionFloor(v, &v->req->decryptedExtHdr1) != 0 ||
	    spkgCheckExtHeaderField30(v, &v->req->decryptedExtHdr1) != 0) {
		return SCE_SBL_ERROR_UPD_PREPARE_FAILED;
	}

	status = spkgCheckTargetCompatibility(v, &v->req->decryptedExtHdr1);
	if (status == 0) {
		status = spkgCheckFactoryUpdatePolicy(v, &v->req->decryptedExtHdr1);
		if (status == 0)
			status = spkgCheckPkgVersionAndChassis(v, &v->req->decryptedExtHdr1);
	}
	return status;
}

/* 0x8102e6 (name ours) */
uint32_t spkgIsQaFlagSet(void)
{
	uint32_t slot50a[8];

	spkgReadKeyringSlot32(0x50au, slot50a);
	return ((const uint8_t *)slot50a)[15] & 1u;
}

/* ============================================================================
 * Bigmac LLI & SHA-256 Hardware Helpers (0x8113ec..0x811e45)
 * ============================================================================ */

int bigmacStartHash(void *dstDigest, const void *src, uint32_t size,
		uint32_t hashAlgo, const uint32_t *bmCtrl);

/* 0x8113ec (name ours) */
int bigmacInitLliChain(BigmacLliDesc *list, uint32_t count, uint32_t funcFlags,
		uint32_t keySlot, const void *iv)
{
	uint32_t i;

	if (count == 0u || ((uintptr_t)list & 0x1fu) != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	for (i = 0u; i < count; i++) {
		list[i].src = 0u;
		list[i].dst = 0u;
		list[i].size = 0u;
		list[i].func = funcFlags;
		list[i].keySlot = keySlot;
		list[i].iv = (uint32_t)(uintptr_t)iv;
		list[i].rsvd18 = 0u;
		list[i].next = (uint32_t)(uintptr_t)&list[i + 1u];
	}
	list[count - 1u].next = 0xffffffffu;
	return 0;
}

/* 0x811dee (name ours) */
static int bigmacStartAesCipherLli(BigmacLliDesc *list, uint32_t op,
		uint32_t mode, uint32_t keySizeType, const uint32_t *ctrl)
{
	uint32_t channel = ctrl[0];
	uint32_t funcMask = (op & 7u) | ((mode << 3) & 0x38u) |
			    ((keySizeType << 8) & 0x300u);
	BigmacLliDesc *cur = list;
	uint32_t regBase;

	do {
		if (((uintptr_t)cur & 0x1fu) != 0u)
			return SCE_SBL_ERROR_INVALID_VALUE;
		cur->func |= funcMask;
		cur = (BigmacLliDesc *)(uintptr_t)cur->next;
	} while ((uintptr_t)cur != 0xffffffffu);

	regBase = 0xe0050000u + ((channel & 1u) << 7);
	REG32(regBase + 0x20u) = (uint32_t)(uintptr_t)list;
	return 0;
}

/* 0x811674 (name ours) */
int bigmacStartAes128CbcEncryptLli(BigmacLliDesc *list, const uint32_t *ctrl)
{
	return bigmacStartAesCipherLli(list, 1u, 1u, 1u, ctrl);
}

/* 0x81168c (name ours) */
int bigmacStartAes128CbcDecryptLli(BigmacLliDesc *list, const uint32_t *ctrl)
{
	return bigmacStartAesCipherLli(list, 2u, 1u, 1u, ctrl);
}

/* 0x8117b8 (name ours) */
int bigmacStartSha256(void *dstDigest, const void *src, uint32_t size,
		const uint32_t *bmCtrl)
{
	return bigmacStartHash(dstDigest, src, size, 2u, bmCtrl);
}
