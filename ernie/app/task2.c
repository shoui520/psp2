/*
 * Task 2: the events of event flag 3, each group of bits cleared and handled.
 */
#pragma rtos_task task2

#include "ernie.h"
#include "kernel.h"

extern unsigned int sub_2AB46(void);
extern unsigned char sub_10D7A(void);
extern unsigned char sub_10DC1(void);
extern void sub_15667(void);
extern void sub_1D162(void);
extern void sub_21F93(void);
extern void sub_2E8C6(void);
extern void sub_263AD(void);
extern void sub_1EF84(void);
extern void sub_2D254(void);
extern void sub_2D67C(void);
extern unsigned char sub_22542(unsigned char reg, int a, unsigned char *buf, int n);
extern void sub_10F77(unsigned char);
extern unsigned char sub_11138(void);
extern void sub_110D3(long);
extern unsigned char sub_10CB3(void);
extern unsigned char sub_10DEA(void);
extern unsigned char sub_10E06(void);
extern unsigned char sub_10E22(void);
extern void sub_23A42(int);
extern unsigned char sub_1B4B4(void);
extern unsigned char sub_11D1B(void);
extern unsigned char sub_27642(void);
extern void sub_275F0(int);
extern void sub_1BBCB(void);
extern void sub_1D728(void);
extern void sub_1BB60(int);
extern unsigned char sub_2764F(void);
extern void sub_1BB76(void);
extern void sub_1BBB2(int);
extern void sub_27631(void);
extern unsigned char sub_11D4B(void);
extern void sub_2607C(int);
extern void sub_260A4(void);
extern void sub_162E8(void);
extern void sub_261C9(FLGPTN ptn);
extern void sub_1D1A9(FLGPTN ptn);
extern void sub_10E32(void);
extern void sub_2D2A8(FLGPTN ptn);
extern void sub_1F33A(FLGPTN ptn);
extern void sub_2C415(void);

unsigned char sub_10C03(void);
unsigned char sub_10D08(void);
void sub_10B67(FLGPTN ptn);

void task2(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;

	ptn = 0;
	for (;;) {
		ercd = wai_flg(3, 0xffff, TWF_ORW, &ptn);
		if (ercd == E_OK) {
			if (ptn & 0x1000) {
				clr_flg(3, ~0x1000);
				sub_10C03();
			}
			if (ptn & 0x0101) {
				clr_flg(3, ~(ptn & 0x0101));
				sub_10B67(ptn);
			}
			if (ptn & 0x2000) {
				clr_flg(3, ~(ptn & 0x2000));
				sub_162E8();
			}
			if (ptn & 0x0e1e)
				sub_261C9(ptn);
			if (ptn & 0x00a0) {
				clr_flg(3, ~(ptn & 0x00a0));
				sub_1D1A9(ptn);
			}
			if (ptn & 0x8000) {
				clr_flg(3, ~0x8000);
				sub_10E32();
			}
			if (ptn & 0x4000) {
				clr_flg(3, ~0x4000);
				sub_2D2A8(ptn);
			}
			if (ptn & 0x0040) {
				clr_flg(3, ~(ptn & 0x0040));
				sub_1F33A(ptn);
			}
		}
		sub_2C415();
	}
}

void sub_10B67(FLGPTN ptn)
{
	unsigned char state;

	state = sub_1B4B4();
	if (ptn & 0x0001) {
		if (sub_11D1B()) {
			if (sub_27642()) {
				sub_275F0(0);
				switch (state) {
				case 0:
				case 3:
					sub_1BBCB();
					sub_1D728();
					sub_1BB60(2);
					break;
				case 1:
					break;
				}
			} else if (sub_2764F()) {
				sub_1BB76();
				sub_1BBB2(3);
				sub_275F0(0);
			} else {
				sub_1BBCB();
				sub_1BB76();
				sub_27631();
			}
		} else {
			sub_1BBCB();
			sub_1BB76();
			sub_27631();
		}
	}
	if (ptn & 0x0100) {
		if (sub_11D4B())
			sub_2607C(2);
		else
			sub_260A4();
	}
	return;
}

unsigned char sub_10C03(void)
{
	unsigned char ret;
	unsigned int state;

	ret = 0;
	state = sub_2AB46();
	if (state & 0x8000) {
		switch (state) {
		case 0x8020:
			ret = sub_10D7A();
			break;
		case 0x8008:
			ret = sub_10DC1();
			break;
		case 0x8004:
			ret = sub_10D08();
			break;
		case 0x8002:
			ret = sub_10CB3();
			break;
		case 0x8010:
			ret = sub_10DEA();
			break;
		case 0x8800:
			ret = sub_10E06();
			break;
		case 0x8040:
			ret = sub_10E22();
			break;
		}
		sub_23A42(2);
	} else {
		switch (state) {
		case 0x0002:
		case 0x0004:
		case 0x0008:
		case 0x0010:
		case 0x0020:
		case 0x0040:
		case 0x0800:
			break;
		}
	}
	return ret;
}

unsigned char sub_10D08(void)
{
	unsigned char ret;
	unsigned char value;

	ret = 0;
	sub_15667();
	sub_1D162();
	sub_21F93();
	sub_2E8C6();
	sub_263AD();
	if (sub_27642() || sub_2764F())
		sub_275F0(0);
	else
		sub_27631();
	sub_1EF84();
	sub_2D254();
	sub_2D67C();
	if (sub_22542(0x2d, 1, &value, 1) == 0)
		sub_10F77(value);
	if (sub_11138() == 1)
		sub_110D3(-1);
	return ret;
}
