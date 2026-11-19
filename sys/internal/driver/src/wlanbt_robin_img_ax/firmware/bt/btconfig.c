/*
 * The Bluetooth configuration store (include/robin/btconfig.h): finds the
 * address and key records in the host's EEPROM, takes them from the host,
 * answers and changes their items, and writes changed items back.
 */
#include <stdint.h>
#include "robin/btconfig.h"
#include "robin/btpool.h"
#include "robin/btrom.h"
#include "robin/nvram.h"
#include "robin/rom.h"
#include "robin/session.h"
#include "robin/sys.h"

struct BtConfigStore btConfigStore __attribute__((section(".itcm.bss")));
uint8_t btConfigAddressSource __attribute__((section(".itcm.rw"))) = BT_ADDRESS_DEFAULT;
uint8_t btConfigOrigin __attribute__((section(".itcm.rw")));
uint32_t btConfigAllocFailures __attribute__((section(".itcm.rw")));

/*
 * Finds the record of a type in the EEPROM's record list and reads it:
 * 1 when it is there and its checksum holds, with its place in *offset.
 */
int btSbcVerifyState(unsigned int type, struct BtConfigHeader *record, unsigned int length,
		     uint32_t *offset)
{
	uint32_t node;
	uint32_t entry;
	uint32_t word;
	uint32_t *data;
	uint16_t found = 0;
	int valid = 0;
	unsigned int words;
	unsigned int i;
	uint8_t sum;

	nvramOpen();
	nvramRead(0, &node);
	if (node == NVRAM_MAGIC) {
		nvramRead(BT_CONFIG_LIST_HEAD, &node);
		while (node != 0xffffffff) {
			nvramRead(node + 4, &entry);
			if (entry != 0xffffffff) {
				nvramRead(entry, &word);
				if ((uint8_t)word == type) {
					found = entry;
					break;
				}
			}
			node = entry;
		}
	}
	nvramClose();
	if (found != 0) {
		data = (uint32_t *)record;
		nvramOpen();
		words = length >> 2;
		for (i = 0; i < words; i++)
			nvramRead(found + 4 * i, data++);
		nvramClose();
		sum = 0;
		for (i = 0; i < record->length; i++)
			sum += ((uint8_t *)record)[i];
		if (sum == BT_CONFIG_CHECKSUM) {
			valid = 1;
			*offset = found;
		}
	}
	return valid;
}

/* Sets a record's checksum and writes its header word back to the EEPROM. */
void btSbcSelectState(struct BtConfigHeader *record, unsigned int length, uint32_t offset)
{
	uint8_t sum = 0;
	unsigned int i;

	record->checksum = 0;
	for (i = 0; i < length; i++)
		sum += ((uint8_t *)record)[i];
	record->checksum = BT_CONFIG_CHECKSUM - sum;
	nvramOpen();
	nvramControl(NVRAM_CONTROL_WRITE_ENABLE, 0, NULL);
	nvramWrite(offset, *(uint32_t *)record);
	nvramControl(NVRAM_CONTROL_WRITE_DISABLE, 0, NULL);
	nvramClose();
}

/* Makes the BD_ADDR from two values, unless one was stored. */
void btSbcAdjustState(uint32_t low, uint32_t high)
{
	uint8_t *address = btConfigStore.address.address;

	if (btConfigAddressSource != BT_ADDRESS_STORED) {
		address[1] = high;
		address[2] = low >> 24;
		address[3] = low >> 16;
		address[4] = low >> 8;
		address[5] = low;
		address[0] = (high >> 8) + 1;
		btConfigAddressSource = BT_ADDRESS_DERIVED;
	}
}

/* Sets the defaults, then reads both records from the EEPROM. */
void btSbcRecordState(void)
{
	uint8_t *address = btConfigStore.address.address;
	uint32_t saved[2];
	int found;

	btSbcCheckContext();
	saved[1] = ((uint32_t *)address)[1];
	saved[0] = ((uint32_t *)address)[0];
	found = btSbcVerifyState(BT_CONFIG_ADDRESS_RECORD, &btConfigStore.address.header,
				 sizeof(btConfigStore.address), &btConfigStore.addressOffset);
	btConfigStore.addressFound = found;
	if (found != 0) {
		btConfigOrigin = BT_CONFIG_FROM_EEPROM;
		if (address[0] == 0 && address[1] == 0 && address[2] == 0 && address[3] == 0 &&
		    address[4] == 0 && address[5] == 0)
			__aeabi_memcpy4(address, saved, 6);
		else
			btConfigAddressSource = BT_ADDRESS_STORED;
	}
	btConfigStore.keyFound = btSbcVerifyState(BT_CONFIG_KEY_RECORD, &btConfigStore.keys.header,
						  sizeof(btConfigStore.keys), &btConfigStore.keyOffset);
}

