/*
 * The mdev device framework of the Mask ROM as the RAM code uses it: a driver
 * descriptor handed to mdevRegisterDriver(), and the device handles the
 * firmware opens ("wlan", "sdio", "bt_sdio", ...). Field roles are inferred.
 */
#ifndef ROBIN_MDEV_H
#define ROBIN_MDEV_H

#include <stdint.h>

struct MdevDriver {
	uint32_t link[2];	/* list links, owned by the ROM */
	const char *name;
	uint32_t index;
	void *callbacks[7];
	uint32_t reserved;
};

_Static_assert(sizeof(struct MdevDriver) == 48, "mdev driver descriptor");

#endif /* ROBIN_MDEV_H */
