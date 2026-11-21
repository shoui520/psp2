/*
 * system_update_registry.h: the system update's settings (system_update_registry.cpp): the
 * console's language and region, the update list's url, whether the download may use the
 * phone line, and what removes a file for the system update's own process. Every name is ours
 * (PLACEHOLDER) until a record or string names it.
 */
#ifndef _SYSTEM_UPDATE_REGISTRY_H
#define _SYSTEM_UPDATE_REGISTRY_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include "../../../../lowlayer/src/ipmi/client.h"
#include <system_update/system_update_job.h>

/* the console's system software version, as Bgdl_8137C396 gives it (PLACEHOLDER names) */
struct SystemUpdateVersionInfo {
	int version;
	int reserved;
	int flag;			/* 0: the registry's server may be used */
	int reserved2;
};

/* what removes a file for the system update's own process (PLACEHOLDER) */
class SystemUpdateClient {
public:
	int Bgdl_81133156(const char *path);

	IPMI::Client *m_client;
};

SystemUpdateResult Bgdl_81132B02(int type, int *value, bool usePscode);
int Bgdl_81132BAE(const char *path, paf::string *str);
paf::string Bgdl_81132CDC(void);
SystemUpdateResult Bgdl_81132EDA(int type, paf::string *url);
int Bgdl_811330A6(void);
int Bgdl_811330BA(SceInt64 size);

/* other files' */
int Bgdl_8126E1AE(char *url);
int Bgdl_8126E216(int kind);
int Bgdl_8126E252(int kind, SceUInt64 *size);
int Bgdl_811361D4(paf::string *region);
int Bgdl_8137C396(SystemUpdateVersionInfo *info);

#endif
