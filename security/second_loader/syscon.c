/*
 * syscon.c: SceSyscon SPI packet driver, Ernie command execution, NVS and
 * scratchpad reads, power/rail control, and handshake packet transport
 * (0x80363c..0x804beb).
 *
 * Function and structure names follow Sony's SceSyscon driver in
 * sys/internal/driver/src/syscon/ and <syscon/syscon.h>.
 */

#include "second_loader.h"

/* ============================================================================
 * .bss (0x80cfc0..0x80d23f), COMMON (0x80d808..0x80da6f), .sdata/.sbss
 * ============================================================================ */

static SceSysconPacket s_sysconMultiWaitPkt __attribute__((aligned(64)));
static SceSysconPacket s_sysconAckPkt __attribute__((aligned(64)));
static SceSysconPacket s_sysconReadResultPkt __attribute__((aligned(64)));
static SceSysconPacket s_sysconHandshakePkt __attribute__((aligned(64)));
static uint8_t s_sysconHandshakeTxBuf[64] __attribute__((aligned(64)));
static uint8_t s_sysconHandshakeRxBuf[64] __attribute__((aligned(64)));

SysconDriverState g_sysconCtrl;

int g_sysconAsyncCallbackStatus SL_SDATA = (int)0x800f002c;

static uint32_t s_sysconInitialized SL_SBSS;
static int s_sysconPollElapsedMs SL_SBSS;
static int (*s_sysconAsyncCallback)(void) SL_SBSS;

/* ============================================================================
 * .rodata (0x80bf98..0x80c04f)
 * ============================================================================ */

static const SysconNotificationDescriptor s_sysconStandardNotifications[19] = {
	{ 0u, 0x01u, 0u, 0x00u, &g_sysconCtrl.notifFlags[0] },
	{ 0u, 0x02u, 0u, 0x01u, &g_sysconCtrl.notifFlags[1] },
	{ 0u, 0x04u, 0u, 0x02u, &g_sysconCtrl.notifFlags[2] },
	{ 0u, 0x08u, 0u, 0x03u, &g_sysconCtrl.notifFlags[3] },
	{ 0u, 0x10u, 0u, 0x04u, &g_sysconCtrl.notifFlags[4] },
	{ 0u, 0x20u, 0u, 0x05u, &g_sysconCtrl.notifFlags[5] },
	{ 0u, 0x40u, 0u, 0x06u, &g_sysconCtrl.notifFlags[6] },
	{ 1u, 0x01u, 0u, 0x08u, &g_sysconCtrl.notifFlags[7] },
	{ 1u, 0x02u, 0u, 0x09u, &g_sysconCtrl.notifFlags[8] },
	{ 1u, 0x04u, 0u, 0x0au, &g_sysconCtrl.notifFlags[9] },
	{ 1u, 0x80u, 0u, 0x0fu, &g_sysconCtrl.notifFlags[11] },
	{ 2u, 0x01u, 0u, 0x10u, &g_sysconCtrl.notifFlags[12] },
	{ 2u, 0x02u, 0u, 0x11u, &g_sysconCtrl.notifFlags[13] },
	{ 2u, 0x04u, 0u, 0x12u, &g_sysconCtrl.notifFlags[14] },
	{ 2u, 0x08u, 0u, 0x13u, &g_sysconCtrl.notifFlags[15] },
	{ 2u, 0x10u, 0u, 0x14u, &g_sysconCtrl.notifFlags[16] },
	{ 2u, 0x40u, 0u, 0x16u, &g_sysconCtrl.notifFlags[18] },
	{ 2u, 0x80u, 0u, 0x17u, &g_sysconCtrl.notifFlags[19] },
	{ 3u, 0x02u, 0u, 0x19u, &g_sysconCtrl.notifFlags[20] },
};

static const SysconNotificationDescriptor s_sysconExtendedNotifications[4] = {
	{ 1u, 0x40u, 1u, 0x20u, &g_sysconCtrl.notifFlags[25] },
	{ 1u, 0x10u, 1u, 0x21u, &g_sysconCtrl.notifFlags[26] },
	{ 3u, 0x08u, 0u, 0x22u, &g_sysconCtrl.notifFlags[27] },
	{ 3u, 0x02u, 1u, 0x23u, &g_sysconCtrl.notifFlags[28] },
};

/* ============================================================================
 * Functions (0x80363c..0x804beb)
 * ============================================================================ */

/* 0x80363c */
static int sysconDispatchCallback(unsigned int index, int arg)
{
	SysconCallbackEntry *entry = &g_sysconCtrl.callbacks[index];

	if (entry->callback != NULL)
		entry->callback(arg, entry->argp);
	return 0;
}

/* 0x803664 */
static int sysconStartPacketTx(SceSysconPacket *pkt)
{
	volatile SceSpiRegisters *spi = g_sysconCtrl.spiRegs;
	const uint16_t *txWords;
	uint8_t txLen;
	int i;

	if (g_sysconCtrl.debugHooks != NULL)
		g_sysconCtrl.debugHooks->start(pkt);

	if ((pkt->flags & 0x800u) == 0)
		txWords = (const uint16_t *)pkt->tx;
	else
		txWords = (const uint16_t *)pkt->extTxBuf;

	sceGpioPortClear(0, 3);
	while (spi->receiveCount != 0)
		(void)spi->receive;
	(void)spi->transmitCount;
	spi->intStatus = 0x600u;

	if ((pkt->flags & 0x40000u) == 0) {
		txLen = (uint8_t)txWords[1];
		for (i = 0; i <= (int)(txLen + 2u); i += 2) {
			spi->transmit = (uint32_t)(*txWords);
			txWords++;
		}
	} else {
		uint16_t cmdWord = txWords[0];
		uint32_t sum = (cmdWord & 0xffu) + ((cmdWord >> 8) & 0xffu) + 1u;

		spi->transmit = cmdWord;
		spi->transmit = ((~sum & 0xffu) << 8) | 1u;
	}

	spi->control = 0u;
	spi->enable = 1u;
	(void)spi->enable;
	sceGpioPortSet(0, 3);
	pkt->flags |= 0x40020000u;
	return 0;
}

