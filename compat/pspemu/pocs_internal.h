/*
 * pocs_internal.h: shared PocketStation emulator hardware state and
 * cross-file internal interfaces for ScePspemu.
 */
#ifndef PSPEMU_POCS_INTERNAL_H
#define PSPEMU_POCS_INTERNAL_H

#include "host_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POCS_BIOS_ROM_SIZE          0x4000U
#define POCS_INTERNAL_RAM_SIZE      0x0800U
#define POCS_FLASH_SIZE             0x20000U
#define POCS_VMP_HEADER_SIZE        0x0080U
#define POCS_VMP_IMAGE_SIZE         0x20080U
#define POCS_MCX_FILE_SIZE          0x200A0U

typedef struct PocsSchedEvent PocsSchedEvent;
typedef PocsSchedEvent *(*PocsSchedCallback)(PocsSchedEvent *event);

struct PocsSchedEvent {
	PocsSchedEvent   *next;
	PocsSchedEvent   *prev;
	int32_t           targetCycle;
	PocsSchedCallback callback;
};

_Static_assert(sizeof(PocsSchedEvent) == 16U, "PocsSchedEvent size");

typedef struct PocsTimer {
	PocsSchedEvent event;
	uint16_t       reloadValue;
	uint16_t       currentValue;
	uint16_t       irqMask;
	uint8_t        prescaleShift;
	uint8_t        control;
	uint8_t        padBytes[8];
} PocsTimer;

_Static_assert(sizeof(PocsTimer) == 32U, "PocsTimer size");

/*
 * Complete PocketStation machine state (0x81067C30..0x810688B7, 0xC88 bytes).
 */
typedef struct PocsMachineState {
	/* +0x000: ARM7TDMI core register file and banked state */
	uint32_t       regs[16];
	uint32_t       cpsr;
	uint32_t       bankedRegs[18];
	uint32_t       spsrBanked[2];
	uint32_t       nibbleMaskTable[16];

	/* +0x0D4: 3 hardware timers */
	PocsTimer      timers[3];

	/* +0x134: cycle-sorted event scheduler */
	PocsSchedEvent schedHead;
	int32_t        schedTargetCycle;
	int32_t        cpuCyclesLeft;

	/* +0x14C: clock / system ID / flash / display config */
	uint8_t        clockShift;
	uint8_t        clockMode;
	uint16_t       sysCtrl;
	uint32_t       serialId;
	uint8_t        flashBankMap[16];
	int8_t         flashPhysMap[16];
	uint16_t       flashReg;
	uint8_t        flashMode;
	uint8_t        lcdRefreshRate;
	uint8_t        lcdContrast;
	uint8_t        lcdDirtyFlags;

	/* +0x17A: audio DAC timing and control */
	uint16_t       audioSampleLatched;
	uint32_t       audioLastCycle;
	int16_t        audioCurrentSample;
	uint16_t       audioWriteIdx;
	uint8_t        audioEnabled;
	uint8_t        spiBusyFlag;
	uint8_t        inputSuppress;
	uint8_t        vmcInserted;

	/* +0x188: clock controller and RTC */
	PocsSchedEvent clockEvent;
	uint8_t        clockPendingReg;
	uint8_t        clockStatusReg;
	uint8_t        rtcControl;
	uint8_t        rtcAdjustStep;
	PocsSchedEvent rtcEvent;
	uint32_t       rtcDateBcd;
	uint32_t       rtcTimeBcd;

	/* +0x1B4: IRDA controller */
	PocsSchedEvent irdaEvent;
	uint8_t        irdaRxLevel;
	uint8_t        irdaControl;

	/* +0x1C6: LCD controller and framebuffers */
	uint8_t        lcdControl;
	uint8_t        lcdLine;
	PocsSchedEvent lcdEvent;
	uint32_t       lcdVramFront[32];
	uint32_t       lcdVramBack[32];
	uint32_t       lcdLineHistory[32];
	uint32_t       lcdShadowBuf[32];

	/* +0x3D8: LED / RTC sync / debounce / SPI / GPIO / INTC / battery */
	uint8_t        ledNotifyFlag;
	uint8_t        ledState;
	uint8_t        rtcNeedsSync;
	uint8_t        buttonDebounce;
	uint8_t        spiReservedByte;
	uint8_t        spiRxByte;
	volatile SceInt8 spiActive;
	uint8_t        spiCmdFlag;
	uint8_t        spiCtrl;
	uint8_t        spiStatus;
	uint8_t        spiTxByte;
	uint8_t        spiShiftReg;
	uint8_t        spiPhase;
	uint8_t        gpioDir;
	uint8_t        gpioData;
	volatile SceInt8 saveCountdown;
	uint16_t       intcEnable;
	uint16_t       intcRawStatus;
	uint16_t       intcLatch;
	uint8_t        batteryLevel;
	uint8_t        standaloneActive;

	/* +0x3F0: flash memory pointer, CPU PC, flash page buffer */
	uint8_t       *flashDataPtr;
	uint32_t       cpuPc;
	uint8_t        flashPageBuf[128];
	PocsSchedEvent flashEvent;

	/* +0x488: 2 KiB PocketStation internal SRAM (0x00000000..0x000007FF) */
	uint8_t        internalRam[POCS_INTERNAL_RAM_SIZE];
} PocsMachineState;

