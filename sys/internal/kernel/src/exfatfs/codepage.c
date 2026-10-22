/* Sony: codepage.c */
#include <scetypes.h>
#include <kernel/types.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>

#include "codepage_tables.h"

SceUInt16 *g_gbkTable;
SceUInt16 (*g_big5Table)[157];
SceUInt16 *g_koreanTable;
SceUID g_codepageMemBlock;
void *g_codepageBuffer;
int g_codepage;

void _gbkKoreanToUnicode(int codepage, int firstByte, int secondByte,
			 unsigned char *lowByte, unsigned char *highByte)
{
	SceUInt16 **table;
	SceUInt16 *mapping;
	int rowCount;
	int row;
	SceUInt16 unicode;
	unsigned char convertedLow;
	unsigned char convertedHigh;

	if (codepage == 15) {
		table = &g_koreanTable;
		rowCount = 125;
	} else {
		table = &g_gbkTable;
		rowCount = 126;
	}
	mapping = *table;

	if (firstByte == 5)
		goto escapedFirstByte;
	if (firstByte > 128) {
		row = firstByte - 129;
		goto convertPair;
	}
	if (codepage == 14 && firstByte == 128) {
		*lowByte = 0xac;
		*highByte = 0x20;
	} else {
		*lowByte = firstByte;
		*highByte = 0;
	}
	return;
escapedFirstByte:
	row = 100;
convertPair:
	if ((unsigned char)(secondByte - 64) > 190)
		goto invalidPair;
	if (row >= rowCount)
		goto invalidPair;
	secondByte += row * 191;
	secondByte -= 64;
	unicode = mapping[secondByte];
	if (!unicode)
		goto invalidPair;
	convertedLow = unicode;
	convertedHigh = unicode >> 8;
	goto writeUnicode;
invalidPair:
	convertedLow = 0xa0;
	convertedHigh = 0x25;
writeUnicode:
	*lowByte = convertedLow;
	*highByte = convertedHigh;
}

void _unicodeToGbkKorean(int codepage, unsigned int lowByte, int highByte,
			 unsigned char *firstByte, unsigned char *secondByte)
{
	SceUInt16 unicode;
	SceUInt16 *mapping;
	unsigned char replacement;
	int rowCount;
	int row;
	int column;

	if (codepage == 15) {
		mapping = g_koreanTable;
		unicode = lowByte + (highByte << 8);
		replacement = 0xe1;
		rowCount = 125;
	} else {
		unicode = lowByte + (highByte << 8);
		mapping = g_gbkTable;
		if (codepage == 14) {
			if (unicode == 0x20ac) {
				*firstByte = 0x80;
				*secondByte = 0;
				return;
			}
			if (unicode == 0x80) {
				*firstByte = 0xa1;
				*secondByte = 0xf6;
				return;
			}
		}
		replacement = 0xf6;
		rowCount = 126;
	}
	if (lowByte <= 0x80 && !highByte) {
		*firstByte = lowByte;
		*secondByte = 0;
		return;
	}
	if (unicode != 0x25a0) {
		for (row = 0; row < rowCount; ++row) {
			for (column = 0; column < 191; ++column) {
				if (mapping[column] == unicode) {
					SceUInt16 code = column + (row << 8) + 0x8140;

					*firstByte = code >> 8;
					*secondByte = code;
					return;
				}
			}
			mapping += 191;
		}
	}
	*firstByte = 0xa1;
	*secondByte = replacement;
}

void _big5ToUnicode(int codepage, int firstByte, int secondByte,
		    unsigned char *lowByte, unsigned char *highByte)
{
	int row;
	unsigned int column;
	SceUInt16 unicode;
	unsigned char convertedLow;
	unsigned char convertedHigh;

	if (firstByte == 5) {
		row = 68;
		firstByte = 229;
	} else if (firstByte > 128) {
		row = firstByte - 161;
	} else {
		*lowByte = firstByte;
		*highByte = 0;
		return;
	}
	if (secondByte > 126) {
		if (secondByte <= 160)
			goto invalidPair;
		column = secondByte - 98;
	} else {
		column = secondByte - 64;
	}
	if ((unsigned int)(firstByte - 161) <= 88 && column <= 156) {
		unicode = g_big5Table[row][column];
		if (unicode) {
			convertedLow = unicode;
			convertedHigh = unicode >> 8;
			goto writeUnicode;
		}
	}
invalidPair:
	convertedLow = 0xa0;
	convertedHigh = 0x25;
writeUnicode:
	*lowByte = convertedLow;
	*highByte = convertedHigh;
}

