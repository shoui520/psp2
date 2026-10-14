/* SceDsi display interface. Private table and context field names are unknown. */
#include "dsi_data.h"
#include "lowio_internal.h"
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/sysevent.h>
#include <stdbool.h>

#define DSI_ERROR_HEAD ((uint32_t)0x803f0600)
#define DSI_ERROR_VIC ((uint32_t)0x803f0601)
#define DSI_ERROR_PARAM ((uint32_t)0x803f0602)
#define DSI_ERROR_DISABLED ((uint32_t)0x803f0603)
#define DSI_REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))

/* Raw context words preserve unrecovered fields and their original offsets. */
static uint32_t s_dsiState[2][0x12];
#define DSI_WORD(offset) (*(uint32_t *)((uint8_t *)s_dsiState + (offset)))
#define DSI_BYTE(offset) (*((uint8_t *)s_dsiState + (offset)))

static const uint32_t s_dsiDisplayModes[4] = {2, 2, 3, 4};
uint32_t kscePervasiveGetSoCRevision(void);
uint32_t ScePervasiveForDriver_350BF570(unsigned head);
uint32_t ScePervasiveForDriver_F1908D3C(unsigned head);
uint32_t __aeabi_uidiv(uint32_t numerator, uint32_t denominator);

/* name unknown */
static const uint32_t *DsiFindVic(unsigned vic)
{
	unsigned i;

	for (i = 0; i < 15; i++) {
		if (s_dsiVicMap[i].vic == vic)
			return s_dsiVicMap[i].mode;
	}
	return NULL;
}

uint32_t SceDsiForDriver_F2921E29(unsigned head, unsigned mode)
{
	if (head > 1)
		return DSI_ERROR_HEAD;
	DSI_BYTE(head * 0x48 + 7) = mode;
	if (!head && !mode) {
		DSI_BYTE(4) = 0;
		s_dsiVicMap[0].mode = s_dsiMode8d00;
		s_dsiVicMap[1].mode = s_dsiMode8bb4;
		s_dsiVicMap[2].mode = s_dsiMode8ed0;
	} else if (!head && mode == 1) {
		DSI_BYTE(4) = 0;
		s_dsiVicMap[0].mode = s_dsiMode8c10;
		s_dsiVicMap[1].mode = NULL;
		s_dsiVicMap[2].mode = NULL;
	} else if (head == 1 && mode == 1) {
		DSI_BYTE(0x4c) = 0;
		s_dsiVicMap[0].mode = NULL;
		s_dsiVicMap[1].mode = NULL;
		s_dsiVicMap[2].mode = NULL;
	}
	return 0;
}

uint32_t ksceDsiSetLanesAndPixelSize(unsigned head, unsigned lanes, unsigned pixelSize)
{
	if (head > 1)
		return DSI_ERROR_HEAD;
	if ((!head && (lanes != 2 || pixelSize != 24)) ||
	    (head && (lanes < 2 || lanes > 3 || (pixelSize != 24 && pixelSize != 30))))
		return DSI_ERROR_PARAM;
	DSI_BYTE(head * 0x48 + 5) = pixelSize;
	DSI_BYTE(head * 0x48 + 6) = lanes;
	return 0;
}

uint32_t ksceDsiGetVicResolution(unsigned vic, int *width, int *height)
{
	const uint32_t *mode = DsiFindVic(vic);
	uint32_t w, h;

	if (!mode)
		return DSI_ERROR_VIC;
	w = mode[5] - mode[10] - mode[8] - mode[9];
	if (mode[0] & 8)
		w -= 2;
	if (mode[7] == 1)
		h = mode[6] + 1 - 2 * (mode[13] + mode[12] + mode[11]);
	else
		h = mode[6] - mode[13] - mode[12] - mode[11];
	if (width)
		*width = w;
	if (height)
		*height = h;
	return 0;
}

uint32_t ksceDsiSetVic(unsigned head, unsigned vic)
{
	const uint32_t *mode;

	if (head > 1)
		return DSI_ERROR_HEAD;
	mode = DsiFindVic(vic);
	if (!mode)
		return DSI_ERROR_VIC;
	s_dsiState[head][3] = vic;
	s_dsiState[head][4] = (uint32_t)mode;
	return 0;
}

uint32_t ksceDsiGetPixelClock(unsigned head)
{
	const uint32_t *mode;

	if (head > 1)
		return DSI_ERROR_HEAD;
	mode = (const uint32_t *)s_dsiState[head][4];
	if (!mode)
		return DSI_ERROR_VIC;
	return mode[DSI_BYTE(head * 0x48 + 5) == 24 ? 1 : 3];
}

