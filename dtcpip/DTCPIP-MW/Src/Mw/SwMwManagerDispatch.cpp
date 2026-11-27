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
int SwMwManagerSessionConnect(int *device, const int *values, void *payload, int size);
int SwMwManagerSessionDisconnect(int device);
int SwMwManagerSessionStatus(int device);
int SwMwManagerSessionResult(int device, int *out);
int SwMwManagerSessionClose(int device);
int SwMwManagerSessionOpen(int device);
int SwMwManagerSessionWaitReady(int device, int *out);
int SwMwManagerSessionDestroy(int device);
int SwMwManagerConnectDevice(int device);
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
extern "C" int SwMwManagerConnect(struct SwMwCommChannel *channel, struct SwMwIpcMessage *request,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 162);
	}
	if (*device != 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 168);
	}
	values[0] = request->value0;
	values[1] = request->value1;
	values[2] = request->value2;
	values[3] = request->value3;
	result = SwMwManagerSessionConnect(device, values, request->payload, request->size);
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
	return 0;
}

/* 0x2102: disconnect the manager */
extern "C" int SwMwManagerDisconnect(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 206);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 212);
	}
	result = SwMwManagerSessionDisconnect(*device);
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
	return 0;
}

/* 0x2117: wait for the session to be ready */
extern "C" int SwMwManagerWaitReady(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 241);
	}
	result = SwMwManagerSessionWaitReady(*device, &value);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = value;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 253);
	return 0;
}

/* 0x2118: destroy the session */
extern "C" int SwMwManagerDestroy(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 266);
	}
	result = SwMwManagerSessionDestroy(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 277);
	return 0;
}

/* 0x210a: update the manager's SRM data */
extern "C" int SwMwManagerSetSrmCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 290);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 296);
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
	return 0;
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
	result = SwMwManagerConnectDevice(*device);
	if (result != 0)
		return swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 326);
	return result;
}

/* 0x2103: create a SAC device handle */
extern "C" int SwMwManagerCreateSacDevice(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 338);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 344);
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
	return 0;
}

/* 0x2104: destroy a SAC device handle */
extern "C" int SwMwManagerDestroySacDevice(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 370);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 376);
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
	return 0;
}

/* 0x2105: obtain the 4-byte key from a SAC device */
extern "C" int SwMwManagerSacSetKey(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 403);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 409);
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
	return 0;
}

/* 0x2106: exchange a 32-byte certificate pair on a SAC device */
extern "C" int SwMwManagerSacExchangeCert(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 437);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 443);
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
	return 0;
}

/* 0x2107: hand a 32-byte certificate to a SAC device */
extern "C" int SwMwManagerSacSetCert(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 473);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 479);
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
	return 0;
}

/* 0x2108: store sender/receiver ids, length and certificate on a SAC device */
extern "C" int SwMwManagerSacSetInfo(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 511);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 517);
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
	return 0;
}

/* 0x210b: update the manager's cached network state */
extern "C" int SwMwManagerSetNetworkStateCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 605);
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
	return 0;
}

/* 0x210d: begin enumerating saved resume records */
extern "C" int SwMwManagerStartResumeListCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 635);
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
	return 0;
}

/* 0x210c: read one resume record from the active list */
extern "C" int SwMwManagerGetResumeDataCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 664);
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
	return 0;
}

/* 0x210e: finish enumerating saved resume records */
extern "C" int SwMwManagerEndResumeListCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 695);
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
	return 0;
}

/* 0x210f: append one resume record */
extern "C" int SwMwManagerAddResumeDataCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 723);
	}
	isSink = request->value0;
	wire = (const struct SwMwWireResumeRecord *)request->payload;
	if (wire == NULL || request->size < sizeof(*wire)) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 732);
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
	return 0;
}

/* 0x2110: clear saved sink or source resume records */
extern "C" int SwMwManagerClearResumeDataCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 770);
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
	return 0;
}

