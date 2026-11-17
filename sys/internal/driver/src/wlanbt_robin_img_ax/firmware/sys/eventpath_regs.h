/*
 * Registers of the event paths, shared by sys/eventpower.c, sys/eventprimary.c
 * and sys/eventtertiary.c.
 */
#ifndef ROBIN_SYS_EVENTPATH_REGS_H
#define ROBIN_SYS_EVENTPATH_REGS_H

#include "robin/soc.h"

/* Registers of the event paths; the names say what the code does with them. */
#define EVENT_PATH_PENDING_TIMER	SOC_REG(0x040)
#define EVENT_PATH_PENDING_CHANNEL	SOC_REG(0x044)	/* bit 1: a path was entered */
#define EVENT_PATH_PENDING_PARAM	SOC_REG(0x048)
#define EVENT_PATH_PENDING_CONFIG	SOC_REG(0x04c)	/* bit 16: the engine runs */
#define EVENT_PATH_PENDING_CONTROL	SOC_REG(0x060)
#define EVENT_PATH_EVENT		SOC_REG(0x09c)
#define EVENT_PATH_CHANNEL		SOC_REG(0x130)
#define EVENT_PATH_STATUS		SOC_REG(0x140)
#define EVENT_PATH_BUS_CLOCK		SOC_REG(0x144)	/* low nibble: bus clock divider */
#define EVENT_PATH_COUNTER		SOC_REG(0x180)
#define EVENT_PATH_RATE			SOC_REG(0x188)
#define EVENT_PATH_KEY			SOC_REG(0x18c)
#define EVENT_PATH_COMMAND		SOC_REG(0x1a8)
#define EVENT_PATH_REGISTER		SOC_REG(0x200)
#define EVENT_PATH_RECORD		SOC_REG(0x204)
#define EVENT_PATH_FRAME		SOC_REG(0x250)
#define EVENT_PATH_SECONDARY_CONFIGURATION	SOC_REG(0x4c4)	/* SOC_EVENT_SECONDARY_CONFIGURATION, by its fixed address */
#define EVENT_PATH_CONTROL		SOC_CONTROL		/* bit 3 */
#define EVENT_PATH_MEASURE		SOC_SLEEP_MEASURE_CONTROL	/* bit 7 */

#define SOC_IRQ_PRIMARY_EVENT		0x200000
#define SOC_IRQ_SECONDARY_EVENT		0x8000000
#define SOC_IRQ_MIDDLE_EVENT		0x100

#endif /* ROBIN_SYS_EVENTPATH_REGS_H */
