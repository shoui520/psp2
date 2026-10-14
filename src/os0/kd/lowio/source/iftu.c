/* SceIftu image format conversion. Private context field names are unknown. */
#include "lowio_internal.h"
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sce/sysevent.h>
#include <stdbool.h>

/* Raw context layout: five contexts, each 0x214 bytes. */
static uint32_t s_iftuState[5][0x85];
static SceKernelFastMutex s_iftuFastMutex;
#define IFTU_WORD(offset) (*(uint32_t *)((uint8_t *)s_iftuState + (offset)))
#define IFTU_REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))
#define s_iftuBase2 ((volatile uint32_t *)s_iftuState[4][0])

uint32_t ScePervasiveForDriver_07F2A738(void);
uint32_t ScePervasiveForDriver_E92E28FF(void);
uint32_t ScePervasiveForDriver_5AFE0AF0(void);
/* Intermediate context field names remain unknown. */
static int Iftu2IntrHandler(int param_1, void *arg)
{
	int *param_2 = arg;
	int i1;
	(void)param_1;

	i1 = *param_2;
	(void)*(volatile uint32_t *)(i1 + 0x40);
	*(volatile uint32_t *)(i1 + 0x40) = 0;
	(void)*(volatile uint32_t *)(i1 + 0x40);
	LowioDsb();
	ksceKernelSetEventFlag(param_2[0x84], 1);
	return 0xffffffff;
}

/* Intermediate context field names remain unknown. */
static int IftuIntrHandler(int param_1, void *arg)
{
	int *param_2 = arg;
	uint32_t unk1;
	int i2;
	uint32_t unk3;
	int i4;
	int i5;
	int *p6;
	int i7;
	int i8;
	int i9;
	int i10;
	int i11;
	int i12;
	int i13;
	int i14;
	(void)param_1;

	p6 = param_2 + 0x80;
	i5 = *param_2;
	unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(p6);
	if (param_2[0x7a] != 0) {
		unk3 = param_2[0x7b];
		(void)*(volatile uint32_t *)(i5 + 0x40);
		*(volatile uint32_t *)(i5 + 0x50) = 0;
		*(volatile uint32_t *)(i5 + 0x40) = 0;
		(void)*(volatile uint32_t *)(i5 + 0x40);
		if (unk3 != 0) {
			*(volatile int *)(i5 + 0x180) = param_2[0x7d];
			if ((int)unk3 < 0) {
				i7 = ((unk3 & 1) + 2) * 0x100;
				i4 = i7 + i5;
				if (param_2[0x61] == 0) {
					i7 = param_2[0x76];
					i5 = param_2[0x77];
					*(volatile uint32_t *)(i4 + 0x4c) = 1;
					*(volatile uint32_t *)(i4 + 0x40) = 0x10;
					*(volatile int *)(i4 + 0x44) = i7;
					*(volatile int *)(i4 + 0x48) = i5;
					*(volatile uint32_t *)(i4 + 0x60) = 0;
					*(volatile uint32_t *)(i4 + 100) = 0;
					*(volatile uint32_t *)(i4 + 0x68) = 0;
					*(volatile uint32_t *)(i4 + 0x6c) = 0;
					*(volatile uint32_t *)(i4 + 0xc0) = 0x10000;
					*(volatile uint32_t *)(i4 + 0xc4) = 0x10000;
				} else {
					i12 = param_2[0x62];
					i11 = param_2[99];
					i10 = param_2[100];
					i9 = param_2[0x65];
					i13 = param_2[0x66];
					i14 = param_2[0x67];
					i8 = param_2[0x68];
					*(volatile int *)(i4 + 0x40) = param_2[0x61];
					i2 = param_2[0x69];
					*(volatile int *)(i4 + 0x44) = i12;
					i12 = param_2[0x6a];
					*(volatile int *)(i4 + 0x48) = i11;
					i11 = param_2[0x6b];
					*(volatile int *)(i4 + 0x54) = i10;
					i10 = param_2[0x72];
					*(volatile int *)(i4 + 0x58) = i9;
					i9 = param_2[0x73];
					*(volatile int *)(i5 + i7) = i13;
					i5 = param_2[0x6c];
					i13 = param_2[0x74];
					i7 = param_2[0x75];
					*(volatile int *)(i4 + 4) = i14;
					i14 = param_2[0x6d];
					*(volatile int *)(i4 + 8) = i8;
					i8 = param_2[0x6e];
					*(volatile int *)(i4 + 0x20) = i2;
					*(volatile int *)(i4 + 0x24) = i12;
					*(volatile int *)(i4 + 0x28) = i11;
					*(volatile int *)(i4 + 0x60) = i10;
					*(volatile int *)(i4 + 100) = i9;
					*(volatile int *)(i4 + 0x68) = i13;
					*(volatile int *)(i4 + 0x6c) = i7;
					*(volatile int *)(i4 + 0xc0) = i5;
					*(volatile int *)(i4 + 0xc4) = i14;
					i5 = param_2[0x71];
					*(volatile int *)(i4 + 200) = i8;
					i7 = param_2[0x70];
					*(volatile int *)(i4 + 0xcc) = param_2[0x6f];
					*(volatile int *)(i4 + 0xd0) = i7;
					*(volatile int *)(i4 + 0xd4) = i5;
					*(volatile uint32_t *)(i4 + 0x4c) = 0;
				}
			}
			param_2[0x7b] = 0;
		}
		LowioDsb();
		ksceKernelSpinlockLowUnlockCpuResumeIntr(p6, unk1);
		return 0xffffffff;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(p6, unk1);
	return 0xfffffffe;
}

/* Intermediate context field names remain unknown. */
static int IftuSysEvent(int param_1, int param_2, void *args, void *opt)
{
	uint32_t unk1;
	uint32_t unk2;
	uint32_t unk3;
	uint32_t unk4;
	uint32_t unk5;
	uint32_t unk6;
	uint32_t unk7;
	uint32_t unk8;
	uint32_t unk9;
	uint32_t unk10;
	uint32_t unk11;
	uint32_t unk12;
	uint32_t unk13;
	int i14;
	int *p15;
	int *p16;
	(void)args;
	(void)opt;

	if (param_1 == 0) {
		if (param_2 == 0x4005) {
			p15 = (int *)&IFTU_WORD(0x0);
			do {
				p16 = p15 + 0x80;
				unk11 = ksceKernelSpinlockLowLockCpuSuspendIntr(p16);
				if (p15[0x7a] != 0) {
					i14 = *p15;
					p15[0x7a] = 0;
					*(volatile uint32_t *)(i14 + 0x24c) = 1;
					*(volatile uint32_t *)(i14 + 0x34c) = 1;
					*(volatile uint32_t *)(i14 + 0x50) = 0;
					*(volatile uint32_t *)(i14 + 0x40) = 0;
					(void)*(volatile uint32_t *)(i14 + 0x40);
					LowioDsb();
					ksceKernelDisableIntr(p15[0x81]);
				}
				p15 = p15 + 0x85;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(p16, unk11);
			} while (p15 != (int *)s_iftuState[4]);
			return 0;
		}
	} else if ((param_1 == 1) && (param_2 == 0x10005)) {
		ScePervasiveForDriver_07F2A738();
		ScePervasiveForDriver_E92E28FF();
		unk11 = IFTU_WORD(0xa54);
		(void)IFTU_REG((uint8_t *)s_iftuState[4][0], 0x58);
		LowioDsb();
		IFTU_REG((uint8_t *)s_iftuState[4][0], 0x58) = 0x108;
		ksceKernelEnableIntr(unk11);
		unk12 = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&IFTU_WORD(0xa50));
		unk13 = IFTU_WORD(0x9d0);
		unk9 = IFTU_WORD(0x9cc);
		unk8 = IFTU_WORD(0x9c8);
		unk7 = IFTU_WORD(0x9c4);
		unk6 = IFTU_WORD(0x9c0);
		unk5 = IFTU_WORD(0x9bc);
		unk4 = IFTU_WORD(0x9b8);
		unk3 = IFTU_WORD(0x9b4);
		unk2 = IFTU_WORD(0x9b0);
		unk1 = IFTU_WORD(0x9a0);
		unk11 = IFTU_WORD(0x99c);
		i14 = s_iftuState[4][0];
		if (IFTU_WORD(0xa38) != 0) {
			IFTU_REG((uint8_t *)s_iftuState[4][0], 0x130) =
			    IFTU_WORD(0x998);
			unk10 = IFTU_WORD(0x9a4);
			*(volatile uint32_t *)(i14 + 0x134) = unk11;
			unk11 = IFTU_WORD(0x9a8);
			*(volatile uint32_t *)(i14 + 0x138) = unk2;
			unk2 = IFTU_WORD(0x9ac);
			*(volatile uint32_t *)(i14 + 0x13c) = unk3;
			*(volatile uint32_t *)(i14 + 0x140) = unk4;
			*(volatile uint32_t *)(i14 + 0x144) = unk5;
			*(volatile uint32_t *)(i14 + 0x148) = unk6;
			*(volatile uint32_t *)(i14 + 0x14c) = unk7;
			*(volatile uint32_t *)(i14 + 0x150) = unk8;
			*(volatile uint32_t *)(i14 + 0x154) = unk9;
			*(volatile uint32_t *)(i14 + 0x158) = unk13;
			*(volatile uint32_t *)(i14 + 0x15c) = unk1;
			*(volatile uint32_t *)(i14 + 0x160) = unk10;
			*(volatile uint32_t *)(i14 + 0x164) = unk11;
			*(volatile uint32_t *)(i14 + 0x168) = unk2;
			LowioDmb();
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&IFTU_WORD(0xa50), unk12);
		unk13 = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&IFTU_WORD(0xa50));
		unk9 = IFTU_WORD(0x994);
		unk8 = IFTU_WORD(0x990);
		unk7 = IFTU_WORD(0x98c);
		unk6 = IFTU_WORD(0x988);
		unk5 = IFTU_WORD(0x984);
		unk4 = IFTU_WORD(0x980);
		unk3 = IFTU_WORD(0x97c);
		unk2 = IFTU_WORD(0x978);
		unk1 = IFTU_WORD(0x974);
		unk11 = IFTU_WORD(0x960);
		i14 = s_iftuState[4][0];
		if (IFTU_WORD(0xa38) != 0) {
			IFTU_REG((uint8_t *)s_iftuState[4][0], 0x104) =
			    IFTU_WORD(0x95c);
			*(volatile uint32_t *)(i14 + 0x108) = unk11;
			*(volatile uint32_t *)(i14 + 0x10c) = unk1;
			*(volatile uint32_t *)(i14 + 0x110) = unk2;
			*(volatile uint32_t *)(i14 + 0x114) = unk3;
			*(volatile uint32_t *)(i14 + 0x118) = unk4;
			*(volatile uint32_t *)(i14 + 0x11c) = unk5;
			*(volatile uint32_t *)(i14 + 0x120) = unk6;
			*(volatile uint32_t *)(i14 + 0x124) = unk7;
			*(volatile uint32_t *)(i14 + 0x128) = unk8;
			*(volatile uint32_t *)(i14 + 300) = unk9;
			LowioDmb();
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&IFTU_WORD(0xa50), unk13);
		(void)IFTU_REG((uint8_t *)s_iftuState[4][0], 0x58);
		LowioDsb();
		ScePervasiveForDriver_5AFE0AF0();
		return 0;
	}
	return 0;
}

