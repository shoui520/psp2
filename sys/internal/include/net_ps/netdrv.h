/*
 * The network driver interface of SceNetPs (os0:kd/net_ps.skprx), as its
 * Ethernet drivers fill it in (os0:kd/bootimage/usb_ether_smsc,
 * usb_ether_rtl) and register it with SceNetPsForDriver_1ABF937D. The
 * interface name the drivers pass is "SceNetDrvEther". Type and field names
 * are reviewed descriptive names where Sony's names are unavailable.
 */

#pragma once

#include <scetypes.h>
#include <net/ether.h>
#include <stddef.h>

/* a packet's data: the frame runs from data to end */
struct NetDriverBuffer {
	unsigned char *allocationStart;
	unsigned char *allocationEnd;
	unsigned char *data;
	unsigned char *end;
	struct NetDriverBuffer *next;
};

_Static_assert(offsetof(struct NetDriverBuffer, data) == 8, "NetDriverBuffer data");
_Static_assert(offsetof(struct NetDriverBuffer, next) == 16, "NetDriverBuffer next");
_Static_assert(sizeof(struct NetDriverBuffer) == 20, "NetDriverBuffer size");

/* a packet SceNetPs hands to the driver or takes from it */
struct NetDriverPacket {
	unsigned int checksumFlags;
	unsigned int reserved0;
	unsigned int length;
	unsigned int allocationSize;
	unsigned int releaseTimeMs;
	unsigned int emulationFlags;
	unsigned int sequenceNumber;
	unsigned int reserved1;
	struct NetDriverBuffer *buffer;
	struct NetDriverPacket *next;
	struct NetDriverPacket *prev;
};

_Static_assert(offsetof(struct NetDriverPacket, length) == 8, "NetDriverPacket length");
_Static_assert(offsetof(struct NetDriverPacket, buffer) == 32, "NetDriverPacket buffer");
_Static_assert(offsetof(struct NetDriverPacket, next) == 36, "NetDriverPacket next");
_Static_assert(sizeof(struct NetDriverPacket) == 44, "NetDriverPacket size");

/*
 * NetDriver.notify takes 64-bit events: an event in the low word, its
 * argument in the high word
 */
#define NET_DRIVER_EVENT(event, argument) \
	((SceUInt64)(unsigned int)(argument) << 32 | (unsigned int)(event))

/* NetDriver.notify events */
#define NET_DRIVER_EVENT_STATUS		0	/* argument: NET_DRIVER_STATUS_* */
#define NET_DRIVER_EVENT_LINK_UP	1
#define NET_DRIVER_EVENT_LINK_DOWN	2
#define NET_DRIVER_EVENT_RESUMED	0x20	/* the USB power came back */

/* NET_DRIVER_EVENT_STATUS arguments */
#define NET_DRIVER_STATUS_STARTED	1
#define NET_DRIVER_STATUS_STOPPED	2
#define NET_DRIVER_STATUS_RECEIVED	4
#define NET_DRIVER_STATUS_ERROR		8	/* data: the error code */

/* NetDriver.control commands; the argument is one int */
#define NET_DRIVER_CTL_GET_LINK		0x40200001	/* NET_DRIVER_LINK_* */
#define NET_DRIVER_CTL_GET_PROMISCUOUS	0x40200002
#define NET_DRIVER_CTL_SET_LINK		0x60200001
#define NET_DRIVER_CTL_SET_PROMISCUOUS	0x60200002

/* link modes: what NET_DRIVER_CTL_SET_LINK asks for, GET_LINK reports */
#define NET_DRIVER_LINK_UP		0x01
#define NET_DRIVER_LINK_FULL_DUPLEX	0x02
#define NET_DRIVER_LINK_AUTO		0x04
#define NET_DRIVER_LINK_10M		0x10
#define NET_DRIVER_LINK_100M		0x20
#define NET_DRIVER_LINK_1000M		0x40
#define NET_DRIVER_LINK_ANY		0x77	/* autonegotiate everything */

