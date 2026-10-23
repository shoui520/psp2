/*
 * SCE: the psp2 port's interrupt priority levels. The stack runs in net_ps's own threads
 * under its lock, so raising and restoring a level does nothing (callout_reset, softclock).
 */
#ifndef _PSP2_INTR_H_
#define _PSP2_INTR_H_

#define splnet()	0
#define splsoftnet()	0
#define splsoftclock()	0
#define splclock()	0
#define splstatclock()	0
#define splsched()	0
#define splhigh()	0
#define splvm()		0
#define splbio()	0
#define spltty()	0
#define splserial()	0
#define spl0()		0
#define splx(s)		((void)(s))

#endif /* _PSP2_INTR_H_ */
