/*
 * uart_debug.c — Pervasive UART Console & POST LED Debug Output
 *
 * Address range: 0x808a44..0x808e3d
 *
 * Drives the pervasive UART controllers at 0xe2030000 (port 0) and 0xe2040000
 * (port 1) and the 8-bit GPIO POST code LED bank on GPIO0 pins 16..23, and
 * formats diagnostic status codes during boot.
 */

#include "second_loader.h"

/* .rodata: 0x80c408..0x80c42f */
static const char s_uartSyncBanner[4] = "UUU";
static const char s_uartSepSpace[4] = " ";
static const char s_uartSepColon[4] = ": ";
static const char s_uartSepComma[4] = ", ";
static const char s_uartNewline[4] = "\n";
static const char s_hexDigits[20] = "0123456789abcdef";

/* .data: 0x80ccb4..0x80ccc3 */
static u32 s_uartDebugInitialized = 0;
static u32 s_uartDebugFlags = 0;
static u32 s_uartDebugRadix = 10;
static u32 s_uartDebugStream = 0;

/* .sdata: 0x80cd1c */
int (*g_uartPutcharCallback)(int ch) __attribute__((section(".sdata"))) = uartDebugPutchar;

/* .bss: 0x80d790..0x80d7a7 */
static u32 s_uartPortEnabled[2];
static u32 s_uartPortHwInitialized[2];
static u32 s_gpioPostLedInitialized;
static u32 s_gpioPostLedEnabled;

/* 0x808a44: uartPortPutc (name ours) */
void uartPortPutc(u32 port, u8 ch)
{
	volatile u8 *uartBase;
	u32 tries;

	if (s_uartPortEnabled[port] == 0)
		return;

	if (s_uartPortHwInitialized[port] == 0) {
		volatile u32 *clkEnReg = (volatile u32 *)(0xe3102120U + port * 4U);
		volatile u32 *rstReg = (volatile u32 *)(0xe3101120U + port * 4U);
		volatile u32 *baudReg = (volatile u32 *)(0xe3105000U + port * 4U);

		*clkEnReg = 1;
		while (*clkEnReg == 0)
			;

		*rstReg = 0;
		while (*rstReg != 0)
			;

		uartBase = (volatile u8 *)(0xe2030000U + port * 0x10000U);
		*(volatile u32 *)(uartBase + 0x64) = 0x101U;
		while (*(volatile u32 *)(uartBase + 0x64) != 0)
			;

		*(volatile u32 *)(uartBase + 0x10) = 1;
		*baudReg = 0x1001aU;
		*(volatile u32 *)(uartBase + 0x20) = 3;
		*(volatile u32 *)(uartBase + 0x40) = 0;
		*(volatile u32 *)(uartBase + 0x50) = 0x77fU;
		*(volatile u32 *)(uartBase + 0x30) = 0;
		*(volatile u32 *)(uartBase + 0x60) = 0;
		*(volatile u32 *)(uartBase + 0x04) = 1;
		s_uartPortHwInitialized[port] = 1;
	}

	uartBase = (volatile u8 *)(0xe2030000U + port * 0x10000U);
	for (tries = 3300; tries != 0; tries--) {
		if ((*(volatile u32 *)(uartBase + 0x28) & 0x100U) != 0) {
			*(volatile u32 *)(uartBase + 0x70) = ch;
			return;
		}
	}
}

/* 0x808b18: uartDebugPutchar (name ours) */
int uartDebugPutchar(int ch)
{
	u8 byte = (u8)ch;

	if (byte == '\n')
		uartPortPutc(0, '\r');
	uartPortPutc(0, byte);

	if (byte == '\n')
		uartPortPutc(1, '\r');
	uartPortPutc(1, byte);

	return ch;
}

/* 0x808b54: uartEnablePort (name ours) */
void uartEnablePort(u32 port)
{
	if (port < 2)
		s_uartPortEnabled[port] = 1;
}

/* 0x808b66: uartDisablePort (name ours) */
void uartDisablePort(u32 port)
{
	if (port < 2)
		s_uartPortEnabled[port] = 0;
}

/* 0x808b7a: debugEnablePostLeds (name ours) */
void debugEnablePostLeds(void)
{
	s_gpioPostLedEnabled = 1;
}

/* 0x808b82: debugArePostLedsEnabled (name ours) */
u32 debugArePostLedsEnabled(void)
{
	return s_gpioPostLedEnabled;
}

