/* Fatal screen loop: real display changes, input edges and reset/power SMCs. */
#include "sce/cp15.h"
#include <sce/moncall.h>
#include "bsod_internal.h"
#include "report_data.h"

#include "exception_record.h"
extern void (*s_bsodPrinter)(const char *format, ...);
void ksceKernelSpinlockLowLock(int *lock);
int ksceKernelDisableIntr(unsigned interrupt);
void *ksceSysrootGetSysroot(void);
void bsodPrintf(const char *format, ...);
int bsodFreezeLog(void);
int bsodPrintLog(const void *frame, uint32_t buttons);
void bsodSetFont(unsigned width, unsigned height);
void bsodSetFramebuffer(void *pixels, unsigned pitch, unsigned height, unsigned format);
void bsodResetColors(void);
void bsodSetBackground(uint32_t color);
void bsodClearGrid(void);
void bsodRasterize(void);
uint32_t bsodReadButtons(void);
void bsodPowerOff(void);

uint32_t s_bsodButtons;

static void bsodReboot(unsigned dump)
{
	void *sysroot = ksceSysrootGetSysroot();

	moncall4(MONCALL_CRASH_DUMP, dump, readTtbr0(), readVbar(), (uintptr_t)sysroot);
}

void bsodCrashHandler(int kind)
{
	uint32_t buttons, edge;
	unsigned page = kind ? 0 : 3;

	ksceKernelSpinlockLowLock(&s_bsodDisplay.lock);
	ksceKernelDisableIntr(0xd5);
	ksceKernelDisableIntr(0xd2);
	ksceKernelDisableIntr(0xf8);
	bsodFreezeLog();
	s_bsodPrinter = bsodPrintf;
	bsodReadFramebuffer(0, &s_bsodDisplay.previous);
	if (!s_bsodDisplay.previous.physical)
		bsodReadFramebuffer(1, &s_bsodDisplay.previous);
	bsodWriteFramebuffer(1, &s_bsodDisplay.current);
	bsodWriteFramebuffer(0, &s_bsodDisplay.previous);
	bsodSetFont(6, 16);
	bsodSetFramebuffer(s_bsodDisplay.pixels, 960, 544, 3);
	s_bsodButtons = bsodReadButtons();
	for (;;) {
		bsodWriteFramebuffer(1, &s_bsodDisplay.current);
		bsodWriteFramebuffer(0, &s_bsodDisplay.previous);
		buttons = bsodReadButtons();
		edge = buttons & ~s_bsodButtons;
		s_bsodButtons = buttons;
		if (edge & 0x400) {
			page = page == 3 ? 0 : page + 1;
		} else if (edge & 0x200) {
			page = page ? page - 1 : 3;
		} else if (edge & 0x100) {
			bsodResetColors();
			bsodSetBackground(0xc0400000);
			for (;;) {
				bsodClearGrid();
				bsodPrintf(s_bsodTextMenuSpacing);
				bsodPrintf(s_bsodTextMenuCross);
				bsodPrintf(s_bsodTextMenuSelect);
				bsodPrintf(s_bsodTextMenuStart);
				bsodPrintf(s_bsodTextMenuNewline);
				bsodPrintf(s_bsodTextMenuStandby);
				bsodRasterize();
				buttons = bsodReadButtons();
				edge = buttons & ~s_bsodButtons;
				s_bsodButtons = buttons;
				if (edge & 0x160)
					break;
				if (edge & 0x800) {
					bsodReboot(s_bsodException.excpCode == 0);
					for (;;) {}
				}
				if (edge & 0x4000) {
					bsodPowerOff();
					for (;;) {}
				}
			}
			bsodClearGrid();
			continue;
		}
		bsodResetColors();
		bsodSetBackground(buttons & 0x1000 ? 0 : 0xc0400000);
		bsodClearGrid();
		switch (page) {
		case 0: bsodReportRegisters(&s_bsodException); break;
		case 1: bsodReportStack(&s_bsodException, 0); break;
		case 2: bsodReportStack(&s_bsodException, 1); break;
		case 3: bsodPrintLog(&s_bsodException, edge); break;
		}
		bsodRasterize();
	}
}

void bsodDebugCrashCallback(void)
{
	bsodCrashHandler(1);
}
