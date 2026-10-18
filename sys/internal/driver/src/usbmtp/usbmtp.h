/*
 * SceUsbMtp private layouts. Sony's names are unknown except where the
 * USB and MTP specifications name a field.
 */
#pragma once
#include <stdint.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <usbstor/usbstor.h>
#include <mtpif/mtpif.h>
#include <udcd/udcd.h>

/* the generic MTP container on the bulk pipes */
typedef struct MtpContainer {
	unsigned length;
	uint16_t type;
	uint16_t code;
	unsigned transaction;
	unsigned params[5];
} MtpContainer;

/* an operation as mtpif passes it */
typedef struct MtpCommand {
	uint16_t code;
	uint16_t reserved;
	unsigned flags;
	unsigned transaction;
	unsigned params[5];
	unsigned count;
} MtpCommand;

typedef struct MtpEvent {
	uint16_t code;
	uint16_t reserved;
	unsigned flags;
	unsigned transaction;
	unsigned params[3];
	unsigned count;
} MtpEvent;

typedef struct MtpRequest {
	SceUdcdDeviceRequest request;
	uint8_t reserved[20];
} MtpRequest;

/* one speed's configuration, carved out of the memblock */
typedef struct MtpInterfaceDescriptors {
	SceUdcdInterfaceDescriptor descriptor[2];
} MtpInterfaceDescriptors;

typedef struct MtpEndpointDescriptors {
	SceUdcdEndpointDescriptor descriptor[4];
} MtpEndpointDescriptors;

typedef struct MtpDescriptors {
	SceUdcdConfigDescriptor config;
	SceUdcdInterfaceSettings settings;
	MtpInterfaceDescriptors interfaces;
	MtpEndpointDescriptors endpoints;
} MtpDescriptors;

/* Microsoft OS string descriptor ("MSFT100", vendor code 1) */
typedef struct MtpOsStringDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bString[8];
} MtpOsStringDescriptor;

/* Microsoft extended compatible ID descriptor: one "MTP" function */
typedef struct MtpCompatibleIdDescriptor {
	uint32_t dwLength;
	uint16_t bcdVersion;
	uint16_t wIndex;
	uint8_t bCount;
	uint8_t reserved0[7];
	uint8_t bFirstInterfaceNumber;
	uint8_t reserved1;
	char compatibleID[8];
	char subCompatibleID[8];
	uint8_t reserved2[6];
} MtpCompatibleIdDescriptor;

_Static_assert(sizeof(MtpDescriptors) == 0x94, "MtpDescriptors size");
_Static_assert(sizeof(MtpOsStringDescriptor) == 18, "MtpOsStringDescriptor size");
_Static_assert(sizeof(MtpCompatibleIdDescriptor) == 40, "MtpCompatibleIdDescriptor size");
