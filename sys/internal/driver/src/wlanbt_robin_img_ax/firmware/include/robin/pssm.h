/*
 * The power-save state machine of a connection and the sleep it decides on
 * (0x13410..0x14306): struct WlanPsMachine and struct WlanPsSleep in
 * robin/conn.h. The names are inferred from use; see wlan/pssm.c.
 */
#ifndef ROBIN_PSSM_H
#define ROBIN_PSSM_H

#include <stdint.h>
#include "robin/conn.h"

/* Events posted to the machine (mdevRomQueryState); numbers only, few are understood. */
#define WLAN_PS_EVENT_SLEEP_CONFIRMED	9	/* the host confirmed the sleep */
#define WLAN_PS_EVENT_TRAFFIC		27	/* frames to send: do not sleep */
#define WLAN_PS_EVENT_IEEE_ENTER	41	/* 802.11 power save on: PM=1 null frames */
#define WLAN_PS_EVENT_IEEE_EXIT		42

/* wlanPostConnectionEvent() bits: the host is told of the sleep and the wake-up. */
#define WLAN_EVENT_HOST_PS_PEER	0x0200
#define WLAN_EVENT_HOST_PS_AWAKE		0x1000
#define WLAN_EVENT_HOST_PS_SLEEP		0x2000

void wlanPsMachineReportAwake(struct WlanPsMachine *machine);
const int32_t *wlanPsMachineRoot(struct WlanPsMachine *machine, const int32_t *event);
const int32_t *wlanPsMachineOff(struct WlanPsMachine *machine, const int32_t *event);
void wlanPsSleepTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsStartSleepTimer(struct WlanConnection *connection);
void wlanPsActivityTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsActivity(struct WlanConnection *connection);
void wlanPsMachineStopTimers(struct WlanPsMachine *machine);
const int32_t *wlanPsMachineOn(struct WlanPsMachine *machine, const int32_t *event);
void wlanPsAnnounceSleep(struct WlanConnection *connection);
int wlanPsDtimTooClose(struct WlanPsSleep *sleep, uint64_t now);
int wlanPsMachineMaySleep(struct WlanPsMachine *machine, uint64_t now);
const int32_t *wlanPsMachineConfirm(struct WlanPsMachine *machine, const int32_t *event);
const int32_t *wlanPsMachineSleep(struct WlanPsMachine *machine, const int32_t *event);
const int32_t *wlanPsMachineAwake(struct WlanPsMachine *machine, const int32_t *event);
const int32_t *wlanPsMachineIeee(struct WlanPsMachine *machine, const int32_t *event);
const int32_t *wlanPsMachineIeeeNull(struct WlanPsMachine *machine, const int32_t *event);
const int32_t *wlanPsMachineIeeeExit(struct WlanPsMachine *machine, const int32_t *event);
UINT wlanPsMachineCreate(struct WlanConnectionInfo *info);
int wlanPsMachineReset(struct WlanConnectionInfo *info);
int wlanPsRxFilter(const uint8_t *frame, uint8_t *forward);
void wlanPsMachineInitStates(struct WlanPsMachine *machine);

#endif /* ROBIN_PSSM_H */
