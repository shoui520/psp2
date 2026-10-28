/*
 * pspbtcnf.c: ePSP 6.61 boot configuration table (pspbtcnf.bin, 2,724 bytes)
 * parsed by _loadprx_by_pspbtcnf_bin in sys661/kernel/src/loadcore/loadelf_boot.c.
 */

#include <stddef.h>
#include "boot_config.h"

#define PSP_BOOTCONFIG_MAGIC	0x0f803001u

/* Boot mode flags */
#define BTCNF_MODE_VSH		0x00000008u
#define BTCNF_MODE_UMDEMU	0x00000040u

/* Module load attributes */
#define BTCNF_LOAD_KERNEL	0x00010000u
#define BTCNF_LOAD_USER		0x00020000u
#define BTCNF_LOAD_ANCHOR	0x00040000u
#define BTCNF_ENCRYPTED		0x80000000u

typedef struct PspBootConfigStrings {
	char empty[1];
	char sysmem[sizeof("/kd/sysmem.prx")];
	char loadcore[sizeof("/kd/loadcore.prx")];
	char exceptionman[sizeof("/kd/exceptionman.prx")];
	char interruptman[sizeof("/kd/interruptman.prx")];
	char threadman[sizeof("/kd/threadman.prx")];
	char kermit[sizeof("/kd/kermit.prx")];
	char dmacman[sizeof("/kd/dmacman.prx")];
	char systimer[sizeof("/kd/systimer.prx")];
	char iofilemgr[sizeof("/kd/iofilemgr.prx")];
	char memlmd_01g[sizeof("/kd/memlmd_01g.prx")];
	char modulemgr[sizeof("/kd/modulemgr.prx")];
	char init[sizeof("/kd/init.prx")];
	char loadexec_01g[sizeof("/kd/loadexec_01g.prx")];
	char kermit_lowio[sizeof("/kd/kermit_lowio.prx")];
	char ge[sizeof("/kd/ge.prx")];
	char kermit_peripheral[sizeof("/kd/kermit_peripheral.prx")];
	char kermit_idstorage[sizeof("/kd/kermit_idstorage.prx")];
	char kermit_rtc[sizeof("/kd/kermit_rtc.prx")];
	char kermit_display[sizeof("/kd/kermit_display.prx")];
	char kermit_ctrl[sizeof("/kd/kermit_ctrl.prx")];
	char kermit_flashfs[sizeof("/kd/kermit_flashfs.prx")];
	char kermit_msfs[sizeof("/kd/kermit_msfs.prx")];
	char kermit_power[sizeof("/kd/kermit_power.prx")];
	char kermit_msemu[sizeof("/kd/kermit_msemu.prx")];
	char kermit_audio[sizeof("/kd/kermit_audio.prx")];
	char kermit_hpremote[sizeof("/kd/kermit_hpremote.prx")];
	char openpsid[sizeof("/kd/openpsid.prx")];
	char kermit_usb[sizeof("/kd/kermit_usb.prx")];
	char kermit_wlan[sizeof("/kd/kermit_wlan.prx")];
	char registry[sizeof("/kd/registry.prx")];
	char mesg_led_01g[sizeof("/kd/mesg_led_01g.prx")];
	char amctrl[sizeof("/kd/amctrl.prx")];
	char npdrm[sizeof("/kd/npdrm.prx")];
	char iofilemgr_dnas[sizeof("/kd/iofilemgr_dnas.prx")];
	char np9660[sizeof("/kd/np9660.prx")];
	char isofs[sizeof("/kd/isofs.prx")];
	char kermit_utility[sizeof("/kd/kermit_utility.prx")];
	char kermit_me_wrapper[sizeof("/kd/kermit_me_wrapper.prx")];
	char impose_kermit[sizeof("/kd/impose_kermit.prx")];
	char mediasync[sizeof("/kd/mediasync.prx")];
	char exitgame[sizeof("/kd/exitgame.prx")];
	char usersystemlib[sizeof("/kd/usersystemlib.prx")];
	char dummy_anchor[sizeof("/kd/dummy_anchor_IhariUafaayk98.prx")];
	char chkreg[sizeof("/kd/chkreg.prx")];
	char chnnlsv[sizeof("/kd/chnnlsv.prx")];
	char popsman[sizeof("/kd/popsman.prx")];
	char libfont_hv[sizeof("/vsh/module/libfont_hv.prx")];
	char pafmini[sizeof("/vsh/module/pafmini.prx")];
	char common_util[sizeof("/vsh/module/common_util.prx")];
	char libpspvmc[sizeof("/vsh/module/libpspvmc.prx")];
} PspBootConfigStrings;

typedef struct PspBootConfigImage {
	PspBootConfigHeader header;
	PspBootConfigMode modes[2];
	PspBootConfigModule modules[50];
	PspBootConfigStrings strings;
} PspBootConfigImage;

#define MOD_ENTRY(field, flagsVal) \
	{ { .nameOffset = offsetof(PspBootConfigStrings, field) }, 0u, (flagsVal), 0u, { 0 } }

