#include "update_internal.h"
#include "syscon_handshake.h"

int SysconUtilHandshake(void)
{
	SysconHandshakeData data, reply;
	int smId = -1;
	int result;

	result = UpdateUtilSetupSm(0, &smId, 0);
	if (!result) {
		result = SysconUtilHandshakeStep(0, smId, NULL, &data);
		if (!result) {
			result = SysconUtilHandshakeStep(1, smId, &data, &reply);
			if (!result) {
				result = SysconUtilHandshakeStep(2, smId, NULL, &data);
				if (!result)
					result = SysconUtilHandshakeStep(3, smId, &data, NULL);
			}
		}
	}
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	return result;
}
