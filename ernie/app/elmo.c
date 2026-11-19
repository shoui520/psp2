/*
 * The Elmo, the power management IC: its register writes and reads over the IICA, the copy
 * of the registers the firmware writes, and the commands that set, clear or send the status
 * masks (registers 0xa0-0xa4) and register 0xa7.
 */
#pragma sfr

#include <stddef.h>

#include "ernie.h"
#include "elmo.h"
#include "iica.h"
#include "kernel.h"
#include "log.h"
#include "powerseq.h"
#include "powerwait.h"
#include "state.h"

#define ELMO_TRIES	3		/* a transfer's tries */
#if BOARD_DOL1001
#define ELMO_TIMEOUT	1		/* the wait for a transfer's end, in ticks */
#else
#define ELMO_TIMEOUT	0x4b
#endif
#define ELMO_DONE	0x0010		/* event flag 4's bit for a transfer's end */
#define ELMO_NO_ANSWER	0x10		/* the status when the transfer did not end */

/* a register written at a power on, and its value */
struct ElmoRegInit {
	unsigned char	reg;
	unsigned int	*value;
	unsigned int	initial;
};



union ElmoStatusMask elmoStatusMask;
unsigned int elmoLed;
unsigned int elmoRegA6;
unsigned int elmoRegA7;
unsigned int elmoRegAA;
#if FW_SINCE(0x00090009)
unsigned int elmoRegAB;
#endif
#if FW_SINCE(0x00090804)
unsigned int elmoReg60;
#endif
#if !FW_SINCE(0x01000104)
unsigned char elmoPowerOnStage;
#endif

#if FW_SINCE(0x00090804)
static const struct ElmoRegInit elmoRegInit[] = {
	{ 0xa0, &elmoStatusMask.reg[0], 0 },
	{ 0xa1, &elmoStatusMask.reg[1], 0 },
	{ 0xa2, &elmoStatusMask.reg[2], 0 },
	{ 0xa3, &elmoStatusMask.reg[3], 0 },
#if !BOARD_DOL1002
	{ 0xa4, &elmoStatusMask.reg[4], 0 },
#endif
	{ 0xa5, &elmoLed, 0 },
	{ 0xa6, &elmoRegA6, 0 },
	{ 0xa7, &elmoRegA7, 0 },
	{ 0xaa, &elmoRegAA, 0 },
	{ 0xab, &elmoRegAB, 0 },
	{ 0x60, &elmoReg60, 0xfff0 },
};
#endif

#if BOARD_DOL1002
/* DOL-1002's: elmoWrite and elmoRead do nothing while the Elmo's version is unread (0), 0xfd or
   absent (0xff); the register setup and the version read go to the bus directly */
#define ELMO_NO_TRANSFER	(deviceVersions.elmo[0] == 0 || deviceVersions.elmo[0] == 0xfd || \
				 deviceVersions.elmo[0] == 0xff)
#define ELMO_TABLE_COUNT	10
static unsigned char elmoWriteBus(unsigned char *data, unsigned char length);
static unsigned char elmoReadBus(unsigned char *reg, unsigned char *value);
#define elmoInitWrite		elmoWriteBus
#define elmoVersionRead		elmoReadBus
#else
#define ELMO_TABLE_COUNT	11
#define elmoInitWrite		elmoWrite
#define elmoVersionRead		elmoRead
#endif

#if SOURCE_0103
/* the slim's: nothing to set up before the versions are read */
void elmoInit(void)
{
}

/* writes every register of elmoRegInit with its initial value, after the Elmo's version
   answered (powerSeqOn); the copy keeps the value only when the write went through */
