/*
 * The Elmo, the power management IC: its register writes and reads over the IICA, the copy
 * of the registers the firmware writes, and the commands that set, clear or send the status
 * masks (registers 0xa0-0xa4) and register 0xa7. The updater's: each transfer waited for by
 * polling (iicaTransfer), once.
 */
#include <stddef.h>

#include "ernie.h"
#include "elmo.h"
#include "iica.h"
#include "powerseq.h"
#include "updater.h"

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
unsigned int elmoRegAB;
unsigned int elmoReg60;

static const struct ElmoRegInit __far elmoRegInit[] = {
	{ 0xa0, &elmoStatusMask.reg[0], 0 },
	{ 0xa1, &elmoStatusMask.reg[1], 0 },
	{ 0xa2, &elmoStatusMask.reg[2], 0 },
	{ 0xa3, &elmoStatusMask.reg[3], 0 },
	{ 0xa4, &elmoStatusMask.reg[4], 0 },
	{ 0xa5, &elmoLed, 0 },
	{ 0xa6, &elmoRegA6, 0 },
	{ 0xa7, &elmoRegA7, 0 },
	{ 0xaa, &elmoRegAA, 0 },
	{ 0xab, &elmoRegAB, 0 },
	{ 0x60, &elmoReg60, 0xfff0 },
};

/* sets every register of elmoRegInit to its initial value, and writes it after a reset */
void elmoInitRegs(void)
{
	unsigned char result;
	unsigned int i;
	unsigned char command[3];

	for (i = 0; i < 11; i++) {
		*elmoRegInit[i].value = elmoRegInit[i].initial;
		if (updReset != 0) {
			command[0] = elmoRegInit[i].reg;
			command[1] = ((unsigned char *)elmoRegInit[i].value)[1];
			command[2] = *(unsigned char *)elmoRegInit[i].value;
			result = elmoWrite(command, 3);
		}
	}
}

/* writes length bytes, a register number and its value; 0, or the transfer's status */
unsigned char elmoWrite(unsigned char *data, unsigned char length)
{
	struct IicaRequest request;
	unsigned char status;

	if (deviceVersions.elmo[0] == 0xff)
		return 0;
	request.rxData = NULL;
	request.rxCount = 0;
	request.txData = data;
	request.txCount = length;
	request.address = ELMO_ADDRESS;
	request.status = &status;
	iicaTransfer(&request);
	return status;
}

/* reads the register *reg into value[0-1]; 0, or the transfer's status */
unsigned char elmoRead(unsigned char *reg, unsigned char *value)
{
	struct IicaRequest request;
	unsigned char status;

	if (deviceVersions.elmo[0] == 0xff) {
		*value++ = 0;
		*value = 0;
		return 0;
	}
	request.rxData = value;
	request.rxCount = 2;
	request.txData = reg;
	request.txCount = 1;
	request.address = ELMO_ADDRESS;
	request.status = &status;
	iicaTransfer(&request);
	return status;
}

unsigned char elmoReadVersion(unsigned char *version)
{
	unsigned char reg;

	reg = ELMO_VERSION;
	return elmoRead(&reg, version);
}

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
	if (select & 0x10) {
		command[length++] = 0xa4;
		command[length++] = elmoStatusMask.byte[9];
		command[length++] = elmoStatusMask.byte[8];
	}
	return length;
}
