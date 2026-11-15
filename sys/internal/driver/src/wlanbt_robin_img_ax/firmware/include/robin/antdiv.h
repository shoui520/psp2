/*
 * Antenna diversity (sqram/antdiv.c): while the radio configuration lets the
 * firmware choose the antennas (primaryDiversity), the supervision of a
 * peer's rate tries the other antenna now and then, and keeps it when the
 * beacons' SNR or the data rate got better. No Marvell source names it; the
 * names are inferred.
 */
#ifndef ROBIN_ANTDIV_H
#define ROBIN_ANTDIV_H

#include <stdint.h>

struct WlanConnection;

void antDivApply(void);							/* SQRAM 0xc000a290 */
void antDivSwitch(void);						/* SQRAM 0xc000a2b4 */
void antDivRevert(void);						/* SQRAM 0xc000a314 */
void antDivToggle(void);						/* SQRAM 0xc000a32a */
void antDivDecide(struct WlanConnection *connection);			/* SQRAM 0xc000a336 */
void antDivTick(void);							/* SQRAM 0xc000a3aa */
void antDivReset(void);							/* SQRAM 0xc000a3d4 */
void antDivStart(void);							/* SQRAM 0xc000a3f6 */
int antDivHoldOff(void);						/* SQRAM 0xc000a430 */
int antDivNotReady(struct WlanConnection *connection, int busy);	/* SQRAM 0xc000a444 */
unsigned int antDivStep(struct WlanConnection *connection, unsigned int rate, uint32_t frames,
			int busy);					/* SQRAM 0xc000a482 */
void antDivRestart(struct WlanConnection *connection);			/* SQRAM 0xc000a59e */
unsigned int antDivRecord(struct WlanConnection *connection);		/* SQRAM 0xc000a5ea */

#endif /* ROBIN_ANTDIV_H */
