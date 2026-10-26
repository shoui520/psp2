/*
 * The settings: two banks in the flash (configBanks), each a header and a ConfigBlock; the
 * valid one written last is used (configGet), and a newer layout's defaults (configDefault)
 * are merged in at the start. The host opens a copy (configOpen), changes it by offset or by
 * records ending in a SHA-1 digest, and commits it to the other bank, which becomes the one
 * used.
 */
#include <string.h>

#include "ernie.h"
#include "config.h"
#include "sha1.h"
#include "state.h"
#include "updatereq.h"

#define CONFIG_CLOSED		0
#define CONFIG_OPEN		1
#define CONFIG_COMMITTING	2

#define CONFIG_RECORD_HASH	'H'	/* the digest of the records before it */
#define CONFIG_RECORD_TEXT	'T'
#define CONFIG_RECORD_WRITE	'W'	/* an offset and the bytes to write there */

struct Config {
	unsigned char	ready;
	unsigned char	bank;			/* the bank used */
	unsigned char	state;			/* CONFIG_* */
	unsigned char	changed;		/* set: a commit changed the bank used */
};

/* the records' digest, in the updater's buffers */
struct ConfigHash {
	SHA1Context	context;
	unsigned char __near *expected;
	unsigned long __near *w;
	unsigned char __near *block;
	unsigned char	*digest;
	unsigned char	unhashed;		/* set: records written since the last digest */
};

/* a record: its length (of what follows it), its type and its bytes */
struct ConfigRecord {
	unsigned char	length;
	unsigned char	type;			/* CONFIG_RECORD_* */
	union {
		struct {
			unsigned char	offsetLow;	/* in the block */
			unsigned char	offsetHigh;
			unsigned char	data[SHA1BlockSize - 4];
		} write;
		unsigned char	digest[SHA1HashSize];
	} body;
};

/* the records received, hashed and applied a block at a time */
struct ConfigRecords {
	unsigned char	buffer[SHA1BlockSize];
	unsigned char	count;
	unsigned char	reserved[3];
};

/* the flash library (the updater's, at 0xf2b2-0xf6f2) */
extern void FSL_Open(void);
extern unsigned char FSL_Init(unsigned char *descriptor);
extern unsigned char FSL_ModeCheck(void);
extern unsigned char FSL_BlankCheck(unsigned int block);
extern unsigned char FSL_Erase(unsigned int block);
extern unsigned char FSL_IVerify(unsigned int block);
extern unsigned char FSL_Write(unsigned long address, unsigned char words);
extern void FSL_Close(void);
extern unsigned char fslDataBuffer[];		/* the library's descriptor and write buffer */

/* the banks, in the updater's part of the flash (app.lk), and the defaults of this build's
   layout (configdefault.c, at 0x800) */
extern const struct ConfigBank configBanks[2];
extern const struct ConfigBank configDefault;

static struct ConfigBank configBuffer;		/* the open copy */
static struct Config config;
static struct ConfigHash configHash;
static struct ConfigRecords configRecords;

static char configApplyRecords(unsigned char *records, unsigned int length,
			       unsigned char check);
static unsigned char configMerge(unsigned char from, unsigned char to);
static unsigned char configCheck(unsigned char bank);
static unsigned char configNewerVersion(void);
static unsigned char configNewerCount(void);

/* picks the bank to use, merging this build's defaults in if its layout is older */
int configInit(void)
{
	unsigned char newer;
	unsigned char result0;
	unsigned char result1;

	memset(&configRecords, 0, sizeof(configRecords));
	configHash.expected = (unsigned char __near *)updater.expected;
	configHash.w = (unsigned long __near *)updater.shaW;
	configHash.block = (unsigned char __near *)updater.shaBlock;
	configHash.digest = updater.hash;
	result0 = configCheck(0);
	result1 = configCheck(1);
	if (result0 == 0 && result1 == 0) {
		newer = configNewerVersion();
		if (newer == 2) {
			config.bank = configNewerCount();
		} else {
			result0 = configMerge(newer, newer == 0 ? 1 : 0);
			if (result0 != 0) {
			}
			config.bank = newer;
		}
	} else {
		config.bank = result0 == 0 ? 0 : 1;
	}
	if (configBanks[config.bank].version < CONFIG_VERSION) {
		result0 = configMerge(2, config.bank);
		if (result0 != 0) {
		}
	}
	config.ready = 1;
	config.changed = 0;
	return 0;
}

