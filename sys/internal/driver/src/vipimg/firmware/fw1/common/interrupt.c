/*
 * fw1 runtime: interrupt set-up. An interrupt enters at its vector in local RAM,
 * VIP_INTERRUPT_VECTORS + 4 * its number, which holds a jmp to its handler; each
 * core keeps the jmp instructions in its program (vectors.S) and copies them in.
 */
#include "runtime.h"

#define VIP_INTERRUPT_VECTORS	(VIP_LOCAL_RAM + 0x30)

#if defined(VIP_PROGRAM_fw1_pu0)
#define VIP_INTERRUPT_LEVELS1	0x3333
#else
#define VIP_INTERRUPT_LEVELS1	0x33333
#endif

/* Install the vectors of the DMA-complete and mailbox interrupts. */
void installInterruptVectors(void)
{
	uint32_t *vectors = (uint32_t *)VIP_INTERRUPT_VECTORS;

	vectors[VIP_INTERRUPT_DMA] = vipDmaVector;
	vectors[VIP_INTERRUPT_MAILBOX] = vipMailboxVector;
}

void installVectors(void)
{
	installInterruptVectors();
}

/* Install the vectors of the core's other interrupts. */
void copyVectorCode(void)
{
	uint32_t i;

	for (i = 0; i < g_vectorCopy.wordCount; i++)
		g_vectorCopy.destination[i] = g_vectorCopy.source[i];
}

void configureInterrupts(void)
{
	vipReadControlBus(VIP_CB_INTERRUPT_PENDING);
	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING, 0xffffffff);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0,
		vipReadControlBus(VIP_CB_INTERRUPT_LEVEL0) | 0x33333033);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1,
		vipReadControlBus(VIP_CB_INTERRUPT_LEVEL1) | VIP_INTERRUPT_LEVELS1);
}

/* Enable the DMA-complete and mailbox interrupts. */
void enableInterrupts(void)
{
	uint32_t enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE) |
		1u << VIP_INTERRUPT_DMA | 1u << VIP_INTERRUPT_MAILBOX;

	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable);
	mep_ei();
}

/* Start the DMA engine, and enable its interrupt and the mailbox's. */
void enableDmaInterrupt(void)
{
	uint32_t enable, psw;

	vipWriteControlBus(VIP_CB_DMA_CONTROL, 1);
	enable = vipReadControlBus(VIP_CB_INTERRUPT_ENABLE) |
		1u << VIP_INTERRUPT_DMA | 1u << VIP_INTERRUPT_MAILBOX;
	psw = vipReadControl(VIP_CR_PSW);
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, enable);
	vipWriteControl(VIP_CR_PSW, psw | VIP_PSW_IEP | VIP_PSW_UMC | VIP_PSW_IEC);
	mep_ei();
}