void elmoInitRegs(void)
{
#if BOARD_DOL1002
	unsigned char i;
#else
	unsigned int i;
#endif

	for (i = 0; i < ELMO_TABLE_COUNT; i++) {
		elmoCommand[0] = elmoRegInit[i].reg;
		elmoCommand[1] = elmoRegInit[i].initial >> 8;
#if BOARD_DOL1002
		elmoCommand[2] = *(const unsigned char __far *)&elmoRegInit[i].initial;
#else
		elmoCommand[2] = elmoRegInit[i].initial;
#endif
		if (elmoInitWrite(elmoCommand, 3) == 0)
			*elmoRegInit[i].value = elmoRegInit[i].initial;
	}
}
#else
/* writes every register of elmoRegInit with its initial value */
void elmoInitRegs(void)
{
#if !FW_SINCE(0x00090804)
	elmoStatusMask.reg[0] = 0;
	elmoStatusMask.reg[1] = 0;
	elmoStatusMask.reg[2] = 0;
	elmoStatusMask.reg[3] = 0;
	elmoStatusMask.reg[4] = 0;
	elmoLed = 0;
	elmoRegA6 = 0;
	elmoRegA7 = 0;
	elmoRegAA = 0;
#if FW_SINCE(0x00090009)	/* 0x00080504 has no multi connector power */
	elmoRegAB = 0;
#endif
#else
#if !FW_SINCE(0x0100060B)
	unsigned char result;
#endif
	unsigned int i;

	for (i = 0; i < 11; i++) {
		*elmoRegInit[i].value = elmoRegInit[i].initial;
		elmoCommand[0] = elmoRegInit[i].reg;
		elmoCommand[1] = ((unsigned char *)elmoRegInit[i].value)[1];
		elmoCommand[2] = *(unsigned char *)elmoRegInit[i].value;
#if FW_SINCE(0x0100060B)
		elmoWrite(elmoCommand, 3);
#else
		result = elmoWrite(elmoCommand, 3);
#endif
	}
#endif
}
#endif /* SOURCE_0103 */

#if BOARD_DOL1002
unsigned char elmoWrite(unsigned char *data, unsigned char length)
{
	if (ELMO_NO_TRANSFER)
		return 0;
	return elmoWriteBus(data, length);
}

/* writes length bytes, a register number and its value; 0, or the transfer's status, which a
   failed transfer logs (events 0x21 and 0x22) */
static unsigned char elmoWriteBus(unsigned char *data, unsigned char length)
#else
/* writes length bytes, a register number and its value; 0, or the transfer's status */
unsigned char elmoWrite(unsigned char *data, unsigned char length)
#endif
{
	ER ercd;
	FLGPTN ptn;
	struct IicaRequest request;
	unsigned char status;
	unsigned char tries;

	tries = ELMO_TRIES - 1;
#if !BOARD_DOL1002
	if (deviceVersions.elmo[0] == 0xff)
		return 0;
#endif
	do {
		request.rxData = NULL;
		request.rxCount = 0;
		request.txData = data;
		request.txCount = length;
		request.address = ELMO_ADDRESS;
		request.status = &status;
#if FW_SINCE(0x0100060B)
		clr_flg(4, ~ELMO_DONE);
#endif
		iicaStart(&request);
		ercd = twai_flg(4, ELMO_DONE, TWF_ORW, &ptn, ELMO_TIMEOUT);
		if (ercd == E_OK) {
			if (ptn & ELMO_DONE) {
				clr_flg(4, ~ELMO_DONE);
				break;
			}
		} else if (ercd == E_TMOUT)
			status = ELMO_NO_ANSWER;
	} while (tries-- != 0);
#if BOARD_DOL1002
	if (status != 0) {
		logEvent(0x21);
		logEventValue(0x22, status);
	}
#endif
	return status;
}

#if BOARD_DOL1002
unsigned char elmoRead(unsigned char *reg, unsigned char *value)
{
	if (ELMO_NO_TRANSFER) {
		*value++ = 0;
		*value = 0;
		return 0;
	}
	return elmoReadBus(reg, value);
}

