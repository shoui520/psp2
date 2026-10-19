/* Original source-file and internal names are unavailable. */
#include "magicgate_private.h"
#include <string.h>
#include <mgkeymgr/mgkeymgr.h>

/* Ciphertext owners are supplied by the firmware's data table. */
extern MagicGateKey initialFirstWrappedKey;
extern MagicGateKey initialSecondWrappedKey;
const MagicGateKey wrappedKeyResetMarker = { .bytes = { 0 } };
extern const MagicGateKey wrappedTreeKeys[18];
extern const MagicGateKey wrappedTagKey;
extern const MagicGateKey wrappedFinalTagKey;
extern const MagicGateKey wrappedAuxiliaryDriverKey;
extern const MagicGateKey wrappedRecordCipherKey;
extern const MagicGateBundledRecord bundledRecord;
extern const MagicGateBundledEkb2Record bundledEkb2Record;

MagicGateKey firstWrappedKey;
MagicGateKey secondWrappedKey;

const MagicGateDeviceIdentity *activeDeviceIdentity;
MagicGateKeyState magicGateKeyState;

uint64_t *getSavedChallenge(uint64_t *challenge)
{
	if (challenge)
		*challenge = magicGateKeyState.savedChallenge;
	return challenge;
}

uint64_t *setSavedChallenge(uint64_t *challenge)
{
	if (challenge)
		magicGateKeyState.savedChallenge = *challenge;
	return challenge;
}

const MagicGateDeviceIdentity *getDeviceIdentity(int type)
{
	if ((unsigned int)(type - 2) > 1) {
		activeDeviceIdentity = NULL;
		return NULL;
	}
	activeDeviceIdentity = &defaultDeviceIdentity;
	return &defaultDeviceIdentity;
}

int decryptWrappedKeys(void *output, const void *input, unsigned int size)
{
	const uint8_t *source = input;
	const uint8_t *next;
	const uint8_t *end;
	uint8_t *destination = output;
	unsigned int remainder;
	int result = 0;

	if (!memcmp(&firstWrappedKey, &wrappedKeyResetMarker, 16)) {
		result = SceSblMgKeyMgrForDriver_1263FB04(&secondWrappedKey,
						       &initialSecondWrappedKey);
		if (result < 0)
			return result;
		result = SceSblMgKeyMgrForDriver_1263FB04(&firstWrappedKey,
						       &initialFirstWrappedKey);
		if (result < 0)
			return result;
	}
	if (size <= 15)
		return result;
	next = source + 16;
	end = next + ((size - 16) & ~15U);
	result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
			&secondWrappedKey, source, destination);
	remainder = ((end - source - 16) >> 4) & 3;
	if (result < 0)
		goto clearKey;
	destination += 16;
	if (next == end)
		return result;
	if (remainder) {
		if (remainder != 1) {
			if (remainder != 2) {
				result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
						&secondWrappedKey, next, destination);
				if (result < 0)
					goto clearKey;
				next += 16;
				destination += 16;
			}
			result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
					&secondWrappedKey, next, destination);
			if (result < 0)
				goto clearKey;
			next += 16;
			destination += 16;
		}
		result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
				&secondWrappedKey, next, destination);
		if (result < 0)
			goto clearKey;
		next += 16;
		destination += 16;
		if (next == end)
			return result;
	}
	do {
		result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
				&secondWrappedKey, next, destination);
		if (result < 0)
			goto clearKey;
		result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
				&secondWrappedKey, next + 16, destination + 16);
		if (result < 0)
			goto clearKey;
		result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
				&secondWrappedKey, next + 32, destination + 32);
		if (result < 0)
			goto clearKey;
		result = SceSblMgKeyMgrForDriver_DF3B808D(&firstWrappedKey,
				&secondWrappedKey, next + 48, destination + 48);
		next += 64;
		destination += 64;
		if (result < 0)
			goto clearKey;
	} while (next != end);
	return result;

clearKey:
	firstWrappedKey.words[0] = 0;
	firstWrappedKey.words[1] = 0;
	firstWrappedKey.words[2] = 0;
	firstWrappedKey.words[3] = 0;
	return result;
}

int loadDriverKeyTable(void)
{
	return decryptWrappedKeys(magicGateKeyState.treeKeys, wrappedTreeKeys,
				  sizeof(magicGateKeyState.treeKeys));
}

int getTreeKey(int type, MagicGateKey *output, unsigned int depth)
{
	if ((unsigned int)(type - 2) > 1)
		return 0x80501000;
	depth -= 8;
	if (depth > 17)
		return 0x80501004;
	*output = magicGateKeyState.treeKeys[depth];
	return 0;
}

int getDriverKey(void *output, int index)
{
	const MagicGateKey *key;
	int result;

	if (!output)
		return 0x80501000;
	switch (index) {
	case 0:
		key = &wrappedTagKey;
		goto decryptKey;
	case 1:
		key = &wrappedFinalTagKey;
		goto decryptKey;
	case 2:
		key = &wrappedAuxiliaryDriverKey;
		goto decryptKey;
	case 3:
		key = &wrappedRecordCipherKey;
decryptKey:
		result = decryptWrappedKeys(output, key, sizeof(*key));
		break;
	default:
		result = 0x80501001;
		break;
	}
	return result;
}

int SceMagicGateForDriver_6A4D4F91(MagicGateDeviceIdentity *output)
{
	activeDeviceIdentity = &defaultDeviceIdentity;
	*output = defaultDeviceIdentity;
	return 0;
}

int SceMagicGateForDriver_0201E54B(MagicGateDeviceIdentity *output)
{
	activeDeviceIdentity = &defaultDeviceIdentity;
	*output = defaultDeviceIdentity;
	return 0;
}

int SceMagicGateForDriver_758D7158(void *output, unsigned int size)
{
	if (output) {
		if (size >= sizeof(bundledRecord))
			size = sizeof(bundledRecord);
		memcpy(output, &bundledRecord, size);
	}
	return sizeof(bundledRecord);
}

int SceMagicGateForDriver_0EBA5958(void *output, unsigned int size)
{
	if (output) {
		if (size >= sizeof(bundledEkb2Record))
			size = sizeof(bundledEkb2Record);
		memcpy(output, &bundledEkb2Record, size);
	}
	return sizeof(bundledEkb2Record);
}
