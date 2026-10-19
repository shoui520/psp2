/*
 * SceSblGcAuthMgrPkg: the PSP package signature check, the driver side of
 * _sceSblGcAuthMgrPkgVry. Not written in C yet: the prototype follows npdrm's use
 * (package.c).
 */
#pragma once

int SceSblGcAuthMgrPkgForDriver_E459A9A8(const char *pHash, const char *pSig);