void _unicodeToBig5(int codepage, unsigned int lowByte, int highByte,
		    unsigned char *firstByte, unsigned char *secondByte)
{
	SceUInt16 unicode;
	SceUInt16 (*mapping)[157];
	int row;
	int column;

	if (lowByte <= 0x80 && !highByte) {
		*firstByte = lowByte;
		*secondByte = 0;
		return;
	}
	row = 0;
	unicode = lowByte + (highByte << 8);
	if (unicode != 0x25a0) {
		mapping = g_big5Table;
		for (; row < 89; ++row) {
			for (column = 0; column < 157; ++column) {
				if ((*mapping)[column] == unicode) {
					SceUInt16 code = column + ((row + 161) << 8);

					if (column <= 62)
						code += 64;
					else
						code += 98;
					*firstByte = code >> 8;
					*secondByte = code;
					return;
				}
			}
			++mapping;
		}
	}
	*firstByte = 0xa1;
	*secondByte = 0xbd;
}

void _uto1char(int codepage, unsigned int lowByte, int highByte,
	       unsigned char *firstByte, unsigned char *secondByte)
{
	const SceUInt16 *mapping;
	SceUInt16 unicode;
	int index;

	if ((highByte == 0) & (~lowByte >> 7)) {
		*firstByte = lowByte;
		*secondByte = 0;
		SCE_DBG_PRINTF_LEVEL(2, 15, 779, "ASCII code(0x%02x)\n", *firstByte);
		return;
	}
	unicode = lowByte + (highByte << 8);
	if ((unsigned int)(codepage - 2) <= 15)
		mapping = singleByteCodepageTables[codepage - 2];
	else
		mapping = cp437ToUnicode;
	SCE_DBG_PRINTF_LEVEL(2, 15, 835, "search code=0x%04x\n", unicode);
	for (index = 0; index < 128; ++index) {
		if (mapping[index] == unicode) {
			*firstByte = index + 128;
			*secondByte = 0;
			goto printCharacter;
		}
	}
	if (codepage == 12)
		*firstByte = 0x85;
	else if (codepage == 17)
		*firstByte = 0xbf;
	else
		*firstByte = 0xfe;
	*secondByte = 0;
printCharacter:
	SCE_DBG_PRINTF_LEVEL(2, 15, 875, "1char code=0x%02x\n", *firstByte);
}

void _jtou(int codepage, int firstByte, int secondByte,
	   unsigned char *lowByte, unsigned char *highByte)
{
	int row;
	int column;
	SceUInt16 unicode;

	if (firstByte == 5) {
		row = 105;
	} else {
		if (((unsigned int)(firstByte - 160) <= 63) | (firstByte <= 128)) {
			unsigned char upperByte;

			if ((unsigned int)(firstByte - 161) <= 62) {
				firstByte += 0xfec0;
				upperByte = 0xff;
			} else {
				upperByte = 0;
			}
			*lowByte = firstByte;
			*highByte = upperByte;
			return;
		}
		if (firstByte <= 159)
			row = firstByte * 2 - 225;
		else
			row = firstByte * 2 - 353;
	}
	if (secondByte > 158) {
		column = secondByte - 126;
		++row;
	} else {
		int adjustment;

		if (secondByte <= 127)
			adjustment = 31;
		else
			adjustment = 32;
		column = secondByte - adjustment;
	}
	row -= 33;
	if ((unsigned int)row > 119 || (unsigned int)(column - 33) > 93) {
		unicode = 0x25a0;
	} else {
		unicode = jisToUnicode[row][column - 33];
		if (!unicode) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(2, 15, 355, "Unknown code found, u == 0\n");
#endif
			unicode = 0x25a0;
		}
	}
	*lowByte = unicode;
	*highByte = unicode >> 8;
}

void _utoj(int codepage, unsigned int lowByte, int highByte,
	   unsigned char *firstByte, unsigned char *secondByte)
{
	SceUInt16 unicode;
	const SceUInt16 (*mapping)[94];
	int row;
	int column;
	SceUInt16 code;

	if (lowByte <= 128 && highByte == 0) {
		*firstByte = lowByte;
		*secondByte = 0;
		return;
	}
	row = 0;
	unicode = lowByte + (highByte << 8);
	if ((SceUInt16)(unicode + 159) <= 62) {
		*firstByte = unicode + 320;
		*secondByte = 0;
		return;
	}
	if (unicode == 0x25a0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 15, 386, "found unknown code (0x%04x)\n", unicode);
#endif
		goto unknownCharacter;
	}
	mapping = jisToUnicode;
	do {
		const SceUInt16 *character;

		column = (unsigned int)(row - 8) <= 6;
		if (row == 12)
			column = 0;
		if (column | ((unsigned int)(row - 88) <= 3))
			goto nextRow;
		character = *mapping;
		do {
			if (*character++ != unicode)
				goto nextColumn;
			if (row > 61) {
				row -= 62;
				code = 0xe040;
				if (row)
					code = ((row >> 1) << 8) - 8128;
			} else {
				code = 0x8140;
				if (row)
					code = ((row >> 1) << 8) - 32448;
			}
			if (row & 1)
				code += 95;
			else if (column > 62)
				++column;
			code += column;
			*firstByte = code >> 8;
			*secondByte = code;
			return;

nextColumn:
			++column;
		} while (column != 94);
nextRow:
		++row;
		++mapping;
	} while (row != 120);
