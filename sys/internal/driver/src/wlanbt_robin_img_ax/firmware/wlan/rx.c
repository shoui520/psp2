/*
 * The receive path (0xabc..0x1912): the frames the MAC queued are taken one
 * by one, matched to their connection, filtered, decrypted and handed to the
 * host. The filename and the function names are inferred; no source filename
 * or symbol survives.
 */
#include "robin/wlanrx.h"
#include "robin/apps.h"
#include "robin/conn.h"
#include "robin/coex.h"
#include "robin/defrag.h"
#include "robin/dmaring.h"
#include "robin/event.h"
#include "robin/ibss.h"
#include "robin/icverr.h"
#include "robin/ie.h"
#include "robin/ieee80211.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/psctl.h"
#include "robin/pspoll.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/signal.h"
#include "robin/slot.h"
#include "robin/stapssm.h"
#include "robin/stream.h"
#include "robin/subscribe.h"
#include "robin/swcrypto.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/txring.h"
#include "robin/txstats.h"
#include "robin/wep.h"

/* The frame header the MAC status points at (unaligned pointer, written back after a defragmentation). */
static inline WlanWirelessFrame *rxFrame(const struct WlanRxStatus *status)
{
	return (WlanWirelessFrame *)__aeabi_uread4(status->frame);
}

static inline WlanWirelessFrame *rxSetFrame(struct WlanRxStatus *status, OsaBuffer *buffer)
{
	return (WlanWirelessFrame *)__aeabi_uwrite4((int)((uint8_t *)buffer + buffer->dataOffset), status->frame);
}

static inline uint16_t rxBodyLength(const WlanWirelessFrame *frame)
{
	return frame->bodyLength[1] << 8 | frame->bodyLength[0];
}

static inline void rxSubBodyLength(WlanWirelessFrame *frame, unsigned int length)
{
	unsigned int value = rxBodyLength(frame) - length;

	frame->bodyLength[0] = value;
	frame->bodyLength[1] = value >> 8;
}

/*
 * Reads a beacon's signal quality from the status and keeps the link
 * monitor's averages: the connection's last beacon noise, RSSI and SNR.
 */
void wlanBeaconSignal(struct WlanConnection *connection, const struct WlanRxStatus *status)
{
	int snr;
	int noise;
	int rssi;

	wlanSignalBeacon(connection, status, &snr, &noise, &rssi);
	wlanLinkFrameReceived(connection, snr, rssi);
	connection->noise = noise;
	connection->beaconRssi = rssi;
	connection->snr = snr;
	wlanSubscribeCheckBeacon(connection);
}

/*
 * Antenna diversity: while the event policy is in the antenna-switching mode
 * a strong frame (RSSI at or above wlanAntennaRssiHigh) moves the
 * receive to antenna 1, and the frames below wlanAntennaRssiLow move it back
 * to antenna 0 after five seconds without a strong one.
 */
void wlanUpdateAntennaDiversity(const struct WlanRxStatus *status)
{
	struct MacEventPathConfiguration configuration;
	struct MacEventPolicy policy;
	unsigned int antenna = 0;
	unsigned int pathStates = 0;
	int hold = 0;
	int rssi;

	if (!macGetEventPolicy(&policy))
		return;
	if (!((policy.flags & 0x10) || (policy.flags & 0x20)) || (policy.mode & 0xf) != 1)
		return;
	macGetEventPathConfiguration(sysSelectedEventPath, &configuration);
	if (sysSelectedEventPath == 1)
		return;
	rssi = (int8_t)status->rssi;
	if (sysSelectedEventPath == 0)
		pathStates = policy.pathStates & 0xf;
	if (rssi >= wlanAntennaRssiHigh) {
		antenna = 1;
		wlanAntennaStrongTime = romGetMicroseconds();
	} else if (rssi > wlanAntennaRssiLow) {
		hold = 1;
		if (wlanAntenna == 0)
			return;
		antenna = 1;
		wlanAntennaStrongTime = romGetMicroseconds();
	} else if ((unsigned int)wlanTimeDelta(romGetMicroseconds(), wlanAntennaStrongTime) <= 5000000) {
		if (wlanAntenna == 0)
			return;
		antenna = 1;
	}
	if (wlanAntenna != antenna && !hold) {
		wlanAntenna = antenna;
		sysConfigureEventPath(&configuration, 1, antenna, pathStates, policy.mode & 0xf);
	}
}