/* reads the register *reg into value[0-1]; 0, or the transfer's status, logged as elmoWriteBus's */
static unsigned char elmoReadBus(unsigned char *reg, unsigned char *value)
#else
/* reads the register *reg into value[0-1]; 0, or the transfer's status */
unsigned char elmoRead(unsigned char *reg, unsigned char *value)
#endif
{
	ER ercd;
	FLGPTN ptn;
	struct IicaRequest request;
	unsigned char status;
	unsigned char tries;

	tries = ELMO_TRIES - 1;
#if !BOARD_DOL1002
	if (deviceVersions.elmo[0] == 0xff) {
		*value++ = 0;
		*value = 0;
		return 0;
	}
#endif
	do {
		request.rxData = value;
		request.rxCount = 2;
		request.txData = reg;
		request.txCount = 1;
		request.address = ELMO_ADDRESS;
		request.status = &status;
#if FW_SINCE(0x0100060B)
		clr_flg(4, ~ELMO_DONE);
#endif
		iicaStart(&request);
		ercd = twai_flg(4, ELMO_DONE, TWF_ORW, &ptn, ELMO_TIMEOUT);
		if (ercd == E_OK) {
			if (ptn & ELMO_DONE) {
				clr_flg(4, ~ELMO_DONE);
				break;
			}
		} else if (ercd == E_TMOUT)
			status = ELMO_NO_ANSWER;
	} while (tries-- != 0);
#if BOARD_DOL1002
	if (status != 0) {
		logEvent(0x21);
		logEventValue(0x22, status);
	}
#endif
	return status;
}

#if SOURCE_0103
/* the version's two bytes, as the slim's code reaches them */
struct ElmoVersion {
	unsigned char	low;
	unsigned char	high;
};
#define ELMO_VERSION_OF(version)	((struct ElmoVersion *)(version))
#endif

unsigned char elmoReadVersion(unsigned char *version)
{
	unsigned char reg;

#if SOURCE_0103 && !BOARD_DOL1002
	/* the slim's: not again once the Elmo did not answer */
	if (deviceVersions.elmo[0] == 0xff) {
		ELMO_VERSION_OF(version)->low = ELMO_VERSION_OF(version)->high = 0xff;
		return 0;
	}
#endif
	reg = ELMO_VERSION;
	return elmoVersionRead(&reg, version);
}

#if SOURCE_0103
/* the 0x0103 source's: sets (set) or clears masks[0-3] (the PS TV's: [0-4]) in the status masks, and clears or sets
   masks->regA7 in register 0xa7, in one write of elmoCommand; the copies change only when the
   write went through. 0, or elmoWrite's error */
/* the status masks as the slim's code reaches them: a register each */
#define ELMO_STATUS	((struct ElmoMasks *)&elmoStatusMask)

/* value[]'s entry of register 0xa7 (DOL-1002: its 0xaa's after it) */
#if BOARD_DOL1002
#define ELMO_VALUE_A7	4
#define ELMO_VALUE_AA	5
#else
#define ELMO_VALUE_A7	5
#endif

