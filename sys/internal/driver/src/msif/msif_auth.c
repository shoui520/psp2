/* Sony's msif_auth.c: medium identity and authentication helpers. */
#include "skeleton.h"
#include "msif_auth.h"
#include "rmauth.h"
#include <mgkeymgr/mgkeymgr.h>
#include <sysmem/debug.h>
#include <psp2kern/kernel/utils.h>
#include <psp2kern/kernel/ssmgr.h>
#include <arm_neon.h>
#include <string.h>

int MsproalGetSystemInformation(struct MsproalSystemInformation *information);
int MsproalGetMediaInformation(struct MsproalMediaInformation *information);
int ksceSblDmac5Rnd(void *result, unsigned int size, int coherent);

static const ScePortabilityData gMsifParameterPrimary = {
	.msg_size = 32,
	.msg = {
		0xb0, 0x6c, 0xa2, 0x7e, 0xaf, 0xbe, 0x0c, 0x17, 0x3d, 0x8f, 0xa7, 0x8f, 0xd4, 0xe1, 0xe6, 0xb6,
		0xf0, 0xc8, 0x91, 0x93, 0x6b, 0xb1, 0x95, 0x6e, 0x54, 0x7c, 0xfc, 0xc8, 0x32, 0x5c, 0xc1, 0xe2,
	},
};

static const ScePortabilityData gMsifParameterFallback = {
	.msg_size = 32,
	.msg = {
		0x34, 0xcb, 0x9e, 0xf9, 0x3f, 0xda, 0x96, 0x15, 0x7a, 0xb2, 0x0c, 0x2a, 0xb4, 0x87, 0x36, 0x24,
		0x21, 0xb4, 0x07, 0xf2, 0x4f, 0x61, 0x35, 0x85, 0x5f, 0x8e, 0xf7, 0x72, 0xb3, 0x9a, 0x08, 0x53,
	},
};

static const uint64_t gMsifParameterIv[2] = { 0, 0 };

/* Encrypted curve data, shared unchanged by both TOOL originals. */
static const union MsifEcdsaParameters gMsifEncryptedParameters = {
	.bytes = {
		0xf0, 0x8c, 0xf8, 0x29, 0xd7, 0x6e, 0x1b, 0x03, 0xfa, 0xc5, 0x62, 0xf3, 0x38, 0xdf, 0xb2, 0x2a,
		0x1c, 0xdc, 0x38, 0x8b, 0x2c, 0x1b, 0xda, 0x5d, 0x6f, 0x13, 0x8c, 0x0f, 0x1e, 0x5f, 0xd8, 0xb3,
		0x26, 0x9d, 0x0e, 0xa4, 0xe6, 0x94, 0x97, 0xa5, 0x1e, 0x9a, 0x5d, 0x83, 0x56, 0x2d, 0xb9, 0x30,
		0xcd, 0xe8, 0x9a, 0xe6, 0xbf, 0x52, 0x0f, 0x91, 0x60, 0x04, 0xa5, 0x74, 0x44, 0x17, 0x61, 0x03,
		0xbd, 0x46, 0x30, 0x13, 0xfb, 0x86, 0x96, 0x2e, 0xc4, 0x3b, 0x09, 0x36, 0x72, 0x55, 0x44, 0x30,
		0x6b, 0x57, 0x54, 0x09, 0x41, 0xf6, 0xc2, 0xff, 0x98, 0xf1, 0x16, 0xc8, 0x04, 0x15, 0x84, 0x3b,
		0x83, 0xbc, 0xce, 0xb0, 0xb2, 0x37, 0x2a, 0xe3, 0x65, 0xd5, 0xb1, 0xd8, 0x3c, 0xf7, 0x43, 0xd6,
		0x13, 0x9f, 0x11, 0xe7, 0x80, 0x75, 0x5a, 0xec, 0x95, 0x66, 0x1c, 0xe7, 0xc4, 0x35, 0xd6, 0x57,
		0x7f, 0xd6, 0xcb, 0x78, 0x52, 0x0a, 0x03, 0x70, 0xea, 0x11, 0x7b, 0xa2, 0xd2, 0x4e, 0x59, 0x87,
		0x9b, 0xa0, 0xbb, 0xf1, 0x49, 0x86, 0x2c, 0x2d, 0xf9, 0x20, 0x77, 0x4c, 0xa9, 0x93, 0xac, 0xd5,
		0x5b, 0xb2, 0x9d, 0x93, 0x7e, 0xdb, 0xf7, 0xbf, 0xb3, 0x90, 0xe9, 0x6a, 0x44, 0xa9, 0xd8, 0xdc,
		0x04, 0x46, 0x19, 0x40, 0xd6, 0x60, 0x9d, 0x8d, 0x2b, 0xe7, 0xd8, 0x4e, 0x4e, 0xcf, 0x44, 0x32,
		0x2b, 0x80, 0x0b, 0x00, 0x5b, 0xc2, 0xa6, 0x67, 0xe7, 0x48, 0xbf, 0xaa, 0x05, 0x58, 0xe4, 0xdf,
		0x7e, 0x91, 0xdf, 0x24, 0xbc, 0x2d, 0xe8, 0x99, 0xf4, 0x5f, 0x5c, 0x94, 0x58, 0x06, 0xfe, 0x30,
	},
};

