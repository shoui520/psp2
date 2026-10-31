#ifndef IPMI_IPC_NAME_H
#define IPMI_IPC_NAME_H

#include <libkernel/libkernel.h>
#include "assert.h"

/*
 * Names of the kernel objects one client shares with its server (Sony's ipc_name.h): the
 * service name, the client's process ID, its client index and, for the objects a client
 * has several of, the object's index.
 */
namespace IPMI {
namespace impl {

#define IPC_NAME_SIZE	32

static inline void get_name_of_client_request_msg_pipe(char *pName, const char *pServiceName,
						       SceUID pid, SceUInt32 clientIndex)
{
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xRQM%02x", pServiceName, pid, clientIndex);
}

static inline void get_name_of_client_request_counter_semaphore(char *pName,
								const char *pServiceName,
								SceUID pid, SceUInt32 clientIndex)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xRQS%02x", pServiceName, pid, clientIndex);
}

static inline void get_name_of_client_sync_response_msg_pipe(char *pName,
							     const char *pServiceName,
							     SceUID pid, SceUInt32 clientIndex)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xREM%02x", pServiceName, pid, clientIndex);
}

static inline void get_name_of_client_sync_response_counter_semaphore(char *pName,
								      const char *pServiceName,
								      SceUID pid,
								      SceUInt32 clientIndex)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xRES%02x", pServiceName, pid, clientIndex);
}

static inline void get_name_of_client_async_response_msg_pipe(char *pName,
							      const char *pServiceName,
							      SceUID pid, SceUInt32 clientIndex,
							      SceUInt32 index)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xARM%02x%02x", pServiceName, pid,
			clientIndex, index);
}

static inline void get_name_of_client_async_response_counter_semaphore(char *pName,
								       const char *pServiceName,
								       SceUID pid,
								       SceUInt32 clientIndex,
								       SceUInt32 index)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xARS%02x%02x", pServiceName, pid,
			clientIndex, index);
}

static inline void get_name_of_client_event_flag(char *pName, const char *pServiceName,
						 SceUID pid, SceUInt32 clientIndex,
						 SceUInt32 index)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xEVF%02x%02x", pServiceName, pid,
			clientIndex, index);
}

static inline void get_name_of_client_msg_queue_pipe(char *pName, const char *pServiceName,
						     SceUID pid, SceUInt32 clientIndex,
						     SceUInt32 index)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xMQP%02x%02x", pServiceName, pid,
			clientIndex, index);
}

static inline void get_name_of_client_msg_queue_counter(char *pName, const char *pServiceName,
							SceUID pid, SceUInt32 clientIndex,
							SceUInt32 index)
{
	IPMI_ASSERT(clientIndex < 32);
	sceClibSnprintf(pName, IPC_NAME_SIZE, "%s%08xMQC%02x%02x", pServiceName, pid,
			clientIndex, index);
}

}
}

#endif
