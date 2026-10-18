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

/* sceSysconCmdExec/ExecAsync flags; they become the low half of the status */
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

/* a notification callback; the value is the notification's new state */
typedef void (*SceSysconCallback)(int value, void *argp);

/* SceSysconForDriver_F245CD6F: called around each packet */
typedef struct SceSysconDebugHandlers {
	uint32_t size;
	void (*start)(SceSysconPacket *packet);
	void (*end)(SceSysconPacket *packet);
} SceSysconDebugHandlers;

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

int SceSysconForDriver_55DF1C9B(void);
int sceSysconCmdExec(SceSysconPacket *packet, uint32_t flags);
int SceSysconForDriver_C2224E82(SceSysconPacket *packet, uint32_t flags,
			   int (*callback)(SceSysconPacket *packet, void *argp),
			   void *argp);
int SceSysconForDriver_FF86F4C5(void);
int SceSysconForDriver_CBD6D8BC(void);
int SceSysconForDriver_40FF3898(uint32_t device, uint32_t mode);
int SceSysconForDriver_063425AE(uint32_t value);
int SceSysconForDriver_76506A57(uint32_t mode, uint32_t offset,
				 void *data, uint32_t size);

/* name unknown: a syscon state word; (state & 0x7f) == 0x17 after a crash reboot */
int SceSysconForDriver_CF5B2F2F(uint32_t *state);

int SceSysconForDriver_63B14156(void);
int SceSysconForDriver_99A254A9(void);

