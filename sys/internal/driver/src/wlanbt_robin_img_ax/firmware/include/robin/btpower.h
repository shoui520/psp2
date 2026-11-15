/*
 * The Bluetooth controller's transmit level (bt/lcpower.c, 0x42488 - 0x42b3e): the level follows the
 * temperature the SoC's sensor reports, with a hysteresis, and the tables of the radio registers that go
 * with each level. The names of the firmware's placeholders are kept (btLmpLc...State); the roles are
 * inferred from what the code does.
 */
#ifndef ROBIN_BTPOWER_H
#define ROBIN_BTPOWER_H

#include <stdint.h>

/* The state of the level control, 0x55268. */
struct BtPowerState {
	int8_t entry;			/* offset of the range the level is taken in (-8 after the first shift) */
	uint8_t table;			/* the control is set up (btLmpLcParseState) */
	uint8_t buffer;			/* remainder of the level division, the level's base */
	uint8_t packet;			/* step of the level table */
	uint8_t frame;			/* the radio mode was read (1) or not (0) */
	uint8_t header;			/* the second radio mode */
	uint8_t descriptor;		/* the record table was shifted */
	uint8_t queue;			/* 1 while the control is running */
	uint8_t timer;			/* level index of the first channel */
	uint8_t channel;		/* level index of the channel in use */
	uint8_t param;			/* the last temperature sample */
	uint8_t config;			/* the level changed: the radio is to be told */
	int8_t status;			/* direction of the last level change: -1, 0 or 1 */
	uint8_t reserved[3];
	uint32_t control;		/* the controller's rate setting (260 is its full rate) */
};

/* One device's record of the level (ten bytes). */
struct BtPowerValue {
	uint16_t reserved;
	uint16_t index;			/* radio register word of the level */
	uint16_t range;			/* 0 within the first 48 steps of the base level, else 3 */
	uint16_t record;		/* record word of the level */
	uint16_t mode;			/* 6 or 7: which end of the table the level is in */
};

#define BT_POWER_LEVELS		67
#define BT_POWER_MASKS		28	/* the radio registers btLmpLcApplyEntry() sets from the mask table */
#define BT_POWER_PROFILES	10	/* the reference clock profiles (sysRefClockProfile) */
#define BT_POWER_WORDS		76	/* the longest list of radio words */

/* One radio word: the register, and the value it is given. */
struct BtPowerWord {
	uint16_t reg;			/* 0xfff ends the list, 0xffe skips the word */
	uint8_t value;
	uint8_t reserved;
};
#define BT_POWER_DEVICES	10

extern struct BtPowerState btPower;				/* 0x55268 */
extern uint16_t btPowerIndexRegister[BT_POWER_LEVELS];		/* 0x55470 */
extern uint16_t btPowerIndexRecord[BT_POWER_LEVELS];	/* 0x554f6 */
extern uint8_t btPowerLevelHeader;				/* 0x55a32: base of the level index */
extern uint8_t btPowerLevelContext;				/* 0x55797: 7 when the level is fixed */
extern uint8_t btPowerIndexCommand[8];				/* 0x55468 */
extern uint8_t btPowerMaskContext[BT_POWER_MASKS][7];		/* 0x5557c */
extern uint16_t btPowerMaskEntry[BT_POWER_MASKS];		/* 0x55640 */
extern uint8_t btPowerValueRecord;				/* 0x5578c */
extern uint8_t btPowerLevelState[BT_POWER_PROFILES];		/* 0x5578d */
extern uint8_t btPowerLevelEntry;				/* 0x55798 */
extern struct BtPowerWord btPowerLevelPacket[BT_POWER_WORDS];	/* 0x557aa */

extern uint8_t *btLinkRecord;					/* 0x400f95c */
extern uint8_t *btLinkEvent;					/* 0x400f948 */
extern uint16_t *btLinkStatus;					/* 0x400f8f0 */
extern struct BtPowerValue *btLinkValues;			/* 0x400f960 */
extern uint16_t btPeerPacket;					/* 0x400adaa */
extern uint32_t btPeerFrame;					/* 0x400ae0c */
extern uint8_t btPeerBuffer[];					/* 0x400ada0 */
extern uint8_t btDevicePolicy[BT_POWER_DEVICES];		/* 0x400d610 */
extern uint8_t btDeviceRate[BT_POWER_DEVICES];			/* 0x400d5a0 */
extern uint8_t btThresholdPayload;				/* 0x400f429 */
extern int8_t btThresholdRegister;				/* 0x400f42b */
extern uint8_t btThresholdRecord;				/* 0x400f430 */
extern int8_t btModeState;					/* 0x400f431 */
extern uint8_t btModeContext;					/* 0x400f432 */
extern uint8_t btMaskPacket;					/* 0x400f8c0 */
extern uint8_t btMaskFrame;					/* 0x400f8c1 */