/* what the Ethernet drivers put in NetDriver.capabilities (bits unknown) */
#define NET_DRIVER_CAPABILITIES_ETHER	0xe0000
#define NET_DRIVER_TYPE_ETHER		1

/* the error an Ethernet driver reports when it cannot open now */
#define NET_DRIVER_ERROR_NOT_READY	0x80411682

struct SceNetPsDriverContext;

struct NetDriver {
	struct NetDriver *next;
	struct NetDriver *prev;
	/* SceNetPs services */
	void *(*allocate)(struct NetDriver *driver, unsigned int size,
			  unsigned int alignment);
	int (*free)(struct NetDriver *driver, void *ptr);
	struct NetDriverPacket *(*allocateRx)(struct NetDriver *driver, unsigned int length,
					    unsigned int alignment);
	struct NetDriverBuffer *(*appendBuffer)(struct NetDriver *driver,
					       struct NetDriverPacket *packet,
					       unsigned int length,
					       unsigned int alignment);
	void (*releaseTx)(struct NetDriver *driver, struct NetDriverPacket *packet);
	int (*deliverRx)(struct NetDriver *driver, struct NetDriverPacket *packet);
	struct NetDriverPacket *(*dequeueTx)(struct NetDriver *driver);
	int (*notify)(struct NetDriver *driver, SceUInt64 events, const void *data,
		      int size);
	int (*setMtu)(struct NetDriver *driver, unsigned int mtu);
	unsigned int reserved2[2];
	struct SceNetPsDriverContext *context;
	/* the driver's entry points */
	int (*stop)(struct NetDriver *driver, void *memoryBase, int memorySize);
	int (*start)(struct NetDriver *driver, void *memoryBase);
	int (*open)(struct NetDriver *driver, int argument); /* bring the link up, report STARTED */
	int (*close)(struct NetDriver *driver, int argument); /* take it down, report STOPPED */
	int (*kickTx)(struct NetDriver *driver, int argument);
	int (*multicast)(struct NetDriver *driver, const unsigned char *addresses, unsigned int size);
	int (*control)(struct NetDriver *driver, unsigned int command, int *argument, unsigned int size);
	unsigned int capabilities;
	unsigned int type;			/* NET_DRIVER_TYPE_* */
	SceNetEtherAddr mac;
	unsigned short padding;
	unsigned int mtu;
	unsigned int prefix;			/* bytes the driver puts before a frame */
	void *private;				/* the driver's context */
	/* statistics */
	unsigned int txBytes, txPackets, txBroadcastBytes, txBroadcastPackets;
	unsigned int txMulticastBytes, txMulticastPackets, txErrors, txDrops;
	unsigned int rxBytes, rxPackets, rxBroadcastBytes, rxBroadcastPackets;
	unsigned int rxMulticastBytes, rxMulticastPackets, rxDrops, rxErrors;
};

_Static_assert(offsetof(struct NetDriver, next) == 0, "NetDriver next");
_Static_assert(offsetof(struct NetDriver, prev) == 4, "NetDriver prev");
_Static_assert(offsetof(struct NetDriver, allocate) == 8, "NetDriver allocate");
_Static_assert(offsetof(struct NetDriver, free) == 12, "NetDriver free");
_Static_assert(offsetof(struct NetDriver, allocateRx) == 0x10, "NetDriver allocateRx");
_Static_assert(offsetof(struct NetDriver, appendBuffer) == 20, "NetDriver appendBuffer");
_Static_assert(offsetof(struct NetDriver, setMtu) == 40, "NetDriver setMtu");
_Static_assert(offsetof(struct NetDriver, context) == 0x34, "NetDriver context");
_Static_assert(offsetof(struct NetDriver, stop) == 0x38, "NetDriver stop");
_Static_assert(offsetof(struct NetDriver, mac) == 0x5c, "NetDriver mac");
_Static_assert(sizeof(struct NetDriver) == 0xb0, "NetDriver size");

/* name unknown: registers a driver with SceNetPs */
int SceNetPsForDriver_1ABF937D(void *driver);
/* name unknown: removes a driver's interface and its queued packets */
int SceNetPsForDriver_F5788FF8(void *argument);
