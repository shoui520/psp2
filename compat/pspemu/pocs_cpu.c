/*
 * pocs_cpu.c: ScePspemu PocketStation ARM7TDMI interpreter main cycle loop
 * (Thumb and ARM decode/execute, scheduler dispatch, and exception handling,
 * 0x81025e28..0x81028eb3).
 */
#include "pocs_bus.h"

#include <stdlib.h>

#define POCS_IRQ_WAKE_MASK          0x0E01U
#define POCS_IRQ_FIQ_MASK           0x2040U
#define POCS_IRQ_NORMAL_MASK        0xDFBFU
#define POCS_IRQ_DOCK_MASK          0x0800U
#define POCS_GPIO_DOCK_BIT          0x10U

#define POCS_ARM_PUSH_TEMPLATE      0xE92D0000U
#define POCS_ARM_POP_TEMPLATE       0xE8BD0000U
#define POCS_ARM_STMIA_TEMPLATE     0xE8A00000U

static inline __attribute__((always_inline)) uint32_t pocsCpuClz32(uint32_t val)
{
	return (val == 0U) ? 32U : (uint32_t)__builtin_clz(val);
}

static inline __attribute__((always_inline)) void pocsCpuSetNz(uint32_t val)
{
	g_pocsState.cpsr = (val & 0x80000000U) |
	                   (((pocsCpuClz32(val) & 0x3FU) << 25) & 0x40000000U) |
	                   (g_pocsState.cpsr & 0x3FFFFFFFU);
}

static inline __attribute__((always_inline)) void pocsCpuSetAddFlags(uint32_t lhs, uint32_t rhs, uint32_t result)
{
	uint32_t aBit = lhs >> 31;
	uint32_t bBit = rhs >> 31;
	uint32_t rNeg = (result >> 31) ^ 1U;
	uint32_t carry = (((rNeg | bBit) & aBit) | (rNeg & bBit)) & 1U;
	uint32_t overflow = ((rNeg & bBit & aBit) | (((rNeg | bBit | aBit) == 0U) ? 1U : 0U)) & 1U;

	g_pocsState.cpsr = (result & 0x80000000U) |
	                   (((pocsCpuClz32(result) & 0x3FU) << 25) & 0x40000000U) |
	                   (g_pocsState.cpsr & 0x0FFFFFFFU) |
	                   (carry << 29) |
	                   (overflow << 28);
}

static inline __attribute__((always_inline)) void pocsCpuSetSubFlags(uint32_t lhs, uint32_t rhs, uint32_t result)
{
	uint32_t aBit = (lhs >> 31) ^ 1U;
	uint32_t bBit = rhs >> 31;
	uint32_t rBit = result >> 31;
	uint32_t borrow = (((rBit | bBit) & aBit) | (rBit & bBit)) & 1U;
	uint32_t carry = borrow ^ 1U;
	uint32_t overflow = ((rBit & bBit & aBit) | (((rBit | bBit | aBit) == 0U) ? 1U : 0U)) & 1U;

	g_pocsState.cpsr = (result & 0x80000000U) |
	                   (((pocsCpuClz32(result) & 0x3FU) << 25) & 0x40000000U) |
	                   (g_pocsState.cpsr & 0x0FFFFFFFU) |
	                   (carry << 29) |
	                   (overflow << 28);
}

static inline __attribute__((always_inline)) int pocsCpuCondMet(uint32_t condCode)
{
	uint32_t cpsr = g_pocsState.cpsr;
	int baseCond;

	switch (condCode >> 1) {
	case 0U: /* EQ / NE */
		baseCond = (cpsr & 0x40000000U) != 0U;
		break;
	case 1U: /* CS / CC */
		baseCond = (cpsr & 0x20000000U) != 0U;
		break;
	case 2U: /* MI / PL */
		baseCond = (int32_t)cpsr < 0;
		break;
	case 3U: /* VS / VC */
		baseCond = (cpsr & 0x10000000U) != 0U;
		break;
	case 4U: /* HI / LS */
		baseCond = ((cpsr >> 29) & 3U) == 1U;
		break;
	case 5U: /* GE / LT */
		baseCond = (((cpsr & 0x10000000U) != 0U) == ((int32_t)cpsr < 0));
		break;
	case 6U: /* GT / LE */
		baseCond = !((((cpsr & 0x10000000U) != 0U) ^ ((int32_t)cpsr < 0)) ||
		             ((cpsr & 0x40000000U) != 0U));
		break;
	default: /* AL / NV */
		baseCond = 1;
		break;
	}
	return ((int)(condCode & 1U) != baseCond);
}

static inline __attribute__((always_inline)) int32_t pocsBranchPenaltyThumb(uint32_t targetPc)
{
	if ((targetPc & 0x0E000000U) == 0U)
		return -3;
	return ((g_pocsState.clockPendingReg & 8U) != 0U) ? -6 : -3;
}

static inline __attribute__((always_inline)) int32_t pocsBranchPenaltyArm(uint32_t targetPc)
{
	if ((targetPc & 0x0E000000U) == 0U)
		return -3;
	return ((g_pocsState.clockPendingReg & 8U) != 0U) ? -12 : -6;
}

static inline __attribute__((always_inline)) uint32_t pocsExecBx(uint32_t targetAddr)
{
	int toThumb = (int)(targetAddr & 1U);

	if ((targetAddr & 0x0E000000U) != 0U) {
		int mult = toThumb + 1;
		if ((g_pocsState.clockPendingReg & 8U) != 0U)
			mult *= 2;
		g_pocsState.cpuCyclesLeft -= 3 * mult;
	} else {
		g_pocsState.cpuCyclesLeft -= 3;
	}

	g_pocsState.cpsr = (g_pocsState.cpsr & ~0x20U) | ((uint32_t)toThumb << 5);
	g_pocsState.schedTargetCycle -= pocsScaleCpuCycles(g_pocsState.cpuCyclesLeft);
	g_pocsState.cpuCyclesLeft = 0;
	return targetAddr & ~1U;
}

