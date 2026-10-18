/* Descriptive source boundary for the original Pcact stack wrappers. */
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include "pcact.h"
#include <gcauthmgr/sblgcauthmgrmlnpsnl.h>
#include <intrmgr/syscall.h>

int GcAuthPcactEncryptChallenge(const GcAuthPcactEncryptChallengeRequest *request);
int GcAuthPcactGetAesDecryptKey(const GcAuthPcactKeyRequest *request);
int GcAuthPcactCreateChallenge(const GcAuthPcactChallengeRequest *request);
int GcAuthPcactGetAuthenticatedAesKey(const GcAuthPcactKeyRequest *request);
int GcAuthPcactCreateRtcChallenge(const GcAuthPcactRtcChallengeRequest *request);
int GcAuthPcactApplyRtcActivation(const GcAuthPcactRtcActivationRequest *request);
int GcAuthPcactGetCertificate(const GcAuthPcactCertificateRequest *request);
int GcAuthPsmactCreateChallenge(const GcAuthPsmactChallengeRequest *request);
int GcAuthPsmactVerifyActivation(const GcAuthPsmactVerifyRequest *request);

int GcAuthPcactEncryptChallengeWithStack(unsigned char mode,
		const GcAuthPcactChallengeInput *input,
		const unsigned char *rtcSeconds, GcAuthPcactEncryptedChallenge *output,
		GcAuthPcactRandomRecord *random)
{
	GcAuthPcactEncryptChallengeRequest request;

	request.input = input;
	request.mode = mode;
	request.rtcSeconds = rtcSeconds;
	request.output = output;
	request.random = random;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactEncryptChallenge, &request);
}

int GcAuthPcactGetAesDecryptKeyWithStack(const GcAuthPcactActivationBlock *activation,
		const GcAuthPcactRandomRecord *random, unsigned char *outputKey)
{
	GcAuthPcactKeyRequest request;

	request.random = random;
	request.activation = activation;
	request.outputKey = outputKey;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactGetAesDecryptKey, &request);
}

int sceSblGcAuthMgrMlnpsnlAuth1ForDriver(const unsigned char *entropy,
		const unsigned char *rtcSeconds, GcAuthPcactChallenge *output,
		GcAuthPcactRandomRecord *random)
{
	GcAuthPcactChallengeRequest request;

	request.rtcSeconds = rtcSeconds;
	request.entropy = entropy;
	request.output = output;
	request.random = random;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactCreateChallenge, &request);
}

int sceSblGcAuthMgrMlnpsnlAuth2ForDriver(const GcAuthPcactActivationBlock *activation,
		const GcAuthPcactRandomRecord *random, unsigned char *outputKey)
{
	GcAuthPcactKeyRequest request;

	request.random = random;
	request.activation = activation;
	request.outputKey = outputKey;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactGetAuthenticatedAesKey, &request);
}

int GcAuthPcactCreateRtcChallengeWithStack(SceSblGcAuthMgrSclkData1 *output)
{
	GcAuthPcactRtcChallengeRequest request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.output = output;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactCreateRtcChallenge, &request);
	sceSyscallLeave(state);
	return result;
}

int sceSblGcAuthMgrSclkGetData1ForDriver(SceSblGcAuthMgrSclkData1 *output)
{
	GcAuthPcactRtcChallengeRequest request;

	request.output = output;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactCreateRtcChallenge, &request);
}

int GcAuthPcactApplyRtcActivationWithStack(const SceSblGcAuthMgrSclkData2 *activation)
{
	GcAuthPcactRtcActivationRequest request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.activation = activation;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactApplyRtcActivation, &request);
	sceSyscallLeave(state);
	return result;
}

int sceSblGcAuthMgrSclkSetData2ForDriver(const SceSblGcAuthMgrSclkData2 *activation)
{
	GcAuthPcactRtcActivationRequest request;

	request.activation = activation;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactApplyRtcActivation, &request);
}

int SceSblGcAuthMgrPsmactForDriver_39222A58(unsigned char *output)
{
	GcAuthPcactCertificateRequest request;

	request.output = output;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPcactGetCertificate, &request);
}

int GcAuthPsmactCreateChallengeWithStack(unsigned char mode,
		const unsigned char *rtcSeconds, GcAuthPsmactChallenge *output,
		GcAuthPsmactRandomRecord *random)
{
	GcAuthPsmactChallengeRequest request;

	request.rtcSeconds = rtcSeconds;
	request.mode = mode;
	request.output = output;
	request.random = random;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPsmactCreateChallenge, &request);
}

int GcAuthPsmactVerifyActivationWithStack(const GcAuthPsmactActivation *activation,
		const GcAuthPsmactRandomRecord *random, GcAuthPsmactDerivedKeys *output)
{
	GcAuthPsmactVerifyRequest request;

	request.random = random;
	request.activation = activation;
	request.output = output;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPsmactVerifyActivation, &request);
}