/*
 * What a received frame means for the power-save state machines: a frame of
 * a legacy access category with the More Data bit sends a PS-Poll for the
 * next one, an individually addressed frame answers the transmit side, and
 * a TIM or poll answer is flagged to the host.
 */
static inline __attribute__((always_inline)) void wlanRxPowerSaveAccount(struct WlanConnection *connection, const struct WlanRxStatus *status,
				   const WlanWirelessFrame *header, const uint8_t *destination,
				   unsigned int priority, int moreData, int result)
{
	unsigned int eosp = (status->qosControl[0] >> 4) & 1;

	if (wlanPsAcLegacy(priority)) {
		if (moreData)
			macTxSendPsPoll(connection, 0);
		else
			wlanPsPeriodHold = 0;
	}
	if (!(destination[0] & 1)) {
		if (wlanCoexActive())
			result = wlanCoexTxCompleted(connection, moreData, result, eosp);
		else
			result = wlanPsPollHostQueued(connection, moreData, result, eosp);
	}
	wlanPsPollFrameReceived(connection, moreData, header->frameControl[0] >> 4);
	if (result == 1) {
		if (wlanSessionId != 0 && mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0)
			wlanPsPollFlag = 1;
		else
			wlanRaiseEvent(21);
	}
}

/*
 * Gives a frame to the host: the SDIO receive packet is built in front of
 * the frame, the access point relays or drops frames between its stations
 * (wlanConnectionInfo.pktFwdCtl), and the host's callback takes the buffer.
 * Returns 1 when the host has the frame, 0 when it was dropped or only the
 * power-save bookkeeping was wanted.
 */
