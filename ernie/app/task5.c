/*
 * Task 5: the events of event flag 4, each bit cleared and handled: among them the power and
 * battery requests (powerreq.c).
 */
#pragma rtos_task task5

#include "ernie.h"
#include "kernel.h"
#include "powerreq.h"

extern unsigned char byteFF2EF;

extern void sub_2A5EC(void);
extern void sub_244DD(void);
extern void sub_2A593(void);
extern void sub_28F92(void);
extern void sub_2E499(void);
extern void sub_2817E(void);
extern void sub_21F8E(void);
extern void sub_21F53(void);
extern void sub_2DF06(void);
extern void sub_2E528(void);
extern void sub_2BBF2(void);
extern void sub_2C44F(void);

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