unknownCharacter:
	*firstByte = 0x81;
	*secondByte = 0xa1;
}

void _1chartou(int codepage, int firstByte, int secondByte,
	      unsigned char *lowByte, unsigned char *highByte)
{
	const SceUInt16 *mapping;
	SceUInt16 unicode;

	if (secondByte) {
		*lowByte = 0xa0;
		*highByte = 0x25;
	} else if (firstByte <= 127) {
		*lowByte = firstByte;
		*highByte = 0;
	} else {
		if ((unsigned int)(codepage - 2) <= 15)
			mapping = singleByteDecodeTables[codepage - 2];
		else
			mapping = cp437ToUnicode;
		unicode = mapping[firstByte - 128];
		if (!unicode)
			unicode = 0x25a0;
		*lowByte = unicode;
		*highByte = unicode >> 8;
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 15, 761, "unicode=0x%02x\n", unicode);
#endif
	}
}

int sceFatfsCpInitTable(void)
{
	int intrState;
	SceUID memBlock;

	g_codepageMemBlock = 0;
	g_codepageBuffer = NULL;
	intrState = SceCpuForDriver_821FC0EE();
	g_codepage = 13;
	sceKernelCpuResumeIntr(intrState);
	memBlock = sceKernelAllocMemBlockForKernel("SceFatfsCPTable", 0x1020d006, 0xc000, NULL);
	if (memBlock < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 15, 193, "cannot create FPL (0x%x)\n", memBlock);
#endif
		return memBlock;
	}
	g_codepageMemBlock = memBlock;
	sceKernelGetMemBlockBase(memBlock, &g_codepageBuffer);
	g_koreanTable = g_codepageBuffer;
	g_gbkTable = g_codepageBuffer;
	g_big5Table = g_codepageBuffer;
	return 0;
}

int utf16ToCodepage(unsigned char *destination, int capacity,
		    const SceUInt16 *source)
{
	void (*convert)(int, unsigned int, int, unsigned char *, unsigned char *);
	unsigned char firstByte;
	unsigned char secondByte;
	unsigned char *output;
	unsigned int unicode;
	int intrState;
	int codepage;
	int count;

	intrState = SceCpuForDriver_821FC0EE();
	codepage = g_codepage;
	sceKernelCpuResumeIntr(intrState);
	switch (codepage) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 17:
		convert = _uto1char;
		break;
	case 13:
		convert = _utoj;
		break;
	case 14:
		convert = _unicodeToGbkKorean;
		break;
	case 15:
		convert = _unicodeToGbkKorean;
		break;
	case 16:
		convert = _unicodeToBig5;
		break;
	default:
		return 0x80010016;
	}
	if (capacity <= 0)
		return 0;
	count = 0;
	*destination = 0;
	unicode = *source;
	if (!unicode)
		return 0;
	output = destination;
	for (;;) {
		intrState = SceCpuForDriver_821FC0EE();
		codepage = g_codepage;
		sceKernelCpuResumeIntr(intrState);
		convert(codepage, (unsigned char)unicode, unicode >> 8,
			&firstByte, &secondByte);
		if (secondByte) {
			if (capacity - count <= 1)
				return count;
			count += 2;
			output[1] = secondByte;
			*output = firstByte;
		} else {
			++count;
			*output = firstByte;
		}
		output = &destination[count];
		if (count >= capacity)
			return count;
		destination[count] = 0;
		unicode = *++source;
		if (!unicode)
			return count;
	}
}

