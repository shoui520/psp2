/*
 * system_update_version.h: the system software version and the console's region as the system
 * update needs them (system_update_version.cpp, a file of ours: no record or string names
 * Sony's). Every name is ours (PLACEHOLDER).
 */
#ifndef _SYSTEM_UPDATE_VERSION_H
#define _SYSTEM_UPDATE_VERSION_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>

int Bgdl_81135E8C(paf::string version);				/* a version's number, -1 */
paf::string Bgdl_811360AA(void);				/* the version, "%x.%03x" */
paf::wstring Bgdl_8113614A(void);				/* the same, in UTF-16 */
int Bgdl_811361A0(int *productCode);				/* the console's product code */
int Bgdl_811361D4(paf::string *region);				/* its CDN region */
void Bgdl_811363D6(SceInt64 size);				/* the update's size */
void Bgdl_811363E4(bool enabled, const char *file, int line, const char *fmt, ...);

#endif
