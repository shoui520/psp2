/*
 * The handlers of the input groups' bits (input.c's inputGroupUpdate dispatches them): each
 * bit's list of handlers and its length, and for each group the table of its bits.
 */
#include <stddef.h>

#include "input.h"
#include "inputbits.h"
#include "powerreq.h"

/* set up by modules not written yet; inputHandlerFE16C never is (nothing else refers to it, so
   its owner is not known): type 0, never called */
extern struct InputHandler inputHandlerFE16C;
extern struct InputHandler inputHandler27446;
extern struct InputHandler inputHandler27452;

#define LENGTH(list)	(sizeof(list) / sizeof((list)[0]))

static struct InputHandler * const inputHandlers1Bit4[] = { &inputHandler2B475 };
static struct InputHandler * const inputHandlers1Bit2[] = {
	&inputHandler1245B, &inputHandler1245C
};
static struct InputHandler * const inputHandlers1Bit1[] = { &inputHandlerFE16C };
static struct InputHandler * const inputHandlers1Bit5[] = {
	&inputHandlerSerial1, &inputHandler1245D
};
static struct InputHandler * const inputHandlers1Bit6[] = { &inputHandler2B4BC };
static struct InputHandler * const inputHandlers1Bit7[] = {
	&inputHandler29B68, &inputHandler1275F
};
static struct InputHandler * const inputHandlers1Bit3[] = { &inputHandler12698 };
static struct InputHandler * const inputHandlers3Bit0[] = { &inputHandler25263 };
static struct InputHandler * const inputHandlers3Bit1[] = { &inputHandler2526B };
static struct InputHandler * const inputHandlers3Bit2[] = { &inputHandler224FD };
static struct InputHandler * const inputHandlers2Bit0[] = {
	&powerInputHandler, &inputHandler27446
};
static struct InputHandler * const inputHandlers2Bit1[] = { &inputHandler27452 };
static struct InputHandler * const inputHandlers1Bit0[] = { &inputHandler12489 };

static unsigned char inputHandlers1Bit6Count(void);
static unsigned char inputHandlers1Bit7Count(void);
static unsigned char inputHandlers1Bit4Count(void);
static unsigned char inputHandlers1Bit2Count(void);
static unsigned char inputHandlers1Bit1Count(void);
static unsigned char inputHandlers1Bit5Count(void);
static unsigned char inputHandlers1Bit3Count(void);
static unsigned char inputHandlers3Bit0Count(void);
static unsigned char inputHandlers3Bit1Count(void);
static unsigned char inputHandlers3Bit2Count(void);
static unsigned char inputHandlers2Bit0Count(void);
static unsigned char inputHandlers2Bit1Count(void);
static unsigned char inputHandlers1Bit0Count(void);

/* the second bit of group 1 is not sampled */
const struct InputBit inputBits1[8] = {
	{ inputHandlers1Bit0, inputHandlers1Bit0Count },
	{ inputHandlers1Bit1, inputHandlers1Bit1Count },
	{ inputHandlers1Bit2, inputHandlers1Bit2Count },
	{ inputHandlers1Bit3, inputHandlers1Bit3Count },
	{ inputHandlers1Bit4, inputHandlers1Bit4Count },
	{ inputHandlers1Bit5, inputHandlers1Bit5Count },
	{ inputHandlers1Bit6, inputHandlers1Bit6Count },
	{ inputHandlers1Bit7, inputHandlers1Bit7Count },
};

const struct InputBit inputBits2[6] = {
	{ inputHandlers2Bit0, inputHandlers2Bit0Count },
	{ inputHandlers2Bit1, inputHandlers2Bit1Count },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
};

const struct InputBit inputBits3[6] = {
	{ inputHandlers3Bit0, inputHandlers3Bit0Count },
	{ inputHandlers3Bit1, inputHandlers3Bit1Count },
	{ inputHandlers3Bit2, inputHandlers3Bit2Count },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
};

unsigned char inputBits1Count(void)
{
	return LENGTH(inputBits1);
}

unsigned char inputBits2Count(void)
{
	return LENGTH(inputBits2);
}

unsigned char inputBits3Count(void)
{
	return LENGTH(inputBits3);
}

static unsigned char inputHandlers1Bit6Count(void)
{
	return LENGTH(inputHandlers1Bit6);
}

static unsigned char inputHandlers1Bit7Count(void)
{
	return LENGTH(inputHandlers1Bit7);
}

static unsigned char inputHandlers1Bit4Count(void)
{
	return LENGTH(inputHandlers1Bit4);
}

static unsigned char inputHandlers1Bit2Count(void)
{
	return LENGTH(inputHandlers1Bit2);
}

static unsigned char inputHandlers1Bit1Count(void)
{
	return LENGTH(inputHandlers1Bit1);
}

static unsigned char inputHandlers1Bit5Count(void)
{
	return LENGTH(inputHandlers1Bit5);
}

static unsigned char inputHandlers1Bit3Count(void)
{
	return LENGTH(inputHandlers1Bit3);
}

static unsigned char inputHandlers3Bit0Count(void)
{
	return LENGTH(inputHandlers3Bit0);
}

static unsigned char inputHandlers3Bit1Count(void)
{
	return LENGTH(inputHandlers3Bit1);
}

static unsigned char inputHandlers3Bit2Count(void)
{
	return LENGTH(inputHandlers3Bit2);
}

static unsigned char inputHandlers2Bit0Count(void)
{
	return LENGTH(inputHandlers2Bit0);
}

static unsigned char inputHandlers2Bit1Count(void)
{
	return LENGTH(inputHandlers2Bit1);
}

static unsigned char inputHandlers1Bit0Count(void)
{
	return LENGTH(inputHandlers1Bit0);
}
