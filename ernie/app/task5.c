/*
 * Task 5: the events of event flag 4, each bit cleared and handled: among them the power and
 * battery requests (powerreq.c).
 */
#pragma rtos_task task5

#include "ernie.h"
#include "config.h"
#include "kernel.h"
#include "powerreq.h"
#include "task5.h"

extern unsigned char byteFF2EE;
extern unsigned char byteFF2EF;
extern unsigned char elmoVersion[2];

unsigned int wordFF24E;

extern void sub_2A5EC(void);
extern void sub_244DD(void);
extern void sub_2A593(void);
extern void sub_28F92(void);
extern void sub_2E499(void);
extern void sub_2817E(void);
extern void sub_2DF06(void);
extern void sub_2E528(void);
extern void sub_2BBF2(void);
extern void sub_2C44F(void);
extern void sub_2C315(ID flgid, FLGPTN setptn);
extern unsigned char sub_126C9(void);
extern unsigned char sub_29CCF(void);
extern void sub_2DF86(unsigned int on);
extern void sub_2DFCE(unsigned int on);
extern void sub_2E03A(unsigned int on);
extern void sub_2E165(unsigned int);

static void sub_21F53(void);
static void sub_21F8E(void);

void task5(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;

	sub_2A5EC();
	for (;;) {
		ercd = wai_flg(4, 0xffab, TWF_ORW, &ptn);
		if (ercd == E_OK) {
			if (ptn & 0x0002) {
				clr_flg(4, ~0x0002);
				byteFF2EF = 0;
				sub_244DD();
			}
			if (ptn & 0x0080) {
				clr_flg(4, ~0x0080);
				sub_2A593();
			}
			if (ptn & 0x0001) {
				clr_flg(4, ~0x0001);
				powerRequestEvent();
			}
			if (ptn & 0x0100) {
				clr_flg(4, ~0x0100);
				batteryRequestEvent();
			}
			if (ptn & 0x0020) {
				clr_flg(4, ~0x0020);
				sub_28F92();
				sub_2E499();
			}
			if (ptn & 0x0400) {
				clr_flg(4, ~0x0400);
				sub_2817E();
			}
			if (ptn & 0x0008) {
				clr_flg(4, ~0x0008);
				sub_21F8E();
			}
			if (ptn & 0x1000) {
				clr_flg(4, ~0x1000);
				sub_21F53();
			}
			if (ptn & 0x0800) {
				clr_flg(4, ~0x0800);
				sub_2E499();
			}
			if (ptn & 0x2000) {
				clr_flg(4, ~0x2000);
				sub_2DF06();
			}
			if (ptn & 0x0200) {
				clr_flg(4, ~0x0200);
				sub_2E528();
			}
			if (ptn & 0x4000) {
				clr_flg(4, ~0x4000);
				sub_2BBF2();
			}
			if (ptn & 0x8000)
				clr_flg(4, ~0x8000);
		}
		sub_2C44F();
	}
}

void sub_21E89(void)
{
	byteFF2EE = 1;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21E9A(void)
{
	byteFF2EE = 2;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21EAC(void)
{
	byteFF2EE = 3;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21EBE(void)
{
	byteFF2EE = 4;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21ED0(void)
{
	byteFF2EE = 5;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21EE2(void)
{
	byteFF2EE = 6;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21EF4(void)
{
	byteFF2EE = 7;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21F06(void)
{
	byteFF2EE = 9;
	sub_2C315(4, 0x0002);
	return;
}

void sub_21F18(void)
{
	byteFF2EE = 8;
	sub_2C315(4, 0x0002);
	return;
}

unsigned char sub_21F2A(void)
{
	return elmoVersion[0];
}

void sub_21F33(unsigned int bits)
{
	wordFF24E |= bits;
	sub_2C315(4, 0x0080);
	return;
}

static void sub_21F53(void)
{
	if (sub_126C9()) {
		if (sub_2C658()->indicatorOn0) {
			sub_2DF86(1);
			return;
		}
		sub_2DF86(0);
		if (sub_29CCF())
			sub_2DFCE(1);
	} else {
		sub_2DFCE(0);
		sub_2E03A(0);
	}
	return;
}

static void sub_21F8E(void)
{
	sub_2E165(1);
}
