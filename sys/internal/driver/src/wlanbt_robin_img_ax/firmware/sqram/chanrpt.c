/*
 * HostCmd_CMD_CHAN_REPORT_REQUEST (0x00dd) and its EVENT_CHANNEL_REPORT_RDY
 * (SQRAM 0xc000974c..0xc0009b34): the radio dwells on the channel the host
 * names for the time it asks, measuring the channel's load and its noise
 * histogram and watching for frames; the report goes to the host as the
 * event. The command, the event and the 11h basic report TLV are those of
 * Marvell's host interface (NXP's mlan: HostCmd_DS_CHAN_RPT_REQ,
 * HostCmd_DS_CHAN_RPT_RSP, TLV_TYPE_CHANRPT_11H_BASIC); the other two TLVs
 * are named from what they carry. The filename and the other names are
 * inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/slot.h"
#include "robin/task.h"
#include "robin/txring.h"

#define HostCmd_CMD_CHAN_REPORT_REQUEST	0x00dd
#define EVENT_CHANNEL_REPORT_RDY	0x54

/* The TLVs of the event; 0x15b is TLV_TYPE_CHANRPT_11H_BASIC. */
#define TLV_TYPE_CHANRPT_CHAN_LOAD	0x0159
#define TLV_TYPE_CHANRPT_NOISE_HIST	0x015a
#define TLV_TYPE_CHANRPT_11H_BASIC	0x015b

/* MeasRptBasicMap_t's bits (IEEE 802.11h basic report). */
#define MEAS_RPT_BSS			0x01
#define MEAS_RPT_OFDM_PREAMBLE		0x02
#define MEAS_RPT_RADAR			0x08

/* Slot events other than slot.h's; 6 is the code chanRptFinish() aborts the slot with. */
#define CHAN_RPT_EVENT_HOLD		1
#define CHAN_RPT_EVENT_RELEASE		2
#define CHAN_RPT_EVENT_EXPIRED		4
#define CHAN_RPT_EVENT_FINISHED		6
#define CHAN_RPT_EVENT_ENDED		8

#define CHAN_RPT_BUFFER_SIZE		0x386
#define CHAN_RPT_BUFFER_CLASS		16
#define CHAN_RPT_EVENT_SIZE		0x5b
#define CHAN_RPT_SLOT_MARGIN		10000	/* microseconds the slot may run past the dwell */
#define CHAN_RPT_HISTOGRAM_BINS		11
/* The MAC events kept unmasked while the histogram runs (inferred). */
#define CHAN_RPT_EVENT_MASK		0x806200ca

/* The command: HostCmd_DS_CHAN_RPT_REQ, then TLVs. */
struct ChanRptCommand {
	uint8_t startFrequency[2];	/* MrvlChannelDesc_t, little endian */
	uint8_t bandConfig;
	uint8_t channel;
	uint8_t dwellTime[4];		/* milliseconds */
	uint8_t tlvs[];
};

/* What the measurement is asked to do. */
struct ChanRptRequest {
	uint8_t band;			/* bits 0-1: the band; bits 2-3: the command's band configuration bits 0-1 */
	uint8_t channel;
	uint8_t reserved2[2];
	uint32_t delay;			/* before the slot starts */
	uint32_t reserved8[2];
	uint32_t dwell;			/* microseconds */
	uint8_t reserved20[40];
	uint8_t histogramWideRange;
	uint8_t reserved61[3];
	uint32_t radar;			/* the host asked for the radar bit */
	uint8_t reserved68[4];
};

_Static_assert(offsetof(struct ChanRptRequest, histogramWideRange) == 60, "channel report request");
_Static_assert(sizeof(struct ChanRptRequest) == 72, "channel report request");