/* Intermediate context field names remain unknown. */
uint32_t SceIftuForDriver_0FCBF457(unsigned param_1, uint32_t *param_2)
{
	uint32_t unk1;
	uint32_t *p2;
	uint32_t *p3;
	uint32_t *p4;
	uint32_t *p5;
	int i6;
	uint32_t unk7;
	int i8;
	uint32_t unk9;
	uint32_t unk10;
	uint32_t unk11;
	uint32_t unk12;
	uint32_t unk13;
	uint32_t unk14;
	uint32_t unk15;
	uint32_t unk16;

	if (param_1 < 5) {
		i8 = param_1 * 0x214;
		unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i8));
		if (param_2 == (uint32_t *)0x0) {
			param_2 = (uint32_t *)((uint8_t *)&IFTU_WORD(0x10c) + i8);
		} else {
			p3 = (uint32_t *)((uint8_t *)&IFTU_WORD(0x10c) + i8);
			p5 = param_2;
			do {
				p4 = p5;
				p2 = p3;
				p5 = p4 + 4;
				unk10 = p4[1];
				p3 = p2 + 4;
				unk13 = p4[2];
				unk15 = p4[3];
				*p2 = *p4;
				p2[1] = unk10;
				p2[2] = unk13;
				p2[3] = unk15;
			} while (p5 != param_2 + 0xc);
			unk10 = p4[5];
			unk13 = p4[6];
			*p3 = *p5;
			p2[5] = unk10;
			p2[6] = unk13;
		}
		i6 = 0;
		if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] != 0) {
			i6 = (&IFTU_WORD(0x0))[param_1 * 0x85];
			unk12 = param_2[1];
			unk11 = param_2[6];
			unk9 = param_2[7];
			unk14 = param_2[8];
			unk16 = param_2[9];
			unk7 = param_2[10];
			unk15 = param_2[0xb];
			unk10 = param_2[0xc];
			p3 = (uint32_t *)param_2[0xd];
			unk13 = param_2[0xe];
			*(volatile uint32_t *)(i6 + 0x104) = *param_2;
			*(volatile uint32_t *)(i6 + 0x108) = unk12;
			*(volatile uint32_t *)(i6 + 0x10c) = unk11;
			*(volatile uint32_t *)(i6 + 0x110) = unk9;
			*(volatile uint32_t *)(i6 + 0x114) = unk14;
			*(volatile uint32_t *)(i6 + 0x118) = unk16;
			*(volatile uint32_t *)(i6 + 0x11c) = unk7;
			*(volatile uint32_t *)(i6 + 0x120) = unk15;
			*(volatile uint32_t *)(i6 + 0x124) = unk10;
			*(uint32_t *volatile *)(i6 + 0x128) = p3;
			*(volatile uint32_t *)(i6 + 300) = unk13;
			LowioDmb();
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i8), unk1);
		return 0;
	}
	return 0x803f0700;
}