int SceSysconForDriver_030D447F(SceKernelTouchpanelDeviceInfo2 *info);	/* both touch panels' device info (newer syscon) */
int SceSysconForDriver_0396BA09(void);
int SceSysconForDriver_03C50DC3(void);
int SceSysconForDriver_060E55C1(void);
int SceSysconForDriver_0D0B6D25(void);
int SceSysconForDriver_10327C64(void);
int SceSysconForDriver_129EA022(SceSysconCallback callback, void *argument);
int SceSysconForDriver_142D5E82(void);
int SceSysconForDriver_14B99945(void);
int SceSysconForDriver_1546A141(int first, int second);
int SceSysconForDriver_16A466E7(uint32_t index, uint32_t *value);
int SceSysconForDriver_1C5D466C(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_23558805(void);
int SceSysconForDriver_25144105(uint32_t *value);
int SceSysconForDriver_27758A64(void);
int SceSysconForDriver_2A61ACE1(uint32_t *value);
int SceSysconForDriver_2BCE7F5A(uint32_t index, uint32_t *value);
int SceSysconForDriver_2D471528(SceSysconCallback callback, void *argument);
int SceSysconForDriver_3168F3AF(uint32_t *value, uint32_t *extension);	/* RTC alarm: the stored alarm and its word */
int SceSysconForDriver_31B8F7BE(uint32_t index, uint32_t *status, uint16_t *first, uint16_t *second);
int SceSysconForDriver_356B9696(const void *data, uint32_t offset, uint32_t size);
int SceSysconForDriver_35E1689F(SceSysconCallback callback, void *argument);
int SceSysconForDriver_373ECF8A(uint32_t value);
int SceSysconForDriver_3B06F619(uint32_t first, uint32_t second, const void *data, uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_3FDD29D6(uint32_t mask, int enabled);
int SceSysconForDriver_41942B50(uint32_t *value);
int SceSysconForDriver_48ED8981(void);
int SceSysconForDriver_4A184B7C(void);
int SceSysconForDriver_4A42712F(SceSysconCallback callback, void *argument);
int SceSysconForDriver_4D03754A(const void *request, uint32_t requestSize, void *reply, uint32_t replySize);
int SceSysconForDriver_4DFA12FB(uint32_t first, uint32_t second, const void *data, uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_4E0EB57B(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_4E88B4D9(SceSysconCallback callback, void *argument);
int SceSysconForDriver_50CAE242(void);
int SceSysconForDriver_51164951(uint32_t value, uint32_t extension);	/* RTC alarm: seconds * 2 until it (-1: none) and a caller word kept with it */
int SceSysconForDriver_55B6FEF5(uint32_t first, uint32_t second, uint32_t size, void *reply, uint32_t *replyLength);
int SceSysconForDriver_567865A3(uint32_t *value);
int SceSysconForDriver_5946B29B(void);
int SceSysconForDriver_5CDDA14D(uint32_t value);
int SceSysconForDriver_63352A39(SceSysconCallback callback, void *argument);
int SceSysconForDriver_63683B9B(const void *request, uint32_t requestSize, void *reply, uint32_t replySize);
int SceSysconForDriver_6450D6BE(uint32_t index, uint32_t *value);
int SceSysconForDriver_67A4CB9F(SceSysconCallback callback, void *argument);
int SceSysconForDriver_68366D29(uint32_t index, uint32_t *value);
int SceSysconForDriver_6D408E30(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_734544E4(const void *data, uint32_t offset, uint32_t size);
int SceSysconForDriver_7EC67149(uint32_t first, uint32_t second, uint32_t size, const void *data, uint32_t *replyLength);
int SceSysconForDriver_80D6E061(SceSysconCallback callback, void *argument);
int SceSysconForDriver_830A9347(uint8_t *value);
int SceSysconForDriver_8351526D(SceSysconCallback callback, void *argument);	/* RTC alarm event handler */
int SceSysconForDriver_856DFC0E(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_86BAAF7D(void);	/* whether the RTC alarm fired (1) */
int SceSysconForDriver_8874EF45(void);
int SceSysconForDriver_8B0B4A97(uint32_t *value);
int SceSysconForDriver_8EBA8A26(uint32_t offset, uint32_t *replyLength);
int SceSysconForDriver_94418CB4(uint32_t first, uint32_t second, uint32_t size, const void *data, uint32_t *replyLength);
int SceSysconForDriver_9962CE6F(uint32_t index, uint32_t *status, uint16_t *first, uint16_t *second);
int SceSysconForDriver_998ECD6D(uint32_t *value);
int SceSysconForDriver_9A28BEEF(void);
int SceSysconForDriver_9A4304F2(uint8_t *information);
int SceSysconForDriver_9B6B8BB9(uint32_t *value);	/* front touch panel firmware version */
int SceSysconForDriver_9C840439(uint32_t *value);
int SceSysconForDriver_9CA6EB70(uint32_t enabled);
int SceSysconForDriver_9DA2A5AB(void);
int SceSysconForDriver_9F8340FF(SceSysconCallback callback, void *argument);
int SceSysconForDriver_A26586B2(SceSysconCallback callback, void *argument);
int SceSysconForDriver_A2FE9BF9(uint32_t *value);
int SceSysconForDriver_ACC7F71E(void);
int SceSysconForDriver_ACEE1C70(void);
int SceSysconForDriver_AE127A96(uint32_t *value);
int SceSysconForDriver_B3960346(uint8_t *value);
int SceSysconForDriver_B63AF451(uint32_t offset, uint32_t *replyLength);
int SceSysconForDriver_B9835A63(uint32_t first, uint32_t second, const void *data, uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_B9EA2FA8(void);
int SceSysconForDriver_BC1D93D7(uint32_t *value);
int SceSysconForDriver_C0F215B7(void);
int SceSysconForDriver_C2955814(uint8_t *information);
int SceSysconForDriver_C3504ADE(void);
int SceSysconForDriver_C4A61241(uint32_t *value);	/* back touch panel firmware version */
int SceSysconForDriver_C9DB5BFF(SceSysconPacket *packet);
int SceSysconForDriver_CA203A5F(uint32_t first, uint32_t second, uint32_t size, void *reply, uint32_t *replyLength);
int SceSysconForDriver_CD6A6BC7(uint32_t first, uint32_t second, const void *data, uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_CEE2B8F7(uint32_t *value);
int SceSysconForDriver_D2F456DC(uint8_t *information);
int SceSysconForDriver_D632D197(uint32_t *value);
int SceSysconForDriver_D7F5A797(void);
int SceSysconForDriver_D9B11C0A(uint32_t *value);
int SceSysconForDriver_DE613081(SceSysconCallback callback, void *argument);
int SceSysconForDriver_DFB024C4(uint32_t *value);
int SceSysconForDriver_E38045F5(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_EBDF88B9(const void *request, uint32_t requestSize, void *reply, uint32_t replySize);
int SceSysconForDriver_EBE3262C(void);
int SceSysconForDriver_F162D3D6(uint32_t *value);
int SceSysconForDriver_F99BC858(uint32_t *value);
int SceSysconForDriver_FB37D7AD(uint32_t *value);
int SceSysconForDriver_74B2AB55(uint32_t offset);
int SceSysconForDriver_448DAFF1(uint32_t offset, uint32_t first, uint32_t second, void *data, uint32_t size);
int SceSysconForDriver_87DA378D(void);
int SceSysconForDriver_E4F29744(uint32_t offset, uint32_t mode, const void *data, uint32_t size);
int SceSysconForDriver_2CEF078E(void);
int SceSysconForDriver_E4AE7852(void);
int SceSysconForDriver_A4968B8C(void);
int SceSysconForDriver_3843D657(uint32_t value);
int sceSysconCmdSync(SceSysconPacket *packet, int noWait);
int SceSysconForDriver_7B9B3617(void);
int SceSysconForDriver_8D1D97E8(uint32_t value);
int SceSysconForDriver_B1F88B11(uint32_t value);
int SceSysconForDriver_727F985A(uint32_t mode);
int SceSysconForDriver_3274A925(uint32_t value);
int SceSysconForDriver_62155962(uint32_t value);
int SceSysconForDriver_04EC7579(uint32_t led, int enabled);
int SceSysconForDriver_C6A2C9EF(uint32_t value);
int SceSysconForDriver_4946538A(uint32_t value);
int SceSysconForDriver_FCC3E8EE(void);
int SceSysconForDriver_9ADC9936(uint32_t *first, uint32_t *second, uint32_t *third, uint32_t *fourth);
int SceSysconForDriver_68E0031E(uint32_t *first, uint32_t *second, uint32_t *third);
int SceSysconForDriver_965C68C3(uint32_t *information);	/* one 16-byte hardware-information buffer */
int SceSysconForDriver_701535FC(uint32_t *information);
int SceSysconForDriver_4FEC564C(uint32_t *value);
int SceSysconForDriver_3E09A1F4(uint32_t *value);
int SceSysconForDriver_D6F6D472(uint32_t *information);
int SceSysconForDriver_1503D6A0(uint32_t *information);
int SceSysconForDriver_3B354824(uint32_t *information);
int SceSysconForDriver_F492E69E(uint16_t *information);
int SceSysconForDriver_EF810687(uint32_t *value);
int SceSysconForDriver_34574496(void);
int SceSysconForDriver_956D07CB(void);
int SceSysconForDriver_9ADD60D2(void);
int SceSysconForDriver_1E3130EE(void);
int SceSysconForDriver_1A0C140F(void);	/* returns a sign-extended byte */
int SceSysconForDriver_89C5CFD6(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_487D97F3(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_4E55CF5E(void);
int SceSysconForDriver_9C0B1E61(void);
int SceSysconForDriver_ACAFA2B8(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_81A6060D(uint32_t value);
int SceSysconForDriver_10C9657A(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_E26488B9(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_8D7724C0(void);
int SceSysconForDriver_6D65B70F(void);
int SceSysconForDriver_D27C3D80(const uint8_t *data, uint32_t size, uint32_t *checksum);
int SceSysconForDriver_C7747A63(const uint32_t *information, uint32_t size);
int SceSysconForDriver_69AD76E4(uint32_t value);
int SceSysconForDriver_B487C2FB(uint32_t mode);
int SceSysconForDriver_9B00BC7F(uint32_t segment);
int SceSysconForDriver_CC6F90A8(uint32_t offset, const void *data, uint32_t size);

int SceSysconForDriver_00A65FC1(uint32_t *value);
int SceSysconForDriver_00AE3AEB(uint32_t value);
int SceSysconForDriver_010F95D9(uint32_t value);
int SceSysconForDriver_012B57B3(void);
int SceSysconForDriver_02350352(uint32_t index);
int SceSysconForDriver_03F11220(uint32_t *voltage);
int SceSysconForDriver_058941D7(uint32_t value);
int SceSysconForDriver_0826BA07(int32_t *current);
int SceSysconForDriver_08A561C7(uint32_t *information);
int SceSysconForDriver_0D300158(uint32_t value);
int SceSysconForDriver_0D58E297(uint32_t polling);
int SceSysconForDriver_14730196(SceSysconCallback callback, void *argument);
int SceSysconForDriver_154676F1(SceSysconCallback callback, void *argument);
int SceSysconForDriver_175CE5A1(uint32_t *information);
int SceSysconForDriver_18A6F4D9(SceSysconCallback callback, void *argument);
int SceSysconForDriver_1C29C00E(uint32_t *value);
int SceSysconForDriver_1E08A40F(void);
int SceSysconForDriver_229A07C2(SceSysconCallback callback, void *argument);
int SceSysconForDriver_240A604E(uint32_t value);
int SceSysconForDriver_253CC522(uint8_t *value);
int SceSysconForDriver_2659535C(void);
int SceSysconForDriver_26E36625(uint32_t offset, uint32_t size, const void *data);
int SceSysconForDriver_26F9D729(uint32_t *maximum, uint32_t *level);
int SceSysconForDriver_270B7B0B(void *reply, uint32_t capacity);
int SceSysconForDriver_285594F8(uint32_t value);
int SceSysconForDriver_29CF4335(void);
int SceSysconForDriver_2A4B0437(uint32_t control, uint32_t enabled);
int SceSysconForDriver_2E4A2198(void);
int SceSysconForDriver_2E4BA4B8(SceSysconCallback callback, void *argument);
int SceSysconForDriver_2E6D97CD(uint32_t *value);
int SceSysconForDriver_2EC6D55D(uint32_t value);
int SceSysconForDriver_32B2DB3D(void);
int SceSysconForDriver_33B5CDB3(uint32_t *value);
int SceSysconForDriver_351946B0(uint32_t *value);
int SceSysconForDriver_357CC9D9(uint32_t first, uint32_t second);
int SceSysconForDriver_3664E2C0(uint32_t *value);
int SceSysconForDriver_36AC812B(uint32_t offset, uint32_t size, void *data);
int SceSysconForDriver_376CCCB8(SceSysconCallback callback, void *argument);
int SceSysconForDriver_3B57B777(uint32_t *information);
int SceSysconForDriver_3BAAC8A9(SceSysconCallback callback, void *argument);
int SceSysconForDriver_3C3B949C(uint32_t *value);
int SceSysconForDriver_3C80B529(uint32_t offset, uint32_t size, void *data);
int SceSysconForDriver_423D0C58(SceSysconCallback callback, void *argument);
int SceSysconForDriver_4278E614(unsigned int value);
int SceSysconForDriver_4295D497(void);
int SceSysconForDriver_42E599AC(uint32_t value);
int SceSysconForDriver_451C1662(uint32_t value);
int SceSysconForDriver_474A9EA7(SceSysconCallback callback, void *argument);
int SceSysconForDriver_490C5548(void);
int SceSysconForDriver_4BC63A40(void);
int SceSysconForDriver_596B17B7(uint32_t value);
int SceSysconForDriver_59DC5938(uint32_t value);
int SceSysconForDriver_5A614349(uint32_t value);
int SceSysconForDriver_5BF765BB(uint32_t index, uint16_t *value);
int SceSysconForDriver_61AE3970(SceSysconCallback callback, void *argument);
int SceSysconForDriver_6A093056(uint32_t *value);
int SceSysconForDriver_6F586D1A(uint32_t led, uint32_t level);
int SceSysconForDriver_730E4725(uint32_t *value);
int SceSysconForDriver_76272CB9(uint32_t *information);
int SceSysconForDriver_7682FE69(SceSysconCallback callback, void *argument);
int SceSysconForDriver_769F9AC4(void);
int SceSysconForDriver_79074DE4(int32_t *value);
int SceSysconForDriver_79E6DD8B(uint32_t mask, int enabled);
int SceSysconForDriver_7AA00C01(SceSysconCallback callback, void *argument);
int SceSysconForDriver_7BAFE083(uint32_t value);
int SceSysconForDriver_7BFA95DA(uint32_t value);
int SceSysconForDriver_7BFBA09E(uint32_t mode, uint32_t offset, uint32_t size, const void *data);
int SceSysconForDriver_7D25F6D2(uint32_t index, uint8_t *value);
int SceSysconForDriver_7DE84CE3(uint32_t index);
int SceSysconForDriver_7F198FA2(uint8_t rail, uint32_t code);
int SceSysconForDriver_85E5DEBF(SceSysconCallback callback, void *argument);
int SceSysconForDriver_87FF8041(uint32_t value);
int SceSysconForDriver_8A97E690(const uint32_t *information);
int SceSysconForDriver_901D6CD4(uint32_t mode);
int SceSysconForDriver_9070F139(uint32_t *temperature);
int SceSysconForDriver_91D3B7A3(uint32_t *health, uint32_t *status);
int SceSysconForDriver_91EF4EC3(uint32_t *value);
int SceSysconForDriver_92D2C6A4(void);
int SceSysconForDriver_93075DD1(uint32_t *value);
int SceSysconForDriver_933D813F(uint32_t mode, uint32_t offset, uint32_t size);
int SceSysconForDriver_93FE51F5(uint32_t *value);
int SceSysconForDriver_94678881(SceSysconCallback callback, void *argument);
int SceSysconForDriver_94AB13CC(uint32_t value);
int SceSysconForDriver_95975DD1(uint32_t index, uint16_t *value);
int SceSysconForDriver_95C16911(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_9A4F4B7C(void);
int SceSysconForDriver_9A7858B6(uint32_t control, uint32_t value);
int SceSysconForDriver_9B6A6F64(uint32_t value);
int SceSysconForDriver_9B779DB0(uint32_t index, uint32_t value);
int SceSysconForDriver_9BF78047(uint32_t *value);
int SceSysconForDriver_9F4042F8(void);
int SceSysconForDriver_9F9156BE(const void *request, void *reply);
int SceSysconForDriver_A039B563(uint32_t *value);
int SceSysconForDriver_A1F1B973(void);
int SceSysconForDriver_A2E85DB9(uint32_t value);
int SceSysconForDriver_A57B5433(void);
int SceSysconForDriver_A5AB19B1(uint32_t index, uint16_t *value);
int SceSysconForDriver_A6F05153(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_AD0A8275(uint32_t *value);
int SceSysconForDriver_B7BCC638(void);
int SceSysconForDriver_B7E294AF(uint32_t *information);
int SceSysconForDriver_B832B72C(void);
int SceSysconForDriver_B841C141(uint32_t *time);
int SceSysconForDriver_B872E904(uint32_t value);
int SceSysconForDriver_B8F4F4E3(uint32_t value);
int SceSysconForDriver_BA09F171(uint32_t *first, uint32_t *second);
int SceSysconForDriver_BF5CDB61(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_BFDA5590(void);
int SceSysconForDriver_C14BD637(void);
int SceSysconForDriver_C2FB5565(uint32_t index, uint16_t *value);
int SceSysconForDriver_C314B64B(uint32_t *information);
int SceSysconForDriver_C442D0BE(SceSysconCallback callback, void *argument);
int SceSysconForDriver_C50568E9(void);
int SceSysconForDriver_C51B4E6F(uint32_t *information);
int SceSysconForDriver_C562AF3A(uint32_t *capacity);
int SceSysconForDriver_CB41B531(uint32_t first, uint32_t second, uint32_t third, uint32_t fourth);
int SceSysconForDriver_CBA836FF(const uint32_t *information, uint32_t size);
int SceSysconForDriver_CCA56A16(uint32_t first, uint32_t second);
int SceSysconForDriver_CCC71C28(uint32_t index, uint32_t value);
int SceSysconForDriver_CD73079D(uint32_t *value);
int SceSysconForDriver_CE346793(uint32_t size, const void *data);
int SceSysconForDriver_CE48E8EB(uint16_t *information);
int SceSysconForDriver_CFCEE733(uint32_t value);
int SceSysconForDriver_D01E64FC(uint32_t *value);
int SceSysconForDriver_D2ADABCA(uint32_t index, uint32_t value);
int SceSysconForDriver_D7BEFF8B(uint32_t *value);
int SceSysconForDriver_DD16ABD9(uint32_t value);
int SceSysconForDriver_E0D52DF0(SceSysconCallback callback, void *argument);
int SceSysconForDriver_E1885F68(uint32_t *value);
int SceSysconForDriver_E7893732(uint32_t value);
int SceSysconForDriver_E7F5D3DC(uint32_t *value);
int SceSysconForDriver_F39300D3(uint32_t index, uint32_t value);
int SceSysconForDriver_F648BE61(const void *request, void *reply);
int SceSysconForDriver_F6D4DDC4(uint32_t *value);
int SceSysconForDriver_F87679EE(uint32_t value);
int SceSysconForDriver_F93CF833(uint32_t *value);
int SceSysconForDriver_F9584DC9(uint32_t mode, uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_FD65FFCB(uint32_t index, uint32_t mode);
int SceSysconForDriver_FDB3AE9D(uint32_t *information);
int SceSysconForDriver_DECCB2B4(uint32_t value);
int sceSysconCtrlRMRPower(uint32_t enabled);
int sceSysconCtrlSdPower(uint32_t enabled);
uint64_t SceSysconForDriver_4D588A0A(void);
int SceSysconForDriver_145F59A4(uint32_t *information);
int SceSysconForDriver_483FAE05(void);
int SceSysconForDriver_44A173F5(void);
int SceSysconForDriver_D24BF916(uint32_t first, uint32_t second);
int SceSysconForDriver_299B1CE7(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_32418370(SceSysconCallback callback, void *argument);
int SceSysconForDriver_F245CD6F(const SceSysconDebugHandlers *handlers);
int SceSysconForDriver_3F0DB7C0(SceSysconCallback callback, void *argument);
int SceSysconForDriver_8AAB6308(uint32_t port);
int sceSysconSetPowerMode(uint32_t mode, uint32_t argument);
int SceSysconForDriver_773B8126(SceSysconCallback callback, void *argument);
