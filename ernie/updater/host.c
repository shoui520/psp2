/*
 * The host link in the updater: Kermit's frames over CSI00, polled. Each frame starts with
 * the command and the status byte exchanged, then the length, then the data and checksum. A
 * command whose bit 7 is clear is answered in the same frame (hostCommandFast); one with bit
 * 7 set is queued (hostSlowCommands): its first frame answers 0x80, it runs from the main
 * loop (hostSlowRun), and a later frame with the same command takes its answer. The log
 * goes out on UART3, through the multi connector.
 */
#pragma sfr
#pragma DI
#pragma EI
#pragma NOP

#include <stdarg.h>
#include <string.h>
#include "chip.h"
#include "ernie.h"
#include "board.h"
#include "sleepfactor.h"
#include "updater.h"

/* hostSlowCommand's states */
#define HOST_SLOW_QUEUED	1	/* runs from the main loop */
#define HOST_SLOW_DONE		2	/* its answer is ready */

/* the frame's status byte */
#define HOST_STATUS_CHECKSUM	0x02	/* the last command's checksum was wrong */
#define HOST_STATUS_ALWAYS	0x48
#define HOST_STATUS_SLOW	0x20	/* slow commands are queued */

/* the answers' result bytes the link gives itself */
#define HOST_RESULT_QUEUED	0x80	/* the slow command was queued */
#define HOST_RESULT_RUNNING	0x81	/* and is not done yet */
#define HOST_RESULT_FULL	0x82	/* no free queue entry */
#define HOST_RESULT_UNKNOWN	0x3f

#define HOST_SLOW_COMMANDS	3

struct UpdHostLink hostLink;
struct HostCommandFrame hostRx;
struct HostAnswerFrame hostTx;
struct HostSlowCommand hostSlowCommands[HOST_SLOW_COMMANDS];
char logBuffer[0x86];

int hostOpen(void)
{
	hostLink.statusBit1 = 0;
	memset(hostSlowCommands, 0, sizeof(hostSlowCommands));
	hostLink.statusBit5 = 0;
	hostLink.resetWait = 0x150;
	csiOpen();
	return 0;
}

int hostClose(void)
{
	P5.6 = 0;
	P5.7 = 0;
	csiClose();
	return 0;
}

/* a frame, while the host holds P1.6 */
unsigned char hostPoll(void)
{
	unsigned char size;
	unsigned char queued;
	unsigned int timeout;
	struct HostSlowCommand *slow;
	unsigned char result;

	queued = 0;
	timeout = 0x1000;
	slow = NULL;
	P5.6 = 1;
	P90 = 0;
	P5.7 = 0;
	hostTx.status = sleepFactorSnapshot() | (hostLink.statusBit1 ? HOST_STATUS_CHECKSUM : 0) |
			0x08 | (hostLink.statusBit5 ? HOST_STATUS_SLOW : 0) | 0x40;
	hostTx.reserved = 0;
	csiTransferPoll((unsigned char *)&hostRx.command, &hostTx.status, 2);
	if (hostRx.command & 0x0080 ? 0 : 1) {
		result = hostCommandFast(hostRx.command);
		if (result) {
			hostTx.length = 2;
			hostTx.result = result;
		}
		if (result != HOST_RESULT_UNKNOWN)
			sub_3B721();
	} else if ((slow = hostSlowFind(hostRx.command)) != NULL) {
		if (slow->state == HOST_SLOW_QUEUED) {
			logPrintf(" SlowCmd %04X exe resume\r\n", slow->command);
			hostTx.length = 2;
			hostTx.result = HOST_RESULT_RUNNING;
		} else {
			if (slow->command != 0x1181)
				logPrintf(" SlowCmd %04X exesta=%d tag=%x\r\n", slow->command,
					  slow->state, slow->answer.result);
			memcpy(&hostTx.length, &slow->answer.length, sizeof(slow->answer) - 2);
			hostSlowFree(slow);
		}
		hostLink.statusBit5 = hostSlowPending();
	} else if ((slow = hostSlowAlloc(hostRx.command)) != NULL) {
		hostTx.length = 2;
		hostTx.result = HOST_RESULT_QUEUED;
		queued = 1;
	} else {
		logPrintf(" Slowcmd no free queue %04X\r\n", slow->command);
		hostTx.length = 2;
		hostTx.result = HOST_RESULT_FULL;
	}
	csiTransferPoll(&hostRx.length, &hostTx.length, 1);
	size = hostLink.frameSize - 3 > hostTx.length ? hostTx.length : hostLink.frameSize - 3;
	hostTx.data[size - 2] = hostChecksum(&hostTx.status, size + 2);
	size = (hostTx.length < hostRx.length ? hostRx.length : hostTx.length) <
	       hostLink.frameSize - 3 ?
	       (hostTx.length < hostRx.length ? hostRx.length : hostTx.length) :
	       hostLink.frameSize - 3;
	if (!(size & 1))
		size++;
	csiTransferPoll(hostRx.data, &hostTx.result, size);
	P90 = 1;
	P5.7 = 1;
	while (P1.6 && timeout--)
		;
	hostLink.statusBit1 = hostChecksum((unsigned char *)&hostRx.command, hostRx.length + 2) !=
			      hostRx.data[hostRx.length - 1] ? 1 : 0;
	if (queued) {
		result = hostCommandSlow(hostRx.command, slow);
		if (result != 0x14)
			hostLink.statusBit5 = 1;
		if (result != 1)
			sub_3B721();
	}
	return 0;
}