/* 0x80373c */
uint32_t sysconReadPervasiveRevision(void)
{
	return PERVASIVE_MISC->revision;
}

/* 0x803744 */
int isSysconInitialized(void)
{
	return (int)s_sysconInitialized;
}

/* 0x80374a */
static void sysconDelayUsThunk(uint32_t us)
{
	cmepDelayUs(us);
}

/* 0x80374e */
int sceSysconAsyncPollInit(void)
{
	uint32_t dummy;

	slMemset(&g_sysconCtrl, 0, sizeof(g_sysconCtrl));

	slMemset(&s_sysconAckPkt, 0, sizeof(s_sysconAckPkt));
	s_sysconAckPkt.tx[0] = 3u;
	s_sysconAckPkt.tx[2] = 1u;
	s_sysconAckPkt.tx[3] = 0xfbu;
	s_sysconAckPkt.queueIndex = 0x11u;
	s_sysconAckPkt.status = -1;

	slMemset(&s_sysconMultiWaitPkt, 0, sizeof(s_sysconMultiWaitPkt));
	s_sysconMultiWaitPkt.tx[0] = 4u;
	s_sysconMultiWaitPkt.tx[2] = 1u;
	s_sysconMultiWaitPkt.tx[3] = 0xfau;
	s_sysconMultiWaitPkt.queueIndex = 0x11u;
	s_sysconMultiWaitPkt.status = -1;

	slMemset(&s_sysconReadResultPkt, 0, sizeof(s_sysconReadResultPkt));
	s_sysconReadResultPkt.tx[0] = 3u;
	s_sysconReadResultPkt.tx[1] = 1u;
	s_sysconReadResultPkt.tx[2] = 1u;
	s_sysconReadResultPkt.tx[3] = 0xfau;
	s_sysconReadResultPkt.queueIndex = 0x11u;
	s_sysconReadResultPkt.status = -1;

	g_sysconCtrl.cmdDelayUs = 4000u;
	g_sysconCtrl.retryDelayUs = 20000u;
	g_sysconCtrl.allowQueuedFlag1 = 1u;

	sceGpioPortClear(0, 3);
	sceGpioSetPortMode(0, 3, 0);
	sceGpioSetPortMode(0, 4, 1);
	sceGpioSetIntrMode(0, 4, 3);

	g_sysconCtrl.spiRegs = (volatile SceSpiRegisters *)0xe0a00000u;
	sceGpioEnableIntr(0, 4);
	sceGpioAcquireIntr(0, 4);

	dummy = PERVASIVE_GATE->spi[0];
	PERVASIVE_GATE->spi[0] = 1u;
	dummy = PERVASIVE_GATE->spi[0];
	dummy = PERVASIVE_RESET->spi[0];
	PERVASIVE_RESET->spi[0] = 0u;
	dummy = PERVASIVE_RESET->spi[0];
	(void)dummy;

	g_sysconCtrl.spiRegs->mode = 0u;

	while (sceSysconQueryBaryonVersion(&g_sysconCtrl.baryonVersion) < 0)
		;
	while (sceSysconQueryHardwareInfo(&g_sysconCtrl.hardwareInfo) < 0)
		;
	while (sceSysconQueryBuildVersion(g_sysconCtrl.buildVersion) < 0)
		;
	g_sysconCtrl.buildVersion[15] = 0u;

	if (g_sysconCtrl.baryonVersion < 0x01000004u) {
		if (g_sysconCtrl.baryonVersion > 0x00070501u) {
			while (sceSysconSetAlertLevel(2u) < 0)
				;
		}
	} else {
		while (sceSysconSetAlertLevel(0x12u) < 0)
			;
	}

	g_sysconCtrl.notifFlags[5] = 0xffu;
	g_sysconCtrl.notifFlags[6] = 0xffu;
	s_sysconInitialized = 1u;
	return 0;
}

/* 0x8038ec */
uint32_t sceSysconGetBaryonVersion(void)
{
	return g_sysconCtrl.baryonVersion;
}

/* 0x8038f2 */
uint32_t sceSysconGetHardwareInfo(void)
{
	return g_sysconCtrl.hardwareInfo;
}

/* 0x8038f8 */
int sceSysconGetHardwareInfoEx(void *outBuf16)
{
	if ((g_sysconCtrl.hardwareInfo & 0x20u) == 0)
		return (int)0x80250003;
	slMemcpy(outBuf16, g_sysconCtrl.hardwareInfoEx, 16u);
	return 0;
}

/* 0x803922 */
const uint8_t *sceSysconGetBuildVersion(void)
{
	return g_sysconCtrl.buildVersion;
}