/* Intermediate context field names remain unknown. */
uint32_t SceIftuForDriver_D64F4C6B(unsigned param_1, uint32_t *param_2)
{
	uint32_t *p1;
	uint32_t unk2;
	uint32_t *p3;
	uint32_t *p4;
	uint32_t *p5;
	int i6;
	uint32_t unk7;
	uint32_t unk8;
	uint32_t unk9;
	int i10;
	uint32_t unk11;
	uint32_t unk12;
	uint32_t unk13;
	uint32_t unk14;
	uint32_t unk15;
	uint32_t unk16;
	uint32_t unk17;
	uint32_t unk18;
	uint32_t unk19;

	if (param_1 < 5) {
		i10 = param_1 * 0x214;
		unk2 = ksceKernelSpinlockLowLockCpuSuspendIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i10));
		if (param_2 == (uint32_t *)0x0) {
			param_2 = (uint32_t *)((uint8_t *)&IFTU_WORD(0x148) + i10);
		} else {
			p1 = (uint32_t *)((uint8_t *)&IFTU_WORD(0x148) + i10);
			p5 = param_2;
			do {
				p4 = p5;
				p3 = p1;
				p5 = p4 + 4;
				unk12 = p4[1];
				unk16 = p4[2];
				unk18 = p4[3];
				*p3 = *p4;
				p3[1] = unk12;
				p3[2] = unk16;
				p3[3] = unk18;
				p1 = p3 + 4;
			} while (p5 != param_2 + 0xc);
			unk12 = p4[5];
			unk16 = p4[6];
			p3[4] = *p5;
			p3[5] = unk12;
			p3[6] = unk16;
		}
		if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] != 0) {
			i6 = (&IFTU_WORD(0x0))[param_1 * 0x85];
			unk9 = param_2[0xd];
			unk7 = param_2[0xe];
			unk11 = param_2[10];
			unk17 = param_2[0xb];
			unk19 = param_2[0xc];
			unk16 = param_2[6];
			unk15 = param_2[7];
			unk12 = param_2[1];
			unk14 = param_2[8];
			unk13 = param_2[9];
			*(volatile uint32_t *)(i6 + 0x130) = *param_2;
			unk8 = param_2[2];
			*(volatile uint32_t *)(i6 + 0x134) = unk12;
			unk12 = param_2[3];
			*(volatile uint32_t *)(i6 + 0x138) = unk16;
			unk16 = param_2[4];
			unk18 = param_2[5];
			*(volatile uint32_t *)(i6 + 0x13c) = unk15;
			*(volatile uint32_t *)(i6 + 0x140) = unk14;
			*(volatile uint32_t *)(i6 + 0x144) = unk13;
			*(volatile uint32_t *)(i6 + 0x148) = unk11;
			*(volatile uint32_t *)(i6 + 0x14c) = unk17;
			*(volatile uint32_t *)(i6 + 0x150) = unk19;
			*(volatile uint32_t *)(i6 + 0x154) = unk9;
			*(volatile uint32_t *)(i6 + 0x158) = unk7;
			*(volatile uint32_t *)(i6 + 0x15c) = unk8;
			*(volatile uint32_t *)(i6 + 0x160) = unk12;
			*(volatile uint32_t *)(i6 + 0x164) = unk16;
			*(volatile uint32_t *)(i6 + 0x168) = unk18;
			LowioDmb();
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i10), unk2);
		unk2 = 0;
	} else {
		unk2 = 0x803f0700;
	}
	return unk2;
}