int wlanRxIndicate(OsaBuffer *buffer, struct WlanRxStatus *status)
{
	struct WlanConnection *connection;
	struct WlanHostRxPacket *packet;
	struct WlanConnectionInfo *info = NULL;
	struct WlanConnection *peer = NULL;
	WlanWirelessFrame header;
	const uint8_t *destination;
	const uint8_t *source;
	unsigned int session = WLAN_RX_BA_MAX;
	unsigned int posture;
	unsigned int ampdu;
	int moreData = 0;
	int result = 0;
	int qos;
	int group;
	int relay;
	int drop;

	if (status == NULL)
		return 0;
	connection = buffer->context;
	__aeabi_memcpy(&header, (uint8_t *)buffer + buffer->dataOffset, 32);
	source = header.address3;
	destination = (header.frameControl[1] & IEEE80211_FC1_TODS) ? header.address3 : header.address1;
	packet = (struct WlanHostRxPacket *)((uint8_t *)buffer + 32);
	ampdu = (__aeabi_uread4(&status->flags) >> 3) & 1;
	qos = ieee80211IsQosData(header.frameControl[0]);
	if (qos || ampdu) {
		posture = sysIrqDisable();
		session = wlanRxBaFind(header.address2, status->qosControl[0] & 7, wlanRxBaSessions);
		if (ampdu && session < WLAN_RX_BA_MAX)
			wlanRxBaSessions[session].frameCount++;
		sysIrqRestore(posture);
	}
	wlanUpdateAntennaDiversity(status);
	packet->descriptor.header.wire.packetOffset[0] = buffer->dataOffset - 32;
	packet->descriptor.header.wire.packetOffset[1] = (buffer->dataOffset - 32) >> 8;
	buffer->dataOffset = 32;
	packet->descriptor.sequence = (header.sequenceControl[1] << 8 | header.sequenceControl[0]) >> 4;
	packet->descriptor.header.wire.bssType = connection->bssType;
	packet->descriptor.header.wire.bssNumber = connection->bssNumber;
	if (buffer->length != 0 || buffer->packetClass == 32) {
		if (header.frameControl[1] & IEEE80211_FC1_MORE_DATA)
			moreData = 1;
		if (ieee80211IsData(header.frameControl[0])) {
			if (qos && wlanPsConfirmArmed != 0 && wlanCoexUseP2pSleepMode() &&
			    (status->qosControl[0] & 0x10) != 0 && wlanPsIeeeFlag != 0 &&
			    wlanPsDataPending != 0 && wlanStaPsTxIdle() &&
			    wlanDmaRingsHaveRoom() != 1 && wlanPsHostBusy == 0)
				result = 1;
			if (wlanApPsEnabled() && !wlanIsNullData(&header))
				wlanPsActivity(wlanConnectionByRole(2));
		}
	}
	wlanBeaconSignal(connection, status);
	packet->descriptor.snr = wlanClampSnr(connection->snr);
	packet->descriptor.noiseFloor = wlanClampNoise(connection->noise);
	/* The raw QoS bit: a management frame with it set reads the status' TID too. */
	packet->descriptor.priority = (header.frameControl[0] & IEEE80211_FC0_QOS) ? status->qosControl[0] & 7 : 0xff;
	if (buffer->length == 0 || wlanIsNullData(&header)) {
		packet->descriptor.header.wire.packetLength[0] = 0;
		packet->descriptor.header.wire.packetLength[1] = 0;
		if (buffer->length != 0 || buffer->packetClass == 32)
			wlanRxPowerSaveAccount(connection, status, &header, destination,
					       packet->descriptor.priority, moreData, result);
		posture = sysIrqDisable();
		if (wlanIsNullData(&header) && wlanHostCallback != NULL) {
			__aeabi_memcpy((uint8_t *)packet + 24, source, 6);
			packet->descriptor.header.wire.packetType[0] = 0xeb;
			packet->descriptor.header.wire.packetType[1] = 0;
			wlanHostCallback(wlanDevice, buffer, 0, 5);
		} else {
			wlanReleasePacket(buffer);
		}
		goto restoreAndReturn;
	}
	if (status->qosControl[0] & 0x80) {
		if (!wlanPrepareAmsduFrame(buffer, &packet->descriptor.header)) {
			packet->descriptor.header.wire.packetLength[0] = 0;
			packet->descriptor.header.wire.packetLength[1] = 0;
			wlanReleasePacket(buffer);
			return 0;
		}
	} else if (!wlanConvertDataFrame(buffer, &packet->descriptor.header, (uintptr_t)status, &header)) {
		packet->descriptor.header.wire.packetLength[0] = 0;
		packet->descriptor.header.wire.packetLength[1] = 0;
		posture = sysIrqDisable();
		wlanReleasePacket(buffer);
		goto restoreAndReturn;
	}
	packet->descriptor.header.packetLength = buffer->length;
	wlanRxDecodeRate(status, &packet->descriptor.htInfo, &packet->descriptor.rate);
	if (connection->mode == 2) {
		uint8_t forwarding;

		info = wlanConnectionInfo(connection);
		forwarding = info->pktFwdCtl;
		group = source[0] & 1;
		relay = 0;
		drop = 0;
		if (group) {
			if ((forwarding & WLAN_PKT_FWD_RELAY) && !(forwarding & WLAN_PKT_FWD_NO_GROUP_RELAY))
				relay = 1;
		} else {
			peer = wlanConnectionFindByPeer(source);
			if (peer != NULL && (peer->flags2 & WLAN_CONN2_ASSOCIATED)) {
				if (connection->bssNumber == peer->bssNumber) {
					if (session >= WLAN_RX_BA_MAX) {
						if (forwarding & WLAN_PKT_FWD_DROP_INTRA_BSS) {
							drop = 1;
						} else if (forwarding & WLAN_PKT_FWD_RELAY) {
							relay = 1;
							drop = 1;
						}
					}
				} else if ((forwarding & WLAN_PKT_FWD_DROP_INTER_BSS) && session >= WLAN_RX_BA_MAX) {
					drop = 1;
				}
			}
		}
		if (relay) {
			/* Another station of the BSS gets its own copy as a transmit event. */
			OsaBuffer *copy = wlanTxBufferAlloc(&wlanBlockPool28, wlanMgmtFrameSize, 0, 3);

			if (copy != NULL) {
				WlanTxDescriptor *descriptor;

				copy->length = buffer->length;
				copy->dataOffset = (uint16_t)wlanPacketHeadroom();
				copy->packetClass = 8;
				descriptor = (WlanTxDescriptor *)((uint8_t *)copy + copy->dataOffset);
				__aeabi_memclr(descriptor, 15);
				descriptor->header.wire.packetOffset[0] = 16;
				descriptor->header.wire.packetOffset[1] = 0;
				descriptor->header.wire.packetType[0] = packet->descriptor.header.wire.packetType[0];
				descriptor->header.wire.packetType[1] = packet->descriptor.header.wire.packetType[1];
				if (info->qosEnabled == 1 && peer != NULL && (connection->flags2 & WLAN_CONN2_QOS))
					descriptor->priority = packet->descriptor.priority;
				__aeabi_memcpy((uint8_t *)descriptor + 16,
					       (uint8_t *)packet + (int16_t)(packet->descriptor.header.wire.packetOffset[1] << 8 |
									      packet->descriptor.header.wire.packetOffset[0]),
					       buffer->length);
				copy->context = connection->device;
				descriptor->header.wire.bssNumber = connection->bssNumber;
				descriptor->header.wire.bssType = connection->bssType;
				descriptor->header.packetLength = buffer->length;
				wlanForwardEventPacket(copy);
			}
		}
		if (drop) {
			packet->descriptor.header.wire.packetLength[0] = 0;
			packet->descriptor.header.wire.packetLength[1] = 0;
			wlanReleasePacket(buffer);
			return 0;
		}
	}
	if (wlanApPsEnabled())
		wlanRaiseEvent(5);
	posture = sysIrqDisable();
	wlanHostCallback(wlanDevice, buffer, 0, 5);
	sysIrqRestore(posture);
	wlanRxPowerSaveAccount(connection, status, &header, destination, packet->descriptor.priority,
			       moreData, result);
	return 1;

restoreAndReturn:
	sysIrqRestore(posture);
	return 0;
}