uint32_t SceDsiForDriver_8610B795(unsigned head, uint32_t numerator, uint32_t denominator)
{
	if (head > 1)
		return DSI_ERROR_HEAD;
	s_dsiState[head][7] = __aeabi_uidiv(numerator * s_dsiState[head][6], denominator);
	return 0;
}
/* Command program construction; intermediate field names are unknown. */
uint32_t ksceDsiEnableHead(unsigned param_1)
{
	uint32_t unk1;
	uint32_t unk2;
	uint32_t unk3;
	uint32_t unk4;
	uint32_t unk5;
	uint32_t unk6;
	uint32_t unk7;
	uint32_t unk8;
	int *p9;
	uint32_t *p10;
	uint32_t unk11;
	int i12;
	int i13;
	int i14;
	uint32_t *p15;
	uint32_t unk16;
	SceKernelSpinlock *p17;
	uint32_t unk18;
	uint32_t unk19;
	uint32_t unk20;
	uint32_t unk21;
	uint32_t commands[59];
	uint32_t local_134;
	uint32_t local_130;
	int local_12c;
	bool flag22;

	if (1 < param_1) {
		unk1 = 0x803f0600;
		goto unk42d0;
	}
	i13 = param_1 * 0x48;
	p17 = (SceKernelSpinlock *)((uint8_t *)&DSI_WORD(0x38) + i13);
	ScePervasiveForDriver_350BF570(param_1);
	unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(p17);
	if ((&DSI_WORD(0x14))[param_1 * 0x12] != 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(p17, unk1);
		unk1 = 0;
		goto unk42d0;
	}
	i14 = (&DSI_WORD(0x0))[param_1 * 0x12];
	p15 = (uint32_t *)(&DSI_WORD(0x10))[param_1 * 0x12];
	*(volatile uint32_t *)(i14 + 0x54) = 0;
	*(volatile uint32_t *)(i14 + 0x518) = 1;
	unk2 = kscePervasiveGetSoCRevision();
	if (0xff < (unk2 & 0x1ff00)) {
		*(volatile uint32_t *)(i14 + 0x900) =
		    *(volatile uint32_t *)(i14 + 0x900) & 0xfffffffc | 2;
		*(volatile uint32_t *)(i14 + 0x904) =
		    *(volatile uint32_t *)(i14 + 0x904) & 0xfffffffc | 2;
		*(volatile uint32_t *)(i14 + 0x908) =
		    *(volatile uint32_t *)(i14 + 0x908) & 0xfffffffc | 2;
		if ((&DSI_BYTE(0x6))[i13] == '\x03') {
			*(volatile uint32_t *)(i14 + 0x90c) =
			    *(volatile uint32_t *)(i14 + 0x90c) & 0xfffffffc | 2;
		}
	}
	*(volatile uint32_t *)(i14 + 0x940) = 0x200;
	*(volatile uint32_t *)(i14 + 0x944) = 0x200;
	*(volatile uint32_t *)(i14 + 0x948) = 0x200;
	if ((&DSI_BYTE(0x6))[i13] == '\x03') {
		*(volatile uint32_t *)(i14 + 0x94c) = 0x200;
	}
	if ((&DSI_BYTE(0x5))[i13] == '\x18') {
		p10 = (uint32_t *)p15[2];
	} else {
		p10 = (uint32_t *)p15[4];
	}
	if (p10 != (uint32_t *)0x0) {
		unk3 = p10[4];
		unk2 = p10[1];
		unk18 = p10[2];
		unk20 = p10[3];
		*(volatile uint32_t *)(i14 + 0x810) = *p10;
		*(volatile uint32_t *)(i14 + 0x814) = unk2;
		unk6 = p10[5];
		*(volatile uint32_t *)(i14 + 0x818) = unk18;
		unk19 = p10[6];
		*(volatile uint32_t *)(i14 + 0x81c) = unk20;
		unk20 = p10[7];
		unk18 = p10[8];
		*(volatile uint32_t *)(i14 + 0x820) = unk3;
		*(volatile uint32_t *)(i14 + 0x824) = unk6;
		*(volatile uint32_t *)(i14 + 0x828) = unk19;
		*(volatile uint32_t *)(i14 + 0x82c) = unk20;
		*(volatile uint32_t *)(i14 + 0x830) = unk18;
		*(volatile uint32_t *)(i14 + 0x83c) = unk2 | unk2 << 0x10;
	}
	if (param_1 == 1) {
		*(volatile uint32_t *)(i14 + 0x834) = 0xf;
		*(volatile uint32_t *)(i14 + 0x838) = 0;
		*(volatile uint32_t *)(i14 + 0x804) = 1;
		*(volatile uint32_t *)(i14 + 0x514) = 0xffffffff;
		if (DSI_BYTE(0x4f) == '\x01') {
			i13 = (p15[5] - (p15[10] + p15[9])) - p15[8];
			if ((int)(*p15 << 0x1c) < 0) {
				i13 = i13 + -2;
			}
			commands[1] = 0xa30000a2;
			if (DSI_BYTE(0x4e) == '\x03') {
				commands[1] = 0xa30000a4;
			}
			commands[0xc] = i13 * 0x300 & 0xffffffU | 0x4000003e;
			commands[0xe] =
			    (((p15[6] - 2) - p15[0xc]) - p15[0xd]) - p15[0xb] & 0xffff | 0x4030000;
			commands[6] = (p15[0xd] + p15[0xc]) - 3 & 0xffff | 0x4010000;
			commands[0x11] = commands[0xc];
			commands[0x14] = p15[0xb] - 2 & 0xffff | 0x4010000;
			commands[0] = 0x40effff;
			commands[2] = 0x28000004;
			commands[3] = 0x10000001;
			commands[0x17] = 0xc000000;
			i13 = 0x18;
			commands[4] = 0x28000001;
			commands[7] = 0x28000001;
			commands[9] = 0x28000001;
			commands[0x15] = 0x28000001;
			commands[5] = 0x10000021;
			commands[8] = 0x10000021;
			commands[10] = 0x10000021;
			commands[0xf] = 0x10000021;
			commands[0x13] = 0x10000021;
			commands[0x16] = 0x10000021;
			commands[0xb] = 0x40005619;
			commands[0x10] = 0x40005619;
			commands[0xd] = 0x40015019;
			commands[0x12] = 0x40015019;
			goto unk42a8;
		}
		if (p15[7] == 0) {
			unk5 = p15[0xb];
			unk7 = p15[0xd];
			unk2 = p15[8];
			unk4 = p15[0xc];
			unk8 = *p15;
			local_12c = (p15[6] - (unk5 + unk7)) - unk4;
			unk11 = (uint32_t)DSI_BYTE(0x4d);
			i13 = (p15[5] - (p15[10] + unk2)) - p15[9];
			flag22 = (unk8 & 1) != 0;
			if (flag22) {
				unk2 = unk11 * unk2;
			}
			unk16 = 0;
			if (flag22) {
				unk16 = (unk2 >> 3) - 0xc | 0x80000000;
			}
			if ((unk8 & 2) == 0) {
				unk2 = 0;
			} else {
				unk2 = (unk11 * p15[9] >> 3) - 10 | 0x80000000;
			}
			flag22 = (unk8 & 4) != 0;
			unk21 = unk11;
			if (!flag22) {
				local_134 = 0;
				unk21 = unk8 & 2;
			}
			if (flag22) {
				unk21 = p15[10] * unk21;
			}
			if (flag22) {
				local_134 = (unk21 >> 3) - 10 | 0x80000000;
			}
			commands[1] = 0xa30000a2;
			if (DSI_BYTE(0x4e) == '\x03') {
				commands[1] = 0xa30000a4;
			}
			commands[0] = 0x40dffff;
			if (unk16 != 0) {
				commands[0] = 0x417ffff;
			}
			commands[2] = 0x28000004;
			commands[3] = 0x10000001;
			if (unk2 == 0) {
				commands[4] = 0x28000002;
				commands[9] = 0x28000002;
				commands[0xd] = 0x28000002;
				commands[6] = unk4 - 2 & 0xffff | 0x4030000;
				commands[0xf] = unk7 - 2 & 0xffff | 0x4030000;
				commands[0x12] = 0x28000002;
				commands[5] = 0x10000031;
				commands[10] = 0x10000031;
				commands[0xe] = 0x10000031;
				commands[7] = 0x28000001;
				commands[0x10] = 0x28000001;
				commands[8] = 0x10000021;
				commands[0x11] = 0x10000021;
				commands[0xb] = 0x28000008;
				commands[0xc] = 0x10000011;
			} else {
				commands[4] = (unk2 & 0xffff) << 8 | 0x40000019;
				commands[6] = unk4 - 2 & 0xffff | 0x4030000;
				commands[0xf] = unk7 - 2 & 0xffff | 0x4030000;
				commands[9] = commands[4];
				commands[0xd] = commands[4];
				commands[0x12] = commands[4];
				commands[5] = 0x10000031;
				commands[10] = 0x10000031;
				commands[0xe] = 0x10000031;
				commands[7] = 0x28000001;
				commands[0x10] = 0x28000001;
				commands[8] = 0x10000021;
				commands[0x11] = 0x10000021;
				commands[0xb] = 0x28000008;
				commands[0xc] = 0x10000011;
			}
			commands[0x11] = 0x10000021;
			commands[0xe] = 0x10000031;
			commands[0xc] = 0x10000011;
			commands[10] = 0x10000031;
			commands[8] = 0x10000021;
			commands[5] = 0x10000031;
			commands[0x13] = 0x10000031;
			if (unk16 == 0) {
				commands[0x15] = 0x28000001;
				commands[0x16] = 0x10000021;
				commands[0x14] = local_12c - 1U & 0xffff | 0x4050000;
				commands[0x17] = 0x28000002;
				if (unk2 != 0) {
					commands[0x17] = (unk2 & 0xffff) << 8 | 0x40000019;
				}
				commands[0x18] = 0x10000031;
				commands[0x19] = 0x28000010;
				if (local_134 != 0) {
					commands[0x19] = (local_134 & 0xffff) << 8 | 0x40000019;
				}
				if (unk11 == 0x18) {
					commands[26] = i13 * 0x300 & 0xffffffU | 0x4000003e;
				} else {
					commands[26] = i13 * 0x3c0 & 0xffff00U | 0x40000029;
				}
				commands[29] = 0x10000021;
				commands[30] = 0x28000002;
				commands[27] = unk5 - 1 & 0xffff | 0x4030000;
				commands[28] = 0x28000001;
				if (unk2 != 0) {
					commands[30] = (unk2 & 0xffff) << 8 | 0x40000019;
				}
				p15 = &commands[32];
				commands[31] = 0x10000031;
			} else {
				commands[0x14] = 0x28000001;
				commands[0x15] = 0x10000021;
				commands[0x16] = 0x28000002;
				if (unk2 != 0) {
					commands[0x16] = (unk2 & 0xffff) << 8 | 0x40000019;
				}
				commands[0x17] = 0x10000031;
				commands[0x18] = 0x28000010;
				if (local_134 != 0) {
					commands[0x18] = (local_134 & 0xffff) << 8 | 0x40000019;
				}
				if (unk11 == 0x18) {
					commands[0x19] = i13 * 0x300 & 0xffffffU | 0x4000003e;
				} else {
					commands[0x19] = i13 * 0x3c0 & 0xffff00U | 0x40000029;
				}
				commands[26] = (unk16 & 0xffff) << 8 | 0x40000019;
				commands[28] = 0x10000021;
				commands[29] = 0x28000002;
				commands[27] = local_12c - 2U & 0xffff | 0x4050000;
				if (unk2 != 0) {
					commands[29] = (unk2 & 0xffff) << 8 | 0x40000019;
				}
				commands[30] = 0x10000031;
				if (local_134 == 0) {
					commands[31] = 0x28000010;
				} else {
					commands[31] = (local_134 & 0xffff) << 8 | 0x40000019;
				}
				if (unk11 == 0x18) {
					commands[32] = i13 * 0x300 & 0xffffffU | 0x4000003e;
				} else {
					commands[32] = i13 * 0x3c0 & 0xffff00U | 0x40000029;
				}
				commands[33] = commands[26];
				commands[34] = 0x10000021;
				if (unk2 == 0) {
					commands[39] = 0x10000021;
					commands[35] = 0x28000002;
					commands[40] = 0x28000002;
					commands[36] = 0x10000031;
					commands[37] = unk5 - 2 & 0xffff | 0x4030000;
					commands[38] = 0x28000001;
				} else {
					commands[39] = 0x10000021;
					commands[35] = (unk2 & 0xffff) << 8 | 0x40000019;
					commands[40] = commands[35];
					commands[36] = 0x10000031;
					commands[37] = unk5 - 2 & 0xffff | 0x4030000;
					commands[38] = 0x28000001;
				}
				commands[39] = 0x10000021;
				commands[38] = 0x28000001;
				commands[36] = 0x10000031;
				p15 = commands + 42;
				commands[41] = 0x10000031;
				commands[35] = commands[40];
			}
			*p15 = 0xc000000;
			i13 = (int)p15 + (4 - (int)commands);
		} else {
			unk4 = p15[0xb];
			unk5 = p15[0xc];
			unk7 = p15[0xd];
			unk11 = p15[6] + 1 + (unk5 + unk4 + unk7) * -2 >> 1;
			i13 = (p15[5] - (p15[9] + p15[10])) - p15[8];
			unk8 = (uint32_t)DSI_BYTE(0x4d);
			unk2 = 0;
			if ((*p15 & 2) != 0) {
				unk2 = (unk8 * p15[9] >> 3) - 10 | 0x80000000;
			}
			flag22 = (*p15 & 4) != 0;
			unk16 = 0;
			if (flag22) {
				unk16 = p15[10] * unk8;
			}
			if (flag22) {
				unk16 = (unk16 >> 3) - 10 | 0x80000000;
			}
			commands[1] = 0xa30000a2;
			if (DSI_BYTE(0x4e) == '\x03') {
				commands[1] = 0xa30000a4;
			}
			commands[0] = 0x415ffff;
			commands[2] = 0x28800005;
			commands[3] = 0x10000081;
			if (unk2 == 0) {
				local_130 = unk11 - 1 & 0xffff | 0x4050000;
				commands[0x14] = local_130;
				commands[0xf] = unk7 - 3 & 0xffff | 0x4030000;
				commands[4] = 0x28000002;
				commands[6] = unk5 - 2 & 0xffff | 0x4030000;
				commands[9] = 0x28000002;
				commands[0xd] = 0x28000002;
				commands[0x12] = 0x28000002;
				commands[0x17] = 0x28000002;
				commands[5] = 0x100000b1;
				commands[10] = 0x100000b1;
				commands[0xe] = 0x100000b1;
				commands[0x13] = 0x100000b1;
				commands[7] = 0x28000001;
				commands[0x10] = 0x28000001;
				commands[0x15] = 0x28000001;
				commands[8] = 0x100000a1;
				commands[0x11] = 0x100000a1;
				commands[0x16] = 0x100000a1;
				commands[0xb] = 0x28000008;
				commands[0xc] = 0x10000091;
			} else {
				local_130 = unk11 - 1 & 0xffff | 0x4050000;
				commands[0x14] = local_130;
				commands[4] = (unk2 & 0xffff) << 8 | 0x40000099;
				commands[9] = commands[4];
				commands[0xd] = commands[4];
				commands[0x12] = commands[4];
				commands[0x17] = commands[4];
				commands[6] = unk5 - 2 & 0xffff | 0x4030000;
				commands[5] = 0x100000b1;
				commands[10] = 0x100000b1;
				commands[0xe] = 0x100000b1;
				commands[0x13] = 0x100000b1;
				commands[0xf] = unk7 - 3 & 0xffff | 0x4030000;
				commands[7] = 0x28000001;
				commands[0x10] = 0x28000001;
				commands[0x15] = 0x28000001;
				commands[8] = 0x100000a1;
				commands[0x11] = 0x100000a1;
				commands[0x16] = 0x100000a1;
				commands[0xb] = 0x28000008;
				commands[0xc] = 0x10000091;
			}
			commands[0x16] = 0x100000a1;
			commands[0x15] = 0x28000001;
			commands[0x13] = 0x100000b1;
			commands[0x11] = 0x100000a1;
			commands[0xe] = 0x100000b1;
			commands[0xc] = 0x10000091;
			commands[10] = 0x100000b1;
			commands[8] = 0x100000a1;
			commands[5] = 0x100000b1;
			commands[0x18] = 0x100000b1;
			commands[0x19] = 0x28000010;
			if (unk16 != 0) {
				commands[0x19] = (unk16 & 0xffff) << 8 | 0x40000099;
			}
			if (unk8 == 0x18) {
				commands[26] = i13 * 0x300 & 0xffffffU | 0x400000be;
			} else {
				commands[26] = i13 * 0x3c0 & 0xffff00U | 0x400000a9;
			}
			commands[29] = 0x100000a1;
			commands[27] = unk4 & 0xffff | 0x4030000;
			commands[28] = 0x28000001;
			if (unk2 == 0) {
				commands[35] = 0x28000001;
				commands[36] = 0x100000a1;
				commands[42] = 0x28000001;
				commands[43] = 0x100000a1;
				commands[34] = unk5 - 1 & 0xffff | 0x4030000;
				commands[41] = unk7 - 2 & 0xffff | 0x4030000;
				commands[46] = local_130;
				commands[47] = 0x28000001;
				commands[48] = 0x100000a1;
				commands[30] = 0x28000002;
				commands[37] = 0x28000002;
				commands[44] = 0x28000002;
				commands[49] = 0x28000002;
				commands[31] = 0x100000b1;
				commands[38] = 0x100000b1;
				commands[45] = 0x100000b1;
				commands[32] = 0x28000004;
				commands[33] = 0x100000c1;
				commands[39] = 0x28000008;
				commands[40] = 0x100000d1;
			} else {
				commands[35] = 0x28000001;
				commands[36] = 0x100000a1;
				commands[42] = 0x28000001;
				commands[34] = unk5 - 1 & 0xffff | 0x4030000;
				commands[41] = unk7 - 2 & 0xffff | 0x4030000;
				commands[43] = 0x100000a1;
				commands[30] = (unk2 & 0xffff) << 8 | 0x40000099;
				commands[46] = local_130;
				commands[47] = 0x28000001;
				commands[48] = 0x100000a1;
				commands[37] = commands[30];
				commands[44] = commands[30];
				commands[49] = commands[30];
				commands[31] = 0x100000b1;
				commands[38] = 0x100000b1;
				commands[45] = 0x100000b1;
				commands[32] = 0x28000004;
				commands[33] = 0x100000c1;
				commands[39] = 0x28000008;
				commands[40] = 0x100000d1;
			}
			commands[46] = local_130;
			commands[48] = 0x100000a1;
			commands[47] = 0x28000001;
			commands[45] = 0x100000b1;
			commands[43] = 0x100000a1;
			commands[42] = 0x28000001;
			commands[40] = 0x100000d1;
			commands[39] = 0x28000008;
			commands[38] = 0x100000b1;
			commands[36] = 0x100000a1;
			commands[35] = 0x28000001;
			commands[33] = 0x100000c1;
			commands[32] = 0x28000004;
			commands[31] = 0x100000b1;
			commands[50] = 0x100000b1;
			commands[51] = 0x28000010;
			if (unk16 != 0) {
				commands[51] = (unk16 & 0xffff) << 8 | 0x40000099;
			}
			if (unk8 == 0x18) {
				commands[52] = i13 * 0x300 & 0xffffffU | 0x400000be;
			} else {
				commands[52] = i13 * 0x3c0 & 0xffff00U | 0x400000a9;
			}
			commands[55] = 0x100000a1;
			commands[53] = unk4 - 1 & 0xffff | 0x4030000;
			commands[54] = 0x28000001;
			commands[56] = 0x28000002;
			if (unk2 != 0) {
				commands[56] = (unk2 & 0xffff) << 8 | 0x40000099;
			}
			i13 = 0xec;
			commands[57] = 0x100000b1;
			commands[58] = 0xc000000;
			commands[0x12] = commands[0x17];
			commands[0x14] = local_130;
			commands[30] = commands[49];
			commands[37] = commands[49];
			commands[44] = commands[49];
		}
		commands[0x10] = 0x28000001;
		commands[0xb] = 0x28000008;
		commands[7] = 0x28000001;
		i13 = i13 >> 2;
		commands[4] = commands[0x12];
		commands[9] = commands[0x12];
		commands[0xd] = commands[0x12];
		if (0 < i13)
			goto unk42a8;
	} else {
		unk2 = p15[9];
		unk7 = p15[8];
		unk21 = p15[10];
		unk11 = p15[5];
		unk4 = p15[6];
		commands[0] = 0x407ffff;
		unk8 = p15[0xc];
		*(volatile uint32_t *)(i14 + 0x834) = 7;
		unk5 = p15[0xd];
		unk16 = p15[0xb];
		*(volatile uint32_t *)(i14 + 0x838) = 0;
		*(volatile uint32_t *)(i14 + 0x804) = 1;
		*(volatile uint32_t *)(i14 + 0x514) = 0xffffffff;
		commands[4] = 0x400b7819;
		commands[0xc] = unk21 * 0x300 - 0xa00 & 0xffffff | 0x40000019;
		if ((&DSI_BYTE(0x7))[i13] == '\0') {
			commands[4] = 0x400b4019;
		}
		commands[1] = 0xa3000082;
		commands[3] = 0x10000001;
		i13 = 0x13;
		commands[2] = 0x28000004;
		commands[0xd] = ((unk11 - (unk7 + unk2)) - unk21) * 0x300 & 0xffffff | 0x4000003e;
		commands[5] = (unk8 + unk5) - 2 & 0xffff | 0x4020000;
		commands[9] = (((unk4 - 1) - unk8) - unk5) - unk16 & 0xffff | 0x4030000;
		commands[0xe] = unk16 - 1 & 0xffff | 0x4020000;
		commands[0x12] = 0xc000000;
		commands[8] = commands[4];
		commands[0x11] = commands[4];
		commands[6] = 0x28000001;
		commands[10] = 0x28000001;
		commands[0xf] = 0x28000001;
		commands[7] = 0x10000021;
		commands[0xb] = 0x10000021;
		commands[0x10] = 0x10000021;
	unk42a8:
		p9 = (int *)commands - 1;
		i12 = 0;
		do {
			p9 = p9 + 1;
			i12 = i12 + 1;
			*(int *)(i14 + 0x500) = *p9;
		} while (i12 < i13);
	}
	*(volatile uint32_t *)(i14 + 0x51c) = 1;
	(&DSI_WORD(0x14))[param_1 * 0x12] = 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(p17, unk1);
	ksceKernelEnableIntr((&DSI_WORD(0x40))[param_1 * 0x12]);
	unk1 = 0;
unk42d0:
	return unk1;
}

