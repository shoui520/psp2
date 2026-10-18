/* SceEnumWakeUp internals shared by its source files. */

#pragma once

#include <scetypes.h>
#include <sysmem/memblock.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <string.h>
#include "enum_wakeup.h"
#include <stddef.h>
#include <kernel/modulemgr.h>

#include "enum_wakeup_imports.h"
#include <mtpif/mtpif_driver.h>

#define EW_MEMTYPE_FB		SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW	/* uncached, physically contiguous */

/* main thread mode */
#define EW_MODE_MTP		0	/* MTP responder, display off */
#define EW_MODE_MTP_DISPLAY	1	/* MTP responder, display on (QA: Up held) */
#define EW_MODE_DISPLAY		2	/* blinking icon, no MTP */

/* power-off thread event flag */
#define EW_EVENT_POWER_PRESSED	0x001
#define EW_EVENT_POWER_RELEASED	0x002
#define EW_EVENT_UNPLUGGED	0x010
#define EW_EVENT_TIMER		0x020	/* MTP modes */
#define EW_EVENT_DISPLAY_TIMER	0x040
#define EW_EVENT_CHARGED	0x100
#define EW_EVENT_SHUTDOWN	0x200

#define EW_BUTTON_SELECT	0x00000001
#define EW_BUTTON_UP		0x00000010
#define EW_BUTTON_RIGHT		0x00000020
#define EW_BUTTON_DOWN		0x00000040
#define EW_BUTTON_LEFT		0x00000080
#define EW_BUTTON_START		0x00000008
#define EW_BUTTON_PS		0x00000400
#define EW_BUTTON_POWER		0x40000000

typedef struct EwButtons {
	SceUInt32 previous;
	SceUInt32 pressed;		/* went down since the last read */
	SceUInt32 reserved;
	SceUInt32 released;
	SceUInt32 current;
} EwButtons;

typedef struct EwState {
	void *fb[2];
	SceUID fbBlock;
	void *icon;			/* RGB, iconWidth x iconHeight */
	int iconWidth;
	int iconHeight;
	SceUID iconBlock;
	int mode;			/* EW_MODE_* */
	int clocks[4];			/* ARM, bus, GPU, GPU crossbar (MHz) */
	int brightness;
	int blank;			/* display off (Select toggles) */
	int connected;			/* USB host present */
	int charged;			/* EW_EVENT_CHARGED sent */
	int powerButton;		/* the last power button state */
	SceUInt32 powerMask;
	int powerCallback;
	int spinlock;
	SceUID evf;			/* EW_EVENT_* */
	SceUID timer;
	SceUInt32 motionFlags;
	int haveBrightness;
	int (*setBrightness)(int brightness);
	int (*displayOn)(void);
} EwState;

typedef struct EwDrawTarget {
	int width;
	int pitch;
	SceUInt32 *base;
	int height;
} EwDrawTarget;

/* text console state, initialised for the 8x16 font (nothing prints) */
typedef struct EwConsole {
	int reserved0;			/* zeroed by the set up, never read */
	int width;
	int height;
	int reserved1;
	int glyphWidth;
	int glyphHeight;
	const SceUInt8 *font;
	int glyphBytes;
	int columns;
	int rows;
	int x;
	int y;
	int visible;
	int reserved2;
	int scale;
	SceUInt32 colors[16];		/* [0] background, [1] text */
} EwConsole;

extern const SceUInt8 ewIconPpm[];		/* icon.ppm */
extern const SceUInt8 ewFont8x16[];		/* font8x16.c */
extern const MtpDeviceInfo ewDeviceInfo;
extern const MtpPropDesc ewPropDescs[2];


/* the module's data in Sony's order: a file cache miss uses the word after the
   cache, so the layout matters */
