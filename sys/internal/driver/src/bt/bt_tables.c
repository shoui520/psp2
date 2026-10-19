/* Bluetooth data tables; the original source filename is unavailable. */
#include <stdint.h>
#include <sysmem/procevent.h>
#include <bt/bt_registered.h>
#include <bt/bt_audio.h>
#include <sysmem/sysroot_bt.h>
#include "bt_internal.h"

BtStack gBtStack;
#if FW_VERSION < 0x04000000
BtInquiryResultForTest gBtTestInquiryResult;
#endif
BtRegisteredOutput gBtRegisteredInfo;
SceUID gBtSysEventHandlerId;
uint64_t gBtBusyDumpTime;
#if FW_VERSION >= 0x04000000
BtInquiryResultForTest gBtTestInquiryResult;
#endif

int gBtPanUserEnabled;
int gBtNetworkAccessPointEnabled;

const SceKernelProcEventHandler gBtProcEventHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = NULL,
	.exit = BtProcessExit,
	.kill = BtProcessKill,
	.suspend = BtProcessSuspend,
	.resume = BtProcessResume,
	.notify = NULL
};

const char gBtEmptyHciFormat[] = "";
const char gBtDeviceAddressHciFormat[] = "6";

const SysrootBtOps gBtSysrootOps = {
	.size = sizeof(SysrootBtOps),
	.getRegisteredInfo = SceBtForDriver_F86D25E2,
	.getConnectingInfo = SceBtForDriver_E9936446
};

const char gBtControllerEmptyFormat[] = "";
const char gBtQueueInitializationFormat[] = "";

const BtChannelOps gBtAvrcpBrowsingOps = {
	.opened = BtOpenAvrcpBrowsing,
	.closed = NULL,
	.receive = BtReceiveAvrcpBrowsing,
	.requestDisconnect = NULL
};

const BtAvrcpPlayerListResponse gBtAvrcpPlayerListResponse = {
	.parameterLengthBe = { 0, 42 },
	.status = 4,
	.uidCounterBe = { 0, 1 },
	.itemCountBe = { 0, 1 },
	.itemType = 1,
	.itemLengthBe = { 0, 34 },
	.playerIdBe = { 0, 1 },
	.majorType = 1,
	.subTypeBe = { 0, 0, 0, 0 },
	.playStatus = 0,
	.features = { 0, 0, 0, 0, 0, 0xb7, 1, 0,
		      0, 0, 0, 0, 0, 0, 0, 0 },
	.characterSetBe = { 0, 106 },
	.nameLengthBe = { 0, 6 },
	.name = "Player"
};

/* Vendor-dependent AV/C command followed by Bluetooth SIG's company ID. */
const BtAvrcpVendorHeader gBtAvrcpVendorHeader = {
	.opcode = 0,
	.companyId = { 0x00, 0x19, 0x58 }
};

const BtChannelOps gBtAvrcpOps = {
	.opened = BtAvrcpOpened,
	.closed = BtAvrcpClosed,
	.receive = BtReceiveAvrcp,
	.requestDisconnect = BtRequestAvrcpDisconnect
};

const uint8_t gBtAvrcpGroupNavigation[5] = { 5, 8, 0, 0x46, 0 };

const BtChannelOps gBtAvdtpReceiveOps = {
	.opened = BtAvdtpReceiveOpened,
	.closed = BtAvdtpClosed,
	.receive = BtDeliverMediaPacket,
	.requestDisconnect = BtRequestAvdtpReceiveDisconnect
};

const BtChannelOps gBtAvdtpTransmitOps = {
	.opened = BtAvdtpTransmitOpened,
	.closed = BtAvdtpClosed,
	.receive = NULL,
	.requestDisconnect = BtRequestAvdtpTransmitDisconnect
};

const BtAvdtpCapabilities gBtAvdtpReceiveCapabilities = {
	.transport = { 1, 0 },
	.sbc = { { 7, 6 }, 0, 0, 0xff, 0xff, 2, 51 },
	.contentProtection = { { 4, 2 }, { 2, 0 } }
};

const BtAvdtpEndpoint gBtAvdtpLocalEndpoints[2] = {
	{ 4, 0 },
	{ 8, 8 }
};

const BtAvdtpCapabilities gBtAvdtpTransmitCapabilities = {
	.transport = { 1, 0 },
	.sbc = { { 7, 6 }, 0, 0, 0x19, 0xf5, 2, 51 },
	.contentProtection = { { 4, 2 }, { 2, 0 } }
};

const BtChannelOps gBtAvdtpSignalingOps = {
	.opened = BtAvdtpSignalingOpened,
	.closed = BtAvdtpClosed,
	.receive = BtReceiveAvdtpSignaling,
	.requestDisconnect = NULL
};

