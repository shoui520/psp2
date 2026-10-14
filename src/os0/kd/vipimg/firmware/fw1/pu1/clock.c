/*
 * PU1: the clocks of the picture units, on while a picture is decoded, and the
 * core's, which the debugger stops.
 */
#include "codec.h"

void enableDecoderClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);

	mep_di();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		(vipReadControlBus(VIP_CB_CLOCK_ENABLE) & 0xffff) | VIP_CLOCK_PICTURE_UNITS);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST,
		(vipReadControlBus(VIP_CB_CLOCK_REQUEST) & 0xffff) | VIP_CLOCK_REQUEST_UNITS);
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/* Stop the clocks once the DMA and every picture unit are idle. */
void gateDecoderClocks(void)
{
	uint32_t psw = vipReadControl(VIP_CR_PSW);
	uint32_t busy;

	mep_di();
	busy = vipReadControlBus(VIP_CB_DMA_BUSY) & 0xffff;
	busy |= vipReadControlBus(VIP_CB_PREDICTION_COMMAND) & 0xffff;
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(0)) & 0xffff;
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(1)) & 0xffff;
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(2)) & 0xffff;
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(3)) & 0xffff;
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(4)) & 0xffff;
	if (!busy) {
		vipWriteControlBus(VIP_CB_CLOCK_REQUEST, vipReadControlBus(VIP_CB_CLOCK_REQUEST) &
			0xffff & ~VIP_CLOCK_REQUEST_UNITS);
		vipWriteControlBus(VIP_CB_CLOCK_ENABLE, vipReadControlBus(VIP_CB_CLOCK_ENABLE) &
			0xffff & ~VIP_CLOCK_PICTURE_UNITS);
	}
	if (psw & VIP_PSW_IEC)
		mep_ei();
}

/* The debugger lets the core run again (common/debug.c). */
void vipWakeHardware(void)
{
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
		vipReadControlBus(VIP_CB_CLOCK_ENABLE) | VIP_CLOCK_CORE);
	enableDecoderClocks();
}

/* Stop the clocks; the core's too while the debugger resets, saves or drains it. */
void vipIdleHardware(void)
{
	gateDecoderClocks();
	if (g_status->command == VIP_DEBUG_RESET || g_status->command == VIP_DEBUG_SAVE ||
	    g_status->command == VIP_DEBUG_DRAIN)
		vipWriteControlBus(VIP_CB_CLOCK_ENABLE,
			vipReadControlBus(VIP_CB_CLOCK_ENABLE) & ~VIP_CLOCK_CORE);
}
