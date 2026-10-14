/*
 * PU1: the hardware state the debugger's commands stop, save and restore
 * (common/debug.c).
 */
#include "codec.h"
#include "../common/debug.h"

#define RESET_POLLS		0x8000000

/* the local RAM data vipSaveHardware keeps (fw1-pu1.ld checks the section fits) */
#define LOCAL_DATA_WORDS	0x50

/* what vipSaveHardware keeps for vipRestoreHardware */
typedef struct VipHardwareState {
	uint32_t predictionSetup;
	uint32_t interruptStatus;
	uint32_t interruptEnable;
	uint32_t interruptPending;
	uint32_t interruptLevel[4];
	uint32_t dmaControl;
	uint32_t pauseFlags;
	uint32_t localData[LOCAL_DATA_WORDS];
} VipHardwareState;

static VipHardwareState g_savedHardware;

/* Whether the DMA or a picture unit still runs. */
uint32_t vipHardwareBusy(uint32_t state)
{
	uint32_t busy = 0;

	(void)state;
	busy |= vipReadControlBus(VIP_CB_DMA_BUSY);
	busy |= vipReadControlBus(VIP_CB_PREDICTION_COMMAND);
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(0));
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(1));
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(2));
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(3));
	busy |= vipReadControlBus(VIP_CB_UNIT_COMMAND(4));
	return busy;
}

/* Keep the interrupt controller and the local RAM data for the debugger. */
void vipSaveHardware(void)
{
	const uint32_t *data = (const uint32_t *)__lram_data_start;
	uint32_t i;

	g_savedHardware.interruptEnable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 0);
	g_savedHardware.interruptStatus = vipReadControlBus(VIP_CB_INTERRUPT_STATUS);
	g_savedHardware.interruptPending = vipReadControlBus(VIP_CB_INTERRUPT_PENDING);
	g_savedHardware.interruptLevel[0] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL0);
	g_savedHardware.interruptLevel[1] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1);
	g_savedHardware.interruptLevel[2] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL2);
	g_savedHardware.interruptLevel[3] = vipReadControlBus(VIP_CB_INTERRUPT_LEVEL3);
	g_savedHardware.dmaControl = vipReadControlBus(VIP_CB_DMA_CONTROL);
	g_savedHardware.predictionSetup = vipReadControlBus(VIP_CB_PREDICTION_SETUP);
	for (i = 0; i < (uint32_t)__lram_data_size / 4; i++)
		g_savedHardware.localData[i] = data[i];
	g_savedHardware.pauseFlags = g_pauseFlags;
}

void vipRestoreHardware(void)
{
	uint32_t *data = (uint32_t *)__lram_data_start;
	uint32_t *unitRam = (uint32_t *)VIP_UNIT_RAM;
	uint32_t i;

	for (i = 0; i < VIP_UNIT_RAM_WORDS; i++)
		unitRam[i] = 0xff;
	for (i = 0; i < (uint32_t)__lram_data_size / 4; i++)
		data[i] = g_savedHardware.localData[i];
	g_pauseFlags = g_savedHardware.pauseFlags;
	vipWriteControlBus(VIP_CB_PREDICTION_SETUP, g_savedHardware.predictionSetup);
	vipWriteControlBus(VIP_CB_DMA_CONTROL, g_savedHardware.dmaControl);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 0);
	vipWriteControlBus(VIP_CB_INTERRUPT_STATUS, g_savedHardware.interruptStatus);
	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING, g_savedHardware.interruptPending);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0, g_savedHardware.interruptLevel[0]);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1, g_savedHardware.interruptLevel[1]);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL2, g_savedHardware.interruptLevel[2]);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL3, g_savedHardware.interruptLevel[3]);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, g_savedHardware.interruptEnable);
}

/* Stop every unit; how much of the wait for them was left (0: they did not stop). */
uint32_t vipResetHardware(void)
{
	uint32_t remaining = RESET_POLLS, busy;

	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 0);
	vipWriteControlBus(VIP_CB_DMA_CONTROL, 0);
	vipWriteControlBus(VIP_CB_DMA_BUSY, 0);
	vipWriteControlBus(VIP_CB_PREDICTION_CONTROL, 0);
	vipWriteControlBus(VIP_CB_PREDICTION_COMMAND, 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(1), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(3), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(4), 0);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(5), 0);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 0);
	do {
		busy = vipReadControlBus(VIP_CB_DMA_BUSY);
		busy |= vipReadControlBus(VIP_CB_PREDICTION_COMMAND);
		remaining--;
	} while (busy && remaining);
	vipWriteControlBus(VIP_CB_PREDICTION_ENABLE, 0);
	return remaining;
}

/* Route an interrupt to level 1, and enable it. */
void setupInterrupt(uint32_t interrupt)
{
	uint32_t bit = 1u << interrupt;
	uint32_t shift = (interrupt & 7) * 4;
	uint32_t mask = ~(0xfu << shift), level = 1u << shift, pending;

	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING,
		vipReadControlBus(VIP_CB_INTERRUPT_PENDING) | bit);
	switch ((int32_t)interrupt >> 3) {
	case 0:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL0) & mask) | level);
		break;
	case 1:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1) & mask) | level);
		break;
	case 2:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL2,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL2) & mask) | level);
		break;
	default:
		vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL3,
			(vipReadControlBus(VIP_CB_INTERRUPT_LEVEL3) & mask) | level);
		break;
	}
	pending = vipReadControlBus(VIP_CB_INTERRUPT_CLEAR) & ~bit;
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, pending);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE,
		vipReadControlBus(VIP_CB_INTERRUPT_ENABLE) | bit);
	mep_ei();
}