const uint8_t gBtImagingTargetUuid[16] = {
	0xe3, 0x3d, 0x95, 0x45, 0x83, 0x74, 0x4a, 0xd7,
	0x9e, 0xc5, 0xc1, 0x6b, 0xe3, 0x1e, 0xde, 0x8e
};

const uint32_t gBtSniffIntervals[7] = {
	359, 367, 373, 379, 383, 389, 397
};

const uint8_t gBtDhcpMagicCookie[4] = { 0x63, 0x82, 0x53, 0x63 };

const char gBtHspButtonPrefix[] = "AT+CKPD=";
const char gBtHspMicrophonePrefix[] = "AT+VGM=";
const char gBtHspSpeakerPrefix[] = "AT+VGS=";

const BtHspCommand gBtHspCommands[3] = {
	{ gBtHspButtonPrefix, BtHspPressButton },
	{ gBtHspMicrophonePrefix, BtHspSetMicrophoneGain },
	{ gBtHspSpeakerPrefix, BtHspSetSpeakerGain }
};

const BtChannelOps gBtHidInterruptOps = {
	.opened = BtHidInterruptOpened,
	.closed = BtHidInterruptClosed,
	.receive = BtReceiveHidInterrupt,
	.requestDisconnect = NULL
};

const BtChannelOps gBtHidControlOps = {
	.opened = BtHidControlOpened,
	.closed = NULL,
	.receive = BtReceiveHidControl,
	.requestDisconnect = NULL
};

const uint32_t gBtHidAuthenticationPrefix[8] = {
	0x292890b7, 0x0a4dc847, 0x2ff1bfa1, 0xb7b62cd9,
	0xd58fe228, 0x1bc18883, 0xa8f5cb6e, 0x4d56df3e
};

const BtChannelOps gBtBnepOps = {
	.opened = BtBnepOpened,
	.closed = BtBnepClosed,
	.receive = BtReceiveBnep,
	.requestDisconnect = NULL
};

const BtChannelOps gBtRfcommOps = {
	.opened = BtRfcommOpened,
	.closed = BtRfcommClosed,
	.receive = BtReceiveRfcomm,
	.requestDisconnect = BtRequestRfcommDisconnect
};

/* Common trailing bytes of Bluetooth's 128-bit base UUID. */
const uint8_t gBtSdpBaseUuidSuffix[12] = {
	0x00, 0x00, 0x10, 0x00, 0x80, 0x00,
	0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb
};

const BtChannelOps gBtSdpOps = {
	.opened = BtSdpOpened,
	.closed = BtSdpClosed,
	.receive = BtReceiveSdp,
	.requestDisconnect = NULL
};

#define SDP_UINT8(value) 0x08, (uint8_t)(value)
#define SDP_UINT16(value) 0x09, (uint8_t)((value) >> 8), (uint8_t)(value)
#define SDP_UINT32(value) 0x0a, (uint8_t)((value) >> 24), \
	(uint8_t)((value) >> 16), (uint8_t)((value) >> 8), (uint8_t)(value)
#define SDP_UINT64(value) 0x0b, (uint8_t)((value) >> 56), \
	(uint8_t)((value) >> 48), (uint8_t)((value) >> 40), \
	(uint8_t)((value) >> 32), (uint8_t)((value) >> 24), \
	(uint8_t)((value) >> 16), (uint8_t)((value) >> 8), (uint8_t)(value)
#define SDP_UUID16(value) 0x19, (uint8_t)((value) >> 8), (uint8_t)(value)
#define SDP_SEQUENCE(size) 0x35, (size)
#define SDP_ATTRIBUTE(id, ...) \
	(sizeof((uint8_t[]){ __VA_ARGS__ }) + 3), \
	(uint8_t)((id) >> 8), (uint8_t)(id), __VA_ARGS__
#define SDP_RECORD(...) (sizeof((uint8_t[]){ __VA_ARGS__ }) + 1), __VA_ARGS__