/* The report as it is measured, in the measurement's buffer. */
struct ChanRptReport {
	uint64_t startTsf;
	uint64_t duration;		/* microseconds */
	uint8_t band;			/* bits 0-1: band, bits 2-3: band configuration */
	uint8_t channel;
	uint8_t basicMap;		/* MeasRptBasicMap_t */
	uint8_t channelLoad;		/* busy time, in 255ths of the dwell */
	uint16_t noiseAverage;
	uint8_t histogramBins;
	uint8_t histogram[CHAN_RPT_HISTOGRAM_BINS];
	uint8_t reserved34[22];
};

_Static_assert(sizeof(struct ChanRptReport) == 56, "channel report");

/* The measurement in progress (0x5e868). */
struct ChanRptState {
	int (*done)(int status, OsaBuffer *buffer);	/* 0 */
	uint32_t slot;			/* 4: the slot scheduler's handle, 0: none */
	uint8_t status;			/* 8: passed to done */
	uint8_t reserved9[3];
	uint32_t dwell;			/* 12 */
	uint32_t held;			/* 16 */
	uint32_t expired;		/* 20: the measurement ran to its end */
	uint32_t savedEventMask;	/* 24 */
	uint8_t histogramWideRange;	/* 28 */
	uint8_t reserved29[3];
	OsaBuffer *buffer;		/* 32 */
	struct ChanRptReport *report;	/* 36 */
	struct WlanConnection *connection;	/* 40 */
	uint32_t radar;			/* 44 */
};

_Static_assert(sizeof(struct ChanRptState) == 48, "channel report state");

struct ChanRptState chanRptState __attribute__((section(".itcm.bss")));

static int chanRptSlotCallback(uint32_t handle, int event, uint32_t argument);
static int chanRptFrameReceived(uint32_t handle, int unused, OsaBuffer *frame, const uint8_t *info);
static int chanRptSendEvent(int status, OsaBuffer *buffer);

/* Ends the measurement: the time it ran, the channel's load and the noise histogram. */
static void __attribute__((noinline)) chanRptFinish(int expired)
{
	struct ChanRptState *state = &chanRptState;
	struct ChanRptReport *report;
	uint8_t histogram[12];
	uint16_t average;
	uint32_t busy;
	uint8_t bins;

	romMacEventMaskSet(state->savedEventMask);
	state->expired = expired;
	busy = wlanMacRead5f8();
	report = state->report;
	report->duration = romGetTimerTimestamp() - report->startTsf;
	report->channelLoad = (uint64_t)(busy * 255) / report->duration;
	__builtin_memset(histogram, 0, sizeof(histogram));
	bins = bbpNoiseHistogramRead(histogram, &average);
	report = state->report;
	report->histogramBins = bins;
	report->noiseAverage = average;
	__aeabi_memcpy(report->histogram, histogram, CHAN_RPT_HISTOGRAM_BINS);
	state->report->basicMap &= ~MEAS_RPT_RADAR;
	wlanSlotAbort(state->slot, CHAN_RPT_EVENT_FINISHED);
}

/* Hands the measurement's buffer to whoever asked for it. */
static int __attribute__((noinline)) chanRptComplete(void)
{
	struct ChanRptState *state = &chanRptState;

	if (state->done != NULL)
		return state->done(state->status, state->buffer);
	return osa_BufferFree(state->buffer);
}

