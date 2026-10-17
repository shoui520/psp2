/*
 * SceUsbAudio's private layouts. Names are ours (no debug records); the
 * descriptor layouts and their field names are the USB Audio Class 1.0
 * specification's.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <psp2kern/usbd.h>
#include <psp2/usbaudioin.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>

#include <psp2kern/kernel/proc_event.h>
#include <threadmgr/threadmgr.h>
int ksceUsbAudioGetDeviceIdList(unsigned *list, int size);
int ksceUsbAudioRegisterCallback(int callback);
int ksceUsbAudioUnregisterCallback(int callback);
int ksceUsbAudioInput(unsigned device, void *buffer, unsigned size);
int ksceUsbAudioInput2(unsigned device, void *buffer, unsigned size);
int SceUsbAudioIn_198A7C33(SceUInt32 device, SceUInt32 *volume);

/* USB Audio Class 1.0 */
#define UAC_SUBCLASS_AUDIOCONTROL	0x01
#define UAC_SUBCLASS_AUDIOSTREAMING	0x02
#define UAC_CS_INTERFACE		0x24	/* class-specific descriptor types */
#define UAC_CS_ENDPOINT			0x25

/* AudioControl interface descriptor subtypes */
#define UAC_HEADER			0x01
#define UAC_INPUT_TERMINAL		0x02
#define UAC_OUTPUT_TERMINAL		0x03
#define UAC_MIXER_UNIT			0x04
#define UAC_SELECTOR_UNIT		0x05
#define UAC_FEATURE_UNIT		0x06
#define UAC_PROCESSING_UNIT		0x07
#define UAC_EXTENSION_UNIT		0x08

/* AudioStreaming interface descriptor subtypes */
#define UAC_AS_GENERAL			0x01
#define UAC_FORMAT_TYPE			0x02
#define UAC_FORMAT_TYPE_I		0x01

#define UAC_TERMINAL_STREAMING		0x0101	/* USB streaming terminal type */
#define UAC_FEATURE_VOLUME		0x02	/* bmaControls bit */

/* class requests */
#define UAC_SET_CUR			0x01
#define UAC_GET_CUR			0x81
#define UAC_GET_MIN			0x82
#define UAC_GET_MAX			0x83
#define UAC_VOLUME_CONTROL		0x02	/* feature unit control selector */
#define UAC_SAMPLING_FREQ_CONTROL	0x01	/* endpoint control selector */

#define USB_LANGID_EN_US		0x0409
#define USB_STRING_MAX			0x200	/* bytes of a string descriptor read */
#define UAC_NO_UNIT			0xff

/* the start of every class-specific descriptor */
typedef struct UacDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t data[];
} UacDescriptor;

typedef struct UacHeaderDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;		/* UAC_HEADER */
	uint8_t bcdADC[2];
	uint8_t wTotalLength[2];
	uint8_t bInCollection;
	uint8_t baInterfaceNr[];		/* the streaming interfaces */
} UacHeaderDescriptor;

/* a terminal or unit: the field after the subtype is its ID */
typedef struct UacEntityDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bEntityID;
	uint8_t data[];
} UacEntityDescriptor;

typedef struct UacOutputTerminalDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;		/* UAC_OUTPUT_TERMINAL */
	uint8_t bTerminalID;
	uint16_t wTerminalType;
	uint8_t bAssocTerminal;
	uint8_t bSourceID;
	uint8_t iTerminal;
} __attribute__((packed)) UacOutputTerminalDescriptor;

/* mixer and selector units */
typedef struct UacPinsDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bUnitID;
	uint8_t bNrInPins;
	uint8_t baSourceID[];
} UacPinsDescriptor;

typedef struct UacFeatureUnitDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;		/* UAC_FEATURE_UNIT */
	uint8_t bUnitID;
	uint8_t bSourceID;
	uint8_t bControlSize;
	uint8_t bmaControls[];			/* [0]: the master channel */
} UacFeatureUnitDescriptor;

/* processing and extension units */
typedef struct UacProcessingUnitDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bUnitID;
	uint8_t wProcessType[2];
	uint8_t bNrInPins;
	uint8_t baSourceID[];
} UacProcessingUnitDescriptor;

