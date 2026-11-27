/*
 * swagner's request dispatcher for the manager's commands (DTCPIP-MW/Src/Mw/
 * SwMwManagerDispatch.cpp, from its assert and error strings): the session commands the
 * library's IPC sends (0x2101 connect, 0x2102 disconnect, 0x2103..0x2119), and the 0x1101
 * device connect. Each handler fills the channel's request context, calls the manager
 * (SwMwManager.cpp) or SAC device (SwMwSACDevice.cpp) and answers through the server's
 * reply.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include "../Comm/vita/SwMwCommServer.h"
#include "SwMwMoveSink.h"
#include "SwMwSACDevice.h"

#define SWMW_MANAGER_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwManagerDispatch.cpp"

/* contiguous in/out certificate pair for 0x2106 (SwMwSacDeviceSetCertificatePair) */
struct SwMwSacCertificatePair {
	unsigned char input[32];
	unsigned char output[32];
};

/* wire layout of a 0x230-byte resume record passed to 0x210f */
struct SwMwWireResumeRecord {
	int requested;
	int reservedWord;
	size_t size;
	unsigned char payload[512];
	int kind;
	unsigned char keyBlock[32];
};

extern "C" {
int swMwTrace(const char *format, ...);

/* the manager object's session methods (SwMwManager.cpp) */
int SwMwManagerCreate(int *device, const int *values, void *payload, int size);
int SwMwManagerTeardown(int device);
int SwMwManagerStartActivation(void);
int SwMwManagerGetActivationResult(int *out);
int SwMwManagerCancel(void);
int SwMwManagerCheckActivation(void);
int SwMwManagerEnsureSession(int device, int *out);
int SwMwManagerCancelDownloadWrap(void);
int SwMwManagerNoop(void);
int SwMwManagerSetSrm(int device, void *srm, int size);
int SwMwManagerSetNetworkState(int device, int connected, int valid);
int SwMwManagerStartResumeList(int device, int isSink);
int SwMwManagerGetResumeData(int device, int isSink, int contentId, struct SwMwMoveRecord *out);
int SwMwManagerEndResumeList(int device, int isSink);
int SwMwManagerAddSinkResumeData(int device, const struct SwMwMoveRecord *record);
int SwMwManagerAddSourceResumeData(int device, const struct SwMwMoveRecord *record);
int SwMwManagerClearSinkResumeData(int device);
int SwMwManagerClearSourceResumeData(int device);
int SwMwManagerNotifyLabel(int device, int label, int status);
int SwMwManagerClearLabels(int device);
int SwMwManagerInitKey(int device);
}

/* 0x2101: connect the manager to the library's session */
extern "C" void SwMwManagerConnect(struct SwMwCommChannel *channel, struct SwMwIpcMessage *request,
				  void *argument)
{
	struct SwMwManagerContextBlock context;
	int values[4];
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 162);
		return;
	}
	if (*device != 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 168);
		return;
	}
	values[0] = request->value0;
	values[1] = request->value1;
	values[2] = request->value2;
	values[3] = request->value3;
	result = SwMwManagerCreate(device, values, request->payload, request->size);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 184);
	else
		SwMwCommServerSetRequestValue((struct SwMwCommRequest *)channel, context.head);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 192);
}

/* 0x2102: disconnect the manager */
extern "C" void SwMwManagerDisconnect(struct SwMwCommChannel *channel,
				     struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 206);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 212);
		return;
	}
	result = SwMwManagerTeardown(*device);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 217);
	*device = 0;
	SwMwCommServerSetRequestValue((struct SwMwCommRequest *)channel, context.head);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 226);
}

/* 0x2117: wait for the session to be ready */
extern "C" void SwMwManagerWaitReady(struct SwMwCommChannel *channel,
				    struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int value;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 241);
		return;
	}
	result = SwMwManagerEnsureSession(*device, &value);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = value;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 253);
}

/* 0x2118: destroy the session */
extern "C" void SwMwManagerDestroy(struct SwMwCommChannel *channel,
				  struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 266);
		return;
	}
	result = SwMwManagerCancelDownloadWrap();
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 277);
}

/* 0x210a: update the manager's SRM data */
extern "C" void SwMwManagerSetSrmCommand(struct SwMwCommChannel *channel,
					struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 290);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 296);
		return;
	}
	result = SwMwManagerSetSrm(*device, request->payload, request->size);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 304);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 310);
}

/* 0x1101: connect the channel's device */
extern "C" int SwMwManagerDeviceConnect(struct SwMwCommChannel *channel,
					struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->value0 == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 321);
	}
	result = SwMwManagerNoop();
	if (result != 0)
		return swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 326);
	return result;
}

/* 0x2103: create a SAC device handle */
extern "C" void SwMwManagerCreateSacDevice(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	struct SwMwSacDevice *sacDevice;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 338);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 344);
		return;
	}
	result = SwMwSacDeviceCreate(&sacDevice);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 349);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = (int)(uintptr_t)sacDevice;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 356);
}

