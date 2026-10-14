/* Adhoc exchange wrappers on a dedicated kernel stack. */
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/cpu.h>
#include "adhoc.h"

int GcAuthAdhocCreateContext224(const GcAuthAdhocCreateContext224Request *request);
int GcAuthAdhocAnswerChallenge224(const GcAuthAdhocAnswerChallenge224Request *request);
int GcAuthAdhocAuthenticateResponse224(const GcAuthAdhocAuthenticateResponse224Request *request);

int GcAuthAdhocCreateContext224ForUser(GcAuthAdhocContext224 *context,
		unsigned char *packet)
{
	GcAuthAdhocCreateContext224Request request;
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	request.context = context;
	request.packet = packet;
	result = ksceKernelRunWithStack(8192,
		(int (*)(void *))GcAuthAdhocCreateContext224, &request);
	EXIT_SYSCALL(state);
	return result;
}

int GcAuthAdhocAnswerChallenge224ForUser(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocAnswerChallenge224Request request;
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = ksceKernelRunWithStack(8192,
		(int (*)(void *))GcAuthAdhocAnswerChallenge224, &request);
	EXIT_SYSCALL(state);
	return result;
}

int GcAuthAdhocAuthenticateResponse224ForUser(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocAuthenticateResponse224Request request;
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = ksceKernelRunWithStack(8192,
		(int (*)(void *))GcAuthAdhocAuthenticateResponse224, &request);
	EXIT_SYSCALL(state);
	return result;
}