/* 0x803928 */
int sceSysconCmdExecAsync(SceSysconPacket *pkt, uint32_t flags,
		int (*callback)(SceSysconPacket *, void *), void *argp)
{
	uint8_t *txBuf;
	uint32_t qIdx;
	SceSysconPacket *head;
	unsigned int i;

	if (g_sysconCtrl.suspended != 0)
		return (int)0x80250102;

	if ((flags & 0x800u) == 0) {
		pkt->extTxBuf = NULL;
		if ((uint8_t)(pkt->tx[2] - 1u) > 0x1cu)
			return (int)0x80250006;
		txBuf = pkt->tx;
	} else {
		txBuf = pkt->extTxBuf;
		if (txBuf == NULL)
			return (int)0x80250002;
		pkt->tx[0] = txBuf[0];
		pkt->tx[1] = txBuf[1];
		pkt->tx[2] = txBuf[2];
		if ((uint8_t)(txBuf[2] - 1u) > 0xfcu)
			return (int)0x80250006;
	}

	if ((flags & 0x400u) == 0) {
		pkt->extRxBuf = NULL;
		pkt->extRxMaxSize = 0u;
	} else {
		if (pkt->extRxBuf == NULL)
			return (int)0x80250002;
		if (pkt->extRxMaxSize < 0x21u)
			return (int)0x80250001;
	}

	if ((flags & 0x100u) == 0) {
		uint32_t len = txBuf[2];
		uint8_t sum = 0;

		for (i = 0; i <= len + 1u; i++)
			sum = (uint8_t)(sum + txBuf[i]);
		txBuf[len + 2u] = (uint8_t)~sum;
		for (i = len + 3u; i < 32u; i++)
			txBuf[i] = 0xffu;
	}

	for (i = 0; i < 32u; i++)
		pkt->rx[i] = 0xffu;

	pkt->extRxReceived = 0u;
	pkt->next = NULL;
	pkt->flags = (flags & 0xffffu) | 0x10000u;
	pkt->status = -1;
	pkt->callback = callback;
	pkt->callbackArg = argp;

	qIdx = txBuf[1];
	if ((int8_t)txBuf[0] < 0) {
		if (qIdx > 15u)
			qIdx = 16u;
	} else {
		qIdx = 17u;
	}
	pkt->queueIndex = qIdx;
	pkt->timeoutUs = (qIdx == 17u) ? 0u : g_sysconCtrl.retryDelayUs;

	head = g_sysconCtrl.queueHead[qIdx];
	if (head == NULL) {
		g_sysconCtrl.queueHead[qIdx] = pkt;
		g_sysconCtrl.queueTail[qIdx] = pkt;
	} else if ((flags & 1u) == 0) {
		g_sysconCtrl.queueTail[qIdx]->next = pkt;
		g_sysconCtrl.queueTail[qIdx] = pkt;
	} else if ((head->flags & 0x40060000u) == 0) {
		pkt->next = head;
		g_sysconCtrl.queueHead[qIdx] = pkt;
	} else {
		pkt->next = head->next;
		head->next = pkt;
		if (pkt->next == NULL)
			g_sysconCtrl.queueTail[qIdx] = pkt;
	}

	cmepDelayUs((pkt->queueIndex == 17u) ? 4u : g_sysconCtrl.cmdDelayUs);
	g_sysconCtrl.activePacket = pkt;
	sysconStartPacketTx(pkt);
	return 0;
}