static int gMsifParametersInitialized;
static union MsifEcdsaParameters gMsifParameters;

int get_ecdsa_param(struct MsifEcdsaPoint *publicKey, struct MsifEcdsaCurve *curve)
{
	ScePortabilityData portability;
	struct MsifParameterCbcContext state;
	union {
		uint64_t words[4];
		uint8_t bytes[32];
	} decoded;
	unsigned int blockBytes, offset, i;

	if (gMsifParametersInitialized)
		goto publish;
	portability = (ScePortabilityData){ .msg_size = 32 };
	if (ksceSblSsDecryptWithPortability(4, (void *)gMsifParameterIv,
		(ScePortabilityData *)&gMsifParameterPrimary, &portability) ||
		(memcmp(portability.msg, gMsifParameterIv, 16) &&
		 ksceSblSsDecryptWithPortability(4, (void *)gMsifParameterIv,
			(ScePortabilityData *)&gMsifParameterFallback, &portability))) {
		__builtin_memset(&portability, 0, sizeof(portability));
		return -204;
	}
	if (ksceAesInit1((SceAesContext *)&state.aes, 128, 128,
		portability.msg + 16) < 0) {
		__builtin_memset(&portability, 0, sizeof(portability));
		memset(&state, 0, sizeof(state));
		return -204;
	}
	blockBytes = state.aes.encryption.blockWords * 4;
	if (blockBytes)
		memcpy(state.chain, gMsifParameterIv, blockBytes);
	for (offset = 0; offset < sizeof(gMsifEncryptedParameters); offset += 16) {
		ksceAesDecrypt1((SceAesContext *)&state.aes,
			gMsifEncryptedParameters.bytes + offset, decoded.bytes);
		blockBytes = state.aes.encryption.blockWords * 4;
		i = 0;
		if (blockBytes >> 4) {
			uint8x16_t chain = vld1q_u8(state.chain);
			uint8x16_t ciphertext = vld1q_u8(gMsifEncryptedParameters.bytes + offset);
			uint8x16_t plaintext = vld1q_u8(decoded.bytes);

			vst1q_u8(state.chain, ciphertext);
			vst1q_u8(gMsifParameters.bytes + offset, veorq_u8(plaintext, chain));
			if ((blockBytes >> 4) > 1) {
				chain = vld1q_u8(state.chain + 16);
				ciphertext = vld1q_u8(gMsifEncryptedParameters.bytes + offset + 16);
				plaintext = vld1q_u8(decoded.bytes + 16);
				vst1q_u8(state.chain + 16, ciphertext);
				vst1q_u8(gMsifParameters.bytes + offset + 16,
					veorq_u8(plaintext, chain));
			}
			i = (blockBytes >> 4) * 16;
		}
		for (; i < blockBytes; i++) {
			uint8_t ciphertext = gMsifEncryptedParameters.bytes[offset + i];

			gMsifParameters.bytes[offset + i] = decoded.bytes[i] ^ state.chain[i];
			state.chain[i] = ciphertext;
		}
	}
	SCE_DBG_ASSERT_FUNC(1, 335, "get_ecdsa_param");
	__builtin_memset(&portability, 0, sizeof(portability));
	memset(&state, 0, sizeof(state));
	gMsifParametersInitialized = 1;
publish:
	publicKey->x = gMsifParameters.publicX;
	publicKey->y = gMsifParameters.publicY;
	curve->prime = gMsifParameters.prime;
	curve->coefficientA = gMsifParameters.coefficientA;
	curve->coefficientB = gMsifParameters.coefficientB;
	curve->order = gMsifParameters.order;
	curve->baseX = gMsifParameters.baseX;
	curve->baseY = gMsifParameters.baseY;
	return 0;
}