struct ConfigBlock *configGet(void)
{
	return &configBanks[config.bank].block;
}

/* the version and the count of the bank used, then the other's */
unsigned char configGetVersions(struct ConfigVersions *versions)
{
	unsigned char bank;

	bank = config.bank;
	versions->version = configBanks[bank].version;
	versions->count = configBanks[bank].count;
	bank = bank == 0 ? 1 : 0;
	versions->otherVersion = configBanks[bank].version;
	versions->otherCount = configBanks[bank].count;
	return 0;
}

unsigned char configRead(unsigned int offset, unsigned char size, unsigned char *data)
{
	if (offset + size > CONFIG_SIZE)
		return 0x3c;
	if (size > 0x20)
		return 0x3c;
	memcpy(data, (unsigned char *)configGet() + offset, size);
	return 0;
}

unsigned char configWrite(unsigned int offset, unsigned char size, unsigned char *data)
{
	struct ConfigBank *bank;

	if (config.state != CONFIG_OPEN)
		return 2;
	if (offset + size > CONFIG_SIZE)
		return 0x3c;
	if (size > 0x20)
		return 0x3c;
	bank = &configBuffer;
	memcpy((unsigned char *)&bank->block + offset, data, size);
	return 0;
}

/* copies the bank used for the host to change */
unsigned char configOpen(void)
{
	if (stateGet() == STATE_UPDATE_ANSWER)
		return 0x22;
	if (config.state == CONFIG_COMMITTING)
		return 2;
	configHash.unhashed = 0;
	configRecords.count = 0;
	memcpy(&configBuffer, &configBanks[config.bank], sizeof(configBuffer));
	config.state = CONFIG_OPEN;
	return 0;
}

/* writes the open copy to the other bank, which becomes the one used */
unsigned char configCommit(void)
{
	unsigned char magic[4] = "Conf";
	unsigned char *p;
	unsigned char result;
	unsigned long i;
	unsigned long sum;
	struct ConfigBank *bank;

	sum = 0;
	if (config.state != CONFIG_OPEN)
		return 2;
	if (configHash.unhashed != 0)
		return 6;
	if (configRecords.count != 0) {
		result = configApplyRecords(configRecords.buffer, configRecords.count, 0);
		configRecords.count = 0;
		if (result != 0)
			return result;
	}
	config.state = CONFIG_COMMITTING;
	bank = &configBuffer;
	memcpy(&bank->magic[0], magic, sizeof(magic));
	bank->state = CONFIG_WRITTEN;
	bank->size = CONFIG_SIZE;
	if (bank->version == 0 || bank->version == 0xffff)
		bank->version = 1;
	bank->count++;
	p = (unsigned char *)&bank->block;
	for (i = 0; i < CONFIG_SIZE; i++, p++)
		sum += *p;
	bank->checksum = ~sum;
	i = (unsigned long)&configBanks[config.bank == 0 ? 1 : 0];
	result = configFlashWrite(i);
	if (result == 0) {
		config.bank = config.bank == 0 ? 1 : 0;
		config.changed = 1;
	}
	config.state = CONFIG_OPEN;
	return result;
}

/* records for the open copy (check 0), or to compare with the bank used (check 1), from
   offset 0 on: hashed and applied 64 bytes at a time */
unsigned char configWriteRecords(unsigned int offset, unsigned char size, unsigned char *data,
				 unsigned char check)
{
	unsigned char result;
	unsigned char chunk;

	result = 0;
	if (stateGet() == STATE_UPDATE_ANSWER)
		return 0x22;
	if (offset == 0) {
		configRecords.count = 0;
		configHash.unhashed = 0;
		if ((unsigned char)SHA1Reset((SHA1Context __near *)&configHash.context,
					     configHash.block, configHash.w) != 0)
			return 0x20;
	}
	while (size != 0 && result == 0) {
		chunk = SHA1BlockSize - configRecords.count > size ?
			size : SHA1BlockSize - configRecords.count;
		memcpy(&configRecords.buffer[configRecords.count], data, chunk);
		configRecords.count += chunk;
		data += chunk;
		if (configRecords.count == SHA1BlockSize) {
			result = configApplyRecords(configRecords.buffer, configRecords.count, check);
			configRecords.count = 0;
		}
		size -= chunk;
	}
	return result;
}

