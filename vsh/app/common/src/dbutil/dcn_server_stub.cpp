/*
 * The DCN server's method stubs: each takes a method's inputs from its IPMI request, calls the
 * DCNServer function for it and returns its outputs. A post and a get always answer 0; the
 * server function's own result is dropped.
 */
#include <stddef.h>
#include <scetypes.h>
#include <ipmi/ipmi_error.h>
#include "connection.h"
#include "dcn_client.h"
#include "dcn_server.h"

namespace dbutil {

/* Sony's build calls these whole */
static __attribute__((noinline)) int RespondGetEvent(IPMI::Session *session,
						     const IPMI::DataInfo *inList,
						     SceUInt32 inCount, IPMI::BufferInfo *outList,
						     SceUInt32 outCount);
static __attribute__((noinline)) int InvokePostEvent(IPMI::Session *session, SceUID processId,
						     SceBool local, Connection *source, int type,
						     SceInt64 rowid, const char *database,
						     const char *table);
static __attribute__((noinline)) int InvokeGetEvent(IPMI::Session *session, SceUID *processId,
						    SceBool *local, Connection **source, int *type,
						    SceInt64 *rowid, int databaseSize,
						    SceSize *databaseLength, char *database,
						    int tableSize, SceSize *tableLength, char *table);
static __attribute__((noinline)) int InvokeStop(IPMI::Session *session);

/* the nine outputs of DCN_METHOD_GET_EVENT, in the order of DCNClient's GetEvent */
static int RespondGetEvent(IPMI::Session *session, const IPMI::DataInfo *inList,
			   SceUInt32 inCount, IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	const int *sizes = (const int *)inList[0].pData;
	SceSize *databaseLength = (SceSize *)outList[5].pBuffer;
	SceSize *tableLength = (SceSize *)outList[7].pBuffer;
	int ret;

	ret = InvokeGetEvent(session, (SceUID *)outList[0].pBuffer, (SceBool *)outList[1].pBuffer,
			     (Connection **)outList[2].pBuffer, (int *)outList[3].pBuffer,
			     (SceInt64 *)outList[4].pBuffer, sizes[0], databaseLength,
			     (char *)outList[6].pBuffer, sizes[1], tableLength,
			     (char *)outList[8].pBuffer);
	outList[0].outSize = sizeof(SceUID);
	outList[1].outSize = sizeof(SceBool);
	outList[2].outSize = sizeof(Connection *);
	outList[3].outSize = sizeof(int);
	outList[4].outSize = sizeof(SceInt64);
	outList[5].outSize = sizeof(SceSize);
	outList[6].outSize = *databaseLength;
	outList[7].outSize = sizeof(SceSize);
	outList[8].outSize = *tableLength;
	return session->respondToSyncMethodRequest(ret, outList, outCount);
}

int DCNDispatchSyncMethod(IPMI::Session *session, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount,
			  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	const DCNEventHeader *header;
	int ret;

	switch (methodId) {
	case DCN_METHOD_POST_EVENT:
		header = (const DCNEventHeader *)inList[0].pData;
		ret = InvokePostEvent(session, header->processId, header->local, header->source,
				      header->type, header->rowid, (const char *)inList[1].pData,
				      (const char *)inList[2].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	case DCN_METHOD_GET_EVENT:
		return RespondGetEvent(session, inList, inCount, outList, outCount);
	case DCN_METHOD_STOP:
		ret = InvokeStop(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	return IPMI_ERROR_UNKNOWN_METHOD;
}

/* the server has no asynchronous methods */
int DCNDispatchAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	return IPMI_ERROR_UNKNOWN_METHOD;
}

static int InvokePostEvent(IPMI::Session *session, SceUID processId, SceBool local,
			   Connection *source, int type, SceInt64 rowid, const char *database,
			   const char *table)
{
	DCNServer::PostEvent(session, processId, local, source, type, rowid, database, table);
	return 0;
}

static int InvokeGetEvent(IPMI::Session *session, SceUID *processId, SceBool *local,
			  Connection **source, int *type, SceInt64 *rowid, int databaseSize,
			  SceSize *databaseLength, char *database, int tableSize,
			  SceSize *tableLength, char *table)
{
	DCNServer::GetEvent(session, processId, local, source, type, rowid, databaseSize,
			    databaseLength, database, tableSize, tableLength, table);
	return 0;
}

static int InvokeStop(IPMI::Session *session)
{
	return DCNServer::StopSession(session);
}

}
