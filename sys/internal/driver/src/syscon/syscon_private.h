#ifndef SYSCON_PRIVATE_H
#define SYSCON_PRIVATE_H

#include <psp2kern/types.h>
#include <lowio/spi_regs.h>
#include <syscon/syscon.h>
#include <stddef.h>
#include <stdint.h>

/* SceSysconPacket.status, above the caller's flags */
#define SYSCON_PACKET_QUEUED		0x00010000
#define SYSCON_PACKET_TRANSFERRING	0x00020000
#define SYSCON_PACKET_COMPLETION	0x00040000	/* syscon reported the command done */
#define SYSCON_PACKET_DONE		0x00080000
#define SYSCON_PACKET_NO_RESPONSE	0x00100000	/* nothing received */
#define SYSCON_PACKET_TX_LEFT		0x00200000	/* the transmit FIFO did not drain */
#define SYSCON_PACKET_RX_LEFT		0x00400000	/* the receive FIFO did not drain */
#define SYSCON_PACKET_BAD_CHECKSUM	0x00800000
#define SYSCON_PACKET_WRONG_COMPLETION	0x01000000	/* completion for another command */
#define SYSCON_PACKET_RX_OVERFLOW	0x02000000	/* externalRx too small */
#define SYSCON_PACKET_STARTED		0x40000000	/* sent at least once */

#define SYSCON_PACKET_TRANSFER_ERROR	(SYSCON_PACKET_NO_RESPONSE | SYSCON_PACKET_TX_LEFT | \
					 SYSCON_PACKET_BAD_CHECKSUM)
#define SYSCON_PACKET_IN_FLIGHT		(SYSCON_PACKET_STARTED | SYSCON_PACKET_COMPLETION | \
					 SYSCON_PACKET_TRANSFERRING)

