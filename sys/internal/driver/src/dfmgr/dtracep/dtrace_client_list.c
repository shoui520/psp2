/*
 * dtrace_client_list.c: the list of DTrace's clients, each a debugger
 * connection or a process's helper DOF.
 */
#include <dtrace/heap.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include "dtrace_client_list.h"

list_t *dtraceCreateClientList(void)
{
	list_t *list;

	list = dtrace_heap_alloc(sizeof(*list));
	if (list == NULL) {
		SCE_DBG_ASSERT_FAIL(21);
		return NULL;
	}
	memset(list, 0, sizeof(*list));
	list_create(list, sizeof(DtraceClientCB), offsetof(DtraceClientCB, node));
	return list;
}

DtraceClientCB *dtraceAllocClient(dev_t dev, SceUID processId, SceUID moduleId, int flags,
				  int token)
{
	DtraceClientCB *client;

	SCE_DBG_ASSERT_CHECK(dev != 0, 34);

	client = dtrace_heap_alloc(sizeof(*client));
	if (client == NULL) {
		SCE_DBG_ASSERT_FAIL(38);
		return NULL;
	}
	memset(client, 0, sizeof(*client));
	client->processId = processId;
	client->moduleId = moduleId;
	client->flags = flags;
	client->token = token;
	client->dtraceDev = makedevice(getmajor(dev), 0);
	client->helperDofGen = -1;
	return client;
}

DtraceClientCB *dtraceFindClientByToken(list_t *list, int token)
{
	DtraceClientCB *client;

	SCE_DBG_ASSERT_CHECK(list != NULL, 60);

	for (client = list_head(list); client != NULL; client = list_next(list, client)) {
		if (client->token == token)
			return client;
	}
	return NULL;
}

DtraceClientCB *dtraceFindClientByProcessId(list_t *list, SceUID processId)
{
	DtraceClientCB *client;

	SCE_DBG_ASSERT_CHECK(list != NULL, 74);

	for (client = list_head(list); client != NULL; client = list_next(list, client)) {
		if (client->processId == processId)
			return client;
	}
	return NULL;
}

DtraceClientCB *dtraceFindClientByModuleId(list_t *list, SceUID processId, SceUID moduleId)
{
	DtraceClientCB *client;

	SCE_DBG_ASSERT_CHECK(list != NULL, 88);

	for (client = list_head(list); client != NULL; client = list_next(list, client)) {
		if (client->processId == processId && client->moduleId == moduleId)
			return client;
	}
	return NULL;
}

int dtraceIsClientListEmpty(list_t *list)
{
	return list_is_empty(list);
}

void dtraceAddClient(list_t *list, DtraceClientCB *client)
{
	list_insert_tail(list, client);
}

void dtraceRemoveClient(list_t *list, DtraceClientCB *client)
{
	list_remove(list, client);
}

void dtraceFreeClient(DtraceClientCB *client)
{
	SCE_DBG_ASSERT_CHECK(client != NULL, 117);

	if (client->buffer != NULL) {
		dtrace_heap_free(client->buffer);
		client->buffer = NULL;
	}
	dtrace_heap_free(client);
}

void dtraceDestroyClientList(list_t *list)
{
	DtraceClientCB *client;

	SCE_DBG_ASSERT_CHECK(list != NULL, 131);

	while ((client = list_head(list)) != NULL) {
		list_remove(list, client);
		dtraceFreeClient(client);
	}
	list_destroy(list);
	dtrace_heap_free(list);
}
