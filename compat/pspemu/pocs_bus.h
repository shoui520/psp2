/*
 * pocs_bus.h: PocketStation ARM7TDMI memory bus, flash page write state
 * machine, and MMIO dispatch helpers shared by pocs_periph.c and pocs_cpu.c.
 */
#ifndef PSPEMU_POCS_BUS_H
#define PSPEMU_POCS_BUS_H

#include "pocs_internal.h"
#include <string.h>

#define POCS_LCD_VRAM_BASE          0x0D000100U
#define POCS_LCD_VRAM_SPAN          0x7FU
#define POCS_FLASH_CMD_ADDR_A       0x080055AAU
#define POCS_FLASH_CMD_ADDR_B       0x08002A54U
#define POCS_FLASH_CMD_VAL_UNLOCK_A 0xFFAAU
#define POCS_FLASH_CMD_VAL_UNLOCK_B 0xFF55U
#define POCS_FLASH_CMD_VAL_WRITE    0xFFA0U
#define POCS_FLASH_PAGE_DELAY       799

typedef struct __attribute__((packed)) PocsUnalignedU32 {
	uint32_t value;
} PocsUnalignedU32;

typedef struct __attribute__((packed)) PocsUnalignedU16 {
	uint16_t value;
} PocsUnalignedU16;

static inline __attribute__((always_inline)) uint32_t pocsLoadLe32(const uint8_t *buf, uint32_t byteOff)
{
	return ((const PocsUnalignedU32 *)(const void *)&buf[byteOff])->value;
}

static inline __attribute__((always_inline)) uint16_t pocsLoadLe16(const uint8_t *buf, uint32_t byteOff)
{
	return ((const PocsUnalignedU16 *)(const void *)&buf[byteOff])->value;
}

static inline __attribute__((always_inline)) void pocsStoreLe32(uint8_t *buf, uint32_t byteOff, uint32_t val)
{
	((PocsUnalignedU32 *)(void *)&buf[byteOff])->value = val;
}

static inline __attribute__((always_inline)) void pocsStoreLe16(uint8_t *buf, uint32_t byteOff, uint16_t val)
{
	((PocsUnalignedU16 *)(void *)&buf[byteOff])->value = val;
}

static inline __attribute__((always_inline)) uint32_t pocsRotateRight32(uint32_t value, unsigned int shift)
{
	shift &= 31U;
	if (shift == 0U)
		return value;
	return (value >> shift) | (value << (32U - shift));
}

static inline __attribute__((always_inline)) uint32_t pocsRemapFlashAddr(uint32_t addr, int isFetch)
{
	int8_t physBank = g_pocsState.flashPhysMap[(addr >> 13) & 0xFU];
	if (physBank < 0)
		pocsSysFatalAccess((int)physBank, isFetch);
	return (addr & 0xFFFE1FFFU) | (((uint32_t)(uint8_t)physBank & 0xFU) << 13);
}

static inline __attribute__((always_inline)) int32_t pocsRomWaitDelta32(uint8_t waitReg)
{
	return 1 - (2 << ((waitReg & 0x10U) != 0U));
}

static inline __attribute__((always_inline)) int32_t pocsRomWaitDelta16(uint8_t waitReg)
{
	return ((waitReg & 0x10U) != 0U) ? -1 : 0;
}

static inline __attribute__((always_inline)) uint32_t pocsMmioRead(uint32_t addr, int accessWidth)
{
	uint32_t regOff = addr & 0x7FFFFFU;
	uint32_t devIdx = (addr >> 23) & 0x1FU;

	g_pocsState.cpuCyclesLeft--;

	switch (devIdx) {
	case 0x0CU:
		return pocsSysRegRead(regOff, accessWidth);
	case 0x14U:
		return (uint32_t)pocsIntcRead((int)regOff);
	case 0x15U:
		return pocsTimerRead(regOff);
	case 0x16U:
		return (uint32_t)pocsClockRead((int)regOff);
	case 0x17U:
		return (uint32_t)pocsRtcRead((int)regOff);
	case 0x18U:
		return (uint32_t)pocsSpiRead((int)regOff);
	case 0x19U:
		return (uint32_t)pocsIrdaRead((int)regOff);
	case 0x1AU:
		return (uint32_t)pocsLcdRead((int)regOff);
	case 0x1BU:
		switch (regOff >> 4) {
		case 0U:
			return (uint32_t)pocsGpioRead((int)regOff);
		case 1U:
			return (uint32_t)pocsAudioRead((int)regOff);
		case 2U:
			return (uint32_t)pocsBatteryRead((int)regOff);
		default:
			return 0U;
		}
	default:
		return 0U;
	}
}