/* Intermediate context field names remain unknown. */
uint32_t ksceIftuSetInputFrameBuffer(unsigned param_1, uint32_t *param_2, int param_3, int param_4)
{
	uint32_t unk1;
	uint32_t unk2;
	uint32_t unk3;
	int i4;
	int i5;
	uint32_t unk6;
	uint32_t unk7;
	uint32_t unk8;
	int i9;
	uint32_t unk10;
	int i11;
	int i12;
	uint32_t unk13;
	uint32_t *p14;
	uint32_t unk15;
	uint32_t unk16;

	if (4 < param_1) {
		return 0x803f0700;
	}
	i11 = param_1 * 0x214;
	if (param_2 == (uint32_t *)0x0) {
		param_2 = (uint32_t *)((uint8_t *)&IFTU_WORD(0x184) + i11);
		unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i11));
	} else {
		unk3 = *param_2;
		if (unk3 != 0x1000) {
			if (unk3 < 0x1001) {
				if (unk3 != 0x10) {
					if (unk3 < 0x11) {
						if (unk3 != 4) {
							if (unk3 < 5) {
								if (2 < unk3) {
									return 0x803f0703;
								}
							} else if (unk3 != 8) {
								return 0x803f0703;
							}
						}
					} else if (unk3 != 0x40) {
						if (unk3 < 0x41) {
							if (unk3 != 0x20) {
								return 0x803f0703;
							}
						} else if ((unk3 != 0x80) && (unk3 != 0x100)) {
							return 0x803f0703;
						}
					}
				}
			} else if (unk3 != 0x20000) {
				if (unk3 < 0x20001) {
					if (unk3 != 0x4000) {
						if (unk3 < 0x4001) {
							if (unk3 != 0x2000) {
								return 0x803f0703;
							}
						} else if ((unk3 != 0x8000) && (unk3 != 0x10000)) {
							return 0x803f0703;
						}
					}
				} else if (unk3 != 0x80020000) {
					if (unk3 < 0x80020001) {
						if (unk3 != 0x200000) {
							return 0x803f0703;
						}
					} else if ((unk3 != 0x80140000) && (unk3 != 0x80180000)) {
						return 0x803f0703;
					}
				}
			}
		}
		unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i11));
		memcpy((uint8_t *)&IFTU_WORD(0x184) + i11, param_2, 0x54);
	}
	if (-1 < param_3) {
		(&IFTU_WORD(0x1f4))[param_1 * 0x85] = param_3;
	}
	unk3 = 0;
	if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] == 0)
		goto unk5e5a;
	i5 = (&IFTU_WORD(0x0))[param_1 * 0x85];
	if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] == 2) {
		(&IFTU_WORD(0x1e8))[param_1 * 0x85] = 1;
	unk5eb2:
		p14 = (uint32_t *)(&IFTU_WORD(0x8))[param_1 * 0x85];
		unk2 = (uint32_t)(*(volatile int *)(i5 + 4) << 0x1e) >> 0x1f;
		if (*(uint32_t *)((uint8_t *)&IFTU_WORD(0x1f0) + i11) == unk2) {
			unk2 = unk2 ^ 1;
		} else if (*(volatile int *)(i5 + 4) << 0x1f < 0) {
			*p14 = unk2;
			i4 = *(volatile int *)(i5 + 4);
			*(uint32_t *)((uint8_t *)&IFTU_WORD(0x1f0) + i11) = unk2;
			unk2 = (uint32_t)(i4 << 0x1e) >> 0x1f ^ 1;
		}
		i9 = (unk2 + 2) * 0x100;
		i4 = i5 + i9;
		if (*param_2 == 0) {
			unk8 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1d8) + i11);
			unk16 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1dc) + i11);
			*(volatile uint32_t *)(i4 + 0x4c) = 1;
			*(volatile uint32_t *)(i4 + 0x40) = 0x10;
			*(volatile uint32_t *)(i4 + 0x44) = unk8;
			*(volatile uint32_t *)(i4 + 0x48) = unk16;
			*(volatile uint32_t *)(i4 + 0x60) = 0;
			*(volatile uint32_t *)(i4 + 100) = 0;
			*(volatile uint32_t *)(i4 + 0x68) = 0;
			*(volatile uint32_t *)(i4 + 0x6c) = 0;
			*(volatile uint32_t *)(i4 + 0xc0) = 0x10000;
			*(volatile uint32_t *)(i4 + 0xc4) = 0x10000;
		} else {
			unk13 = param_2[1];
			unk10 = param_2[2];
			unk7 = param_2[3];
			unk6 = param_2[4];
			*(volatile uint32_t *)(i4 + 0x40) = *param_2;
			unk3 = param_2[5];
			*(volatile uint32_t *)(i4 + 0x44) = unk13;
			unk13 = param_2[6];
			*(volatile uint32_t *)(i4 + 0x48) = unk10;
			unk10 = param_2[7];
			*(volatile uint32_t *)(i4 + 0x54) = unk7;
			unk7 = param_2[8];
			*(volatile uint32_t *)(i4 + 0x58) = unk6;
			unk6 = param_2[9];
			*(volatile uint32_t *)(i5 + i9) = unk3;
			unk15 = param_2[10];
			unk3 = param_2[0x11];
			*(volatile uint32_t *)(i4 + 4) = unk13;
			unk13 = param_2[0x12];
			*(volatile uint32_t *)(i4 + 8) = unk10;
			unk10 = param_2[0x13];
			*(volatile uint32_t *)(i4 + 0x20) = unk7;
			unk7 = param_2[0x14];
			*(volatile uint32_t *)(i4 + 0x24) = unk6;
			unk6 = param_2[0xb];
			*(volatile uint32_t *)(i4 + 0x28) = unk15;
			unk15 = param_2[0xc];
			*(volatile uint32_t *)(i4 + 0x60) = unk3;
			unk3 = param_2[0xd];
			*(volatile uint32_t *)(i4 + 100) = unk13;
			*(volatile uint32_t *)(i4 + 0x68) = unk10;
			*(volatile uint32_t *)(i4 + 0x6c) = unk7;
			*(volatile uint32_t *)(i4 + 0xc0) = unk6;
			*(volatile uint32_t *)(i4 + 0xc4) = unk15;
			*(volatile uint32_t *)(i4 + 200) = unk3;
			unk3 = param_2[0x10];
			unk6 = param_2[0xf];
			*(volatile uint32_t *)(i4 + 0xcc) = param_2[0xe];
			*(volatile uint32_t *)(i4 + 0xd0) = unk6;
			*(volatile uint32_t *)(i4 + 0xd4) = unk3;
			*(volatile uint32_t *)(i4 + 0x4c) = 0;
		}
		*p14 = unk2;
		unk3 = (&IFTU_WORD(0x1f4))[param_1 * 0x85];
		unk6 = *(volatile uint32_t *)(i5 + 0x180);
		*(uint32_t *)((uint8_t *)&IFTU_WORD(0x1f0) + i11) = unk2;
		if (unk6 != unk3) {
			unk3 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1ec) + i11) | 0x40000000;
			*(uint32_t *)((uint8_t *)&IFTU_WORD(0x1ec) + i11) = unk3;
			*(volatile uint32_t *)(i5 + 0x50) = 3;
		}
	} else {
		if (param_4 != 0)
			goto unk5eb2;
		unk10 = param_2[5];
		unk7 = param_2[6];
		unk13 = param_2[7];
		unk3 = -((*(volatile int *)(i5 + 4) << 0x1e) >> 0x1f);
		unk15 = param_2[8];
		unk6 = param_2[9];
		unk2 = param_2[10];
		i12 = (unk3 + 2) * 0x100;
		i9 = ((unk3 ^ 1) + 2) * 0x100;
		i4 = i5 + i12;
		unk3 = i5 + i9;
		*(volatile uint32_t *)(i5 + i12) = unk10;
		*(volatile uint32_t *)(i4 + 4) = unk7;
		*(volatile uint32_t *)(i4 + 8) = unk13;
		*(volatile uint32_t *)(i4 + 0x20) = unk15;
		*(volatile uint32_t *)(i4 + 0x24) = unk6;
		*(volatile uint32_t *)(i4 + 0x28) = unk2;
		*(volatile uint32_t *)(i5 + i9) = unk10;
		*(volatile uint32_t *)(unk3 + 4) = unk7;
		*(volatile uint32_t *)(unk3 + 8) = unk13;
		*(volatile uint32_t *)(unk3 + 0x20) = unk15;
		*(volatile uint32_t *)(unk3 + 0x24) = unk6;
		*(volatile uint32_t *)(unk3 + 0x28) = unk2;
	}
	LowioDmb();
unk5e5a:
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)((uint8_t *)&IFTU_WORD(0x200) + i11), unk1);
	return 0;
}

/* Intermediate context field names remain unknown. */
uint32_t SceIftuForDriver_357EAE24(unsigned param_1, unsigned param_2)
{
	uint32_t unk1;
	int i2;

	if ((param_1 & 0xfffffffd) != 1) {
		return 0x803f0700;
	}
	if (0x100 < param_2) {
		return 0x803f0701;
	}
	unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(
	    (int *)((uint8_t *)&IFTU_WORD(0x200) + param_1 * 0x214));
	(&IFTU_WORD(0x1f8))[param_1 * 0x85] = param_2;
	if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] != 0) {
		i2 = (&IFTU_WORD(0x0))[param_1 * 0x85];
		if (param_2 == 0x100) {
			*(volatile uint32_t *)(i2 + 0x8c) = 0;
			*(volatile uint32_t *)(i2 + 0xa0) = 1;
		} else {
			*(volatile uint32_t *)(i2 + 0x8c) = param_2;
			*(volatile uint32_t *)(i2 + 0xa0) = 0;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(
	    (int *)((uint8_t *)&IFTU_WORD(0x200) + param_1 * 0x214), unk1);
	return 0;
}

/* Intermediate context field names remain unknown. */
uint32_t ksceIftuSetMergeSetting(unsigned param_1, int param_2)
{
	uint32_t unk1;
	uint32_t unk2;
	int i3;

	if ((param_1 & 0xfffffffd) != 1) {
		return 0x803f0700;
	}
	unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(
	    (int *)((uint8_t *)&IFTU_WORD(0x200) + param_1 * 0x214));
	(&IFTU_WORD(0x1fc))[param_1 * 0x85] = param_2;
	if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] != 0) {
		i3 = (&IFTU_WORD(0x4))[param_1 * 0x85];
		if (param_2 == 0x80) {
			*(volatile uint32_t *)(i3 + 0x20) = 0;
			unk2 = *(volatile uint32_t *)(i3 + 4) & 0xfffffffe;
		} else {
			*(volatile int *)(i3 + 0x20) = param_2;
			unk2 = *(volatile uint32_t *)(i3 + 4) | 1;
		}
		*(volatile uint32_t *)(i3 + 4) = unk2;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(
	    (int *)((uint8_t *)&IFTU_WORD(0x200) + param_1 * 0x214), unk1);
	return 0;
}

