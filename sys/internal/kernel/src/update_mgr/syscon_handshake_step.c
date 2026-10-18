#include <sm_comm/sm_comm.h>
#include "update_internal.h"
#include "syscon_handshake.h"
#include <syscon/syscon.h>

int SysconUtilHandshakeStep(unsigned int phase, int smId,
		const SysconHandshakeData *input, SysconHandshakeData *output)
{
	SysconHandshakeRequest request;
	unsigned int serviceResult;
	int send;
	int result;

	switch (phase) {
	case 0:
		request.mode = 0;
		send = 1;
		break;
	case 1:
		request.mode = 1;
		send = 1;
		break;
	case 2:
		request.mode = 3;
		send = 1;
		break;
	case 3:
		request.mode = 4;
		send = 0;
		break;
	default:
		return (int)0x800f0216;
	}
	if (input)
		request.input = *input;
	result = SceSblSmCommForKernel_DB9FC204(smId, 0xd0002, &serviceResult,
			&request, sizeof(request));
	if (result)
		return result;
	result = serviceResult;
	if (result)
		return result;
	if (send) {
		request.input = request.output;
		result = SceSysconForDriver_4D03754A(&request.input, 40, &request.output, 40);
		if (result)
			return result;
	}
	if (output)
		*output = request.output;
	if (phase == 1) {
		request.mode = 2;
		request.input = request.output;
		result = SceSblSmCommForKernel_DB9FC204(smId, 0xd0002, &serviceResult,
				&request, sizeof(request));
		if (!result)
			result = serviceResult;
	}
	return result;
}
