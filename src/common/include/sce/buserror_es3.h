/*
 * ES3 bus error collection and decoding, shared by the SceKernelBusError
 * modules: the secure-world slb2:kbl/SceKernelBusError collects the error
 * registers into a record in the sysroot shared memory, the debug builds of it
 * and of os0:kd/buserror.skprx decode that record to the console. Sony's
 * source is buserror_es3.c (the debug records of both modules carry the same
 * lines); the tables are compiled into both.
 *
 * This header holds definitions: include it from exactly one C file of a
 * module. Everything is static, what a module does not use is dropped. The
 * debug print and assert calls use the os0 import names; the secure-world
 * module maps them to its ForTZS imports before including.
 * Define SCE_BUSERROR_TABLES_ONLY to reuse the predicates and tables without
 * the collection and formatting helpers.
 *
 * Type, table and helper names are unknown unless a debug record names them.
 */

#pragma once

#include <stddef.h>
#include <psp2kern/types.h>
#include "sce/debug.h"

#pragma push_macro("__FILE_NAME__")
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-macro-redefined"
#undef __FILE_NAME__
#define __FILE_NAME__ "buserror_es3.c"
#pragma GCC diagnostic pop

/* release builds keep the tables but not their names */
#if FW_DEBUG
#define BUSERROR_NAME(s)	s
#else
#define BUSERROR_NAME(s)	""
#endif

#define REG(address)		((SceUInt32 *)(address))

/*
 * Pervasive reset (0xe3101000) and clock gate (0xe3102000) registers: a
 * device's error registers are only read while it is out of reset and clocked.
 */
#define PERVASIVE_RESET(offset)	(*REG(0xe3101000 + (offset)))
#define PERVASIVE_GATE(offset)	(*REG(0xe3102000 + (offset)))

/* OCP bus error target status (BET0, BET1, BEBT), two sets */
#define OCP_STATUS0		0xe3110d80
#define OCP_STATUS1		0xe3110dc0

/* error log register block of the GRAB and register bus bridges */
typedef struct BusErrorLogRegs {
	SceUInt32 unk0[2];
	SceUInt32 status;			/* 0x08 bit 2 write, bit 1 read error */
	SceUInt32 unkC;
	SceUInt32 write[4];			/* 0x10 ID, address, attribute, info */
	SceUInt32 read[4];			/* 0x20 */
} BusErrorLogRegs;

#define GRAB_SECURE		((const BusErrorLogRegs *)0xe8300000)
#define GRAB			((const BusErrorLogRegs *)0xe8301000)
#define GRAB_EXTRA		REG(0xe8301208)
#define REGBUS_SECURE		((const BusErrorLogRegs *)0xe8000100)
#define REGBUS			((const BusErrorLogRegs *)0xe8001100)

/* VIP and Venezia protection error registers (ERRV, ERRC, ERRA per unit) */
#define VIP_PROT		REG(0xe3110248)	/* BAP, VDPD, VDPM */
#define VENEZIA_PROT		REG(0xe3110348)

/* SonicsSX interconnect: a bus, and an initiator/target agent on it */
typedef struct SsxBusRegs {
	SceUInt32 unk0[0x44];
	struct {
		SceUInt64 value;
		SceUInt32 unk8[6];
	} flagStatus[5];			/* 0x110 SI_FLAG_STATUS0-4 */
} SsxBusRegs;

typedef struct SsxAgentRegs {
	SceUInt32 unk0[10];
	SceUInt64 status;			/* 0x28 */
	SceUInt32 unk30[10];
	SceUInt64 errorLog;			/* 0x58 */
	SceUInt64 errorAddress;			/* 0x60 */
} SsxAgentRegs;

#define DEBUG_BUS		0xec500000

#include "sce/buserror_types.h"

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))

/* is the device at this pervasive offset out of reset and clocked */
static inline int pervasiveIsOn(unsigned int offset)
{
	if (PERVASIVE_RESET(offset) != 0)
		return 0;
	return PERVASIVE_GATE(offset) != 0;
}

/* the same for one bit of a shared reset/gate register */
static inline int pervasiveBitIsOn(unsigned int offset, unsigned int bit)
{
	if (PERVASIVE_RESET(offset) & (1 << bit))
		return 0;
	return (PERVASIVE_GATE(offset) >> bit) & 1;
}

#define PERVASIVE_PREDICATE(name, offset)				\
static int __attribute__((noinline, unused)) name(void)			\
{									\
	return pervasiveIsOn(offset);					\
}

PERVASIVE_PREDICATE(isDmac0On, 0x50)
PERVASIVE_PREDICATE(isDmac1On, 0x54)
PERVASIVE_PREDICATE(isDmac2On, 0x58)
PERVASIVE_PREDICATE(isDmac3On, 0x5c)
PERVASIVE_PREDICATE(isDmac4On, 0x60)
PERVASIVE_PREDICATE(isDmac5On, 0x64)
PERVASIVE_PREDICATE(isDmac6On, 0x68)

static int __attribute__((noinline, unused)) isSpm32On(void)
{
	return pervasiveBitIsOn(0x178, 3);
}

static int __attribute__((noinline, unused)) isSpm128On(void)
{
	return pervasiveBitIsOn(0x178, 2);
}

PERVASIVE_PREDICATE(isDdrIf0On, 0x160)
PERVASIVE_PREDICATE(isDdrIf1On, 0x164)
/* also guards the GRAB and register bus bridges */
PERVASIVE_PREDICATE(isCompatRamOn, 0x28)
PERVASIVE_PREDICATE(isDebugPaOn, 0x48)
PERVASIVE_PREDICATE(isTimerOn, 0x170)
PERVASIVE_PREDICATE(isSdio0On, 0x40)
PERVASIVE_PREDICATE(isSdio1On, 0x44)

/* Pervasive and Pervasive2 are always on */
static int __attribute__((noinline, unused)) isAlwaysOn(void)
{
	return 1;
}