/* Command program construction; intermediate field names are unknown. */
uint32_t ksceDsiStartDisplay(unsigned param_1, unsigned param_2)
{
	uint32_t unk1;
	int i2;
	int i3;
	uint32_t unk4;
	uint32_t unk5;
	int i6;
	int i7;
	uint32_t unk8;
	SceKernelSpinlock *p9;
	uint32_t unk10;
	volatile uint32_t *p11;
	int *p12;
	int i13;
	uint32_t unk14;
	int i15;
	int i16;
	uint32_t unk17;
	uint32_t unk18;
	int i19;
	int i20;
	uint32_t unk21;

	if (1 < param_1) {
		return 0x803f0600;
	}
	if (4 < param_2) {
		return 0x803f0602;
	}
	i15 = param_1 * 0x48;
	p9 = (SceKernelSpinlock *)((uint8_t *)&DSI_WORD(0x38) + i15);
	unk1 = ksceKernelSpinlockLowLockCpuSuspendIntr(p9);
	if ((&DSI_WORD(0x14))[param_1 * 0x12] == 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(p9, unk1);
		return 0x803f0601;
	}
	p12 = (int *)(&DSI_WORD(0x10))[param_1 * 0x12];
	p11 = (volatile uint32_t *)(&DSI_WORD(0x0))[param_1 * 0x12];
	i6 = p12[5];
	i20 = p12[9];
	i16 = i20 + p12[10];
	unk18 = p12[6];
	i2 = p12[0xb];
	i7 = (i6 - i16) - p12[8];
	if (*p12 << 0x1c < 0) {
		i7 = i7 + -2;
	}
	unk17 = p12[0xc];
	i3 = p12[0xd];
	i13 = p12[7];
	if (i13 == 1) {
		unk10 = 3;
	} else {
		unk10 = 0;
	}
	p11[1] = unk10;
	if ((&DSI_BYTE(0x6))[param_1 * 0x48] == '\x02') {
		i19 = 6;
	} else if ((&DSI_BYTE(0x5))[param_1 * 0x48] == '\x18') {
		i19 = 4;
	} else {
		i19 = 5;
	}
	unk21 = i20 * i19;
	unk4 = (uint32_t)(i6 * i19) >> 1;
	p11[2] = unk4;
	if (i13 == 1) {
		unk8 = unk18 + 1 >> 1;
		unk14 = (uint32_t)(i6 * i19) >> 2;
		p11[3] = (unk18 & 0x1fff) << 0x10 | unk8 & 0x1fff;
		p11[4] = 0x10001;
		p11[5] = ((uint32_t)((i7 + i16) * i19) >> 1) + 1 & 0xffff |
			 ((uint32_t)(i16 * i19) >> 1) << 0x10;
		p11[7] = (i3 + unk17 + -1) * 0x10000 & 0x1fff0000 | unk8 - i2 & 0x1fff;
		p11[8] = (i3 + unk17 + (unk18 - 1 >> 1)) * 0x10000 & 0x1fff0000 |
			 (unk18 + 1) - i2 & 0x1fff;
		p11[9] = (unk21 >> 1) << 0x10 | 1;
		p11[0xb] = 0x10001;
		p11[0xc] = (unk17 & 0x1fff) << 0x10 | unk4 & 0xffff;
		p11[0xd] = (unk8 & 0x1fff) << 0x10 | unk14 + 1 & 0xffff;
		p11[0xe] = (unk17 + unk8) * 0x10000 & 0x1fff0000 | unk14 & 0xffff;
	} else {
		p11[3] = unk18;
		unk8 = unk18 + 1 >> 1;
		p11[4] = 0x10001;
		p11[5] = ((uint32_t)((i7 + i16) * i19) >> 1) + 1 & 0xffff |
			 ((uint32_t)(i16 * i19) >> 1) << 0x10;
		p11[7] = (unk18 + 1) - i2 & 0x1fff | (i3 + unk17) * 0x10000 & 0x1fff0000;
		p11[9] = (unk21 >> 1) << 0x10 | 1;
		p11[0xb] = 0x10001;
		p11[0xc] = unk4 & 0xffff | (unk17 & 0x1fff) << 0x10;
	}
	if (((&DSI_BYTE(0x6))[param_1 * 0x48] == '\x03') &&
	    ((&DSI_BYTE(0x5))[param_1 * 0x48] == '\x1e')) {
		if (unk18 == 0x20d) {
			p11[5] = 0x130083a;
			p11[6] = 0x131083a;
			p11[9] = 0x9a0001;
			p11[10] = 0x9b0861;
			unk10 = 0x440;
			unk5 = 4;
		} else {
			if (unk18 != 0x2ee)
				goto unk4eec;
			p11[5] = 0x28a0f0b;
			p11[6] = 0x2890f0b;
			p11[9] = 0x640001;
			p11[10] = 0x63101d;
			unk10 = 0;
			unk5 = 0x444;
		}
		p11[0x1d] = unk10;
		p11[0x1e] = unk5;
		p11[1] = p11[1] | 0x100;
	}
unk4eec:
	p11[0xf] = 1;
	i3 = i3 + unk17;
	p11[0x10] = (unk8 + (i3 - 8U)) * 0x10000 & 0x1fff0000 | i3 - 8U & 0x1fff;
	if (i13 == 1) {
		p11[0x17] = (unk8 & 0x1fff) << 0x10 | 1;
		p11[0x18] = (i3 + (unk18 - 1 >> 1)) * 0x10000 & 0x1fff0000 | 1;
		p11[0x1b] = (unk18 + 1) * 0x4000 & 0x1fff0000 | 0x40000001;
		p11[0x19] = (unk18 & 0x1fff) << 0x10 | 1;
		p11[0x1a] = (i3 + -1) * 0x10000 & 0x1fff0000U | 1;
		p11[0x1c] = (unk18 + 1) * 0xc000 & 0x1fff0000 | 0x40000001;
	} else {
		p11[0x17] = 0x10001;
		p11[0x18] = i3 * 0x10000 & 0x1fff0000U | 1;
		p11[0x1b] = (i3 + (unk18 >> 1)) * 0x10000 & 0x1fff0000 | 0x40000001;
		p11[0x1c] = 0;
	}
	unk10 = (&DSI_WORD(0x44))[param_1 * 0x12];
	p11[0x14] = p11[0x14];
	p11[0x15] = unk10;
	if ((&DSI_BYTE(0x4))[i15] == '\0') {
		p11[0x20e] = 0;
	} else {
		p11[0x20e] = 1;
	}
	if (param_2 == 1) {
		unk17 = (param_1 ^ 1) & 1;
	} else {
		unk17 = 0;
	}
	p11[0x142] = 0xffffffff;
	*(uint64_t *)((uint8_t *)&DSI_WORD(0x28) + i15) = 0;
	*(uint32_t *)((uint8_t *)&DSI_WORD(0x18) + i15) = i6 * unk18;
	*(uint32_t *)((uint8_t *)&DSI_WORD(0x1c) + i15) = i6 * unk18;
	*(uint64_t *)((uint8_t *)&DSI_WORD(0x20) + i15) = 0;
	if (unk17 == 0) {
		if (param_2 == 2) {
			param_1 = param_1 & 1;
		} else {
			param_1 = 0;
		}
		if ((param_1 == 0) && (param_2 - 1 < 4)) {
			unk10 = *(uint32_t *)((uint8_t *)s_dsiDisplayModes + (param_2 - 1) * 4);
			goto unk4fac;
		}
	}
	unk10 = 1;
unk4fac:
	*p11 = unk10;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(p9, unk1);
	return 0;
}
uint32_t ksceDsiDisableHead(unsigned head)
{
	uint32_t *ctx;
	uint8_t *base;
	uint32_t state;

	if (head > 1)
		return DSI_ERROR_HEAD;
	ctx = s_dsiState[head];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[14]);
	if (ctx[5]) {
		ctx[5] = 0;
		ksceKernelDisableIntr(ctx[16]);
		base = (uint8_t *)ctx[0];
		while (DSI_REG(base, 0x48) & 0xf)
			;
		DSI_REG(base, 0x514) = 0xffffffff;
		DSI_REG(base, 0x940) = 1;
		DSI_REG(base, 0x944) = 1;
		DSI_REG(base, 0x948) = 1;
		if (DSI_BYTE(head * 0x48 + 6) == 3)
			DSI_REG(base, 0x94c) = 1;
		DSI_REG(base, 0x834) = 0;
		DSI_REG(base, 0x804) = 1;
		(void)DSI_REG(base, 0x410);
		LowioDsb();
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
		ScePervasiveForDriver_F1908D3C(head);
		return 0;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
	return 0;
}