/* 0x2104: destroy a SAC device handle */
extern "C" void SwMwManagerDestroySacDevice(struct SwMwCommChannel *channel,
					   struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 370);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 376);
		return;
	}
	result = SwMwSacDeviceDestroy((struct SwMwSacDevice *)(uintptr_t)request->value0);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 383);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 389);
}

/* 0x2105: obtain the 4-byte key from a SAC device */
extern "C" void SwMwManagerSacSetKey(struct SwMwCommChannel *channel,
				    struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	unsigned char key[4];
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 403);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 409);
		return;
	}
	result = SwMwSacDeviceSetKey((struct SwMwSacDevice *)(uintptr_t)request->value0, key);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 416);
	request->result = result;
	request->outSize = 4;
	request->outPayload = key;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 422);
}

/* 0x2106: exchange a 32-byte certificate pair on a SAC device */
extern "C" void SwMwManagerSacExchangeCert(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	struct SwMwSacCertificatePair certPair;
	struct SwMwSacDevice *sacDevice;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 437);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 443);
		return;
	}
	sacDevice = (struct SwMwSacDevice *)(uintptr_t)request->value0;
	memcpy(certPair.input, request->payload, sizeof(certPair.input));
	result = SwMwSacDeviceSetCertificatePair(sacDevice, &certPair);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 452);
	request->result = result;
	request->outSize = 32;
	request->outPayload = certPair.output;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 458);
}

/* 0x2107: hand a 32-byte certificate to a SAC device */
extern "C" void SwMwManagerSacSetCert(struct SwMwCommChannel *channel,
				     struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	unsigned char cert[32];
	struct SwMwSacDevice *sacDevice;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 473);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 479);
		return;
	}
	sacDevice = (struct SwMwSacDevice *)(uintptr_t)request->value0;
	memcpy(cert, request->payload, sizeof(cert));
	result = SwMwSacDeviceSetCertificate(sacDevice, cert);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 488);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 494);
}

/* 0x2108: store sender/receiver ids, length and certificate on a SAC device */
extern "C" void SwMwManagerSacSetInfo(struct SwMwCommChannel *channel,
				     struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	struct SwMwSacDeviceInfo info;
	struct SwMwSacDevice *sacDevice;
	const unsigned char *cursor;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 511);
		return;
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 517);
		return;
	}
	sacDevice = (struct SwMwSacDevice *)(uintptr_t)request->value0;
	cursor = (const unsigned char *)request->payload;
	memcpy(info.senderId, cursor, sizeof(info.senderId));
	cursor += sizeof(info.senderId);
	memcpy(info.receiverId, cursor, sizeof(info.receiverId));
	cursor += sizeof(info.receiverId);
	memcpy(&info.length, cursor, sizeof(info.length));
	cursor += sizeof(info.length);
	memcpy(info.certificate, cursor, sizeof(info.certificate));
	result = SwMwSacDeviceSetInfo(sacDevice, &info);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 541);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 547);
}

/* 0x210b: update the manager's cached network state */
extern "C" void SwMwManagerSetNetworkStateCommand(struct SwMwCommChannel *channel,
						 struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 605);
		return;
	}
	result = SwMwManagerSetNetworkState(*device, request->value0, request->value1);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 613);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 619);
}

/* 0x210d: begin enumerating saved resume records */
extern "C" void SwMwManagerStartResumeListCommand(struct SwMwCommChannel *channel,
						 struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 635);
		return;
	}
	result = SwMwManagerStartResumeList(*device, request->value0);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 641);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 648);
}

/* 0x210c: read one resume record from the active list */
extern "C" void SwMwManagerGetResumeDataCommand(struct SwMwCommChannel *channel,
					       struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	struct SwMwMoveRecord record;
	int *device;
	int result;
	int reply;
	int failed;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 664);
		return;
	}
	result = SwMwManagerGetResumeData(*device, request->value0, request->value1, &record);
	request->result = result;
	failed = 0;
	if (result == 12016) {
		failed = 1;
	} else if (result != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 672);
		failed = 1;
	}
	if (failed != 0) {
		request->outSize = 0;
		request->outPayload = NULL;
	} else {
		request->outSize = sizeof(record);
		request->outPayload = &record;
	}
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 681);
}

/* 0x210e: finish enumerating saved resume records */
extern "C" void SwMwManagerEndResumeListCommand(struct SwMwCommChannel *channel,
					       struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 695);
		return;
	}
	result = SwMwManagerEndResumeList(*device, request->value0);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 701);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 708);
}

/* 0x210f: append one resume record */
extern "C" void SwMwManagerAddResumeDataCommand(struct SwMwCommChannel *channel,
					       struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	struct SwMwMoveRecord record;
	const struct SwMwWireResumeRecord *wire;
	int *device;
	int isSink;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 723);
		return;
	}
	isSink = request->value0;
	wire = (const struct SwMwWireResumeRecord *)request->payload;
	if (wire == NULL || request->size < sizeof(*wire)) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 732);
		return;
	}
	record.id = 0;
	record.kind = wire->kind;
	memcpy(&record.label, wire->keyBlock, sizeof(wire->keyBlock));
	record.requested = wire->requested;
	record.size = (int)wire->size;
	if (wire->size != 0)
		memcpy(record.payload, wire->payload, wire->size);
	if (isSink != 0)
		result = SwMwManagerAddSinkResumeData(*device, &record);
	else
		result = SwMwManagerAddSourceResumeData(*device, &record);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 757);
}

