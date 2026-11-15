/*
 * The calibration data's sources and its chain (SQRAM 0xc0009c78..
 * 0xc000a290): the data comes from the host (HostCmd_CMD_CFG_DATA) or from
 * the NVRAM, is kept in calDataBuffer, and is walked annex by annex from the
 * main annex (type 12 or 13, macEventState), each annex type going to its
 * loader in sqram/caldata.c. Loading it also sets the station's MAC address.
 * The filename and the names are inferred.
 */
#include <stdint.h>
#include "robin/btconfig.h"
#include "robin/calibration.h"
#include "robin/event.h"
#include "robin/hostcmd.h"
#include "robin/macdefaults.h"
#include "robin/mactx.h"
#include "robin/nvram.h"
#include "robin/peerrate.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/util.h"
#include "caldata.h"

#define CAL_DATA_END			0xffffffffu
#define CAL_DATA_HEADER_SIZE		64

/* The calibration flags of calDataFlagsGet(). */
#define CAL_DATA_FLAG_PAYLOAD		0x02
#define CAL_DATA_FLAG_PATH		0x04

/* The annex types the chain carries. */
enum {
	CAL_DATA_ANNEX_POWER = 14,
	CAL_DATA_ANNEX_POWER_LEVELS = 16,
	CAL_DATA_ANNEX_18 = 18,
	CAL_DATA_ANNEX_31 = 31,
	CAL_DATA_ANNEX_POWER_PATH1 = 37,
	CAL_DATA_ANNEX_LIMITS = 42,
	CAL_DATA_ANNEX_31_PATH1 = 47,
	CAL_DATA_ANNEX_BAND0 = 49,
	CAL_DATA_ANNEX_VALUE2 = 52,
	CAL_DATA_ANNEX_VALUE1 = 53,
	CAL_DATA_ANNEX_VALUE0 = 54,
	CAL_DATA_ANNEX_CHANNELS = 60,
};

/* The NVRAM's layout: a magic word, the MAC address, and where the image is. */
#define CAL_NVRAM_MAGIC			0x38333058u	/* "X038" */
#define CAL_NVRAM_OFFSET_MAGIC		0
#define CAL_NVRAM_OFFSET_ADDRESS_LOW	12
#define CAL_NVRAM_OFFSET_ADDRESS_HIGH	16
#define CAL_NVRAM_OFFSET_IMAGE_LENGTH	72
#define CAL_NVRAM_OFFSET_IMAGE		76

#define MAC_ADDRESS_HIGH_WORD		REG32(0x8000a52c)

/* Words and halves written into byte arrays. */
typedef uint32_t __attribute__((may_alias)) CalDataWord;
typedef uint16_t __attribute__((may_alias)) CalDataHalf;

#define CAL_DATA_ANNEX42_PARAMS		5
#define CAL_DATA_ANNEX42_PATHS		2
#define CAL_DATA_ANNEX60_PATHS		1
#define CAL_DATA_ANNEX60_ENTRIES	6
#define CAL_DATA_ANNEX60_VALUES		4

/* Annex 60: each path's channel calibration values (macTxNotifyNotifyFrame()). */
struct CalDataAnnex60Entry {
	uint8_t header[4];
	uint16_t values[CAL_DATA_ANNEX60_VALUES];
};

struct CalDataAnnex60 {
	struct CalDataHeader header;
	uint8_t reserved8;
	uint8_t mode;
	uint8_t reserved10;
	uint8_t pathValue[2];
	uint8_t reserved13[3];
	struct CalDataAnnex60Entry entries[CAL_DATA_ANNEX60_ENTRIES];
};

_Static_assert(sizeof(struct CalDataAnnex60) == 88, "annex 60");

uint8_t calDataHeaderPresent __attribute__((section(".dtcm.rw")));
uint16_t calDataOffset __attribute__((section(".dtcm.rw")));
struct CalDataHostHeader calDataHeader __attribute__((section(".dtcm.rw")));
uint8_t calDataBuffer[CAL_DATA_BUFFER_SIZE] __attribute__((section(".dtcm.bss"), aligned(4)));

/* The flags the power annexes keep for a path (calDataFlagsHook). */
unsigned int calDataFlagsGet(int index)
{
	unsigned int flags;
	unsigned int path;

	if (macEventState.state != MAC_EVENT_STATE_LOADED)
		return 0;
	flags = ((macEventState.payload >> 6) & 1) << 1;
	if (index == 0)
		path = macEventState.pathFlags & 1;
	else
		path = (macEventState.pathFlags >> 4) & 1;
	return flags | path << 2;
}

