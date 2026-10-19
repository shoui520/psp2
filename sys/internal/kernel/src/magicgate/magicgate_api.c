/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"
#include <ss_mgr/ss_mgr.h>
#include <string.h>
#include <sysmem/sysclib.h>

const uint32_t defaultMacIv[4] = { 0, 0, 0, 0 };

const MagicGateDeviceIdentity defaultDeviceIdentity = {
	.prefix = { 0 },
	.path = { 0x03, 0x8e, 0, 0, 0, 0, 0, 0 },
};

int installRecord(int slot, const MagicGateRecordHeader *record,
		  unsigned int size, int type)
{
	const MagicGateDeviceIdentity *identity;
	unsigned int treeSize;
	int count = 63;
	int depth;
	int result;

	identity = getDeviceIdentity(type);
	if (!identity)
		return (int)0x80501001;
	if (size < sizeof(*record))
		return (int)0x80501002;
	if (record->identity.format)
		return (int)0x80501004;
	if (record->identity.authenticationMode) {
		if (record->identity.authenticationMode != 2 || record->identity.keyMode != 2)
			return (int)0x80501004;
	} else if (record->identity.keyMode)
		return (int)0x80501004;
	result = checkCachedRecord(slot, record);
	if (!result)
		return result;
	result = SceMagicGateForDriver_A49B9D70();
	if (result < 0)
		return result;
	result = selectRecordBlocks(installedRecord.blocks, &count, &depth,
				   identity->path, record, size);
	if (result < 0)
		goto out;
	installedRecord.count[0] = (unsigned int)count >> 24;
	installedRecord.count[1] = (unsigned int)count >> 16;
	installedRecord.count[2] = (unsigned int)count >> 8;
	installedRecord.count[3] = count;
	installedRecord.depth[0] = (unsigned int)depth >> 24;
	installedRecord.depth[1] = (unsigned int)depth >> 16;
	installedRecord.depth[2] = (unsigned int)depth >> 8;
	installedRecord.depth[3] = depth;
	installedRecord.identity = record->identity;
	if (record->identity.authenticationMode == 2) {
		result = installRecordKey(slot, type, &installedRecord, NULL);
		if (result < 0)
			goto out;
		treeSize = (unsigned int)record->treeSize[0] << 24 |
			   (unsigned int)record->treeSize[1] << 16 |
			   (unsigned int)record->treeSize[2] << 8 | record->treeSize[3];
		result = verifyRecordTag(record->tag, &installedRecord.identity.identifier,
					 record + 1, treeSize, slot);
		if (result < 0)
			goto out;
	} else {
		result = installRecordKey(slot, type, &installedRecord,
					 (const uint64_t *)&record->tag[8]);
		if (result < 0)
			goto out;
	}
	notifyRecordInstalled(slot, &installedRecord.identity);
out:
	SceMagicGateForDriver_492EE950();
	return result;
}

