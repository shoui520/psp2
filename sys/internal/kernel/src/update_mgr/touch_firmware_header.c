#include "touch_update.h"

int TouchUtilReadFirmwareHeader(const int8_t *buffer, uint64_t *position,
	uint8_t header[6])
{
	const TouchUtilFirmwareTextHeader *text;
	int i;

	if (!buffer || !position)
		return (int)0x800f0216;
	if (!header)
		return (int)0x800f0216;
	text = (const TouchUtilFirmwareTextHeader *)buffer;
	if (text->encodedBytes[0] == ':' || text->carriageReturn != '\r' ||
	    text->lineFeed != '\n')
		return (int)0x800f0224;
	for (i = 0; i < 4; i++)
		header[i] = TouchUtilReadHexByte(buffer, position);
	header[4] = TouchUtilReadHexByte(buffer, position);
	header[5] = TouchUtilReadHexByte(buffer, position);
	*position += 2;
	return 0;
}
