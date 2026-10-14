/* Event-register access family; the filename is inferred, not attested. */
#include "robin/sys.h"
#include "robin/soc.h"

/* Original ARM busy-wait helper supplied by boot/reset.S. */
extern int robinDelayLoop(int iterations);

volatile void *sysEventRegisterBase __attribute__((section(".itcm.rw"))) =
	(volatile void *)0x80002400;

void sysInitializeEventRegisters(void)
{
	SOC_EVENT_CONFIGURATION |= 0x10000;
	SOC_EVENT_CONFIGURATION = (SOC_EVENT_CONFIGURATION | 0x11f) & ~0x2000u;
	SOC_EVENT_SECONDARY_CONTROL |= 0x80;
	SOC_EVENT_TERTIARY_CONTROL |= 0x80;
	SOC_EVENT_GLOBAL_CONTROL |= 0x200;
	SOC_EVENT_GLOBAL_CONTROL |= 0x1000;
}

uint32_t sysReadEventConfigurationStatus(void)
{
	return SOC_EVENT_CONFIGURATION_STATUS & 0xffff;
}

void sysSetEventModeFlag(void)
{
	SOC_EVENT_GLOBAL_CONTROL |= 1u << 20;
}

void sysClearEventModeFlag(void)
{
	SOC_EVENT_GLOBAL_CONTROL &= ~(1u << 20);
}

uint32_t sysEventAlternateModeIsSet(void)
{
	return (SOC_EVENT_HANDSHAKE_STATUS >> 8) & 1;
}

void sysClearEventSleepRequest(void)
{
	SOC_EVENT_GLOBAL_CONTROL &= ~(1u << 17);
}

void sysSetEventSleepRequest(void)
{
	SOC_EVENT_GLOBAL_CONTROL |= 1u << 17;
}

void sysWaitForEventSleepHandshake(void)
{
	if (SOC_EVENT_HANDSHAKE_STATUS & (1u << 31)) {
		while (SOC_EVENT_HANDSHAKE_STATUS & 0xf0)
			;
	} else {
		/* The original also reads status again on the non-waiting path. */
		(void)SOC_EVENT_HANDSHAKE_STATUS;
	}
}

void sysWaitForEventTransactions(void)
{
	while (SOC_EVENT_HANDSHAKE_STATUS & 0xf0)
		;
}

void sysSetEventPrimaryTiming(uint32_t lower, uint32_t upper)
{
	SOC_EVENT_PRIMARY_TIMING = (upper << 16) | lower;
}

void sysSetEventPrimaryTimingLower(uint32_t lower)
{
	SOC_EVENT_PRIMARY_TIMING = (SOC_EVENT_PRIMARY_TIMING & 0xffff0000u) | lower;
}

void sysSetEventTimingControl(uint32_t control)
{
	SOC_EVENT_TIMING_CONTROL = (SOC_EVENT_TIMING_CONTROL & 0xffff0000u) | control;
}

void sysSetEventClockPeriod(uint32_t period)
{
	SOC_EVENT_CLOCK_PERIOD = (SOC_EVENT_CLOCK_PERIOD & 0xffff0000u) | period;
}

void sysSetEventSecondaryTiming(uint32_t lower, uint32_t upper)
{
	SOC_EVENT_SECONDARY_TIMING = (upper << 16) | lower;
}

void sysSetEventControlByte(uint32_t control)
{
	SOC_EVENT_GLOBAL_CONTROL = (SOC_EVENT_GLOBAL_CONTROL & 0xffffff00u) | control;
}

void sysWaitForEventMeasurement(void)
{
	while (!(SOC_EVENT_HANDSHAKE_STATUS & 1))
		;
	SOC_EVENT_GLOBAL_CONTROL &= ~(1u << 15);
}

uint32_t sysStartEventMeasurement(uint32_t extent)
{
	uint32_t control;

	(void)SOC_EVENT_MEASUREMENT_REQUEST;
	SOC_EVENT_MEASUREMENT_REQUEST = extent;
	control = SOC_EVENT_GLOBAL_CONTROL | (1u << 15);
	SOC_EVENT_GLOBAL_CONTROL = control;
	return control;
}