/*
 * Removes the WEP trailer of a frame and decrypts it with the hardware
 * (wlanWepDecrypt): the 4-byte IV and key index follow the header at
 * ivOffset, the 4-byte ICV ends the body. Returns the decrypted length (1 for
 * a reassembled MSDU), or 0 when the frame is dropped.
 */
int wlanRxDecryptWep(OsaBuffer **buffer, struct WlanRxStatus *status, cipher_key_t *key, int ivOffset)
{
	struct WlanConnection *connection = (*buffer)->context;
	WlanWirelessFrame *frame;
	const uint8_t *iv;
	uint8_t seed[16];
	uint8_t isWep104 = 0;
	uint8_t keyIndex;
	int result;

	if (key == NULL)
		return 0;
	frame = rxFrame(status);
	iv = (const uint8_t *)frame + ivOffset + 32;
	if (!(frame->frameControl[1] & IEEE80211_FC1_PROTECTED))
		return (*buffer)->length;
	if (rxBodyLength(frame) <= 0x68)
		return 0;
	wlanWepBuildSeed(iv, (const WlanWepKeyTable *)key, seed, &isWep104, &keyIndex);
	if (wlanIsFragment(frame)) {
		OsaBuffer *assembled = wlanDefragmentWep(*buffer, seed, isWep104, keyIndex,
							 &wlanDefragReassemblyWep, &wlanDefragContext,
							 &wlanIbssIcvErrList, &wlanTxStatistics);

		if (assembled == NULL) {
			*buffer = NULL;
			return 0;
		}
		*buffer = assembled;
		frame = rxSetFrame(status, assembled);
		assembled->length = rxBodyLength(frame);
		return 1;
	}
	result = wlanWepDecrypt((uint8_t *)iv, iv + 4, rxBodyLength(frame) - 100, seed, isWep104, frame);
	if (result) {
		wlanWepIcvErrForget(frame, &wlanIbssIcvErrList);
		rxSubBodyLength(frame, 8);
		(*buffer)->length -= 8;
	} else {
		uint8_t *errors = (uint8_t *)&wlanTxStatistics.wep_icv_err_cnt[keyIndex];

		__aeabi_uwrite4(__aeabi_uread4(errors) + 1, errors);
		wlanWepIcvErrRecord(connection, rxFrame(status), keyIndex, isWep104,
				    ((const WlanWepKeyTable *)key)->keys[keyIndex].key, &wlanIbssIcvErrList);
	}
	return result;
}

/*
 * Checks the replay counter of a TKIP frame, has the crypto unit decrypt it
 * and strips the 20 bytes of IV, MIC and ICV. A MIC failure is reported to
 * the key management (the countermeasures). Returns 1 when the frame is good.
 */
