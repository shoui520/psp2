/* SceAppMgr's debug console: text drawn into a framebuffer with an 8x16 font (debug_console.c). */
#pragma once

#include <scetypes.h>

/* appmgrDebugConsoleSetBuffer's pixel formats */
#define DEBUG_CONSOLE_FORMAT_RGB565	0
#define DEBUG_CONSOLE_FORMAT_RGBA5551	1
#define DEBUG_CONSOLE_FORMAT_RGBA4444	2
#define DEBUG_CONSOLE_FORMAT_RGBA8888	3

int appmgrDebugConsoleInitialize(void);
int appmgrDebugConsoleSetBuffer(void *buffer, SceUInt32 width, SceUInt32 height, int format);
void appmgrDebugConsoleSetForeColor(SceUInt32 color);
void appmgrDebugConsoleSetBackColor(SceUInt32 color);
void appmgrDebugConsoleSetCursor(SceUInt32 x, SceUInt32 y);
int appmgrDebugConsoleClear(void);
int appmgrDebugConsolePutChar(SceChar8 c);
int appmgrDebugConsolePuts(const SceChar8 *string);
int appmgrDebugConsoleFlush(void);