static int chanRptSlotCallback(uint32_t handle, int event, uint32_t argument)
{
	struct ChanRptState *state = &chanRptState;

	if (state->slot != handle) {
		if (state->slot != 0) {
			chanRptFinish(0);
			wlanSlotAbort(state->slot, CHAN_RPT_EVENT_FINISHED);
		}
		return WLAN_SLOT_FAILED;
	}
	switch (event) {
	case CHAN_RPT_EVENT_HOLD:
		state->held = 1;
		break;
	case CHAN_RPT_EVENT_RELEASE:
		state->held = 0;
		break;
	case WLAN_SLOT_EVENT_START:
		if (state->dwell == 0) {
			chanRptFinish(0);
			wlanSlotAbort(state->slot, CHAN_RPT_EVENT_FINISHED);
			break;
		}
		state->report->startTsf = romGetTimerTimestamp();
		wlanMacClear46cBits();
		state->savedEventMask = romMacEventMaskGet();
		romMacEventMaskSet(state->savedEventMask | CHAN_RPT_EVENT_MASK);
		wlanMacRead5f8();
		bbpNoiseHistogramStart(state->dwell - (state->dwell >> 4), 0, state->histogramWideRange);
		break;
	case CHAN_RPT_EVENT_EXPIRED:
		chanRptFinish(1);
		wlanSlotAbort(state->slot, CHAN_RPT_EVENT_FINISHED);
		break;
	case WLAN_SLOT_EVENT_POLL:
		chanRptFinish(0);
		wlanSlotAbort(state->slot, CHAN_RPT_EVENT_FINISHED);
		break;
	case WLAN_SLOT_EVENT_CANCEL:
		chanRptFinish(0);
		if (state->buffer != NULL) {
			osa_BufferFree(state->buffer);
			state->buffer = NULL;
		}
		/* fall through */
	case CHAN_RPT_EVENT_FINISHED:
	case CHAN_RPT_EVENT_ENDED:
		state->slot = 0;
		chanRptComplete();
		break;
	}
	return WLAN_SLOT_OK;
}

/* A frame was heard on the channel: a BSS with OFDM preambles is there. */
static int chanRptFrameReceived(uint32_t handle, int unused, OsaBuffer *frame, const uint8_t *info)
{
	const uint8_t *data = NULL;

	if (frame != NULL)
		data = (const uint8_t *)frame + frame->dataOffset;
	if (info != NULL || data != NULL) {
		rfGetBand();
		chanRptState.report->basicMap |= MEAS_RPT_OFDM_PREAMBLE;
		chanRptState.report->basicMap |= MEAS_RPT_BSS;
	}
	return 0;
}

/* Starts a measurement on the slot scheduler; `done` gets the report buffer, or (0, NULL) when none can start. */
static int __attribute__((noinline, noclone)) chanRptStart(struct WlanConnection *connection,
						  const struct ChanRptRequest *request,
						  int (*done)(int status, OsaBuffer *buffer))
{
	struct ChanRptState *state = &chanRptState;
	struct WlanSlotParams params;
	struct ChanRptReport *report;
	OsaBuffer *buffer;

	if (state->slot != 0)
		return done(0, NULL);
	__aeabi_memclr4(&params, sizeof(params));
	__aeabi_memclr4(state, sizeof(*state));
	state->connection = connection;
	state->slot = 0;
	while ((buffer = osa_BufferAlloc(&wlanBlockPool7, CHAN_RPT_BUFFER_SIZE, 0)) == NULL)
		osaTaskSleep(1);
	buffer->packetClass = CHAN_RPT_BUFFER_CLASS;
	buffer->context = connection;
	state->buffer = buffer;
	state->report = (struct ChanRptReport *)((uint8_t *)buffer + buffer->dataOffset);
	__aeabi_memclr4(state->report, sizeof(*state->report));
	state->done = done;
	state->dwell = request->dwell;
	state->histogramWideRange = request->histogramWideRange;
	report = state->report;
	report->channel = request->channel;
	report->band = (report->band & ~3) | (request->band & 3);
	report->band = (report->band & 0xf3) | (((request->band >> 2) & 3) << 2);

	params.callback = chanRptSlotCallback;
	params.context = (uint32_t)(uintptr_t)chanRptFrameReceived;
	params.mode = request->channel;
	params.bssTypeLow = request->band & 3;
	params.bssTypeMiddle = (request->band >> 2) & 3;
	params.duration = request->dwell;
	params.tag = request->dwell + CHAN_RPT_SLOT_MARGIN;
	params.delay = request->delay;
	params.flags |= 6;
	if (request->radar != 0)
		state->radar = 1;
	if (macMgmtDispatchContext(params.mode, params.bssTypeLow, params.bssTypeMiddle))
		params.flags &= ~1;
	else
		params.flags |= 1;
	params.connection = state->connection;
	state->slot = wlanSlotSubmit(&params);
	if (state->slot == 0)
		return done(0, NULL);
	return state->slot;
}