uint32_t sysReadEventMeasurementSample(void)
{
	return SOC_EVENT_MEASUREMENT_SAMPLE;
}

uint32_t sysReadEventMeasurementUnits(void)
{
	return SOC_EVENT_MEASUREMENT_UNITS;
}

uint32_t sysReadEventClock(void)
{
	return SOC_EVENT_CLOCK;
}

uint32_t sysClearEventFlags(uint32_t flags)
{
	SOC_EVENT_FLAGS &= ~flags;
	return flags;
}

uint32_t sysSetEventFlags(uint32_t flags)
{
	SOC_EVENT_FLAGS |= flags;
	return flags;
}

uint32_t sysSetSecondaryEventFlags(uint32_t flags)
{
	SOC_EVENT_SECONDARY_FLAGS |= flags;
	return flags;
}

uint32_t sysClearSecondaryEventFlags(uint32_t flags)
{
	SOC_EVENT_SECONDARY_FLAGS &= ~flags;
	return flags;
}

uint32_t sysEventPrimaryHandshakeComplete(void)
{
	return (SOC_EVENT_HANDSHAKE_STATUS >> 1) & 1;
}

uint32_t sysEventSecondaryHandshakeComplete(void)
{
	return (SOC_EVENT_HANDSHAKE_STATUS >> 3) & 1;
}

void sysConfigureSecondaryEventEngine(void)
{
	SOC_EVENT_SECONDARY_CONTROL |= 0x40;
}

void sysResetSecondaryEventConfiguration(void)
{
	SOC_EVENT_SECONDARY_CONFIGURATION &= ~0x37u;
}

void sysDisableSecondaryEventMode(void)
{
	SOC_EVENT_SECONDARY_CONFIGURATION &= ~8u;
}

void sysEnableSecondaryEventMode(void)
{
	SOC_EVENT_SECONDARY_CONFIGURATION |= 8;
}

void sysStartSecondaryEventEngine(void)
{
	SOC_EVENT_SECONDARY_CONTROL |= 1;
}

void sysStopSecondaryEventEngine(void)
{
	SOC_EVENT_SECONDARY_CONTROL &= ~1u;
}

void sysConfigureTertiaryEventPath(void)
{
	SOC_EVENT_GLOBAL_CONTROL |= 1u << 18;
	SOC_EVENT_CONFIGURATION &= ~(1u << 14);
	SOC_EVENT_TERTIARY_CONFIGURATION &= ~1u;
}

void sysRestoreTertiaryEventPath(void)
{
	SOC_EVENT_TERTIARY_CONFIGURATION |= 1;
	SOC_EVENT_GLOBAL_CONTROL &= ~(1u << 18);
	SOC_EVENT_CONFIGURATION |= 1u << 14;
}

void sysDisableTertiaryEventMode(void)
{
	SOC_EVENT_TERTIARY_CONFIGURATION &= ~2u;
}

void sysEnableTertiaryEventMode(void)
{
	SOC_EVENT_TERTIARY_CONFIGURATION |= 2;
}

void sysStartTertiaryEventEngine(void)
{
	SOC_EVENT_TERTIARY_CONTROL |= 1;
}

void sysStopTertiaryEventEngine(void)
{
	SOC_EVENT_TERTIARY_CONTROL &= ~1u;
}

void sysDisableEventEngineMode(void)
{
	SOC_EVENT_ENGINE_CONFIGURATION &= ~1u;
}

void sysEnableEventEngineMode(void)
{
	SOC_EVENT_ENGINE_CONFIGURATION |= 1;
}

void sysStartEventEngine(void)
{
	SOC_EVENT_ENGINE_CONTROL |= 1;
	if (SOC_EVENT_ENGINE_CONTROL & (1u << 22)) {
		SOC_EVENT_ENGINE_CONTROL &= ~(1u << 22);
		robinDelayLoop(10);
	}
	SOC_EVENT_ENGINE_CONTROL |= 1u << 22;
}

