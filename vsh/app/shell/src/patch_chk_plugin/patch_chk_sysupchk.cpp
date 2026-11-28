/*
 * patch_chk_sysupchk.cpp: launches and unloads sysupchk_plugin when a game
 * patch requires a newer system software version (0x812AFD1C-0x812AFE90).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "../../sysupchk.h"
#include "src/livespace_db/lsdb_keys.h"
#include "patch_chk_internal.h"

using namespace paf;

/* Constructor (0x812AFD1C) */
PatchChkSysUpChk::PatchChkSysUpChk()
	: m_checkMode(0),
	  m_dialogMode(0),
	  m_result(0),
	  m_callback(NULL),
	  m_userData(NULL)
{
}

/* Destructor (0x812AFD2C) */
PatchChkSysUpChk::~PatchChkSysUpChk()
{
	m_dialogMode = 0;
	m_result = 0;
	m_callback = NULL;
	m_userData = NULL;
}

/* Configures ShellSysupchkParam and loads sysupchk_plugin asynchronously (0x812AFD38) */
void PatchChkSysUpChk::StartSysUpChkPlugin()
{
	ShellSysupchkParam param;
	param.caller = "__main__";
	param.callback = OnSysUpChkFinished;
	param.reserved = (void *)OnSysUpChkUnloaded;
	param.arg = this;
	param.checkMode = 1;
	param.dialogMode = 3;
	param.launchMode = 1;
	param.acceptedFlag = (m_checkMode == 1) ? 1 : 0;
	param.npCheckLevel = 1;

	ShellSysupchkInitParam initParam;
	initParam.SetParam(param);
	initParam.Load(NULL, 0);
}

/* Begins system update check if dialogMode == 1 (0x812AFDFA) */
int PatchChkSysUpChk::Launch(int checkMode, int dialogMode,
			     SysUpChkCallback callback, void *userData)
{
	if (callback == NULL)
		return PATCH_CHK_ERROR_INVALID_ARGUMENT;

	m_callback = callback;
	m_userData = userData;
	m_checkMode = checkMode;
	m_dialogMode = dialogMode;
	if (dialogMode == 1) {
		StartSysUpChkPlugin();
		return 0;
	}
	return PATCH_CHK_ERROR_INVALID_ARGUMENT;
}

/* Unloads sysupchk_plugin if currently active (0x812AFE24) */
void PatchChkSysUpChk::Unload()
{
	if (m_dialogMode == 1 && Plugin::Find("sysupchk_plugin") != NULL)
		Plugin::UnloadAsync("sysupchk_plugin", NULL, false);
}

/* Callback fired once sysupchk_plugin finishes unloading (0x812AFE4E) */
void PatchChkSysUpChk::OnSysUpChkUnloaded(void *userData)
{
	PatchChkSysUpChk *self = (PatchChkSysUpChk *)userData;
	if (self != NULL && self->m_callback != NULL)
		self->m_callback(self->m_result, self->m_userData);
}

/* Callback fired when sysupchk_plugin reports its result code (0x812AFE62) */
void PatchChkSysUpChk::OnSysUpChkFinished(int result, void *userData)
{
	PatchChkSysUpChk *self = (PatchChkSysUpChk *)userData;
	int outErr = PATCH_CHK_ERROR_USER_CANCELED;
	if (self->m_checkMode != 1 && (result & 0xF0) == 0x10)
		outErr = 0;
	self->m_result = outErr;
	Plugin::UnloadAsync("sysupchk_plugin", NULL, false);
}