int wlanRxDecryptTkip(OsaBuffer **buffer, struct WlanRxStatus *status, cipher_key_t *key)
{
	OsaBuffer *rxBuffer = *buffer;
	struct WlanConnection *connection = rxBuffer->context;
	WlanWirelessFrame *frame = rxFrame(status);
	uint8_t *iv = (uint8_t *)frame + 36;
	uint32_t mic[2];
	uint32_t high;		/* Sony's: only set when the replay check ran */
	uint16_t low;
	int tkip = 0;
	unsigned int errors;

	if (wlanConnectionIsReady(connection) && wlanRxReplayCheck(iv, key, &low, &high))
		return 0;
	if (rxBodyLength(frame) <= 0x68)
		return 0;
	errors = wlanCcmpDecrypt(frame, iv, (uint16_t)(rxBodyLength(frame) - 104), status->qosControl[0] & 7,
				 (const uint32_t *)key->ckd.aes.rxMicKey, key->ckd.aes.key,
				 &key->ckd.aes.decryptState[0], &key->ckd.aes.decryptState[1],
				 &key->ckd.aes.decryptState[2], &key->ckd.aes.decryptState[3],
				 &key->ckd.aes.decryptState[4]);
	if (wlanIsFragment(frame)) {
		OsaBuffer *assembled;

		tkip = 0;
		if (!wlanRxTkipTrailer(errors, (uint8_t *)frame, iv, rxBuffer, &tkip, mic))
			return 0;
		assembled = wlanDefragment(rxBuffer, frame, &wlanDefragReassembly, &wlanDefragContext,
					   &wlanDefragDiscarding);
		if (assembled == NULL) {
			*buffer = NULL;
			return 0;
		}
		*buffer = assembled;
		frame = rxSetFrame(status, assembled);
		if (tkip && !wlanRxTkipMicCheck((uint8_t *)frame, iv, assembled, key, mic)) {
			wlanMicErrorReport(connection, frame->address1[0] & 1);
			return 0;
		}
	} else {
		if (errors != 0) {
			if (((errors >> 2) & 3) != 0 || !(errors & 2))
				return 0;
			wlanMicErrorReport(connection, frame->address1[0] & 1);
			return 0;
		}
		rxSubBodyLength(frame, 20);
		rxBuffer->length -= 20;
	}
	wlanRxKeySetReplayCounter(key, low, high);
	return 1;
}

/*
 * Decrypts a frame in software with its key (TKIP or WEP; the crypto unit
 * does AES and WAPI) and tells the key management when it was a good frame
 * of an installed receive key. A frame that is not protected passes unless
 * the connection refuses unprotected frames (bit 2 of its transmit options;
 * the name of the bit is not known). Returns the length or 1 for success,
 * 0 for a frame to drop.
 */
int wlanRxDecrypt(OsaBuffer **buffer, struct WlanRxStatus *status, cipher_key_t *key)
{
	OsaBuffer *own = *buffer;
	OsaBuffer *received = own;
	const WlanWirelessFrame *frame = rxFrame(status);
	struct WlanConnection *connection = received->context;
	int result = 0;

	if (!(frame->frameControl[1] & IEEE80211_FC1_PROTECTED)) {
		if (!(connection->txOptions & 4))
			result = 1;
	} else if (key == NULL) {
		return 0;
	} else if (key->hdr.keyState & KEY_STATE_ENABLE) {
		if (key->hdr.keyType == KEY_TYPE_TKIP)
			result = wlanRxDecryptTkip(&own, status, key);
		else
			result = wlanRxDecryptWep(&own, status, key, 4);
		*buffer = own;
		if (!(frame->address1[0] & 1) && result && (key->hdr.keyDirection & KEY_DIRECTION_RX))
			keyMgmtSuccessfulDecryptNotify(key);
	}
	return result;
}

/*
 * Host sleep: a frame that matches the host's wake filter wakes the host,
 * one that does not is dropped (kept only for the security checks), and
 * the stations' own power save is told about a frame received while it
 * sleeps.
 */