void sysStopEventEngineAndWait(void)
{
	SOC_EVENT_ENGINE_CONTROL &= ~1u;
	while (SOC_EVENT_HANDSHAKE_STATUS & (1u << 28))
		;
}

int sysEventEngineIsActive(void)
{
	return SOC_EVENT_ENGINE_CONTROL & 1;
}

void sysClearEventEngineTrigger(void)
{
	SOC_EVENT_ENGINE_CONTROL &= ~(1u << 22);
}

void sysResetEventEngineConfiguration(void)
{
	SOC_EVENT_ENGINE_CONFIGURATION &= ~6u;
}

uint32_t sysConfigureEventEngineTiming(uint32_t primary, uint32_t secondary)
{
	uint32_t control;

	SOC_EVENT_ENGINE_SECONDARY_TIMING =
		(SOC_EVENT_ENGINE_SECONDARY_TIMING & 0xf0000000u) |
		(secondary & 0x0fffffffu);
	SOC_EVENT_ENGINE_PRIMARY_TIMING =
		(SOC_EVENT_ENGINE_PRIMARY_TIMING & 0xfff80000u) |
		(primary & 0x7ffff);
	SOC_EVENT_ENGINE_CONTROL =
		(SOC_EVENT_ENGINE_CONTROL & ~0x1f0000u) + 0x100000;
	control = SOC_EVENT_ENGINE_CONTROL | 0x200000;
	SOC_EVENT_ENGINE_CONTROL = control;
	return control;
}

uint32_t sysReadEventEngineCountdown(void)
{
	return SOC_EVENT_ENGINE_COUNTDOWN;
}

void sysProgramEventCountdown(uint32_t count, uint32_t unusedPrimary,
			      uint32_t unusedSecondary, uint32_t limit)
{
	uint32_t clock;

	SOC_EVENT_COUNTDOWN_CONTROL |= 1u << 16;
	(void)SOC_EVENT_ENGINE_COUNTDOWN_RELOAD;
	SOC_EVENT_ENGINE_COUNTDOWN_RELOAD = count;
	(void)SOC_EVENT_ENGINE_COUNTDOWN_LIMIT;
	SOC_EVENT_ENGINE_COUNTDOWN_LIMIT = limit;
	(void)SOC_EVENT_ENGINE_COUNTDOWN_SETUP;
	SOC_EVENT_ENGINE_COUNTDOWN_SETUP = 0xf0;
	SOC_EVENT_CONFIGURATION |= 0x10;
	SOC_EVENT_CONFIGURATION &= ~0x20u;
	do {
		clock = SOC_EVENT_CLOCK;
		while (SOC_EVENT_CLOCK == clock)
			;
	} while (sysReadEventEngineCountdown());
	SOC_EVENT_CONFIGURATION |= 0x20;
	SOC_EVENT_GLOBAL_CONTROL |= 0x2000;
}

uint32_t sysReadEventCountdownReload(void)
{
	return SOC_EVENT_ENGINE_COUNTDOWN_RELOAD;
}

void sysSetEventCountdownMode(void)
{
	SOC_EVENT_CONFIGURATION |= 0x20;
}

void sysClearEventCountdownMode(void)
{
	SOC_EVENT_CONFIGURATION &= ~0x20u;
}

uint32_t sysSelectEventEngineSource(uint32_t source)
{
	uint32_t control;

	SOC_EVENT_ENGINE_CONTROL =
		(SOC_EVENT_ENGINE_CONTROL & ~0x0f800000u) |
		((source & 0x1f) << 23);
	control = SOC_EVENT_ENGINE_CONTROL | 0x10000000;
	SOC_EVENT_ENGINE_CONTROL = control;
	return control;
}

uint32_t sysEventTransitionModeIsSet(void)
{
	return (SOC_EVENT_CONFIGURATION >> 19) & 1;
}

void sysSetEventTransitionMode(void)
{
	SOC_EVENT_CONFIGURATION |= 1u << 19;
}

void sysClearEventTransitionMode(void)
{
	SOC_EVENT_CONFIGURATION &= ~(1u << 19);
}

void sysTriggerEventEngineSource(void)
{
	SOC_EVENT_ENGINE_CONTROL |= 0x100;
}