/* Answers an item: its length in *value, or 0 when there is none. */
int btSbcFilterState(int item, uint8_t *value, unsigned int length)
{
	struct BtConfigAddressRecord *record = &btConfigStore.address;
	unsigned int slot;

	(void)length;		/* the callers' buffer size; not checked */
	switch (item) {
	case 1:
		*value = record->item1;
		return 1;
	case 2:
		*value = record->item2;
		return 1;
	case 3:
		*value = record->item3;
		return 1;
	case 4:
		*value = record->item4;
		return 1;
	case 5:
		*value = record->item5;
		return 1;
	case 6:
		*value = record->item6;
		return 1;
	case BT_CONFIG_ITEM_BAUD_RATE:
		__aeabi_memcpy(value, &record->baudRate, 4);
		return 4;
	case BT_CONFIG_ITEM_KEY:
		__aeabi_memcpy(value, btConfigStore.keys.key, 16);
		return 16;
	case BT_CONFIG_ITEM_HIGH_NIBBLE:
		*value = (record->nibbles >> 4) + 1;
		return 1;
	case BT_CONFIG_ITEM_LOW_NIBBLE:
		*value = (record->nibbles & 0xf) + 1;
		return 1;
	case BT_CONFIG_ITEM_ADDRESS:
		__aeabi_memcpy(value, record->address, 6);
		return 6;
	default:
		if ((unsigned int)item >= btLinkKeyFirstItem && (unsigned int)item <= btLinkKeyLastItem) {
			slot = item - btLinkKeyFirstItem;
			if ((1u << slot) & btConfigStore.keys.linkKeyMask) {
				__aeabi_memcpy(value, btConfigStore.keys.linkKeys[slot], BT_LINK_KEY_ENTRY);
				return BT_LINK_KEY_ENTRY;
			}
		}
		return 0;
	}
}

/*
 * Changes an item: the key and the link keys (set, or cleared when set is 0)
 * are also written to the key record in the EEPROM, when it has one.
 */
int btSbcHandleContext(unsigned int item, const uint8_t *value, int set)
{
	struct BtConfigKeyRecord *keys = &btConfigStore.keys;
	uint32_t *word;
	unsigned int place;
	unsigned int slot;
	unsigned int i;
	uint8_t *entry;

	if (item == BT_CONFIG_ITEM_KEY) {
		place = 8;
		word = (uint32_t *)keys->key;
		__aeabi_memcpy(keys->key, value, 16);
		if (btConfigStore.keyFound != 0) {
			nvramOpen();
			for (i = 0; i < 4; i++) {
				nvramControl(NVRAM_CONTROL_WRITE_ENABLE, 0, NULL);
				nvramWrite(btConfigStore.keyOffset + place, *word);
				nvramControl(NVRAM_CONTROL_WRITE_DISABLE, 0, NULL);
				place += 4;
				word++;
			}
			nvramClose();
			btSbcSelectState(&keys->header, sizeof(*keys), btConfigStore.keyOffset);
		}
		return 16;
	}
	if (item == BT_CONFIG_ITEM_ADDRESS) {
		__aeabi_memcpy(btConfigStore.address.address, value, 6);
		return 6;
	}
	if (item < btLinkKeyFirstItem || item > btLinkKeyLastItem)
		return 0;

	slot = item - btLinkKeyFirstItem;
	entry = keys->linkKeys[slot];
	if (set != 0) {
		__aeabi_memcpy(entry, value, BT_LINK_KEY_ENTRY);
		keys->linkKeyMask |= 1u << (item - btLinkKeyFirstItem);
	} else {
		__aeabi_memclr(entry, BT_LINK_KEY_ENTRY);
		keys->linkKeyMask &= ~(1u << (item - btLinkKeyFirstItem));
	}
	if (btConfigStore.keyFound != 0) {
		nvramOpen();
		nvramControl(NVRAM_CONTROL_WRITE_ENABLE, 0, NULL);
		nvramWrite(btConfigStore.keyOffset + 24, keys->linkKeyMask);
		nvramControl(NVRAM_CONTROL_WRITE_DISABLE, 0, NULL);
		/* The words that hold the entry; the mask's word too when it starts one. */
		place = BT_LINK_KEY_ENTRY * slot + 26;
		if (item & 1)
			place -= 2;
		word = (uint32_t *)((uint8_t *)keys + place);
		for (i = 0; i < 6; i++) {
			nvramControl(NVRAM_CONTROL_WRITE_ENABLE, 0, NULL);
			nvramWrite(btConfigStore.keyOffset + place, *word);
			nvramControl(NVRAM_CONTROL_WRITE_DISABLE, 0, NULL);
			place += 4;
			word++;
		}
		nvramClose();
		btSbcSelectState(&keys->header, sizeof(*keys), btConfigStore.keyOffset);
	}
	return set;
}

