/*
 * Clock requests and the reference clock tables (sys/clock.c). The chip's
 * clocks are picked from tables of rates in units of 100 kHz; every client
 * (up to CLOCK_CLIENTS of them) asks for the rate it needs in each of four
 * clock domains and the fastest request wins. The names are inferred from
 * use; no Marvell source names this code.
 */
#ifndef ROBIN_CLOCK_H
#define ROBIN_CLOCK_H

#include <stdint.h>

#define CLOCK_CLIENTS		6
#define CLOCK_DOMAINS		4

/* The domains, in the order of the request table. */
#define CLOCK_DOMAIN_CORE	0	/* the processor and bus clock; sets sysClockFrequency */
#define CLOCK_DOMAIN_AUX	1	/* a clock selector field of SOC_REG(0x108) */
#define CLOCK_DOMAIN_BUS	2	/* divider fields of SOC_REG(0x30) and SOC_REG(0x9c) */
#define CLOCK_DOMAIN_VOTE	3	/* only recorded */

/* What the chip wakes at, the rate a client asks for, in kHz. */
#define CLOCK_KHZ_BASE		4000

uint32_t clockRequest(uint32_t khz, unsigned int client, int high, int low);
uint32_t clockRequestCore(uint32_t khz, unsigned int client);
uint32_t clockRequestBus(uint32_t khz, unsigned int client);
uint32_t clockRequestAux(uint32_t khz, unsigned int client);
uint32_t clockRequestVote(uint32_t khz, unsigned int client);
uint32_t clockRequestRead(unsigned int client, int high, int low);
uint32_t clockRequestedCore(unsigned int client);
uint32_t clockRequestedBus(unsigned int client);
uint32_t clockRequestedVote(unsigned int client);

unsigned int clockIndexFor(uint32_t khz);
unsigned int clockSourceMode(void);
uint32_t clockCurrentRate(void);
uint32_t clockSetCore(uint32_t khz);
int clockSetBus(int khz);
unsigned int clockRegistersInit(void);
uint32_t clockReferenceRate(void);
uint32_t clockBaseRate(void);
int clockSelectSource(int enable);
unsigned int clockApplyCore(unsigned int khz);
uint32_t clockSlowestRate(void);
uint32_t clockSlowestRateGet(void);
const uint8_t *clockDividerRowSelect(void);
void clockPllProgram(void);
unsigned int clockSelectProgram(void);

/* The clock rate (client 3) the host commands choose. */
void clockHostSetMax(void);
void clockHostSetMin(void);
void clockHostApplyTarget(void);
void clockHostSelectPreset0(void);
void clockHostSelectPreset1(void);
void clockHostSelectPreset2(void);
void clockHostSelectPreset3(void);
extern uint32_t hostClockPreset[4];	/* DTCM 0x040089b0 */

/* The reference clock profile, a strap of SOC_REG(0): an index into the tables. */
int sysRefClockProfile(void);
extern const uint16_t rfCalibrationTable[];	/* 0x53d50 */

#endif /* ROBIN_CLOCK_H */
