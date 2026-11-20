/*
 * The handlers of the input groups' bits (input.c's inputGroupUpdate dispatches them): each
 * bit's list of handlers and its length, and for each group the table of its bits.
 */
#include <stddef.h>

#include "bbmc.h"
#include "board.h"
#include "input.h"
#include "inputbits.h"
#include "powerreq.h"

#if !SOURCE_0103	/* the slim's group 1 bit 1 and group 3 bit 2 have no handlers */
/* nothing else refers to it, so its owner is not known: type 0, never called; the program's
   one initialised variable */
struct InputHandler inputHandler1Bit1 = { 0, 0, 0 };
#endif

#define LENGTH(list)	(sizeof(list) / sizeof((list)[0]))

static struct InputHandler * const inputHandlers1Bit4[] = { &inputHandlerWake2 };
#if !FW_SINCE(0x0100060B)
static struct InputHandler * const inputHandlers1Bit2[] = {
	&inputHandler1Bit2Low, &inputHandler1Bit2High
};
#endif
#if !SOURCE_0103
static struct InputHandler * const inputHandlers1Bit1[] = { &inputHandler1Bit1 };
#endif
static struct InputHandler * const inputHandlers1Bit5[] = {
	&inputHandlerSerial1, &inputHandlerMultiCn
};
static struct InputHandler * const inputHandlers1Bit6[] = { &inputHandlerWake1 };
#if FW_SINCE(0x00090804)
static struct InputHandler * const inputHandlers1Bit7[] = {
	&inputHandlerCharge, &inputHandler1Bit7
};
#else
static struct InputHandler * const inputHandlers1Bit7[] = { &inputHandlerCharge };
#endif
static struct InputHandler * const inputHandlers1Bit3[] = { &inputHandler1Bit3 };
static struct InputHandler * const inputHandlers3Bit0[] = { &inputHandlerSerial2Request };
static struct InputHandler * const inputHandlers3Bit1[] = { &inputHandlerSerial2Link };
#if !SOURCE_0103
static struct InputHandler * const inputHandlers3Bit2[] = { &inputHandler3Bit2 };
#endif
#if INPUT_BITS2_HANDLERS
static struct InputHandler * const inputHandlers2Bit0[] = {
	&powerInputHandler, &bbmcInputHandlerPower
};
static struct InputHandler * const inputHandlers2Bit1[] = { &bbmcInputHandlerWake };
#endif
#if FW_SINCE(0x00090009)	/* 0x00080504 has no input 1 bit 0 handler */
static struct InputHandler * const inputHandlers1Bit0[] = { &inputHandler1Bit0 };
#endif

static unsigned char inputHandlers1Bit6Count(void);
static unsigned char inputHandlers1Bit7Count(void);
static unsigned char inputHandlers1Bit4Count(void);
#if !FW_SINCE(0x0100060B)
static unsigned char inputHandlers1Bit2Count(void);
#endif
#if !SOURCE_0103
static unsigned char inputHandlers1Bit1Count(void);
#endif
static unsigned char inputHandlers1Bit5Count(void);
static unsigned char inputHandlers1Bit3Count(void);
static unsigned char inputHandlers3Bit0Count(void);
static unsigned char inputHandlers3Bit1Count(void);
#if !SOURCE_0103
static unsigned char inputHandlers3Bit2Count(void);
#endif
#if INPUT_BITS2_HANDLERS
static unsigned char inputHandlers2Bit0Count(void);
static unsigned char inputHandlers2Bit1Count(void);
#endif
#if FW_SINCE(0x00090009)
static unsigned char inputHandlers1Bit0Count(void);
#endif

/* the second bit of group 1 is not sampled */
const struct InputBit inputBits1[8] = {
#if FW_SINCE(0x00090009)
	{ inputHandlers1Bit0, inputHandlers1Bit0Count },
#else
	{ NULL, NULL },
#endif
#if SOURCE_0103
	{ NULL, NULL },
#else
	{ inputHandlers1Bit1, inputHandlers1Bit1Count },
#endif
#if FW_SINCE(0x0100060B)
	{ NULL, NULL },
#else
	{ inputHandlers1Bit2, inputHandlers1Bit2Count },
#endif
	{ inputHandlers1Bit3, inputHandlers1Bit3Count },
	{ inputHandlers1Bit4, inputHandlers1Bit4Count },
	{ inputHandlers1Bit5, inputHandlers1Bit5Count },
	{ inputHandlers1Bit6, inputHandlers1Bit6Count },
	{ inputHandlers1Bit7, inputHandlers1Bit7Count },
};

const struct InputBit inputBits2[INPUT_BITS2] = {
#if INPUT_BITS2_HANDLERS
	{ inputHandlers2Bit0, inputHandlers2Bit0Count },
	{ inputHandlers2Bit1, inputHandlers2Bit1Count },
#else
	{ NULL, NULL },
	{ NULL, NULL },
#endif
	{ NULL, NULL },
	{ NULL, NULL },
#if INPUT_BITS2 == 6
	{ NULL, NULL },
	{ NULL, NULL },
#endif
};

const struct InputBit inputBits3[INPUT_BITS3] = {
	{ inputHandlers3Bit0, inputHandlers3Bit0Count },
	{ inputHandlers3Bit1, inputHandlers3Bit1Count },
#if INPUT_BITS3 == 2
};
#else
#if SOURCE_0103
	{ NULL, NULL },
#else
	{ inputHandlers3Bit2, inputHandlers3Bit2Count },
#endif
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
};
#endif

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

#if !FW_SINCE(0x0100060B)
static unsigned char inputHandlers1Bit2Count(void)
{
	return LENGTH(inputHandlers1Bit2);
}
#endif

#if !SOURCE_0103
static unsigned char inputHandlers1Bit1Count(void)
{
	return LENGTH(inputHandlers1Bit1);
}
#endif

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

#if !SOURCE_0103
static unsigned char inputHandlers3Bit2Count(void)
{
	return LENGTH(inputHandlers3Bit2);
}
#endif

#if INPUT_BITS2_HANDLERS
static unsigned char inputHandlers2Bit0Count(void)
{
	return LENGTH(inputHandlers2Bit0);
}

static unsigned char inputHandlers2Bit1Count(void)
{
	return LENGTH(inputHandlers2Bit1);
}
#endif

#if FW_SINCE(0x00090009)
static unsigned char inputHandlers1Bit0Count(void)
{
	return LENGTH(inputHandlers1Bit0);
}
#endif

