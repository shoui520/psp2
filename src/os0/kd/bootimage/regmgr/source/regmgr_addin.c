/*
 * regmgr_addin.c: the initial values that are computed when the registry is
 * initialized (REGMGR_INIT_COMPUTED): the add-in functions.
 */

#include "regmgr_internal.h"

#define REGMGR_ADDIN_SIZE	0x7bf	/* the largest value (name unknown) */

/* a token of the console's PS ID (name unknown) */
int regMgrAddinPsIdToken(RegMgrValue *value)
{
	return regMgrMakePsIdToken(value->data);
}

/* a user name, "user" and a number from the time (name unknown) */
int regMgrAddinUserName(RegMgrValue *value)
{
	static const char userName[] = "user";

	snprintf(value->data, value->size, "%s%03d", userName,
		 ksceKernelGetProcessTimeLowCore() % 1000);
	value->size = strnlen(value->data, REGMGR_ADDIN_SIZE) + 1;
	return 0;
}

static int (*const addinFunc[])(RegMgrValue *value) = {
	regMgrAddinUserName,
	regMgrAddinPsIdToken,
};

int regMgrComputeInitValue(int initType, RegMgrValue *value)
{
	int addin = initType ^ REGMGR_INIT_COMPUTED;

	if (addin > 1)
		return SCE_REGMGR_ERROR_BAD_INIT;
	if (value == NULL || value->data == NULL)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	if ((SceUInt32)(value->size - 1) >= REGMGR_ADDIN_SIZE)
		return SCE_REGMGR_ERROR_BAD_SIZE;
	memset(value->data, 0, value->size);
	return addinFunc[addin](value);
}
