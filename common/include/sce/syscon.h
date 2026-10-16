/*
 * SceSyscon (os0:kd/syscon.skprx) driver interface additions to vitasdk.
 *
 * The packet is syscon's own layout with its fields named; vitasdk's
 * psp2kern/kernel/syscon.h declares the same structure with offset names,
 * so a file includes one header or the other.
 */

#pragma once

#include <psp2kern/types.h>
#include <stddef.h>
#include <stdint.h>

/* ksceSysconCmdExec/ExecAsync flags; they become the low half of the status */
#define SCE_SYSCON_PACKET_FLAG_PRIORITY		0x0001	/* to the head of its queue */
#define SCE_SYSCON_PACKET_FLAG_NO_RETRY		0x0010	/* not resent when syscon is busy */
#define SCE_SYSCON_PACKET_FLAG_CHECKSUMMED	0x0100	/* tx[] already carries its checksum */
#define SCE_SYSCON_PACKET_FLAG_KEEP_SELECT	0x0200	/* leave the chip select asserted */
#define SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX	0x0400	/* the reply goes to externalRx */
#define SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX	0x0800	/* the command comes from externalTx */

typedef struct SceSysconPacket {
	struct SceSysconPacket *next;
	uint32_t status;		/* SCE_SYSCON_PACKET_FLAG_*, SYSCON_PACKET_* */
	int semaId;
	uint32_t queue;
	union {
		uint8_t tx[32];
		struct {
			uint8_t command[2];	/* little endian */
			uint8_t length;		/* data bytes plus the checksum */
			uint8_t data[29];	/* then the checksum */
		} request;
	};
	union {
		uint8_t rx[32];
		struct {
			uint8_t status[2];
			uint8_t length;		/* result and data bytes plus the checksum */
			uint8_t result;
			union {
				uint8_t data[28];
				uint32_t word[7];
			};
		} reply;
	};
	uint8_t *externalTx;		/* the command's data past tx[] */
	uint8_t *externalRx;		/* the reply's data past rx[] */
	uint32_t externalRxSize;
	uint32_t received;
	int (*callback)(struct SceSysconPacket *packet, void *argp);
	void *argp;
	uint32_t time;
	uint32_t delay;
	uint32_t reserved[4];
} SceSysconPacket;

_Static_assert(sizeof(SceSysconPacket) == 0x80, "SceSysconPacket size");
_Static_assert(offsetof(SceSysconPacket, externalTx) == 0x50,
	       "SceSysconPacket externalTx");
_Static_assert(offsetof(SceSysconPacket, callback) == 0x60,
	       "SceSysconPacket callback");

/* one touch panel's report; names from the henkaku wiki (guessed there) */
typedef struct SceKernelTouchpanelInfo {
	SceUInt16 vendorID;
	SceUInt16 firmwareRev;
	SceUInt16 configRev;
	SceUInt8 hwVersion;
	SceUInt8 vendorInfo;
	SceUInt8 reserved[2];
} SceKernelTouchpanelInfo;

typedef struct SceKernelTouchpanelDeviceInfo2 {
	SceKernelTouchpanelInfo front;
	SceKernelTouchpanelInfo rear;
} SceKernelTouchpanelDeviceInfo2;

_Static_assert(sizeof(SceKernelTouchpanelDeviceInfo2) == 0x14,
	       "SceKernelTouchpanelDeviceInfo2 size");

int ksceSysconWaitInitialized(void);
int ksceSysconCmdExec(SceSysconPacket *packet, uint32_t flags);
int ksceSysconCmdExecAsync(SceSysconPacket *packet, uint32_t flags,
			   int (*callback)(SceSysconPacket *packet, void *argp),
			   void *argp);
int ksceSysconGetBaryonVersion(void);
int ksceSysconGetHardwareInfo(void);
int ksceSysconCtrlDeviceReset(uint32_t device, uint32_t mode);
int SceSysconForDriver_063425AE(uint32_t value);
int SceSysconForDriver_76506A57(uint32_t mode, uint32_t offset,
				 void *data, uint32_t size);

/* name unknown: a syscon state word; (state & 0x7f) == 0x17 after a crash reboot */
int SceSysconForDriver_CF5B2F2F(SceUInt32 *state);

int SceSysconForDriver_63B14156(void);
int SceSysconForDriver_99A254A9(void);