/* 0x2111: update a move label status */
extern "C" int SwMwManagerNotifyLabelCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 801);
	}
	request->result = SwMwManagerNotifyLabel(*device, request->value0, request->value1);
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 814);
	return 0;
}

/* 0x2112: clear all saved move labels */
extern "C" int SwMwManagerClearLabelsCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 826);
	}
	request->result = SwMwManagerClearLabels(*device);
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 837);
	return 0;
}

/* 0x2113: the session's status, or 12003 when the request names no stream */
extern "C" int SwMwManagerGetSessionStatus(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 850);
	}
	result = SwMwManagerSessionStatus(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 861);
	return 0;
}

/* 0x2114: the session status's value in out0 */
extern "C" int SwMwManagerGetSessionResult(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 875);
	}
	result = SwMwManagerSessionResult(*device, &value);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = value;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 887);
	return 0;
}

/* 0x2115: close the session */
extern "C" int SwMwManagerCloseSession(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 899);
	}
	result = SwMwManagerSessionClose(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 910);
	return 0;
}

/* 0x2116: open the session */
extern "C" int SwMwManagerOpenSession(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 922);
	}
	result = SwMwManagerSessionOpen(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 933);
	return 0;
}

/* 0x2119: initialize the key manager */
extern "C" int SwMwManagerInitKeyCommand(struct SwMwCommChannel *channel,
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
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 948);
	}
	request->result = SwMwManagerInitKey(*device);
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 959);
	return 0;
}

/* the manager's command dispatcher */
extern "C" int SwMwManagerCommandDispatch(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	if (channel == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 57);
	if (request == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 58);
	switch (request->command) {
	case 0x2101:
		return SwMwManagerConnect(channel, request, argument);
	case 0x2102:
		return SwMwManagerDisconnect(channel, request, argument);
	case 0x2103:
		return SwMwManagerCreateSacDevice(channel, request, argument);
	case 0x2104:
		return SwMwManagerDestroySacDevice(channel, request, argument);
	case 0x2105:
		return SwMwManagerSacSetKey(channel, request, argument);
	case 0x2106:
		return SwMwManagerSacExchangeCert(channel, request, argument);
	case 0x2107:
		return SwMwManagerSacSetCert(channel, request, argument);
	case 0x2108:
		return SwMwManagerSacSetInfo(channel, request, argument);
	case 0x210a:
		return SwMwManagerSetSrmCommand(channel, request, argument);
	case 0x210b:
		return SwMwManagerSetNetworkStateCommand(channel, request, argument);
	case 0x210c:
		return SwMwManagerGetResumeDataCommand(channel, request, argument);
	case 0x210d:
		return SwMwManagerStartResumeListCommand(channel, request, argument);
	case 0x210e:
		return SwMwManagerEndResumeListCommand(channel, request, argument);
	case 0x210f:
		return SwMwManagerAddResumeDataCommand(channel, request, argument);
	case 0x2110:
		return SwMwManagerClearResumeDataCommand(channel, request, argument);
	case 0x2111:
		return SwMwManagerNotifyLabelCommand(channel, request, argument);
	case 0x2112:
		return SwMwManagerClearLabelsCommand(channel, request, argument);
	case 0x2113:
		return SwMwManagerGetSessionStatus(channel, request, argument);
	case 0x2114:
		return SwMwManagerGetSessionResult(channel, request, argument);
	case 0x2115:
		return SwMwManagerCloseSession(channel, request, argument);
	case 0x2116:
		return SwMwManagerOpenSession(channel, request, argument);
	case 0x2117:
		return SwMwManagerWaitReady(channel, request, argument);
	case 0x2118:
		return SwMwManagerDestroy(channel, request, argument);
	case 0x2119:
		return SwMwManagerInitKeyCommand(channel, request, argument);
	case 0x1101:
		return SwMwManagerDeviceConnect(channel, request, argument);
	default:
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 147);
	}
}