typedef struct EwGlobals {
	EwCtrlData ctrl[8];
	MtpContext mtp;
	SceUInt32 cacheMiss;			/* cache[MTP_CACHE_ENTRIES] starts here */
	char version[16];
	EwButtons buttons;
	SceUInt32 reserved0;
	EwState ew;
	EwDrawTarget draw;
	MtpDeviceStrings strings;
	MtpDeviceStringsProvider provider;
	SceUID heap;
	char serial[0x40];
	SceUInt32 reserved1[16];
	SceAllocOpt allocOpt;
	SceUInt32 reserved2;
	char text[2][0x1540];
	EwConsole console;
} EwGlobals;

_Static_assert(offsetof(EwGlobals, ew) == 0x1280, "SceEnumWakeUp state");
_Static_assert(offsetof(EwGlobals, text) == 0x1a98, "SceEnumWakeUp text");
_Static_assert(offsetof(EwGlobals, console) + sizeof(EwConsole) == 0x4594, "SceEnumWakeUp BSS");

extern EwGlobals ewg;

#define MTP_DEVICE_INFO_MAX	0x2000

/* an MTP string being packed: its length in characters, then UTF-16 */
typedef struct MtpString {
	SceUInt8 length;			/* in: room in characters */
	void *buf;				/* UTF-16, at any byte offset */
} MtpString;

/* draw.c */
int ewDrawSetTarget(SceUInt32 *base, int width, int height, int pitch);
int ewDrawClear(SceUInt32 color);
int ewDrawPixel(SceUInt32 color, int x, int y);

/* console.c */
int ewConsoleInit(void);

/* MTP responder */
int mtpUtf8ToUtf16(SceUInt16 *dst, SceSize size, const SceUInt8 *src);
int mtpGetDeviceInfo(MtpContext *ctx);
int mtpOpenSession(MtpContext *ctx);
int mtpCloseSession(MtpContext *ctx);
int mtpGetDevicePropDesc(MtpContext *ctx);
int mtpThread(SceSize args, void *argp);
int mtpStart(int portId, MtpDeviceStringsProvider provider);
int mtpInit(void);
int mtpIsSessionOpen(const MtpContext *ctx);
int mtpBuildPropDesc(SceUInt32 code, char *buf, SceSize size);
void mtpSetupDeviceStrings(MtpDeviceStringsProvider provider);
void mtpClearDeviceStrings(void);
const char *mtpSyncPartner(void);
const char *mtpIconPath(void);
const char *mtpFriendlyName(void);
const MtpDeviceInfo *mtpDeviceInfo(void);
const MtpPropDesc *mtpFindPropDesc(SceUInt32 code);
int mtpHeapCreate(SceSize size);
int mtpHeapDelete(void);
void *mtpAlloc(SceSize size);
void mtpFree(void *p);
char *mtpStrcpy(char *dst, const char *src);
int mtpSetResponse(SceUInt16 code, const SceUInt32 *params, SceUInt32 nparams,
		   const MtpContainer *request, MtpContainer *response);
int mtpPackString(MtpString *out, const char *str);
int mtpPackArray8(void *out, const MtpArray *in);
int mtpPackArray16(void *out, const MtpArray *in);
int mtpPackArray32(void *out, const MtpArray *in);
int mtpPackArray64(void *out, const MtpArray *in);
int mtpReadSerial(MtpDeviceStrings *out);
int mtpCacheLoad(const char *path, SceSize size);
int mtpCacheRead(const char *path, void *dst, SceSize max);
int mtpXferThread(SceSize args, void *argp);
int mtpXferInit(MtpContext *ctx, MtpXfer **out);

/* enum_wakeup.c */
void ewPowerButton(uint32_t state, uint32_t previous, void *argument);
int ewPowerOffThread(SceSize args, void *argp);
int ewTimer(void);
int ewNoDisplay(void);
void ewInitDisplay(void);
int ewMainThread(SceSize args, void *argp);
int ewFillDeviceStrings(MtpProvidedStrings *strings, int reserved);