const PspBootConfigImage g_pspbtcnf __attribute__((section(".rodata.pspbtcnf"), used)) = {
	.header = {
		.signature		= PSP_BOOTCONFIG_MAGIC,
		.devkit			= 0x06060110u,
		.reservedSignature	= { 0x6b8b4567u, 0x327b23c6u },
		.modeStart		= offsetof(PspBootConfigImage, modes),
		.numModes		= 2u,
		.reservedModes		= { 0x643c9869u, 0x66334873u },
		.moduleStart		= offsetof(PspBootConfigImage, modules),
		.numModules		= 50u,
		.reservedModules	= { 0x74b0dc51u, 0x19495cffu },
		.moduleNameStart	= offsetof(PspBootConfigImage, strings),
		.moduleNameEnd		= sizeof(PspBootConfigImage),
		.reservedNames		= { 0x2ae8944au, 0x625558ecu },
	},
	.modes = {
		{ 50u, 0u, BTCNF_MODE_UMDEMU, 7u, { .configNameOffset = 0u }, 0u, 0u, 0u, 0u },
		{ 50u, 0u, BTCNF_MODE_VSH,    4u, { .configNameOffset = 0u }, 0u, 0u, 0u, 0u },
	},
	.modules = {
		MOD_ENTRY(sysmem,            BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(loadcore,          BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(exceptionman,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(interruptman,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(threadman,         BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit,            BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(dmacman,           BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(systimer,          BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(iofilemgr,         BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(memlmd_01g,        BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(modulemgr,         BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(init,              BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(loadexec_01g,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_lowio,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(ge,                BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_peripheral, BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_idstorage,  BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_rtc,        BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_display,    BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_ctrl,       BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_flashfs,    BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_msfs,       BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_power,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_msemu,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_audio,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU),
		MOD_ENTRY(kermit_hpremote,   BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(openpsid,          BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_usb,        BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU),
		MOD_ENTRY(kermit_wlan,       BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU),
		MOD_ENTRY(registry,          BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(chkreg,            BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_VSH),
		MOD_ENTRY(mesg_led_01g,      BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(amctrl,            BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(npdrm,             BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(iofilemgr_dnas,    BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(chnnlsv,           BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_VSH),
		MOD_ENTRY(np9660,            BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU),
		MOD_ENTRY(isofs,             BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU),
		MOD_ENTRY(kermit_utility,    BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(popsman,           BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_VSH),
		MOD_ENTRY(kermit_me_wrapper, BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU),
		MOD_ENTRY(impose_kermit,     BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(mediasync,         BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(exitgame,          BTCNF_ENCRYPTED | BTCNF_LOAD_KERNEL | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(usersystemlib,     BTCNF_ENCRYPTED | BTCNF_LOAD_USER   | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
		MOD_ENTRY(libfont_hv,        BTCNF_ENCRYPTED | BTCNF_LOAD_USER   | BTCNF_MODE_VSH),
		MOD_ENTRY(pafmini,           BTCNF_ENCRYPTED | BTCNF_LOAD_USER   | BTCNF_MODE_VSH),
		MOD_ENTRY(common_util,       BTCNF_ENCRYPTED | BTCNF_LOAD_USER   | BTCNF_MODE_VSH),
		MOD_ENTRY(libpspvmc,         BTCNF_ENCRYPTED | BTCNF_LOAD_USER   | BTCNF_MODE_VSH),
		MOD_ENTRY(dummy_anchor,      BTCNF_LOAD_ANCHOR | BTCNF_MODE_UMDEMU | BTCNF_MODE_VSH),
	},
	.strings = {
		"",
		"/kd/sysmem.prx",
		"/kd/loadcore.prx",
		"/kd/exceptionman.prx",
		"/kd/interruptman.prx",
		"/kd/threadman.prx",
		"/kd/kermit.prx",
		"/kd/dmacman.prx",
		"/kd/systimer.prx",
		"/kd/iofilemgr.prx",
		"/kd/memlmd_01g.prx",
		"/kd/modulemgr.prx",
		"/kd/init.prx",
		"/kd/loadexec_01g.prx",
		"/kd/kermit_lowio.prx",
		"/kd/ge.prx",
		"/kd/kermit_peripheral.prx",
		"/kd/kermit_idstorage.prx",
		"/kd/kermit_rtc.prx",
		"/kd/kermit_display.prx",
		"/kd/kermit_ctrl.prx",
		"/kd/kermit_flashfs.prx",
		"/kd/kermit_msfs.prx",
		"/kd/kermit_power.prx",
		"/kd/kermit_msemu.prx",
		"/kd/kermit_audio.prx",
		"/kd/kermit_hpremote.prx",
		"/kd/openpsid.prx",
		"/kd/kermit_usb.prx",
		"/kd/kermit_wlan.prx",
		"/kd/registry.prx",
		"/kd/mesg_led_01g.prx",
		"/kd/amctrl.prx",
		"/kd/npdrm.prx",
		"/kd/iofilemgr_dnas.prx",
		"/kd/np9660.prx",
		"/kd/isofs.prx",
		"/kd/kermit_utility.prx",
		"/kd/kermit_me_wrapper.prx",
		"/kd/impose_kermit.prx",
		"/kd/mediasync.prx",
		"/kd/exitgame.prx",
		"/kd/usersystemlib.prx",
		"/kd/dummy_anchor_IhariUafaayk98.prx",
		"/kd/chkreg.prx",
		"/kd/chnnlsv.prx",
		"/kd/popsman.prx",
		"/vsh/module/libfont_hv.prx",
		"/vsh/module/pafmini.prx",
		"/vsh/module/common_util.prx",
		"/vsh/module/libpspvmc.prx",
	},
};

_Static_assert(sizeof(PspBootConfigImage) == 0xaa4, "pspbtcnf.bin size");