unsigned char elmoUpdateMasks(struct ElmoMasks *masks, unsigned char set)
{
	unsigned char length;
	unsigned char result;
	unsigned int value[6];

	length = 0;
	result = 0;
	if (masks->status0 != 0) {
		value[0] = set ? ELMO_STATUS->status0 | masks->status0 :
			~masks->status0 & ELMO_STATUS->status0;
#if BOARD_DOL1001 && !BOARD_DOL1002
		/* the PS TV's: a clear keeps bit 2 in system states 8 and 0x8000 while status
		   mask 2's bit 12 is set */
		if (!set && (masks->status0 & 0x04) && (stateGet() & 0x8008) &&
		    (ELMO_STATUS->status2 & 0x1000))
			value[0] |= 0x04;
#endif
		elmoCommand[length++] = 0xa0;
		elmoCommand[length++] = ((unsigned char *)&value[0])[1];
		elmoCommand[length++] = (unsigned char)value[0];
	}
	if (masks->status1 != 0) {
		value[1] = set ? ELMO_STATUS->status1 | masks->status1 :
			~masks->status1 & ELMO_STATUS->status1;
		elmoCommand[length++] = 0xa1;
		elmoCommand[length++] = ((unsigned char *)&value[1])[1];
		elmoCommand[length++] = (unsigned char)value[1];
	}
	if (masks->status2 != 0) {
		value[2] = set ? ELMO_STATUS->status2 | masks->status2 :
			~masks->status2 & ELMO_STATUS->status2;
		elmoCommand[length++] = 0xa2;
		elmoCommand[length++] = ((unsigned char *)&value[2])[1];
		elmoCommand[length++] = (unsigned char)value[2];
	}
	if (masks->status3 != 0) {
		value[3] = set ? ELMO_STATUS->status3 | masks->status3 :
			~masks->status3 & ELMO_STATUS->status3;
		elmoCommand[length++] = 0xa3;
		elmoCommand[length++] = ((unsigned char *)&value[3])[1];
		elmoCommand[length++] = (unsigned char)value[3];
	}
#if !BOARD_USS1001 && !BOARD_DOL1002	/* the PS TV's: register 0xa4 too */
	if (masks->status4 != 0) {
		value[4] = set ? ELMO_STATUS->status4 | masks->status4 :
			~masks->status4 & ELMO_STATUS->status4;
		elmoCommand[length++] = 0xa4;
		elmoCommand[length++] = ((unsigned char *)&value[4])[1];
		elmoCommand[length++] = (unsigned char)value[4];
	}
#endif
	if (masks->regA7 != 0) {
		value[ELMO_VALUE_A7] = set ? ~masks->regA7 & elmoRegA7 : elmoRegA7 | masks->regA7;
		elmoCommand[length++] = 0xa7;
		elmoCommand[length++] = ((unsigned char *)&value[ELMO_VALUE_A7])[1];
		elmoCommand[length++] = (unsigned char)value[ELMO_VALUE_A7];
	}
#if BOARD_DOL1002
	if (masks->regAA != 0) {
		value[ELMO_VALUE_AA] = set ? elmoRegAA | masks->regAA : ~masks->regAA & elmoRegAA;
		elmoCommand[length++] = 0xaa;
		elmoCommand[length++] = ((unsigned char *)&value[ELMO_VALUE_AA])[1];
		elmoCommand[length++] = (unsigned char)value[ELMO_VALUE_AA];
	}
#endif
	if (length != 0) {
#if BOARD_DOL1002
		/* DOL-1002's: status mask 3's bit 12 lowers P0.1 before a clear; a set writes polled,
		   with dispatching disabled, and raises P0.1 5 ms after it went through */
		if (!set && (masks->status3 & 0x1000))
			P0.1 = 0;
		if (set && (masks->status3 & 0x1000)) {
			dis_dsp();
			result = elmoWritePolled(elmoCommand, length);
			if (result == 0) {
				powerWaitMsPolled(5);
				P0.1 = 1;
			}
			ena_dsp();
		} else
#endif
		result = elmoWrite(elmoCommand, length);
		if (result == 0) {
			if (masks->status0 != 0)
				ELMO_STATUS->status0 = value[0];
			if (masks->status1 != 0)
				ELMO_STATUS->status1 = value[1];
			if (masks->status2 != 0)
				ELMO_STATUS->status2 = value[2];
			if (masks->status3 != 0)
				ELMO_STATUS->status3 = value[3];
#if !BOARD_USS1001 && !BOARD_DOL1002
			if (masks->status4 != 0)
				ELMO_STATUS->status4 = value[4];
#endif
			if (masks->regA7 != 0)
				elmoRegA7 = value[ELMO_VALUE_A7];
#if BOARD_DOL1002
			if (masks->regAA != 0)
				elmoRegAA = value[ELMO_VALUE_AA];
#endif
		}
	}
	return result;
}
#else
/* sets masks[0-4] in the status masks and clears masks->regA7 in register 0xa7, and writes the
   command for each register changed into command; its length */
unsigned char elmoSetMasks(struct ElmoMasks *masks, unsigned char *command)
{
	unsigned char length;

	length = 0;
	if (masks->status0 != 0) {
		elmoStatusMask.reg[0] |= masks->status0;
		command[length++] = 0xa0;
		command[length++] = elmoStatusMask.byte[1];
		command[length++] = elmoStatusMask.byte[0];
	}
	if (masks->status1 != 0) {
		elmoStatusMask.reg[1] |= masks->status1;
		command[length++] = 0xa1;
		command[length++] = elmoStatusMask.byte[3];
		command[length++] = elmoStatusMask.byte[2];
	}
	if (masks->status2 != 0) {
		elmoStatusMask.reg[2] |= masks->status2;
		command[length++] = 0xa2;
		command[length++] = elmoStatusMask.byte[5];
		command[length++] = elmoStatusMask.byte[4];
	}
	if (masks->status3 != 0) {
		elmoStatusMask.reg[3] |= masks->status3;
		command[length++] = 0xa3;
		command[length++] = elmoStatusMask.byte[7];
		command[length++] = elmoStatusMask.byte[6];
	}
	if (masks->status4 != 0) {
		elmoStatusMask.reg[4] |= masks->status4;
		command[length++] = 0xa4;
		command[length++] = elmoStatusMask.byte[9];
		command[length++] = elmoStatusMask.byte[8];
	}
	if (masks->regA7 != 0) {
		elmoRegA7 &= ~masks->regA7;
		command[length++] = 0xa7;
		command[length++] = ((unsigned char *)&elmoRegA7)[1];
		command[length++] = *(unsigned char *)&elmoRegA7;
	}
	return length;
}