/* 0x803b10 */
int sceSysconCmdWait(SceSysconPacket *pkt, int noWait)
{
	if (noWait == 0) {
		while ((pkt->flags & 0x80000u) == 0) {
			volatile SceSpiRegisters *spi;
			SceSysconPacket *cur;
			uint8_t *rxBuf;
			uint32_t maxRx;
			uint32_t rxStatus;
			uint32_t readOffset;
			uint32_t i;

			cmepDelayUs((pkt->queueIndex == 17u) ? 4u : g_sysconCtrl.cmdDelayUs);
			do {
				cur = g_sysconCtrl.activePacket;
			} while (sceGpioQueryIntr(0, 4) == 0);

			g_sysconCtrl.activePacket = NULL;
			g_sysconCtrl.inInterrupt = 1u;
			if ((cur->flags & 0x200u) == 0)
				sceGpioPortClear(0, 3);
			sceGpioAcquireIntr(0, 4);

			spi = g_sysconCtrl.spiRegs;
			if ((cur->flags & 0x400u) == 0) {
				rxBuf = cur->rx;
				maxRx = 32u;
			} else {
				rxBuf = cur->extRxBuf + cur->extRxReceived;
				maxRx = cur->extRxMaxSize - cur->extRxReceived;
			}

			if (spi->receiveCount == 0) {
				rxBuf[0] = 0xffu;
				rxBuf[1] = 0xffu;
				rxBuf[2] = 0u;
				rxStatus = 0xffffffffu;
				cur->flags |= 0x100000u;
			} else {
				rxStatus = 0u;
			}

			if (spi->transmitCount != 0) {
				rxStatus = 0xffffffffu;
				cur->flags |= 0x200000u;
			}
			if ((spi->intStatus & 0x200u) != 0) {
				spi->intStatus = 0x200u;
				cur->flags |= 0x400000u;
			}

			readOffset = 0u;
			while (readOffset + 1u < maxRx && spi->receiveCount != 0) {
				uint32_t word = spi->receive;

				if (readOffset == 0u)
					rxStatus = word & 0xffu;
				rxBuf[readOffset] = (uint8_t)word;
				rxBuf[readOffset + 1u] = (uint8_t)(word >> 8);
				readOffset += 2u;
			}
			if ((cur->flags & 0x400u) != 0)
				slMemcpy(cur->rx, rxBuf, 4u);

			spi->enable = 0u;
			(void)spi->enable;
			sceGpioPortClear(0, 3);
			if (g_sysconCtrl.debugHooks != NULL)
				g_sysconCtrl.debugHooks->end(cur);

			if (rxStatus != 0xffffffffu) {
				uint32_t rxLen = rxBuf[2];

				if (rxLen > 1u && rxLen + 3u <= maxRx) {
					uint8_t sum = 0;

					for (i = 0; i < rxLen + 2u; i++)
						sum = (uint8_t)(sum + rxBuf[i]);
					if (rxBuf[rxLen + 2u] != (uint8_t)~sum)
						rxStatus = 0xfffffffeu;
				} else {
					rxStatus = 0xfffffffeu;
				}
			}

			cur->flags &= ~0x20000u;
			if ((int32_t)rxStatus < 0) {
				cur->flags |= 0x800000u;
			} else {
				uint32_t prevStatus = g_sysconCtrl.lastRxStatus;
				uint16_t cmdCode;

				g_sysconCtrl.lastRxStatus = (uint8_t)rxStatus;
				g_sysconCtrl.prevRxStatus = (uint8_t)rxStatus;
				if (((prevStatus ^ rxStatus) & 8u) != 0)
					sysconDispatchCallback(0x24u, (int)((rxStatus >> 3) & 1u));

				cmdCode = *(const uint16_t *)cur->tx;
				if (cmdCode == 3u || cmdCode == 4u) {
					uint8_t *notifPkt = ((cur->flags & 0x400u) == 0) ? cur->rx :
							    ((cur->extRxReceived == 0) ? cur->extRxBuf : NULL);
					if (notifPkt != NULL) {
						uint32_t changedMask = 0u;

						g_sysconCtrl.statusWord = ((uint32_t)notifPkt[7] << 24) |
									  ((uint32_t)notifPkt[6] << 16) |
									  ((uint32_t)notifPkt[5] << 8) |
									  (uint32_t)notifPkt[4];
						for (i = 0; i < 19u; i++) {
							const SysconNotificationDescriptor *d =
								&s_sysconStandardNotifications[i];
							uint8_t bitVal = ((notifPkt[4u + d->byteIndex] & d->mask) != 0) ^
									 d->polarities;
							uint8_t oldVal = *d->flag;

							*d->flag = bitVal;
							if (oldVal == 0xffu || oldVal != bitVal)
								changedMask |= (1u << i);
						}
						for (i = 0; i < 19u; i++) {
							const SysconNotificationDescriptor *d =
								&s_sysconStandardNotifications[i];
							if ((changedMask & (1u << i)) != 0)
								sysconDispatchCallback(d->subcode, (int)(int8_t)*d->flag);
						}
						if ((changedMask & 0x20u) != 0) {
							if (g_sysconCtrl.notifFlags[5] == 0u) {
								if (g_sysconCtrl.notifFlags[30] == 1u)
									g_sysconCtrl.notifFlags[30] = 0u;
								g_sysconCtrl.extStatusWord = 0x0004ff00u;
								sysconDispatchCallback(0x25u, (int)g_sysconCtrl.extStatusWord);
							} else if (g_sysconCtrl.notifFlags[30] == 0u) {
								g_sysconCtrl.notifFlags[30] = 1u;
							}
						}
					}
				} else if (cmdCode == 0x100u || cmdCode == 0x101u || cmdCode == 0x104u) {
					uint8_t *notifPkt = ((cur->flags & 0x400u) == 0) ? cur->rx :
							    ((cur->extRxReceived == 0) ? cur->extRxBuf : NULL);
					if (notifPkt != NULL) {
						uint32_t changedMask = 0u;

						for (i = 0; i < 4u; i++) {
							const SysconNotificationDescriptor *d =
								&s_sysconExtendedNotifications[i];
							uint8_t bitVal = ((notifPkt[4u + d->byteIndex] & d->mask) != 0) ^
									 d->polarities;
							uint8_t oldVal = *d->flag;

							*d->flag = bitVal;
							if (oldVal == 0xffu || oldVal != bitVal)
								changedMask |= (1u << i);
						}
						for (i = 0; i < 4u; i++) {
							const SysconNotificationDescriptor *d =
								&s_sysconExtendedNotifications[i];
							if ((changedMask & (1u << i)) != 0)
								sysconDispatchCallback(d->subcode, (int)(int8_t)*d->flag);
						}
					}
				} else if (cmdCode == 0x103u) {
					uint8_t *notifPkt = ((cur->flags & 0x400u) == 0) ? cur->rx :
							    ((cur->extRxReceived == 0) ? cur->extRxBuf : NULL);
					if (notifPkt != NULL) {
						slMemcpy(&g_sysconCtrl.extStatusWord, notifPkt + 4u, 4u);
						g_sysconCtrl.notifFlags[30] = 0u;
						sysconDispatchCallback(0x25u, (int)g_sysconCtrl.extStatusWord);
					}
				}

				if ((cur->rx[3] & 0x3fu) == 0 &&
				    (cur->flags & 0x2000400u) == 0x400u &&
				    cur->rx[2] > 1u) {
					uint32_t payloadLen = (uint32_t)cur->rx[2] - 2u;

					if (cur->extRxReceived + payloadLen > cur->extRxMaxSize) {
						cur->flags |= 0x2000000u;
					} else {
						slMemmove(cur->extRxBuf + cur->extRxReceived,
								cur->extRxBuf + cur->extRxReceived + 4u,
								payloadLen);
						cur->extRxReceived += payloadLen;
					}
				}
			}

			if ((cur->flags & 0x800010u) != 0 || (cur->rx[3] & 0x40u) == 0) {
				uint32_t qIdx;

				if ((cur->flags & 0x40000u) != 0) {
					cur->flags &= ~0x40000u;
					g_sysconCtrl.pendingMultiCount--;
				}
				if (*(const int16_t *)cur->tx == 4) {
					uint8_t respLen = cur->rx[2];
					int pos;

					for (pos = 1; pos < (int)respLen - 6; pos += 2) {
						uint16_t completedCmd = *(const uint16_t *)&cur->rx[7 + pos];

						if ((completedCmd & 0x8000u) == 0) {
							uint32_t subQ = (completedCmd >> 8) & 0xffu;
							SceSysconPacket *qPkt;

							if ((int8_t)completedCmd < 0) {
								if (subQ > 15u)
									subQ = 16u;
							} else {
								subQ = 17u;
							}
							qPkt = g_sysconCtrl.queueHead[subQ];
							if (qPkt != NULL) {
								if (completedCmd == *(const uint16_t *)qPkt->tx) {
									if ((qPkt->flags & 0x40000u) == 0) {
										g_sysconCtrl.pendingMultiCount++;
										qPkt->flags |= 0x40000u;
									}
								} else {
									qPkt->flags |= 0x1000000u;
								}
							}
						}
					}
				}

				qIdx = cur->queueIndex;
				g_sysconCtrl.queueHead[qIdx] = cur->next;
				if (cur->next == NULL)
					g_sysconCtrl.queueTail[qIdx] = NULL;
				cur->next = NULL;
				cur->flags = (cur->flags & ~0x40090000u) | 0x80000u;

				if (cur->callback != NULL) {
					int cbAction = cur->callback(cur, cur->callbackArg);

					if (cbAction == 1) {
						SceSysconPacket *oldHead = g_sysconCtrl.queueHead[qIdx];

						g_sysconCtrl.queueHead[qIdx] = cur;
						if (oldHead == NULL)
							g_sysconCtrl.queueTail[qIdx] = cur;
						cur->next = oldHead;
						cur->flags = (cur->flags & ~0x90000u) | 0x10000u;
					} else if (cbAction == 2) {
						if (g_sysconCtrl.queueTail[qIdx] != NULL)
							g_sysconCtrl.queueTail[qIdx]->next = cur;
						g_sysconCtrl.queueTail[qIdx] = cur;
						cur->next = NULL;
						cur->flags = (cur->flags & ~0x90000u) | 0x10000u;
					}
				}
			}

			/* Select next queued packet to transmit */
			cur = g_sysconCtrl.queueHead[17];
			if (cur == NULL) {
				SceSysconPacket *firstReady = NULL;
				SceSysconPacket *firstPending = NULL;
				uint32_t scanIdx = g_sysconCtrl.nextQueueIndex;

				for (i = 0; i < 17u; i++) {
					SceSysconPacket *cand = g_sysconCtrl.queueHead[scanIdx];

					if (cand != NULL) {
						if ((cand->flags & 0x40000u) != 0) {
							cur = cand;
							goto found_next;
						}
						if ((cand->flags & 0x40000000u) == 0) {
							if (firstReady == NULL)
								firstReady = cand;
						} else if (firstPending == NULL) {
							firstPending = cand;
						}
					}
					scanIdx++;
					if (scanIdx == 17u)
						scanIdx = 0u;
				}
				cur = firstReady;
				if (firstReady == NULL && firstPending != NULL &&
				    (g_sysconCtrl.allowQueuedFlag1 != 0 || g_sysconCtrl.allowQueuedFlag2 != 0)) {
					cur = firstPending;
				}
found_next:
				if (g_sysconCtrl.nextQueueIndex < 16u)
					g_sysconCtrl.nextQueueIndex++;
				else
					g_sysconCtrl.nextQueueIndex = 0u;
			}

			if (cur != NULL) {
				cmepDelayUs(g_sysconCtrl.retryDelayUs);
				g_sysconCtrl.activePacket = cur;
				sysconStartPacketTx(cur);
			}
			g_sysconCtrl.inInterrupt = 0u;
		}
	} else {
		if (noWait != 1)
			return (int)0x80250000;
		if (pkt == NULL) {
			unsigned int q;

			for (q = 0; q < 17u; q++) {
				if (g_sysconCtrl.queueHead[q] != NULL)
					return 1;
			}
			return 0;
		}
		if ((pkt->flags & 0x80000u) == 0)
			return 1;
	}

	if ((pkt->flags & 0xb00000u) != 0)
		return (int)0x80250101;
	if ((pkt->flags & 0x2000000u) != 0)
		return (int)0x80250105;
	if ((pkt->rx[3] & 0x3fu) == 0)
		return 0;
	return (int)((uint32_t)pkt->rx[3] | 0x80250200u);
}

