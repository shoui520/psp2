/*
 * Power and clock set-up of the SoC and of the radio (sys/socpower.c). The
 * names are inferred from use.
 */
#ifndef ROBIN_SOCPOWER_H
#define ROBIN_SOCPOWER_H

#include <stdint.h>

int sysDelayUs(uint32_t microseconds);
unsigned int clockSelectsClear(void);
void periphControlInit(void);
void periphLatchSet(void);
int macResetAssert(void);
int macResetRelease(void);
uint8_t *macRadioPowerCycle(void);
void rfBusSettle(void);
void socHookNop(void);
uint8_t *rfRestart(void);
unsigned int socClockSetDivider(char divider);
unsigned int socClockSetMode(void);
unsigned int socClockPrepare(int mode, char divider);
int socClockConfigure(void);
void rfRegWriteLow7(char value);
void socClockInit(void);
void sysChipIdRead(void);
int socSensorCalibration(void);
int socSensorRead(void);
int socSensorSample(void);
unsigned int socClockFilter(void);
unsigned int socPowerPreset(void);
void socEnableBlockA(void);
int uapPrepareEntry(uint8_t entry);
void rfCalibrateTrim(void);
void rfRegSequenceA(void);
void rfRegSequenceB(void);
void rfRegSequenceC(void);
void rfRegSequenceInit(void);
int socStrapIsSet(void);
void uapSelectEntry(void);
uint32_t uapActiveEntryGet(void);
void sysDelayHookInit(void);
unsigned int sysPendingEventPolicyIsEnabled(void);
int wlanPsSetupEntry(void);

int rfEventTransitionActive(void);
int rfBlock0Set(int on);
int rfBlock1Set(int on);
int rfBlock2Set(int on);
int rfBlock3Set(int on);
int rfBlock4Set(int on);
int rfBlock4Get(void);
int rfEventTransitionSet(int on);
int rfBlock0Get(void);
int rfBlock1Get(void);
int rfBlock2Get(void);
int rfBlock3Get(void);
extern int rfPowerBlock[5];			/* 0x54dc8 */

extern int (*sysDelayHook)(uint32_t microseconds);	/* 0x54e0c */
extern uint16_t sysChipIdHigh;				/* 0x040089d4 */
extern uint16_t sysChipIdLow;				/* 0x040089d6 */
extern uint32_t sysMacVersion;				/* 0x040089dc */

/* Not converted yet; provisional names. Addresses are those of the 4.00 image. */
void macTxNotifyResetPacket(void);			/* 0x32940 */
int wlanPsSetupHeader(void);				/* 0x3e29c */
uint32_t uapSqramUpdateEntry(void);			/* SQRAM 0xc0008e2e, via the veneer at 0x53074 */
uint32_t uapSqramProcessEntry(void);			/* SQRAM 0xc0008d7e, via the veneer at 0x5307c */
int p2pSqramPrepareEntry(uint8_t entry, int mode);	/* SQRAM 0xc000e290, via the veneer at 0x5306c */

#endif /* ROBIN_SOCPOWER_H */
