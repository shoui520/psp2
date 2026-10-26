/*
 * The downloader's start at DOWNLOADER_ENTRY (0x2000), where the application's
 * updaterStartDownloader enters the updater: at a fixed address, apart from the rest of the
 * code (ernie/updater/updater.lk).
 */
#pragma sfr

#include <string.h>
#include "chip.h"
#include "ernie.h"
#include "downloader.h"
#include "rtc.h"
#include "sleepfactor.h"
#include "version.h"
#include "updater.h"

void mainLoop(void);

/* sleepfactor.c's, reached as far data */
extern union SleepFactor sleepFactor;
extern union SleepFactor sleepFactorRead;
extern union SleepFactorStatus sleepFactorStatus;

/*
 * the downloader's start (DOWNLOADER_ENTRY), from the application's updaterStartDownloader:
 * the stack reset, the sleep factor and host link it hands over in DOWNLOADER_START taken,
 * every interrupt masked, and the main loop entered as after an update (updReset 0)
 */
void downloaderStart(void)
{
#asm
	MOVW	SP, #0D500H
#endasm
	sleepFactorStatus.value = DOWNLOADER_START->snapshot;
	sleepFactor.bytes[0] = DOWNLOADER_START->sleepFactor[0];
	sleepFactor.bytes[1] = DOWNLOADER_START->sleepFactor[1];
	sleepFactor.bytes[2] = DOWNLOADER_START->sleepFactor[2];
	sleepFactor.bytes[3] = DOWNLOADER_START->sleepFactor[3];
	hostLink.frameSize = DOWNLOADER_START->frameSize;
	hostLink.config = DOWNLOADER_START->link.config;
	updReset = 0;
	memcpy(updVersion, IMAGE_VERSION_INFO->version, 4);
	timeStampDigits(updBuild, (const char __far *)(IMAGE_VERSION_INFO + 1));
	MK0 = 0xffff;
	MK1 = 0xffff;
	MK2 = 0xffff;
	IF0 = 0;
	IF1 = 0;
	IF2 = 0;
	rtcInit();
	sleepFactorRead = sleepFactor;
	mainLoop();
}