uint32_t ksceDsiSendBlankingPacket(unsigned head)
{
	uint32_t *ctx;
	uint8_t *base;
	uint32_t state;
	bool threeLanes;

	if (head > 1)
		return DSI_ERROR_HEAD;
	ctx = s_dsiState[head];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[14]);
	if (ctx[5]) {
		base = (uint8_t *)ctx[0];
		threeLanes = DSI_BYTE(head * 0x48 + 6) == 3;
		DSI_REG(base, 0x504) = 0xc;
		(void)DSI_REG(base, 0x504);
		LowioDmb();
		DSI_REG(base, 0x50c) = DSI_REG(base, 0x414);
		while (DSI_REG(base, 0x414) & 0x1000000)
			;
		DSI_REG(base, 0x500) = threeLanes ? 0xa30000a4 : 0xa30000a2;
		DSI_REG(base, 0x500) = 0x40000019;
		DSI_REG(base, 0x500) = threeLanes ? 0x30000079 : 0x30000039;
		DSI_REG(base, 0x500) = 0xa6002000;
		DSI_REG(base, 0x500) = 0x18000001;
		DSI_REG(base, 0x508) = 0xffffffff;
		LowioDmb();
		while (!(DSI_REG(base, 0x414) & 0x1000000))
			;
		DSI_REG(base, 0x514) = 0xffffffff;
		DSI_REG(base, 0x50c) = 0x1000000;
		DSI_REG(base, 0x504) = 0xc;
		(void)DSI_REG(base, 0x504);
		LowioDmb();
		DSI_REG(base, 0) = 0;
		(void)DSI_REG(base, 0);
		LowioDmb();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
	return 0;
}