static inline __attribute__((always_inline)) uint16_t pocsFetchThumb16(uint32_t pc)
{
	uint32_t region = (pc >> 25) & 7U;

	if (region == 0U)
		return pocsLoadLe16(g_pocsState.internalRam, pc & 0x7FEU);
	if (region == 2U) {
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta16(g_pocsState.lcdRefreshRate);
		return pocsLoadLe16(g_pocsBiosRom, pc & 0x3FFEU);
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(pc, 1) : pc;
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta16(g_pocsState.lcdContrast);
		return pocsLoadLe16(g_pocsState.flashDataPtr, effAddr & 0x1FFFEU);
	}
	if (pc - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		uint32_t off = (pc - POCS_LCD_VRAM_BASE) & ~1U;
		g_pocsState.cpuCyclesLeft--;
		return (uint16_t)((g_pocsState.lcdVramFront[off >> 2] >> (8U * (off & 2U))) & 0xFFFFU);
	}
	return 0U;
}

static inline __attribute__((always_inline)) uint32_t pocsFetchArm32(uint32_t pc)
{
	uint32_t region = (pc >> 25) & 7U;

	if (region == 0U)
		return pocsLoadLe32(g_pocsState.internalRam, pc & 0x7FCU);
	if (region == 2U) {
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta32(g_pocsState.lcdRefreshRate);
		return pocsLoadLe32(g_pocsBiosRom, pc & 0x3FFCU);
	}
	if (region == 1U || region == 4U) {
		uint32_t effAddr = (region == 1U) ? pocsRemapFlashAddr(pc, 1) : pc;
		g_pocsState.cpuCyclesLeft += pocsRomWaitDelta32(g_pocsState.lcdContrast);
		return pocsLoadLe32(g_pocsState.flashDataPtr, effAddr & 0x1FFFCU);
	}
	if (pc - POCS_LCD_VRAM_BASE <= POCS_LCD_VRAM_SPAN) {
		g_pocsState.cpuCyclesLeft--;
		return g_pocsState.lcdVramFront[(pc - POCS_LCD_VRAM_BASE) >> 2];
	}
	return 0U;
}