/* 0x80422a */
int sceSysconCmdSync(SceSysconPacket *pkt, uint32_t flags)
{
	int status;

	status = sceSysconCmdExecAsync(pkt, flags, NULL, NULL);
	if (status >= 0)
		status = sceSysconCmdWait(pkt, 0);
	return status;
}

/* 0x804258 */
static int sysconReadU32Command(uint32_t *outVal, uint16_t cmdCode)
{
	SceSysconPacket pkt;
	uint32_t tmp;
	int status;

	if (outVal == NULL)
		return (int)0x80250002;

	pkt.tx[0] = (uint8_t)cmdCode;
	pkt.tx[1] = (uint8_t)(cmdCode >> 8);
	pkt.tx[2] = 1u;
	status = sceSysconCmdSync(&pkt, 0u);
	if (status >= 0) {
		if (((pkt.rx[2] - 3u) & 0xffu) > 3u)
			return (int)0x80250100;
		tmp = 0u;
		slMemcpy(&tmp, &pkt.rx[4], (size_t)(pkt.rx[2] - 2u));
		*outVal = tmp;
		status = 0;
	}
	return status;
}

/* 0x8042da */
uint8_t sceSysconGetBaryonTimestamp(void)
{
	return g_sysconCtrl.lastRxStatus;
}