/* 0x808b88: debugInitPostLedGpios (name ours) */
void debugInitPostLedGpios(void)
{
	if (s_gpioPostLedInitialized == 0) {
		gpioSetPortDirection(0, 16, 0);
		gpioSetPortDirection(0, 17, 0);
		gpioSetPortDirection(0, 18, 0);
		gpioSetPortDirection(0, 19, 0);
		gpioSetPortDirection(0, 20, 0);
		gpioSetPortDirection(0, 21, 0);
		gpioSetPortDirection(0, 22, 0);
		gpioSetPortDirection(0, 23, 0);
		s_gpioPostLedInitialized = 1;
	}
}

/* 0x808bee: debugWritePostLedCode (name ours) */
void debugWritePostLedCode(u8 code)
{
	if (s_gpioPostLedEnabled != 0) {
		debugInitPostLedGpios();
		MMIO_W32(0xe20a000cU, 0x00ff0000U);
		MMIO_W32(0xe20a0008U, (u32)code << 16);
	}
}

/* 0x808c24: uartDebugSetFlags (name ours) */
void uartDebugSetFlags(u32 flags)
{
	s_uartDebugFlags = flags;
}

/* 0x808c2a: uartDebugInit (name ours) */
int uartDebugInit(void)
{
	int res;

	if (s_uartDebugInitialized == 0) {
		if (sysconGetHardwareVersion() < 0x90004U)
			return 0x800f0025;
		if ((qafGetCachedLogEnableFlag() & 0xffU) == 0xffU)
			return SCE_SBL_ERROR_SL_EINVAL;

		res = sysconEnableUartPower(1);
		if ((qafGetCachedLogEnableFlag() & 0xffU) == 1U && res != 0) {
			qafSetCachedLogEnableFlag(0xff);
			return res;
		}

		uartEnablePort(0);
		s_uartDebugInitialized = 1;
		if ((s_uartDebugFlags & 8U) != 0)
			uartDebugPuts(s_uartSyncBanner);
	}
	return 0;
}

/* 0x808caa: uartDebugReportEvent (name ours) */
void uartDebugReportEvent(u32 category, u32 subcode, u32 errCode, u32 extra)
{
	if (extra == 0) {
		uartDebugSetRadix(&s_uartDebugStream, 16);
		uartDebugPrintU32(category & 0xffU);
		uartDebugPuts(s_uartSepSpace);
		uartDebugPrintU32(subcode & 0xffU);
		uartDebugPuts(s_uartSepColon);
		extra = errCode;
	} else {
		uartDebugSetRadix(&s_uartDebugStream, 16);
		uartDebugPrintU32(category & 0xffU);
		uartDebugPuts(s_uartSepSpace);
		uartDebugPrintU32(subcode & 0xffU);
		uartDebugPuts(s_uartSepColon);
		uartDebugPrintU32(errCode);
		uartDebugPuts(s_uartSepComma);
	}
	uartDebugPrintU32(extra);
	uartDebugPuts(s_uartNewline);
}

/* 0x808d40: uartDebugSetRadix (name ours) */
void uartDebugSetRadix(u32 *stream, u32 radix)
{
	(void)stream;
	s_uartDebugRadix = radix;
}

/* 0x808d46: uartDebugPrintU32 (name ours) */
void uartDebugPrintU32(u32 val)
{
	uartDebugFormatUint(val, s_uartDebugRadix, 0);
}

/* 0x808d54: uartDebugWrite (name ours) */
void uartDebugWrite(const char *buf, u32 len)
{
	for (; len != 0; len--) {
		uartDebugPutchar(*buf++);
	}
}

/* 0x808d7a: uartDebugPutc (name ours) */
void uartDebugPutc(char ch)
{
	char tmp = ch;

	uartDebugWrite(&tmp, 1);
}

/* 0x808d92: uartDebugPuts (name ours) */
void uartDebugPuts(const char *str)
{
	u32 len = strnlen(str, 0x40);

	uartDebugWrite(str, len);
}

/* 0x808db2: uartDebugFormatUint (name ours) */
void uartDebugFormatUint(u32 val, u32 radix, int width)
{
	char digitBuf[12];
	char *p;
	int padCount;

	for (padCount = width - 8; padCount > 0; padCount--)
		uartDebugPutc('0');

	if (width < 1) {
		if (val == 0) {
			uartDebugPutc('0');
			return;
		}
	} else if (width > 8) {
		width = 8;
	}

	p = digitBuf + sizeof(digitBuf);
	while (width > 0 || val != 0) {
		u32 rem = val % radix;

		*--p = s_hexDigits[rem];
		val /= radix;
		width--;
	}

	uartDebugWrite(p, (u32)((digitBuf + sizeof(digitBuf)) - p));
}
