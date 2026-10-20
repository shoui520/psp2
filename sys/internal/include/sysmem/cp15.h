/*
 * Coprocessor register access, one MRC/MCR per accessor: C has no other way
 * to reach CP15 (system control) or CP14 (debug). Names are ARM's register
 * names. Reads are plain; writes carry a "memory" clobber so they stay
 * ordered with the loads and stores around them (DACR is widened around a
 * single store, the context ID changes the address space).
 */
#pragma once

#define CP_READ(coproc, opc1, crn, crm, opc2) ({				\
	unsigned int __value;							\
	__asm__ volatile("mrc " #coproc ", " #opc1 ", %0, " #crn ", " #crm ", " #opc2	\
			 : "=r" (__value) :: );						\
	__value;								\
})

#define CP_WRITE(coproc, opc1, crn, crm, opc2, value)				\
	__asm__ volatile("mcr " #coproc ", " #opc1 ", %0, " #crn ", " #crm ", " #opc2	\
			 :: "r" (value) : "memory")

/* CP15 identification */
#define readMidr()		CP_READ(p15, 0, c0, c0, 0)
#define readCtr()		CP_READ(p15, 0, c0, c0, 1)
#define readTcmtr()		CP_READ(p15, 0, c0, c0, 2)
#define readTlbtr()		CP_READ(p15, 0, c0, c0, 3)
#define readMpidr()		CP_READ(p15, 0, c0, c0, 5)
#define readIdPfr0()		CP_READ(p15, 0, c0, c1, 0)
#define readIdPfr1()		CP_READ(p15, 0, c0, c1, 1)
#define readIdDfr0()		CP_READ(p15, 0, c0, c1, 2)
#define readIdMmfr0()		CP_READ(p15, 0, c0, c1, 4)
#define readIdMmfr1()		CP_READ(p15, 0, c0, c1, 5)
#define readIdMmfr2()		CP_READ(p15, 0, c0, c1, 6)
#define readIdMmfr3()		CP_READ(p15, 0, c0, c1, 7)
#define readIdIsar0()		CP_READ(p15, 0, c0, c2, 0)
#define readIdIsar1()		CP_READ(p15, 0, c0, c2, 1)
#define readIdIsar2()		CP_READ(p15, 0, c0, c2, 2)
#define readIdIsar3()		CP_READ(p15, 0, c0, c2, 3)
#define readIdIsar4()		CP_READ(p15, 0, c0, c2, 4)
#define readCcsidr()		CP_READ(p15, 1, c0, c0, 0)
#define readClidr()		CP_READ(p15, 1, c0, c0, 1)
#define readAidr()		CP_READ(p15, 1, c0, c0, 7)
#define readCsselr()		CP_READ(p15, 2, c0, c0, 0)
#define writeCsselr(v)		CP_WRITE(p15, 2, c0, c0, 0, v)