/* Clears a link key slot. */
int btSbcProcessContext(unsigned int item)
{
	if (item >= btLinkKeyFirstItem && item <= btLinkKeyLastItem)
		btSbcHandleContext(item, NULL, 0);
	return 0;
}

/*
 * Takes records from the host: a list (its length in byte 3) of entries, each
 * a byte that says the checksum need not be checked and the record itself.
 * 0 while each record was taken, else -1.
 */
int btSbcUpdateContext(const uint8_t *records)
{
	int result = -1;
	unsigned int total = records[3];
	unsigned int trusted;
	unsigned int type;
	unsigned int length;
	const uint8_t *entry;
	uint16_t offset;

	if (total != 0) {
		for (offset = 0; offset < total; offset = offset + length) {
			entry = records + offset;
			trusted = entry[2];
			type = entry[4];
			length = entry[6] | entry[7] << 8;
			osa_LockAcquire();
			result = -1;
			if (type == BT_CONFIG_ADDRESS_RECORD) {
				if (length == sizeof(btConfigStore.address) &&
				    (trusted != 0 || btSbcValidateContext(entry + 4, length))) {
					__aeabi_memcpy(&btConfigStore.address, entry + 4, length);
					result = 0;
					btConfigOrigin = BT_CONFIG_FROM_HOST;
					btConfigAddressSource = BT_ADDRESS_STORED;
				}
			} else if (type == BT_CONFIG_KEY_RECORD && length == sizeof(btConfigStore.keys) &&
				   (trusted != 0 || btSbcValidateContext(entry + 4, length))) {
				__aeabi_memcpy(&btConfigStore.keys, entry + 4, length);
				result = 0;
			}
			osa_LockRelease();
			if (result != 0)
				break;
		}
	}
	return result;
}

/* Answers the host's read of the address record: where it came from, then the record. */
int btSbcConfigureContext(const uint8_t *request, uint8_t *length, uint8_t *reply)
{
	if (request[0] != BT_CONFIG_ADDRESS_RECORD)
		return -1;
	reply[0] = btConfigOrigin;
	__aeabi_memcpy(reply + 1, &btConfigStore.address, sizeof(btConfigStore.address));
	*length = 1 + sizeof(btConfigStore.address);
	return 0;
}

/* The records' defaults. */
void btSbcCheckContext(void)
{
	struct BtConfigAddressRecord *record = &btConfigStore.address;

	record->header.length = sizeof(*record);
	record->item2 = 4;
	record->item4 = 0;
	record->item5 = -70;
	record->item6 = -50;
	record->baudRate = 115200;
	if (btConfigAddressSource == BT_ADDRESS_DEFAULT)
		__aeabi_memset4(record->address, 6, 0x52);
	record->nibbles = 0xf0;
	btConfigOrigin = 0;
	__aeabi_memclr4(&btConfigStore.keys, sizeof(btConfigStore.keys));
	btConfigStore.keys.header.length = sizeof(btConfigStore.keys);
}

/* 1 when a record's bytes add up to its checksum value. */
__attribute__((noinline)) int btSbcValidateContext(const uint8_t *record, unsigned int length)
{
	unsigned int i;
	uint8_t sum = 0;

	for (i = 0; i < length; i++)
		sum += record[i];
	if (sum == BT_CONFIG_CHECKSUM)
		return 1;
	return 0;
}

/*
 * A reply buffer of size bytes; its data follow the OsaBuffer header, and its
 * length is kept in the header (btSbcParseContext(), btSbcBuildContext()).
 */
uint8_t *btSbcPrepareContext(uint16_t size)
{
	OsaBuffer *buffer;

	buffer = osa_BufferAlloc(&btTablePool, (uint16_t)(size + sizeof(OsaBuffer)), sysInIsr() != 0 ? 0 : ~0u);
	if (buffer != NULL)
		return (uint8_t *)buffer + buffer->dataOffset;
	btConfigAllocFailures++;
	return NULL;
}

/* Frees a reply buffer by its data. */
int btSbcDispatchContext(uint8_t **data)
{
	osa_BufferFree((OsaBuffer *)(*data - sizeof(OsaBuffer)));
	*data = NULL;
	return 0;
}

void btSbcParseContext(uint16_t length, uintptr_t unused, uint8_t **data)
{
	(void)unused;
	((OsaBuffer *)(*data - sizeof(OsaBuffer)))->replyLength = length;
}

uint16_t btSbcBuildContext(uint8_t *data)
{
	return ((OsaBuffer *)(data - sizeof(OsaBuffer)))->replyLength;
}