/* Length-delimited local service records, in the original advertising order. */
const uint8_t gBtSdpServiceRecords[] = {
	/* Headset audio gateway, RFCOMM channel 4. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x00010003)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(6),
			SDP_UUID16(0x1112), SDP_UUID16(0x1203)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(12),
			SDP_SEQUENCE(3), SDP_UUID16(0x0100),
			SDP_SEQUENCE(5), SDP_UUID16(0x0003), SDP_UINT8(4)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x1108), SDP_UINT16(0x0102))),
	/* A2DP source and sink. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x00010005)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x110a)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(16),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(0x0019),
			SDP_SEQUENCE(6), SDP_UUID16(0x0019), SDP_UINT16(0x0102)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x110d), SDP_UINT16(0x0102))),
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x00010006)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x110b)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(16),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(0x0019),
			SDP_SEQUENCE(6), SDP_UUID16(0x0019), SDP_UINT16(0x0102)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x110d), SDP_UINT16(0x0102))),
	/* AVRCP controller and target, including the browsing protocol. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x00010007)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(6),
			SDP_UUID16(0x110e), SDP_UUID16(0x110f)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(16),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(0x0017),
			SDP_SEQUENCE(6), SDP_UUID16(0x0017), SDP_UINT16(0x0103)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x110e), SDP_UINT16(0x0104)),
		SDP_ATTRIBUTE(0x0311, SDP_UINT16(2))),
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x00010008)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x110c)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(16),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(0x0017),
			SDP_SEQUENCE(6), SDP_UUID16(0x0017), SDP_UINT16(0x0103)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x110e), SDP_UINT16(0x0104)),
		SDP_ATTRIBUTE(0x000d, SDP_SEQUENCE(16),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(0x001b),
			SDP_SEQUENCE(6), SDP_UUID16(0x0017), SDP_UINT16(0x0103)),
		SDP_ATTRIBUTE(0x0311, SDP_UINT16(1))),
	/* Device identification: USB vendor 054c, product 058c. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x0001000a)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x1200)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(13),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(1),
			SDP_SEQUENCE(3), SDP_UUID16(0x0001)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x1200), SDP_UINT16(0x0103)),
		SDP_ATTRIBUTE(0x0200, SDP_UINT16(0x0103)),
		SDP_ATTRIBUTE(0x0201, SDP_UINT16(0x054c)),
		SDP_ATTRIBUTE(0x0202, SDP_UINT16(0x058c)),
		SDP_ATTRIBUTE(0x0203, SDP_UINT16(0x0100)),
		SDP_ATTRIBUTE(0x0204, 0x28, 1),
		SDP_ATTRIBUTE(0x0205, SDP_UINT16(2))),
	/* Network access point, BNEP IPv4 and ARP. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x0001000b)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x1116)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(24),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(0x000f),
			SDP_SEQUENCE(14), SDP_UUID16(0x000f), SDP_UINT16(0x0100),
			SDP_SEQUENCE(6), SDP_UINT16(0x0800), SDP_UINT16(0x0806)),
		SDP_ATTRIBUTE(0x0006, SDP_SEQUENCE(9), SDP_UINT16(0x656e),
			SDP_UINT16(0x006a), SDP_UINT16(0x0100)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x1116), SDP_UINT16(0x0100)),
		SDP_ATTRIBUTE(0x0100, 0x25, 3, 'N', 'A', 'P'),
		SDP_ATTRIBUTE(0x0101, 0x25, 3, 'N', 'A', 'P'),
		SDP_ATTRIBUTE(0x030a, SDP_UINT16(1)),
		SDP_ATTRIBUTE(0x030b, SDP_UINT16(0xfffe)),
		SDP_ATTRIBUTE(0x030c, SDP_UINT32(184000))),
	/* Personal-area-network user. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x0001000d)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x1115)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(24),
			SDP_SEQUENCE(6), SDP_UUID16(0x0100), SDP_UINT16(0x000f),
			SDP_SEQUENCE(14), SDP_UUID16(0x000f), SDP_UINT16(0x0100),
			SDP_SEQUENCE(6), SDP_UINT16(0x0800), SDP_UINT16(0x0806)),
		SDP_ATTRIBUTE(0x0006, SDP_SEQUENCE(9), SDP_UINT16(0x656e),
			SDP_UINT16(0x006a), SDP_UINT16(0x0100)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x1115), SDP_UINT16(0x0100)),
		SDP_ATTRIBUTE(0x0100, 0x25, 4, 'P', 'A', 'N', 'U'),
		SDP_ATTRIBUTE(0x0101, 0x25, 4, 'P', 'A', 'N', 'U'),
		SDP_ATTRIBUTE(0x030a, SDP_UINT16(1))),
	/* Imaging responder, RFCOMM channel 6 and OBEX. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x0001000f)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x111b)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(17),
			SDP_SEQUENCE(3), SDP_UUID16(0x0100),
			SDP_SEQUENCE(5), SDP_UUID16(0x0003), SDP_UINT8(6),
			SDP_SEQUENCE(3), SDP_UUID16(0x0008)),
		SDP_ATTRIBUTE(0x0009, SDP_SEQUENCE(8), SDP_SEQUENCE(6),
			SDP_UUID16(0x111a), SDP_UINT16(0x0100)),
		SDP_ATTRIBUTE(0x0310, SDP_UINT8(1)),
		SDP_ATTRIBUTE(0x0311, SDP_UINT16(3)),
		SDP_ATTRIBUTE(0x0312, SDP_UINT32(0x00010003)),
		SDP_ATTRIBUTE(0x0313, SDP_UINT64(UINT64_C(1000000000)))),
	/* Serial port, RFCOMM channel 7. */
	SDP_RECORD(
		SDP_ATTRIBUTE(0x0000, SDP_UINT32(0x00010010)),
		SDP_ATTRIBUTE(0x0001, SDP_SEQUENCE(3), SDP_UUID16(0x1101)),
		SDP_ATTRIBUTE(0x0004, SDP_SEQUENCE(12),
			SDP_SEQUENCE(3), SDP_UUID16(0x0100),
			SDP_SEQUENCE(5), SDP_UUID16(0x0003), SDP_UINT8(7)))
};

