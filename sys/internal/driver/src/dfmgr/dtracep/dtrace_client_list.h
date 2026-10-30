/*
 * The list of DTrace's clients (Sony's dtrace_client_list.c).
 */
#pragma once

#include "dtrace_client.h"

extern list_t *dtraceClients;

list_t *dtraceCreateClientList(void);
DtraceClientCB *dtraceAllocClient(dev_info_t *devi, SceUID processId, SceUID moduleId, int flags,
				  int token);
DtraceClientCB *dtraceFindClientByToken(list_t *list, int token);
DtraceClientCB *dtraceFindClientByProcessId(list_t *list, SceUID processId);
DtraceClientCB *dtraceFindClientByModuleId(list_t *list, SceUID processId, SceUID moduleId);
int dtraceIsClientListEmpty(list_t *list);
void dtraceAddClient(list_t *list, DtraceClientCB *client);
void dtraceRemoveClient(list_t *list, DtraceClientCB *client);
void dtraceFreeClient(DtraceClientCB *client);
void dtraceDestroyClientList(list_t *list);
