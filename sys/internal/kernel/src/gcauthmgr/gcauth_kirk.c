/* KIRK commands, run by the gcauthmgr secure module. */
#include <stddef.h>
#include <string.h>
#include <authmgr/sblauthmgr.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/sysroot.h>

/* the request of secure-module service 0x1000B: one KIRK command */
typedef struct GcAuthKirkRequest {
	int count;
	int command;
	unsigned char data[2048];
	int keyId;
	SceSize size;
	int reserved;
} GcAuthKirkRequest;

_Static_assert(sizeof(GcAuthKirkRequest) == 2068, "KIRK request ABI");

static const SceSelfAuthInfo gcAuthKirkSmAuthInfo = {
	.programAuthorityId = 0x2808000000000001ULL,
	.capability = { { 0x00f000c000000080ULL, 0xffffffff00000000ULL } },
	.attribute = { {
		0x00c3000003800980ULL,
		0x0000008009800000ULL,
		0x0000000000000000ULL,
		0xffffffff00000000ULL,
	} },
};

static __attribute__((noinline)) int GcAuthKirkStopSmWithResult(SceSblSmCommId *id, SceSblSmCommPair *result)
{
	if (id == NULL || result == NULL)
		return (int)0x800f1816;
	result->result = -1;
	result->additionalResult = -1;
	return sceSblSmCommStopSm(*id, result);
}

static __attribute__((noinline)) int GcAuthKirkStopSm(SceSblSmCommId *id)
{
	SceSblSmCommPair stopResult;
	int result;

	if (id == NULL || *id == -1)
		return (int)0x800f1816;
	result = GcAuthKirkStopSmWithResult(id, &stopResult);
	*id = -1;
	return result;
}

int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId)
{
	struct SysrootSelfInfo selfInfo;
	SelfAuthRequest auth;
	GcAuthKirkRequest request;
	SceSblSmCommId id;
	SceUInt32 serviceResult;
	int result;

	id = -1;
	serviceResult = 0;
	if (size > 0x800)
		return (int)0x800f1816;
	request.count = 1;
	request.command = command;
	if (input != NULL)
		memcpy(request.data, input, size);
	selfInfo.selfData = NULL;
	request.keyId = keyId;
	request.size = size;
	request.reserved = 0;
	selfInfo.selfSize = 0;
	selfInfo.size = sizeof(selfInfo);
	if (SceSysrootForKernel_F10AB792(SCE_KERNEL_SYSROOT_SELF_INDEX_GCAUTHMGR_SM,
		&selfInfo) < 0) {
		result = (int)0x800f0016;
		goto stop;
	}
	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.caller, &gcAuthKirkSmAuthInfo, sizeof(auth.caller));
	auth.mediaType = 2;
	auth.selfType = 2;
	result = SceSblSmCommForKernel_039C73B1(0, selfInfo.selfData,
		selfInfo.selfSize, 0, &auth, &id);
	if (result != 0)
		goto stop;
	result = sceSblSmCommCallFunc(id, 0x1000b, &serviceResult, &request,
		sizeof(request));
	if (result != 0)
		goto stop;
	result = serviceResult;
	if (result != 0)
		goto stop;

	if (request.size > 0x800)
		goto invalid;
	if ((request.command == 4 || request.command == 7) &&
	    (size != request.size || (size & 15) != 0 || size == 0))
		goto invalid;
	if (request.command == 12 && request.size != 60)
		goto invalid;
	if (request.command == 13 && request.size != 40)
		goto invalid;
	if (request.command == 14 && request.size != 20)
		goto invalid;
	if ((request.command == 16 || request.command == 33) && request.size != 40)
		goto invalid;
	if (request.command == 17 && request.size != 100)
		goto invalid;
	if (request.command == 18 && request.size != 184)
		goto invalid;
	if (request.command == 20 && request.size != 84)
		goto invalid;
	if (request.command == 21 && request.size != 56)
		goto invalid;
	if (request.command == 22 && request.size != 28)
		goto invalid;
	if ((request.command == 23 || request.command == 34) && request.size != 56)
		goto invalid;
	if (request.command == 24 && request.size != 140)
		goto invalid;
	if (request.command == 25 && request.size != 232)
		goto invalid;
	if (request.command == 27 && request.size != 83)
		goto invalid;
	if ((request.command == 28 || request.command == 30) && request.size != 51)
		goto invalid;
	if (request.command == 29 && request.size != 163)
		goto invalid;
	if ((request.command == 31 || request.command == 35) && request.size != 32)
		goto invalid;
	if (request.command == 32 && request.size != 52)
		goto invalid;
	if (output != NULL)
		memcpy(output, request.data, request.size);
	goto stop;
invalid:
	result = 16;
stop:
	if (id != -1)
		GcAuthKirkStopSm(&id);
	return result;
}