uint32_t SceDsiForDriver_6F8029A1(unsigned head)
{
	uint8_t *base;

	if (head > 1)
		return DSI_ERROR_HEAD;
	if (!s_dsiState[head][5])
		return DSI_ERROR_DISABLED;
	base = (uint8_t *)s_dsiState[head][0];
	return (DSI_REG(base, 0x4c) >> 16 & 0x1fff) - 1;
}

/* name unknown: packet sequencer used by the short-write exports. */
static uint32_t DsiShortWrite(unsigned head, uint32_t packet)
{
	uint32_t *ctx;
	uint8_t *base;
	uint32_t state;
	static const uint32_t words[4] = {
	    0x8e00bf7f,
	    0x911affff,
	    0x940001ff,
	    0x86043f0f,
	};
	unsigned i;

	if (head > 1)
		return DSI_ERROR_HEAD;
	ctx = s_dsiState[head];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[14]);
	if (!ctx[5]) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
		return DSI_ERROR_DISABLED;
	}
	base = (uint8_t *)ctx[0];
	DSI_REG(base, 0x51c) = 1;
	while (!(DSI_REG(base, 0x410) & 0x100000))
		;
	DSI_REG(base, 0x504) = 8;
	(void)DSI_REG(base, 0x504);
	LowioDsb();
	for (i = 0; i < 4; i++)
		DSI_REG(base, 0x510) = words[i];
	DSI_REG(base, 0x510) = DSI_BYTE(head * 0x48 + 6) == 3 ? 0x83003c25 : 0x83003c23;
	DSI_REG(base, 0x510) = packet | 0x10000000;
	DSI_REG(base, 0x510) = 0x30000003;
	DSI_REG(base, 0x510) = 0xc000000;
	DSI_REG(base, 0x50c) = DSI_REG(base, 0x414);
	while (DSI_REG(base, 0x414) & 0x41007)
		;
	DSI_REG(base, 0x51c) = 0;
	while (!(DSI_REG(base, 0x414) & 0x41007))
		;
	DSI_REG(base, 0x50c) = 0x1000;
	while (DSI_REG(base, 0x414) & 0x1000)
		;
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
	return 0;
}

