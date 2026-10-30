/*
 * DTrace's clients (Sony's dtrace_client.c): a debugger connection or a
 * process's helper DOF, each holding the providers' devices open.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <dtrace/sce/solaris_ddi.h>
#include <sys/list.h>

typedef struct DtraceClientCB {
	list_node_t node;		/* in dtraceClients */
	SceUID processId;
	SceUID moduleId;
	int flags;			/* sceDTraceClientOpen's */
	int token;			/* what the client is known by */
	dev_t dtraceDev;		/* the providers' devices it holds open */
	dev_t sdtDev;
	dev_t profileDev;
	dev_t systraceDev;
	dev_t irqtraceDev;
	dev_t fasttrapDev;
	void *bigData;			/* a reply too big for one packet */
	SceSize bigDataSize;
	SceSize bigDataOffset;		/* what the host has of it */
	int helperDofGen;		/* its helper DOF's generation; -1: none */
} DtraceClientCB;

int dtraceClientInit(void);
int dtraceClientFini(void);
int sceDTraceClientOpen(const char *path, SceUID processId, SceUID moduleId, int flags);
int sceDTraceClientClose(int token);