int codepageToUtf16(SceUInt16 *destination, int capacity,
		    const unsigned char *source)
{
	void (*convert)(int, int, int, unsigned char *, unsigned char *);
	unsigned char lowByte;
	unsigned char highByte;
	SceUInt16 *output;
	SceUInt16 *end;
	unsigned int leadingByte;
	unsigned int trailingByte;
	int firstByte;
	int secondByte;
	int codepage;
	int intrState;
	int count;

	intrState = SceCpuForDriver_821FC0EE();
	codepage = g_codepage;
	sceKernelCpuResumeIntr(intrState);
	switch (codepage) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 17:
		convert = _1chartou;
		break;
	case 13:
		convert = _jtou;
		break;
	case 14:
	case 15:
		convert = _gbkKoreanToUnicode;
		break;
	case 16:
		convert = _big5ToUnicode;
		break;
	default:
		return 0x80010016;
	}
	capacity /= 2;
	if (capacity <= 0)
		return 0;
	count = 0;
	*destination = 0;
	leadingByte = *source;
	if (!leadingByte)
		return 0;
	end = destination + capacity;
	output = destination + 1;
	for (;;) {
		intrState = SceCpuForDriver_821FC0EE();
		codepage = g_codepage;
		sceKernelCpuResumeIntr(intrState);
		switch (codepage) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 17:
			goto singleByte;
		case 14:
			if ((leadingByte != 5) & ((unsigned char)(leadingByte + 127) > 125))
				goto singleByte;
			trailingByte = source[1];
			if ((trailingByte - 64 > 62) & ((trailingByte ^ 128) > 126))
				goto singleByte;
			goto doubleByte;
		case 15:
			if ((leadingByte != 5) & ((unsigned char)(leadingByte + 127) > 124))
				goto singleByte;
			trailingByte = source[1];
			if ((trailingByte & ~32U) - 65 > 25 &&
			    (unsigned char)(trailingByte + 127) > 125)
				goto singleByte;
			goto doubleByte;
		case 16:
			if ((leadingByte != 5) & ((unsigned char)(leadingByte + 95) > 88))
				goto singleByte;
			trailingByte = source[1];
			if ((trailingByte - 64 > 62) &
			    ((unsigned char)(trailingByte + 95) > 93))
				goto singleByte;
			goto doubleByte;
		default:
			if ((unsigned char)(leadingByte + 32) > 28 &&
			    (unsigned char)(leadingByte + 127) > 30 && leadingByte != 5)
				goto singleByte;
			trailingByte = source[1];
			if ((trailingByte - 64 > 62) & ((trailingByte ^ 128) > 124))
				goto singleByte;
			goto doubleByte;
		}
singleByte:
		firstByte = *source++;
		secondByte = 0;
		goto convertCharacter;
doubleByte:
		secondByte = trailingByte;
		firstByte = *source;
		source += 2;
convertCharacter:
		intrState = SceCpuForDriver_821FC0EE();
		codepage = g_codepage;
		++count;
		sceKernelCpuResumeIntr(intrState);
		convert(codepage, firstByte, secondByte, &lowByte, &highByte);
		output[-1] = lowByte | (highByte << 8);
		if (output == end)
			return 2 * count;
		*output++ = 0;
		leadingByte = *source;
		if (!leadingByte)
			return 2 * count;
	}
}

int isCodepageDoubleByte(const unsigned char *source)
{
	unsigned int firstByte = *source;
	unsigned int secondByte;
	int intrState;
	int codepage;
	int result;

	intrState = SceCpuForDriver_821FC0EE();
	codepage = g_codepage;
	sceKernelCpuResumeIntr(intrState);
	switch (codepage) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 17:
		result = 0;
		break;
	case 14:
		result = (firstByte == 5) | ((unsigned char)(firstByte + 127) <= 125);
		if (result) {
			secondByte = source[1];
			result = secondByte - 64 <= 62 || (secondByte ^ 128) <= 126;
		}
		break;
	case 15:
		result = (firstByte == 5) | ((unsigned char)(firstByte + 127) <= 124);
		if (result) {
			secondByte = source[1];
			result = (secondByte & ~32U) - 65 <= 25 ||
				 (unsigned char)(secondByte + 127) <= 125;
		}
		break;
	case 16:
		result = (firstByte == 5) | ((unsigned char)(firstByte + 95) <= 88);
		if (result) {
			secondByte = source[1];
			result = secondByte - 64 <= 62 ||
				 (unsigned char)(secondByte + 95) <= 93;
		}
		break;
	default:
		result = ((unsigned char)(firstByte + 32) <= 28) |
			 ((unsigned char)(firstByte + 127) <= 30);
		if (result || firstByte == 5) {
			secondByte = source[1];
			result = secondByte - 64 <= 62 || (secondByte ^ 128) <= 124;
		}
		break;
	}
	return result;
}

int isUnicodeDoubleByte(unsigned int unicode)
{
	int intrState;
	int codepage;

	intrState = SceCpuForDriver_821FC0EE();
	codepage = g_codepage;
	sceKernelCpuResumeIntr(intrState);
	switch (codepage) {
	case 13: {
		SceUInt16 halfwidth = unicode + 158;

		--unicode;
		return ((unicode <= 125) | (halfwidth <= 60)) ^ 1;
	}
	case 14:
		return ((unicode == 0x20ac) || (unicode - 1 <= 125)) ^ 1;
	case 15:
	case 16:
		return unicode - 1 > 125;
	default:
		return 0;
	}
}
