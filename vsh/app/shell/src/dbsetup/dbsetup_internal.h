/*
 * dbsetup_internal.h: internal interface shared between dbsetup_lib,
 * dbrecovery_lib, and id_dat (0x8135F470..0x81362107).
 */
#ifndef SHELL_DBSETUP_INTERNAL_H
#define SHELL_DBSETUP_INTERNAL_H

#include <scetypes.h>
#include <std/string>
#include "../../console_info/console_info.h"
#include "../np/manager/np_manager_core.h"

namespace dbutil {
class Connection;
struct Value;
}

#define DBCONFIG_ERR_INTERNAL		-2146404864	/* 0x80107600 */
#define DBCONFIG_ERR_NOT_FOUND		-2146404863	/* 0x80107601 */

/* Path resolver in dbsetup_lib.cpp (0x813601A8) */
int dbSetupResolvePath(int dbId, char *outPath, SceSize outLen);

/* tbl_config helpers in dbrecovery_lib.cpp (0x81361196..0x813613E3) */
int dbConfigSetValue(dbutil::Connection *conn, int key, const dbutil::Value *value);
int dbConfigSetInt(dbutil::Connection *conn, int key, int value);
int dbConfigGetValue(dbutil::Connection *conn, int key, dbutil::Value *outValue);
int dbConfigGetInt(dbutil::Connection *conn, int key, int *outValue);

/* dbrecovery helpers (0x81360432, 0x813607BE, 0x81360836) */
int DbCheckGetMemoryCardStatus(void);
int DbCheckSetCorruptByPath(const char *path, int writeMagic);
int DbCheckClearCorruptByPath(const char *path);

/* ux0:id.dat helpers in id_dat.cpp (0x813613E4..0x81362107) */
bool shellIdDatIsAccountBoundMode(void);
int shellIdDatReadFromPath(ShellActivationInfo *outInfo, const char *devicePrefix);
int shellIdDatWriteEx(const NpIdDatUpdateInfo *info, SceUInt32 fieldMask,
		      SceUInt8 forceWrite, const char *devicePrefix,
		      SceUInt8 writeInvalidDigest);
int shellIdDatWrite(const NpIdDatUpdateInfo *info, SceUInt32 fieldMask,
		    SceUInt8 forceWrite, int unusedDevicePrefix);
void shellIdDatGenerateMid(paf::string *outMid);
int shellIdDatGenerateDid(paf::string *outDid);

#endif
