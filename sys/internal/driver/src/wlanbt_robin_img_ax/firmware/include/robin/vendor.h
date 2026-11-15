/*
 * The Bluetooth vendor commands (bt/vendor.c, 0x4e740..0x4ec50): the host's
 * HCI commands of group 63 and what they act on, and the radio set-up the
 * link controller runs at start (its register defaults and the fixed level's
 * tables).
 *
 * Marvell published no source; names are inferred from use.
 */
#ifndef ROBIN_VENDOR_H
#define ROBIN_VENDOR_H

#include <stdint.h>
#include "robin/btpower.h"

#define BT_VENDOR_GROUP		63	/* the opcode group of vendor commands */
#define BT_STATUS_INVALID	18	/* HCI status: invalid command parameters */

/* What the transport's ioctl SBC_IOCTL 0x1c is given (btSbcResolveContext()). */
#define BT_SDIO_IOCTL_REGISTER	0x1c

struct BtSdioRegisterRequest {
	uint16_t first;
	uint16_t second;
	uint8_t third;			/* set by the 6-byte form only */
	uint8_t fourth;
	uint16_t form;			/* 0: the 4-byte form, 1: the 6-byte form */
};

/* A register default of the link rate block: its byte offset and value. */
struct BtRateDefault {
	uint8_t offset;
	uint8_t reserved;
	uint16_t value;
};

#define BT_RADIO_DEFAULTS	65
#define BT_RATE_DEFAULTS	8

/* The radio's register window the defaults go to, and a register set beside it. */
#define BT_RADIO_WINDOW		((volatile uint16_t *)0x90010c00)
#define BT_RADIO_CONTROL	REG16(0x90010ab8)

extern const struct BtRateDefault btRateDefaults[BT_RATE_DEFAULTS];	/* 0x542a0 */
extern const uint16_t btRadioDefaults[BT_RADIO_DEFAULTS];		/* 0x542c0 */

/* The ROM's flag that a vendor command 97 is waiting (DTCM). */
extern uint8_t romVendorPending;			/* DTCM 0x400f590 */

/* The queues the vendor messages go to; the values are the original's literals. */
#define BT_QUEUE_VENDOR_LINK	0xe1302d1eu
#define BT_QUEUE_VENDOR_POWER	0xef000000u

#define BT_MESSAGE_VENDOR_LINK	0x13ef
#define BT_MESSAGE_LINK_POWER	0xcd3	/* btLmpLcResolveBuffer() takes it */

void btSbcComputeContext(unsigned int reg, struct BtPowerWord *words, unsigned int count);	/* 0x4e740 */
int btSbcQueryContext(unsigned int reg, uint8_t value, struct BtPowerWord *words, unsigned int count); /* 0x4e75e */
void btSbcResetContext(const struct BtPowerUpdate *updates, struct BtPowerWord *words,
		       unsigned int count);						/* 0x4e780 */
void btSbcResolveContext(unsigned int length, const uint8_t *parameters, uintptr_t unused2,
			 uintptr_t unused3, uint8_t *status);				/* 0x4e8e0 */
void btSbcScheduleContext(void);							/* 0x4e998 */
void btSbcNotifyContext(void);								/* 0x4e9b6 */
void btSbcFormatContext(unsigned int length, const uint8_t *parameters, uint8_t *status);	/* 0x4e9cc */
void btSbcInspectContext(unsigned int opcode, unsigned int length, const uint8_t *parameters, uintptr_t argument,
			 uint8_t *returnLength, uint8_t *status, uint8_t *returnParameters);	/* 0x4ea08 */

#define BT_STATUS_UNKNOWN	1	/* HCI status: unknown command */

/* A SoC control register; bit 1 is cleared by vendor command 94 with a nonzero parameter. */
#define BT_SOC_CONTROL		REG32(0x80002044)

/* The ROM's copies of the transport's header settings (DTCM); provisional names. */
extern uint8_t romLcHeaderSize;				/* DTCM 0x400f54e */
extern uint16_t romLcDescriptor;			/* DTCM 0x400f562 */

/* The ROM and SQRAM routines the vendor commands hand on to. */
int btLdaRomQueryState(unsigned int length, const uint8_t *parameters);
uint32_t mdevRomValidateState(void);
uint8_t mdevRomScheduleState(uint8_t *reply, void *state, const void *tasks);
uint8_t p2pSqramBuildEntry(unsigned int length, const uint8_t *parameters);	/* SQRAM 0xc000e46e */
uint8_t p2pSqramHandleTable(unsigned int length, const uint8_t *parameters);	/* SQRAM 0xc000edbc */

void btSbcTrackContext(unsigned int opcode, unsigned int length, const uint8_t *command);	/* 0x4ec50 */
int btSbcManageContext(uintptr_t first, uintptr_t second, uintptr_t third, uintptr_t fourth, uintptr_t fifth,
		       uintptr_t sixth);						/* 0x4ec96 */
void btSbcAcquireContext(void);								/* 0x4ecb2: nothing */
void btSbcReleaseContext(void);								/* 0x4ecb4 */
void btSbcSetupContext(const uint8_t *address, uint8_t value);				/* 0x4ecc4 */
void btSbcClearContext(const uint8_t *event);						/* 0x4ed68 */

/*
 * The links' requests (0x55679): each of the 8 links' state, the value its
 * request carried and the reason it was refused with (btSbcConvertContext()
 * and the functions after it).
 */
#define BT_LINK_REQUESTS	8

struct BtLinkRequests {
	uint8_t state[BT_LINK_REQUESTS];	/* BT_LINK_REQUEST_* */
	uint8_t value[BT_LINK_REQUESTS];
	uint8_t reason[BT_LINK_REQUESTS];
};

#define BT_LINK_REQUEST_IDLE	0
#define BT_LINK_REQUEST_PENDING	1
#define BT_LINK_REQUEST_OPEN	2
#define BT_LINK_REQUEST_CLOSING	3

extern struct BtLinkRequests btLinkRequests;		/* 0x55679 */

#define BT_MESSAGE_LINK_REQUEST	80

/* The ROM's command completion: status and the opcode answered. */
int btBdhRomValidateContext(unsigned int status, unsigned int opcode);

/* Not converted yet; provisional names and prototypes. */
int btSbcParsePacket(uintptr_t first, uintptr_t second, uintptr_t third, uintptr_t fourth, uintptr_t fifth,
		     uintptr_t sixth);							/* 0x51da0 */
extern uint8_t btVendorTaskState[];			/* 0x5b850 */
extern const uint8_t btTaskTable[];			/* 0x54634: the tasks, "hct" first */
void btSbcCheckEntry(unsigned int length, const uint8_t *parameters, uint8_t *status, uint8_t *returnLength,
		     uint8_t *returnParameters);					/* 0x4f294 */
void btSbcValidateEntry(unsigned int length, const uint8_t *parameters, uintptr_t argument,
			uint8_t *returnLength, uint8_t *status, uint8_t *returnParameters);	/* 0x4f318 */

#endif /* ROBIN_VENDOR_H */