int MsifComputeMediaMac(uint64_t *mac, const uint64_t *data, unsigned int length)
{
	union MsifMacBlock block = { .value = 0 };
	union MsifMacBlock firstSubkey, secondSubkey;
	uint64_t encrypted, bigEndian, chain = 0;
	int result;

	result = ksceSblDmac5DesEcbEncWithKeyslot(&block, &encrypted, 8,
		28, 192, 1);
	if (result < 0)
		return result;
	block.value = encrypted;
	bigEndian = (((((((uint64_t)block.bytes[0] << 8 | block.bytes[1]) << 8 |
		block.bytes[2]) << 8 | block.bytes[3]) << 8 | block.bytes[4]) << 8 |
		block.bytes[5]) << 8 | block.bytes[6]) << 8 | block.bytes[7];
	bigEndian <<= 1;
	firstSubkey.bytes[0] = bigEndian >> 56;
	firstSubkey.bytes[1] = bigEndian >> 48;
	firstSubkey.bytes[2] = bigEndian >> 40;
	firstSubkey.bytes[3] = bigEndian >> 32;
	firstSubkey.bytes[4] = bigEndian >> 24;
	firstSubkey.bytes[5] = bigEndian >> 16;
	firstSubkey.bytes[6] = bigEndian >> 8;
	firstSubkey.bytes[7] = bigEndian;
	if (block.bytes[0] & 0x80)
		firstSubkey.bytes[7] ^= 0x1b;
	bigEndian = (((((((uint64_t)firstSubkey.bytes[0] << 8 |
		firstSubkey.bytes[1]) << 8 | firstSubkey.bytes[2]) << 8 |
		firstSubkey.bytes[3]) << 8 | firstSubkey.bytes[4]) << 8 |
		firstSubkey.bytes[5]) << 8 | firstSubkey.bytes[6]) << 8 |
		firstSubkey.bytes[7];
	bigEndian <<= 1;
	secondSubkey.bytes[0] = bigEndian >> 56;
	secondSubkey.bytes[1] = bigEndian >> 48;
	secondSubkey.bytes[2] = bigEndian >> 40;
	secondSubkey.bytes[3] = bigEndian >> 32;
	secondSubkey.bytes[4] = bigEndian >> 24;
	secondSubkey.bytes[5] = bigEndian >> 16;
	secondSubkey.bytes[6] = bigEndian >> 8;
	secondSubkey.bytes[7] = bigEndian;
	if (firstSubkey.bytes[0] & 0x80)
		secondSubkey.bytes[7] ^= 0x1b;
	while (length > 8) {
		uint64_t value = *data++;

		__builtin_prefetch(data + 6);
		block.value = value ^ chain;
		result = ksceSblDmac5DesEcbEncWithKeyslot(&block, &encrypted, 8,
			28, 192, 1);
		if (result < 0)
			return result;
		length -= 8;
		chain = encrypted;
	}
	if (length == 8) {
		block.value = chain ^ firstSubkey.value ^ *data;
	} else {
		block.value = *data;
		block.bytes[length] = 0;
		if (length != 7)
			memset(block.bytes + length + 1, 0, 7 - length);
		block.value ^= chain ^ secondSubkey.value;
	}
	result = ksceSblDmac5DesEcbEncWithKeyslot(&block, &encrypted, 8,
		28, 192, 1);
	if (result < 0)
		return result;
	*mac = encrypted;
	return 0;
}

