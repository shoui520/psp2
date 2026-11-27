/*
 * SceShell lsdb_job: LiveSpace DB job manager singleton
 * 3.65-CEX: 0x811C4BF8 - 0x811C4CB8 (sti 0x811C4C46)
 */

#include <stddef.h>
#include <scetypes.h>

#include "../../livespace_db/lsdb_keys.h"

extern "C" {
extern void *SceLsdb_D322A870(void *mem);
extern void *SceLsdb_56D8DCF0(void *obj);
}

struct LsdbJobClientHolder {
	void *client;
};

LsdbJobClientHolder *g_shellLsdbClient = NULL;		/* 0x8156B860 */

/* 0x811C4BF8 */
int shellLsdbClientCreate(void)
{
	LsdbJobClientHolder *holder =
		(LsdbJobClientHolder *)operator new(sizeof(LsdbJobClientHolder));
	if (holder != NULL) {
		g_shellLsdbClient = holder;
		void *mem = operator new(4);
		holder->client = (mem != NULL) ? SceLsdb_D322A870(mem) : NULL;
	}
	return 0;
}

void lsdbJobMgrInit(void)
{
	shellLsdbClientCreate();
}

/* 0x811C4C24 */
void shellLsdbClientDestroy(void)
{
	LsdbJobClientHolder *holder = g_shellLsdbClient;
	if (holder != NULL) {
		if (holder->client != NULL) {
			void *clientMem = SceLsdb_56D8DCF0(holder->client);
			operator delete(clientMem);
		}
		operator delete(holder);
	}
}

void lsdbJobMgrFini(void)
{
	shellLsdbClientDestroy();
}
