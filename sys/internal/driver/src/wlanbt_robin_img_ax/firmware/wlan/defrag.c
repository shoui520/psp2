/*
 * 802.11 defragmentation (0x7bf8..0x7ffc). The first fragment's 32-byte
 * header is copied into a pool buffer and every fragment's body appended to
 * it; a fragment out of sequence, from another transmitter or out of order
 * discards the MSDU.
 */
#include "robin/btrom.h"
#include "robin/defrag.h"
#include "robin/mef.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/threadx.h"

ROBIN_PATCH_SLOT(wlanDefragmentOverride);
ROBIN_PATCH_SLOT(wlanDefragmentWepOverride);

/* the 802.11 header the reassembled frame keeps: bodyLength and the four-address header */
#define WLAN_DEFRAG_HEADER_SIZE	32

static inline uint16_t frameBodyLength(const WlanWirelessFrame *frame)
{
	return (frame->bodyLength[1] << 8) | frame->bodyLength[0];
}

static inline void setFrameBodyLength(WlanWirelessFrame *frame, uint32_t length)
{
	frame->bodyLength[0] = length;
	frame->bodyLength[1] = length >> 8;
}

static inline void releaseRxBuffer(OsaBuffer *rxBuffer)
{
	wlanRomServices.releaseDmaSpan(rxBuffer->ringStart, rxBuffer->ringEnd, 3);
}

/* Waits for a reassembly buffer. */
static void allocReassemblyBuffer(OsaBuffer **reassembly, uint32_t size)
{
	while ((*reassembly = osa_BufferAlloc(wlanRomServices.packetPool, size, 0)) == NULL)
		tx_thread_sleep(1);
}

void wlanDefragReset(WlanDefragContext *context)
{
	context->sequenceNumber = 0;
	context->writePtr = NULL;
	context->lastFragmentNumber = 0xff;
	context->buffer = NULL;
}

/*
 * Takes one received fragment. Returns the reassembled MSDU when this was the
 * last fragment, rxBuffer itself for an unfragmented frame, else NULL.
 * *discarding is set while the rest of a broken MSDU is dropped.
 */
OsaBuffer *wlanDefragment(OsaBuffer *rxBuffer, WlanWirelessFrame *frame,
			  OsaBuffer **reassembly, WlanDefragContext *context,
			  uint8_t *discarding)
{
	OsaBuffer *result;
	WlanWirelessFrame *assembled;
	uint32_t sequenceNumber, fragmentNumber;
	uint32_t extra, headroomCopied, copyLength;

	if (wlanDefragmentOverride(rxBuffer, frame, reassembly, context, discarding, &result))
		return result;

	sequenceNumber = ((frame->sequenceControl[1] << 8) | frame->sequenceControl[0]) >> 4;
	fragmentNumber = frame->sequenceControl[0] & 0xf;

	if (fragmentNumber == 0 && !(frame->frameControl[1] & WLAN_FC1_MORE_FRAGMENTS)) {
		/* not fragmented */
		rxBuffer->length = frameBodyLength(frame);
		return rxBuffer;
	}

	extra = rxBuffer->length > WLAN_DEFRAG_HEADER_SIZE ?
		(uint16_t)(rxBuffer->length - WLAN_DEFRAG_HEADER_SIZE) : 0;

	if (fragmentNumber == 0) {
		/* the first fragment: start a new MSDU */
		*discarding = 0;
		if (*reassembly == NULL)
			allocReassemblyBuffer(reassembly, WLAN_DEFRAG_BUFFER_SIZE);

		headroomCopied = 0;
		wlanRomServices.resetBufferDataOffset(*reassembly);
		wlanDefragReset(context);
		context->buffer = *reassembly;
		assembled = (WlanWirelessFrame *)((uint8_t *)*reassembly + (*reassembly)->dataOffset);
		context->writePtr = (uint8_t *)assembled + WLAN_DEFRAG_HEADER_SIZE;
		wlanRomServices.submitBbpCommand((uint32_t)assembled, (uint32_t)frame,
						 WLAN_DEFRAG_HEADER_SIZE);
		context->startTime = SOC_MAC_TIMER + wlanRomServices.timerOffset;
		(*reassembly)->context = rxBuffer->context;
	} else if (*discarding == 1) {
		releaseRxBuffer(rxBuffer);
		return NULL;
	} else {
		if (context->sequenceNumber != sequenceNumber ||
		    memcmp(frame->address2,
			   ((WlanWirelessFrame *)((uint8_t *)context->buffer +
						  context->buffer->dataOffset))->address2, 6) != 0) {
			/* another MSDU or another transmitter */
			osa_BufferFree(*reassembly);
			releaseRxBuffer(rxBuffer);
			wlanDefragReset(context);
			*discarding = 1;
			*reassembly = NULL;
			return NULL;
		}

		assembled = (WlanWirelessFrame *)((uint8_t *)context->buffer +
						  context->buffer->dataOffset);
		if (context->lastFragmentNumber + 1u != fragmentNumber) {
			/* a fragment is missing */
			osa_BufferFree(*reassembly);
			releaseRxBuffer(rxBuffer);
			wlanDefragReset(context);
			*discarding = 1;
			*reassembly = NULL;
			return NULL;
		}

		setFrameBodyLength(assembled, frameBodyLength(assembled) + extra);
		headroomCopied = wlanOffloadHeadroom;
	}

	/* the first fragment also brings the offload headroom */
	copyLength = wlanOffloadHeadroom - headroomCopied + extra;
	if (copyLength) {
		wlanRomServices.submitBbpCommand((uint32_t)context->writePtr,
						 (uint32_t)((uint8_t *)frame + headroomCopied +
							    WLAN_DEFRAG_HEADER_SIZE),
						 copyLength);
		context->writePtr += copyLength;
	}

	if (frame->frameControl[1] & WLAN_FC1_MORE_FRAGMENTS) {
		context->sequenceNumber = sequenceNumber;
		context->lastFragmentNumber = fragmentNumber;
		releaseRxBuffer(rxBuffer);
		return NULL;
	}

	/* the last fragment */
	if (extra == 0)
		setFrameBodyLength(assembled, frameBodyLength(assembled) -
					      (WLAN_DEFRAG_HEADER_SIZE - rxBuffer->length));
	assembled->frameControl[1] &= ~WLAN_FC1_MORE_FRAGMENTS;
	(*reassembly)->length = frameBodyLength(assembled);
	releaseRxBuffer(rxBuffer);
	wlanDefragReset(context);
	result = *reassembly;
	*reassembly = NULL;
	return result;
}