_Static_assert(offsetof(PocsMachineState, nibbleMaskTable) == 0x094U, "nibbleMaskTable offset");
_Static_assert(offsetof(PocsMachineState, timers) == 0x0D4U, "timers offset");
_Static_assert(offsetof(PocsMachineState, schedHead) == 0x134U, "schedHead offset");
_Static_assert(offsetof(PocsMachineState, schedTargetCycle) == 0x144U, "schedTargetCycle offset");
_Static_assert(offsetof(PocsMachineState, clockShift) == 0x14CU, "clockShift offset");
_Static_assert(offsetof(PocsMachineState, serialId) == 0x150U, "serialId offset");
_Static_assert(offsetof(PocsMachineState, audioSampleLatched) == 0x17AU, "audioSampleLatched offset");
_Static_assert(offsetof(PocsMachineState, vmcInserted) == 0x187U, "vmcInserted offset");
_Static_assert(offsetof(PocsMachineState, rtcControl) == 0x19AU, "rtcControl offset");
_Static_assert(offsetof(PocsMachineState, rtcDateBcd) == 0x1ACU, "rtcDateBcd offset");
_Static_assert(offsetof(PocsMachineState, irdaEvent) == 0x1B4U, "irdaEvent offset");
_Static_assert(offsetof(PocsMachineState, lcdVramFront) == 0x1D8U, "lcdVramFront offset");
_Static_assert(offsetof(PocsMachineState, ledNotifyFlag) == 0x3D8U, "ledNotifyFlag offset");
_Static_assert(offsetof(PocsMachineState, saveCountdown) == 0x3E7U, "saveCountdown offset");
_Static_assert(offsetof(PocsMachineState, intcEnable) == 0x3E8U, "intcEnable offset");
_Static_assert(offsetof(PocsMachineState, flashDataPtr) == 0x3F0U, "flashDataPtr offset");
_Static_assert(offsetof(PocsMachineState, cpuPc) == 0x3F4U, "cpuPc offset");
_Static_assert(offsetof(PocsMachineState, flashEvent) == 0x478U, "flashEvent offset");
_Static_assert(offsetof(PocsMachineState, internalRam) == 0x488U, "internalRam offset");
_Static_assert(sizeof(PocsMachineState) == 0xC88U, "PocsMachineState size");

/* Known PocketStation BIOS internal SRAM offsets (relative to internalRam) */
#define POCS_RAM_OFF_BIOS_STATUS    0x0C0U /* 0x81068178 */
#define POCS_RAM_OFF_YEAR_LO_BCD    0x0CDU /* 0x81068185 */
#define POCS_RAM_OFF_CENTURY_BCD    0x0CFU /* 0x81068187 */
#define POCS_RAM_OFF_KERNEL_RUNNING 0x0D0U /* 0x81068188 */
#define POCS_RAM_OFF_MEMC_SECTOR    0x0F0U /* 0x810681A8 */
#define POCS_RAM_OFF_APP_STATE      0x400U /* 0x810684B8 */

/* Known PocketStation BIOS ROM patch offsets (relative to g_pocsBiosRom at 0x81063080) */
#define POCS_ROM_OFF_CENTURY_BCD    0x034CU /* 0x810633CC */
#define POCS_ROM_OFF_RTC_PATCH_A    0x289EU /* 0x8106591E */
#define POCS_ROM_OFF_RTC_PATCH_B    0x33A3U /* 0x81066423 */
#define POCS_ROM_OFF_RESET_PATCH_A  0x35C0U /* 0x81066640 */
#define POCS_ROM_OFF_RESET_PATCH_B  0x35C1U /* 0x81066641 */

extern PocsMachineState g_pocsState;
extern uint8_t g_pocsBiosRom[POCS_BIOS_ROM_SIZE];
extern const uint8_t g_pocsVertexShaderGxp[];
extern const uint8_t g_pocsFragmentShaderGxp[];
extern const uint8_t g_pocsLedShadowData[120];
extern const uint8_t g_pocsLedLargeData[8928];
extern const uint8_t g_pocsLedSmallData[1792];

static inline int32_t pocsScaleCpuCycles(int32_t cycles)
{
	uint8_t shift = g_pocsState.clockShift;
	if (shift <= 7U)
		return cycles << shift;
	if (shift == 8U)
		return cycles * 244;
	return cycles;
}