void wlanRxHostSleepFilter(OsaBuffer *buffer, struct WlanRxStatus *status)
{
	WlanWirelessFrame *header;
	const uint8_t *destination;
	OsaBuffer *event = NULL;
	uint32_t wake;			/* Sony's: set by wlanMefProcessFrame() */
	uint8_t securityDrop = 0;
	int wakeHost = 0;
	int dropped = 0;

	if (buffer->length != 0) {
		if (wlanHostSleepArmed()) {
			if (!wlanIsNullData(rxFrame(status))) {
				if (wlanMefProcessFrame(buffer, &wake, 1)) {
					if (wake != 0) {
						event = wlanTxAllocFrameType2(79, NULL);
						wakeHost = 1;
					}
				} else if (wlanMefAcceptFrame(buffer, &securityDrop)) {
					wakeHost = 1;
				}
			}
			if (wakeHost) {
				wlanMefConfig.hostEvents = 1;
				wlanPsCancelWakeTimer();
				if (wlanPsGetOverride() == 0 && !wlanPsSleepStateReached()) {
					wlanPsSleepTimeout = 0;
					wlanPsSleepSequence = 0;
					wlanMefConfig.sleepHeld = 1;
					wlanRaiseEvent(3);
				}
				wlanHostEventNotify(WLAN_WAKE_ALL, event);
				if (wlanHostSleepEndOnWake != 0 && wlanHostSleepSettings.gpio == -1) {
					wlanHostSleepCancel();
					wlanHostSleepEndOnWake = 0;
					wlanMefConfig.wakeSignalled = 0;
					wlanMefConfig.hostState = WLAN_HOST_AWAKE;
				} else {
					int tries = 2;

					do {
						osaTaskSleep(1);
					} while (--tries != 0 && wlanMefConfig.hostState != WLAN_HOST_AWAKE);
				}
				wlanRaiseEvent(28);
				wlanMefConfig.queuedEvents = 0;
			} else if (securityDrop == 0) {
				buffer->length = 0;
				buffer->packetClass = 32;
				dropped = 1;
			}
		}
		if (!dropped && buffer->length != 0 && !wlanHostSleepArmed())
			wlanMefProcessFrame(buffer, &wake, 2);
	}
	if (wlanPsEnabled() && buffer->length != 0 && !wlanCoexUseP2pMode() && !wlanCoexUseStationMode()) {
		header = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
		destination = (header->frameControl[1] & IEEE80211_FC1_TODS) ? header->address3 : header->address1;
		if (wlanPsWakeOnDtim != 0 && wlanPsWakeClasses == 9 && (destination[0] & 1)) {
			if (header->frameControl[1] & IEEE80211_FC1_MORE_DATA)
				wlanPsRearmWakeTimer();
			else
				wlanRaiseEvent(26);
		} else if (macTxGetPowerSave() != 1) {
			wlanPsArmWakeTimer();
		}
	}
}

/*
 * The receive task's loop: takes every frame the MAC queued, finds its
 * connection, applies the connection's frame filter and the access point's
 * power-save tracking, decrypts the frame and indicates it to the host.
 */