_Static_assert(sizeof(gBtSdpServiceRecords) == 696, "SDP service records size");

/* Reflected IEEE CRC-32 polynomial 0xedb88320. */
const uint32_t gBtCrc32Table[256] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

/* Original 3.6x puts the key before the RFCOMM table; 4.00 puts it after. */
#if FW_VERSION < 0x04000000
#include "bt_rng_key.h"
#endif

/* Reflected CRC-8 polynomial 0xe0, used for RFCOMM frame checksums. */
const uint8_t gBtRfcommFcsTable[256] = {
	0x00, 0x91, 0xe3, 0x72, 0x07, 0x96, 0xe4, 0x75, 0x0e, 0x9f, 0xed, 0x7c, 0x09, 0x98, 0xea, 0x7b,
	0x1c, 0x8d, 0xff, 0x6e, 0x1b, 0x8a, 0xf8, 0x69, 0x12, 0x83, 0xf1, 0x60, 0x15, 0x84, 0xf6, 0x67,
	0x38, 0xa9, 0xdb, 0x4a, 0x3f, 0xae, 0xdc, 0x4d, 0x36, 0xa7, 0xd5, 0x44, 0x31, 0xa0, 0xd2, 0x43,
	0x24, 0xb5, 0xc7, 0x56, 0x23, 0xb2, 0xc0, 0x51, 0x2a, 0xbb, 0xc9, 0x58, 0x2d, 0xbc, 0xce, 0x5f,
	0x70, 0xe1, 0x93, 0x02, 0x77, 0xe6, 0x94, 0x05, 0x7e, 0xef, 0x9d, 0x0c, 0x79, 0xe8, 0x9a, 0x0b,
	0x6c, 0xfd, 0x8f, 0x1e, 0x6b, 0xfa, 0x88, 0x19, 0x62, 0xf3, 0x81, 0x10, 0x65, 0xf4, 0x86, 0x17,
	0x48, 0xd9, 0xab, 0x3a, 0x4f, 0xde, 0xac, 0x3d, 0x46, 0xd7, 0xa5, 0x34, 0x41, 0xd0, 0xa2, 0x33,
	0x54, 0xc5, 0xb7, 0x26, 0x53, 0xc2, 0xb0, 0x21, 0x5a, 0xcb, 0xb9, 0x28, 0x5d, 0xcc, 0xbe, 0x2f,
	0xe0, 0x71, 0x03, 0x92, 0xe7, 0x76, 0x04, 0x95, 0xee, 0x7f, 0x0d, 0x9c, 0xe9, 0x78, 0x0a, 0x9b,
	0xfc, 0x6d, 0x1f, 0x8e, 0xfb, 0x6a, 0x18, 0x89, 0xf2, 0x63, 0x11, 0x80, 0xf5, 0x64, 0x16, 0x87,
	0xd8, 0x49, 0x3b, 0xaa, 0xdf, 0x4e, 0x3c, 0xad, 0xd6, 0x47, 0x35, 0xa4, 0xd1, 0x40, 0x32, 0xa3,
	0xc4, 0x55, 0x27, 0xb6, 0xc3, 0x52, 0x20, 0xb1, 0xca, 0x5b, 0x29, 0xb8, 0xcd, 0x5c, 0x2e, 0xbf,
	0x90, 0x01, 0x73, 0xe2, 0x97, 0x06, 0x74, 0xe5, 0x9e, 0x0f, 0x7d, 0xec, 0x99, 0x08, 0x7a, 0xeb,
	0x8c, 0x1d, 0x6f, 0xfe, 0x8b, 0x1a, 0x68, 0xf9, 0x82, 0x13, 0x61, 0xf0, 0x85, 0x14, 0x66, 0xf7,
	0xa8, 0x39, 0x4b, 0xda, 0xaf, 0x3e, 0x4c, 0xdd, 0xa6, 0x37, 0x45, 0xd4, 0xa1, 0x30, 0x42, 0xd3,
	0xb4, 0x25, 0x57, 0xc6, 0xb3, 0x22, 0x50, 0xc1, 0xba, 0x2b, 0x59, 0xc8, 0xbd, 0x2c, 0x5e, 0xcf,
};

#if FW_VERSION >= 0x04000000
#include "bt_rng_key.h"
#endif
