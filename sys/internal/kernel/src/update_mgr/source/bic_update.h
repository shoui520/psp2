#ifndef UPDATE_MGR_BIC_UPDATE_H
#define UPDATE_MGR_BIC_UPDATE_H

#include <stddef.h>
#include <stdint.h>

typedef struct BicUpdateCommand {
	uint32_t type;
	uint32_t compareAddress;
	uint32_t compareOffset;
	uint32_t delayMilliseconds;
	uint32_t size;
	uint8_t data[100];
} BicUpdateCommand;

typedef union BicUpdateToken {
	int8_t text[4];
	uint32_t cleared;
} BicUpdateToken;

_Static_assert(sizeof(BicUpdateCommand) == 120, "battery update command size");
_Static_assert(offsetof(BicUpdateCommand, data) == 20,
		"battery update command data offset");
_Static_assert(sizeof(BicUpdateToken) == 4, "battery update token size");

int BicUtilStringToInt(const int8_t *text, int base);
int BicUtilWaitCookieStatus(void);

#endif