int wlanRxProcessFrames(void)
{
	cipher_key_t *key = NULL;	/* kept from the frame before when the connection has none */
	struct WlanRxStatus status;
	OsaBuffer *buffer;

	if (wlanIbssPsEnabled() && wlanHostSleepArmed())
		wlanMefConfig.queuedEvents = 1;
	while ((buffer = wlanRxTakePacket(0, 1, &status)) != NULL) {
		struct WlanConnectionRole *role = NULL;
		struct WlanConnection *connection;
		WlanWirelessFrame *frame;
		OsaBuffer *indicated;
		uint8_t *header;
		const uint8_t *destination;
		struct WlanKeyBuffer **holder;
		int ampdu = (status.flags >> 3) & 1;

		if (wlanConnectionHasPrimary()) {
			buffer->context = wlanConnectionGetPrimary();
			wlanDeliverRxFrame(buffer, &status);
			continue;
		}
		wlanSlotRunningResult((uint32_t)((uint8_t *)buffer + buffer->dataOffset), (uint32_t)&status);
		frame = rxFrame(&status);
		if (frame == NULL) {
			buffer->length = 0;
			wlanReleasePacket(buffer);
			continue;
		}
		connection = wlanConnectionFindByPeer(frame->address2);
		if (connection == NULL) {
			connection = wlanConnectionFindByLocal(1, frame->address3);
			if (connection == NULL || !(connection->stationFlags & 0x80)) {
				connection = wlanConnectionFindByLocal(1, frame->address3);
				if (connection != NULL && connection->state == 6) {
					connection = wlanConnectionFindDirectPeer(connection, frame->address2);
					if (connection != NULL)
						wlanConnectionTouch(connection);
				}
			}
		}
		if (wlanRxCheckFrameClass(connection, frame) || connection == NULL) {
			buffer->length = 0;
			wlanReleasePacket(buffer);
			continue;
		}
		buffer->context = connection;
		if (connection->mode == 2) {
			role = wlanConnectionInfoSlot(connection);
			wlanRxFilterToDs((connection->flags2 >> 4) & 1, buffer, frame);
		} else if (connection->interfaceType == 1) {
			wlanRxFilterIbss(1, buffer, frame, connection->localAddress);
			wlanIbssResetPeerCheck(connection);
		} else if (connection->interfaceType == 4) {
			if (!(frame->frameControl[1] & IEEE80211_FC1_FROMDS) ||
			    !(frame->frameControl[1] & IEEE80211_FC1_TODS))
				buffer->length = 0;
		} else {
			wlanRxFilterStation(1, buffer, frame, connection->localAddress);
		}
		header = (uint8_t *)buffer + buffer->dataOffset;
		destination = (header[3] & IEEE80211_FC1_TODS) ? header + 18 : header + 6;
		if (buffer->length != 0 && !ampdu) {
			if (!(destination[0] & 1)) {
				wlanRxCheckFragment(buffer, frame, &connection->lastSequence,
						    &connection->lastFragment, 10);
				if (buffer->length == 0) {
					wlanBeaconSignal(connection, &status);
					wlanReleasePacket(buffer);
					continue;
				}
			} else {
				wlanRxDropLoopback(buffer, (const WlanWirelessFrame *)header,
						   connection->interfaceAddress);
			}
		}
		if (buffer->length != 0) {
			if (connection->mode == 2) {
				unsigned int powerSave = (frame->frameControl[1] >> 4) & 1;

				wlanApPsTrigger(connection, &status);
				if (role->peerPowerSave != powerSave)
					wlanApPsSetState(connection, powerSave);
				wlanConnectionUpdateListenInterval(connection);
				if (role->state == 1)
					buffer->length = 0;
			}
		}
		if (buffer->length == 0)
			goto indicate;
		if (!(destination[0] & 1))
			wlanPsPollAnswered(&status);
		if (wlanRxClearProtectedOnNull(frame)) {
			buffer->length -= 64;
			indicated = wlanDefragment(buffer, (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset),
						   &wlanDefragReassembly, &wlanDefragContext,
						   &wlanDefragDiscarding);
			if (indicated == NULL)
				continue;
			buffer = indicated;
			rxSetFrame(&status, buffer);
			goto indicate;
		}
		holder = (frame->address1[0] & 1) ? &connection->keyBlocks[0] : &connection->keyBlocks[1];
		if (*holder != NULL)
			key = &(*holder)->key;
		if (key == NULL) {
			frame->frameControl[1] &= ~IEEE80211_FC1_PROTECTED;
			buffer->length = 0;
			goto indicate;
		}
		if (key->hdr.keyType == KEY_TYPE_AES || key->hdr.keyType == KEY_TYPE_WAPI) {
			/* The crypto unit works on the frame; the result comes back through the task. */
			wlanRxWaitForBuffer(connection->lastFragment == 0, frame);
			wlanCryptoReceive(buffer, &status, key);
			if (key->hdr.keyType == KEY_TYPE_AES || key->hdr.keyType == KEY_TYPE_WAPI)
				continue;
			goto indicate;
		}
		if (key->hdr.keyType != KEY_TYPE_TKIP && key->hdr.keyType != KEY_TYPE_WEP) {
			for (;;)
				;	/* the original stops here for a key type it does not know */
		}
		while (wlanCryptoCount(0))
			;
		if (!wlanRxDecrypt(&buffer, &status, key)) {
			if (buffer == NULL)
				continue;
			buffer->length = 0;
		}
indicate:
		wlanRxHostSleepFilter(buffer, &status);
		wlanRxIndicate(buffer, &status);
	}
	MAC_510_EVENT_STATUS = ~0x80;
	if (wlanDmaRing3HasRoom())
		macTxPostEvent(4);
	wlanMefConfig.queuedEvents = 0;
	wlanMefConfig.hostEvents = 0;
	return 0;
}
