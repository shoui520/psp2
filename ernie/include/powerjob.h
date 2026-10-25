/*
 * Task 5's part of the power sequences (app/powerjob.c): the jobs a sequence posts
 * (powerJobPostOn, ..., task5.c) and task 5 runs (powerJob).
 */
#ifndef ERNIE_POWERJOB_H
#define ERNIE_POWERJOB_H

#define POWER_JOB_ON		1	/* powerOn */
#define POWER_JOB_RESUME	2	/* powerResume */
#define POWER_JOB_OFF		3	/* powerOff */
#define POWER_JOB_SUSPEND	4	/* powerSuspend */
#define POWER_JOB_SHUTDOWN	5	/* powerShutdown */
#define POWER_JOB_CUT		6	/* powerCut */
#define POWER_JOB_OFF_END	7	/* powerOff, when saddr_FFE70 is 0x56 */
#define POWER_JOB_UPDATE	8	/* powerUpdate */
#define POWER_JOB_RESET		9	/* powerReset */

extern unsigned char powerJobId;
extern unsigned char powerJobFlag;

extern void powerJob(void);

#endif