/* CP15 system control */
#define readSctlr()		CP_READ(p15, 0, c1, c0, 0)
#define writeSctlr(v)		CP_WRITE(p15, 0, c1, c0, 0, v)
#define readActlr()		CP_READ(p15, 0, c1, c0, 1)
#define readCpacr()		CP_READ(p15, 0, c1, c0, 2)
#define readScr()		CP_READ(p15, 0, c1, c1, 0)	/* Secure state only */
#define readNsacr()		CP_READ(p15, 0, c1, c1, 2)
#define NSACR_PLE		0x00010000	/* the preload engine is usable */
#define writeCpacr(v)		CP_WRITE(p15, 0, c1, c0, 2, v)
#define readTtbr0()		CP_READ(p15, 0, c2, c0, 0)
#define writeTtbr0(v)		CP_WRITE(p15, 0, c2, c0, 0, v)
#define readTtbr1()		CP_READ(p15, 0, c2, c0, 1)
#define writeTtbr1(v)		CP_WRITE(p15, 0, c2, c0, 1, v)
#define readTtbcr()		CP_READ(p15, 0, c2, c0, 2)
#define writeTtbcr(v)		CP_WRITE(p15, 0, c2, c0, 2, v)
#define readDacr()		CP_READ(p15, 0, c3, c0, 0)
#define writeDacr(v)		CP_WRITE(p15, 0, c3, c0, 0, v)
#define DACR_ALL_CLIENT		0x55555555	/* every domain checked against the page tables */
#define DACR_ALL_MANAGER	0xffff0000	/* domains 8-15 unchecked: vector page writable */
#define DACR_KERNEL		0x15450000	/* the kernel's domains (8, 9, 11-14) checked */
#define DACR_USER		0x15455551	/* and the user's (0, 2-7) */
#define readDfsr()		CP_READ(p15, 0, c5, c0, 0)
#define writeDfsr(v)		CP_WRITE(p15, 0, c5, c0, 0, v)
#define readIfsr()		CP_READ(p15, 0, c5, c0, 1)
#define writeIfsr(v)		CP_WRITE(p15, 0, c5, c0, 1, v)
#define readAdfsr()		CP_READ(p15, 0, c5, c1, 0)
#define readAifsr()		CP_READ(p15, 0, c5, c1, 1)
#define writeAdfsr(v)		CP_WRITE(p15, 0, c5, c1, 0, v)
#define writeAifsr(v)		CP_WRITE(p15, 0, c5, c1, 1, v)
#define readDfar()		CP_READ(p15, 0, c6, c0, 0)
#define writeDfar(v)		CP_WRITE(p15, 0, c6, c0, 0, v)
#define readIfar()		CP_READ(p15, 0, c6, c0, 2)
#define writeIfar(v)		CP_WRITE(p15, 0, c6, c0, 2, v)
#define writeAts1cpr(v)		CP_WRITE(p15, 0, c7, c8, 0, v)	/* translate, privileged read */
#define writeAts1cpw(v)		CP_WRITE(p15, 0, c7, c8, 1, v)	/* translate, privileged write */
#define writeAts1cur(v)		CP_WRITE(p15, 0, c7, c8, 2, v)	/* translate, user read */
#define writeAts1cuw(v)		CP_WRITE(p15, 0, c7, c8, 3, v)	/* translate, user write */
#define readPar()		CP_READ(p15, 0, c7, c4, 0)
#define writePar(v)		CP_WRITE(p15, 0, c7, c4, 0, v)
/* cache and branch predictor maintenance; IS: all cores in the inner shareable domain */
#define writeIcialluis(v)	CP_WRITE(p15, 0, c7, c1, 0, v)
#define writeBpiallis(v)	CP_WRITE(p15, 0, c7, c1, 6, v)
#define writeIciallu(v)		CP_WRITE(p15, 0, c7, c5, 0, v)
#define writeIcimvau(v)		CP_WRITE(p15, 0, c7, c5, 1, v)
#define writeBpiall(v)		CP_WRITE(p15, 0, c7, c5, 6, v)
#define writeDcimvac(v)		CP_WRITE(p15, 0, c7, c6, 1, v)
#define writeDcisw(v)		CP_WRITE(p15, 0, c7, c6, 2, v)
#define writeDccmvac(v)		CP_WRITE(p15, 0, c7, c10, 1, v)
#define writeDccsw(v)		CP_WRITE(p15, 0, c7, c10, 2, v)
#define writeDccimvac(v)	CP_WRITE(p15, 0, c7, c14, 1, v)
#define writeDccisw(v)		CP_WRITE(p15, 0, c7, c14, 2, v)

/* TLB maintenance */
#define writeTlbimvais(v)	CP_WRITE(p15, 0, c8, c3, 1, v)
#define writeTlbimvaais(v)	CP_WRITE(p15, 0, c8, c3, 3, v)
#define writeTlbiall(v)		CP_WRITE(p15, 0, c8, c7, 0, v)

#define readPmcr()		CP_READ(p15, 0, c9, c12, 0)
#define writePmcr(v)		CP_WRITE(p15, 0, c9, c12, 0, v)
#define readPmcntenset()	CP_READ(p15, 0, c9, c12, 1)
#define writePmcntenclr(v)	CP_WRITE(p15, 0, c9, c12, 2, v)
#define readPmovsr()		CP_READ(p15, 0, c9, c12, 3)
#define writePmswinc(v)		CP_WRITE(p15, 0, c9, c12, 4, v)
#define readPmselr()		CP_READ(p15, 0, c9, c12, 5)
#define writePmselr(v)		CP_WRITE(p15, 0, c9, c12, 5, v)
#define readPmccntr()		CP_READ(p15, 0, c9, c13, 0)
#define readPmxevtyper()	CP_READ(p15, 0, c9, c13, 1)
#define readPmxevcntr()		CP_READ(p15, 0, c9, c13, 2)
#define readPmuserenr()		CP_READ(p15, 0, c9, c14, 0)

