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

LsdbJobClientHolder *g_lsdbJobClientHolder = NULL;		/* 0x8156B860 */

/* 0x811C4BF8 */
void lsdbJobMgrInit(void)
{
	LsdbJobClientHolder *h = (LsdbJobClientHolder *)operator new(sizeof(LsdbJobClientHolder));
	if (h != NULL) {
		g_lsdbJobClientHolder = h;
		void *mem = operator new(4);
		h->client = (mem != NULL) ? SceLsdb_D322A870(mem) : NULL;
	}
}

/* 0x811C4C24 */
void lsdbJobMgrFini(void)
{
	LsdbJobClientHolder *h = g_lsdbJobClientHolder;
	if (h != NULL) {
		if (h->client != NULL) {
			void *p = SceLsdb_56D8DCF0(h->client);
			operator delete(p);
		}
		operator delete(h);
	}
}