/* exchanges count bytes on CSI00, polled */
int csiTransferPoll(unsigned char *rx, unsigned char *tx, unsigned char count)
{
	if (!count)
		return 0;
	while (count--) {
		SIO00 = *tx++;
		do {
			NOP();
		} while (SSR00 & 0x0040);
		*rx++ = SIO00;
	}
	return 0;
}

/* sends count bytes on UART3, polled */
int uart3WritePoll(unsigned char *data, unsigned char count)
{
	while (count--) {
		SDR12 = *data++;
		do {
			NOP();
		} while (SSR12 & 0x0040);
	}
	return 0;
}

/* in the log's trace mode only: the multi connector switched to the host while it is sent */
void logPrintf(const char *format, ...)
{
	int length;
	va_list ap;
	struct {
		unsigned char	output:2;
		unsigned char	reserved:6;
	} multiCn;

	if (logMode != 3)
		return;
	va_starttop(ap, format);
	length = vsprintf(logBuffer, format, ap);
	va_end(ap);
	multiCn.output = P13.0 ? BOARD_MULTICN_DONGLE : BOARD_MULTICN_HOST;
	boardSetMultiCnOutput(BOARD_MULTICN_HOST);
	DI();
	uart3WritePoll((unsigned char *)logBuffer, length);
	EI();
	boardSetMultiCnOutput(multiCn.output);
}

unsigned char hostChecksum(unsigned char *data, unsigned char length)
{
	unsigned char sum;

	sum = 0;
	while (length--)
		sum += *data++;
	return ~sum;
}

/* the digits of a Subversion $Date$, "$Date:: yyyy-mm-dd hh:mm", as yyyymmddhhmm */
int timeStampDigits(char *digits, const char *stamp)
{
	unsigned char index[12] = { 8, 9, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23 };
	unsigned char i;

	for (i = 0; i < 12; i++)
		digits[i] = stamp[index[i]];
	return 0;
}

struct HostSlowCommand *hostSlowFind(unsigned int command)
{
	int i;

	for (i = 0; i < HOST_SLOW_COMMANDS; i++)
		if (hostSlowCommands[i].command == command)
			return &hostSlowCommands[i];
	return NULL;
}

struct HostSlowCommand *hostSlowAlloc(unsigned int command)
{
	int i;
	struct HostSlowCommand *slow;

	for (i = 0; i < HOST_SLOW_COMMANDS; i++) {
		slow = &hostSlowCommands[i];
		if (!slow->command) {
			slow->command = command;
			return slow;
		}
	}
	return NULL;
}

int hostSlowFree(struct HostSlowCommand *slow)
{
	if (slow != NULL) {
		memset(slow, 0, sizeof(*slow));
		return 0;
	}
	return 1;
}

unsigned char hostSlowPending(void)
{
	int i;

	for (i = 0; i < HOST_SLOW_COMMANDS; i++)
		if (hostSlowCommands[i].command)
			return 1;
	return 0;
}

/* the queued slow commands' handlers run */
void hostSlowRun(void)
{
	int i;
	struct HostSlowCommand *slow;

	for (i = 0; i < HOST_SLOW_COMMANDS; i++) {
		slow = &hostSlowCommands[i];
		if (slow->command && slow->state == HOST_SLOW_QUEUED && slow->handler != NULL) {
			slow->handler(slow);
			if (slow->state == HOST_SLOW_DONE)
				hostLink.statusBit5 = 1;
		}
	}
}
