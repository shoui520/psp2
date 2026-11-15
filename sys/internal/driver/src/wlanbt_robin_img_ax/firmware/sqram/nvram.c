/*
 * The non-volatile store as the rest of the firmware sees it (0xc00088dc ..
 * 0xc0008b30): the first of five configurations that answers is taken (an
 * EEPROM with one address byte if none does), its drivers in sys/nvram.c are
 * set up, and the store is opened, read, written and controlled through them.
 * The names and the file name are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/nvram.h"
#include "robin/rfreg.h"

static struct NvramDevice nvramStore __attribute__((section(".itcm.bss")));	/* 0x56ed8 */

struct NvramDevice *nvramDevice __attribute__((section(".itcm.rw"))) = &nvramStore;	/* 0x54da8 */

struct NvramConfig nvramProbeConfigs[NVRAM_PROBE_CONFIGS] __attribute__((section(".itcm.rw"))) = {
	{ .type = NVRAM_TYPE_EEPROM, .addressMode = 1, .mode = 1 },
	{ .type = NVRAM_TYPE_EEPROM, .addressMode = 2, .mode = 1 },
	{ .type = NVRAM_TYPE_FLASH, .addressMode = 2, .mode = 1 },
	{ .type = NVRAM_TYPE_FLASH, .addressMode = 3, .mode = 2 },
	{ .type = NVRAM_TYPE_FLASH, .addressMode = 3, .mode = 1 },
};											/* 0x54dac */

/* Whether a store of this configuration is fitted. */
int nvramProbe(struct NvramConfig config)
{
	int found = 0;

	if (config.type == NVRAM_TYPE_FLASH) {
		flashConfig.addressBytes = config.addressMode;
		flashMode = config.mode;
		rfBusEnter0082();
		found = flashProbe();
		rfBusLeave0082();
	} else if (config.type == NVRAM_TYPE_EEPROM) {
		eepromAddressMode = config.addressMode;
		eepromPresent = config.mode;
		rfBusEnter0002();
		found = eepromProbe();
		rfBusLeave0002();
	}
	return found;
}

/* The store fitted; without one, an EEPROM with one address byte. Returns 1 if one answered. */
int nvramDetect(void)
{
	int i;

	for (i = 0; i < NVRAM_PROBE_CONFIGS; i++) {
		if (nvramProbe(nvramProbeConfigs[i])) {
			nvramDevice->config = nvramProbeConfigs[i];
			return 1;
		}
	}
	nvramDevice->config.mode = 1;
	nvramDevice->config.type = NVRAM_TYPE_EEPROM;
	nvramDevice->config.addressMode = 1;
	return 0;
}

/* The drivers of the configured store; writing and erasing start disabled. Returns 0 for an unknown type. */
int nvramConfigure(void)
{
	struct NvramDevice *device = nvramDevice;

	switch (device->config.type) {
	case NVRAM_TYPE_EEPROM:
		eepromAddressMode = device->config.addressMode;
		device->open = (int (*)(void))rfBusEnter0002;
		device->close = (int (*)(void))rfBusLeave0002;
		device->read = eepromReadWord;
		device->write = (void (*)(uint32_t, uint32_t))eepromWriteWord;
		break;
	case NVRAM_TYPE_FLASH:
		flashConfig.addressBytes = device->config.addressMode;
		flashMode = device->config.mode;
		device->open = rfBusEnter0082;
		device->close = (int (*)(void))rfBusLeave0082;
		device->read = flashReadWord;
		device->write = flashWriteWord;
		break;
	default:
		return 0;
	}
	device->erase = NULL;
	device->eraseSize = 0;
	device->writeEnabled = 0;
	device->ready = 1;
	return 1;
}

/* Opens the store, looking for it the first time (wlanSessionOps.eepromOpen). */
int nvramOpen(void)
{
	flashConfig.statusTimeout = 2000;
	flashConfig.programTimeout = 100;
	flashConfig.eraseTimeout = 10000;
	if (!nvramDevice->ready) {
		nvramDetect();
		nvramConfigure();
	}
	return nvramDevice->open();
}

int nvramClose(void)
{
	return nvramDevice->close();
}

/* Reads a word (wlanSessionOps.eepromRead). Returns 1. */
int nvramRead(unsigned int offset, uint32_t *value)
{
	*value = nvramDevice->read(offset);
	return 1;
}

/* Writes a word if writing is enabled. Returns 1 if it was written. */
int nvramWrite(unsigned int offset, uint32_t value)
{
	struct NvramDevice *device = nvramDevice;

	if (!device->writeEnabled || device->write == NULL)
		return 0;
	device->write(offset, value);
	return 1;
}

/* An NVRAM_CONTROL_* command; result takes what it reads. Returns 1 if the store does it. */
int nvramControl(unsigned int command, uint32_t argument, uint32_t *result)
{
	struct NvramDevice *device = nvramDevice;
	uint32_t eraseSize;

	switch (command) {
	case NVRAM_CONTROL_WRITE_DISABLE:
		if (device->config.type == NVRAM_TYPE_FLASH) {
			flashWriteDisable();
			nvramDevice->writeEnabled = 0;
			return 1;
		}
		if (device->config.type == NVRAM_TYPE_EEPROM) {
			device->writeEnabled = 0;
			return 1;
		}
		return 0;
	case NVRAM_CONTROL_WRITE_ENABLE:
		if (device->config.type == NVRAM_TYPE_FLASH) {
			flashWriteEnable();
			nvramDevice->writeEnabled = 1;
			return 1;
		}
		if (device->config.type == NVRAM_TYPE_EEPROM) {
			device->writeEnabled = 1;
			return 1;
		}
		return 0;
	case NVRAM_CONTROL_WRITE_STATUS:
		if (device->config.type != NVRAM_TYPE_FLASH)
			return 0;
		*result = flashWriteStatus((uint8_t)argument);
		return 1;
	case NVRAM_CONTROL_READ_STATUS:
		if (device->config.type != NVRAM_TYPE_FLASH)
			return 0;
		*result = flashReadStatus();
		return 1;
	case NVRAM_CONTROL_GET_CONFIG:
		*result = device->configWord;
		return 1;
	case NVRAM_CONTROL_SET_CONFIG:
		device->configWord = argument;
		nvramConfigure();
		return 1;
	case NVRAM_CONTROL_DETECT:
		nvramDetect();
		nvramConfigure();
		return 1;
	case NVRAM_CONTROL_GET_ERASE:
		*result = device->eraseSize;
		return 1;
	case NVRAM_CONTROL_SET_ERASE:
		eraseSize = device->eraseSize;
		if (device->config.mode == 2 && device->config.type == NVRAM_TYPE_FLASH && argument == 0x10000) {
			device->eraseSize = argument;
			device->erase = flashEraseSector;
			*result = eraseSize;
			return 1;
		}
		device->erase = NULL;
		device->eraseSize = 0;
		return 0;
	default:
		return 0;
	}
}