/* PMCR and PMCNTENSET/CLR bits (ARMv7-A) */
#define PMCR_E			0x01		/* counters enabled */
#define PMCR_X			0x10		/* events exported */
#define PMCNTEN_C		0x80000000	/* the cycle counter */

/* memory region remap */
#define readPrrr()		CP_READ(p15, 0, c10, c2, 0)
#define readNmrr()		CP_READ(p15, 0, c10, c2, 1)

/* the preload engine (PLE) */
#define readPleasr()		CP_READ(p15, 0, c11, c0, 2)	/* activity status */
#define readPleuar()		CP_READ(p15, 0, c11, c1, 0)	/* user accessibility */
#define readPlepcr()		CP_READ(p15, 0, c11, c1, 1)	/* channel control */

#define readVbar()		CP_READ(p15, 0, c12, c0, 0)
#define writeVbar(v)		CP_WRITE(p15, 0, c12, c0, 0, v)
#define readMvbar()		CP_READ(p15, 0, c12, c0, 1)	/* Secure state only */
#define writeMvbar(v)		CP_WRITE(p15, 0, c12, c0, 1, v)	/* Secure state only */
#define readFcseidr()		CP_READ(p15, 0, c13, c0, 0)
#define readContextId()		CP_READ(p15, 0, c13, c0, 1)
#define writeContextId(v)	CP_WRITE(p15, 0, c13, c0, 1, v)
#define readTpidrurw()		CP_READ(p15, 0, c13, c0, 2)
#define writeTpidrurw(v)	CP_WRITE(p15, 0, c13, c0, 2, v)
#define readTpidruro()		CP_READ(p15, 0, c13, c0, 3)
#define writeTpidruro(v)	CP_WRITE(p15, 0, c13, c0, 3, v)
#define readTpidrprw()		CP_READ(p15, 0, c13, c0, 4)
#define writeTpidrprw(v)	CP_WRITE(p15, 0, c13, c0, 4, v)

/* Cortex-A9 Power Control Register */
#define readPowerControl()	CP_READ(p15, 0, c15, c0, 0)

/* CP14 debug */
#define readDbgdscr()		CP_READ(p14, 0, c0, c1, 0)
#define readDbgdscrExt()	CP_READ(p14, 0, c0, c2, 2)

/* CP14 ThumbEE */
#define readTeecr()		CP_READ(p14, 6, c0, c0, 0)
#define writeTeecr(v)		CP_WRITE(p14, 6, c0, c0, 0, v)
#define readTeehbr()		CP_READ(p14, 6, c1, c0, 0)
#define writeTeehbr(v)		CP_WRITE(p14, 6, c1, c0, 0, v)

/* CP14 Jazelle (c3: implementation defined, the v1 array object layout) */
#define readJidr()		CP_READ(p14, 7, c0, c0, 0)
#define readJoscr()		CP_READ(p14, 7, c1, c0, 0)
#define writeJoscr(v)		CP_WRITE(p14, 7, c1, c0, 0, v)
#define readJmcr()		CP_READ(p14, 7, c2, c0, 0)
#define writeJmcr(v)		CP_WRITE(p14, 7, c2, c0, 0, v)
#define readJaolr()		CP_READ(p14, 7, c3, c0, 0)

/* breakpoint and watchpoint n (a constant: it is part of the instruction) */
#define writeDbgbvr(n, v)	CP_WRITE(p14, 0, c0, c##n, 4, v)
#define writeDbgbcr(n, v)	CP_WRITE(p14, 0, c0, c##n, 5, v)
#define writeDbgwvr(n, v)	CP_WRITE(p14, 0, c0, c##n, 6, v)
#define writeDbgwcr(n, v)	CP_WRITE(p14, 0, c0, c##n, 7, v)