/* Intermediate context field names remain unknown. */
uint32_t ksceIftuEnable(unsigned param_1)
{
	uint32_t unk1;
	uint32_t unk2;
	uint32_t unk3;
	int i4;
	volatile uint32_t *p5;
	uint32_t unk6;
	int i7;
	volatile uint32_t *p8;
	uint32_t unk9;
	int i10;
	uint32_t unk11;
	uint32_t unk12;
	uint32_t unk13;
	uint32_t unk14;
	uint32_t unk15;
	uint32_t unk16;
	uint32_t unk17;
	bool flag18;

	if (param_1 < 4) {
		i10 = param_1 * 0x214;
		unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i10));
		if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] == 0) {
			i7 = (&IFTU_WORD(0x0))[param_1 * 0x85];
			unk2 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x12c) + i10);
			unk3 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x130) + i10);
			unk14 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x134) + i10);
			unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x10c) + i10);
			unk15 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x110) + i10);
			unk16 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x124) + i10);
			unk9 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x128) + i10);
			unk12 = (&IFTU_WORD(0x1f4))[param_1 * 0x85];
			*(volatile uint32_t *)(i7 + 0x58) = 0x108;
			unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x138) + i10);
			*(volatile uint32_t *)(i7 + 0x180) = unk12;
			unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x13c) + i10);
			*(volatile uint32_t *)(i7 + 0x104) = unk11;
			unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x140) + i10);
			*(volatile uint32_t *)(i7 + 0x108) = unk15;
			unk15 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x144) + i10);
			*(volatile uint32_t *)(i7 + 0x10c) = unk16;
			unk16 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x148) + i10);
			*(volatile uint32_t *)(i7 + 0x110) = unk9;
			unk9 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x14c) + i10);
			*(volatile uint32_t *)(i7 + 0x114) = unk2;
			unk2 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x160) + i10);
			*(volatile uint32_t *)(i7 + 0x118) = unk3;
			unk3 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x164) + i10);
			*(volatile uint32_t *)(i7 + 0x11c) = unk14;
			*(volatile uint32_t *)(i7 + 0x120) = unk13;
			*(volatile uint32_t *)(i7 + 0x124) = unk12;
			*(volatile uint32_t *)(i7 + 0x128) = unk11;
			*(volatile uint32_t *)(i7 + 300) = unk15;
			*(volatile uint32_t *)(i7 + 0x130) = unk16;
			*(volatile uint32_t *)(i7 + 0x134) = unk9;
			*(volatile uint32_t *)(i7 + 0x138) = unk2;
			*(volatile uint32_t *)(i7 + 0x13c) = unk3;
			unk2 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x16c) + i10);
			unk3 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x170) + i10);
			unk14 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x174) + i10);
			unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x178) + i10);
			unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x17c) + i10);
			unk15 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x150) + i10);
			unk16 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x154) + i10);
			unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x180) + i10);
			*(volatile uint32_t *)(i7 + 0x140) = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x168) + i10);
			unk9 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x158) + i10);
			*(volatile uint32_t *)(i7 + 0x144) = unk2;
			unk2 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x15c) + i10);
			*(volatile uint32_t *)(i7 + 0x148) = unk3;
			i4 = (&IFTU_WORD(0x1f8))[param_1 * 0x85];
			*(volatile uint32_t *)(i7 + 0x14c) = unk14;
			*(volatile uint32_t *)(i7 + 0x150) = unk13;
			flag18 = i4 == 0x100;
			*(volatile uint32_t *)(i7 + 0x154) = unk12;
			if (flag18) {
				i4 = 1;
			}
			*(volatile uint32_t *)(i7 + 0x158) = unk11;
			*(volatile uint32_t *)(i7 + 0x15c) = unk15;
			*(volatile uint32_t *)(i7 + 0x160) = unk16;
			*(volatile uint32_t *)(i7 + 0x164) = unk9;
			*(volatile uint32_t *)(i7 + 0x168) = unk2;
			*(volatile uint32_t *)(i7 + 0x80) = 0;
			*(volatile uint32_t *)(i7 + 0x84) = 0;
			*(volatile uint32_t *)(i7 + 0x88) = 0;
			if (flag18) {
				*(volatile uint32_t *)(i7 + 0x8c) = 0;
				*(volatile int *)(i7 + 0xa0) = i4;
			} else {
				*(volatile int *)(i7 + 0x8c) = i4;
			}
			unk2 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1e0) + i10);
			if (!flag18) {
				*(volatile uint32_t *)(i7 + 0xa0) = 0;
			}
			i4 = *(int *)((uint8_t *)&IFTU_WORD(0x184) + i10);
			unk3 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1d8) + i10);
			unk9 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1dc) + i10);
			*(volatile uint32_t *)(i7 + 0x2a0) = unk2;
			*(volatile uint32_t *)(i7 + 0x2a4) = unk3;
			*(volatile uint32_t *)(i7 + 0x2a8) = unk9;
			if (i4 == 0) {
				*(volatile uint32_t *)(i7 + 0x24c) = 1;
				*(volatile uint32_t *)(i7 + 0x240) = 0x10;
				*(volatile uint32_t *)(i7 + 0x244) = unk3;
				*(volatile uint32_t *)(i7 + 0x248) = unk9;
				*(volatile uint32_t *)(i7 + 0x260) = 0;
				*(volatile uint32_t *)(i7 + 0x264) = 0;
				*(volatile uint32_t *)(i7 + 0x268) = 0;
				*(volatile uint32_t *)(i7 + 0x26c) = 0;
				*(volatile uint32_t *)(i7 + 0x2c0) = 0x10000;
				*(volatile uint32_t *)(i7 + 0x2c4) = 0x10000;
				*(volatile uint32_t *)(i7 + 0x3a0) = unk2;
				*(volatile uint32_t *)(i7 + 0x3a4) = unk3;
				*(volatile uint32_t *)(i7 + 0x3a8) = unk9;
				*(volatile uint32_t *)(i7 + 0x34c) = 1;
				*(volatile uint32_t *)(i7 + 0x340) = 0x10;
				*(volatile uint32_t *)(i7 + 0x344) = unk3;
				*(volatile uint32_t *)(i7 + 0x348) = unk9;
				*(volatile uint32_t *)(i7 + 0x360) = 0;
				*(volatile uint32_t *)(i7 + 0x364) = 0;
				*(volatile uint32_t *)(i7 + 0x368) = 0;
				*(volatile uint32_t *)(i7 + 0x36c) = 0;
				*(volatile uint32_t *)(i7 + 0x3c0) = 0x10000;
				*(volatile uint32_t *)(i7 + 0x3c4) = 0x10000;
			} else {
				unk14 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x188) + i10);
				unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x18c) + i10);
				unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x190) + i10);
				unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x194) + i10);
				unk15 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x198) + i10);
				unk16 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x19c) + i10);
				*(volatile int *)(i7 + 0x240) = i4;
				*(volatile uint32_t *)(i7 + 0x244) = unk14;
				*(volatile uint32_t *)(i7 + 0x248) = unk13;
				unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1a0) + i10);
				*(volatile uint32_t *)(i7 + 0x254) = unk12;
				unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1a4) + i10);
				*(volatile uint32_t *)(i7 + 600) = unk11;
				unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1a8) + i10);
				*(volatile uint32_t *)(i7 + 0x200) = unk15;
				unk14 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1ac) + i10);
				*(volatile uint32_t *)(i7 + 0x204) = unk16;
				unk15 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1c8) + i10);
				*(volatile uint32_t *)(i7 + 0x208) = unk13;
				unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1cc) + i10);
				*(volatile uint32_t *)(i7 + 0x220) = unk12;
				unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1d0) + i10);
				*(volatile uint32_t *)(i7 + 0x224) = unk11;
				unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1d4) + i10);
				*(volatile uint32_t *)(i7 + 0x228) = unk14;
				unk14 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1b0) + i10);
				*(volatile uint32_t *)(i7 + 0x260) = unk15;
				unk15 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1b4) + i10);
				*(volatile uint32_t *)(i7 + 0x264) = unk13;
				unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1b8) + i10);
				*(volatile uint32_t *)(i7 + 0x268) = unk12;
				*(volatile uint32_t *)(i7 + 0x26c) = unk11;
				*(volatile uint32_t *)(i7 + 0x2c0) = unk14;
				*(volatile uint32_t *)(i7 + 0x2c4) = unk15;
				unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1bc) + i10);
				*(volatile uint32_t *)(i7 + 0x2c8) = unk13;
				unk16 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1c0) + i10);
				unk17 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1c4) + i10);
				unk14 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x188) + i10);
				unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x18c) + i10);
				*(volatile uint32_t *)(i7 + 0x2cc) = unk11;
				unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x190) + i10);
				unk15 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x194) + i10);
				*(volatile uint32_t *)(i7 + 0x2d0) = unk16;
				*(volatile uint32_t *)(i7 + 0x2d4) = unk17;
				unk16 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x198) + i10);
				unk17 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x19c) + i10);
				*(volatile uint32_t *)(i7 + 0x24c) = 0;
				*(volatile uint32_t *)(i7 + 0x3a0) = unk2;
				*(volatile uint32_t *)(i7 + 0x3a4) = unk3;
				unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1ac) + i10);
				unk2 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1a0) + i10);
				*(volatile uint32_t *)(i7 + 0x3a8) = unk9;
				unk3 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1a4) + i10);
				*(volatile int *)(i7 + 0x340) = i4;
				unk9 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1a8) + i10);
				*(volatile uint32_t *)(i7 + 0x344) = unk14;
				*(volatile uint32_t *)(i7 + 0x348) = unk13;
				unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1c8) + i10);
				*(volatile uint32_t *)(i7 + 0x354) = unk12;
				unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1cc) + i10);
				*(volatile uint32_t *)(i7 + 0x358) = unk15;
				*(volatile uint32_t *)(i7 + 0x300) = unk16;
				*(volatile uint32_t *)(i7 + 0x304) = unk17;
				*(volatile uint32_t *)(i7 + 0x308) = unk2;
				*(volatile uint32_t *)(i7 + 800) = unk3;
				*(volatile uint32_t *)(i7 + 0x324) = unk9;
				*(volatile uint32_t *)(i7 + 0x328) = unk11;
				*(volatile uint32_t *)(i7 + 0x360) = unk13;
				*(volatile uint32_t *)(i7 + 0x364) = unk12;
				unk11 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1b0) + i10);
				unk13 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1b4) + i10);
				unk14 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1b8) + i10);
				unk9 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1bc) + i10);
				unk2 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1c0) + i10);
				unk12 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1d4) + i10);
				unk3 = *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1c4) + i10);
				*(volatile uint32_t *)(i7 + 0x368) =
				    *(uint32_t *)((uint8_t *)&IFTU_WORD(0x1d0) + i10);
				*(volatile uint32_t *)(i7 + 0x36c) = unk12;
				*(volatile uint32_t *)(i7 + 0x3c0) = unk11;
				*(volatile uint32_t *)(i7 + 0x3c4) = unk13;
				*(volatile uint32_t *)(i7 + 0x3c8) = unk14;
				*(volatile uint32_t *)(i7 + 0x3cc) = unk9;
				*(volatile uint32_t *)(i7 + 0x3d0) = unk2;
				*(volatile uint32_t *)(i7 + 0x3d4) = unk3;
				*(volatile uint32_t *)(i7 + 0x34c) = 0;
			}
			p8 = (uint32_t *)(&IFTU_WORD(0x4))[param_1 * 0x85];
			unk6 = p8[1];
			if ((param_1 & 0xfffffffd) == 1) {
				i4 = (&IFTU_WORD(0x1fc))[param_1 * 0x85];
				if (i4 == 0x80) {
					unk6 = unk6 & 0xfffffffe;
					i4 = 0;
				} else {
					unk6 = unk6 | 1;
				}
				p8[8] = i4;
			}
			if ((param_1 & 1) == 0) {
				if (*(int *)((uint8_t *)&IFTU_WORD(0x1e4) + i10) == 1) {
					unk6 = unk6 | 4;
					p8[5] = 0;
				} else {
					unk6 = unk6 & 0xfffffffb;
				}
			} else if (*(int *)((uint8_t *)&IFTU_WORD(0x1e4) + i10) == 1) {
				unk6 = unk6 | 0x10;
				p8[7] = 0;
			} else {
				unk6 = unk6 & 0xffffffef;
			}
			p5 = (uint32_t *)(&IFTU_WORD(0x8))[param_1 * 0x85];
			*(uint32_t *)((uint8_t *)&IFTU_WORD(0x1f0) + i10) = 0;
			*p5 = 0;
			p8[1] = unk6;
			*p8 = 1;
			LowioDmb();
			(&IFTU_WORD(0x1e8))[param_1 * 0x85] = 2;
			ksceKernelEnableIntr((&IFTU_WORD(0x204))[param_1 * 0x85]);
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(
		    (int *)((uint8_t *)&IFTU_WORD(0x200) + i10), unk1);
		unk1 = 0;
	} else {
		unk1 = 0x803f0700;
	}
	return unk1;
}

