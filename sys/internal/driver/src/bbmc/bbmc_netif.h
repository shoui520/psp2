#pragma once

#include <stddef.h>
#include <net_ps/netdrv.h>
#include <threadmgr/fast_mutex_object.h>

typedef struct BbmcPacketInterface BbmcPacketInterface;

int bbmcNotifyPacketInterfaceEvent(unsigned int events, int status);
int bbmcClearPacketInterfaceUsbState(void);
int bbmcRequestNetworkStop(void);
int bbmcRegisterNetworkInterface(void);

typedef struct BbmcPacketPrefix {
	SceUInt8 marker;
	SceUInt8 zeroBytes[5];
} BbmcPacketPrefix;

_Static_assert(sizeof(BbmcPacketPrefix) == 6, "packet USB prefix");

typedef struct BbmcPacketReceiveBuffer {
	BbmcPacketInterface *owner;
	SceSize length;
	unsigned char *data;
} BbmcPacketReceiveBuffer;

typedef struct BbmcPacketTransmitBuffer {
	BbmcPacketInterface *owner;
	SceSize length;
	unsigned char *data;
	int pending;
} BbmcPacketTransmitBuffer;

struct BbmcPacketInterface {
	struct NetDriver driver;
	struct ThreadmgrFastMutex flagMutex;
	int linkOpen;
	int usbAttached;
	int disconnectPending;
	SceNetEtherAddr localMac;
	SceNetEtherAddr peerMac;
	SceUInt32 networkHandle;
	/* No semantic readers/writers beyond the complete-owner reset. */
	SceUInt8 reserved[20];
	SceUInt8 reserved1;
	SceUInt8 prependPacketHeader;
	SceUInt8 reserved2;
	SceUInt8 hasIpv4Address;
	SceUInt32 ipv4Address;
	SceUInt8 reserved3[16];
	SceUInt32 runtimeValue;
	SceUInt32 activeCalls;
	int receivePipe;
	int transmitPipe;
	BbmcPacketReceiveBuffer receiveBuffers[4];
	BbmcPacketTransmitBuffer transmitBuffers[2];
	SceUInt32 usbSpeed;
	SceSize maxPacketSize;
	struct ThreadmgrFastMutex transmitMutex;
	SceKernelSpinlock expiryLock;
	SceUInt32 expiryCount;
	SceUID expiryTimer;
};

_Static_assert(sizeof(BbmcPacketReceiveBuffer) == 12, "packet receive buffer");
_Static_assert(sizeof(BbmcPacketTransmitBuffer) == 16, "packet transmit buffer");
_Static_assert(sizeof(BbmcPacketInterface) == 492, "packet interface");
_Static_assert(offsetof(BbmcPacketInterface, flagMutex) == 176, "packet flag mutex");
_Static_assert(offsetof(BbmcPacketInterface, networkHandle) == 264, "network handle");
_Static_assert(offsetof(BbmcPacketInterface, receivePipe) == 320, "packet receive pipe");
_Static_assert(offsetof(BbmcPacketInterface, transmitMutex) == 416, "packet transmit mutex");
_Static_assert(offsetof(BbmcPacketInterface, expiryLock) == 480, "packet expiry lock");

/* netif/netif.c */
int bbmcInitializeUsbPacketInterface(void);
int bbmcFinalizeUsbPacketInterface(void);
int bbmcUnregisterNetworkInterface(void);
int bbmcStopUsbSerialDevice(int argumentSize, void *arguments, void *context);
int bbmcStartUsbSerialDevice(int argumentSize, void *arguments, void *context);
int DisconnectNetwork(void);
