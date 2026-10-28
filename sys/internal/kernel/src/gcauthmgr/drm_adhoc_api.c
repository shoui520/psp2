/* Adhoc exchange wrappers on a dedicated kernel stack. */
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include "adhoc.h"
#include <intrmgr/syscall.h>
#include <gcauthmgr/sblgcauthmgrdrmbb.h>
#include <gcauthmgr/sblgcauthmgrpkg.h>

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

int GcAuthAdhocVerifyResponse224ForUser(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocVerifyResponse224Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocVerifyResponse224, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocFinishContext224ForUser(GcAuthAdhocContext224 *context,
		const GcAuthAdhocFinalPacket *packet)
{
	GcAuthAdhocFinishContext224Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocFinishContext224, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocGetSeeds224ForUser(const GcAuthAdhocContext224 *context,
		unsigned char *random, unsigned char *contextRandom, unsigned char *seed)
{
	GcAuthAdhocGetSeeds224Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.random = random;
	request.contextRandom = contextRandom;
	request.seed = seed;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocGetSeeds224, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocVerifyFixed160ForUser(const unsigned char *digest,
		const GcAuthAdhocSignature160 *signature)
{
	GcAuthAdhocVerifyPair160 request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.digest = digest;
	request.signature = signature;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocVerifyFixed160, &request);
	sceSyscallLeave(state);
	return result;
}

int SceSblGcAuthMgrPkgForDriver_E459A9A8(const char *pHash, const char *pSig)
{
	GcAuthAdhocVerifyPair160 request;

	request.digest = (const unsigned char *)pHash;
	request.signature = (const GcAuthAdhocSignature160 *)pSig;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocVerifyFixed160, &request);
}

int SceSblGcAuthMgrDrmBBForDriver_30A0B441(const unsigned char *digest, unsigned char *signature, int keyId)
{
	GcAuthAdhocSign160Request request;

	request.digest = digest;
	request.signature = (GcAuthAdhocSignature160 *)signature;
	request.keyId = keyId;
	return SceThreadmgrForDriver_E54FD746(8192, (int (*)(void *))GcAuthAdhocSign160, &request);
}

int SceSblGcAuthMgrDrmBBForDriver_4CEA150C(const unsigned char *digest, unsigned char *signature, int keyId)
{
	GcAuthAdhocSign160Request request;

	request.digest = digest;
	request.signature = (GcAuthAdhocSignature160 *)signature;
	request.keyId = keyId;
	return SceThreadmgrForDriver_E54FD746(8192, (int (*)(void *))GcAuthAdhocVerifyRequest160, &request);
}

int SceSblGcAuthMgrDrmBBForDriver_050DC6DF(const unsigned char *digest, unsigned char *signature, int keyId)
{
	GcAuthAdhocSign224Request request;

	request.digest = digest;
	request.signature = (GcAuthAdhocSignature224 *)signature;
	request.keyId = keyId;
	return SceThreadmgrForDriver_E54FD746(8192, (int (*)(void *))GcAuthAdhocSign224, &request);
}

int SceSblGcAuthMgrDrmBBForDriver_4B506BE7(const unsigned char *digest, unsigned char *signature, int keyId)
{
	GcAuthAdhocSign224Request request;

	request.digest = digest;
	request.signature = (GcAuthAdhocSignature224 *)signature;
	request.keyId = keyId;
	return SceThreadmgrForDriver_E54FD746(8192, (int (*)(void *))GcAuthAdhocVerifyRequest224, &request);
}

int GcAuthAdhocInitializeState160ForUser(void)
{
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	result = GcAuthAdhocInitializeState();
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocClearState160ForUser(void)
{
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	result = GcAuthAdhocClearState();
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocCreateContext160ForUser(GcAuthAdhocContext160 *context,
		unsigned char *packet)
{
	GcAuthAdhocCreateContext160Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocCreateContext160, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocAnswerChallenge160ForUser(GcAuthAdhocContext160 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocAnswerChallenge160Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocAnswerChallenge160, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocAuthenticateResponse160ForUser(GcAuthAdhocContext160 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocAuthenticateResponse160Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocAuthenticateResponse160, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocVerifyResponse160ForUser(GcAuthAdhocContext160 *context,
		const unsigned char *packet, unsigned char *output)
{
	GcAuthAdhocVerifyResponse160Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	request.output = output;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocVerifyResponse160, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocFinishContext160ForUser(GcAuthAdhocContext160 *context,
		const GcAuthAdhocFinalPacket *packet)
{
	GcAuthAdhocFinishContext160Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.packet = packet;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocFinishContext160, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocGetSeeds160ForUser(const GcAuthAdhocContext160 *context,
		unsigned char *random, unsigned char *contextRandom, unsigned char *seed)
{
	GcAuthAdhocGetSeeds160Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.random = random;
	request.contextRandom = contextRandom;
	request.seed = seed;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocGetSeeds160, &request);
	sceSyscallLeave(state);
	return result;
}

int GcAuthAdhocUniCastEncrypt160ForUser(GcAuthAdhocContext160 *context,
		unsigned char *data, unsigned int size, GcAuthAdhocAuthTag *tag, int encrypt)
{
	GcAuthAdhocUniCastEncrypt160Request request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.data = data;
	request.size = size;
	request.tag = tag;
	request.encrypt = encrypt;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthAdhocUniCastEncrypt160Stack, &request);
	sceSyscallLeave(state);
	return result;
}