/* The hooks btLmpLcManageState() installs (0x400ebbc - 0x400ebe8). */
struct BtPowerHooks {
	uint8_t *(*cipher)(void);			/* 0x400ebbc */
	uint8_t *(*policy)(void);			/* 0x400ebc0 */
	uint8_t *(*profile)(void);			/* 0x400ebc4 */
	uint8_t *(*event)(void);			/* 0x400ebc8 */
	int (*command)(uint32_t a1, uint32_t a2);	/* 0x400ebcc */
	int (*payload)(uint8_t a1);			/* 0x400ebd0 */
	int (*reg)(uint8_t a1);				/* 0x400ebd4 */
	int (*record)(void);				/* 0x400ebd8 */
	int (*state)(uint8_t *a1);			/* 0x400ebdc */
	int (*context)(int a1);				/* 0x400ebe0 */
	uint16_t *(*entry)(int a1, uint8_t a2);		/* 0x400ebe4 */
	unsigned int (*table)(uint8_t a1);		/* 0x400ebe8 */
};
extern struct BtPowerHooks btPowerHooks;
extern unsigned int (*btPowerThresholdBitmap)(void);		/* 0x400f364 */
extern uint8_t *(*btPowerThresholdRate)(uint8_t a1);		/* 0x400f368 */

void btLmpLcHandleState(int state);				/* 0x42488 */
int btLmpLcProcessState(void);					/* 0x424b4 */
unsigned int btLmpLcUpdateState(void);				/* 0x4252e */
uint8_t *btLmpLcConfigureState(uint8_t *first, uint8_t *channel);	/* 0x4255a */
uint8_t *btLmpLcCheckState(uint8_t a1);				/* 0x42616 */
int btLmpLcValidateState(uint8_t *command);			/* 0x42638 */
int btLmpLcPrepareState(void);					/* 0x42662 */
int btLmpLcDispatchState(uint32_t a1, uint32_t a2);		/* 0x4268c */
int btLmpLcParseState(uint32_t a1, uint32_t a2);		/* 0x4269a */
uint8_t *btLmpLcBuildState(void);				/* 0x426fc */
uint8_t *btLmpLcComputeState(void);				/* 0x4270a */
int btLmpLcQueryState(uint8_t a1);				/* 0x42718 */
int btLmpLcResetState(int link);				/* 0x42720 */
uint16_t *btLmpLcSyncState(int link, int index);		/* 0x42782 */
uint16_t *btLmpLcApplyState(int link, uint8_t channel);		/* 0x427da */
unsigned int btLmpLcResolveState(uint8_t channel);		/* 0x427ee */
int btLmpLcScheduleState(uint8_t *timer);			/* 0x42826 */
uint8_t *btLmpLcNotifyState(void);				/* 0x42834 */
uint8_t *btLmpLcFormatState(void);				/* 0x42842 */
int btLmpLcInspectState(void);					/* 0x42852 */
unsigned int btLmpLcTrackState(void);				/* 0x4285e */
unsigned int (*btLmpLcManageState(void))(void);			/* 0x428e0 */
uint8_t *btLmpLcAcquireState(void);					/* 0x42936 */
void btLmpLcReleaseState(void);					/* 0x4296c */
int btLmpLcSetupState(void);					/* 0x42990 */
void btLmpLcClearState(void);					/* 0x429fc */
int btLmpLcConvertState(void);					/* 0x42a1e */
int btLmpLcVerifyState(void);					/* 0x42a58 */
unsigned int btLmpLcSelectState(uint8_t channel);		/* 0x42a70 */
int btLmpLcAdjustState(uint8_t channel);			/* 0x42a90 */
unsigned int btLmpLcFilterState(void);				/* 0x42b3e */

int btLmpLcApplyEntry(void);					/* 0x439a8 */
int btLmpLcResolveEntry(void);					/* 0x43ad6 */
void btLmpLcScheduleEntry(uint8_t value);			/* 0x43b8c */
int btLmpLcNotifyEntry(void);					/* 0x43b94 */

/* Not converted yet. */
void btSbcApplyContext(void);					/* 0x4e85c */
unsigned int btSbcSyncContext(void);				/* 0x4e7c4 */
int btSbcFilterState(int selector, uint8_t *value);		/* 0x4e35e */

#endif
