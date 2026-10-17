/* BSOD's bounded-width circular debug log and screen viewport. */
#include "bsod_internal.h"
#include <sysmem/debug.h>

typedef int (*BsodPutchar)(unsigned tty, int character);
typedef void (*BsodPrinter)(const char *format, ...);
typedef struct BsodLog {
	int cursor;
	int wrapped;
	int column;
	int firstLine;
	BsodPutchar previous;
	int writeLine;
	int lines;
} BsodLog;

BsodLog s_bsodLog;
uint8_t s_bsodLogBuffer[1024][160];
BsodPrinter s_bsodPrinter;

static const char *bsodLogLine(int32_t index)
{
	/* a short log can have a negative cursor: that reads before the buffer */
	return (const char *)s_bsodLogBuffer[index];
}

int bsodLogPutchar(unsigned tty, int character)
{
	int line, column;

	if (s_bsodLog.cursor < 0) {
		if (character == 0x200) {
			s_bsodLog.column = 0;
		} else if (character != 0x201) {
			column = s_bsodLog.column;
			line = s_bsodLog.writeLine;
			if (character == '\n') {
				s_bsodLog.column = 0;
				s_bsodLogBuffer[line][column] = 0;
				s_bsodLog.writeLine = ++line;
				if (line >= 1024) {
					s_bsodLog.writeLine = 0;
					s_bsodLog.wrapped = 1;
					s_bsodLog.lines = 1024;
				}
				if (s_bsodLog.wrapped)
					s_bsodLog.firstLine = s_bsodLog.writeLine;
				else
					s_bsodLog.lines++;
			} else if (column <= 158) {
				s_bsodLog.column = column + 1;
				s_bsodLogBuffer[line][column] = character;
			} else {
				s_bsodLogBuffer[line][column] = 0;
			}
		}
	}
	if (s_bsodLog.previous)
		return s_bsodLog.previous(tty, character);
	return 0;
}

int bsodInitializePutchar(void)
{
	s_bsodLog.wrapped = 0;
	s_bsodLog.column = 0;
	s_bsodLog.writeLine = 0;
	s_bsodLog.lines = 0;
	s_bsodLog.firstLine = 0;
	s_bsodLog.cursor = -1;
	s_bsodLog.previous = (BsodPutchar)SceDebugForKernel_8D474850();
	SceDebugForKernel_22546577((uintptr_t)bsodLogPutchar, 0);
	return 0;
}

int bsodFreezeLog(void)
{
	s_bsodLog.cursor = s_bsodLog.lines - 33;
	return 0;
}

int bsodPrintLog(const void *frame, uint32_t buttons)
{
	int cursor = s_bsodLog.cursor, end;
	unsigned index;

	(void)frame;
	if (buttons & 1) {
		if (cursor >= 1)
			cursor--;
	} else if (buttons & 4) {
		if (cursor < s_bsodLog.lines - 33)
			cursor++;
	} else if (buttons & 0x18) {
		cursor = cursor < 33 ? 0 : cursor - 33;
	} else if (buttons & 0x42) {
		cursor += 33;
		if (s_bsodLog.lines - 33 < cursor)
			cursor = s_bsodLog.lines - 33;
		if (cursor < 0)
			cursor = 0;
	} else if (buttons & 0x20) {
		cursor = s_bsodLog.lines - 33;
		if (cursor < 0)
			cursor = 0;
	} else if (buttons & 0x80) {
		cursor = 0;
	}
	s_bsodLog.cursor = cursor;
	end = cursor + 33;
	for (; cursor < end && cursor <= s_bsodLog.lines; cursor++) {
		index = s_bsodLog.firstLine + cursor;
		if ((int)index > 1023)
			index &= 1023;
		s_bsodPrinter("%s\n", bsodLogLine((int32_t)index));
	}
	cursor = s_bsodLog.cursor;
	end = s_bsodLog.lines > 33 ? cursor + 33 : s_bsodLog.lines;
	s_bsodPrinter(" ---- [Log]   %d-%d of %d lines ----",
		      cursor ? cursor + 1 : 0, end, s_bsodLog.lines);
	return 0;
}
