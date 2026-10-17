/*
 * SceUsbstorDriver (os0:kd/bootimage/usbstor): the USB mass-storage transport,
 * and the storage functions that register with it (SceUsbstorForDriver_C0944A04)
 * to answer the SCSI commands. Private Sony names unknown.
 */
#pragma once
#include <stdint.h>
#include <idstorage/idstorage.h>

/* the driver's start argument (ksceUdcdStart "USBStor_Driver"): a SCSI peripheral device type */
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

/* SCSI */
#define SCSI_TEST_UNIT_READY		0x00
#define SCSI_REQUEST_SENSE		0x03
#define SCSI_INQUIRY			0x12
#define SCSI_MODE_SENSE_6		0x1a
#define SCSI_START_STOP_UNIT		0x1b
#define SCSI_PREVENT_ALLOW_REMOVAL	0x1e
#define SCSI_READ_FORMAT_CAPACITIES	0x23
#define SCSI_READ_CAPACITY_10		0x25
#define SCSI_READ_10			0x28
#define SCSI_WRITE_10			0x2a
#define SCSI_SEEK_10			0x2b
#define SCSI_WRITE_AND_VERIFY_10	0x2e
#define SCSI_VERIFY_10			0x2f
#define SCSI_READ_TOC			0x43
#define SCSI_MODE_SENSE_10		0x5a
#define SCSI_SEND_KEY			0xa3
#define SCSI_REPORT_KEY			0xa4
#define SCSI_READ_12			0xa8
#define SCSI_WRITE_12			0xaa

#define SCSI_SENSE_CURRENT		0x70	/* fixed-format sense response codes */
#define SCSI_SENSE_DEFERRED		0x71
#define SCSI_SENSE_ADDITIONAL		10	/* additional sense length */

#define SCSI_KEY_NOT_READY		0x02
#define SCSI_KEY_MEDIUM_ERROR		0x03
#define SCSI_KEY_HARDWARE_ERROR		0x04
#define SCSI_KEY_ILLEGAL_REQUEST	0x05
#define SCSI_KEY_UNIT_ATTENTION		0x06
#define SCSI_KEY_MISCOMPARE		0x0e

#define SCSI_ASC_NO_SEEK_COMPLETE	0x02
#define SCSI_ASC_WRITE_FAULT		0x03
#define SCSI_ASC_WRITE_ERROR		0x0c
#define SCSI_ASC_UNRECOVERED_READ_ERROR	0x11
#define SCSI_ASC_LBA_OUT_OF_RANGE	0x21
#define SCSI_ASC_INVALID_FIELD_IN_CDB	0x24
#define SCSI_ASC_LUN_NOT_SUPPORTED	0x25
#define SCSI_ASC_POWER_ON_RESET		0x29
#define SCSI_ASC_INCOMPATIBLE_MEDIUM	0x30
#define SCSI_ASC_SAVING_NOT_SUPPORTED	0x39
#define SCSI_ASC_INTERNAL_FAILURE	0x44	/* internal target failure */
#define SCSI_ASC_INTERFACE_FAILURE	0x54	/* SCSI to host system interface failure */
#define SCSI_ASC_KEY_EXCHANGE_FAILURE	0x6f	/* copy protection; ascq 0 authentication failure,
						   2 key not established */

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
int SceUdcdForDriver_78BF2A28(void *info);