uint32_t ksceDsiGenericShortWrite(unsigned head, int a1, int a2)
{
	uint32_t packet;

	if (a1 < 0)
		packet = 3;
	else if (a2 < 0)
		packet = (a1 & 0xff) << 8 | 0x13;
	else
		packet = (a1 & 0xff) << 8 | (a2 & 0xff) << 16 | 0x23;
	return DsiShortWrite(head, packet);
}

uint32_t ksceDsiDcsShortWrite(unsigned head, int a1, int a2)
{
	uint32_t packet = ((uint32_t)a1 << 8 & 0xffff) | 5;

	if (a2 >= 0)
		packet = ((uint32_t)a1 << 8 & 0xffff) | 0x15 | (a2 & 0xff) << 16;
	return DsiShortWrite(head, packet);
}

/* name unknown */
static uint32_t DsiRead(unsigned head, uint32_t packet, uint8_t *data, unsigned length)
{
	uint32_t *ctx;
	uint8_t *base;
	uint32_t state, value;
	unsigned i = 0;

	if (head > 1)
		return DSI_ERROR_HEAD;
	if (length > 32)
		return DSI_ERROR_PARAM;
	ctx = s_dsiState[head];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[14]);
	if (!ctx[5]) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
		return DSI_ERROR_DISABLED;
	}
	base = (uint8_t *)ctx[0];
	DSI_REG(base, 0x51c) = 1;
	while (!(DSI_REG(base, 0x410) & 0x100000))
		;
	DSI_REG(base, 0x504) = 8;
	(void)DSI_REG(base, 0x504);
	LowioDsb();
	DSI_REG(base, 0x510) = DSI_BYTE(head * 0x48 + 6) == 3 ? 0x83003c25 : 0x83003c23;
	DSI_REG(base, 0x510) = length << 8 | 0x10000037;
	DSI_REG(base, 0x510) = packet | 0x10000000;
	DSI_REG(base, 0x510) = 0xc000000;
	DSI_REG(base, 0x50c) = DSI_REG(base, 0x414);
	while (DSI_REG(base, 0x414) & 0x41007)
		;
	DSI_REG(base, 0x51c) = 0;
	while (!(DSI_REG(base, 0x414) & 0x41007))
		;
	if (length && !(DSI_REG(base, 0x410) & 0x20)) {
		do {
			value = DSI_REG(base, 0x430);
			data[i] = value;
			data[i + 1] = value >> 8;
			data[i + 2] = value >> 16;
			data[i + 3] = value >> 24;
			i += 4;
			if (i >= length)
				break;
		} while (!(DSI_REG(base, 0x410) & 0x20));
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
	return 0;
}

