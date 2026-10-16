/*
 * PU0: the clocks of the decoder's bitstream unit, on while it works on a NAL unit,
 * and of the demultiplexer's start code scanner, on while task 1 parses. The two
 * share VIP_CLOCK_SHARED: whichever stops last turns it off.
 */
#include "decoder.h"

void enableDecoderClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		(vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff) | VIP_CLOCK_SHARED);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST,
		(vipReadControlBus(VIP_CB_CLOCK_REQUEST) & 0xffff) | VIP_CLOCK_REQUEST_DECODER);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/*
 * Stop the clocks once the decoder is idle, after the DMA and the unit's last
 * command (the reads), unless the scanner still runs.
 */
void gateDecoderClocks(void)
{
	uint32_t psw;

	if (g_decoderState != VIP_DECODER_IDLE && g_decoderState != VIP_DECODER_FAILED)
		return;
	psw = vipReadControl(VIP_CR_PSW);
	mep_di();
	(void)vipReadControlBus(VIP_CB_DMA_BUSY);
	(void)vipReadControlBus(VIP_CB_BITSTREAM_COMMAND);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST,
		vipReadControlBus(VIP_CB_CLOCK_REQUEST) & 0xffff & ~VIP_CLOCK_REQUEST_DECODER);
	if (!(vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff & VIP_CLOCK_DEMUX))
		vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
			vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff & ~VIP_CLOCK_SHARED);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

void enableDemuxClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		(vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff) | VIP_CLOCK_SHARED);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		(vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff) | VIP_CLOCK_DEMUX);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/* Stop the clocks once the DMA and the scanner are idle. */
void gateDemuxClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	if (!((vipReadControlBus(VIP_CB_DMA_BUSY) & 0xffff) |
	      (vipReadControlBus(VIP_CB_SCANNER_START) & 0xffff))) {
		vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
			vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff & ~VIP_CLOCK_DEMUX);
		if (!(vipReadControlBus(VIP_CB_CLOCK_REQUEST) & VIP_CLOCK_REQUEST_DECODER))
			vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
				vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff &
				~VIP_CLOCK_SHARED);
	}
	if (psw & VIP_PSW_IEC)
		mep_ei();
}