/* each record its length, its type and its bytes: a write's an offset and data, a digest's
   the SHA-1 of the records before it, which the writes must end with */
static char configApplyRecords(unsigned char *records, unsigned int length,
			       unsigned char check)
{
	struct ConfigRecord *record;
	struct ConfigBank *bank;
	unsigned int offset;
	unsigned char size;
	unsigned char result;
	int left;

	if (check) {
		bank = (struct ConfigBank *)&configBanks[config.bank];
	} else {
		if (config.state != CONFIG_OPEN)
			return 2;
		bank = &configBuffer;
	}
	record = (struct ConfigRecord *)records;
	left = length;
	while (left > 0) {
		if (record->length != 0) {
			switch (record->type) {
			case CONFIG_RECORD_WRITE:
				if (record->length >= 3) {
					offset = (unsigned int)record->body.write.offsetHigh << 8 |
						 record->body.write.offsetLow;
					size = record->length - 3;
					if (offset + size > CONFIG_SIZE)
						return 6;
					if (check) {
						if (memcmp((unsigned char *)&bank->block + offset,
							   record->body.write.data, size) != 0)
							return 0x21;
					} else {
						memcpy((unsigned char *)&bank->block + offset,
						       record->body.write.data, size);
					}
					configHash.unhashed = 1;
				}
				break;
			case CONFIG_RECORD_TEXT:
				break;
			case CONFIG_RECORD_HASH:
				configHash.unhashed = 0;
				if (record->length != 1 + SHA1HashSize)
					return 6;
				result = (unsigned char)SHA1Input(
					(SHA1Context __near *)&configHash.context, records, length - left);
				if (result != 0)
					return 0x20;
				result = (unsigned char)SHA1Result(
					(SHA1Context __near *)&configHash.context,
					(unsigned char __near *)configHash.digest);
				if (result != 0)
					return 0x20;
				if (memcmp(configHash.digest, record->body.digest, SHA1HashSize) != 0)
					return 6;
				break;
			}
		}
		left -= (unsigned char)(record->length + 1);
		record = (struct ConfigRecord *)((unsigned char *)record +
						 (unsigned char)(record->length + 1));
	}
	if (configHash.unhashed) {
		result = (unsigned char)SHA1Input((SHA1Context __near *)&configHash.context,
						  records, length);
		if (result != 0)
			return 0x20;
	}
	return left == 0 ? 0 : 6;
}

unsigned char configClose(void)
{
	if (config.state == CONFIG_OPEN) {
	}
	config.state = CONFIG_CLOSED;
	return 0;
}

/* before an update: bank 1 takes bank 0's settings, so the update may write bank 0 */
unsigned char configPrepareUpdate(void)
{
	unsigned char result;

	if (config.bank != 0)
		return 0;
	memcpy(&configBuffer, &configBanks[0], sizeof(configBuffer));
	result = configFlashWrite((unsigned long)&configBanks[1]);
	return result;
}

/* the flash segment (1 KiB) of the bank not used, which an update writes */
unsigned int configUpdateSegment(void)
{
	unsigned long address;

	address = (unsigned long)&configBanks[config.bank != 0 ? 0 : 1];
	return address >> 10;
}

/* after an update: its bank, merged with what the bank used keeps, becomes the one used */
unsigned char configMergeUpdate(void)
{
	unsigned char other;
	unsigned char result;

	other = config.bank != 0 ? 0 : 1;
	result = configMerge(other, config.bank);
	config.bank = other;
	return result;
}

unsigned char configTakeChanged(void)
{
	unsigned char changed;

	changed = config.changed;
	config.changed = 0;
	return changed;
}

/* the banks, then the defaults */
static const struct ConfigBank *const configTable[3] = {
	&configBanks[0], &configBanks[1], &configDefault
};

/* the block of one bank (from: 0 or 1, or 2 the defaults), with what another keeps (to: the
   sticks' calibration, the board, ...), written over to: when from is a new layout's defaults
   and to has been written since its own */
