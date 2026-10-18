/* Adhoc exchange wrappers on a dedicated kernel stack. */
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include "adhoc.h"
#include <intrmgr/syscall.h>

int GcAuthAdhocCreateContext224(const GcAuthAdhocCreateContext224Request *request);
int GcAuthAdhocAnswerChallenge224(const GcAuthAdhocAnswerChallenge224Request *request);
int GcAuthAdhocAuthenticateResponse224(const GcAuthAdhocAuthenticateResponse224Request *request);

int GcAuthAdhocCreateContext224ForUser(GcAuthAdhocContext224 *context,
		unsigned char *packet)
{
	GcAuthAdhocCreateContext224Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocCreateContext224, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocAnswerChallenge224ForUser(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocAnswerChallenge224Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocAnswerChallenge224, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocAuthenticateResponse224ForUser(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocAuthenticateResponse224Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocAuthenticateResponse224, &request);
	sceSyscallLeave(state);
	return result;
}