uint32_t ksceDsiGenericReadRequest(unsigned head, int a1, void *data, unsigned length)
{
	uint32_t packet = a1 < 0 ? 4 : (a1 & 0xff) << 8 | 0x14;

	return DsiRead(head, packet, data, length);
}

uint32_t ksceDsiDcsRead(unsigned head, int16_t a1, void *data, unsigned length)
{
	return DsiRead(head, (uint32_t)(int32_t)a1 << 8 | 6, data, length);
}

/* name unknown */
static int DsiSysEvent(int resume, int eventid, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (!resume && eventid == 0x4004) {
		ksceDsiSendBlankingPacket(0);
		ksceDsiSendBlankingPacket(1);
		ksceDsiDisableHead(0);
		ksceDsiDisableHead(1);
	}
	return 0;
}

/* name unknown */
static uint32_t DsiSubIntrBit(unsigned subIntr)
{
	subIntr &= 0xff;
	return subIntr < 32 ? 1U << subIntr : 0;
}

/* name unknown */
static int DsiIntrPending(int intrCode, unsigned subIntr)
{
	uint32_t *ctx = s_dsiState[intrCode == 0xd2 ? 1 : 0];
	unsigned shift = (subIntr + 1) & 0xff;
	uint32_t value = DSI_REG((uint8_t *)ctx[0], 0x50);

	return shift < 32 ? value >> shift & 1 : 0;
}