/* The decoder reads the image from calDataBuffer. */
void calDataSourceInit(void)
{
	calDataImage = calDataBuffer;
	calDataImageOffset = &calDataOffset;
	calDataFlagsHook = calDataFlagsGet;
}

/* Annex 42's second half: each entry's four bytes for one path or both. */
int calDataLoadAnnex42Params(int fromHost, unsigned int offset)
{
	struct CalDataAnnex42 annex;
	union {
		uint32_t word;
		uint8_t bytes[4];
	} param;
	unsigned int length;
	unsigned int remaining;
	unsigned int entry;
	unsigned int path;

	__aeabi_memclr4(&annex, sizeof(annex));
	param.word = 0;
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	remaining = (uint16_t)(length - sizeof(annex.header));
	for (entry = 0; entry < CAL_DATA_ANNEX42_PARAMS; entry = (uint8_t)(entry + 1)) {
		const uint8_t *bytes = annex.entries[entry].bytes;
		unsigned int kind;

		if (remaining < sizeof(annex.entries[0]))
			break;
		param.bytes[0] = bytes[0];
		param.bytes[1] = bytes[3];
		param.bytes[2] = bytes[1];
		param.bytes[3] = bytes[4];
		kind = bytes[0] & 0xf;
		if (kind == CAL_DATA_ANNEX42_BAND_INDEX0) {
			macTxNotifyResolveFrame(calDataChannelNumber(0, 0), &param.word);
		} else if (kind == CAL_DATA_ANNEX42_BOTH) {
			for (path = 0; path < CAL_DATA_ANNEX42_PATHS; path = (uint8_t)(path + 1))
				macTxNotifyResolveFrame(path, &param.word);
			break;
		}
		remaining = (uint16_t)(remaining - sizeof(annex.entries[0]));
	}
	return 1;
}

/* Annex 60: each path's channel calibration values. */
int calDataLoadAnnex60(int fromHost, unsigned int offset)
{
	struct CalDataAnnex60 annex;
	struct {
		struct WlanCalibrationChannelValues values;
		uint16_t padding;
	} __attribute__((aligned(4))) record;
	int path;
	int value;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	if (!calDataChecksumOk((const uint8_t *)&annex,
			       annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex)))
		return 0;
	for (path = 0; path < CAL_DATA_ANNEX60_PATHS; path++) {
		const struct CalDataAnnex60Entry *entry = &annex.entries[path];
		unsigned int index;

		__aeabi_memclr4(&record, sizeof(record));
		if (path < 1)
			index = calDataChannelNumber(0, (uint8_t)path);
		else
			index = calDataChannelNumber(1, (uint8_t)(path - 1));
		record.values.header[4] = annex.mode;
		record.values.header[5] = annex.pathValue[path];
		record.values.header[0] = entry->header[0];
		record.values.header[1] = entry->header[1];
		record.values.header[2] = entry->header[2];
		record.values.header[3] = entry->header[3];
		for (value = 0; value < CAL_DATA_ANNEX60_VALUES; value++)
			record.values.values[value] = entry->values[value];
		macTxNotifyNotifyFrame(index, &record.values);
	}
	return 1;
}

/* Takes the region, the antenna setup and the antennas from the main annex. */
int calDataHeaderApply(void)
{
	unsigned int version = macEventState.regionVersion;
	struct WlanAntennaProfile *profile;
	unsigned int code;
	unsigned int setup;

	calRegionMajor = version >> 6;
	calRegionMinor = (version >> 2) & 0xf;
	profile = (struct WlanAntennaProfile *)__aeabi_uread4(&wlanAntennaProfile);
	code = macEventState.regionCode;
	profile->regionCode[0] = code;
	profile->regionCode[1] = code >> 8;
	setup = wlanRegionTableIndex(macEventState.pathStates);
	profile = (struct WlanAntennaProfile *)__aeabi_uread4(&wlanAntennaProfile);
	profile->antennaSetup = setup;
	wlanAntennaSelect();
	hostCmd0020Set(0, wlanRadioConfig->secondaryAntenna);
	hostCmd0020Set(1, wlanRadioConfig->primaryAntenna);
	return 0;
}

