#include "backtrace_walk.h"
#include "backtrace_unwind.h"
#include "exidx.h"
#include <kernel/error.h>

struct BacktraceInstructionReader {
	const uint32_t *cursor;
	int words;
	unsigned int byte;
};

extern void *sceKernelGetTLSAddr(unsigned int index);
extern int BacktraceFindExidx(SceUID processId, uintptr_t address,
		struct ExidxResult *result);

static inline __attribute__((always_inline))
unsigned int BacktraceNextByte(struct BacktraceInstructionReader *reader)
{
	unsigned int byte;

	if (!reader->words)
		return 0;
	byte = BacktraceInstructionByte(*reader->cursor, reader->byte);
	if (!reader->byte) {
		reader->cursor++;
		reader->words--;
		reader->byte = 3;
	} else {
		reader->byte--;
	}
	return byte;
}

int BacktraceWalk(SceUID processId, SceKernelCallFrame *frames, SceSize size,
		SceUInt32 *count, SceUInt32 flags, struct BacktraceRegisters *registers)
{
	struct BacktraceInstructionReader reader;
	struct ExidxResult exidx;
	uint32_t lower, upper, previousSp, previousPc, word, stack, restoredSp;
	uint32_t mask, value, shift;
	unsigned int capacity = size >> 3, stored = 0, total = 1;
	unsigned int instruction, operand, reg;

	if (!frames) {
		if (size)
			return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (size < sizeof(*frames)) {
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	}
	lower = *(uint32_t *)sceKernelGetTLSAddr(3);
	upper = *(uint32_t *)sceKernelGetTLSAddr(2);
	if (capacity) {
		frames[0].sp = registers->sp;
		frames[0].pc = registers->pc;
		stored = 1;
	}
	while (!(flags & 2) || total < capacity) {
		previousPc = registers->pc;
		previousSp = registers->sp;
		if (BacktraceFindExidx(0, previousPc, &exidx) < 0)
			break;
		registers->pc = 0;
		if (exidx.descriptor == EXIDX_CANTUNWIND)
			break;
		if ((int32_t)exidx.descriptor < 0) {
			if ((exidx.descriptor & EHABI_PERSONALITY_MASK) != EHABI_COMPACT_PR0)
				break;
			reader.cursor = &exidx.descriptor;
			reader.byte = 2;
			reader.words = 1;
		} else {
			reader.cursor = exidx.instructions;
			word = *reader.cursor;
			if ((word & EHABI_PERSONALITY_MASK) == EHABI_COMPACT_PR0) {
				reader.byte = 2;
				reader.words = 1;
			} else if ((word & EHABI_PERSONALITY_MASK) == EHABI_COMPACT_PR1) {
				reader.byte = 1;
				reader.words = ((word >> 16) & 0xff) + 1;
			} else {
				if ((int32_t)word < 0)
					break;
				/* Generic model: skip the personality word. */
				reader.words = ((const unsigned char *)reader.cursor)[7] + 1;
				reader.cursor++;
				reader.byte = 2;
			}
		}
		while (reader.words > 0) {
			if (registers->sp < lower || registers->sp > upper)
				goto finished;
			instruction = BacktraceNextByte(&reader);
			if ((instruction & EHABI_VSP_MASK) == EHABI_VSP_ADD) {
				registers->sp += BacktraceSmallStackAdjustment(instruction);
			} else if ((instruction & EHABI_VSP_MASK) == EHABI_VSP_SUB) {
				registers->sp -= BacktraceSmallStackAdjustment(instruction);
			} else if ((instruction & EHABI_GROUP_MASK) == EHABI_POP_MASK) {
				mask = ((instruction << 8) | BacktraceNextByte(&reader)) & 0xfff;
				if (!mask)
					goto finished;
				stack = registers->sp;
				restoredSp = 0;
				for (reg = 4; mask; mask >>= 1, reg++) {
					if (!(mask & 1))
						continue;
					value = *(const uint32_t *)stack;
					stack += 4;
					if (reg == BACKTRACE_SP)
						restoredSp = value;
					else
						registers->r[reg] = value;
				}
				registers->sp = restoredSp ? restoredSp : stack;
			} else if ((instruction & EHABI_GROUP_MASK) == EHABI_SET_VSP &&
				   (instruction & 0xd) != 0xd) {	/* not sp or pc */
				registers->sp = registers->r[instruction & 0xf];
			} else if ((instruction & EHABI_GROUP_MASK) == EHABI_POP_R4) {
				stack = registers->sp;
				for (reg = 4; reg <= (instruction & 7) + 4; reg++) {
					registers->r[reg] = *(const uint32_t *)stack;
					stack += 4;
				}
				if (instruction & EHABI_POP_R4_LR) {
					registers->lr = *(const uint32_t *)stack;
					stack += 4;
				}
				registers->sp = stack;
			} else if (instruction == EHABI_FINISH) {
				registers->pc = registers->lr;
				goto frameComplete;
			} else if (instruction == EHABI_POP_R0_R3) {
				if (!reader.words)
					goto finished;
				mask = BacktraceNextByte(&reader);
				if (!mask || (mask & 0xf0))
					goto finished;
				stack = registers->sp;
				for (reg = 0; mask; mask >>= 1, reg++) {
					if (!(mask & 1))
						continue;
					registers->r[reg] = *(const uint32_t *)stack;
					stack += 4;
				}
				registers->sp = stack;
			} else if (instruction == EHABI_VSP_ADD_ULEB128) {
				value = 0;
				shift = 0;
				while (reader.words) {
					operand = BacktraceNextByte(&reader);
					/* ARM register shifts use the low byte, saturating at32. */
					if ((shift & 0xff) < 32)
						value |= (operand & 0x7f) << (shift & 0xff);
					shift += 7;
					if (!(operand & 0x80))
						break;
				}
				/* EHABI: vsp = vsp + 0x204 + (uleb128 << 2) */
				registers->sp += 0x204 + value * 4;
			} else if (instruction == EHABI_POP_VFP_FSTMFDX ||
				   instruction == EHABI_POP_VFP_D16 || instruction == EHABI_POP_VFP) {
				operand = BacktraceNextByte(&reader);
				registers->sp += BacktraceFloatingStackAdjustment(instruction,
									  operand);
			} else if ((instruction & EHABI_SHORT_MASK) == EHABI_POP_VFP_D8_FSTMFDX) {
				registers->sp += BacktraceFloatingStackAdjustment(instruction, 0);
			} else {
				goto finished;
			}
		}
frameComplete:
		if (!registers->pc)
			registers->pc = registers->lr;
		if (previousSp == registers->sp && previousPc == registers->pc)
			break;
		if (total < capacity) {
			frames[total].sp = registers->sp;
			__builtin_prefetch(&frames[total + 1].pc);
			frames[total].pc = registers->pc;
			stored++;
		}
		total++;
	}
finished:
	if (count)
		*count = stored;
	return (flags & 2) ? 0 : total;
}
