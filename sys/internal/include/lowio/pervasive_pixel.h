/*
 * The state SceLowio keeps for each DSI pixel clock (ScePervasiveBaseClk
 * dsi0PixelClock, dsi1PixelClock); SceDriverTzs and the boot loaders keep
 * the same.
 */
#pragma once

#include <stdint.h>
#include <lowio/pervasive_regs.h>

struct PixelClockSetting;

typedef struct PixelClockContext {
	volatile ScePervasivePixelClockRegisters *registers;
	const struct PixelClockSetting *setting;	/* the last one set */
	uint32_t pllConfig;
	uint32_t clock;
	double frequency;
	double pixelFrequency;
} PixelClockContext;

_Static_assert(sizeof(PixelClockContext) == 0x20, "pixel clock context");