static const BusErrorPort __attribute__((unused)) mainXbarInitiators[] = {
	{BUSERROR_NAME("SSX_MainXB_IA_ARM0"), 0xec040000},
	{BUSERROR_NAME("SSX_MainXB_IA_ARM1"), 0xec040400},
	{BUSERROR_NAME("SSX_MainXB_IA_CenterXbar0"), 0xec048000},
	{BUSERROR_NAME("SSX_MainXB_IA_CenterXbar1"), 0xec048400},
	{BUSERROR_NAME("SSX_MainXB_IA_CenterXbar2"), 0xec048800},
	{BUSERROR_NAME("SSX_MainXB_IA_IoMasterBus"), 0xec048c00},
	{BUSERROR_NAME("SSX_MainXB_IA_DmacXbar0"), 0xec049000},
	{BUSERROR_NAME("SSX_MainXB_IA_DmacXbar1"), 0xec049400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) mainXbarTargets[] = {
	{BUSERROR_NAME("SSX_MainXB_TA_Spad_32KiB"), 0xec050000},
	{BUSERROR_NAME("SSX_MainXB_TA_LPDDR0"), 0xec050400},
	{BUSERROR_NAME("SSX_MainXB_TA_ACP"), 0xec050800},
	{BUSERROR_NAME("SSX_MainXB_TA_CenterXbar0"), 0xec058000},
	{BUSERROR_NAME("SSX_MainXB_TA_CenterXbar1"), 0xec058400},
	{BUSERROR_NAME("SSX_MainXB_TA_IoSlaveBus"), 0xec058800},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) centerXbarInitiators[] = {
	{BUSERROR_NAME("SSX_CenterXB_IA_GPUXbar"), 0xec140000},
	{BUSERROR_NAME("SSX_CenterXB_IA_MainXbar0"), 0xec148000},
	{BUSERROR_NAME("SSX_CenterXB_IA_MainXbar1"), 0xec148400},
	{BUSERROR_NAME("SSX_CenterXB_IA_VideoXbar0"), 0xec148800},
	{BUSERROR_NAME("SSX_CenterXB_IA_VideoXbar1"), 0xec148c00},
	{BUSERROR_NAME("SSX_CenterXB_IA_IoMasterBus"), 0xec149000},
	{BUSERROR_NAME("SSX_CenterXB_IA_DmacXbar0"), 0xec149400},
	{BUSERROR_NAME("SSX_CenterXB_IA_DmacXbar1"), 0xec149800},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) centerXbarTargets[] = {
	{BUSERROR_NAME("SSX_CenterXB_TA_Compati_2MiB"), 0xec150000},
	{BUSERROR_NAME("SSX_CenterXB_TA_DevNull"), 0xec150400},
	{BUSERROR_NAME("SSX_CenterXB_TA_GpuXbar"), 0xec150800},
	{BUSERROR_NAME("SSX_CenterXB_TA_MainXbar0"), 0xec158000},
	{BUSERROR_NAME("SSX_CenterXB_TA_MainXbar1"), 0xec158400},
	{BUSERROR_NAME("SSX_CenterXB_TA_MainXbar2"), 0xec158800},
	{BUSERROR_NAME("SSX_CenterXB_TA_VideoXbar"), 0xec158c00},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) videoXbarInitiators[] = {
	{BUSERROR_NAME("SSX_VideoXB_IA_DMAC2"), 0xec240000},
	{BUSERROR_NAME("SSX_VideoXB_IA_DMAC3"), 0xec240400},
	{BUSERROR_NAME("SSX_VideoXB_IA_Venezia"), 0xec240800},
	{BUSERROR_NAME("SSX_VideoXB_IA_LCDDMAC"), 0xec240c00},
	{BUSERROR_NAME("SSX_VideoXB_IA_CenterXbar"), 0xec248000},
	{BUSERROR_NAME("SSX_VideoXB_IA_IftuXbar0"), 0xec248400},
	{BUSERROR_NAME("SSX_VideoXB_IA_IftuXbar1"), 0xec248800},
	{BUSERROR_NAME("SSX_VideoXB_IA_VipXbar0"), 0xec248c00},
	{BUSERROR_NAME("SSX_VideoXB_IA_VipXbar1"), 0xec249000},
	{BUSERROR_NAME("SSX_VideoXB_IA_DebugBus"), 0xec249400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) videoXbarTargets[] = {
	{BUSERROR_NAME("SSX_VideoXB_TA_Spad_128KiB"), 0xec250000},
	{BUSERROR_NAME("SSX_VideoXB_TA_GpuXbar0"), 0xec250400},
	{BUSERROR_NAME("SSX_VideoXB_TA_GpuXbar1"), 0xec250800},
	{BUSERROR_NAME("SSX_VideoXB_TA_GpuXbar2"), 0xec250c00},
	{BUSERROR_NAME("SSX_VideoXB_TA_CenterXbar0"), 0xec258000},
	{BUSERROR_NAME("SSX_VideoXB_TA_CenterXbar1"), 0xec258400},
	{BUSERROR_NAME("SSX_VideoXB_TA_DebugBus"), 0xec258800},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) iftuXbarInitiators[] = {
	{BUSERROR_NAME("SSX_IftuXB_IA_IFTU0a"), 0xec340000},
	{BUSERROR_NAME("SSX_IftuXB_IA_IFTU0b"), 0xec340400},
	{BUSERROR_NAME("SSX_IftuXB_IA_IFTU1a"), 0xec340800},
	{BUSERROR_NAME("SSX_IftuXB_IA_IFTU1b"), 0xec340c00},
	{BUSERROR_NAME("SSX_IftuXB_IA_IFTU2"), 0xec341000},
	{BUSERROR_NAME("SSX_IftuXB_IA_IftuRegBus"), 0xec348000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) iftuXbarTargets[] = {
	{BUSERROR_NAME("SSX_IftuXB_TA_VideoXbar0"), 0xec358000},
	{BUSERROR_NAME("SSX_IftuXB_TA_VideoXbar1"), 0xec358400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) vipXbarInitiators[] = {
	{BUSERROR_NAME("SSX_VipXB_IA_VDP0"), 0xec440000},
	{BUSERROR_NAME("SSX_VipXB_IA_VDP1"), 0xec440400},
	{BUSERROR_NAME("SSX_VipXB_IA_VDP2"), 0xec440800},
	{BUSERROR_NAME("SSX_VipXB_IA_VDP3"), 0xec440c00},
	{BUSERROR_NAME("SSX_VipXB_IA_VDPMeP"), 0xec441000},
	{BUSERROR_NAME("SSX_VipXB_IA_BAPMeP"), 0xec441400},
	{BUSERROR_NAME("SSX_VipXB_IA_GpuRegBus"), 0xec448000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) vipXbarTargets[] = {
	{BUSERROR_NAME("SSX_VipXB_TA_GpuXbar"), 0xec450000},
	{BUSERROR_NAME("SSX_VipXB_TA_VideoXbar0"), 0xec458000},
	{BUSERROR_NAME("SSX_VipXB_TA_VideoXbar1"), 0xec458400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) debugBusInitiators[] = {
	{BUSERROR_NAME("SSX_DebugBus_IA_DMAC6"), 0xec540000},
	{BUSERROR_NAME("SSX_DebugBus_IA_PA"), 0xec540400},
	{BUSERROR_NAME("SSX_DebugBus_IA_VideoXbar"), 0xec548000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) debugBusTargets[] = {
	{BUSERROR_NAME("SSX_DebugBus_TA_SDIO0"), 0xec550000},
	{BUSERROR_NAME("SSX_DebugBus_TA_SDIO1"), 0xec550400},
	{BUSERROR_NAME("SSX_DebugBus_TA_LPDDR1"), 0xec550800},
	{BUSERROR_NAME("SSX_DebugBus_TA_VideoXbar"), 0xec558000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) dmacXbarInitiators[] = {
	{BUSERROR_NAME("SSX_DmacXB_IA_DMAC0"), 0xec640000},
	{BUSERROR_NAME("SSX_DmacXB_IA_DMAC1"), 0xec640400},
	{BUSERROR_NAME("SSX_DmacXB_IA_IoSlaveBus"), 0xec648000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) dmacXbarTargets[] = {
	{BUSERROR_NAME("SSX_DmacXB_TA_MainXbar0"), 0xec658000},
	{BUSERROR_NAME("SSX_DmacXB_TA_MainXbar1"), 0xec658400},
	{BUSERROR_NAME("SSX_DmacXB_TA_CenterXbar0"), 0xec658800},
	{BUSERROR_NAME("SSX_DmacXB_TA_CenterXbar1"), 0xec658c00},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) ioSlaveBusInitiators[] = {
	{BUSERROR_NAME("SSX_IoSlaveBus_IA_MainXbar"), 0xed048000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) ioSlaveBusTargets[] = {
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_PervasiveBus"), 0xed058000},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_NorthIoBus"), 0xed058400},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_GpuRegBus"), 0xed058800},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_MiscIoBus"), 0xed058c00},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_CameraSlaveBus0"), 0xed059000},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_CameraSlaveBus1"), 0xed059400},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_CameraSlaveBus2"), 0xed059800},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_IoMasterBus"), 0xed059c00},
	{BUSERROR_NAME("SSX_IoSlaveBus_TA_DmacXbar"), 0xed05a000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) ioMasterBusInitiators[] = {
	{BUSERROR_NAME("SSX_IoMasterBus_IA_NorthIoBus"), 0xed148000},
	{BUSERROR_NAME("SSX_IoMasterBus_IA_CameraMasterBus"), 0xed148400},
	{BUSERROR_NAME("SSX_IoMasterBus_IA_IoSlaveBus"), 0xed148800},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) ioMasterBusTargets[] = {
	{BUSERROR_NAME("SSX_IoMasterBus_TA_MainXbar"), 0xed158000},
	{BUSERROR_NAME("SSX_IoMasterBus_TA_CenterXbar"), 0xed158400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) pervasiveBusInitiators[] = {
	{BUSERROR_NAME("SSX_PervasiveBus_IA_IoSlaveBus"), 0xed248000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) pervasiveBusTargets[] = {
	{BUSERROR_NAME("SSX_PervasiveBus_TA_DMAC0"), 0xed250000},
	{BUSERROR_NAME("SSX_PervasiveBus_TA_DMAC1"), 0xed250000},
	{BUSERROR_NAME("SSX_PervasiveBus_TA_Pervasive"), 0xed250800},
	{BUSERROR_NAME("SSX_PervasiveBus_TA_CoreSight"), 0xed250c00},
	{BUSERROR_NAME("SSX_PervasiveBus_TA_VFPINT"), 0xed251000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) miscIoBusInitiators[] = {
	{BUSERROR_NAME("SSX_MiscIoBus_IA_IoSlaveBus"), 0xed348000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) miscIoBusTargets[] = {
	{BUSERROR_NAME("SSX_MiscIoBus_TA_UART0"), 0xed350000},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_UART1"), 0xed350400},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_UART2"), 0xed350800},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_UART3"), 0xed350c00},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_UART4"), 0xed351000},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_UART5"), 0xed351400},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_UART6"), 0xed351800},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_GPIO0"), 0xed351c00},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_Timer"), 0xed352000},
	{BUSERROR_NAME("SSX_MiscIoBus_TA_PWM"), 0xed352400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) northIoMasterBusInitiators[] = {
	{BUSERROR_NAME("SSX_NIoMasterBus_IA_DMAC4"), 0xed440000},
	{BUSERROR_NAME("SSX_NIoMasterBus_IA_DMAC5"), 0xed440400},
	{BUSERROR_NAME("SSX_NIoMasterBus_IA_HSMMC1"), 0xed440800},
	{BUSERROR_NAME("SSX_NIoMasterBus_IA_HSMMC2"), 0xed440c00},
	{BUSERROR_NAME("SSX_NIoMasterBus_IA_HSMMC3"), 0xed441000},
	{BUSERROR_NAME("SSX_NIoMasterBus_IA_NIoSlaveBus"), 0xed448000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) northIoMasterBusTargets[] = {
	{BUSERROR_NAME("SSX_NIoMasterBus_TA_KeyRing"), 0xed450000},
	{BUSERROR_NAME("SSX_NIoMasterBus_TA_AudioBus"), 0xed458000},
	{BUSERROR_NAME("SSX_NIoMasterBus_TA_SrcBus"), 0xed458400},
	{BUSERROR_NAME("SSX_NIoMasterBus_TA_IoMasterBus0"), 0xed458800},
	{BUSERROR_NAME("SSX_NIoMasterBus_TA_IoMasterBus1"), 0xed458c00},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) audioBusInitiators[] = {
	{BUSERROR_NAME("SSX_AudioBus_IA_NIoMasterBus"), 0xed548000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) audioBusTargets[] = {
	{BUSERROR_NAME("SSX_AudioBus_TA_I2S0"), 0xed550000},
	{BUSERROR_NAME("SSX_AudioBus_TA_I2S1"), 0xed550400},
	{BUSERROR_NAME("SSX_AudioBus_TA_I2S2"), 0xed550800},
	{BUSERROR_NAME("SSX_AudioBus_TA_I2SD0"), 0xed550c00},
	{BUSERROR_NAME("SSX_AudioBus_TA_I2SD1"), 0xed551000},
	{BUSERROR_NAME("SSX_AudioBus_TA_I2S4"), 0xed551400},
	{BUSERROR_NAME("SSX_AudioBus_TA_I2S7"), 0xed551c00},
	{BUSERROR_NAME("SSX_AudioBus_TA_SPDIF"), 0xed552000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) northIoSlaveBusInitiators[] = {
	{BUSERROR_NAME("SSX_NIoSlaveBus_IA_IoSlaveBus"), 0xed648000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) northIoSlaveBusTargets[] = {
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_DMAC4"), 0xed650000},
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_DMAC5"), 0xed650400},
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_I2C0"), 0xed650800},
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_I2C1"), 0xed650c00},
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_HSMMC1"), 0xed651000},
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_HSMMC2"), 0xed651400},
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_HSMMC3"), 0xed651800},
	{BUSERROR_NAME("SSX_NIoSlaveBus_TA_NIoMasterBus"), 0xed658000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) srcBusInitiators[] = {
	{BUSERROR_NAME("SSX_SrcBus_IA_NIoMasterBus"), 0xed748000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) srcBusTargets[] = {
	{BUSERROR_NAME("SSX_SrcBus_TA_SRC0"), 0xed750000},
	{BUSERROR_NAME("SSX_SrcBus_TA_SRC1"), 0xed750400},
	{BUSERROR_NAME("SSX_SrcBus_TA_SRC2"), 0xed750800},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) gpuRegisterBusInitiators[] = {
	{BUSERROR_NAME("SSX_GpuRegBus_IA_IoSlaveBus"), 0xed848000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) gpuRegisterBusTargets[] = {
	{BUSERROR_NAME("SSX_GpuRegBus_TA_DMAC2"), 0xed850000},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_DMAC3"), 0xed850400},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_LCDDMAC"), 0xed850800},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_DMAC6"), 0xed850c00},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_PA"), 0xed851000},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_Pervasive2"), 0xed851400},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_SonyRegBus"), 0xed851800},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_IftuRegBus"), 0xed858000},
	{BUSERROR_NAME("SSX_GpuRegBus_TA_VipXbar"), 0xed858400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) iftuRegisterBusInitiators[] = {
	{BUSERROR_NAME("SSX_IftuRegBus_IA_GpuRegBus"), 0xed948000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) iftuRegisterBusTargets[] = {
	{BUSERROR_NAME("SSX_IftuRegBus_TA_IFTU0"), 0xed950000},
	{BUSERROR_NAME("SSX_IftuRegBus_TA_IFTU1"), 0xed950400},
	{BUSERROR_NAME("SSX_IftuRegBus_TA_IFTU2"), 0xed950800},
	{BUSERROR_NAME("SSX_IftuRegBus_TA_DSI0"), 0xed950c00},
	{BUSERROR_NAME("SSX_IftuRegBus_TA_DSI1"), 0xed951000},
	{BUSERROR_NAME("SSX_IftuRegBus_TA_IftuXbar"), 0xed958000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) cameraSlaveBusInitiators[] = {
	{BUSERROR_NAME("SSX_CameraSlaveBus_IA_IoSlaveBus0"), 0xee048000},
	{BUSERROR_NAME("SSX_CameraSlaveBus_IA_IoSlaveBus1"), 0xee048400},
	{BUSERROR_NAME("SSX_CameraSlaveBus_IA_IoSlaveBus2"), 0xee048800},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) cameraSlaveBusTargets[] = {
	{BUSERROR_NAME("SSX_CameraSlaveBus_TA_GPIO1"), 0xee050000},
	{BUSERROR_NAME("SSX_CameraSlaveBus_TA_CameraRegBus"), 0xee058000},
	{BUSERROR_NAME("SSX_CameraSlaveBus_TA_SlcdSlaveBus0"), 0xee058400},
	{BUSERROR_NAME("SSX_CameraSlaveBus_TA_UsbSlaveBus"), 0xee058800},
	{BUSERROR_NAME("SSX_CameraSlaveBus_TA_SlcdSlaveBus1"), 0xee058c00},
	{BUSERROR_NAME("SSX_CameraSlaveBus_TA_CameraMasterBus"), 0xee059000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) cameraMasterBusInitiators[] = {
	{BUSERROR_NAME("SSX_CameraMasterBus_IA_CameraIf0"), 0xee140000},
	{BUSERROR_NAME("SSX_CameraMasterBus_IA_CameraIf1"), 0xee140400},
	{BUSERROR_NAME("SSX_CameraMasterBus_IA_SlcdMasterBus"), 0xee148000},
	{BUSERROR_NAME("SSX_CameraMasterBus_IA_UsbMasterBus"), 0xee148400},
	{BUSERROR_NAME("SSX_CameraMasterBus_IA_CameraSlaveBus"), 0xee148800},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) cameraMasterBusTargets[] = {
	{BUSERROR_NAME("SSX_CameraMasterBus_TA_IoMasterBus0"), 0xee158000},
	{BUSERROR_NAME("SSX_CameraMasterBus_TA_IoMasterBus1"), 0xee158400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) usbSlaveBusInitiators[] = {
	{BUSERROR_NAME("SSX_UsbSlaveBus_IA_CameraSlaveBus"), 0xee248000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) usbSlaveBusTargets[] = {
	{BUSERROR_NAME("SSX_UsbSlaveBus_TA_USB0HOST"), 0xee250000},
	{BUSERROR_NAME("SSX_UsbSlaveBus_TA_USB1HOST"), 0xee250400},
	{BUSERROR_NAME("SSX_UsbSlaveBus_TA_USB2HOST"), 0xee250800},
	{BUSERROR_NAME("SSX_UsbSlaveBus_TA_USB1DEVICE"), 0xee252c00},
	{BUSERROR_NAME("SSX_UsbSlaveBus_TA_USB2DEVICE"), 0xee253000},
	{BUSERROR_NAME("SSX_UsbSlaveBus_TA_USB0DEVICE"), 0xee253400},
	{BUSERROR_NAME("SSX_UsbSlaveBus_TA_UsbMasterBus"), 0xee258000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) usbMasterBusInitiators[] = {
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB1OHCI"), 0xee340400},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB1EHCI"), 0xee340800},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB2OHCI"), 0xee340c00},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB2EHCI"), 0xee341000},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB0OHCI"), 0xee341400},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB0EHCI"), 0xee341800},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB1DEVICE"), 0xee343400},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB2DEVICE"), 0xee343800},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_USB0DEVICE"), 0xee343c00},
	{BUSERROR_NAME("SSX_UsbMasterBus_IA_UsbSlaveBus"), 0xee348000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) usbMasterBusTargets[] = {
	{BUSERROR_NAME("SSX_UsbMasterBus_TA_CameraMasterBus"), 0xee358000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) subLcdSlaveBusInitiators[] = {
	{BUSERROR_NAME("SSX_SlcdSlaveBus_IA_CameraSlaveBus0"), 0xee448000},
	{BUSERROR_NAME("SSX_SlcdSlaveBus_IA_CameraSlaveBus1"), 0xee448400},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) subLcdSlaveBusTargets[] = {
	{BUSERROR_NAME("SSX_SlcdSlaveBus_TA_HSMMC0"), 0xee450000},
	{BUSERROR_NAME("SSX_SlcdSlaveBus_TA_SubLCD"), 0xee450400},
	{BUSERROR_NAME("SSX_SlcdSlaveBus_TA_SMSHC"), 0xee450800},
	{BUSERROR_NAME("SSX_SlcdSlaveBus_TA_SPI0"), 0xee450c00},
	{BUSERROR_NAME("SSX_SlcdSlaveBus_TA_SPI1"), 0xee451000},
	{BUSERROR_NAME("SSX_SlcdSlaveBus_TA_SPI2"), 0xee451400},
	{BUSERROR_NAME("SSX_SlcdSlaveBus_TA_SlcdMasterBus"), 0xee458000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) subLcdMasterBusInitiators[] = {
	{BUSERROR_NAME("SSX_SlcdMasterBus_IA_HSMMC0"), 0xee540000},
	{BUSERROR_NAME("SSX_SlcdMasterBus_IA_SubLCD"), 0xee540400},
	{BUSERROR_NAME("SSX_SlcdMasterBus_IA_SMSHC"), 0xee540800},
	{BUSERROR_NAME("SSX_SlcdMasterBus_IA_SlcdSlaveBus"), 0xee548000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) subLcdMasterBusTargets[] = {
	{BUSERROR_NAME("SSX_SlcdMasterBus_TA_CameraMasterBus"), 0xee558000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) cameraRegisterBusInitiators[] = {
	{BUSERROR_NAME("SSX_CameraRegBus_IA_CameraSlaveBus"), 0xee648000},
	{NULL, 0},
};

static const BusErrorPort __attribute__((unused)) cameraRegisterBusTargets[] = {
	{BUSERROR_NAME("SSX_CameraRegBus_TA_CameraIf0"), 0xee650000},
	{BUSERROR_NAME("SSX_CameraRegBus_TA_CameraIf1"), 0xee650400},
	{BUSERROR_NAME("SSX_CameraRegBus_TA_CSI0"), 0xee650800},
	{BUSERROR_NAME("SSX_CameraRegBus_TA_CSI1"), 0xee650c00},
	{NULL, 0},
};

static const BusErrorOcpTarget __attribute__((unused)) ocpTargets0[] = {
	{BUSERROR_NAME("DMAC0"), 0x00000001, isDmac0On,
	 {{REG(0xe3000110), REG(0xe3000114)}, {REG(0xe3000118), REG(0xe300011c)}}},
	{BUSERROR_NAME("DMAC1"), 0x00000002, isDmac1On,
	 {{REG(0xe3010110), REG(0xe3010114)}, {REG(0xe3010118), REG(0xe301011c)}}},
	{BUSERROR_NAME("DMAC2"), 0x00000004, isDmac2On,
	 {{REG(0xe5000110), REG(0xe5000114)}, {REG(0xe5000118), REG(0xe500011c)}}},
	{BUSERROR_NAME("DMAC3"), 0x00000008, isDmac3On,
	 {{REG(0xe5010110), REG(0xe5010114)}, {REG(0xe5010118), REG(0xe501011c)}}},
	{BUSERROR_NAME("DMAC4"), 0x00000010, isDmac4On,
	 {{REG(0xe0400810), REG(0xe0400814)}, {REG(0xe0400818), REG(0xe040081c)}}},
	{BUSERROR_NAME("DMAC5"), 0x00000020, isDmac5On,
	 {{REG(0xe0410110), REG(0xe0410114)}, {REG(0xe0410118), REG(0xe041011c)}}},
	{BUSERROR_NAME("DMAC6"), 0x00000040, isDmac6On,
	 {{REG(0xe50c0110), REG(0xe50c0114)}, {REG(0xe50c0118), REG(0xe50c011c)}}},
	{BUSERROR_NAME("SPM32"), 0x00000800, isSpm32On,
	 {{REG(0xe3110d14), NULL}, {REG(0xe3110d54), NULL}}},
	{BUSERROR_NAME("SPM128"), 0x00001000, isSpm128On,
	 {{REG(0xe3110d04), NULL}, {REG(0xe3110d44), NULL}}},
	{BUSERROR_NAME("DDRIF0"), 0x00004000, isDdrIf0On,
	 {{REG(0xe600c008), REG(0xe600c000)}, {REG(0xe60083a8), REG(0xe60083a0)}}},
	{BUSERROR_NAME("DDRIF1"), 0x00008000, isDdrIf1On,
	 {{REG(0xe588c008), REG(0xe588c000)}, {REG(0xe58883a8), REG(0xe58883a0)}}},
	{BUSERROR_NAME("CompatRAM"), 0x00020000, isCompatRamOn,
	 {{REG(0xe3110d24), NULL}, {REG(0xe3110d64), NULL}}},
	{BUSERROR_NAME("Pervasive"), 0x00100000, isAlwaysOn,
	 {{REG(0xe310013c), REG(0xe3100138)}, {REG(0xe3100134), REG(0xe3100130)}}},
	{BUSERROR_NAME("Pervasive2"), 0x00200000, isAlwaysOn,
	 {{REG(0xe3110d34), REG(0xe3110d38)}, {REG(0xe3110d74), REG(0xe3110d78)}}},
	{BUSERROR_NAME("DebugPA"), 0x00400000, isDebugPaOn,
	 {{REG(0xe50d10f0), REG(0xe50d10f4)}, {REG(0xe50d10f8), REG(0xe50d10fc)}}},
};

static const BusErrorOcpTarget __attribute__((unused)) ocpTargets1[] = {
	{BUSERROR_NAME("TIMER"), 0x00000008, isTimerOn,
	 {{REG(0xe20bf000), REG(0xe20bf004)}, {REG(0xe20bf008), REG(0xe20bf00c)}}},
	{BUSERROR_NAME("SDIO1"), 0x00004000, isSdio1On,
	 {{REG(0xe581fff0), REG(0xe581fff4)}, {REG(0xe581ffe8), REG(0xe581ffec)}}},
	{BUSERROR_NAME("SDIO0"), 0x00008000, isSdio0On,
	 {{REG(0xe580fff0), REG(0xe580fff4)}, {REG(0xe580ffe8), REG(0xe580ffec)}}},
};

static const BusErrorSystemBus __attribute__((unused)) systemBuses[] = {
	{BUSERROR_NAME("Main Xbar"), 0x00000001, BUSERROR_MAIN_XBAR, mainXbarInitiators, mainXbarTargets},
	{BUSERROR_NAME("Center Xbar"), 0x00000002, 0xec100000, centerXbarInitiators, centerXbarTargets},
	{BUSERROR_NAME("Video Xbar"), 0x00000004, 0xec200000, videoXbarInitiators, videoXbarTargets},
	{BUSERROR_NAME("IFTU Xbar"), 0x00000008, 0xec300000, iftuXbarInitiators, iftuXbarTargets},
	{BUSERROR_NAME("VIP Xbar"), 0x00000010, 0xec400000, vipXbarInitiators, vipXbarTargets},
	{BUSERROR_NAME("Debug Bus"), 0x00000020, 0xec500000, debugBusInitiators, debugBusTargets},
	{BUSERROR_NAME("DMAC Xbar"), 0x00000040, 0xec600000, dmacXbarInitiators, dmacXbarTargets},
	{BUSERROR_NAME("IO Slave Bus"), 0x00000100, 0xed000000, ioSlaveBusInitiators, ioSlaveBusTargets},
	{BUSERROR_NAME("IO Master Bus"), 0x00000200, 0xed100000, ioMasterBusInitiators, ioMasterBusTargets},
	{BUSERROR_NAME("Pervasive Bus"), 0x00000400, 0xed200000, pervasiveBusInitiators, pervasiveBusTargets},
	{BUSERROR_NAME("Misc IO Bus"), 0x00000800, 0xed300000, miscIoBusInitiators, miscIoBusTargets},
	{BUSERROR_NAME("North IO Master Bus"), 0x00001000, 0xed400000, northIoMasterBusInitiators, northIoMasterBusTargets},
	{BUSERROR_NAME("Audio Bus"), 0x00002000, 0xed500000, audioBusInitiators, audioBusTargets},
	{BUSERROR_NAME("North IO Slave Bus"), 0x00004000, 0xed600000, northIoSlaveBusInitiators, northIoSlaveBusTargets},
	{BUSERROR_NAME("SRC Bus"), 0x00008000, 0xed700000, srcBusInitiators, srcBusTargets},
	{BUSERROR_NAME("GPU Register Bus"), 0x00010000, 0xed800000, gpuRegisterBusInitiators, gpuRegisterBusTargets},
	{BUSERROR_NAME("IFTU Register Bus"), 0x00020000, 0xed900000, iftuRegisterBusInitiators, iftuRegisterBusTargets},
	{BUSERROR_NAME("Camera Slave Bus"), 0x01000000, 0xee000000, cameraSlaveBusInitiators, cameraSlaveBusTargets},
	{BUSERROR_NAME("Camera Master Bus"), 0x02000000, 0xee100000, cameraMasterBusInitiators, cameraMasterBusTargets},
	{BUSERROR_NAME("USB Slave Bus"), 0x04000000, 0xee200000, usbSlaveBusInitiators, usbSlaveBusTargets},
	{BUSERROR_NAME("USB Master Bus"), 0x08000000, 0xee300000, usbMasterBusInitiators, usbMasterBusTargets},
	{BUSERROR_NAME("SubLCD Slave Bus"), 0x10000000, 0xee400000, subLcdSlaveBusInitiators, subLcdSlaveBusTargets},
	{BUSERROR_NAME("SubLCD Master Bus"), 0x20000000, 0xee500000, subLcdMasterBusInitiators, subLcdMasterBusTargets},
	{BUSERROR_NAME("Camera Register Bus"), 0x40000000, 0xee600000, cameraRegisterBusInitiators, cameraRegisterBusTargets},
};

#ifndef SCE_BUSERROR_TABLES_ONLY

/* SI agent records of one port list; returns the end */
static inline BusErrorSiAgent *collectSiAgents(BusErrorSiAgent *agent, const BusErrorPort *port,
					       SceUInt32 *count)
{
	const SsxAgentRegs *regs;
	SceUInt32 n = 0;

	for (; port->name != NULL; port++) {
		regs = (const SsxAgentRegs *)port->address;
		n++;
		agent->size = sizeof(*agent);
		agent->address = port->address;
		agent->status = regs->status;
		agent->errorLog = regs->errorLog;
		agent->errorAddress = regs->errorAddress;
		agent++;
	}
	*count = n;
	return agent;
}

static inline void collectOcpTarget(BusErrorOcpError *error, const BusErrorOcpTarget *target)
{
	unsigned int i;

	if (!target->isOn()) {
		error[0].address = 0;
		error[0].attribute = 0;
		error[1].address = 0;
		error[1].attribute = 0;
		return;
	}
	for (i = 0; i < 2; i++) {
		error[i].address = *target->error[i].address;
		error[i].attribute = target->error[i].attribute != NULL ? *target->error[i].attribute : 0;
	}
}

/*
 * name unknown: read every bus error register into a record at rec; returns
 * its size.
 */
static SceSize __attribute__((unused)) CollectBusErrorInfo_ES3(BusErrorRecord *rec)
{
	BusErrorOcpInfo *ocp = (BusErrorOcpInfo *)(rec + 1);
	BusErrorSiInfo *si = (BusErrorSiInfo *)(ocp + 1);
	BusErrorSiBus *bus = (BusErrorSiBus *)(si + 1);
	const BusErrorSystemBus *sys;
	const SsxBusRegs *regs;
	BusErrorSiAgent *agent;
	BusErrorGrabInfo *grab, *regbus;
	BusErrorVipInfo *vip;
	SceSize siSize, grabSize, regbusSize;
	SceUInt32 count;
	unsigned int i;

	rec->hdrSize = sizeof(*rec);
	ocp->size = offsetof(BusErrorOcpInfo, target);
	ocp->status[0][0] = *REG(OCP_STATUS0);
	ocp->status[0][1] = *REG(OCP_STATUS0 + 0x10);
	ocp->status[0][2] = *REG(OCP_STATUS0 + 0x14);
	ocp->status[1][0] = *REG(OCP_STATUS1);
	ocp->status[1][1] = *REG(OCP_STATUS1 + 0x10);
	ocp->status[1][2] = *REG(OCP_STATUS1 + 0x14);
	ocp->count = BUSERROR_OCP_TARGETS;
	for (i = 0; i < ARRAY_SIZE(ocpTargets0); i++)
		collectOcpTarget(ocp->target[i].error, &ocpTargets0[i]);
	for (i = 0; i < ARRAY_SIZE(ocpTargets1); i++)
		collectOcpTarget(ocp->target[ARRAY_SIZE(ocpTargets0) + i].error, &ocpTargets1[i]);

	si->size = sizeof(*si);
	count = 0;
	for (sys = systemBuses; sys != &systemBuses[ARRAY_SIZE(systemBuses)]; sys++) {
		/* the debug bus only while it is running */
		if (sys->address == DEBUG_BUS &&
		    (PERVASIVE_RESET(0x154) == 1 || PERVASIVE_GATE(0x154) == 0))
			continue;
		regs = (const SsxBusRegs *)sys->address;
		bus->address = sys->address;
		bus->size = sizeof(*bus);
		count++;
		for (i = 0; i < 5; i++)
			bus->flagStatus[i] = regs->flagStatus[i].value;
		agent = collectSiAgents((BusErrorSiAgent *)(bus + 1), sys->initiators,
					&bus->initiatorCount);
		agent = collectSiAgents(agent, sys->targets, &bus->targetCount);
		bus = (BusErrorSiBus *)agent;
	}
	si->count = count;
	siSize = (char *)bus - (char *)si;

	grab = (BusErrorGrabInfo *)bus;
	grabSize = 0;
	if (isCompatRamOn()) {
		grabSize = sizeof(*grab);
		grab->size = sizeof(*grab);
		grab->status = GRAB->status;
		grab->write[0] = GRAB->write[0];
		grab->write[1] = GRAB->write[1];
		grab->write[2] = GRAB->write[2];
		grab->write[3] = GRAB->write[3];
		grab->read[0] = GRAB->read[0];
		grab->read[1] = GRAB->read[1];
		grab->read[2] = GRAB->read[2];
		grab->read[3] = GRAB->read[3];
		grab->secureStatus = GRAB_SECURE->status;
		grab->secureWrite[0] = GRAB_SECURE->write[0];
		grab->secureWrite[1] = GRAB_SECURE->write[1];
		grab->secureWrite[2] = GRAB_SECURE->write[2];
		grab->secureWrite[3] = GRAB_SECURE->write[3];
		grab->secureRead[0] = GRAB_SECURE->read[0];
		grab->secureRead[1] = GRAB_SECURE->read[1];
		grab->secureRead[2] = GRAB_SECURE->read[2];
		grab->secureRead[3] = GRAB_SECURE->read[3];
		grab->auxiliaryRegisters[0] = GRAB_EXTRA[0];
		grab->auxiliaryRegisters[1] = GRAB_EXTRA[1];
	}

	/*
	 * Sony bug kept: the register bus block is filled in the GRAB layout
	 * (size 0x54, secure fields from word 10) but only 0x2c bytes are
	 * kept; the VIP block overwrites the rest.
	 */
	regbus = (BusErrorGrabInfo *)((char *)grab + grabSize);
	regbusSize = 0;
	if (isCompatRamOn()) {
		regbus->size = sizeof(*regbus);
		regbusSize = sizeof(BusErrorRegbusInfo);
		regbus->status = REGBUS->status;
		regbus->write[0] = REGBUS->write[0];
		regbus->write[1] = REGBUS->write[1];
		regbus->write[2] = REGBUS->write[2];
		regbus->write[3] = REGBUS->write[3];
		regbus->secureStatus = REGBUS_SECURE->status;
		regbus->secureWrite[0] = REGBUS_SECURE->write[0];
		regbus->secureWrite[1] = REGBUS_SECURE->write[1];
		regbus->secureWrite[2] = REGBUS_SECURE->write[2];
		regbus->secureWrite[3] = REGBUS_SECURE->write[3];
	}

	vip = (BusErrorVipInfo *)((char *)regbus + regbusSize);
	vip->size = sizeof(*vip);
	if (PERVASIVE_RESET(0x180) == 0 && pervasiveIsOn(0x34)) {
		for (i = 0; i < 3; i++) {
			vip->bap[i] = VIP_PROT[i];
			vip->vdpd[i] = VIP_PROT[3 + i];
			vip->vdpm[i] = VIP_PROT[6 + i];
		}
	} else {
		for (i = 0; i < 3; i++) {
			vip->bap[i] = 0;
			vip->vdpd[i] = 0;
			vip->vdpm[i] = 0;
		}
	}
	if (PERVASIVE_RESET(0x17c) == 0 && pervasiveIsOn(0x30)) {
		for (i = 0; i < 3; i++)
			vip->venezia[i] = VENEZIA_PROT[i];
	} else {
		for (i = 0; i < 3; i++)
			vip->venezia[i] = 0;
	}

	rec->grabSize = grabSize;
	rec->regbusSize = regbusSize;
	rec->ocpSize = sizeof(*ocp);
	rec->siSize = siSize;
	rec->vipSize = sizeof(*vip);
	rec->size = (char *)(vip + 1) - (char *)rec;
	return rec->hdrSize + rec->size;
}

#if FW_DEBUG

/* tables of the decoder */
static const char *const ocpCommands[] = {
	"Idle", "Write", "Read", "ReadEx", "ReadLinked", "WriteNonPost",
	"WriteConditional", "Broadcast",
};

static const char *const ocpMasters[] = {
	"Reserved_0x00",
	"ARM Core0",
	"ARM Core1",
	"ARM Core2",
	"ARM Core3",
	"ARM L2",
	"Reserved_0x06",
	"Reserved_0x07",
	"Reserved_0x08",
	"Reserved_0x09",
	"Reserved_0x0A",
	"Reserved_0x0B",
	"Reserved_0x0C",
	"Reserved_0x0D",
	"Reserved_0x0E",
	"Reserved_0x0F",
	"DMAC0",
	"DMAC1",
	"DMAC2",
	"DMAC3",
	"DMAC4",
	"DMAC5",
	"Reserved_0x16",
	"GPU",
	"Venezia",
	"VIP vdpd",
	"VIP vdpm",
	"VIP bap",
	"Reserved_0x1C",
	"IFTU0a",
	"IFTU0b",
	"IFTU1a",
	"IFTU1b",
	"IFTU2",
	"Reserved_0x22",
	"Reserved_0x23",
	"USB1 Host EHCI",
	"USB1 Host OHCI",
	"Reserved_0x26",
	"Sensor in 0",
	"Sensor in 1",
	"Reserved_0x29",
	"LCD DMAC",
	"Performance Monitor",
	"USB2 Device DMAC",
	"Sub LCD",
	"DMAC6",
	"USB0 Host EHCI",
	"USB0 Host OHCI",
	"USB2 Host EHCI",
	"USB2 Host OHCI",
	"USB0 Device DMAC",
	"Reserved_0x34",
	"Reserved_0x35",
	"Reserved_0x36",
	"USB1 Device DMAC",
	"Reserved_0x38",
	"Reserved_0x39",
	"Reserved_0x3A",
	"SD/HSMMC0",
	"SD/HSMMC1",
	"SD/HSMMC2",
	"SD/HSMMC3",
	"Memory Stick",
};

/*
 * The decoder writes to buf, or to the console when buf (plus what was
 * written) is NULL; this build has no buffer output, every call site passes
 * NULL.
 */
#define BUSERROR_OUT(_line, ...) do {					\
	if (buf + len == NULL)						\
		SCE_DBG_PRINTF(0, _line, __VA_ARGS__);			\
} while (0)

static int __attribute__((noinline)) FormatOcpTargetInfo_ES3(char *buf, const char *name,
							     SceUInt32 address, SceUInt32 attribute)
{
	int len = 0;

	BUSERROR_OUT(1054,
		"   target : %s\n  address : 0x%08x\nattribute : 0x%08x\n"
		"   master : %s\n      cmd : %s\n%s%s%s",
		name, address, attribute, ocpMasters[(attribute >> 16) & 63],
		ocpCommands[(attribute >> 8) & 7],
		(attribute & 8) ? "   reason : Burst Access Error\n" : "",
		(attribute & 4) ? "   reason : Register Permission Error\n" : "",
		(attribute & 1) ? "   reason : Address Hole Error\n" : "");
	return len;
}

static inline int __attribute__((always_inline, unused)) FormatBusErrorInfo_ES3(char *buf, const BusErrorRecord *rec)
{
	const BusErrorOcpInfo *ocp = (const void *)((const char *)rec + rec->hdrSize);
	const BusErrorSiInfo *si = (const void *)((const char *)ocp + rec->ocpSize);
	const BusErrorGrabInfo *grab = (const void *)((const char *)si + rec->siSize);
	const BusErrorRegbusInfo *reg = (const void *)((const char *)grab + rec->grabSize);
	const BusErrorVipInfo *vip = rec->hdrSize > 27 ?
		(const void *)((const char *)reg + rec->regbusSize) : NULL;
	SceUInt32 status0 = ocp->status[0][0] | ocp->status[1][0];
	SceUInt32 status1 = ocp->status[0][1] | ocp->status[1][1];
	SceUInt32 busStatus = ocp->status[0][2] | ocp->status[1][2];
	const BusErrorOcpTarget *target;
	const BusErrorSystemBus *sys;
	const BusErrorSiBus *bus;
	const BusErrorSiAgent *agent;
	const BusErrorPort *port;
	SceUInt32 i, j, first, combined;
	SceUInt64 status;
	int len = 0;

	BUSERROR_OUT(1102,
		"[ Bus error occured on target device ]\nBET0:0x%08x, BET1:0x%08x, BEBT:0x%08x\n\n",
		status0, status1, busStatus);
	for (i = 0; i < ocp->count; i++) {
		/* the first status word has 15 targets, the second three */
		if (i < ARRAY_SIZE(ocpTargets0)) {
			target = &ocpTargets0[i];
			combined = status0;
			first = ocp->status[0][0];
		} else {
			target = &ocpTargets1[i - ARRAY_SIZE(ocpTargets0)];
			combined = status1;
			first = ocp->status[0][1];
		}
		if (!(combined & target->mask))
			continue;
		j = (first & target->mask) ? 0 : 1;
		len += FormatOcpTargetInfo_ES3(buf + len, target->name,
					       ocp->target[i].error[j].address,
					       ocp->target[i].error[j].attribute);
	}

	bus = (const BusErrorSiBus *)(si + 1);
	for (i = 0; i < si->count; i++) {
		for (sys = systemBuses; sys != &systemBuses[ARRAY_SIZE(systemBuses)]; sys++)
			if (sys->address == bus->address)
				break;
		/* Sony stops at the first unknown bus */
		if (sys == &systemBuses[ARRAY_SIZE(systemBuses)])
			break;
		if (busStatus & sys->mask)
			BUSERROR_OUT(1201,
				"%s\nSI_FLAG_STATUS0:    0x%016llx\n"
				"SI_FLAG_STATUS1:    0x%016llx\n"
				"SI_FLAG_STATUS2:    0x%016llx\n"
				"SI_FLAG_STATUS3:    0x%016llx\n"
				"SI_FLAG_STATUS4:    0x%016llx\n",
				sys->name, bus->flagStatus[0], bus->flagStatus[1],
				bus->flagStatus[2], bus->flagStatus[3], bus->flagStatus[4]);
		agent = (const BusErrorSiAgent *)(bus + 1);
		for (j = 0; j < bus->initiatorCount; j++) {
			for (port = sys->initiators; port->name != NULL; port++)
				if (port->address == agent->address)
					break;
			if (port->name != NULL && agent->status != 0)
				BUSERROR_OUT(1234,
					"%40s: STATUS=0x%08X, ERROR_LOG=0x%012llX, ADDR=0x%08llX\n",
					port->name, (SceUInt32)agent->status, agent->errorLog,
					agent->errorAddress);
			agent = (const void *)((const char *)agent + agent->size);
		}
		for (j = 0; j < bus->targetCount; j++) {
			for (port = sys->targets; port->name != NULL; port++)
				if (port->address == agent->address)
					break;
			status = (SceUInt32)agent->status & 0xffff0fff;
			if (port->name != NULL && status != 0)
				BUSERROR_OUT(1272,
					"%40s: STATUS=0x%08X, ERROR_LOG=0x%012llX, ADDR=0x%08llX\n",
					port->name, (SceUInt32)status, agent->errorLog,
					agent->errorAddress);
			agent = (const void *)((const char *)agent + agent->size);
		}
		bus = (const BusErrorSiBus *)agent;
	}

	if (grab->status & 4)
		BUSERROR_OUT(1304,
			"GRAB_WERRID:   0x%08X\nGRAB_WERRADDR: 0x%08X\n"
			"GRAB_WERRATTR: 0x%08X\nGRAB_WERRINFO: 0x%08X\n",
			grab->write[0], grab->write[1], grab->write[2], grab->write[3]);
	if (grab->status & 2)
		BUSERROR_OUT(1327,
			"GRAB_RERRID:   0x%08X\nGRAB_RERRADDR: 0x%08X\n"
			"GRAB_RERRATTR: 0x%08X\nGRAB_RERRINFO: 0x%08X\n",
			grab->read[0], grab->read[1], grab->read[2], grab->read[3]);
	if (grab->secureStatus & 4)
		BUSERROR_OUT(1351,
			"GRAB_SECWERRID:   0x%08X\nGRAB_SECWERRADDR: 0x%08X\n"
			"GRAB_SECWERRATTR: 0x%08X\nGRAB_SECWERRINFO: 0x%08X\n",
			grab->secureWrite[0], grab->secureWrite[1], grab->secureWrite[2],
			grab->secureWrite[3]);
	if (grab->secureStatus & 2)
		BUSERROR_OUT(1374,
			"GRAB_SECRERRID:   0x%08X\nGRAB_SECRERRADDR: 0x%08X\n"
			"GRAB_SECRERRATTR: 0x%08X\nGRAB_SECRERRINFO: 0x%08X\n",
			grab->secureRead[0], grab->secureRead[1], grab->secureRead[2],
			grab->secureRead[3]);
	if (reg->status & 1)
		BUSERROR_OUT(1400,
			"REGBUS_ERRID:   0x%08X\nREGBUS_ERRADDR: 0x%08X\n"
			"REGBUS_ERRATTR: 0x%08X\nREGBUS_ERRINFO: 0x%08X\n",
			reg->error[0], reg->error[1], reg->error[2], reg->error[3]);
	if (reg->secureStatus & 1)
		BUSERROR_OUT(1423,
			"REGBUS_SECERRID:   0x%08X\nREGBUS_SECERRADDR: 0x%08X\n"
			"REGBUS_SECERRATTR: 0x%08X\nREGBUS_SECERRINFO: 0x%08X\n",
			reg->secureError[0], reg->secureError[1], reg->secureError[2],
			reg->secureError[3]);
	if (vip == NULL)
		return 0;
	if (vip->bap[0] != 0)
		BUSERROR_OUT(1446,
			"VIP_PROT_BAP_ERRV:  0x%08X\nVIP_PROT_BAP_ERRC:  0x%08X\nVIP_PROT_BAP_ERRA:  0x%08X\n",
			vip->bap[0], vip->bap[1], vip->bap[2]);
	if (vip->vdpd[0] != 0)
		BUSERROR_OUT(1466,
			"VIP_PROT_VDPD_ERRV: 0x%08X\nVIP_PROT_VDPD_ERRC: 0x%08X\nVIP_PROT_VDPD_ERRA: 0x%08X\n",
			vip->vdpd[0], vip->vdpd[1], vip->vdpd[2]);
	if (vip->vdpm[0] != 0)
		BUSERROR_OUT(1487,
			"VIP_PROT_VDPM_ERRV: 0x%08X\nVIP_PROT_VDPM_ERRC: 0x%08X\nVIP_PROT_VDPM_ERRA: 0x%08X\n",
			vip->vdpm[0], vip->vdpm[1], vip->vdpm[2]);
	if (vip->venezia[0] != 0)
		BUSERROR_OUT(1507,
			"VENE_PROT_REG_ERRV: 0x%08X\nVENE_PROT_REG_ERRC: 0x%08X\nVENE_PROT_REG_ERRA: 0x%08X\n",
			vip->venezia[0], vip->venezia[1], vip->venezia[2]);
	return 0;
}

#endif /* FW_DEBUG */

#endif /* SCE_BUSERROR_TABLES_ONLY */

#pragma pop_macro("__FILE_NAME__")
