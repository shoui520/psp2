/* ARM status text shared by exception reporting and the debugger. */
#pragma once
#include <stdint.h>

static const char *const armPsrItEven[] = {
	"     ", "ITTTT", "ITTT ", "ITTTE", "ITT  ", "ITTET", "ITTE ", "ITTEE",
	"IT   ", "ITETT", "ITET ", "ITETE", "ITE  ", "ITEET", "ITEE ", "ITEEE",
};

static const char *const armPsrItOdd[] = {
	"     ", "ITEEE", "ITEE ", "ITEET", "ITE  ", "ITETE", "ITET ", "ITETT",
	"IT   ", "ITTEE", "ITTE ", "ITTET", "ITT  ", "ITTTE", "ITTT ", "ITTTT",
};

static const char *const armPsrConditions[] = {
	"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
	"HI", "LS", "GE", "LT", "GT", "LE", "AL", "  ",
};

static inline void armPsrCopyText(char *to, const char *from, unsigned count)
{
	while (count--)
		*to++ = *from++;
}

static inline __attribute__((always_inline))
void armFormatCpsr(uint32_t cpsr, char text[52])
{
	static const char *const modes[16] = {
		"Usr", "Fiq", "Irq", "Svc", "???", "???", "Mon", "Abt",
		"???", "???", "???", "Und", "???", "???", "???", "Sys",
	};
	char *p = text;
	unsigned i, it = (cpsr >> 25 & 3) | (cpsr >> 8 & 0xc);
	const char *state;

	for (i = 0; i < 5; i++)
		*p++ = cpsr & (1u << (31 - i)) ? "NZCVQ"[i] : "nzcvq"[i];
	*p++ = ' '; *p++ = 'G'; *p++ = 'E';
	for (i = 0; i < 4; i++)
		*p++ = cpsr & (1u << (19 - i)) ? '3' - i : '_';
	if (it & 0xf) {
		*p++ = ' ';
		armPsrCopyText(p, (cpsr & 0x1000 ? armPsrItOdd : armPsrItEven)[it & 15], 5);
		p += 5;
		*p++ = ' ';
		armPsrCopyText(p, armPsrConditions[cpsr >> 12 & 15], 2);
		p += 2;
	}
	*p++ = ' ';
	for (i = 0; i < 4; i++)
		*p++ = cpsr & (1u << (9 - i)) ? "EAIF"[i] : "eaif"[i];
	switch (cpsr & 0x1000020) {
	case 0: state = "ARM"; break;
	case 0x20: state = "Thumb"; break;
	case 0x1000000: state = "Jazelle"; break;
	default: state = "ThumbEE"; break;
	}
	*p++ = ' ';
	while (*state)
		*p++ = *state++;
	*p++ = ' ';
	state = (cpsr & 31) >= 16 ? modes[(cpsr & 31) - 16] : "???";
	armPsrCopyText(p, state, 3);
	p[3] = 0;
}
