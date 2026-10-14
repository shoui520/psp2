#include "touch_update.h"

int TouchUtilReadFirmwareRecord(const int8_t *buffer, uint64_t *position,
	TouchUtilFirmwareRecord *record)
{
	const TouchUtilFirmwareTextRecord *text;
	uint16_t high;
	int i;

	if (!position || !buffer || !record)
		return (int)0x800f0216;
	text = (const TouchUtilFirmwareTextRecord *)(buffer + (uint32_t)*position);
	if (text->marker != ':' || text->carriageReturn != '\r' ||
	    text->lineFeed != '\n')
		return (int)0x800f0224;
	(*position)++;
	record->marker = text->marker;
	record->recordTag = TouchUtilReadHexByte(buffer, position);
	high = (uint16_t)(TouchUtilReadHexByte(buffer, position) << 8);
	record->blockIndex = high;
	record->blockIndex = TouchUtilReadHexByte(buffer, position) | high;
	high = (uint16_t)(TouchUtilReadHexByte(buffer, position) << 8);
	record->dataSize = high;
	record->dataSize = TouchUtilReadHexByte(buffer, position) | high;
	if (record->dataSize != 128)
		return (int)0x800f0224;
	for (i = 0; i < 128; i++)
		record->payload[i] = TouchUtilReadHexByte(buffer, position);
	record->checksum = TouchUtilReadHexByte(buffer, position);
	*position += 2;
	return 0;
}