/* Walks the chain from offset, each annex to its loader; nonzero when a loader failed. */
int calDataParse(int fromHost, unsigned int offset)
{
	union {
		struct CalDataHeader header;
		uint32_t words[2];
	} annex;
	int result = 1;

	while (offset != CAL_DATA_END) {
		unsigned int end;

		calDataRead(fromHost, &annex, offset, sizeof(annex));
		if (annex.words[0] == CAL_DATA_END)
			break;
		end = annex.header.length + offset;
		if (annex.header.next < end)
			break;
		if (fromHost && end > CAL_DATA_BUFFER_SIZE)
			break;
		switch (annex.header.type) {
		case CAL_DATA_ANNEX_POWER:
		case CAL_DATA_ANNEX_POWER_PATH1:
			result = calDataLoadAnnex14(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_POWER_LEVELS:
			result = calDataLoadAnnex16(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_18:
			result = calDataLoadAnnex18(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_31:
		case CAL_DATA_ANNEX_31_PATH1:
			result = calDataLoadAnnex31(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_LIMITS:
			result = calDataLoadAnnex42(fromHost, offset);
			result |= calDataLoadAnnex42Params(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_BAND0:
			result = calDataLoadAnnex49(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_VALUE2:
			result = calDataLoadAnnex52(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_VALUE1:
			result = calDataLoadAnnex53(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_VALUE0:
			result = calDataLoadAnnex54(fromHost, offset);
			break;
		case CAL_DATA_ANNEX_CHANNELS:
			result = calDataLoadAnnex60(fromHost, offset);
			wlanCalibrationType60Present |= result;
			break;
		}
		offset = annex.header.next;
		if (result == 0)
			break;
	}
	return result == 0;
}

__attribute__((noinline)) void calDataHeaderClear(void)
{
	__aeabi_memclr4(&macEventState, sizeof(macEventState));
}

/* Loads the main annex; *next is where the chain goes on. Nonzero when it is not one. */
int calDataHeaderLoad(int fromHost, unsigned int offset, uint32_t *next)
{
	int result;

	calDataRead(fromHost, &macEventState, offset, CAL_DATA_HEADER_SIZE);
	if (!calDataChecksumOk((const uint8_t *)&macEventState, CAL_DATA_HEADER_SIZE))
		return 1;
	if (macEventState.state != MAC_EVENT_STATE_LOADED && macEventState.state != MAC_EVENT_STATE_RUNNING)
		return 1;
	result = calDataHeaderApply();
	*next = macEventState.next;
	return result;
}

/* Copies the calibration data out, after the host's header when it gave one. */
int calDataQuery(uint8_t *data, unsigned int *length)
{
	unsigned int size = calDataHeader.length[0] | calDataHeader.length[1] << 8;

	if (calDataHeaderPresent) {
		__aeabi_memcpy(data, &calDataHeader, sizeof(calDataHeader));
		__aeabi_memcpy(data + sizeof(calDataHeader), calDataBuffer, size);
		size += sizeof(calDataHeader);
	} else {
		__aeabi_memcpy(data, calDataBuffer, size);
	}
	*length = size;
	return 0;
}

/* Sets the station's address; none (0 or all ones) gives the default one. */
void calDataMacAddressSet(uint32_t low, uint32_t high)
{
	if ((low == 0xffffffffu && (uint16_t)high == 0xffff) || (low | (uint16_t)high) == 0) {
		low = WLAN_DEFAULT_ADDRESS_LOW;
		high = WLAN_DEFAULT_ADDRESS_HIGH;
	}
	MAC_ADDRESS_LOW = low;
	MAC_ADDRESS_HIGH_WORD = high;
	*(CalDataWord *)&wlanDeviceAddress->address[0] = low;
	*(CalDataHalf *)&wlanDeviceAddress->address[4] = high;
	btSbcAdjustState(low, high);
}

__attribute__((noinline)) int calDataMacAddressDefault(void)
{
	calDataMacAddressSet(0, 0);
	return 0;
}

/* Reads the image from the NVRAM into calDataBuffer, annex by annex; 1 when it does not fit. */
int calDataImageLoadNvram(void)
{
	uint32_t length;
	uint32_t base;
	uint32_t next;
	uint32_t size;
	uint32_t word;
	unsigned int i;

	nvramRead(CAL_NVRAM_OFFSET_IMAGE_LENGTH, &length);
	length = (uint16_t)length;
	nvramRead(CAL_NVRAM_OFFSET_IMAGE, &base);
	if (length > CAL_DATA_BUFFER_SIZE)
		return 1;
	for (i = 0; i < length; i += 4) {
		nvramRead(base + i, &size);
		*(CalDataWord *)&calDataBuffer[i] = util_Swap32(size);
	}
	next = base;
	for (;;) {
		nvramRead(next + 4, &next);
		if (next == CAL_DATA_END)
			break;
		nvramRead(next, &size);
		size >>= 16;
		if (length + size > CAL_DATA_BUFFER_SIZE)
			break;
		for (i = 0; i < size; i += 4) {
			nvramRead(next + i, &word);
			*(CalDataWord *)&calDataBuffer[length + i] = util_Swap32(word);
		}
		length += size;
	}
	calDataHeader.length[0] = length;
	calDataHeader.length[1] = length >> 8;
	macEventProfileActive = 1;
	calDataOffset = base;
	return 0;
}

/* Loads the calibration data and the MAC address from the NVRAM; without them, the defaults. */
int calDataLoadNvram(void)
{
	uint32_t magic;
	uint32_t base;
	uint32_t low;
	uint32_t high;
	uint32_t next;
	int result;

	nvramOpen();
	nvramRead(CAL_NVRAM_OFFSET_MAGIC, &magic);
	nvramRead(CAL_NVRAM_OFFSET_IMAGE, &base);
	if (magic == CAL_NVRAM_MAGIC && base != CAL_DATA_END) {
		nvramRead(base, &magic);
		if ((uint8_t)magic == MAC_EVENT_STATE_LOADED || (uint8_t)magic == MAC_EVENT_STATE_RUNNING) {
			nvramRead(CAL_NVRAM_OFFSET_ADDRESS_LOW, &low);
			nvramRead(CAL_NVRAM_OFFSET_ADDRESS_HIGH, &high);
			calDataMacAddressSet(low, high);
			result = calDataImageLoadNvram();
			if (result != 1) {
				result = calDataHeaderLoad(0, base, &next);
				result |= calDataParse(0, next);
			}
			goto done;
		}
	}
	hostCmd0003CapsSet(0);
	result = calDataMacAddressDefault();
done:
	nvramClose();
	return result;
}

/* Loads the host's calibration data, after its header when it gives one. */
int calDataLoadHost(const uint8_t *data, unsigned int offset, unsigned int length)
{
	unsigned int size;
	uint32_t next;
	int result;

	(void)offset;
	if ((data[0] == 0 || data[0] == 1) && data[1] == 0) {
		calDataHeaderPresent = 1;
		__aeabi_memcpy(&calDataHeader, data, sizeof(calDataHeader));
		length -= sizeof(calDataHeader);
		data += sizeof(calDataHeader);
	} else {
		calDataHeaderPresent = 0;
		__aeabi_memclr(&calDataHeader, sizeof(calDataHeader));
	}
	size = length < CAL_DATA_BUFFER_SIZE ? length : CAL_DATA_BUFFER_SIZE;
	calDataHeader.length[0] = size;
	calDataHeader.length[1] = size >> 8;
	__aeabi_memcpy(calDataBuffer, data, size);
	macEventProfileActive = 1;
	calDataOffset = 0;
	result = calDataHeaderLoad((int)data, 0, &next);
	return calDataParse((int)data, next) | result;
}

/*
 * HostCmd_CMD_CFG_DATA's calibration data: action 0 reads it; otherwise the
 * host's data (or, without any, the NVRAM's) replaces it. Nonzero when it
 * could not be loaded.
 */
int calDataUpdate(uint8_t *data, unsigned int offset, unsigned int length, unsigned int action)
{
	int result;

	if (data != NULL && action == 0)
		return calDataQuery(data, &length);
	__aeabi_memclr4(calDataBuffer, sizeof(calDataBuffer));
	macTxNotifyTrackFrame();
	calDataHeaderClear();
	sysResetEventPolicy();
	if (data != NULL && action != 0)
		result = calDataLoadHost(data, offset, length);
	else
		result = calDataLoadNvram();
	if (result != 0) {
		calDataHeader.version = 0;
		calDataHeader.reserved1 = 0;
		calDataHeader.reserved2[0] = 0;
		calDataHeader.reserved2[1] = 0;
		calDataHeader.length[0] = 0;
		calDataHeader.length[1] = 0;
	}
	return result;
}