/* Intermediate context field names remain unknown. */
uint32_t ksceIftuDisable(unsigned param_1)
{
	uint32_t unk1;
	int i2;
	int *p3;

	if (3 < param_1) {
		return 0x803f0700;
	}
	p3 = (int *)((uint8_t *)&IFTU_WORD(0x200) + param_1 * 0x214);
	unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(p3);
	if ((&IFTU_WORD(0x1e8))[param_1 * 0x85] == 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(p3, unk1);
		return 0;
	}
	i2 = (&IFTU_WORD(0x0))[param_1 * 0x85];
	(&IFTU_WORD(0x1e8))[param_1 * 0x85] = 0;
	*(volatile uint32_t *)(i2 + 0x24c) = 1;
	*(volatile uint32_t *)(i2 + 0x34c) = 1;
	*(volatile uint32_t *)(i2 + 0x50) = 0;
	*(volatile uint32_t *)(i2 + 0x40) = 0;
	(void)*(volatile uint32_t *)(i2 + 0x40);
	LowioDsb();
	ksceKernelDisableIntr((&IFTU_WORD(0x204))[param_1 * 0x85]);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(p3, unk1);
	return 0;
}

/* Intermediate context field names remain unknown. */
uint32_t ksceIftuCsc(uint32_t *param_1, uint32_t *param_2, int param_3)
{
	volatile uint32_t *p1;
	volatile uint32_t *p2;
	uint32_t unk3;
	uint32_t unk4;
	uint32_t unk5;
	uint32_t unk6;
	uint32_t unk7;
	uint32_t unk8;
	uint32_t unk9;
	uint32_t unk10;
	uint32_t unk11;
	volatile uint32_t *p12;
	uint32_t unk13;
	uint32_t unk14;
	uint32_t unk15;
	uint32_t unk16;
	uint32_t *p17;
	uint32_t unk18;
	uint32_t *p19;
	uint32_t unk20;
	uint32_t *p21;
	uint32_t unk22;
	uint32_t unk23;

	unk9 = *param_1 & 0x7fffffff;
	if (unk9 != 0x80) {
		if (unk9 < 0x81) {
			if (unk9 != 8) {
				if (unk9 < 9) {
					if (unk9 == 0) {
						return 0x803f0703;
					}
					if ((2 < unk9) && (unk9 != 4)) {
						return 0x803f0703;
					}
				} else if (((unk9 != 0x20) && (unk9 != 0x40)) && (unk9 != 0x10)) {
					return 0x803f0703;
				}
			}
		} else if (unk9 != 0x4000) {
			if (unk9 < 0x4001) {
				if (((unk9 != 0x1000) && (unk9 != 0x2000)) && (unk9 != 0x100)) {
					return 0x803f0703;
				}
			} else if (unk9 != 0x10000) {
				if (unk9 < 0x10001) {
					if (unk9 != 0x8000) {
						return 0x803f0703;
					}
				} else if ((unk9 != 0x20000) && (unk9 != 0x200000)) {
					return 0x803f0703;
				}
			}
		}
	}
	unk9 = *param_2;
	if (unk9 != 0x1000) {
		if (unk9 < 0x1001) {
			if (unk9 != 0x10) {
				if (unk9 < 0x11) {
					if (unk9 != 4) {
						if (unk9 < 5) {
							if (2 < unk9) {
								return 0x803f0703;
							}
						} else if (unk9 != 8) {
							return 0x803f0703;
						}
					}
				} else if (unk9 != 0x40) {
					if (unk9 < 0x41) {
						if (unk9 != 0x20) {
							return 0x803f0703;
						}
					} else if ((unk9 != 0x80) && (unk9 != 0x100)) {
						return 0x803f0703;
					}
				}
			}
		} else if (unk9 != 0x20000) {
			if (unk9 < 0x20001) {
				if (unk9 != 0x4000) {
					if (unk9 < 0x4001) {
						if (unk9 != 0x2000) {
							return 0x803f0703;
						}
					} else if ((unk9 != 0x8000) && (unk9 != 0x10000)) {
						return 0x803f0703;
					}
				}
			} else if (unk9 != 0x80020000) {
				if (unk9 < 0x80020001) {
					if (unk9 != 0x200000) {
						return 0x803f0703;
					}
				} else if ((unk9 != 0x80140000) && (unk9 != 0x80180000)) {
					return 0x803f0703;
				}
			}
		}
	}
	unk9 = ksceKernelLockFastMutex(&s_iftuFastMutex);
	if (-1 < (int)unk9) {
		unk3 = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&IFTU_WORD(0xa50));
		ScePervasiveForDriver_07F2A738();
		p1 = s_iftuBase2;
		(void)p1[0x16];
		LowioDsb();
		if (param_3 == 0) {
			s_iftuBase2[0x60] = 1;
			p1[0x40] = 0;
			p1[0x20] = 0;
			p1[0x21] = 0;
			p1[0x22] = 0;
			p1[0x23] = 0xff;
			unk9 = 0;
		} else {
			p21 = *(uint32_t **)(param_3 + 8);
			s_iftuBase2[0x60] = *(uint32_t *)(param_3 + 4);
			if (p21 != (uint32_t *)0x0) {
				p2 = &IFTU_WORD(0x998);
				p17 = p21;
				do {
					p19 = p17;
					p12 = p2;
					unk6 = p19[2];
					p17 = p19 + 4;
					unk4 = p19[1];
					unk10 = p19[3];
					*p12 = *p19;
					p12[1] = unk4;
					p12[2] = unk6;
					p12[3] = unk10;
					p2 = p12 + 4;
				} while (p17 != p21 + 0xc);
				unk6 = p19[6];
				unk4 = p19[5];
				p12[4] = *p17;
				p12[5] = unk4;
				p12[6] = unk6;
				p2 = s_iftuBase2;
				if (IFTU_WORD(0xa38) != 0) {
					unk6 = p21[8];
					unk18 = p21[6];
					unk4 = p21[7];
					unk10 = p21[1];
					unk16 = p21[9];
					s_iftuBase2[0x4c] = *p21;
					unk7 = p21[10];
					p2[0x4d] = unk10;
					unk13 = p21[0xb];
					unk8 = p21[0xc];
					p2[0x4e] = unk18;
					p2[0x4f] = unk4;
					unk4 = p21[0xd];
					unk18 = p21[3];
					p2[0x50] = unk6;
					unk6 = p21[0xe];
					p2[0x51] = unk16;
					unk10 = p21[2];
					p2[0x52] = unk7;
					p2[0x53] = unk13;
					unk7 = p21[4];
					unk13 = p21[5];
					p2[0x54] = unk8;
					p2[0x55] = unk4;
					p2[0x56] = unk6;
					p2[0x57] = unk10;
					p2[0x58] = unk18;
					p2[0x59] = unk7;
					p2[0x5a] = unk13;
					LowioDmb();
				}
			}
			p21 = *(uint32_t **)(param_3 + 0xc);
			if (p21 != (uint32_t *)0x0) {
				p2 = &IFTU_WORD(0x95c);
				p17 = p21;
				do {
					p19 = p17;
					p12 = p2;
					p17 = p19 + 4;
					unk4 = p19[1];
					unk6 = p19[2];
					unk10 = p19[3];
					*p12 = *p19;
					p12[1] = unk4;
					p12[2] = unk6;
					p12[3] = unk10;
					p2 = p12 + 4;
				} while (p17 != p21 + 0xc);
				unk6 = p19[6];
				unk4 = p19[5];
				p12[4] = *p17;
				p12[5] = unk4;
				p12[6] = unk6;
				p2 = s_iftuBase2;
				if (IFTU_WORD(0xa38) != 0) {
					unk4 = p21[6];
					unk6 = p21[7];
					unk13 = p21[1];
					unk8 = p21[8];
					unk7 = p21[9];
					unk10 = p21[10];
					s_iftuBase2[0x41] = *p21;
					p2[0x42] = unk13;
					unk13 = p21[0xb];
					p2[0x43] = unk4;
					unk4 = p21[0xc];
					p2[0x44] = unk6;
					unk6 = p21[0xd];
					unk16 = p21[0xe];
					p2[0x45] = unk8;
					p2[0x46] = unk7;
					p2[0x47] = unk10;
					p2[0x48] = unk13;
					p2[0x49] = unk4;
					p2[0x4a] = unk6;
					p2[0x4b] = unk16;
					LowioDmb();
				}
			}
			unk4 = *(uint32_t *)(param_3 + 0x1c);
			unk6 = *(uint32_t *)(param_3 + 0x20);
			unk10 = *(uint32_t *)(param_3 + 0x14);
			unk7 = *(uint32_t *)(param_3 + 0x18);
			unk9 = *(uint32_t *)(param_3 + 0x24) & 3;
			p1[0x40] = *(uint32_t *)(param_3 + 0x10);
			p1[0x20] = unk10;
			p1[0x21] = unk7;
			p1[0x22] = unk4;
			p1[0x23] = unk6;
		}
		unk15 = param_2[1];
		unk14 = param_2[2];
		unk5 = *param_2;
		unk20 = param_2[3];
		unk22 = param_2[4];
		unk11 = param_2[5];
		p1[0x93] = 0;
		p1[0x90] = unk5;
		p1[0x91] = unk15;
		unk15 = param_2[6];
		p1[0x92] = unk14;
		unk14 = param_2[7];
		p1[0x95] = unk20;
		unk20 = param_2[8];
		p1[0x96] = unk22;
		unk22 = param_2[9];
		p1[0x80] = unk11;
		unk5 = param_2[10];
		p1[0x81] = unk15;
		unk15 = param_2[0x11];
		p1[0x82] = unk14;
		unk11 = param_2[0x12];
		p1[0x88] = unk20;
		unk20 = param_2[0x13];
		p1[0x89] = unk22;
		unk22 = param_2[0x14];
		p1[0x8a] = unk5;
		unk5 = *param_1;
		p1[0x98] = unk15;
		unk14 = param_1[1];
		p1[0x99] = unk11;
		p1[0x9a] = unk20;
		p1[0x9b] = unk22;
		p1[0xa8] = unk5;
		p1[0xa9] = unk14;
		unk5 = param_1[6];
		unk14 = param_1[3];
		unk22 = param_1[4];
		unk23 = param_1[5];
		unk11 = param_1[7];
		p1[0xaa] = param_1[2];
		unk20 = param_2[0xb];
		p1[0xad] = unk14;
		unk15 = param_2[0xc];
		p1[0xae] = unk22;
		unk22 = param_2[0xd];
		p1[0xa0] = unk23;
		unk23 = param_2[0xe];
		p1[0xa1] = unk5;
		unk14 = param_2[0xf];
		unk5 = param_2[0x10];
		p1[0xa2] = unk11;
		p1[0xb0] = unk20;
		p1[0xb1] = unk15;
		p1[0xb2] = unk22;
		p1[0xb3] = unk23;
		p1[0xb4] = unk14;
		p1[0xb5] = unk5;
		p1[0x14] = 3;
		p1[0x16] = 0x108;
		if (unk9 == 2) {
			p1[0x24] = 0;
			*p1 = 5;
		} else if (unk9 == 3) {
			p1[0x24] = 0;
			*p1 = 0xd;
		} else {
			if (unk9 == 1) {
				p1[0x24] = 1;
			} else {
				p1[0x24] = 0;
			}
			*p1 = 1;
		}
		LowioDmb();
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&IFTU_WORD(0xa50), unk3);
		ksceKernelWaitEventFlag(IFTU_WORD(0xa60), 1, 5, 0, 0);
		ScePervasiveForDriver_5AFE0AF0();
		unk9 = ksceKernelUnlockFastMutex(&s_iftuFastMutex);
		unk9 = unk9 & (int)unk9 >> 0x1f;
	}
	return unk9;
}