static inline __attribute__((always_inline)) void pocsMmioWrite(uint32_t addr, uint32_t value)
{
	uint32_t regOff = addr & 0x7FFFFFU;
	uint32_t devIdx = (addr >> 23) & 0x1FU;

	g_pocsState.cpuCyclesLeft--;

	switch (devIdx) {
	case 0x0CU:
		pocsSysRegWrite(regOff, (uint16_t)value);
		break;
	case 0x14U:
		pocsIntcWrite((int)regOff, (uint16_t)value);
		break;
	case 0x15U:
		pocsTimerWrite(regOff, (uint16_t)value);
		break;
	case 0x16U:
		pocsClockWrite((int)regOff, (uint8_t)value);
		break;
	case 0x17U:
		pocsRtcWrite((int)regOff, (uint8_t)value);
		break;
	case 0x18U:
		pocsSpiWrite((int)regOff, (int)value);
		break;
	case 0x19U:
		pocsIrdaWrite((int)regOff, (int)value);
		break;
	case 0x1AU:
		pocsLcdWrite((int)regOff, (int)value);
		break;
	case 0x1BU:
		switch (regOff >> 4) {
		case 0U:
			pocsGpioWrite((int)regOff, (uint8_t)value);
			break;
		case 1U:
			pocsAudioWrite((int)regOff, (int)value);
			break;
		case 2U:
			pocsBatteryWrite((int)regOff, (uint8_t)value);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static inline __attribute__((always_inline)) int pocsFlashPreparePageWrite(uint32_t effAddr)
{
	int16_t ctrl = (int16_t)g_pocsState.sysCtrl;
	uint32_t pageIdx = (effAddr >> 7) & 0x3FFU;

	if (ctrl < 0) {
		if (ctrl != -1)
			return 0;
		g_pocsState.sysCtrl = (uint16_t)pageIdx;
		memcpy(g_pocsState.flashPageBuf, &g_pocsState.flashDataPtr[pageIdx << 7],
		       sizeof(g_pocsState.flashPageBuf));
	} else if (pageIdx != (uint32_t)(uint16_t)ctrl) {
		return 0;
	}

	if (g_pocsState.flashEvent.prev != NULL)
		pocsSchedRemoveEvent(&g_pocsState.flashEvent);
	pocsSchedAddEvent(&g_pocsState.flashEvent, POCS_FLASH_PAGE_DELAY);
	return 1;
}

static inline __attribute__((always_inline)) int pocsFlashHandleUnlockHalfword(uint32_t addr, uint32_t value)
{
	int16_t ctrl = (int16_t)g_pocsState.sysCtrl;

	if (ctrl >= 0)
		return 0;

	switch (ctrl) {
	case -4:
		if (addr == POCS_FLASH_CMD_ADDR_A && value == POCS_FLASH_CMD_VAL_UNLOCK_A) {
			g_pocsState.sysCtrl = (uint16_t)-3;
			return 1;
		}
		break;
	case -3:
		if (addr == POCS_FLASH_CMD_ADDR_B && value == POCS_FLASH_CMD_VAL_UNLOCK_B) {
			g_pocsState.sysCtrl = (uint16_t)-2;
			return 1;
		}
		break;
	case -2:
		if (addr == POCS_FLASH_CMD_ADDR_A && value == POCS_FLASH_CMD_VAL_WRITE) {
			g_pocsState.sysCtrl = (uint16_t)-1;
			return 1;
		}
		break;
	default:
		break;
	}
	return 0;
}

static inline __attribute__((always_inline)) uint32_t pocsBusRead32(uint32_t addr)
{
	uint32_t region = (addr >> 25) & 7U;

	if (region == 0U)
		return pocsLoadLe32(g_pocsState.internalRam, addr & 0x7FCU);
	if (region == 2U) {
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta32(g_pocsState.lcdRefreshRate);
		return pocsLoadLe32(g_pocsBiosRom, addr & 0x3FFCU);
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(addr, 0) : addr;
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta32(g_pocsState.lcdContrast);
		return pocsLoadLe32(g_pocsState.flashDataPtr, effAddr & 0x1FFFCU);
	}
	if (addr - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		g_pocsState.cpuCyclesLeft--;
		return g_pocsState.lcdVramFront[(addr - POCS_LCD_VRAM_BASE) >> 2];
	}
	return pocsMmioRead(addr, 4);
}

static inline __attribute__((always_inline)) void pocsBusWrite32(uint32_t addr, uint32_t value)
{
	uint32_t region = (addr >> 25) & 7U;

	if (region == 0U) {
		pocsStoreLe32(g_pocsState.internalRam, addr & 0x7FCU, value);
		return;
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(addr, 0) : addr;
		if (pocsFlashPreparePageWrite(effAddr)) {
			pocsStoreLe32(g_pocsState.flashPageBuf, effAddr & 0x7CU, value);
			return;
		}
		pocsMmioWrite(addr, value);
		return;
	}
	if (addr - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		g_pocsState.cpuCyclesLeft--;
		g_pocsState.lcdVramFront[(addr - POCS_LCD_VRAM_BASE) >> 2] = value;
		return;
	}
	pocsMmioWrite(addr, value);
}

static inline __attribute__((always_inline)) uint32_t pocsBusRead8(uint32_t addr)
{
	uint32_t region = (addr >> 25) & 7U;

	if (region == 0U)
		return g_pocsState.internalRam[addr & 0x7FFU];
	if (region == 2U) {
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta16(g_pocsState.lcdRefreshRate);
		return g_pocsBiosRom[addr & 0x3FFFU];
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(addr, 0) : addr;
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta16(g_pocsState.lcdContrast);
		return g_pocsState.flashDataPtr[effAddr & 0x1FFFFU];
	}
	if (addr - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		uint32_t off = addr - POCS_LCD_VRAM_BASE;
		g_pocsState.cpuCyclesLeft--;
		return (g_pocsState.lcdVramFront[off >> 2] >> (8U * (off & 3U))) & 0xFFU;
	}
	return pocsMmioRead(addr, 1);
}

static inline __attribute__((always_inline)) void pocsBusWrite8(uint32_t addr, uint32_t value)
{
	uint32_t region = (addr >> 25) & 7U;

	if (region == 0U) {
		g_pocsState.internalRam[addr & 0x7FFU] = (uint8_t)value;
		return;
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(addr, 0) : addr;
		if (pocsFlashPreparePageWrite(effAddr)) {
			g_pocsState.flashPageBuf[effAddr & 0x7FU] = (uint8_t)value;
			return;
		}
		pocsMmioWrite(addr, value);
		return;
	}
	if (addr - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		uint32_t off = addr - POCS_LCD_VRAM_BASE;
		uint32_t shift = 8U * (off & 3U);
		g_pocsState.cpuCyclesLeft--;
		g_pocsState.lcdVramFront[off >> 2] =
			(g_pocsState.lcdVramFront[off >> 2] & ~(0xFFU << shift)) | ((value & 0xFFU) << shift);
		return;
	}
	pocsMmioWrite(addr, value);
}

static inline __attribute__((always_inline)) uint32_t pocsBusRead16(uint32_t addr)
{
	uint32_t region = (addr >> 25) & 7U;
	int isOdd = (int)(addr & 1U);

	if (region == 0U) {
		if (isOdd)
			return g_pocsState.internalRam[addr & 0x7FFU];
		return pocsLoadLe16(g_pocsState.internalRam, addr & 0x7FEU);
	}
	if (region == 2U) {
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta16(g_pocsState.lcdRefreshRate);
		if (isOdd)
			return g_pocsBiosRom[addr & 0x3FFFU];
		return pocsLoadLe16(g_pocsBiosRom, addr & 0x3FFEU);
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(addr, 0) : addr;
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta16(g_pocsState.lcdContrast);
		if (isOdd)
			return g_pocsState.flashDataPtr[effAddr & 0x1FFFFU];
		return pocsLoadLe16(g_pocsState.flashDataPtr, effAddr & 0x1FFFEU);
	}
	if (addr - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		uint32_t off = addr - POCS_LCD_VRAM_BASE;
		g_pocsState.cpuCyclesLeft--;
		if (isOdd)
			return (g_pocsState.lcdVramFront[off >> 2] >> (8U * (off & 3U))) & 0xFFU;
		return (g_pocsState.lcdVramFront[off >> 2] >> (8U * (off & 2U))) & 0xFFFFU;
	}
	return pocsMmioRead(addr, isOdd ? 1 : 2);
}

static inline __attribute__((always_inline)) void pocsBusWrite16(uint32_t addr, uint32_t value)
{
	uint32_t region = (addr >> 25) & 7U;

	if (region == 0U) {
		pocsStoreLe16(g_pocsState.internalRam, addr & 0x7FEU, (uint16_t)value);
		return;
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(addr, 0) : addr;
		if (pocsFlashPreparePageWrite(effAddr)) {
			pocsStoreLe16(g_pocsState.flashPageBuf, effAddr & 0x7EU, (uint16_t)value);
			return;
		}
		if (pocsFlashHandleUnlockHalfword(addr, value))
			return;
		pocsMmioWrite(addr, value);
		return;
	}
	if (addr - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		uint32_t off = (addr - POCS_LCD_VRAM_BASE) & ~1U;
		uint32_t shift = 8U * (off & 2U);
		g_pocsState.cpuCyclesLeft--;
		g_pocsState.lcdVramFront[off >> 2] =
			(g_pocsState.lcdVramFront[off >> 2] & ~(0xFFFFU << shift)) | ((value & 0xFFFFU) << shift);
		return;
	}
	if (pocsFlashHandleUnlockHalfword(addr, value))
		return;
	pocsMmioWrite(addr, value);
}

static inline __attribute__((always_inline)) uint32_t pocsBusReadSigned16(uint32_t addr)
{
	uint32_t raw = pocsBusRead16(addr);
	if ((addr & 1U) != 0U)
		return (uint32_t)(int32_t)(uint8_t)raw;
	return (uint32_t)(int32_t)(int16_t)raw;
}

static inline __attribute__((always_inline)) uint32_t pocsBusReadSigned8(uint32_t addr)
{
	return (uint32_t)(int32_t)(int8_t)(uint8_t)pocsBusRead8(addr);
}

#endif /* PSPEMU_POCS_BUS_H */
