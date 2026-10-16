/*
 * SceNetCtl ShellSvc requests.  Every exported operation is forwarded to the
 * NetCtl service through the ShellSvc client object; each request packs the
 * scalar arguments into one input block followed by the caller's buffers.
 */

#include "netctl_internal.h"

/* size of one entry of the 0x20036..0x20039 tables */
#define NETCTL_IPC_ENTRY_SIZE	0x478

int netCtlIpcInit(NetCtlSvc *svc, int arg)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = arg;
	ret = (*svc->vtable->request)(svc, 0x20000, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcTerm(NetCtlSvc *svc, int arg)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = arg;
	ret = (*svc->vtable->request)(svc, 0x20001, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcInetGetResult(NetCtlSvc *svc, int eventType, int *errorCode)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = sizeof(int);
	out[0].address = errorCode;
	param = eventType;
	ret = (*svc->vtable->request)(svc, 0x20002, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20003(NetCtlSvc *svc, int a0, int *value)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = sizeof(int);
	out[0].address = value;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x20003, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcAdhocGetResult(NetCtlSvc *svc, int eventType, int *errorCode)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = sizeof(int);
	out[0].address = errorCode;
	param = eventType;
	ret = (*svc->vtable->request)(svc, 0x20004, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20005(NetCtlSvc *svc, int a0, int *value)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = sizeof(int);
	out[0].address = value;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x20005, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20006(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x20006, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20007(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x20007, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20008(NetCtlSvc *svc, const void *param, const void *data1, SceSize size1,
		   const void *data2, SceSize size2)
{
	int result;
	int ret;
	struct {
		int size1;
		int size2;
	} args;
	NetCtlSvcInBuffer in[4];

	result = 0;
	in[0].size = sizeof(args);
	in[0].address = &args;
	in[1].size = 0x608;
	in[1].address = param;
	in[2].size = size1;
	in[2].address = data1;
	in[3].size = size2;
	in[3].address = data2;
	args.size1 = size1;
	args.size2 = size2;
	ret = (*svc->vtable->request)(svc, 0x20008, in, 4, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20009(NetCtlSvc *svc, const void *param, const void *data1, SceSize size1,
		   const void *data2, SceSize size2, int a6)
{
	int result;
	int ret;
	struct {
		int size1;
		int size2;
		int a6;
	} args;
	NetCtlSvcInBuffer in[4];

	result = 0;
	in[0].size = sizeof(args);
	in[0].address = &args;
	in[1].size = 0x608;
	in[1].address = param;
	in[2].size = size1;
	in[2].address = data1;
	in[3].size = size2;
	in[3].address = data2;
	args.size1 = size1;
	args.size2 = size2;
	args.a6 = a6;
	ret = (*svc->vtable->request)(svc, 0x20009, in, 4, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2000A(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x2000a, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcInetGetInfo(NetCtlSvc *svc, int code, SceNetCtlInfo *info)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = sizeof(*info);
	out[0].address = info;
	param = code;
	ret = (*svc->vtable->request)(svc, 0x2000b, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2000C(NetCtlSvc *svc, void *buf)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = 6;
	out[0].address = buf;
	ret = (*svc->vtable->request)(svc, 0x2000c, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcInetGetState(NetCtlSvc *svc, int *state)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = state;
	ret = (*svc->vtable->request)(svc, 0x2000d, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2000E(NetCtlSvc *svc, int *value)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = value;
	ret = (*svc->vtable->request)(svc, 0x2000e, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcGetPhoneMaxDownloadableSize(NetCtlSvc *svc, SceInt64 *size)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(SceInt64);
	out[0].address = size;
	ret = (*svc->vtable->request)(svc, 0x2000f, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20010(NetCtlSvc *svc, const void *ssids, SceUInt32 count, const void *param,
		   int a4, int a5)
{
	int result;
	int ret;
	struct {
		int count;
		int a5;
	} args;
	NetCtlSvcInBuffer in[4];

	result = 0;
	in[0].size = sizeof(args);
	in[0].address = &args;
	in[1].size = 0x14a;
	in[1].address = ssids;
	in[2].size = 0x38;
	in[2].address = param;
	in[3].size = a5;
	in[3].address = (const void *)a4;
	args.count = count;
	args.a5 = a5;
	ret = (*svc->vtable->request)(svc, 0x20010, in, 4, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20011(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x20011, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20012(NetCtlSvc *svc, int a0, void *buf)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = 0x60;
	out[0].address = buf;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x20012, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20013(NetCtlSvc *svc, void *buf, int count, int *outCount)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[2];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = 0x60;
	out[0].address = buf;
	out[1].size = sizeof(int);
	out[1].address = outCount;
	param = count;
	ret = (*svc->vtable->request)(svc, 0x20013, in, 1, &result,
				       out, 2);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20014(NetCtlSvc *svc, void *a0, void *a1, void *a2, int size, int *outSize)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[2];
	NetCtlSvcOutBuffer out[3];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	in[1].size = 6;
	in[1].address = a0;
	out[0].size = 0x60;
	out[0].address = a1;
	out[1].size = 0x800;
	out[1].address = a2;
	out[2].size = sizeof(int);
	out[2].address = outSize;
	param = size;
	ret = (*svc->vtable->request)(svc, 0x20014, in, 2, &result,
				       out, 3);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20015(NetCtlSvc *svc, void *a0, void *a1, int size, int *outSize)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[3];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = 0x60;
	out[0].address = a0;
	out[1].size = 0x800;
	out[1].address = a1;
	out[2].size = sizeof(int);
	out[2].address = outSize;
	param = size;
	ret = (*svc->vtable->request)(svc, 0x20015, in, 1, &result,
				       out, 3);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20016(NetCtlSvc *svc, void *buf, int count, int *outCount)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[2];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = 0x60;
	out[0].address = buf;
	out[1].size = sizeof(int);
	out[1].address = outCount;
	param = count;
	ret = (*svc->vtable->request)(svc, 0x20016, in, 1, &result,
				       out, 2);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20017(NetCtlSvc *svc, void *a0, void *a1, void *a2, int size, int *outSize)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[2];
	NetCtlSvcOutBuffer out[3];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	in[1].size = 6;
	in[1].address = a0;
	out[0].size = 0x60;
	out[0].address = a1;
	out[1].size = 0x800;
	out[1].address = a2;
	out[2].size = sizeof(int);
	out[2].address = outSize;
	param = size;
	ret = (*svc->vtable->request)(svc, 0x20017, in, 2, &result,
				       out, 3);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20018(NetCtlSvc *svc, int *value)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = value;
	ret = (*svc->vtable->request)(svc, 0x20018, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20019(NetCtlSvc *svc, int *value)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = value;
	ret = (*svc->vtable->request)(svc, 0x20019, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2001A(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x2001a, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2001B(NetCtlSvc *svc, const void *name)
{
	int result;
	int ret;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = 0x21;
	in[0].address = name;
	ret = (*svc->vtable->request)(svc, 0x2001b, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2001C(NetCtlSvc *svc, const void *name)
{
	int result;
	int ret;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = 0x21;
	in[0].address = name;
	ret = (*svc->vtable->request)(svc, 0x2001c, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2001D(NetCtlSvc *svc, const void *name)
{
	int result;
	int ret;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = 0x21;
	in[0].address = name;
	ret = (*svc->vtable->request)(svc, 0x2001d, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2001E(NetCtlSvc *svc, const void *name, int a1)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[2];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	in[1].size = 0x21;
	in[1].address = name;
	param = a1;
	ret = (*svc->vtable->request)(svc, 0x2001e, in, 2, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcAdhocDisconnect(NetCtlSvc *svc, int arg)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = arg;
	ret = (*svc->vtable->request)(svc, 0x2001f, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcAdhocGetState(NetCtlSvc *svc, int *state)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = state;
	ret = (*svc->vtable->request)(svc, 0x20020, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20021(NetCtlSvc *svc, int code, void *info)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = 0x24;
	out[0].address = info;
	param = code;
	ret = (*svc->vtable->request)(svc, 0x20021, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20022(NetCtlSvc *svc, int *value)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = value;
	ret = (*svc->vtable->request)(svc, 0x20022, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcAdhocGetInAddr(NetCtlSvc *svc, SceNetInAddr *inaddr)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(*inaddr);
	out[0].address = inaddr;
	ret = (*svc->vtable->request)(svc, 0x20023, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcGetNatInfo(NetCtlSvc *svc, SceSize size, SceNetCtlNatInfo *natinfo)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = sizeof(*natinfo);
	out[0].address = natinfo;
	param = size;
	ret = (*svc->vtable->request)(svc, 0x20024, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20025(NetCtlSvc *svc, SceSize size, void *info)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	out[0].size = 0x28;
	out[0].address = info;
	param = size;
	ret = (*svc->vtable->request)(svc, 0x20025, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20026(NetCtlSvc *svc, int *value)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = value;
	ret = (*svc->vtable->request)(svc, 0x20026, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20027(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x20027, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20028(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x20028, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20029(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x20029, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2002A(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x2002a, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2002B(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x2002b, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2002C(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x2002c, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2002D(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x2002d, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2002E(NetCtlSvc *svc, void *buf)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = 0x30c;
	out[0].address = buf;
	ret = (*svc->vtable->request)(svc, 0x2002e, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2002F(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x2002f, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int netCtlIpcGetIfStat(NetCtlSvc *svc, int device, SceSize size, SceNetCtlIfStat *ifstat)
{
	int result;
	int ret;
	struct {
		int device;
		int size;
	} args;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(args);
	in[0].address = &args;
	out[0].size = sizeof(*ifstat);
	out[0].address = ifstat;
	args.device = device;
	args.size = size;
	ret = (*svc->vtable->request)(svc, 0x20030, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20031(NetCtlSvc *svc, int device, SceSize size, SceNetCtlIfStat *ifstat)
{
	int result;
	int ret;
	struct {
		int device;
		int size;
	} args;
	NetCtlSvcInBuffer in[1];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(args);
	in[0].address = &args;
	out[0].size = sizeof(*ifstat);
	out[0].address = ifstat;
	args.device = device;
	args.size = size;
	ret = (*svc->vtable->request)(svc, 0x20031, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20032(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x20032, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20033(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x20033, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20034(NetCtlSvc *svc, int a0, int a1)
{
	int result;
	int ret;
	struct {
		int a0;
		int a1;
	} args;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(args);
	in[0].address = &args;
	args.a0 = a0;
	args.a1 = a1;
	ret = (*svc->vtable->request)(svc, 0x20034, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20035(NetCtlSvc *svc, int a0, int a1)
{
	int result;
	int ret;
	struct {
		int a0;
		int a1;
	} args;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(args);
	in[0].address = &args;
	args.a0 = a0;
	args.a1 = a1;
	ret = (*svc->vtable->request)(svc, 0x20035, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20036(NetCtlSvc *svc, const void *entries, int count)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[2];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	in[1].size = count * NETCTL_IPC_ENTRY_SIZE;
	in[1].address = entries;
	param = count;
	ret = (*svc->vtable->request)(svc, 0x20036, in, 2, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20037(NetCtlSvc *svc, const void *ids, void *entries, int count)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[2];
	NetCtlSvcOutBuffer out[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	in[1].size = count * sizeof(int);
	in[1].address = ids;
	out[0].size = count * NETCTL_IPC_ENTRY_SIZE;
	out[0].address = entries;
	param = count;
	ret = (*svc->vtable->request)(svc, 0x20037, in, 2, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20038(NetCtlSvc *svc, const void *entry)
{
	int result;
	int ret;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = NETCTL_IPC_ENTRY_SIZE;
	in[0].address = entry;
	ret = (*svc->vtable->request)(svc, 0x20038, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc20039(NetCtlSvc *svc, void *entry)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = NETCTL_IPC_ENTRY_SIZE;
	out[0].address = entry;
	ret = (*svc->vtable->request)(svc, 0x20039, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2003A(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x2003a, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2003B(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x2003b, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2003C(NetCtlSvc *svc, int *value)
{
	int result;
	int ret;
	NetCtlSvcOutBuffer out[1];

	result = 0;
	out[0].size = sizeof(int);
	out[0].address = value;
	ret = (*svc->vtable->request)(svc, 0x2003c, NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2003D(NetCtlSvc *svc, int a0)
{
	int result;
	int ret;
	int param;
	NetCtlSvcInBuffer in[1];

	result = 0;
	in[0].size = sizeof(param);
	in[0].address = &param;
	param = a0;
	ret = (*svc->vtable->request)(svc, 0x2003d, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* name unknown */
int netCtlIpc2003E(NetCtlSvc *svc)
{
	int result;
	int ret;

	result = 0;
	ret = (*svc->vtable->request)(svc, 0x2003e, NULL, 0, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