uint32_t ksceIftuSetOutputFormat(
    unsigned index, unsigned width, unsigned height, unsigned format, unsigned a4)
{
	uint32_t *ctx;
	uint32_t state;

	if (index > 3)
		return 0x803f0700;
	if ((width & 0xf) || (height & 7))
		return 0x803f0701;
	if (format != 0x10 && format != 0x20 && format != 0x40 && format != 0x80 &&
	    format != 0x1000 && format != 0x2000 && format != 0x4000 && format != 0x8000)
		return 0x803f0703;
	ctx = s_iftuState[index];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[0x80]);
	if (ctx[0x7a]) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[0x80], state);
		return 0x803f0704;
	}
	/* Ghidra drops three SIMD lanes here; the original stores four words. */
	ctx[0x76] = width;
	ctx[0x77] = height;
	ctx[0x78] = format;
	ctx[0x79] = a4;
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[0x80], state);
	return 0;
}

/* name unknown: hardware descriptors at original offset 0x9060. */
static const struct {
	int intrCode;
	const char *intrName;
	const char *name;
	uint32_t paddr;
	const char *secondaryName;
	uint32_t secondaryPaddr;
} s_iftuDevices[5] = {
    {0xcc, "SceIftu0a", "SceIftu0RegA", 0xe5020000, "SceIftuc0Reg", 0xe5022000},
    {0xcd, "SceIftu0b", "SceIftu0RegB", 0xe5021000, NULL, 0xe5022000},
    {0xce, "SceIftu1a", "SceIftu1RegA", 0xe5030000, "SceIftuc1Reg", 0xe5032000},
    {0xcf, "SceIftu1b", "SceIftu1RegB", 0xe5031000, NULL, 0xe5032000},
    {0xff, "SceIftu2", "SceIftu2Reg", 0xe5040000, NULL, 0},
};