/* 0x2110: clear saved sink or source resume records */
extern "C" void SwMwManagerClearResumeDataCommand(struct SwMwCommChannel *channel,
						 struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 770);
		return;
	}
	if (request->value0 != 0)
		result = SwMwManagerClearSinkResumeData(*device);
	else
		result = SwMwManagerClearSourceResumeData(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 787);
}

/* 0x2111: update a move label status */
extern "C" void SwMwManagerNotifyLabelCommand(struct SwMwCommChannel *channel,
					     struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 801);
		return;
	}
	request->result = SwMwManagerNotifyLabel(*device, request->value0, request->value1);
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 814);
}

/* 0x2112: clear all saved move labels */
extern "C" void SwMwManagerClearLabelsCommand(struct SwMwCommChannel *channel,
					     struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 826);
		return;
	}
	request->result = SwMwManagerClearLabels(*device);
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 837);
}

/* 0x2113: the session's status, or 12003 when the request names no stream */
extern "C" void SwMwManagerGetSessionStatus(struct SwMwCommChannel *channel,
					   struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 850);
		return;
	}
	result = SwMwManagerStartActivation();
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 861);
}

/* 0x2114: the session status's value in out0 */
extern "C" void SwMwManagerGetSessionResult(struct SwMwCommChannel *channel,
					   struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int value;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 875);
		return;
	}
	result = SwMwManagerGetActivationResult(&value);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = value;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 887);
}

/* 0x2115: close the session */
extern "C" void SwMwManagerCloseSession(struct SwMwCommChannel *channel,
				       struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 899);
		return;
	}
	result = SwMwManagerCancel();
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 910);
}

/* 0x2116: open the session */
extern "C" void SwMwManagerOpenSession(struct SwMwCommChannel *channel,
				      struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 922);
		return;
	}
	result = SwMwManagerCheckActivation();
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 933);
}

/* 0x2119: initialize the key manager */
extern "C" void SwMwManagerInitKeyCommand(struct SwMwCommChannel *channel,
					 struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int reply;

	(void)argument;
	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 948);
		return;
	}
	request->result = SwMwManagerInitKey(*device);
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 959);
}

/* the manager's command dispatcher */
extern "C" void SwMwManagerCommandDispatch(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	if (channel == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 57);
	if (request == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 58);
	switch (request->command) {
	case 0x2101:
		SwMwManagerConnect(channel, request, argument);
		break;
	case 0x2102:
		SwMwManagerDisconnect(channel, request, argument);
		break;
	case 0x2103:
		SwMwManagerCreateSacDevice(channel, request, argument);
		break;
	case 0x2104:
		SwMwManagerDestroySacDevice(channel, request, argument);
		break;
	case 0x2105:
		SwMwManagerSacSetKey(channel, request, argument);
		break;
	case 0x2106:
		SwMwManagerSacExchangeCert(channel, request, argument);
		break;
	case 0x2107:
		SwMwManagerSacSetCert(channel, request, argument);
		break;
	case 0x2108:
		SwMwManagerSacSetInfo(channel, request, argument);
		break;
	case 0x210a:
		SwMwManagerSetSrmCommand(channel, request, argument);
		break;
	case 0x210b:
		SwMwManagerSetNetworkStateCommand(channel, request, argument);
		break;
	case 0x210c:
		SwMwManagerGetResumeDataCommand(channel, request, argument);
		break;
	case 0x210d:
		SwMwManagerStartResumeListCommand(channel, request, argument);
		break;
	case 0x210e:
		SwMwManagerEndResumeListCommand(channel, request, argument);
		break;
	case 0x210f:
		SwMwManagerAddResumeDataCommand(channel, request, argument);
		break;
	case 0x2110:
		SwMwManagerClearResumeDataCommand(channel, request, argument);
		break;
	case 0x2111:
		SwMwManagerNotifyLabelCommand(channel, request, argument);
		break;
	case 0x2112:
		SwMwManagerClearLabelsCommand(channel, request, argument);
		break;
	case 0x2113:
		SwMwManagerGetSessionStatus(channel, request, argument);
		break;
	case 0x2114:
		SwMwManagerGetSessionResult(channel, request, argument);
		break;
	case 0x2115:
		SwMwManagerCloseSession(channel, request, argument);
		break;
	case 0x2116:
		SwMwManagerOpenSession(channel, request, argument);
		break;
	case 0x2117:
		SwMwManagerWaitReady(channel, request, argument);
		break;
	case 0x2118:
		SwMwManagerDestroy(channel, request, argument);
		break;
	case 0x2119:
		SwMwManagerInitKeyCommand(channel, request, argument);
		break;
	case 0x1101:
		SwMwManagerDeviceConnect(channel, request, argument);
		break;
	default:
		SwMwCommServerReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 147);
		break;
	}
}