/* name unknown */
static int DsiUpdateSubIntr(int intrCode, unsigned subIntr, void *handler)
{
	uint32_t *ctx = s_dsiState[intrCode == 0xd2 ? 1 : 0];
	uint32_t state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[14]);
	uint32_t bit = DsiSubIntrBit(subIntr);

	ctx[17] = (ctx[17] & ~bit) | (handler ? bit : 0);
	if (ctx[5]) {
		DSI_REG((uint8_t *)ctx[0], 0x54) = ctx[17];
		(void)DSI_REG((uint8_t *)ctx[0], 0x54);
		LowioDmb();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
	return 0;
}

/* name unknown */
static int DsiSuspendSubIntr(int intrCode, unsigned subIntr, uint32_t *enabled)
{
	uint32_t *ctx = s_dsiState[intrCode == 0xd2 ? 1 : 0];
	uint32_t state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[14]);

	if (enabled)
		*enabled = !!(ctx[17] & DsiSubIntrBit(subIntr));
	ctx[17] &= ~DsiSubIntrBit(subIntr);
	if (ctx[5]) {
		DSI_REG((uint8_t *)ctx[0], 0x54) = ctx[17];
		(void)DSI_REG((uint8_t *)ctx[0], 0x54);
		LowioDmb();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
	return 0;
}

/* name unknown */
static int DsiDisableSubIntr(int intrCode, unsigned subIntr)
{
	return DsiUpdateSubIntr(intrCode, subIntr, NULL);
}

/* name unknown */
static int DsiEnableSubIntr(int intrCode, unsigned subIntr)
{
	return DsiUpdateSubIntr(intrCode, subIntr, (void *)1);
}

/* name unknown */
static int DsiIntrHandler(int intrCode, void *arg)
{
	uint32_t *ctx = arg;
	const uint32_t *mode = (const uint32_t *)ctx[4];
	uint8_t *base = (uint8_t *)ctx[0];
	uint32_t state = ksceKernelSpinlockLowLockCpuSuspendIntr((int *)&ctx[14]);
	uint32_t status, mask;
	uint64_t a, b;
	unsigned i;

	if (!ctx[5]) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
		return -2;
	}
	status = DSI_REG(base, 0x50);
	DSI_REG(base, 0x50) = status;
	LowioDmb();
	(void)DSI_REG(base, 0x50);
	LowioDsb();
	if (status & 2) {
		a = ((uint64_t)ctx[11] << 32 | ctx[10]) + ctx[7];
		b = ((uint64_t)ctx[9] << 32 | ctx[8]) + ctx[6];
		ctx[10] = a;
		ctx[11] = a >> 32;
		ctx[8] = b;
		ctx[9] = b >> 32;
		if (!mode[7]) {
			/* The original threshold is a zero-extended horizontal total. */
			if (a - b >= mode[5]) {
				b += mode[5];
				ctx[8] = b;
				ctx[9] = b >> 32;
				DSI_REG(base, 0xc) = mode[6] + 1;
			} else {
				DSI_REG(base, 0xc) = mode[6];
			}
		}
	}
	LowioDmb();
	mask = ctx[17];
	ksceKernelSpinlockLowUnlockCpuResumeIntr((int *)&ctx[14], state);
	for (i = 0; i < 5; i++) {
		if (status & mask & (1U << i))
			ksceKernelTriggerSubIntr(intrCode, i, (void *)i);
	}
	return -1;
}

/* SDK callback typedefs have an extra pointer indirection; retain the raw ABI. */
static const struct {
	uint32_t size;
	void *callbacks[9];
} s_dsiIntrHandlers = {
    0x28,
    {
	NULL,
	NULL,
	NULL,
	DsiDisableSubIntr,
	DsiEnableSubIntr,
	DsiDisableSubIntr,
	DsiSuspendSubIntr,
	DsiUpdateSubIntr,
	DsiIntrPending,
    },
};
static SceKernelIntrOptParam s_dsiIntrOpt = {
    0x14,
    5,
    (SceKernelIntrOptHandlers *)&s_dsiIntrHandlers,
    0,
    0,
};

/* name unknown: initializer at original offset 0x39f8. */
static __attribute__((noinline)) int DsiInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	const uint32_t *mode, *timing;
	void *base;
	unsigned i;
	int ret;

	memset(s_dsiState, 0, sizeof(s_dsiState));
	for (i = 0; i < 2; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = i ? 0xe5060000 : 0xe5050000;
		ret = ksceKernelAllocMemBlock(
		    i ? "SceDsi1Reg" : "SceDsi0Reg", 0x20100806, 0x1000, &opt);
		if (ret < 0)
			return ret;
		s_dsiState[i][15] = ret;
		ksceKernelGetMemBlockBase(ret, &base);
		s_dsiState[i][0] = (uint32_t)base;
	}
	s_dsiState[0][16] = 0xd5;
	s_dsiState[1][16] = 0xd2;
	ksceKernelRegisterIntrHandler(
	    0xd5, "SceDsi0", 0, DsiIntrHandler, s_dsiState[0], 0xa0, 0xf, &s_dsiIntrOpt);
	ksceKernelRegisterIntrHandler(
	    0xd2, "SceDsi1", 0, DsiIntrHandler, s_dsiState[1], 0xa0, 0xf, &s_dsiIntrOpt);
	ksceKernelRegisterSysEventHandler("SceDsi", DsiSysEvent, NULL);
	DSI_BYTE(4) = 0;
	DSI_BYTE(0x4c) = 1;
	ret = kscePervasiveGetSoCRevision();
	DSI_BYTE(7) = 0;
	DSI_BYTE(0x4e) = 3;
	timing = (ret & 0x1ff00) < 0x100 ? s_dsiUnk8a8c : s_dsiUnk8fa0;
	for (i = 0; i < 9; i++)
		s_dsiUnkb358[i] = timing[i];
	s_dsiVicMap[0].mode = s_dsiMode8d00;
	s_dsiVicMap[1].mode = s_dsiMode8bb4;
	s_dsiVicMap[2].mode = s_dsiMode8ed0;
	DSI_BYTE(5) = 24;
	DSI_BYTE(0x4d) = 24;
	DSI_BYTE(6) = 2;
	DSI_BYTE(0x4f) = 2;
	mode = DsiFindVic(0);
	if (mode) {
		s_dsiState[0][3] = 0;
		s_dsiState[0][4] = (uint32_t)mode;
	}
	mode = DsiFindVic(0x8300);
	if (mode) {
		s_dsiState[1][3] = 0x8300;
		s_dsiState[1][4] = (uint32_t)mode;
	}
	return 0;
}

int LowioDsiInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (DsiInit() < 0)
		return 2;
	return 0;
}
