/* Power management seen from the idle thread (provisional names). */
#ifndef ROBIN_POWER_H
#define ROBIN_POWER_H

extern void *btSdioDev;

/* 0x00023ac0: run with interrupts off, once, before the idle loop. */
int powerIdlePrepare(void);
/* 0x00023ab0: one pass of the sleep decision; may enter the low-power state. */
unsigned int powerIdleStep(void);

#endif /* ROBIN_POWER_H */
