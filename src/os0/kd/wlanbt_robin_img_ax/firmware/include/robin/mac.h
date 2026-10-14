/*
 * MAC register file at 0x8000a000. Marvell published no register map for the
 * 88W8787; every name is positional (offset, then the role the code gives the
 * register). Registers that only one function touches are written with
 * MAC_REG() in that function.
 */
#ifndef ROBIN_MAC_H
#define ROBIN_MAC_H

#include <stdint.h>
#include "robin/soc.h"

/* Bases of the first three DMA regions, loaded by wlanMacLoadRegions(). */
#define MAC_000_REGION_A	MAC_REG(0x000)
#define MAC_004_REGION_B	MAC_REG(0x004)
#define MAC_008_REGION_C	MAC_REG(0x008)
/* One bit per policy slot, kept in step with a software copy. */
#define MAC_090_POLICY_MASK	MAC_REG(0x090)
#define MAC_13C_CONFIG		MAC_REG(0x13c)
/* Bits requested pending by software, and the hardware's answer. */
#define MAC_254_PENDING_REQUEST	MAC_REG(0x254)
#define MAC_258_PENDING_STATUS	MAC_REG(0x258)
#define MAC_2D0_FLAGS		MAC_REG(0x2d0)
#define MAC_300_MODE		MAC_REG(0x300)
/*
 * Indexed window onto an internal table of 6-byte entries (192..255): write
 * the data words and then the index with the write command (0x400), or write
 * the index with the read command (0x200) and read the words. Bit 16 of the
 * command word is busy.
 */
#define MAC_310_TABLE_COMMAND	MAC_REG(0x310)
#define MAC_314_TABLE_LOW	MAC_REG(0x314)
#define MAC_318_TABLE_HIGH	MAC_REG(0x318)
#define MAC_TABLE_BUSY		0x10000
#define MAC_TABLE_WRITE		0x400
#define MAC_TABLE_READ		0x200
#define MAC_TABLE_FIRST		192
#define MAC_TABLE_COUNT		64
#define MAC_46C_FILTER		MAC_REG(0x46c)
#define MAC_474_VALUE		MAC_REG(0x474)
#define MAC_5F8_STATUS		MAC_REG(0x5f8)
/* Start bits and a two-bit progress field (bits 4-5 of the status). */
#define MAC_650_OPERATION	MAC_REG(0x650)
#define MAC_658_OPERATION_STATUS MAC_REG(0x658)
#define MAC_868_ENABLE		MAC_REG(0x868)
#define MAC_E80_CONTROL		MAC_REG(0xe80)
/*
 * Block Ack hardware slots: bit n of AC0 marks slot n active, ACC holds a
 * nibble per window (slot number | 8), E00 bit 8+w enables window w, EC0+4w
 * is the counter of window w.
 */
#define MAC_AC0_BA_ACTIVE	MAC_REG(0xac0)
#define MAC_ACC_BA_WINDOWS	MAC_REG(0xacc)
#define MAC_E00_BA_CONTROL	MAC_REG(0xe00)
#define MAC_EC0_BA_COUNTER(window) MAC_REG(0xec0 + 4 * (window))

/* ROM routines behind the MAC's indexed entries. */
int romMacEntryCheck(unsigned int index, uint32_t value);			/* 0x03f3ac3d */
int romMacEntryWrite(unsigned int index, uint32_t first, uint32_t second);	/* 0x03f3ac8b */

struct MacRegionProvider {
	uint32_t reserved;
	uint32_t (*lookup)(unsigned int region);
};

int wlanMacPollOperation(unsigned int limit, uint32_t *status, void (*step)(int));
uint32_t wlanMacProgramConfig(uint32_t a, uint8_t b);
void wlanMacSetPolicyBit(uint32_t *mask, uint32_t code);
int wlanMacEntryCheckPair(unsigned int index, const uint32_t *entry, int upper);
int wlanMacEntryWritePair(unsigned int index, const uint32_t *entry);
int wlanMacEntryWriteTable(unsigned int index, unsigned int count, const uint32_t *entries);
int wlanMacEntryCheckTable(unsigned int index, unsigned int count, const uint32_t *entries, int upper);
void wlanMacClear46cBits(void);
void wlanMacSet46cBits(void);
void wlanMacWrite474(uint32_t value);
uint32_t wlanMacRead5f8(void);
void wlanMacSetE80Bits(void);
int wlanMacTableWrite(uint32_t low, uint32_t high, unsigned int index);
int wlanMacTableRead(uint16_t *entry, unsigned int index);
int wlanMacLoadRegions(const struct MacRegionProvider *provider);
int wlanHalfwordsEqual(const uint16_t *const *first, const uint16_t *const *second, unsigned int count);
int wlanMacWriteChecked(unsigned int from, unsigned int to, unsigned int page, uint32_t value);
void wlanMacClear868Enable(void);
unsigned int wlanFindSetBitDown(uint32_t mask, unsigned int start, unsigned int high, unsigned int low);

#define MAC_2D4_FLAGS		MAC_REG(0x2d4)
#define MAC_824_CONTROL		MAC_REG(0x824)

extern int (*wlanTxRingsResetHook)(void);
extern int (*wlanModeVetoHook)(void);
int romMacConfigureEntry(unsigned int index, unsigned int value);	/* 0x03f3a865 */
int wlanMacMode300Apply(int on);
int wlanMacMode300Reset(void);
void *wlanMacModeSwitch(int on);
int wlanTxRingsReset(void);
int wlanHookReturnZero(void);
int wlanTxHooksInit(void);

void wlanBaWindowEnable(int window);
void wlanBaWindowDisable(int window);
int wlanBaSlotApply(int slot, int window);
int wlanBaSlotRelease(int slot, int window);
void wlanBaWindowCounterReset(int window);
void wlanMacE80Select(void);

#endif /* ROBIN_MAC_H */
