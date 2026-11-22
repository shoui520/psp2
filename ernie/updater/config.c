/*
 * The settings, as the updater reads them: two banks in the flash (configBanks), each a header
 * and a ConfigBlock; the valid one written last is used (configGet). The updater neither
 * writes nor merges them.
 */
#include <string.h>

#include "ernie.h"
#include "config.h"
#include "sha1.h"
#include "updater.h"

struct Config {
	unsigned char	ready;
	unsigned char	bank;			/* the bank used */
#if FW_SINCE(0x0100060D)
	unsigned char	state;			/* the application's; never set here */
	unsigned char	changed;		/* cleared at the start; nothing sets it */
#endif
#if SOURCE_0103
	unsigned char	reserved[0x2e];		/* the PS TV's: 0x2e bytes nothing reads */
#endif
};

/* the banks, in the updater's part of the flash */
#if CHIP_RL78
static const struct ConfigBank *const __far configBanks = (const struct ConfigBank *)0x5f800;
#else
static const struct ConfigBank *const __far configBanks = (const struct ConfigBank *)0x3b800;
#endif

/* the slim's 0x0105: a bank's magic kept beside them, and 9 for banks of the same version */
#if BOARD_USS1001 && SOURCE_0104
static const unsigned char __far configMagic[4] = "Conf";
#define CONFIG_SAME_VERSION	9
#else
#define CONFIG_SAME_VERSION	2
#endif

static struct Config config;
#if FW_SINCE(0x0100060D)
static struct ConfigRecords configRecords;
#endif

static unsigned char configCheck(unsigned char bank);
static unsigned char configNewerVersion(void);
static unsigned char configNewerCount(void);

/* picks the bank to use */
unsigned char configStorageInit(void)
{
	unsigned char newer;
	unsigned char result0;
	unsigned char result1;

#if FW_SINCE(0x0100060D)
	memset(&configRecords, 0, sizeof(configRecords));
#endif
	result0 = configCheck(0);
	result1 = configCheck(1);
	if (result0 == 0 && result1 == 0) {
		newer = configNewerVersion();
		if (newer == CONFIG_SAME_VERSION)
			config.bank = configNewerCount();
		else
			config.bank = newer;
	} else {
		config.bank = result0 == 0 ? 0 : 1;
	}
	config.ready = 1;
#if FW_SINCE(0x0100060D)
	config.changed = 0;
#endif
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
#if !(BOARD_USS1001 && SOURCE_0104)
	unsigned char magic[4] = "Conf";
#endif
	const struct ConfigBank *b;
	unsigned long sum;
	unsigned int i;
	const unsigned char *p;

	sum = 0;
	if (bank >= 2)
		return 1;
	b = &configBanks[bank];
#if BOARD_USS1001 && SOURCE_0104
	if (memcmp(b->magic + 0, configMagic, sizeof(configMagic)) != 0)
		return 2;
#else
	if (memcmp(b->magic + 0, magic, sizeof(magic)) != 0)
		return 2;
#endif
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

/* the bank with the newer layout, CONFIG_SAME_VERSION when both have the same */
static unsigned char configNewerVersion(void)
{
	if (configBanks[0].version == configBanks[1].version)
		return CONFIG_SAME_VERSION;
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