int SceMagicGateForDriver_12CE769D(uint64_t *challenge)
{
	uint64_t random;
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		getSavedChallenge(challenge);
		result = sceSblRngPseudoRandomNumber(&random, 8);
		if (result >= 0) {
			if (random)
				*challenge = random ^ *challenge;
			else {
				*challenge = 0;
				((uint8_t *)challenge)[0] = 0x80;
			}
			result = 0;
			setSavedChallenge(challenge);
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_E0888D78(uint64_t *random, uint64_t *challenge)
{
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		result = sceSblRngPseudoRandomNumber(random, 8);
		if (result >= 0) {
			if (*random) {
				result = 0;
				*challenge = *random ^ *challenge;
			} else {
				*challenge = 0;
				result = 0;
				((uint8_t *)challenge)[0] = 0x80;
			}
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_4F4B2DC9(void *output, const void *input,
				void *iv, unsigned int size, int slot)
{
	int result;

	if (slot < 0) {
		result = (int)0x80501006;
		goto out;
	}
	if (size & 7) {
		result = (int)0x80501002;
		goto out;
	}
	result = size;
	if (size) {
		result = SceMagicGateForDriver_A49B9D70();
		if (result >= 0)
			result = cipherBuffer(output, input, iv, size, slot, NULL, 1);
	}
out:
	return result;
}

int SceMagicGateForDriver_B0827B4F(void *output, const void *input,
				void *iv, unsigned int size, int slot)
{
	int result;

	if (slot < 0) {
		result = (int)0x80501006;
		goto out;
	}
	if (size & 7) {
		result = (int)0x80501002;
		goto out;
	}
	result = size;
	if (size) {
		result = SceMagicGateForDriver_A49B9D70();
		if (result >= 0)
			result = cipherBuffer(output, input, iv, size, slot, NULL, 0);
	}
out:
	return result;
}

int SceMagicGateForDriver_593016E1(void *output, const void *input,
				void *iv, unsigned int size, int slot)
{
	int result;

	if (slot < 0)
		return (int)0x80501006;
	if (!iv)
		iv = (void *)defaultMacIv;
	if (size & 7)
		return (int)0x80501002;
	result = size;
	if (size) {
		result = SceMagicGateForDriver_A49B9D70();
		if (result >= 0)
			result = cipherBuffer(output, input, iv, size, slot, NULL, 2);
	}
	return result;
}

int SceMagicGateForDriver_6A240950(void *output, const void *input,
				void *iv, unsigned int size, int slot)
{
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		if (size & 7)
			result = (int)0x80501002;
		else {
			result = size;
			if (size) {
				if (!output)
					output = &sessionKey;
				result = calculateRetailMac(&slotKeys[slot], input,
							    output, size, iv);
			}
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_9CEF4965(void *output, const void *input,
				void *iv, unsigned int size, const void *key)
{
	int result;

	if (!iv)
		iv = (void *)defaultMacIv;
	if (size & 7)
		result = (int)0x80501002;
	else {
		result = size;
		if (size) {
			result = SceMagicGateForDriver_A49B9D70();
			if (result >= 0)
				result = cipherBuffer(output, input, iv, size, -1, key, 2);
		}
	}
	return result;
}

int SceMagicGateForDriver_A495FA95(void *output, const void *input,
				void *iv, unsigned int size)
{
	int result;

	if (size & 7)
		result = (int)0x80501002;
	else {
		result = size;
		if (size) {
			result = SceMagicGateForDriver_A49B9D70();
			if (result >= 0)
				result = cipherBuffer(output, input, iv, size, -2, NULL, 1);
		}
	}
	return result;
}

int SceMagicGateForDriver_0770CBEF(void *output, const void *input,
				void *iv, unsigned int size)
{
	int result;

	if (size & 7)
		result = (int)0x80501002;
	else {
		result = size;
		if (size) {
			result = SceMagicGateForDriver_A49B9D70();
			if (result >= 0)
				result = cipherBuffer(output, input, iv, size, -2, NULL, 0);
		}
	}
	return result;
}

int SceMagicGateForDriver_CFD943E0(void *output, const void *input,
				void *iv, unsigned int size)
{
	int result;

	if (!iv)
		iv = (void *)defaultMacIv;
	if (size & 7)
		result = (int)0x80501002;
	else {
		result = size;
		if (size) {
			result = SceMagicGateForDriver_A49B9D70();
			if (result >= 0)
				result = cipherBuffer(output, input, iv, size, -2, NULL, 2);
		}
	}
	return result;
}

int SceMagicGateForDriver_42A9D74E(int slot, void *output,
		const uint64_t *info, const uint64_t *challenge,
		const uint64_t *response, const MagicGateInstalledRecord *record)
{
	uint64_t input[4];
	void *destination;
	unsigned int initialSlot;
	int result = (int)0x80501001;

	if (getDeviceIdentity(2)) {
		destination = slot >= 0 ? &slotKeys[slot] : NULL;
		input[0] = info[0];
		if (output)
			destination = output;
		input[1] = info[1];
		input[2] = *challenge;
		input[3] = *response;
		initialSlot = getInitialSlot(0);
		if (record->count[0] & 0x80)
			result = (int)0x80503002;
		else {
			result = SceMagicGateForDriver_A49B9D70();
			if (result >= 0) {
				result = installRecordKey(initialSlot, 2, record, NULL);
				if (result >= 0) {
					if (!destination)
						destination = &sessionKey;
					result = calculateRetailMac(&slotKeys[initialSlot],
						input, destination, sizeof(input), NULL);
				}
				SceMagicGateForDriver_492EE950();
			}
		}
	}
	return result;
}

int SceMagicGateForDriver_7A233CBC(int slot, const MagicGateRecordHeader *record,
				unsigned int size)
{
	return installRecord(slot, record, size, 0);
}

int SceMagicGateForDriver_465A7B76(int slot, const MagicGateRecordHeader *record,
				unsigned int size)
{
	return installRecord(slot, record, size, 1);
}

int SceMagicGateForDriver_B20497C7(int slot, const MagicGateRecordHeader *record,
				unsigned int size)
{
	return installRecord(slot, record, size, 2);
}

int SceMagicGateForDriver_2D5069DE(int slot, const MagicGateRecordHeader *record,
				unsigned int size)
{
	return installRecord(slot, record, size, 3);
}

int SceMagicGateForDriver_CA74A13D(void *output, unsigned int capacity,
		const MagicGateRecordHeader *record, unsigned int size,
		const MagicGateDeviceIdentity *challenge)
{
	const MagicGateDeviceIdentity *identity;
	unsigned int copied;
	int count;
	int depth;
	int result;

	if (!challenge) {
		identity = getDeviceIdentity(0);
		if (!identity)
			return (int)0x80501001;
		/* The original advances twice on this currently unreachable path. */
		challenge = (const MagicGateDeviceIdentity *)identity->path;
	}
	if (size < sizeof(*record))
		return (int)0x80501002;
	if (record->identity.format)
		return (int)0x80501004;
	if (record->identity.authenticationMode) {
		if (record->identity.authenticationMode != 2 || record->identity.keyMode != 2)
			return (int)0x80501004;
	} else if (record->identity.keyMode)
		return (int)0x80501004;
	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		count = 63;
		result = selectRecordBlocks(installedRecord.blocks, &count, &depth,
					    challenge->path, record, size);
		if (result >= 0) {
			installedRecord.count[0] = (unsigned int)count >> 24;
			installedRecord.count[1] = (unsigned int)count >> 16;
			installedRecord.count[2] = (unsigned int)count >> 8;
			installedRecord.count[3] = count;
			installedRecord.depth[0] = (unsigned int)depth >> 24;
			installedRecord.depth[1] = (unsigned int)depth >> 16;
			installedRecord.depth[2] = (unsigned int)depth >> 8;
			installedRecord.depth[3] = depth;
			copied = (count + 1) * 16;
			result = copied;
			if (capacity < copied)
				copied = capacity;
			installedRecord.identity = record->identity;
			memcpy(output, &installedRecord, copied);
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_0B01FE3F(const void *tag,
		const MagicGateRecordHeader *record, unsigned int size, int slot)
{
	uint64_t encrypted[2];
	unsigned int treeSize;
	int encryptedResult;
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result < 0)
		return result;
	if (size < sizeof(*record)) {
		result = (int)0x80501002;
		goto out;
	}
	if (record->identity.format)
		goto invalid;
	if (!record->identity.authenticationMode) {
		if (record->identity.keyMode)
			goto invalid;
		result = (int)0x80501004;
		encryptRecordBlock(&slotKeys[slot], &slotKeys[slot], &encrypted[0]);
		encryptedResult = encryptRecordBlock(&slotKeys[slot],
						    &slotKeys[slot].halves[1], &encrypted[1]);
		if (!timingsafe_memcmp(tag, encrypted, sizeof(encrypted)))
			result = encryptedResult;
		goto out;
	}
	if (record->identity.authenticationMode != 2 || record->identity.keyMode != 2)
		goto invalid;
	treeSize = (unsigned int)record->treeSize[0] << 24 |
		   (unsigned int)record->treeSize[1] << 16 |
		   (unsigned int)record->treeSize[2] << 8 | record->treeSize[3];
	result = verifyRecordTag(tag, &record->identity.identifier,
				 record + 1, treeSize, slot);
	goto out;
invalid:
	result = (int)0x80501004;
out:
	SceMagicGateForDriver_492EE950();
	return result;
}

int SceMagicGateForDriver_01FD0260(int slot, const void *input, int sourceSlot)
{
	MagicGateKey *key;
	const MagicGateKey *source;
	uint64_t second;
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		key = &slotKeys[slot];
		result = getDriverKey(key, 3);
		if (result >= 0) {
			source = &slotKeys[sourceSlot];
			second = key->halves[1];
			key->halves[0] ^= source->halves[0];
			key->halves[1] = source->halves[1] ^ second;
			result = decryptRecordEcb(key, input, key, 16);
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_07BDCE2F(void *output, int slot, int sourceSlot)
{
	MagicGateKey key __attribute__((aligned(16)));
	MagicGateKey source;
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		result = getDriverKey(&key, 3);
		if (result >= 0) {
			source = slotKeys[sourceSlot];
			key.halves[0] ^= source.halves[0];
			key.halves[1] ^= source.halves[1];
			result = encryptRecordEcb(&key, &slotKeys[slot], output, 16);
			*(volatile uint64_t *)&key.halves[0] = 0;
			*(volatile uint64_t *)&key.halves[1] = 0;
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_4E8B4EB5(int slot, const void *input, int sourceSlot)
{
	MagicGateKey *key;
	const MagicGateKey *source;
	uint64_t second;
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		key = &slotKeys[slot];
		result = getDriverKey(key, 3);
		if (result >= 0) {
			source = &slotKeys[sourceSlot];
			second = key->halves[1];
			key->halves[0] ^= source->halves[0];
			key->halves[1] = source->halves[1] ^ second;
			result = decryptRecordEcb(key, input, key, 16);
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_C9658A76(void *output, const void *input,
				const void *iv, unsigned int size, int slot)
{
	MagicGateKey key __attribute__((aligned(16)));
	int result;

	memset(&key, 0xff, sizeof(key));
	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		result = encryptRecordEcb(&slotKeys[slot], &key, &key, sizeof(key));
		if (result >= 0) {
			if (!iv)
				iv = defaultMacIv;
			result = calculateRecordMac(&key, input, output, size, iv);
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_05912BF5(void *output, const void *input,
				const void *iv, unsigned int size, int slot)
{
	MagicGateKey key __attribute__((aligned(16)));
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		result = encryptRecordEcb(&slotKeys[slot], defaultMacIv, &key, sizeof(key));
		if (result >= 0) {
			if (!iv)
				iv = defaultMacIv;
			result = calculateRecordMac(&key, input, output, size, iv);
		}
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_ACA0F529(int slot, const void *input, int sourceSlot)
{
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		if (!input)
			input = defaultMacIv;
		result = encryptRecordEcb(&slotKeys[sourceSlot], input, &slotKeys[slot], 16);
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_2B026FFF(void *output, const void *input,
				unsigned int size, int slot, const void *iv)
{
	if (iv)
		return decryptRecordCbc(&slotKeys[slot], input, output, size, iv);
	return decryptRecordEcb(&slotKeys[slot], input, output, size);
}

int SceMagicGateForDriver_F2502A3B(void *output, const void *input,
				unsigned int size, int slot, const void *iv)
{
	if (iv)
		return encryptRecordCbc(&slotKeys[slot], input, output, size, iv);
	return encryptRecordEcb(&slotKeys[slot], input, output, size);
}

int SceMagicGateForDriver_3270BEE6(void *output, int slot)
{
	MagicGateKey key __attribute__((aligned(16)));
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		memset(&key, -1, sizeof(key));
		result = encryptRecordEcb(&key, &slotKeys[slot], output, sizeof(key));
		*(volatile uint64_t *)&key.halves[0] = 0;
		*(volatile uint64_t *)&key.halves[1] = 0;
		SceMagicGateForDriver_492EE950();
	}
	return result;
}

int SceMagicGateForDriver_A855C045(int slot, const void *input)
{
	MagicGateKey key __attribute__((aligned(16)));
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		memset(&key, -1, sizeof(key));
		result = decryptRecordEcb(&key, input, &slotKeys[slot], sizeof(key));
		*(volatile uint64_t *)&key.halves[0] = 0;
		*(volatile uint64_t *)&key.halves[1] = 0;
		SceMagicGateForDriver_492EE950();
	}
	return result;
}
