/*
 * The SCSI commands and sense data the USB mass-storage drivers exchange: the device side's
 * functions under SceUsbstorDriver answer them, the host side's SceUmass sends them.
 */
#pragma once

/* commands */
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

/* sense data */
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
#define SCSI_ASC_MEDIUM_CHANGED		0x28	/* not ready to ready change; 0x2a: parameters changed */
#define SCSI_ASC_POWER_ON_RESET		0x29
#define SCSI_ASC_INCOMPATIBLE_MEDIUM	0x30
#define SCSI_ASC_SAVING_NOT_SUPPORTED	0x39
#define SCSI_ASC_INTERNAL_FAILURE	0x44	/* internal target failure */
#define SCSI_ASC_INTERFACE_FAILURE	0x54	/* SCSI to host system interface failure */
#define SCSI_ASC_KEY_EXCHANGE_FAILURE	0x6f	/* copy protection; ascq 0 authentication failure,
						   2 key not established */

/* MODE SENSE */
#define SCSI_MODE_SENSE_ALL_PAGES	0x3f	/* page code */
#define SCSI_MODE_WRITE_PROTECT		0x80	/* device-specific parameter */