int MsifGetSystemMediaId(uint8_t *mediaId)
{
	struct MsproalSystemInformation information;
	int result = MsproalGetSystemInformation(&information);

	if (!result)
		__builtin_memcpy(mediaId, information.id, 16);
	return result;
}

int MsifAuthenticateMedia(struct MsproalHandle *handle, uint8_t *mediaId)
{
	uint32_t generation;
	uint64_t hostChallenge, receivedMac, mac;
	uint64_t challenges[2];
	union MsifMediaAuthMessage message;
	union MsifAuthTransfer transfer;
	int result;

	result = call_get_key_master_gen_no(&generation);
	if (result < 0)
		return result;
	result = ksceSblDmac5Rnd(&hostChallenge, sizeof(hostChallenge), 1);
	if (result < 0)
		return result;
	message.hostChallenge = hostChallenge;
	__builtin_memcpy(transfer.request.hostChallenge, &hostChallenge,
		sizeof(hostChallenge));
	transfer.request.generation = generation;
	memset(transfer.request.reserved, 0, sizeof(transfer.request.reserved));
	result = MsproalCommandWriteShortData(handle, 0x48,
		sizeof(transfer.request), &transfer, 1000);
	if (result)
		return result == -105 ? -201 : result;
	result = MsproalCommandReadShortData(handle, 0x49, sizeof(transfer),
		&transfer, 1000);
	if (result)
		return result;
	message.card = transfer.response.card;
	__builtin_memcpy(&receivedMac, transfer.response.mac, sizeof(receivedMac));
	result = call_set_ind_key(message.card.mediaId);
	if (result < 0)
		return result;
	result = MsifComputeMediaMac(&mac, message.blocks,
		sizeof(message));
	if (result)
		return result;
	if (mac != receivedMac)
		return -202;
	challenges[0] = hostChallenge;
	__builtin_memcpy(&challenges[1], message.card.cardChallenge,
		sizeof(challenges[1]));
	result = MsifComputeMediaMac(&mac, challenges, sizeof(challenges));
	if (result)
		return result;
	__builtin_memcpy(transfer.proof.mac, &mac, sizeof(mac));
	__builtin_memset(transfer.proof.reserved, 0, sizeof(transfer.proof.reserved));
	result = MsproalCommandWriteShortData(handle, 0x4a,
		sizeof(transfer.proof), &transfer, 1000);
	if (result)
		return result == -106 ? -202 : result;
	if (mediaId)
		__builtin_memcpy(mediaId, message.card.mediaId,
			sizeof(message.card.mediaId));
	return message.card.capacityExponent;
}

int msif_check_mid(struct MsproalHandle *handle, const uint8_t *mediaId)
{
	struct MsifEcdsaSignature signature;
	struct MsifEcdsaPoint publicKey;
	struct MsifEcdsaCurve curve;
	uint8_t digest[SCE_SHA224_DIGEST_SIZE];
	const uint8_t *version = handle->certificate.version;
	int result;

	result = MsproalCommandReadLongData(handle, 0x4b, &handle->certificate,
		1000);
	if (!result) {
		if (((uint32_t)version[0] << 24 | (uint32_t)version[1] << 16 |
			(uint32_t)version[2] << 8 | version[3]) != 1 ||
			get_ecdsa_param(&publicKey, &curve) ||
			(signature.r = handle->certificate.r,
			 signature.s = handle->certificate.s,
			 ksceSha224Digest(mediaId, 16, digest)) ||
			(result = MsifEcdsaVerify224(&signature, digest, &publicKey,
				&curve)))
			result = -204;
	}
	return result;
}

int msif_check_capacity(int capacityExponent)
{
	struct MsproalMediaInformation information;
	int result = MsproalGetMediaInformation(&information);

	if (!result) {
		uint64_t capacity = 1ULL << capacityExponent;

		if ((capacity >> 9) < information.totalSectors)
			return -205;
		if (information.totalSectors < (capacity >> 10))
			return -205;
		return 0;
	}
	return result;
}