/* the WEP IV and ICV and the receive descriptor around the body: bodyLength minus this is the payload */
#define WLAN_DEFRAG_WEP_OVERHEAD	104

/*
 * The same for WEP fragments, each decrypted into the reassembly buffer with
 * the seed (IV and key) of keyIndex; an ICV failure is counted, recorded for
 * the host and discards the MSDU.
 */
OsaBuffer *wlanDefragmentWep(OsaBuffer *rxBuffer, const uint8_t *seed, uint8_t isWep104,
			     uint8_t keyIndex, OsaBuffer **reassembly,
			     WlanDefragContext *context, WepIcvErrList *icvErrList,
			     HostCmd_DS_802_11_GET_LOG *statistics)
{
	OsaBuffer *result;
	struct WlanConnection *connection;
	WlanWirelessFrame *frame, *assembled;
	uint32_t sequenceNumber, fragmentNumber, payloadLength;
	uint8_t *icvErrors;

	if (wlanDefragmentWepOverride(rxBuffer, seed, isWep104, keyIndex, reassembly,
				      context, icvErrList, statistics, &result))
		return result;

	connection = rxBuffer->context;
	frame = (WlanWirelessFrame *)((uint8_t *)rxBuffer + rxBuffer->dataOffset);
	sequenceNumber = ((frame->sequenceControl[1] << 8) | frame->sequenceControl[0]) >> 4;
	fragmentNumber = frame->sequenceControl[0] & 0xf;
	payloadLength = (uint16_t)(frameBodyLength(frame) - WLAN_DEFRAG_WEP_OVERHEAD);

	if (fragmentNumber == 0) {
		if (*reassembly == NULL)
			allocReassemblyBuffer(reassembly, WLAN_DEFRAG_WEP_BUFFER_SIZE);

		wlanRomServices.resetBufferDataOffset(*reassembly);
		wlanDefragReset(context);
		context->buffer = *reassembly;
		assembled = (WlanWirelessFrame *)((uint8_t *)*reassembly + (*reassembly)->dataOffset);
		context->writePtr = (uint8_t *)assembled + wlanOffloadHeadroom +
				    WLAN_DEFRAG_HEADER_SIZE;
		wlanRomServices.submitBbpCommand((uint32_t)assembled, (uint32_t)frame,
						 WLAN_DEFRAG_HEADER_SIZE);
		setFrameBodyLength(assembled, payloadLength + 96);
		context->startTime = SOC_MAC_TIMER + wlanRomServices.timerOffset;
		(*reassembly)->context = rxBuffer->context;
	} else {
		if (context->buffer == NULL ||
		    (assembled = (WlanWirelessFrame *)((uint8_t *)context->buffer +
						       context->buffer->dataOffset)) == NULL) {
			releaseRxBuffer(rxBuffer);
			return NULL;
		}

		if (context->lastFragmentNumber + 1u != fragmentNumber) {
			releaseRxBuffer(rxBuffer);
			goto discard;
		}

		setFrameBodyLength(assembled, frameBodyLength(assembled) + payloadLength);
	}

	/* the body after the 4-byte IV, with its ICV */
	if (!wlanWepDecrypt(context->writePtr,
			    (uint8_t *)frame + wlanOffloadHeadroom + WLAN_DEFRAG_HEADER_SIZE + 4,
			    payloadLength + 4, seed, isWep104, assembled)) {
		icvErrors = (uint8_t *)&statistics->wep_icv_err_cnt[keyIndex];
		__aeabi_uwrite4(__aeabi_uread4(icvErrors) + 1, icvErrors);
		wlanWepIcvErrRecord(connection, assembled, keyIndex, isWep104, seed,
				    icvErrList);
		wlanRomServices.onWepIcvError(connection);
		releaseRxBuffer(rxBuffer);
		goto discard;
	}

	wlanWepIcvErrForget(assembled, icvErrList);
	wlanRomServices.onWepDecrypted(rxBuffer);

	if (frame->frameControl[1] & WLAN_FC1_MORE_FRAGMENTS) {
		context->sequenceNumber = sequenceNumber;
		context->writePtr += payloadLength;
		context->lastFragmentNumber = fragmentNumber;
		releaseRxBuffer(rxBuffer);
		return NULL;
	}

	(*reassembly)->length = frameBodyLength(assembled);
	releaseRxBuffer(rxBuffer);
	wlanDefragReset(context);
	result = *reassembly;
	*reassembly = NULL;
	return result;

discard:
	osa_BufferFree(*reassembly);
	wlanDefragReset(context);
	*reassembly = NULL;
	return NULL;
}
