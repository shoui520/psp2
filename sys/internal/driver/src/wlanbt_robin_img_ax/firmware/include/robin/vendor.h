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

/* What completes a vendor command 97 that is waiting, or NULL (DTCM). */
extern void (*romVendorPending)(void);			/* DTCM 0x400f590 */

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

void btSbcTrackContext(unsigned int opcode, unsigned int length, const uint8_t *command);	/* 0x4ec50 */
int btSbcManageContext(uintptr_t first, uintptr_t second, uintptr_t third, uintptr_t fourth, uintptr_t fifth,
		       uintptr_t sixth);						/* 0x4ec96 */
void btSbcAcquireContext(void);								/* 0x4ecb2: nothing */
void btSbcReleaseContext(void);								/* 0x4ecb4 */
void btSbcSetupContext(const uint8_t *address, uint8_t value);				/* 0x4ecc4 */
void btSbcClearContext(const uint8_t *event);						/* 0x4ed68 */

/*
 * The role switches of the 8 links (0x55679), HCI Switch_Role: encryption is
 * paused around the switch. A link's state, the role asked for, and the
 * status of a switch whose Role Change event waits for encryption to resume.
 */
#define BT_LINK_REQUESTS	8

struct BtRoleSwitches {
	uint8_t state[BT_LINK_REQUESTS];	/* BT_ROLE_SWITCH_* */
	uint8_t role[BT_LINK_REQUESTS];
	uint8_t status[BT_LINK_REQUESTS];
};

#define BT_ROLE_SWITCH_IDLE	0
#define BT_ROLE_SWITCH_PAUSING	1	/* encryption is being paused */
#define BT_ROLE_SWITCH_RUNNING	2	/* the link controller switches */
#define BT_ROLE_SWITCH_RESUMING	3	/* encryption is being resumed */

extern struct BtRoleSwitches btRoleSwitches;		/* 0x55679 */

/* HCI events the role switch sends. */
#define BT_EVENT_ENCRYPTION_CHANGE	0x08
#define BT_EVENT_ROLE_CHANGE		0x12
#define BT_LMP_PAUSE_ENCRYPTION		0x43	/* btSbcPrepareEntry()'s request; inferred */
#define BT_LINK_RECORD			120	/* a link's record (btHccRomApplyState()) */
#define BT_LINK_RECORD_ADDRESS		7
#define BT_LINK_RECORD_ROLE		20

void btSbcConvertContext(unsigned int status, unsigned int link, unsigned int enabled);	/* 0x4eda4 */
void btSbcVerifyContext(const uint8_t *command);					/* 0x4ee3e */
void btSbcSelectContext(unsigned int status, const uint8_t *address, unsigned int role);	/* 0x4eefc */
uint8_t btSbcAdjustContext(unsigned int link);						/* 0x4ef68 */

int btLscRomPrepareState(unsigned int link);
int btLdrRomHandleState(const uint8_t *record, unsigned int kind);
int btLscRomValidateState(unsigned int link);
int btLdrRomApplyState(unsigned int link, unsigned int role, unsigned int argument);

/*
 * The vendor task's messages (btSbcRecordContext()): the first word is the
 * message's kind. An HCI command (BT_MESSAGE_HCI_COMMAND) carries its opcode
 * at +4, its parameter length at +6 and its parameters from +7.
 */
#define BT_MESSAGE_HCI_COMMAND		5010
#define BT_MESSAGE_P2P_TABLE		5092
#define BT_MESSAGE_LINK_PACKET		5105
#define BT_COMMAND_OPCODE		4
#define BT_COMMAND_LENGTH		6
#define BT_COMMAND_PARAMETERS		7

/* A connection handle's states (two bytes per handle). */
struct BtHandleState {
	uint8_t state;		/* 0: none, 2: disconnecting */
	uint8_t flags;
};

extern struct BtHandleState btHandleStates[];		/* DTCM 0x400b53a */
extern uint8_t btScoDisconnectLinks[];			/* DTCM 0x400f567: the ACL handle of SCO links 8..10 */
extern uint8_t btVendorPolicyFlag;			/* DTCM 0x400f8cc: vendor command 29; 0 adds bits 7..4 of the control word */
extern uint8_t btVendorEventMode;			/* DTCM 0x400f534: 3: report commands the role switch ends */
extern uint8_t btLocalFeatureMode;			/* DTCM 0x400f537: below 4, two local features are hidden */
extern int (*btVendorKeyHook)(unsigned int kind, unsigned int length, uint8_t *out);	/* DTCM 0x400ed5c */
extern uint8_t *btVendorP2pRecords;			/* DTCM 0x400f958: two records of 76 bytes */

int btSbcRecordContext(uint8_t *message);						/* 0x4ef6e */
int btSbcFilterContext(const uint8_t *message);						/* 0x4f1b0 */
void btSbcHandleEntry(void);								/* 0x4f1c2 */
int btSbcProcessEntry(unsigned int status, const uint8_t *parameters, unsigned int group, unsigned int command); /* 0x4f246 */
int btSbcUpdateEntry(void);								/* 0x4f28c */
int btSbcConfigureEntry(void);								/* 0x4f290 */
void btSbcPrepareEntry(unsigned int request, unsigned int link, unsigned int value);	/* 0x4f36a */
void btSbcDispatchEntry(unsigned int event);						/* 0x4f38c */
int btSbcParseEntry(const uint8_t *command);						/* 0x4f3aa */
void btSbcBuildEntry(void);								/* 0x4f490 */