/* name unknown: initializer at original offset 0x5888. */
int LowioIftuInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	uint32_t *ctx;
	void *base;
	unsigned i;
	int ret;

	memset(s_iftuState, 0, sizeof(s_iftuState));
	for (i = 0; i < 5; i++) {
		ctx = s_iftuState[i];
		ctx[0x7e] = 0xff;
		ctx[0x7f] = 0x80;
		ctx[0x7d] = 1;
		ctx[0x84] = i == 4 ? ksceKernelCreateEventFlag("SceIftu2", 0x1000, 0, NULL) : -1;
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_iftuDevices[i].paddr;
		ret = ksceKernelAllocMemBlock(s_iftuDevices[i].name, 0x20100806, 0x1000, &opt);
		if (ret < 0)
			return ret;
		ctx[0x82] = ret;
		ksceKernelGetMemBlockBase(ret, &base);
		ctx[0] = (uint32_t)base;
		if (!s_iftuDevices[i].secondaryPaddr) {
			ctx[1] = 0;
			ctx[2] = 0;
			ctx[0x82] = -1;
		} else if (i & 1) {
			ctx[0x82] = -1;
			ctx[1] = s_iftuState[i - 1][1];
			ctx[2] = ctx[1] + 0x18;
		} else {
			opt.size = 0x58;
			opt.attr = 2;
			opt.paddr = s_iftuDevices[i].secondaryPaddr;
			ret = ksceKernelAllocMemBlock(
			    s_iftuDevices[i].secondaryName, 0x20100806, 0x1000, &opt);
			/* Sony does not check this secondary allocation or mapping. */
			ksceKernelGetMemBlockBase(ret, &base);
			ctx[0x82] = ret;
			ctx[1] = (uint32_t)base;
			ctx[2] = (uint32_t)base + 0x10;
		}
		ctx[0x81] = s_iftuDevices[i].intrCode;
		if ((int)ctx[0x81] >= 0) {
			ksceKernelRegisterIntrHandler(ctx[0x81],
			    i == 4 ? "SceIftu2" : s_iftuDevices[i].intrName, 0,
			    i == 4 ? Iftu2IntrHandler : IftuIntrHandler, ctx, 0x50, 0xf, NULL);
			if (i == 4) {
				ksceKernelEnableIntr(s_iftuState[4][0x81]);
				break;
			}
		}
	}
	ScePervasiveForDriver_07F2A738();
	ScePervasiveForDriver_E92E28FF();
	(void)IFTU_REG((uint8_t *)s_iftuState[4][0], 0x58);
	LowioDsb();
	IFTU_REG((uint8_t *)s_iftuState[4][0], 0x58) = 0x108;
	s_iftuState[4][0x7a] = 1;
	(void)IFTU_REG((uint8_t *)s_iftuState[4][0], 0x58);
	LowioDsb();
	ScePervasiveForDriver_5AFE0AF0();
	ksceKernelInitializeFastMutex(&s_iftuFastMutex, "SceIftu2", 2, NULL);
	ksceKernelRegisterSysEventHandler("SceIftu", IftuSysEvent, NULL);
	return 0;
}
