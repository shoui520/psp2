/*
 * SceUsbstorDriver (os0:kd/bootimage/usbstor): the USB mass-storage transport,
 * and the storage functions that register with it (SceUsbstorForDriver_C0944A04)
 * to answer the SCSI commands. Private Sony names unknown.
 */
#pragma once
#include <stdint.h>
#include <idstorage/idstorage.h>
#include <udcd/udcd.h>
#include <usbstor/scsi.h>

/* the driver's start argument (sceUdcdStart "USBStor_Driver"): a SCSI peripheral device type */
#define USBSTOR_TYPE_DISK		0x00	/* product "Type A" */
#define USBSTOR_TYPE_CDROM		0x05	/* product "Type C" */

/* UsbstorFunction.unit */
#define USBSTOR_UNIT_MASK		0x07	/* the logical unit it serves */
#define USBSTOR_UNIT_ANY		0x40	/* it serves every unit */
#define USBSTOR_UNIT_INVALID		0xb8	/* bits a function may not set */

/* UsbstorFunction.priority: lower runs first */
#define USBSTOR_PRIORITY_LAST		0xff

/* UsbstorFunction.event events */
#define USBSTOR_EVENT_ATTACH		1	/* argument: the bus speed, 1 full, 2 high */
#define USBSTOR_EVENT_DETACH		2
#define USBSTOR_EVENT_RESET		4	/* Bulk-Only Mass Storage Reset */

/* UsbstorFunction.command results */
#define USBSTOR_COMMAND_DONE		0
#define USBSTOR_COMMAND_FAILED		(-1)	/* the sense says why */
#define USBSTOR_COMMAND_PHASE_ERROR	(-2)
#define USBSTOR_COMMAND_UNKNOWN		(-3)	/* not this function's command */

/* the timeout the functions give a data transfer, in microseconds (name unknown) */
#define USBSTOR_TRANSFER_TIMEOUT	1000000

/* the sense a function leaves for its unit; REQUEST SENSE reports it */
struct UsbstorSense {
	uint8_t deferred;			/* nonzero: a deferred error */
	uint8_t key;				/* SCSI_KEY_* */
	uint8_t asc;				/* additional sense code */
	uint8_t ascq;				/* its qualifier */
	uint8_t specific[3];			/* sense-key specific */
	uint8_t reserved;
};

struct UsbstorFunction {
	uint8_t unit;				/* USBSTOR_UNIT_* */
	uint8_t requestSense;			/* nonzero: it answers REQUEST SENSE */
	uint8_t busy;
	uint8_t priority;			/* USBSTOR_PRIORITY_LAST: after the rest */
	int (*command)(int unit, const uint8_t *cdb, struct UsbstorSense *sense);
	int (*event)(int event, int argument);	/* USBSTOR_EVENT_* */
	int (*start)(void);
	int (*stop)(void);
};

/* SceUsbstorForDriver; Sony's names unknown */
int SceUsbstorForDriver_C0944A04(struct UsbstorFunction *fn);		/* register */
int SceUsbstorForDriver_EAC08C24(struct UsbstorFunction *fn);		/* unregister */
int SceUsbstorForDriver_D3C7EE70(const void *buffer, unsigned size, unsigned timeout);	/* to the host */
int SceUsbstorForDriver_F1A690DC(void *buffer, unsigned size, unsigned timeout);	/* from the host */
int SceUsbstorForDriver_B6689597(unsigned unit, int field, void *buffer, unsigned size);
int SceUsbstorForDriver_BEF6B5AE(void);					/* the interface subclass */
int SceUsbstorForDriver_73F2FF9D(unsigned deviceClass);
int SceUsbstorForDriver_ADB2AAF2(const char *product, const char *revision, unsigned unit);
unsigned SceUsbstorForDriver_77B7E69E(void);				/* nonzero: a function is busy */

/* Private UDCD device-information operation; original name unknown. */
