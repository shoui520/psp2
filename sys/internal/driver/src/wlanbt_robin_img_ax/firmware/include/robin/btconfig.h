/*
 * The Bluetooth configuration store (bt/btconfig.c, 0x4e1c0..0x4e716): two
 * records the host's EEPROM may hold, found in its record list, or given by
 * the host: the address record (type 55: the controller's settings and its
 * BD_ADDR) and the key record (type 56: a key and the stored link keys).
 * Each record starts with its type, a checksum that makes its bytes add up
 * to 1, and its length; the host reads and writes items of them by number.
 *
 * Marvell published no source; names are inferred from use.
 */
#ifndef ROBIN_BTCONFIG_H
#define ROBIN_BTCONFIG_H

#include <stdint.h>

#define BT_CONFIG_ADDRESS_RECORD	55
#define BT_CONFIG_KEY_RECORD		56
#define BT_CONFIG_LIST_HEAD		76	/* the EEPROM word that points at the record list */
#define BT_CONFIG_CHECKSUM		1	/* what a record's bytes add up to */

/* The items the host reads and writes (btSbcFilterState(), btSbcHandleContext()). */
#define BT_CONFIG_ITEM_BAUD_RATE	7
#define BT_CONFIG_ITEM_KEY		128
#define BT_CONFIG_ITEM_HIGH_NIBBLE	252
#define BT_CONFIG_ITEM_LOW_NIBBLE	253
#define BT_CONFIG_ITEM_ADDRESS		254

#define BT_LINK_KEY_SLOTS		3
#define BT_LINK_KEY_ENTRY		22	/* a BD_ADDR and a 16-byte link key */

struct BtConfigHeader {
	uint8_t type;
	uint8_t checksum;
	uint16_t length;
};

/* The address record; items 1 to 6 are single bytes whose meanings are not known. */
struct BtConfigAddressRecord {
	struct BtConfigHeader header;
	uint8_t reserved4[5];
	uint8_t item1;
	uint8_t item2;			/* 4 */
	uint8_t item3;
	uint8_t item4;			/* 0 */
	uint8_t reserved13;
	int8_t item5;			/* -70 */
	int8_t item6;			/* -50 */
	uint32_t baudRate;		/* of the host interface: 115200 */
	uint8_t address[6];		/* the BD_ADDR, most significant byte first */
	uint8_t nibbles;		/* items 252 and 253, each less one; 0xf0 */
	uint8_t reserved27;
};

_Static_assert(sizeof(struct BtConfigAddressRecord) == 28, "Bluetooth address record");

struct BtConfigKeyRecord {
	struct BtConfigHeader header;
	uint8_t reserved4[4];
	uint8_t key[16];
	uint16_t linkKeyMask;		/* the slots that hold a link key */
	uint8_t linkKeys[BT_LINK_KEY_SLOTS][BT_LINK_KEY_ENTRY];
};

_Static_assert(sizeof(struct BtConfigKeyRecord) == 92, "Bluetooth key record");

struct BtConfigStore {
	uint32_t addressOffset;		/* the records' places in the EEPROM */
	uint32_t keyOffset;
	uint8_t addressFound;
	uint8_t keyFound;
	uint8_t reserved[2];
	struct BtConfigAddressRecord address;
	struct BtConfigKeyRecord keys;
};

_Static_assert(sizeof(struct BtConfigStore) == 132, "Bluetooth configuration store");

/* btConfigAddressSource: where the BD_ADDR came from. */
#define BT_ADDRESS_DEFAULT	0
#define BT_ADDRESS_STORED	1	/* the EEPROM or the host */
#define BT_ADDRESS_DERIVED	2	/* btSbcAdjustState() */

/* btConfigOrigin: where the address record came from. */
#define BT_CONFIG_FROM_EEPROM	1
#define BT_CONFIG_FROM_HOST	2

extern struct BtConfigStore btConfigStore;		/* 0x5ac90 */
extern uint8_t btConfigAddressSource;			/* 0x55460 */
extern uint8_t btConfigOrigin;				/* 0x55461 */
extern uint32_t btConfigAllocFailures;			/* 0x55464 */
/* The item numbers of the link key slots (ROM data). */
extern uint8_t btLinkKeyFirstItem;			/* DTCM 0x400f79e */
extern uint8_t btLinkKeyLastItem;			/* DTCM 0x400f79f */

int btSbcVerifyState(unsigned int type, struct BtConfigHeader *record, unsigned int length,
		     uint32_t *offset);						/* 0x4e1c0 */
void btSbcSelectState(struct BtConfigHeader *record, unsigned int length, uint32_t offset);	/* 0x4e272 */
void btSbcAdjustState(uint32_t low, uint16_t high);				/* 0x4e2bc */
void btSbcRecordState(void);							/* 0x4e2e4 */
int btSbcFilterState(int item, uint8_t *value);					/* 0x4e35e */
int btSbcHandleContext(unsigned int item, const uint8_t *value, int set);	/* 0x4e422 */
int btSbcProcessContext(unsigned int item);					/* 0x4e562 */
int btSbcUpdateContext(const uint8_t *records);					/* 0x4e582 */
int btSbcConfigureContext(const uint8_t *request, uint8_t *length, uint8_t *reply);	/* 0x4e63e */
void btSbcCheckContext(void);							/* 0x4e66c */
int btSbcValidateContext(const uint8_t *record, unsigned int length);		/* 0x4e6b6 */
uint8_t *btSbcPrepareContext(uint16_t size);					/* 0x4e6e0 */
int btSbcDispatchContext(uint8_t **data);					/* 0x4e716 */
void btSbcParseContext(uint16_t length, uintptr_t unused, uint8_t **data);	/* 0x4e728 */
uint16_t btSbcBuildContext(uint8_t *data);					/* 0x4e730 */

#endif /* ROBIN_BTCONFIG_H */