static inline __attribute__((always_inline)) uint32_t pocsExecThumbLoop(uint32_t pc)
{
	while (g_pocsState.cpuCyclesLeft > 0) {
		uint32_t instr = pocsFetchThumb16(pc);
		uint32_t pcPlus4 = pc + 4U;
		uint32_t opGroup = instr >> 11;

		g_pocsState.regs[15] = pcPlus4;

		switch (opGroup) {
		case 0U:
		case 1U:
		case 2U: { /* LSL / LSR / ASR imm5 */
			uint32_t res = pocsCpuBarrelShift((int)((instr >> 3) & 7U), (int)opGroup,
			                                  (int)((instr >> 6) & 0x1FU), 0, 1);
			g_pocsState.regs[instr & 7U] = res;
			pocsCpuSetNz(res);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 3U: { /* ADD / SUB reg or imm3 */
			uint32_t lhs = g_pocsState.regs[(instr >> 3) & 7U];
			uint32_t rhs = ((instr & 0x400U) != 0U) ? ((instr >> 6) & 7U)
			                                        : g_pocsState.regs[(instr >> 6) & 7U];
			int isSub = (instr & 0x200U) != 0U;
			uint32_t res = isSub ? (lhs - rhs) : (lhs + rhs);

			g_pocsState.regs[instr & 7U] = res;
			if (isSub)
				pocsCpuSetSubFlags(lhs, rhs, res);
			else
				pocsCpuSetAddFlags(lhs, rhs, res);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 4U: { /* MOV Rd, #imm8 */
			uint32_t imm = instr & 0xFFU;
			g_pocsState.regs[(instr >> 8) & 7U] = imm;
			pocsCpuSetNz(imm);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 5U: { /* CMP Rd, #imm8 */
			uint32_t lhs = g_pocsState.regs[(instr >> 8) & 7U];
			uint32_t imm = instr & 0xFFU;
			pocsCpuSetSubFlags(lhs, imm, lhs - imm);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 6U: { /* ADD Rd, #imm8 */
			uint32_t rd = (instr >> 8) & 7U;
			uint32_t lhs = g_pocsState.regs[rd];
			uint32_t imm = instr & 0xFFU;
			uint32_t res = lhs + imm;
			g_pocsState.regs[rd] = res;
			pocsCpuSetAddFlags(lhs, imm, res);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 7U: { /* SUB Rd, #imm8 */
			uint32_t rd = (instr >> 8) & 7U;
			uint32_t lhs = g_pocsState.regs[rd];
			uint32_t imm = instr & 0xFFU;
			uint32_t res = lhs - imm;
			g_pocsState.regs[rd] = res;
			pocsCpuSetSubFlags(lhs, imm, res);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 8U: { /* ALU / Hi-reg / BX */
			uint32_t subOp = (instr >> 6) & 0x1FU;
			uint32_t rdLow = instr & 7U;
			uint32_t rsLow = (instr >> 3) & 7U;

			switch (subOp) {
			case 0U: { /* AND */
				uint32_t res = g_pocsState.regs[rdLow] & g_pocsState.regs[rsLow];
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 1U: { /* EOR */
				uint32_t res = g_pocsState.regs[rdLow] ^ g_pocsState.regs[rsLow];
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 2U:
			case 3U:
			case 4U: { /* LSL / LSR / ASR reg */
				uint32_t res = pocsCpuBarrelShift((int)rdLow, (int)(subOp - 2U), (int)rsLow, 1, 1);
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 5U: { /* ADC */
				uint32_t lhs = g_pocsState.regs[rdLow];
				uint32_t rhs = g_pocsState.regs[rsLow];
				uint32_t carry = (g_pocsState.cpsr >> 29) & 1U;
				uint32_t res = lhs + rhs + carry;
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetAddFlags(lhs, rhs, res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 6U: { /* SBC */
				uint32_t lhs = g_pocsState.regs[rdLow];
				uint32_t rhs = g_pocsState.regs[rsLow];
				uint32_t borrow = ((g_pocsState.cpsr >> 29) & 1U) ^ 1U;
				uint32_t res = lhs - rhs - borrow;
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetSubFlags(lhs, rhs, res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 7U: { /* ROR reg */
				uint32_t res = pocsCpuBarrelShift((int)rdLow, 3, (int)rsLow, 1, 1);
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 8U: /* TST */
				pocsCpuSetNz(g_pocsState.regs[rdLow] & g_pocsState.regs[rsLow]);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			case 9U: { /* NEG */
				uint32_t rhs = g_pocsState.regs[rsLow];
				uint32_t res = 0U - rhs;
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetSubFlags(0U, rhs, res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 10U: { /* CMP */
				uint32_t lhs = g_pocsState.regs[rdLow];
				uint32_t rhs = g_pocsState.regs[rsLow];
				pocsCpuSetSubFlags(lhs, rhs, lhs - rhs);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 11U: { /* CMN */
				uint32_t lhs = g_pocsState.regs[rdLow];
				uint32_t rhs = g_pocsState.regs[rsLow];
				pocsCpuSetAddFlags(lhs, rhs, lhs + rhs);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 12U: { /* ORR */
				uint32_t res = g_pocsState.regs[rdLow] | g_pocsState.regs[rsLow];
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 13U: { /* MUL */
				int32_t lhs = (int32_t)g_pocsState.regs[rdLow];
				uint32_t res = (uint32_t)lhs * g_pocsState.regs[rsLow];
				uint32_t absLhs = (uint32_t)((lhs < 0) ? -lhs : lhs);
				uint32_t mCycles = 4U - (pocsCpuClz32(absLhs | 1U) >> 3);

				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				g_pocsState.cpuCyclesLeft -= (int32_t)mCycles + 1;
				pc += 2U;
				break;
			}
			case 14U: { /* BIC */
				uint32_t res = g_pocsState.regs[rdLow] & ~g_pocsState.regs[rsLow];
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 15U: { /* MVN */
				uint32_t res = ~g_pocsState.regs[rsLow];
				g_pocsState.regs[rdLow] = res;
				pocsCpuSetNz(res);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 17U:
			case 18U:
			case 19U: { /* ADD HiReg */
				uint32_t rdHi = rdLow + (((instr & 0x80U) != 0U) ? 8U : 0U);
				uint32_t rsHi = (instr >> 3) & 0xFU;
				uint32_t res = g_pocsState.regs[rdHi] + g_pocsState.regs[rsHi];

				if (rdHi == 15U) {
					pc = res & ~1U;
					g_pocsState.cpuCyclesLeft += pocsBranchPenaltyThumb(pc);
				} else {
					g_pocsState.regs[rdHi] = res;
					pc += 2U;
					g_pocsState.cpuCyclesLeft--;
				}
				break;
			}
			case 21U:
			case 22U:
			case 23U: { /* CMP HiReg */
				uint32_t rdHi = rdLow + (((instr & 0x80U) != 0U) ? 8U : 0U);
				uint32_t rsHi = (instr >> 3) & 0xFU;
				uint32_t lhs = g_pocsState.regs[rdHi];
				uint32_t rhs = g_pocsState.regs[rsHi];
				pocsCpuSetSubFlags(lhs, rhs, lhs - rhs);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
				break;
			}
			case 24U:
			case 25U:
			case 26U:
			case 27U: { /* MOV HiReg */
				uint32_t rdHi = rdLow + (((instr & 0x80U) != 0U) ? 8U : 0U);
				uint32_t rsHi = (instr >> 3) & 0xFU;
				uint32_t res = g_pocsState.regs[rsHi];

				if (rdHi == 15U) {
					pc = res & ~1U;
					g_pocsState.cpuCyclesLeft += pocsBranchPenaltyThumb(pc);
				} else {
					g_pocsState.regs[rdHi] = res;
					pc += 2U;
					g_pocsState.cpuCyclesLeft--;
				}
				break;
			}
			case 16U:
			case 20U:
				pc = (uint32_t)pocsCpuExceptionUndef();
				break;
			default:
				if ((instr & 0xFF87U) == 0x4700U) {
					return pocsExecBx(g_pocsState.regs[(instr >> 3) & 0xFU]);
				}
				pc = (uint32_t)pocsCpuExceptionUndef();
				break;
			}
			break;
		}
		case 9U: { /* LDR Rd, [PC, #imm8*4] */
			uint32_t addr = (pc & ~3U) + 4U + 4U * (instr & 0xFFU);
			g_pocsState.regs[(instr >> 8) & 7U] = pocsBusRead32(addr);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 10U: { /* STR / STRH / STRB / LDSB [Rb, Ro] */
			uint32_t width = 4U >> ((instr >> 9) & 3U);
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + g_pocsState.regs[(instr >> 6) & 7U];
			uint32_t rd = instr & 7U;

			if (width == 0U)
				g_pocsState.regs[rd] = pocsBusReadSigned8(addr);
			else if (width == 4U)
				pocsBusWrite32(addr, g_pocsState.regs[rd]);
			else if (width == 2U)
				pocsBusWrite16(addr, g_pocsState.regs[rd]);
			else
				pocsBusWrite8(addr, g_pocsState.regs[rd]);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 11U: { /* LDR / LDRH / LDRB / LDSH [Rb, Ro] */
			uint32_t width = 4U >> ((instr >> 9) & 3U);
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + g_pocsState.regs[(instr >> 6) & 7U];
			uint32_t rd = instr & 7U;

			if (width == 0U)
				g_pocsState.regs[rd] = pocsBusReadSigned16(addr);
			else if (width == 4U)
				g_pocsState.regs[rd] = pocsRotateRight32(pocsBusRead32(addr), 8U * (addr & 3U));
			else if (width == 2U)
				g_pocsState.regs[rd] = pocsBusRead16(addr);
			else
				g_pocsState.regs[rd] = pocsBusRead8(addr);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 12U: { /* STR Rd, [Rb, #imm5*4] */
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + 4U * ((instr >> 6) & 0x1FU);
			pocsBusWrite32(addr, g_pocsState.regs[instr & 7U]);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 13U: { /* LDR Rd, [Rb, #imm5*4] */
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + 4U * ((instr >> 6) & 0x1FU);
			g_pocsState.regs[instr & 7U] = pocsRotateRight32(pocsBusRead32(addr), 8U * (addr & 3U));
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 14U: { /* STRB Rd, [Rb, #imm5] */
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + ((instr >> 6) & 0x1FU);
			pocsBusWrite8(addr, g_pocsState.regs[instr & 7U]);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 15U: { /* LDRB Rd, [Rb, #imm5] */
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + ((instr >> 6) & 0x1FU);
			g_pocsState.regs[instr & 7U] = pocsBusRead8(addr);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 16U: { /* STRH Rd, [Rb, #imm5*2] */
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + 2U * ((instr >> 6) & 0x1FU);
			pocsBusWrite16(addr, g_pocsState.regs[instr & 7U]);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 17U: { /* LDRH Rd, [Rb, #imm5*2] */
			uint32_t addr = g_pocsState.regs[(instr >> 3) & 7U] + 2U * ((instr >> 6) & 0x1FU);
			g_pocsState.regs[instr & 7U] = pocsBusRead16(addr);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 18U: { /* STR Rd, [SP, #imm8*4] */
			uint32_t addr = g_pocsState.regs[13] + 4U * (instr & 0xFFU);
			pocsBusWrite32(addr, g_pocsState.regs[(instr >> 8) & 7U]);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 19U: { /* LDR Rd, [SP, #imm8*4] */
			uint32_t addr = g_pocsState.regs[13] + 4U * (instr & 0xFFU);
			g_pocsState.regs[(instr >> 8) & 7U] = pocsRotateRight32(pocsBusRead32(addr), 8U * (addr & 3U));
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 20U: /* ADD Rd, PC, #imm8*4 */
			g_pocsState.regs[(instr >> 8) & 7U] = (pcPlus4 & ~3U) + 4U * (instr & 0xFFU);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		case 21U: /* ADD Rd, SP, #imm8*4 */
			g_pocsState.regs[(instr >> 8) & 7U] = g_pocsState.regs[13] + 4U * (instr & 0xFFU);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		case 22U: { /* ADD/SUB SP, #imm7*4 or PUSH */
			uint32_t subOp = (instr >> 9) & 3U;
			if (subOp == 0U) {
				uint32_t delta = 4U * (instr & 0x7FU);
				if ((instr & 0x80U) != 0U)
					g_pocsState.regs[13] -= delta;
				else
					g_pocsState.regs[13] += delta;
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
			} else if (subOp == 2U) {
				uint32_t armPush = POCS_ARM_PUSH_TEMPLATE | (instr & 0xFFU) |
				                   (((instr & 0x100U) != 0U) ? 0x4000U : 0U);
				pocsCpuBlockTransfer(armPush, 1);
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
			} else {
				pc = (uint32_t)pocsCpuExceptionUndef();
			}
			break;
		}
		case 23U: { /* POP */
			if (((instr >> 9) & 3U) != 2U) {
				pc = (uint32_t)pocsCpuExceptionUndef();
			} else {
				uint32_t armPop = POCS_ARM_POP_TEMPLATE | (instr & 0xFFU) |
				                  (((instr & 0x100U) != 0U) ? 0x8000U : 0U);
				if (pocsCpuBlockTransfer(armPop, 0) != 0) {
					pc = g_pocsState.regs[15] & ~1U;
					g_pocsState.cpuCyclesLeft += pocsBranchPenaltyThumb(pc);
				} else {
					pc += 2U;
					g_pocsState.cpuCyclesLeft--;
				}
			}
			break;
		}
		case 24U:
		case 25U: { /* STMIA / LDMIA */
			uint32_t baseReg = (instr >> 8) & 7U;
			uint32_t armLdmStm = POCS_ARM_STMIA_TEMPLATE | (baseReg << 16) | (instr & 0xFFU);

			if (opGroup == 25U) {
				armLdmStm |= 0x00100000U;
				if (((instr >> baseReg) & 1U) != 0U)
					armLdmStm &= ~0x00200000U;
			}
			pocsCpuBlockTransfer(armLdmStm, 0);
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		case 26U:
		case 27U: { /* B<cond> / SWI / UNDEF */
			uint32_t condCode = (instr >> 8) & 0xFU;
			if (condCode == 15U) {
				pc = pocsCpuExceptionSwi();
			} else if (condCode == 14U) {
				pc = (uint32_t)pocsCpuExceptionUndef();
			} else if (pocsCpuCondMet(condCode)) {
				pc = pcPlus4 + (uint32_t)(2 * (int8_t)(instr & 0xFFU));
				g_pocsState.cpuCyclesLeft += pocsBranchPenaltyThumb(pc);
			} else {
				pc += 2U;
				g_pocsState.cpuCyclesLeft--;
			}
			break;
		}
		case 28U: { /* Unconditional B */
			int32_t offset = ((int32_t)(instr << 21)) >> 20;
			pc = pcPlus4 + (uint32_t)offset;
			g_pocsState.cpuCyclesLeft += pocsBranchPenaltyThumb(pc);
			break;
		}
		case 29U: /* Pokejima SCPS10104 undef trap */
			pc = pocsPokejimaHandleUndefTrap(pc + 2U);
			break;
		case 30U: { /* BL prefix */
			int32_t offset = ((int32_t)(instr << 21)) >> 9;
			g_pocsState.regs[14] = pcPlus4 + (uint32_t)offset;
			pc += 2U;
			g_pocsState.cpuCyclesLeft--;
			break;
		}
		default: { /* BL suffix */
			uint32_t target = g_pocsState.regs[14] + 2U * (instr & 0x7FFU);
			g_pocsState.regs[14] = (pc + 2U) | 1U;
			pc = target;
			g_pocsState.cpuCyclesLeft += pocsBranchPenaltyThumb(pc);
			break;
		}
		}
	}
	return pc;
}

static inline __attribute__((always_inline)) uint32_t pocsExecArmLoop(uint32_t pc)
{
	while (g_pocsState.cpuCyclesLeft > 0) {
		uint32_t instr = pocsFetchArm32(pc);
		uint32_t pcPlus8 = pc + 8U;
		uint32_t condCode = instr >> 28;
		int bit4;
		int shOp;
		int setFlags;
		uint32_t rnReg;
		uint32_t rdReg;
		uint32_t rsReg;
		uint32_t rmReg;
		uint32_t decodeKey;
		int pcUpdated = 0;

		g_pocsState.regs[15] = pcPlus8;

		if (!pocsCpuCondMet(condCode)) {
			pc += 4U;
			g_pocsState.cpuCyclesLeft--;
			continue;
		}

		bit4 = (instr & 0x10U) != 0U;
		rsReg = (instr >> 8) & 0xFU;
		shOp = (int)((instr >> 5) & 3U);
		setFlags = (instr & 0x00100000U) != 0U;
		rnReg = (instr >> 16) & 0xFU;
		rdReg = (instr >> 12) & 0xFU;
		rmReg = instr & 0xFU;
		decodeKey = (uint32_t)bit4 +
		            2U * ((uint32_t)((instr & 0x80U) != 0U) + 2U * ((instr >> 23) & 0x1FU));

		switch (decodeKey) {
		case 0x00U:
		case 0x02U:
		case 0x04U:
		case 0x06U:
		case 0x01U:
		case 0x05U: { /* ALU reg / reg-shift */
			int shiftAmt = bit4 ? (int)rsReg : (int)((instr >> 7) & 0x1FU);
			int carryIn = (int)((g_pocsState.cpsr >> 29) & 1U);
			uint32_t lhs = g_pocsState.regs[rnReg];
			uint32_t rhs = pocsCpuBarrelShift((int)rmReg, shOp, shiftAmt, bit4, setFlags);

			g_pocsState.regs[rdReg] = pocsCpuAluOp((int)((instr >> 21) & 7U), setFlags, lhs, rhs, carryIn);
			if (pocsCpuFinishPcWrite((int)rdReg, setFlags, pc) != 0) {
				pc = g_pocsState.regs[15];
				pcUpdated = 1;
			}
			break;
		}
		case 0x03U: { /* MUL / MLA / halfword transfer */
			if (shOp != 0) {
				pocsCpuHalfwordAccess(instr, shOp, (int)rnReg, (int)rdReg);
			} else {
				uint32_t mulOp = (instr >> 21) & 3U;
				if (mulOp == 0U || mulOp == 1U) {
					int32_t rsVal = (int32_t)g_pocsState.regs[rsReg];
					uint32_t absRs = (uint32_t)((rsVal < 0) ? -rsVal : rsVal);
					uint32_t prod = g_pocsState.regs[rmReg] * (uint32_t)rsVal;
					uint32_t mCycles = 4U - (pocsCpuClz32(absRs | 1U) >> 3);

					if (mulOp == 1U) {
						prod += g_pocsState.regs[rdReg];
						mCycles += 1U;
					}
					g_pocsState.regs[rnReg] = prod;
					g_pocsState.cpuCyclesLeft -= (int32_t)mCycles;
					if (setFlags)
						pocsCpuSetNz(prod);
				}
			}
			break;
		}
		case 0x07U: { /* UMULL / UMLAL / SMULL / SMLAL / halfword transfer */
			if (shOp != 0) {
				pocsCpuHalfwordAccess(instr, shOp, (int)rnReg, (int)rdReg);
			} else {
				uint32_t mulOp = (instr >> 21) & 3U;
				int32_t rsVal = (int32_t)g_pocsState.regs[rsReg];
				uint32_t absRs = (uint32_t)((rsVal < 0) ? -rsVal : rsVal);
				uint64_t prod = (uint64_t)g_pocsState.regs[rmReg] * (uint64_t)(uint32_t)rsVal;
				uint32_t mCycles = 5U - (pocsCpuClz32(absRs | 1U) >> 3);

				if (mulOp == 1U || mulOp == 3U) {
					uint64_t acc = ((uint64_t)g_pocsState.regs[rnReg] << 32) | g_pocsState.regs[rdReg];
					prod += acc;
					mCycles += 1U;
				}
				g_pocsState.regs[rdReg] = (uint32_t)prod;
				g_pocsState.regs[rnReg] = (uint32_t)(prod >> 32);
				g_pocsState.cpuCyclesLeft -= (int32_t)mCycles;
				if (setFlags) {
					uint32_t hi = (uint32_t)(prod >> 32);
					if ((uint32_t)prod != 0U)
						hi |= 1U;
					pocsCpuSetNz(hi);
				}
			}
			break;
		}
		case 0x08U:
		case 0x0AU: { /* TST / TEQ / CMP / CMN imm-shift or MRS/MSR reg */
			if (setFlags) {
				if (rdReg != 0U) {
					pc = (uint32_t)pocsCpuExceptionUndef();
					pcUpdated = 1;
				} else {
					uint32_t lhs = g_pocsState.regs[rnReg];
					uint32_t rhs = pocsCpuBarrelShift((int)rmReg, shOp, (int)((instr >> 7) & 0x1FU), 0, 1);
					int aluOp = (int)((instr >> 21) & 3U);
					if (aluOp == 3)
						aluOp = 4;
					pocsCpuAluOp(aluOp, 1, lhs, rhs, 0);
				}
			} else if ((instr & 0xE0U) != 0U) {
				pc = (uint32_t)pocsCpuExceptionUndef();
				pcUpdated = 1;
			} else {
				pocsCpuPsrTransfer(instr, (int)rdReg, g_pocsState.regs[rmReg]);
			}
			break;
		}
		case 0x09U: { /* TST / TEQ / CMP / CMN reg-shift or BX */
			if (setFlags) {
				if (rdReg != 0U) {
					pc = (uint32_t)pocsCpuExceptionUndef();
					pcUpdated = 1;
				} else {
					uint32_t lhs = g_pocsState.regs[rnReg];
					uint32_t rhs = pocsCpuBarrelShift((int)rmReg, shOp, (int)rsReg, 1, 1);
					int aluOp = (int)((instr >> 21) & 3U);
					if (aluOp == 3)
						aluOp = 4;
					pocsCpuAluOp(aluOp, 1, lhs, rhs, 0);
				}
			} else if ((instr & 0x0FFFFFF0U) == 0x012FFF10U) {
				return pocsExecBx(g_pocsState.regs[rmReg]);
			} else {
				pc = (uint32_t)pocsCpuExceptionUndef();
				pcUpdated = 1;
			}
			break;
		}
		case 0x0BU:
		case 0x0FU: { /* SWP / SWPB or halfword transfer */
			if (shOp != 0) {
				pocsCpuHalfwordAccess(instr, shOp, (int)rnReg, (int)rdReg);
			} else {
				int isByte = (instr & 0x00400000U) != 0U;
				uint32_t oldRd = g_pocsState.regs[rdReg];
				uint32_t loaded;

				pocsCpuMemAccess(instr, 2 * isByte + 1, (int)rnReg, (int)rdReg, 0);
				loaded = g_pocsState.regs[rdReg];
				g_pocsState.regs[rdReg] = oldRd;
				pocsCpuMemAccess(instr, 2 * isByte, (int)rnReg, (int)rmReg, 0);
				g_pocsState.regs[rdReg] = loaded;
				g_pocsState.cpuCyclesLeft--;
			}
			break;
		}
		case 0x0CU:
		case 0x0EU:
		case 0x0DU: { /* ORR / MOV / BIC / MVN shifted reg */
			int shiftAmt = bit4 ? (int)rsReg : (int)((instr >> 7) & 0x1FU);
			uint32_t lhs = g_pocsState.regs[rnReg];
			uint32_t rhs = pocsCpuBarrelShift((int)rmReg, shOp, shiftAmt, bit4, setFlags);

			g_pocsState.regs[rdReg] = pocsCpuLogicOp((int)((instr >> 21) & 3U), setFlags, lhs, rhs);
			if (pocsCpuFinishPcWrite((int)rdReg, setFlags, pc) != 0) {
				pc = g_pocsState.regs[15];
				pcUpdated = 1;
			}
			break;
		}
		case 0x10U: case 0x11U: case 0x12U: case 0x13U:
		case 0x14U: case 0x15U: case 0x16U: case 0x17U: { /* ALU imm */
			uint32_t rotAmt = 2U * rsReg;
			int carryIn = (int)((g_pocsState.cpsr >> 29) & 1U);
			uint32_t lhs = g_pocsState.regs[rnReg];
			uint32_t imm = pocsRotateRight32(instr & 0xFFU, rotAmt);

			if (rotAmt != 0U && setFlags)
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | ((imm >> 31) << 29);
			g_pocsState.regs[rdReg] = pocsCpuAluOp((int)((instr >> 21) & 7U), setFlags, lhs, imm, carryIn);
			if (pocsCpuFinishPcWrite((int)rdReg, setFlags, pc) != 0) {
				pc = g_pocsState.regs[15];
				pcUpdated = 1;
			}
			break;
		}
		case 0x18U: case 0x19U: case 0x1AU: case 0x1BU: { /* TST/TEQ/CMP/CMN imm or MSR imm */
			if (setFlags) {
				if (rdReg != 0U) {
					pc = (uint32_t)pocsCpuExceptionUndef();
					pcUpdated = 1;
				} else {
					uint32_t rotAmt = 2U * rsReg;
					uint32_t lhs = g_pocsState.regs[rnReg];
					uint32_t imm = pocsRotateRight32(instr & 0xFFU, rotAmt);
					int aluOp = (int)((instr >> 21) & 3U);

					if (rotAmt != 0U)
						g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | ((imm >> 31) << 29);
					if (aluOp == 3)
						aluOp = 4;
					pocsCpuAluOp(aluOp, 1, lhs, imm, 0);
				}
			} else if (rdReg != 15U) {
				pc = (uint32_t)pocsCpuExceptionUndef();
				pcUpdated = 1;
			} else {
				pocsCpuPsrTransfer(instr, 15, pocsRotateRight32(instr & 0xFFU, 2U * rsReg));
			}
			break;
		}
		case 0x1CU: case 0x1DU: case 0x1EU: case 0x1FU: { /* ORR / MOV / BIC / MVN imm */
			uint32_t rotAmt = 2U * rsReg;
			uint32_t lhs = g_pocsState.regs[rnReg];
			uint32_t imm = pocsRotateRight32(instr & 0xFFU, rotAmt);

			if (rotAmt != 0U && setFlags)
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | ((imm >> 31) << 29);
			g_pocsState.regs[rdReg] = pocsCpuLogicOp((int)((instr >> 21) & 3U), setFlags, lhs, imm);
			if (pocsCpuFinishPcWrite((int)rdReg, setFlags, pc) != 0) {
				pc = g_pocsState.regs[15];
				pcUpdated = 1;
			}
			break;
		}
		case 0x20U: case 0x21U: case 0x22U: case 0x23U:
		case 0x24U: case 0x25U: case 0x26U: case 0x27U:
		case 0x28U: case 0x29U: case 0x2AU: case 0x2BU:
		case 0x2CU: case 0x2DU: case 0x2EU: case 0x2FU: { /* LDR / STR imm12 */
			int mode = (int)((instr & 0x00100000U) != 0U) + 2 * (int)((instr & 0x00400000U) != 0U);
			pocsCpuMemAccess(instr, mode, (int)rnReg, (int)rdReg, (int)(instr & 0xFFFU));
			if (setFlags && pocsCpuFinishPcWrite((int)rdReg, 0, pc) != 0) {
				pc = g_pocsState.regs[15];
				pcUpdated = 1;
			}
			break;
		}
		case 0x30U: case 0x32U: case 0x34U: case 0x36U:
		case 0x38U: case 0x3AU: case 0x3CU: case 0x3EU: { /* LDR / STR shifted reg */
			int mode = (int)((instr & 0x00100000U) != 0U) + 2 * (int)((instr & 0x00400000U) != 0U);
			uint32_t offset = pocsCpuBarrelShift((int)rmReg, shOp, (int)((instr >> 7) & 0x1FU), 0, 0);
			pocsCpuMemAccess(instr, mode, (int)rnReg, (int)rdReg, (int)offset);
			if (setFlags && pocsCpuFinishPcWrite((int)rdReg, 0, pc) != 0) {
				pc = g_pocsState.regs[15];
				pcUpdated = 1;
			}
			break;
		}
		case 0x40U: case 0x41U: case 0x42U: case 0x43U:
		case 0x44U: case 0x45U: case 0x46U: case 0x47U:
		case 0x48U: case 0x49U: case 0x4AU: case 0x4BU:
		case 0x4CU: case 0x4DU: case 0x4EU: case 0x4FU: { /* LDM / STM */
			int loadedPc = pocsCpuBlockTransfer(instr, (instr & 0x01000000U) != 0U);
			if (loadedPc != 0 && setFlags) {
				pc = g_pocsState.regs[15];
				g_pocsState.cpuCyclesLeft += pocsBranchPenaltyArm(pc);
				pcUpdated = 1;
			}
			break;
		}
		case 0x50U: case 0x51U: case 0x52U: case 0x53U:
		case 0x54U: case 0x55U: case 0x56U: case 0x57U: { /* B */
			int32_t offset = ((int32_t)(instr << 8)) >> 6;
			pc = pcPlus8 + (uint32_t)offset;
			g_pocsState.cpuCyclesLeft += pocsBranchPenaltyArm(pc);
			pcUpdated = 1;
			break;
		}
		case 0x58U: case 0x59U: case 0x5AU: case 0x5BU:
		case 0x5CU: case 0x5DU: case 0x5EU: case 0x5FU: { /* BL */
			int32_t offset = ((int32_t)(instr << 8)) >> 6;
			g_pocsState.regs[14] = pc + 4U;
			pc = pcPlus8 + (uint32_t)offset;
			g_pocsState.cpuCyclesLeft += pocsBranchPenaltyArm(pc);
			pcUpdated = 1;
			break;
		}
		case 0x31U: case 0x33U: case 0x35U: case 0x37U:
		case 0x39U: case 0x3BU: case 0x3DU: case 0x3FU:
		case 0x60U: case 0x61U: case 0x62U: case 0x63U:
		case 0x64U: case 0x65U: case 0x66U: case 0x67U:
		case 0x68U: case 0x69U: case 0x6AU: case 0x6BU:
		case 0x6CU: case 0x6DU: case 0x6EU: case 0x6FU:
		case 0x70U: case 0x71U: case 0x72U: case 0x73U:
		case 0x74U: case 0x75U: case 0x76U: case 0x77U:
			pc = (uint32_t)pocsCpuExceptionUndef();
			pcUpdated = 1;
			break;
		default: /* SWI (0x78..0x7F) */
			pc = pocsCpuExceptionSwi();
			pcUpdated = 1;
			break;
		}

		if (!pcUpdated) {
			if (g_pocsState.regs[15] == pcPlus8) {
				pc += 4U;
				g_pocsState.cpuCyclesLeft--;
			} else {
				pc = g_pocsState.regs[15];
				g_pocsState.cpuCyclesLeft += pocsBranchPenaltyArm(pc);
			}
		}
	}
	return pc;
}

/* 0x81025e28 */
uint32_t pocsCpuRunCycles(uint32_t pc, int32_t cycleBudget)
{
	int32_t scaledBudget;
	uint32_t endCycle;

	g_pocsState.cpuCyclesLeft = 0;
	scaledBudget = pocsUnscaleCpuCycles(cycleBudget);
	if (scaledBudget < 1)
		scaledBudget = 1;
	endCycle = (uint32_t)(g_pocsState.schedTargetCycle + pocsScaleCpuCycles(scaledBudget));

	for (;;) {
		int32_t curCycle = pocsGetCurrentCycle();
		PocsSchedEvent *head = g_pocsState.schedHead.next;
		uint32_t nextEventCycle = (uint32_t)head->targetCycle;

		while ((int32_t)(nextEventCycle - 1U - (uint32_t)curCycle) <= 0) {
			PocsSchedEvent *prevEv = head->prev;
			PocsSchedEvent *nextEv = head->next;
			PocsSchedCallback cb = head->callback;

			prevEv->next = nextEv;
			nextEv->prev = prevEv;
			head->prev = NULL;
			g_pocsState.schedTargetCycle = (int32_t)nextEventCycle;
			g_pocsState.cpuCyclesLeft = 0;
			cb(head);

			head = g_pocsState.schedHead.next;
			nextEventCycle = (uint32_t)head->targetCycle;
		}

		{
			uint32_t sliceEnd = (nextEventCycle < endCycle) ? nextEventCycle : endCycle;
			uint16_t pendingIrqs;
			uint32_t cpsr;

			g_pocsState.schedTargetCycle = (int32_t)sliceEnd;
			g_pocsState.cpuCyclesLeft = pocsUnscaleCpuCycles((int32_t)(sliceEnd - (uint32_t)curCycle));

			cpsr = g_pocsState.cpsr;
			pendingIrqs = g_pocsState.intcEnable & g_pocsState.intcLatch;

			if (g_pocsState.clockStatusReg != 0U) {
				if ((pendingIrqs & POCS_IRQ_WAKE_MASK) == 0U) {
					g_pocsState.cpuCyclesLeft = 0;
					if ((int32_t)(endCycle - (uint32_t)pocsGetCurrentCycle()) <= 0)
						break;
					if (g_pocsCpuBreakRequested == 1) {
						g_pocsCpuBreakRequested = 0;
						break;
					}
					continue;
				}
				pocsClockWrite(4, 0U);
			}

			if ((cpsr & 0x40U) == 0U && (pendingIrqs & POCS_IRQ_FIQ_MASK) != 0U) {
				pocsCpuSwitchMode(1, 17);
				g_pocsState.regs[14] = pc + 4U;
				pc = 0x1CU;
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xFFFFFFC0U) | 0xD1U;
				g_pocsState.cpuCyclesLeft -= 3;
			} else if ((cpsr & 0x80U) == 0U && (pendingIrqs & POCS_IRQ_NORMAL_MASK) != 0U) {
				pocsCpuSwitchMode(1, 18);
				g_pocsState.regs[14] = pc + 4U;
				pc = 0x18U;
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xFFFFFFC0U) | 0x92U;
				g_pocsState.cpuCyclesLeft -= 3;
			}
		}

		if ((int32_t)(endCycle - (uint32_t)pocsGetCurrentCycle()) <= 0)
			break;
		if (g_pocsCpuBreakRequested == 1) {
			g_pocsCpuBreakRequested = 0;
			break;
		}
		if (g_pocsState.cpuCyclesLeft == 0)
			continue;

		if ((g_pocsState.cpsr & 0x20U) != 0U)
			pc = pocsExecThumbLoop(pc);
		else
			pc = pocsExecArmLoop(pc);
	}

	if (g_pocsState.inputSuppress != 0U) {
		int wantDocked = (g_pocsState.standaloneActive == 0U && g_pocsState.vmcInserted != 0U) ? 1 : 0;
		int curDocked = ((g_pocsState.gpioData & POCS_GPIO_DOCK_BIT) != 0U) ? 1 : 0;

		if (curDocked != wantDocked) {
			if (wantDocked != 0) {
				g_pocsState.gpioData |= POCS_GPIO_DOCK_BIT;
				g_pocsState.intcRawStatus |= POCS_IRQ_DOCK_MASK;
			} else {
				g_pocsState.gpioData &= (uint8_t)~POCS_GPIO_DOCK_BIT;
				g_pocsState.intcRawStatus &= (uint16_t)~POCS_IRQ_DOCK_MASK;
			}
			g_pocsState.intcLatch |= POCS_IRQ_DOCK_MASK;
			pocsCpuCheckPendingIrq();
		}
		g_pocsState.inputSuppress = 0U;
	}

	g_pocsState.schedTargetCycle -= pocsScaleCpuCycles(g_pocsState.cpuCyclesLeft);
	g_pocsState.cpuCyclesLeft = 0;
	return pc;
}
