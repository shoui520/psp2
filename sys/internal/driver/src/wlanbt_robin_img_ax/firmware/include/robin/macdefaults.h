/*
 * The MAC's register defaults and the local station's (wlan/macdefaults.c,
 * 0x4c80c..0x4cb36): the register table wlanMacBringUp() applies, the rate
 * tables the ROM keeps per rate, the transmit control word, the power-save
 * wake timer and the BSS capability check of the scan.
 *
 * Marvell published no source; names are inferred from use.
 */
#ifndef ROBIN_MACDEFAULTS_H
#define ROBIN_MACDEFAULTS_H

#include <stdint.h>
#include "robin/mactx.h"

/* A register of the MAC's block set to a value under a mask when the MAC starts. */
struct MacRegisterDefault {
	uint16_t offset;	/* from the register block; 0xffff ends the table */
	uint16_t reserved;
	uint32_t mask;		/* the bits replaced */
	uint32_t value;
};

_Static_assert(sizeof(struct MacRegisterDefault) == 12, "MAC register default");

#define MAC_REGISTER_DEFAULTS	62

/*
 * The transmit control word fmCoexResolveBuffer() builds for MAC 0x088. What
 * each field selects is not known; the names say which bits they hold.
 */
union MacTxControl {
	uint32_t word;
	struct {
		uint32_t enable : 1;	/* 1 */
		uint32_t mode : 2;	/* 0 */
		uint32_t rate : 7;	/* 0 */
		uint32_t option : 1;	/* 0 */
		uint32_t format : 2;	/* 1 */
		uint32_t priority : 2;	/* 0 */
		uint32_t last : 1;	/* 1 */
		uint32_t antenna : 4;	/* wlanTxPowerRateClass(14) */
		uint32_t flagA : 1;	/* 0 */
		uint32_t kept : 1;	/* left as it was */
		uint32_t flagB : 1;	/* 0 */
		uint32_t flagC : 1;	/* 0 */
		uint32_t upper : 8;	/* left as it was */
	} bits;
};

_Static_assert(sizeof(union MacTxControl) == 4, "transmit control word");

extern struct MacRegisterDefault macRegisterDefaults[MAC_REGISTER_DEFAULTS];	/* DTCM 0x40009a8 */
extern uint32_t wlanIbssAtimTimer;		/* 0x5540c: osaCreateTimer() handle of the ATIM window */
extern uint32_t macDefaultsTxControlSet;	/* 0x55410: fmCoexApplyBuffer() set the transmit control */

uint32_t fmCoexCheckBuffer(int on);				/* 0x4c80c */
uint32_t fmCoexValidateBuffer(void);				/* 0x4c820 */
uint32_t fmCoexPrepareBuffer(void);				/* 0x4c830 */
uint32_t fmCoexDispatchBuffer(void);				/* 0x4c850 */
void fmCoexParseBuffer(void);					/* 0x4c85c */
int fmCoexBuildBuffer(volatile uint32_t *registers);		/* 0x4c8bc */
uint32_t fmCoexComputeBuffer(void);				/* 0x4c8ec */
int fmCoexQueryBuffer(void);					/* 0x4c906 */
uint32_t fmCoexSyncBuffer(void);				/* 0x4c93c */
void fmCoexApplyBuffer(int enable, uint32_t value);		/* 0x4c942 */
uint32_t fmCoexResolveBuffer(void);				/* 0x4c96e */
int fmCoexScheduleBuffer(struct WlanLocalState *state);		/* 0x4c9d4 */

/* wlan/txsubmit.c */
uint32_t wlanMacEntrySelectGet(void);				/* 0x31a32 */

/* The 14 channels' bytes of MAC register 0x8000a100 (one per word), copied by btSbcManageBuffer(). */
#define WLAN_CHANNELS		14
#define MAC_CHANNEL_REGISTER(n)	REG8(0x8000a100 + 4 * (n))
/* The station's address the MAC holds (0 or all ones: none). */
#define MAC_ADDRESS_LOW		REG32(0x8000a528)
#define MAC_ADDRESS_HIGH	REG16(0x8000a52c)
/* The address taken when the MAC holds none: 00:50:43:02:fe:01, Marvell's OUI. */
#define WLAN_DEFAULT_ADDRESS_LOW	0x02435000u
#define WLAN_DEFAULT_ADDRESS_HIGH	0x01fe

extern uint8_t wlanChannelBytes[WLAN_CHANNELS];			/* 0x55741 */

void btSbcManageBuffer(struct WlanLocalState *state);		/* 0x516c8 */
int btSbcAcquireBuffer(struct WlanLocalState *state);		/* 0x516ec */

#endif /* ROBIN_MACDEFAULTS_H */