/* 0x8042e4 */
uint8_t sceSysconGetBaryonModeBit0(void)
{
	return (g_sysconCtrl.lastRxStatus >> 6) & 1u;
}

/* 0x8042f4 */
int sceSysconGetSnvsVersionInfo(uint32_t *outVer)
{
	SceSysconPacket pkt;
	uint8_t *dst = (uint8_t *)outVer;
	int status;

	if (outVer == NULL)
		return (int)0x80250001;

	pkt.tx[0] = 0u;
	pkt.tx[1] = 0x11u;
	pkt.tx[2] = 1u;
	pkt.tx[3] = 0u;
	status = sceSysconCmdSync(&pkt, 0u);
	if (status >= 0) {
		dst[3] = pkt.rx[4];
		dst[2] = pkt.rx[5];
		dst[1] = pkt.rx[6];
		dst[0] = pkt.rx[7];
	}
	return status;
}

/* 0x80436e */
int sceSysconNvsReadData(uint32_t offset, void *dst, uint32_t size)
{
	uint8_t rxBuf[64];
	SceSysconPacket pkt;
	int status;

	if ((size != 1u && size != 2u && size != 4u && size != 8u &&
	     size != 16u && size != 32u) ||
	    ((size - 1u) & offset) != 0 ||
	    offset + size > 0xb60u) {
		return (int)0x80250001;
	}

	slMemset(rxBuf, 0, sizeof(rxBuf));
	pkt.flags = 0x400u;
	pkt.tx[0] = 0x82u;
	pkt.tx[1] = 0x10u;
	pkt.tx[2] = 4u;
	pkt.tx[3] = (uint8_t)offset;
	pkt.tx[4] = (uint8_t)(offset >> 8);
	pkt.tx[5] = (uint8_t)size;
	pkt.extRxBuf = rxBuf;
	pkt.extRxMaxSize = sizeof(rxBuf);

	status = sceSysconExecTwoPhaseCmd(&pkt, 0x400u);
	if (status >= 0) {
		slMemcpy(dst, pkt.extRxBuf, size);
		status = 0;
	}
	return status;
}

/* 0x804440 */
int sceSysconQueryBaryonVersion(uint32_t *outVersion)
{
	return sysconReadU32Command(outVersion, 0x0001u);
}

/* 0x804446 */
int sceSysconQueryBuildVersion(uint8_t *outBuildVer13)
{
	SceSysconPacket pkt;
	int status;

	if (outBuildVer13 == NULL)
		return (int)0x80250002;

	pkt.tx[0] = 2u;
	pkt.tx[1] = 0u;
	pkt.tx[2] = 1u;
	status = sceSysconCmdSync(&pkt, 0u);
	if (status >= 0) {
		slMemcpy(outBuildVer13, &pkt.rx[4], 12u);
		outBuildVer13[12] = 0u;
		status = 0;
	}
	return status;
}

/* 0x8044b4 */
int sceSysconSetAlertLevel(uint32_t level)
{
	return sceSysconSendShortCommand(level, 0x0080u, 3u);
}

/* 0x8044be */
int sceSysconQueryHardwareInfo(uint32_t *outHwInfo)
{
	return sysconReadU32Command(outHwInfo, 0x0005u);
}

/* 0x8044c4 */
int sceSysconReadBootSummary(void *outSummary16)
{
	SceSysconPacket pkt;
	int status;

	if (outSummary16 == NULL)
		return (int)0x80250002;

	pkt.tx[0] = 6u;
	pkt.tx[1] = 0u;
	pkt.tx[2] = 1u;
	status = sceSysconCmdSync(&pkt, 0u);
	if (status >= 0) {
		slMemcpy(outSummary16, &pkt.rx[4], 16u);
		status = 0;
	}
	return status;
}

/* 0x80452a */
int sceSysconGetWakeupFactor(uint32_t *outFactor)
{
	return sysconReadU32Command(outFactor, 0x0010u);
}