/* Two mdev drivers whose callbacks are all in ROM (DTCM descriptors). */
struct MdevDriver;
extern struct MdevDriver btSbcDrivers[2];		/* DTCM 0x400df10 */
extern uint32_t btSbcDriverStates[2];			/* DTCM 0x400fb38 */
extern const char *btSbcDriverNames[2];			/* DTCM 0x400fb40 */
int btSbcRomAdjustEntry(void);
int btSbcRomRecordEntry(void);
int btSbcRomFilterEntry(void);
int btSbcRomHandleTable(void);
int btSbcRomProcessTable(void);
int btSbcRomConfigureTable(void);
int btSbcRomUpdateTable(void);

void btBdhRomQueryState(const uint8_t *parameters, uint32_t *status);
void btLdaRomDispatchState(unsigned int kind);
void btLdaRomBuildState(uint8_t *out, const uint8_t *in);
void btLdaRomConfigureState(uint8_t *record, const uint8_t *data, unsigned int length);
void btBdhRomResetState(const uint8_t *settings);
unsigned int btHccRomResetState(unsigned int handle);

#define BT_MESSAGE_LINK_REQUEST	80

/* The ROM's command completion: status and the opcode answered. */
int btBdhRomValidateContext(unsigned int status, unsigned int opcode);

/*
 * bt/hcihook.c: the hooks of the Bluetooth host interface installed at
 * start, and vendor command 0xe0's subcommands, each a hook of its own.
 */
#define BT_VENDOR_SUBCOMMANDS		7
#define BT_STATUS_UNSUPPORTED		17	/* HCI status: unsupported feature or parameter value */
#define BT_VENDOR_SUBCOMMAND_GROUP	0xe0
#define BT_EVENT_COMMAND_COMPLETE	14

/* By subcommand 1..7: 0x400ee94, 0x400ee98, 0x400ee90, 0x400ee84, 0x400ee88, 0x400ee80, 0x400ee8c. */
extern int (*btVendorSubcommandHooks[BT_VENDOR_SUBCOMMANDS])(const uint8_t *command);	/* DTCM 0x400ee80 */
extern void (*btLdaResetHook)(void);			/* DTCM 0x400ed50: called while the baseband is reset */
extern void (*btConfigClearHook)(void);			/* DTCM 0x400ea14 */
extern int (*btConfigWriteHook)(unsigned int item, unsigned int length, const uint8_t *value);	/* DTCM 0x400ed60 */
extern int (*btConfigItemHook)(unsigned int item);	/* DTCM 0x400ed58 */

/* The radio registers btSbcProcessPacket() keeps across a reset. */
#define BT_RADIO_KEPT_FIRST	REG16(0x90010a1a)
#define BT_RADIO_KEPT_SECOND	REG16(0x90010a24)
#define BT_RADIO_KEPT_THIRD	REG16(0x90010a1e)
#define SOC_CONTROL_BT_RESET	0x2		/* SOC_CONTROL bit 1, cleared and set again */

void btLdaRomSelectState(void);
void btLscRomComputeState(const uint8_t *message);

void btSbcProcessPacket(void);								/* 0x51c64 */
void btSbcUpdatePacket(const uint8_t *message);						/* 0x51c98 */
int btSbcConfigurePacket(unsigned int item, unsigned int length, uint8_t *value);		/* 0x51cfc */
int btSbcCheckPacket(unsigned int item, unsigned int length, const uint8_t *value);		/* 0x51d0a */
int btSbcValidatePacket(unsigned int item);						/* 0x51d18 */
void btSbcPreparePacket(void);								/* 0x51d20 */
int btSbcDispatchPacket(const uint8_t *command);					/* 0x51d5c */
int btSbcParsePacket(uintptr_t first, uintptr_t second, uintptr_t third, uintptr_t fourth, uintptr_t fifth,
		     uintptr_t sixth);							/* 0x51da0 */
/* A Bluetooth task: what btBdhRomHandleState() creates it with. */
struct BtTask {
	char name[4];
	void (*entry)(void);
	uint16_t stackSize;
	uint8_t priority;
	uint8_t index;
};
#define BT_TASKS		8
#define BT_TASK_STACK_SIZE	8004	/* the tasks' stacks, one after another */
extern uint8_t btTaskStacks[BT_TASK_STACK_SIZE];	/* 0x5b850 */
extern const struct BtTask btTaskTable[BT_TASKS];	/* 0x54634: the tasks, "hct" first */
/* The ROM's task bodies. */
void btLcoRomTask(void);
void btLdaRomTask(void);
void btLscRomTask(void);
void btLdrRomTask(void);
void btLtsRomTask(void);
void btBdhRomTask(void);
void btHccRomTask(void);
void btSbcCheckEntry(unsigned int length, const uint8_t *parameters, uint8_t *status, uint8_t *returnLength,
		     uint8_t *returnParameters);					/* 0x4f294 */
void btSbcValidateEntry(unsigned int length, const uint8_t *parameters, uintptr_t argument,
			uint8_t *returnLength, uint8_t *status, uint8_t *returnParameters);	/* 0x4f318 */
#define BT_MESSAGE_VENDOR_COMMAND	0x140e	/* btSbcCheckEntry()'s message to the HCT task */

#endif /* ROBIN_VENDOR_H */