static inline int32_t pocsUnscaleCpuCycles(int32_t cycles)
{
	uint8_t shift = g_pocsState.clockShift;
	if (shift <= 7U)
		return cycles >> shift;
	if (shift == 8U)
		return cycles / 244;
	return cycles;
}

static inline int32_t pocsGetCurrentCycle(void)
{
	return g_pocsState.schedTargetCycle - pocsScaleCpuCycles(g_pocsState.cpuCyclesLeft);
}

/* remote_pocs.c internal helpers */
int pocsCpuReset(void);
int pocsCpuRaiseUndef(void);
PocsSchedEvent *pocsSchedAddEvent(PocsSchedEvent *event, int32_t delayCycles);
PocsSchedEvent *pocsSchedRemoveEvent(PocsSchedEvent *event);
int pocsLockMainMutex(void);
int pocsUnlockMainMutex(void);
int pocsSpiExchangeWrapper(int portMode, uint8_t txByte);
void *pocsGetActiveFlashData(void);
int pocsAdhocSendData(int dataLen, void *data);

/* pocs_rtc_audio.c */
PocsSchedEvent *pocsRtcEventCallback(PocsSchedEvent *event);
int pocsSyncRtcFromHost(void);
int pocsRtcRead(int regOffset);
PocsSchedEvent *pocsRtcWrite(int regOffset, uint8_t value);
PocsSchedEvent *pocsRtcReset(void);
int pocsGpioRead(int regOffset);
int pocsGpioWrite(int regOffset, uint8_t value);
void pocsGpioReset(void);
int pocsAudioUpdateRing(int sampleValue);
int pocsAudioFlush(void);
int pocsAudioRead(int regOffset);
int pocsAudioWrite(int regOffset, int value);
SceUID pocsAudioInit(void);
int pocsAudioReset(void);
int pocsBatteryRead(int regOffset);
int pocsBatteryWrite(int regOffset, uint8_t value);
void pocsBatteryReset(void);

/* pocs_periph.c */
int pocsIrdaOnIrqEnabled(void);
int pocsIrdaBeginFrame(int32_t currentCycle);
int pocsIrdaSignalConnected(void);
int pocsIrdaWaitSendReady(void);
int pocsIrdaCancelSession(void);
int pocsIrdaEndFrame(int32_t currentCycle);
int pocsIrdaReceivePacket(unsigned int packetLen, const uint8_t *packetData);
int pocsIrdaInit(void);
int pocsIrdaReset(void);
int pocsIrdaSetRole(int isAcceptor);
int pocsLcdNotifyDirty(int mode);
int pocsInputPollAndUpdate(void *gxmFramebufferState);
void *pocsLcdGetFlickerOverlay(void);
void *pocsLcdReset(void);
int pocsSpiLock(void);
int pocsSpiUnlock(void);
int pocsSpiExchange(int portMode, uint8_t txByte);
int pocsSpiInit(void);
int pocsSpiReset(void);
int pocsSpiAbortPending(void);
int pocsSpiSyncHost(void);
int pocsCpuCheckPendingIrq(void);
int pocsCpuExceptionUndef(void);

/* pocs_cpu.c */
uint32_t pocsCpuRunCycles(uint32_t pc, int32_t cycleBudget);

/* pocs_gxm.c & pocs_hw_regs.c */
int pocsGxmInit(void *gxmFramebufferState);
int pocsGxmGetChannelCount(int bgType);
int pocsGxmRequestRender(int bgType, int channel, int presentMode);
int pocsGxmWaitFinish(void);
int pocsPokejimaIsActive(void);
int pocsPokejimaReceivePacket(unsigned int packetLen, const void *packetData);
uint32_t pocsPokejimaHandleUndefTrap(uint32_t nextPc);
void pocsPokejimaPatchFlashTraps(void);
int pocsPokejimaInit(void);
void pocsSysFatalAccess(int isRead, int isWrite);
uint32_t pocsSysRegRead(uint32_t regOffset, int accessWidth);
void pocsSysRegWrite(uint32_t regOffset, uint16_t value);
int pocsSysReset(void);
int pocsIntcRead(int regOffset);
int pocsIntcWrite(int regOffset, uint16_t value);
void pocsIntcReset(void);
uint32_t pocsTimerRead(uint32_t regOffset);
uint32_t pocsTimerWrite(uint32_t regOffset, uint16_t value);
int pocsTimerReset(void);
int pocsClockRead(int regOffset);
PocsSchedEvent *pocsClockWrite(int regOffset, uint8_t value);
PocsSchedEvent *pocsClockReset(void);
int pocsRtcStepBcdField(int fieldIndex);

#ifdef __cplusplus
}
#endif

#endif /* PSPEMU_POCS_INTERNAL_H */