/* 0x804530 */
int sceSysconReadScratchpad(uint16_t offset, void *dst, uint32_t size)
{
	uint8_t rxBuf[64];
	SceSysconPacket pkt;
	int status;

	if ((size != 1u && size != 2u && size != 4u && size != 8u &&
	     size != 16u && size != 32u) ||
	    offset + size > 0x100u) {
		return (int)0x80250001;
	}

	slMemset(rxBuf, 0, sizeof(rxBuf));
	pkt.flags = 0x400u;
	pkt.tx[0] = 0x90u;
	pkt.tx[1] = 0u;
	pkt.tx[2] = 4u;
	pkt.tx[3] = (uint8_t)offset;
	pkt.tx[4] = (uint8_t)(offset >> 8);
	pkt.tx[5] = (uint8_t)size;
	pkt.extRxBuf = rxBuf;
	pkt.extRxMaxSize = sizeof(rxBuf);

	status = sceSysconExecTwoPhaseCmd(&pkt, 0x400u);
	if (status >= 0) {
		slMemcpy(dst, pkt.extRxBuf, size);
		status = 0;
	}
	return status;
}

/* 0x8045fc */
int sceSysconSetLifecycleMode(uint32_t mode)
{
	return sceSysconSendShortCommand(mode, 0x00b2u, 2u);
}

/* 0x804606 */
int sceSysconSetPowerMode(uint8_t mode, uint32_t mask)
{
	SceSysconPacket pkt;
	uint32_t invMask = ~mask;

	pkt.tx[0] = 0xc0u;
	pkt.tx[1] = 0u;
	pkt.tx[2] = 5u;
	pkt.tx[3] = mode;
	pkt.tx[4] = (uint8_t)invMask;
	pkt.tx[5] = (uint8_t)(invMask >> 8);
	pkt.tx[6] = (uint8_t)(invMask >> 16);
	return sceSysconExecTwoPhaseCmd(&pkt, 0u);
}

/* 0x804664 */
int sceSysconSendHandshakePacket(const void *txBuf40, uint32_t txSize)
{
	if (txBuf40 == NULL)
		return (int)0x80250002;
	if (txSize != 40u)
		return (int)0x80250001;

	slMemset(s_sysconHandshakeTxBuf, 0, sizeof(s_sysconHandshakeTxBuf));
	s_sysconHandshakePkt.extTxBuf = s_sysconHandshakeTxBuf;
	slMemset(s_sysconHandshakeRxBuf, 0, sizeof(s_sysconHandshakeRxBuf));
	s_sysconHandshakePkt.extRxBuf = s_sysconHandshakeRxBuf;
	s_sysconHandshakePkt.extRxMaxSize = sizeof(s_sysconHandshakeRxBuf);
	s_sysconHandshakePkt.flags = 0xc00u;

	s_sysconHandshakeTxBuf[0] = 0xa0u;
	s_sysconHandshakeTxBuf[1] = 0u;
	s_sysconHandshakeTxBuf[2] = 41u;
	slMemcpy(&s_sysconHandshakeTxBuf[3], txBuf40, 40u);
	return sceSysconCmdSync(&s_sysconHandshakePkt, 0xc00u);
}

/* 0x804706 */
int sceSysconReceiveHandshakePacket(void *rxBuf40, uint32_t rxSize)
{
	int status;

	if (rxBuf40 == NULL)
		return (int)0x80250002;
	if (rxSize != 40u)
		return (int)0x80250001;

	status = sysconWaitStatusBit(0x20u, -1);
	if (status >= 0) {
		status = sceSysconCmdSync(&s_sysconHandshakePkt, 0xc00u);
		if (status >= 0)
			slMemcpy(rxBuf40, s_sysconHandshakePkt.extRxBuf, 40u);
	}
	return status;
}

/* 0x804764 */
int sceSysconStartAsyncBootCallback(int (*callback)(void))
{
	s_sysconAsyncCallback = callback;
	return 0;
}

/* 0x80476c */
int sceSysconPollAsyncCallbacks(void)
{
	if (s_sysconAsyncCallback != NULL)
		g_sysconAsyncCallbackStatus = s_sysconAsyncCallback();
	return 0;
}

/* 0x804786 */
int sceSysconWaitAsyncBootCallback(int *outStatus)
{
	*outStatus = g_sysconAsyncCallbackStatus;
	g_sysconAsyncCallbackStatus = (int)0x800f002c;
	s_sysconAsyncCallback = NULL;
	return 0;
}

/* 0x8047a6 */
static int sysconDelayAndCheckTimeout(int maxTimeoutMs)
{
	sysconDelayUsThunk(10000u);
	if (maxTimeoutMs < 1)
		return 0;
	s_sysconPollElapsedMs += 10;
	return s_sysconPollElapsedMs >= maxTimeoutMs;
}

/* 0x8047d8 */
int sceSysconResetPollTimer(void)
{
	s_sysconPollElapsedMs = 0;
	return 0;
}

/* 0x8047e2 */
int sysconWaitStatusBit(uint8_t statusMask, int maxTimeoutMs)
{
	SceSysconPacket pkt;
	int status;

	do {
		pkt.tx[0] = 0u;
		pkt.tx[1] = 0u;
		pkt.tx[2] = 1u;
		status = sceSysconCmdSync(&pkt, 0u);
		if (status < 0)
			return status;
		if ((pkt.rx[0] & statusMask) != 0)
			return 0;
	} while (!sysconDelayAndCheckTimeout(maxTimeoutMs));
	return (int)0x80250004;
}

/* 0x804858 */
int sceSysconExecTwoPhaseCmd(SceSysconPacket *pkt, uint32_t flags)
{
	int status;

	status = sceSysconCmdSync(pkt, flags);
	if (status >= 0) {
		sceSysconPollAsyncCallbacks();
		status = sysconWaitStatusBit(0x20u, -1);
		if (status >= 0)
			status = sceSysconCmdSync(pkt, flags);
	}
	return status;
}