/* Writes a TLV header, little endian, byte by byte. */
static inline __attribute__((always_inline)) uint8_t *chanRptPutTlvHeader(uint8_t *tlv, unsigned int type,
									  unsigned int length)
{
	tlv[0] = type;
	tlv[1] = type >> 8;
	tlv[2] = length;
	tlv[3] = length >> 8;
	return tlv + 4;
}

/* EVENT_CHANNEL_REPORT_RDY: HostCmd_DS_CHAN_RPT_RSP with the load, noise histogram and basic report TLVs. */
static int chanRptSendEvent(int status, OsaBuffer *buffer)
{
	const struct ChanRptReport *report = (const struct ChanRptReport *)((uint8_t *)buffer + buffer->dataOffset);
	struct WlanHostEventFrame *frame;
	OsaBuffer *event;
	uint8_t *data, *tlv;
	unsigned int length;

	if (wlanTxAllocFrame(&event, (WlanTxDescriptor **)&frame, CHAN_RPT_EVENT_SIZE,
			     EVENT_CHANNEL_REPORT_RDY) == 0)
		return 0;
	data = (uint8_t *)&frame->code;
	__aeabi_uwrite4(0, data);
	__aeabi_memcpy(data + 4, &report->startTsf, 8);
	__aeabi_uwrite4((uint32_t)report->duration, data + 12);
	tlv = chanRptPutTlvHeader(data + 16, TLV_TYPE_CHANRPT_CHAN_LOAD, 1);
	tlv[0] = report->channelLoad;
	tlv = data + 21;
	chanRptPutTlvHeader(tlv, TLV_TYPE_CHANRPT_NOISE_HIST, report->histogramBins + 2);
	tlv[4] = report->noiseAverage;
	tlv[5] = report->noiseAverage >> 8;
	__aeabi_memcpy(tlv + 6, report->histogram, report->histogramBins);
	tlv += ((tlv[3] << 8) | tlv[2]) + 4;
	chanRptPutTlvHeader(tlv, TLV_TYPE_CHANRPT_11H_BASIC, 1);
	tlv[4] = report->basicMap;
	tlv += 5;
	osa_BufferFree(buffer);
	length = tlv - data;
	((uint8_t *)&frame->length)[0] = length;
	((uint8_t *)&frame->length)[1] = length >> 8;
	wlanHostEventSend(chanRptState.connection, event);
	return 1;
}

/* HostCmd_CMD_CHAN_REPORT_REQUEST: 1 while a measurement runs, else 0 and the measurement started. */
int hostCmd00dd(struct WlanConnection *connection, uint8_t *body, unsigned int length)
{
	const struct ChanRptCommand *command = (const struct ChanRptCommand *)body;
	struct ChanRptRequest request;
	const uint8_t *tlv;

	if (chanRptState.slot != 0)
		return 1;
	__aeabi_memclr4(&request, sizeof(request));
	request.band = (request.band & 0xf3) | ((command->bandConfig & 3) << 2);
	request.histogramWideRange = 1;
	request.channel = command->channel;
	request.band = (request.band & ~3) |
		       (wlanStartFrequencyBand((command->startFrequency[1] << 8) | command->startFrequency[0],
					       command->channel) & 3);
	request.delay = 0;
	request.reserved8[0] = 0;
	request.reserved8[1] = 0;
	request.dwell = __aeabi_uread4(command->dwellTime) * 1000;
	if (length != sizeof(struct ChanRptCommand)) {
		tlv = command->tlvs;
		if (((tlv[1] << 8) | tlv[0]) == TLV_TYPE_CHANRPT_11H_BASIC && (tlv[4] & MEAS_RPT_RADAR))
			request.radar = 1;
	}
	chanRptStart(connection, &request, chanRptSendEvent);
	return 0;
}
