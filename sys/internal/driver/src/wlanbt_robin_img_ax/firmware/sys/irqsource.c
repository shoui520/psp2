/*
 * The interrupt and event source registers and the BBP command copies (0x34f0..0x3588): the
 * enable, disable and acknowledge accessors the event code and the interrupt handlers call.
 * The filename is inferred, not attested.
 */
#include <stdint.h>
#include "robin/dma.h"
#include "robin/soc.h"
#include "robin/sys.h"

uint32_t sysReadInterruptStatus(void)
{
	return SOC_SOURCE_IRQ_STATUS;
}

uint32_t sysEnableInterruptSources(uint32_t sources)
{
	SOC_SOURCE_IRQ_ENABLE |= sources;
	SOC_SOURCE_IRQ_SECONDARY_ENABLE |= sources;
	return sources;
}

uint32_t sysDisableInterruptSources(uint32_t sources)
{
	SOC_SOURCE_IRQ_ENABLE &= ~sources;
	return sources;
}

uint32_t sysAcknowledgeInterruptSources(uint32_t sources)
{
	uint32_t acknowledge = ~sources;

	SOC_SOURCE_IRQ_STATUS = acknowledge;
	return acknowledge;
}

int sysEnableEventSources(uint32_t sources)
{
	SOC_EVENT_SOURCE_ENABLE |= sources;
	SOC_EVENT_SECONDARY_ENABLE |= sources;
	return 1;
}

int sysDisableEventSources(uint32_t sources)
{
	SOC_EVENT_SOURCE_ENABLE &= ~sources;
	SOC_EVENT_SECONDARY_ENABLE &= ~sources;
	return 1;
}

int sysAcknowledgeEvents(uint32_t events)
{
	SOC_EVENT_STATUS = ~events;
	return 1;
}

volatile uint32_t *sysWaitForBbpCommand(void)
{
	do {
		while (BBP_EVENT_B & (1u << 14))
			;
	} while (BBP_SECONDARY_COMMAND);
	return &BBP_EVENT_A;
}

WlanDmaCompletionHook socEventHookInit(void)
{
	wlanDmaCompletionHook = sysWaitForBbpCommand;
	return sysWaitForBbpCommand;
}

/* A copy on DMA engine B: to target, from value, command bytes. */
void sysIssueBbpCommand(uint32_t target, uint32_t value, uint32_t command)
{
	wlanDmaStartCopyB((uint8_t *)target, (const uint8_t *)value, command);
}

/* A copy on DMA engine A. */
void sysIssuePrimaryBbpCommand(uint32_t target, uint32_t value, uint32_t command)
{
	wlanDmaMoveForward((uint8_t *)target, (const uint8_t *)value, command);
}
