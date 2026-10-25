/*
 * Task 3: the events of event flag 2, each bit cleared and handled: the keys, the scan of the
 * inputs and the requests that come with them.
 */
#pragma rtos_task task3
#pragma sfr

#include "ernie.h"
#include "input.h"
#include "kernel.h"

extern unsigned int sub_2AB46(void);
extern void sub_18DD6(void);
extern void sub_1C40A(unsigned int keys);
extern void sub_18DDF(void);
extern void sub_17890(void);
extern void sub_17B95(unsigned char *data);
extern void sub_1791D(void);
extern void sub_17C11(unsigned char *data);
extern void sub_1875E(void);
extern void sub_2C48E(void);

void task3(VP_INT exinf)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char data[2];

	for (;;) {
		ercd = wai_flg(2, 0x00dc, TWF_ORW, &ptn);
		if (ercd == E_OK) {
			if (ptn & 0x0008) {
				clr_flg(2, ~0x0008);
				inputProcess();
				if (sub_2AB46() == 0x0020)
					sub_18DD6();
				if (P4.0)
					P4.0 = 0;
				else
					P4.0 = 1;
			}
			if (ptn & 0x0004) {
				clr_flg(2, ~0x0004);
				sub_1C40A(inputKeys.events & 0xe3);
			}
			if (ptn & 0x0010) {
				clr_flg(2, ~0x0010);
				sub_18DDF();
			}
			if (ptn & 0x0040) {
				clr_flg(2, ~0x0040);
				sub_17890();
				sub_17B95(data);
			} else if (ptn & 0x0080) {
				clr_flg(2, ~0x0080);
				sub_1791D();
				sub_17C11(data);
			}
			if (ptn & 0x0100) {
				clr_flg(2, ~0x0100);
				sub_1875E();
			}
		}
		sub_2C48E();
	}
	ext_tsk();
}