/* clears masks[0-4] in the status masks and sets masks->regA7 in register 0xa7, as
   elmoSetMasks */
unsigned char elmoClearMasks(struct ElmoMasks *masks, unsigned char *command)
{
	unsigned char length;

	length = 0;
	if (masks->status0 != 0) {
		elmoStatusMask.reg[0] &= ~masks->status0;
		if ((masks->status0 & 0x0004) && (stateGet() & 0x8008) &&
		    (elmoStatusMask.reg[2] & 0x1000))
			elmoStatusMask.reg[0] |= 0x0004;
		command[length++] = 0xa0;
		command[length++] = elmoStatusMask.byte[1];
		command[length++] = elmoStatusMask.byte[0];
	}
	if (masks->status1 != 0) {
		elmoStatusMask.reg[1] &= ~masks->status1;
		command[length++] = 0xa1;
		command[length++] = elmoStatusMask.byte[3];
		command[length++] = elmoStatusMask.byte[2];
	}
	if (masks->status2 != 0) {
		elmoStatusMask.reg[2] &= ~masks->status2;
		command[length++] = 0xa2;
		command[length++] = elmoStatusMask.byte[5];
		command[length++] = elmoStatusMask.byte[4];
	}
	if (masks->status3 != 0) {
		elmoStatusMask.reg[3] &= ~masks->status3;
		command[length++] = 0xa3;
		command[length++] = elmoStatusMask.byte[7];
		command[length++] = elmoStatusMask.byte[6];
	}
	if (masks->status4 != 0) {
		elmoStatusMask.reg[4] &= ~masks->status4;
		command[length++] = 0xa4;
		command[length++] = elmoStatusMask.byte[9];
		command[length++] = elmoStatusMask.byte[8];
	}
	if (masks->regA7 != 0) {
		elmoRegA7 |= masks->regA7;
		command[length++] = 0xa7;
		command[length++] = ((unsigned char *)&elmoRegA7)[1];
		command[length++] = *(unsigned char *)&elmoRegA7;
	}
	return length;
}

#endif /* SOURCE_0103 */

/* writes the command for each status mask select's bits 0-4 name into command; its length */
unsigned char elmoGetMasks(unsigned char select, unsigned char *command)
{
	unsigned char length;

	length = 0;
	if (select & 0x01) {
		command[length++] = 0xa0;
		command[length++] = elmoStatusMask.byte[1];
		command[length++] = elmoStatusMask.byte[0];
	}
	if (select & 0x02) {
		command[length++] = 0xa1;
		command[length++] = elmoStatusMask.byte[3];
		command[length++] = elmoStatusMask.byte[2];
	}
	if (select & 0x04) {
		command[length++] = 0xa2;
		command[length++] = elmoStatusMask.byte[5];
		command[length++] = elmoStatusMask.byte[4];
	}
	if (select & 0x08) {
		command[length++] = 0xa3;
		command[length++] = elmoStatusMask.byte[7];
		command[length++] = elmoStatusMask.byte[6];
	}
#if !BOARD_USS1001 && !BOARD_DOL1002	/* the slim's and DOL-1002's leave register 0xa4 out */
	if (select & 0x10) {
		command[length++] = 0xa4;
		command[length++] = elmoStatusMask.byte[9];
		command[length++] = elmoStatusMask.byte[8];
	}
#endif
	return length;
}