typedef struct UacAsGeneralDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;		/* UAC_AS_GENERAL */
	uint8_t bTerminalLink;
	uint8_t bDelay;
	uint8_t wFormatTag[2];
} UacAsGeneralDescriptor;

typedef struct UacFormatTypeIDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;		/* UAC_FORMAT_TYPE */
	uint8_t bFormatType;			/* UAC_FORMAT_TYPE_I */
	uint8_t bNrChannels;
	uint8_t bSubframeSize;
	uint8_t bBitResolution;
	uint8_t bSamFreqType;			/* the number of rates, 0: continuous */
	uint8_t tSamFreq[][3];			/* 24-bit little endian */
} UacFormatTypeIDescriptor;

/* The original uses full 16-bit packet lengths/statuses, unlike the SDK's
 * packed PSW layout. This is its private request representation. */
typedef struct AudioIso {
	void *buffer;
	int packets;
	struct { uint16_t length, status; } packet[8];
} AudioIso;

typedef struct AudioDma {
	uint8_t data[0x600];
	AudioIso request;
	struct AudioStream *stream;
} AudioDma;

typedef struct AudioRing {
	unsigned size, used;
	uint8_t data[0x600];
} AudioRing;

/* AudioStream.event bits */
#define STREAM_DATA_READY		0x02
#define STREAM_STOPPED			0x04	/* no transfer left after a close */
#define STREAM_HALTED			0x08
#define STREAM_SUBMIT_FAILED		0x10

typedef struct AudioStream {
	AudioDma *dma[3];
	unsigned reserved, active;
	SceUID memblock[3];
	unsigned write, ready;
	AudioRing ring[3];
	SceUID event;
	unsigned device, frameBytes, frames, opened;
	SceUID process, mutex;
} AudioStream;

struct AudioPort;
typedef struct AudioPacket {
	struct AudioPort *owner;
	AudioIso request;
	uint8_t data[0x600];
} AudioPacket;

typedef struct AudioQueue {
	AudioPacket packet[3];
	int pending, read, write;
	AudioPacket *queued[2];
	int completed, completeRead, completeWrite;
	AudioPacket *done[2];
} AudioQueue;

typedef struct AudioFormat {
	unsigned supported, channels, bits, reserved, rateCount;
	unsigned *rates;
} AudioFormat;

typedef struct AudioPort {
	SceUsbdInterfaceDescriptor *inputInterface, *outputInterface;
	SceUsbdEndpointDescriptor *inputEndpoint, *outputEndpoint;
	int controlPipe, inputPipe, outputPipe;
	AudioQueue *inputQueue, *outputQueue;
	AudioFormat inputFormat, outputFormat;
	unsigned channels;
	uint8_t volumeUnit, volumeInterface, padding[2];
	unsigned volumeState[2];		/* cleared with the volume unit, never read */
	AudioStream *stream;
	int halted;
} AudioPort;

/* AudioDevice.state: the setup steps, in order */
enum AudioDeviceState {
	DEVICE_NEW,
	DEVICE_MANUFACTURER,			/* reading the string descriptors */
	DEVICE_PRODUCT,
	DEVICE_SERIAL,
	DEVICE_INPUT_INTERFACE,			/* selecting the alternate settings */
	DEVICE_SAMPLING_RATE,
	DEVICE_OUTPUT_INTERFACE,
	DEVICE_READY,
};

typedef struct AudioDevice {
	unsigned id, state;
	uint16_t vendor, product, version;
	uint8_t manufacturerId, productId, serialId, padding[3];
	char manufacturer[0x101], productName[0x101], serial[0x101];
	uint8_t controlBuffer[0x241];
	AudioPort port;
	SceUID event;
	SceKernelFastMutex mutex;
	int controlResult;
	unsigned reserved;
} AudioDevice;

_Static_assert(sizeof(AudioDevice) == 0x610, "device");
_Static_assert(offsetof(AudioDevice, port) == 0x558, "port");
_Static_assert(sizeof(AudioStream) == 0x125c, "stream");
_Static_assert(sizeof(AudioPacket) == 0x62c, "packet");
_Static_assert(sizeof(AudioQueue) == 0x12ac, "capture queue");
