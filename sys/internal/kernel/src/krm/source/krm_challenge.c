#include "krm_internal.h"

int KrmDriverRespondToChallenge(const KrmChallenge *challenge,
				KrmChallengeResponse *response)
{
	KrmChallengeResponse localResponse __attribute__((aligned(8)));
	KrmChallenge localChallenge;
	int ret;

	if (!challenge || !response ||
	    !KrmDriverIsAccessible(challenge, sizeof(*challenge), 1) ||
	    !KrmDriverIsAccessible(response, sizeof(*response), 2))
		return (int)0x80a00000;
	ret = KrmDriverCopyFromUser(&localChallenge, challenge, sizeof(localChallenge));
	if (ret < 0)
		return ret;
	ret = KrmVerifyChallenge(&localChallenge);
	if (ret == 0) {
		ret = KrmCreateChallengeResponse(&localChallenge.publicKey, &localResponse);
		if (ret == 0)
			ret = KrmDriverCopyToUser(response, &localResponse, sizeof(localResponse));
	}
	memset(&localChallenge, 0, sizeof(localChallenge));
	memset(&localResponse, 0, sizeof(localResponse));
	return ret;
}
