/*
 * The settings, as the updater reads them: two banks in the flash (configBanks), each a header
 * and a ConfigBlock; the valid one written last is used (configGet). The updater neither
 * writes nor merges them.
 */
#include <string.h>

#include "ernie.h"
#include "config.h"
#include "updater.h"

struct Config {
	unsigned char	ready;
	unsigned char	bank;			/* the bank used */
};

/* the banks, in the updater's part of the flash */
static const struct ConfigBank *const __far configBanks = (const struct ConfigBank *)0x3b800;

static struct Config config;

static unsigned char configCheck(unsigned char bank);
static unsigned char configNewerVersion(void);
static unsigned char configNewerCount(void);

/* picks the bank to use */
unsigned char configStorageInit(void)
{
	unsigned char newer;
	unsigned char result0;
	unsigned char result1;

	result0 = configCheck(0);
	result1 = configCheck(1);
	if (result0 == 0 && result1 == 0) {
		newer = configNewerVersion();
		if (newer == 2)
			config.bank = configNewerCount();
		else
			config.bank = newer;
	} else {
		config.bank = result0 == 0 ? 0 : 1;
	}
	config.ready = 1;
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