/* commands, named after the export that sends each */
#define SYSCON_CMD_NOP				0x0000
#define SYSCON_CMD_GET_ERNIE_VERSION		0x0001
#define SYSCON_CMD_GET_TIME_STAMP		0x0002
#define SYSCON_CMD_GET_SLEEP_FACTOR		0x0003
#define SYSCON_CMD_GET_WAKEUP_FACTOR		0x0010
#define SYSCON_CMD_GET_CLOCK			0x0011
#define SYSCON_CMD_GET_MANUFACTURES_STATUS	0x0015
#define SYSCON_CMD_SET_CLOCK			0x0081
#define SYSCON_CMD_GET_LOG_INFO			0x0086
#define SYSCON_CMD_LOG_START			0x0087
#define SYSCON_CMD_LOG_READ_DATA		0x0088
#define SYSCON_CMD_LOG_START_WAITING		0x0089
#define SYSCON_CMD_READ_SCRATCH_PAD		0x0090
#define SYSCON_CMD_WRITE_SCRATCH_PAD		0x0091
#define SYSCON_CMD_OUTPUT_CLOCK			0x00b0
#define SYSCON_CMD_CTRL_HOST_OUTPUT_VIA_DONGLE	0x00b2
#define SYSCON_CMD_ERNIE_SHUTDOWN		0x00c1
#define SYSCON_CMD_SNVS_READ_DATA		0x00d2
#define SYSCON_CMD_SNVS_WRITE_DATA		0x00d3
#define SYSCON_CMD_GET_CONTROLS_INFO		0x0100
#define SYSCON_CMD_GET_MULTI_CN_INFO		0x0103
#define SYSCON_CMD_SET_MULTI_CN_PORT		0x0190
#define SYSCON_CMD_GET_TOUCHPANEL_DEVICE_INFO	0x0380
#define SYSCON_CMD_GET_TOUCHPANEL_DEVICE_INFO2	0x0390
#define SYSCON_CMD_MOTION_GET_MEASURE_DATA	0x0400
#define SYSCON_CMD_MOTION_GET_DEVICE_INFO	0x0480
#define SYSCON_CMD_GET_USB_DET_STATUS		0x0805
#define SYSCON_CMD_CTRL_DOLCE_LED		0x0820	/* + the mode, 0-2 */
#define SYSCON_CMD_GET_MANUAL_CHARGE_MODE	0x0840
#define SYSCON_CMD_GET_ELMO_VERSION		0x0880
#define SYSCON_CMD_GET_COOKIE_VERSION		0x0881
#define SYSCON_CMD_GET_POWER_STATUS		0x0883
#define SYSCON_CMD_CTRL_HDMI_CEC_POWER		0x0886
#define SYSCON_CMD_CTRL_MOTION_SENSOR_POWER	0x0887
#define SYSCON_CMD_CTRL_SD_POWER		0x0888
#define SYSCON_CMD_CTRL_ACC_POWER		0x0889
#define SYSCON_CMD_CTRL_WIRELESS_POWER		0x088a
#define SYSCON_CMD_CTRL_VOLTAGE			0x088e
#define SYSCON_CMD_CTRL_DEVICE_RESET		0x088f
#define SYSCON_CMD_CTRL_LED			0x0891
#define SYSCON_CMD_CTRL_CHARGE_AC		0x0892
#define SYSCON_CMD_CTRL_CHARGE_VBUS		0x0893
#define SYSCON_CMD_READ_ELMO_REG		0x0895
#define SYSCON_CMD_WRITE_ELMO_REG		0x0896
#define SYSCON_CMD_READ_COOKIE_REG		0x0897
#define SYSCON_CMD_WRITE_COOKIE_REG		0x0898
#define SYSCON_CMD_CTRL_USB_STATUS		0x089a
#define SYSCON_CMD_CTRL_RMR_POWER		0x089b
#define SYSCON_CMD_CTRL_DEV_KIT_USB_POWER	0x089c
#define SYSCON_CMD_CTRL_LED_PWM_BLINK		0x089d
#define SYSCON_CMD_SET_CHARGE_LED_CTRL		0x089e
#define SYSCON_CMD_CTRL_LED_BLINK_TYPE2		0x089f
#define SYSCON_CMD_GET_BATTERY_SOH_OLD		0x08a4	/* syscon < 0x70504 */
#define SYSCON_CMD_ENABLE_HIBERNATE_IO		0x08b0
#define SYSCON_CMD_CTRL_MANUAL_CHARGE_MODE	0x08c0
#define SYSCON_CMD_CTRL_DOLCE_USB_POWER		0x08c5
#define SYSCON_CMD_READ_COOKIE_STATUS		0x08c6
#define SYSCON_CMD_CTRL_USB_SUPPLY		0x08c8
#define SYSCON_CMD_GET_TEMPERATURE_LOG		0x08ce
#define SYSCON_CMD_CLEAR_TEMPERATURE_LOG	0x08cf
#define SYSCON_CMD_GET_BATTERY_LIFE_PERCENT	0x0902
#define SYSCON_CMD_GET_BATTERY_REMAIN_LEVEL	0x0903
#define SYSCON_CMD_GET_BATTERY_CALIB_DATA	0x0904
#define SYSCON_CMD_GET_BATTERY_SOH		0x0985
#define SYSCON_CMD_BATTERY_SW_RESET		0x0989
#define SYSCON_CMD_GET_CURRENT_FULL_CAP		0x098a
#define SYSCON_CMD_GET_ABBY_TEMP		0x098b
#define SYSCON_CMD_GET_BATTERY_CYCLE_COUNT	0x098c
#define SYSCON_CMD_BATTERY_START_BL_MODE	0x09b0
#define SYSCON_CMD_BATTERY_STOP_BL_MODE		0x09b1
#define SYSCON_CMD_BATTERY_SET_BL_COMMAND	0x09b2
#define SYSCON_CMD_BATTERY_EXEC_BL_COMMAND	0x09b3
#define SYSCON_CMD_BATTERY_READ_BL_COMMAND	0x09b4
#define SYSCON_CMD_NVS_SET_RUN_MODE		0x1080
#define SYSCON_CMD_NVS_READ_DATA		0x1082
#define SYSCON_CMD_NVS_WRITE_DATA		0x1083
#define SYSCON_CMD_GET_ERNIE_DL_VERSION		0x1100
#define SYSCON_CMD_UPDATER_SET_SEGMENT		0x1180
#define SYSCON_CMD_UPDATER_EXEC_PROGRAMMING	0x1182
#define SYSCON_CMD_UPDATER_SET_RUN_MODE		0x1183
#define SYSCON_CMD_UPDATER_EXEC_FINALIZE	0x1184
#define SYSCON_CMD_UPDATER_CHECK_SIGNATURE	0x1185
#define SYSCON_CMD_GET_CONFIG_STORAGE_INFO	0x1300
#define SYSCON_CMD_BEGIN_CONFIGSTORAGE_TRANSACTION	0x1382
#define SYSCON_CMD_END_CONFIGSTORAGE_TRANSACTION	0x1383
#define SYSCON_CMD_COMMIT_CONFIGSTORAGE_TRANSACTION	0x1384
#define SYSCON_CMD_LOAD_CONFIGSTORAGE_SCRIPT	0x1385
#define SYSCON_CMD_VERIFY_CONFIGSTORAGE_SCRIPT	0x1386
#define SYSCON_CMD_JIG_OPEN_PORT		0x2080
#define SYSCON_CMD_JIG_CLOSE_PORT		0x2081
#define SYSCON_CMD_JIG_SET_CONFIG		0x2082

static inline void sysconPacketSetCommand(SceSysconPacket *packet, uint16_t command)
{
	packet->request.command[0] = command;
	packet->request.command[1] = command >> 8;
}

/* the value is the notification's new state */
typedef void (*SceSysconCallback)(int value, void *argp);

typedef struct SceSysconCallbackEntry {
	SceSysconCallback callback;
	void *argp;
} SceSysconCallbackEntry;

typedef struct SceSysconDebugHandlers {
	uint32_t size;
	void (*start)(SceSysconPacket *packet);
	void (*end)(SceSysconPacket *packet);
} SceSysconDebugHandlers;

typedef void (*SysconLogger)(uint32_t event, uint32_t count, ...);

struct SceKernelDebugInfo;
void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
				 const struct SceKernelDebugInfo *info, const void *lr,
				 const char *fmt, ...);

