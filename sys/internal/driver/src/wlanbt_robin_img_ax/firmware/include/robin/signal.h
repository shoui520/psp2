/*
 * The signal of a connection (0x1760c..0x17bd8, 0x53c74..0x53cd0): the noise
 * floor, RSSI and SNR of its beacons, its data frames and the frames of the
 * join, as last values and running averages, and the link check that counts
 * the beacons missed while nothing is heard. The names are inferred from use;
 * see wlan/signal.c.
 */
#ifndef ROBIN_SIGNAL_H
#define ROBIN_SIGNAL_H

#include <stdint.h>
#include "robin/conn.h"
#include "robin/wlanrx.h"

extern uint32_t wlanLinkStaleTimers;

int wlanSignalFilterReady(const struct WlanConnection *connection, int filter);
void wlanSignalFilterAdd(struct WlanConnection *connection, int filter, int sample);
uint16_t wlanSignalAverageShift(unsigned int frames);
void wlanSignalAverageAdd(struct WlanSignalAverage *average, int sample, unsigned int shift);
int wlanSignalAverageGet(const struct WlanSignalAverage *average, unsigned int shift);
int wlanSignalBeacon(struct WlanConnection *connection, const struct WlanRxStatus *status, int *snr,
		     int *noise, int *rssi);
void wlanSignalData(struct WlanConnection *connection, const struct WlanRxStatus *status);
void wlanSignalJoinFrame(struct WlanConnection *connection, const struct WlanRxStatus *status);
int wlanNoiseFloor(struct WlanConnection *connection);
int wlanBeaconRssi(struct WlanConnection *connection);
int wlanDataRssi(struct WlanConnection *connection);
int wlanDataSnr(struct WlanConnection *connection);
int wlanBeaconSnrCck(struct WlanConnection *connection);
int wlanBeaconSnrOfdm(struct WlanConnection *connection);
int wlanBeaconOfdmSeen(const struct WlanConnection *connection);
int wlanBeaconSnr(struct WlanConnection *connection);
void wlanSignalJoinReset(struct WlanConnection *connection);
void wlanSignalBeaconReset(struct WlanConnection *connection);
void wlanSignalDataReset(struct WlanConnection *connection);
void wlanSignalFilterReset(struct WlanConnection *connection);
void wlanSignalReset(struct WlanConnection *connection);
void wlanSignalInit(struct WlanConnection *connection);
int wlanBeaconCckSeen(const struct WlanConnection *connection);
void wlanSignalJoinDone(struct WlanConnection *connection);
void wlanLinkTxDone(struct WlanConnection *connection, int acknowledged);
void wlanLinkFrameReceived(struct WlanConnection *connection, int rssi, int snr);
void wlanLinkTimerExpired(uint32_t timer, uint32_t argument);
void wlanLinkCheck(struct WlanConnection *connection);
void wlanLinkMonitorStart(struct WlanConnection *connection);
void wlanSignalDataAverage(struct WlanConnection *connection);
void wlanSignalBeaconClear(struct WlanConnection *connection);
int wlanSignalMeasure(struct WlanConnection *connection, const struct WlanRxStatus *status, int *snr,
		      int *noise, int *rssi);
int wlanSignalFilterGet(const struct WlanConnection *connection, int filter);

#endif /* ROBIN_SIGNAL_H */