static unsigned char configMerge(unsigned char from, unsigned char to)
{
	struct ConfigBank *copy;
	const struct ConfigBank *kept;
	unsigned char result;

	if (to >= 2 || from >= 3)
		return 1;
	if (configTable[to]->count == 1)
		return 0;
	if (configTable[from]->count != 1)
		return 0;
	if (config.state == CONFIG_CLOSED) {
	}
	config.state = CONFIG_OPEN;
	memcpy(&configBuffer, configTable[from], sizeof(configBuffer));
	kept = configTable[to];
	copy = &configBuffer;
	copy->block.analogCalibration = kept->block.analogCalibration;
	copy->block.board.value = kept->block.board.value;
	copy->block.start.value = kept->block.start.value;
	copy->block.keep.value = kept->block.keep.value;
	if (copy->block.keep.bits.pressTimesSet) {
		copy->block.pressTime1 = kept->block.pressTime1;
		copy->block.pressTime2 = kept->block.pressTime2;
	}
	copy->block.hardwareInfoSelect = kept->block.hardwareInfoSelect;
	copy->block.state.value = kept->block.state.value;
	if (kept->block.start.bits.chargeLedOff)
		copy->block.state.bits.chargeLedOff = kept->block.start.bits.chargeLedOff;
	copy->block.offTransitionBlock = kept->block.offTransitionBlock;
	config.bank = to;
	result = configCommit();
	return result;
}

/* erases the bank at address and writes the open copy there, three times at most */
unsigned char configFlashWrite(unsigned long address)
{
	unsigned int i;
	unsigned char tries;
	unsigned char result;

	tries = 3;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == 0x1f)
		result = FSL_Init(fslDataBuffer);
	if (result == 0) {
		do {
			result = FSL_ModeCheck();
			if (result == 0) {
				do
					result = FSL_BlankCheck((unsigned int)(address >> 10));
				while (result == 0x1f);
				if (result == 0x1b) {
					do
						result = FSL_Erase((unsigned int)(address >> 10));
					while (result == 0x1f);
				}
			}
			if (result == 0) {
				for (i = 0; i < 0x100; i += 0x40) {
					memcpy(fslDataBuffer, (unsigned char *)&configBuffer + i * 4, 0x100);
					do
						result = FSL_Write(address + i * 4, 0x40);
					while (result == 0x1f);
					if (result != 0)
						break;
				}
			}
			if (result == 0) {
				do
					result = FSL_IVerify((unsigned int)(address >> 10));
				while (result == 0x1f);
			}
		} while (tries-- & (result != 0));
	}
	FSL_Close();
	return result;
}

/* the open copy set to the defaults and committed */
unsigned char configRestoreDefaults(void)
{
	unsigned char result;
	unsigned char size;
	unsigned int offset;
	unsigned int left;

	result = configOpen();
	if (result != 0)
		return result;
	offset = 0;
	left = CONFIG_SIZE;
	while (left != 0) {
		size = (unsigned char)(left > 0x20 ? 0x20 : left);
		configWrite(offset, size, (unsigned char *)&configDefault.block + offset);
		offset += size;
		left = CONFIG_SIZE - offset;
	}
	result = configCommit();
	if (result != 0)
		return result;
	result = configClose();
	return result;
}

/* 0 when the bank is valid: the magic, the size, a version and a count, the state, and the
   checksum (but for the defaults, count 1) */
static unsigned char configCheck(unsigned char bank)
{
	unsigned char magic[4] = "Conf";
	const struct ConfigBank *b;
	unsigned long sum;
	unsigned int i;
	const unsigned char *p;

	sum = 0;
	if (bank >= 2)
		return 1;
	b = &configBanks[bank];
	if (memcmp(b->magic + 0, magic, sizeof(magic)) != 0)
		return 2;
	if (b->size != CONFIG_SIZE)
		return 3;
	if (b->version == 0xffff)
		return 4;
	if (b->count == 0xffff)
		return 5;
	switch (b->state) {
	case 0x5050:
	case CONFIG_WRITTEN:
		break;
	default:
		return 6;
	}
	if (b->count != 1) {
		p = (const unsigned char *)&b->block;
		for (i = 0; i < CONFIG_SIZE; i++, p++)
			sum += *p;
		sum = ~sum;
		if (b->checksum != sum)
			return 7;
	}
	return 0;
}

/* the bank with the newer layout, 2 when both have the same */
static unsigned char configNewerVersion(void)
{
	if (configBanks[0].version == configBanks[1].version)
		return 2;
	if (configBanks[1].version < configBanks[0].version)
		return 0;
	return 1;
}

/* the bank written last */
static unsigned char configNewerCount(void)
{
	if (configBanks[0].count >= configBanks[1].count)
		return 0;
	return 1;
}