/*
 * notificationState[]: one entry per notification bit, -1 until the first
 * report. The names come from the exports that read them; the others are
 * named after their bit (Nb_BITn: notification byte b, bit n).
 */
enum SysconState {
	SYSCON_STATE_WLAN_BT = 0,		/* SceWlanBt skips its setup without it */
	SYSCON_STATE_N0_BIT1,
	SYSCON_STATE_N0_BIT2,
	SYSCON_STATE_N0_BIT3,			/* read by SceRtc */
	SYSCON_STATE_N0_BIT4,
	SYSCON_STATE_MULTI_CN,			/* the multi connector: multiCnInfo */
	SYSCON_STATE_N0_BIT6,
	SYSCON_STATE_N1_BIT0,
	SYSCON_STATE_N1_BIT1,
	SYSCON_STATE_N1_BIT2,
	SYSCON_STATE_N1_BIT6,			/* command130Info */
	SYSCON_STATE_N1_BIT7,
	SYSCON_STATE_LOW_BATTERY,
	SYSCON_STATE_POWER_SOURCE,
	SYSCON_STATE_N2_BIT2,
	SYSCON_STATE_N2_BIT3,
	SYSCON_STATE_N2_BIT4,
	SYSCON_STATE_USB_POWER,
	SYSCON_STATE_N2_BIT6,
	SYSCON_STATE_THERMAL_ALERT,
	SYSCON_STATE_N3_BIT1,
	SYSCON_STATE_N3_BIT2,
	SYSCON_STATE_N3_BIT3,
	SYSCON_STATE_LOW_BATTERY_INHIBIT_UPDATE_REBOOT,
	SYSCON_STATE_LOW_BATTERY_INHIBIT_UPDATE_DOWNLOAD,
	/* from the power status word, not the notification bits */
	SYSCON_STATE_POWER_BIT14,
	SYSCON_STATE_POWER_BIT12,
	SYSCON_STATE_HEADPHONE,
	SYSCON_STATE_MOTION_DEV,
	SYSCON_STATE_COUNT
};

/* statusFlags */
#define SYSCON_STATUS_POWER_ONLINE	0x08
#define SYSCON_STATUS_DOWNLOADER_MODE	0x40

/* the callback slots after the notification bits */
#define SYSCON_CALLBACK_POWER		32	/* 32..35: SYSCON_STATE_POWER_BIT14.. */
#define SYSCON_CALLBACK_STATUS		36
#define SYSCON_CALLBACK_MULTI_CN	37
#define SYSCON_CALLBACK_COMMAND130	38
#define SYSCON_CALLBACK_COUNT		39

/* Native GPIO completion and registration use this same control object. */
struct SysconControl {
	SceSysconPacket *currentPacket;
	SceSysconPacket *queueHeads[18];
	SceSysconPacket *queueTails[18];
	const SceSysconDebugHandlers *debugHandlers;
	volatile SceSpiRegisters *spiRegisters;
	uint32_t lastTransferTime;
	uint32_t startupPacketDelay;
	uint32_t commandPacketDelay;
	uint32_t notificationBits;
	uint32_t multiCnInfo;
	uint32_t command130Info;
	uint32_t command884State;
	uint32_t command885State;
	uint8_t statusFlags;			/* SYSCON_STATUS_* */
	uint8_t pendingStatus;
	uint8_t interruptActive;
	uint8_t stopping;
	uint8_t nextQueue;
	uint8_t pendingCompletions;
	int8_t notificationState[SYSCON_STATE_COUNT];
	uint8_t pendingCommand130;
	uint8_t pendingMultiCnInfo;
	uint8_t reserved0[3];
	SceSysconCallbackEntry callbacks[SYSCON_CALLBACK_COUNT];
	uint8_t reserved1[8];
	uint32_t baryonVersion;
	uint32_t hardwareInfo;
	uint32_t hardwareFlags[4];
	uint8_t baryonTimestampText[16];
	uint32_t reserved2;
	uint64_t baryonTimestamp __attribute__((aligned(4), packed));
	uint32_t polling;
	uint32_t affirmativeRertyMode;
	SceKernelSpinlock callbackLock;
	int spiMemblock;
	int mutex;
	int32_t jigPortState;
	volatile uint32_t initialized;
	uint32_t reserved3;
	SysconLogger logger;
};

_Static_assert(offsetof(struct SysconControl, notificationState) == 0xc2,
	       "SysconControl notificationState");
_Static_assert(offsetof(struct SysconControl, callbacks) == 0xe4,
	       "SysconControl callbacks");
_Static_assert(offsetof(struct SysconControl, baryonVersion) == 0x224,
	       "SysconControl baryonVersion");
_Static_assert(offsetof(struct SysconControl, callbackLock) == 0x260,
	       "SysconControl callbackLock");
_Static_assert(sizeof(struct SysconControl) == 0x27c,
	       "SysconControl size");

extern struct SysconControl s_sysconControl;

/* battery.c */
int ksceSysconGetBaryonVersion(void);
int ksceSysconGetHardwareInfo(void);
int ksceSysconIsDownLoaderMode(void);

#endif