/* 0x804896 */
int sceSysconGetPoweroffFactor(uint32_t *outFactor)
{
	*outFactor = 0u;
	return sysconReadU32Command(outFactor, 0x0800u);
}

/* 0x8048a6 */
int sceSysconCtrlSdPower(uint32_t enable)
{
	return sceSysconSendShortCommand(enable, 0x0888u, 2u);
}

/* 0x8048b0 */
int sceSysconSetRailVoltage(uint8_t railId, uint32_t vidCode)
{
	SceSysconPacket pkt;
	uint32_t hwModel;
	int status;

	if (vidCode < 0x40u) {
		hwModel = sceSysconGetHardwareInfo() & 0x00ff0000u;
		if (hwModel >= 0x00600000u && hwModel != 0x00700000u) {
			if ((hwModel < 0x00800000u && hwModel != 0x00720000u) ||
			    vidCode < 0x3cu) {
				return (int)0x80250001;
			}
		}
	} else {
		hwModel = sceSysconGetHardwareInfo() & 0x00ff0000u;
		if (hwModel < 0x00600000u || hwModel == 0x00700000u) {
			if (vidCode - 0x4du > 0x3fu)
				return (int)0x80250001;
			vidCode = 0x8cu - vidCode;
		}
	}

	pkt.tx[0] = 0x8eu;
	pkt.tx[1] = 8u;
	pkt.tx[2] = 3u;
	pkt.tx[3] = railId;
	pkt.tx[4] = (uint8_t)vidCode;
	status = sceSysconExecTwoPhaseCmd(&pkt, 0u);
	if (status > 0)
		status = 0;
	return status;
}

/* 0x804992 */
int sceSysconGetUsbDetStatus(uint32_t *outStatus)
{
	uint32_t val;
	int status;

	status = sysconReadU32Command(&val, 0x0100u);
	if (status >= 0)
		slMemcpy(outStatus, &val, sizeof(val));
	return status;
}

/* 0x8049ca */
int sceSysconCtrlDevicePower(uint32_t param)
{
	return sceSysconSendShortCommand(param, 0x0190u, 3u);
}

/* 0x8049d4 */
int sceSysconGetPowerInfo(uint32_t *outPowerInfo)
{
	return sysconReadU32Command(outPowerInfo, 0x0003u);
}

/* 0x8049da */
int sceSysconExchangeHandshakePacket(const void *txBuf40, uint8_t txSize,
		void *rxBuf40, uint8_t rxSize)
{
	uint8_t rxScratch[128];
	uint8_t txScratch[128];
	SceSysconPacket pkt;
	int status;

	if (!txBuf40 || txSize != 40u || !rxBuf40 || rxSize != 40u)
		return (int)0x80250001;

	slMemset(txScratch, 0, sizeof(txScratch));
	slMemset(rxScratch, 0, sizeof(rxScratch));
	pkt.flags = 0xc00u;
	pkt.tx[0] = 0xd0u;
	pkt.tx[1] = 0u;
	pkt.tx[2] = 41u;
	txScratch[0] = 0xd0u;
	txScratch[2] = 41u;
	pkt.extTxBuf = txScratch;
	pkt.extRxBuf = rxScratch;
	pkt.extRxMaxSize = 46u;
	slMemcpy(&txScratch[3], txBuf40, 40u);

	status = sceSysconExecTwoPhaseCmd(&pkt, 0xc00u);
	if (status >= 0)
		slMemcpy(rxBuf40, rxScratch, 40u);
	return status;
}

/* 0x804ab0 */
int sceSysconSnvsExchangePacket(const void *txBuf16, uint8_t txSize,
		void *rxBuf48, uint8_t rxSize)
{
	uint8_t rxScratch[128];
	uint8_t txScratch[128];
	SceSysconPacket pkt;
	int status;

	if (!txBuf16 || txSize != 16u || !rxBuf48 || rxSize != 48u)
		return (int)0x80250001;

	slMemset(txScratch, 0, sizeof(txScratch));
	slMemset(rxScratch, 0, sizeof(rxScratch));
	pkt.flags = 0xc00u;
	pkt.tx[0] = 0xd2u;
	pkt.tx[1] = 0u;
	pkt.tx[2] = 17u;
	txScratch[0] = 0xd2u;
	txScratch[2] = 17u;
	pkt.extTxBuf = txScratch;
	pkt.extRxBuf = rxScratch;
	pkt.extRxMaxSize = 54u;
	slMemcpy(&txScratch[3], txBuf16, 16u);

	status = sceSysconExecTwoPhaseCmd(&pkt, 0xc00u);
	if (status >= 0)
		slMemcpy(rxBuf48, rxScratch, 48u);
	return status;
}

/* 0x804b8a */
int sceSysconSendShortCommand(uint32_t payload, uint16_t cmdCode, uint8_t payloadLen)
{
	SceSysconPacket pkt;
	int status;

	pkt.tx[0] = (uint8_t)cmdCode;
	pkt.tx[1] = (uint8_t)(cmdCode >> 8);
	pkt.tx[2] = payloadLen;
	pkt.tx[3] = (uint8_t)payload;
	pkt.tx[4] = (uint8_t)(payload >> 8);
	pkt.tx[5] = (uint8_t)(payload >> 16);
	pkt.tx[6] = (uint8_t)(payload >> 24);

	status = sceSysconExecTwoPhaseCmd(&pkt, 0u);
	if (status > 0)
		status = 0;
	return status;
}
