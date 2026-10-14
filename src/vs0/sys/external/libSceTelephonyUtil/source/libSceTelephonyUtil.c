/*
 * SceTelephonyUtil (vs0:sys/external/libSceTelephonyUtil.suprx): three
 * requests to the shell over ShellSvc (0x50051-0x50053). Each sends the
 * caller's word and a zeroed 8-byte block as its two input buffers; 0x50052
 * also returns an 8-byte value. The meanings of the requests are not known
 * (PLACEHOLDER: names).
 */

#ifdef __SNC__
/* built without automatic inlining (SNC -Xautoinlinesize=0) */
#pragma control autoinlinesize=0
#endif

#include <psp2/types.h>
#include <sce/shellsvc.h>

#define SCE_ERROR_ERRNO_EFAULT		0x8001000E

#define SHELLSVC_REQUEST_TELEPHONY_51	0x50051
#define SHELLSVC_REQUEST_TELEPHONY_52	0x50052
#define SHELLSVC_REQUEST_TELEPHONY_53	0x50053

/* the zeroed second input buffer of every request (fields unknown) */
struct TelephonyUtilReply {
	SceUInt32 unk0;
	SceUInt32 unk4;
};

void *sceClibMemset(void *dst, int c, SceSize len);

static int requestTelephony51(struct ShellSvcInterface *svc, struct TelephonyUtilReply *block,
			      SceUInt32 arg)
{
	ShellSvcRequest request = (ShellSvcRequest)svc->methods[5];
	struct ShellSvcBuffer in[2];
	int result = 0;
	int ret;

	in[0].address = &arg;
	in[0].size = sizeof(arg);
	in[1].address = block;
	in[1].size = sizeof(*block);
	ret = request(svc, SHELLSVC_REQUEST_TELEPHONY_51, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static int requestTelephony52(struct ShellSvcInterface *svc, struct TelephonyUtilReply *block,
			      SceUInt32 arg, SceUInt64 *value)
{
	ShellSvcRequest request = (ShellSvcRequest)svc->methods[5];
	struct ShellSvcBuffer in[2], out;
	int result = 0;
	int ret;

	in[0].address = &arg;
	in[0].size = sizeof(arg);
	in[1].address = block;
	in[1].size = sizeof(*block);
	out.address = value;
	out.size = sizeof(*value);
	ret = request(svc, SHELLSVC_REQUEST_TELEPHONY_52, in, 2, &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static int requestTelephony53(struct ShellSvcInterface *svc, struct TelephonyUtilReply *block,
			      SceUInt32 arg)
{
	ShellSvcRequest request = (ShellSvcRequest)svc->methods[5];
	struct ShellSvcBuffer in[2];
	int result = 0;
	int ret;

	in[0].address = &arg;
	in[0].size = sizeof(arg);
	in[1].address = block;
	in[1].size = sizeof(*block);
	ret = request(svc, SHELLSVC_REQUEST_TELEPHONY_53, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int SceTelephonyUtil_D0EDF28D(SceUInt32 arg)
{
	struct TelephonyUtilReply block;

	sceClibMemset(&block, 0, sizeof(block));
	return requestTelephony51(SceShellSvc_B31E7F1C(), &block, arg);
}

int SceTelephonyUtil_68D0E38D(SceUInt32 arg, SceUInt64 *value)
{
	struct TelephonyUtilReply block;
	SceUInt64 reply;
	int ret;

	if (value == NULL)
		return SCE_ERROR_ERRNO_EFAULT;
	sceClibMemset(&block, 0, sizeof(block));
	reply = 0;
	ret = requestTelephony52(SceShellSvc_B31E7F1C(), &block, arg, &reply);
	*value = reply;
	return ret;
}

int SceTelephonyUtil_D7362313(SceUInt32 arg)
{
	struct TelephonyUtilReply block;

	sceClibMemset(&block, 0, sizeof(block));
	return requestTelephony53(SceShellSvc_B31E7F1C(), &block, arg);
}
