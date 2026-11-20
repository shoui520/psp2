/*
 * console_info_ns.cpp: the "ns" record (0x810E821C-0x810E8440): one entry of the console's
 * identity, its system software, model, product code, Open PSID, telephony, network flags and
 * IDU mode, and telephony_plugin's restriction its "nd" field gives. The file's and the names are
 * ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <plugin.h>
#include <ss_mgr/ss_mgr.h>
#include <vshbridge/vshbridge.h>
#include "plugins.h"
#include "console_info.h"

using namespace paf;

/* telephony_plugin not there (PLACEHOLDER name) */
#define CONSOLE_INFO_ERROR_NO_TELEPHONY		-42

/* the flags of ur0:ci/file the "nc" field gives */
#define CONSOLE_INFO_NS_FLAG_INDEX_1	1
#define CONSOLE_INFO_NS_FLAG_INDEX_2	2

static int consoleInfoNsFormatModel(const ConsoleInfoRecord::Field *field, char *buf, int *len);
static int consoleInfoNsFormatNetwork(const ConsoleInfoRecord::Field *field, char *buf, int *len);
static int consoleInfoNsFormatIdu(const ConsoleInfoRecord::Field *field, char *buf, int *len);

/* (0x815444DC) the entry */
static const ConsoleInfoRecord::Field s_nsFields[] = {
	{ "ver",		8,	consoleInfoFormatSystemVersion },
	{ "model",		4,	consoleInfoNsFormatModel },
	{ "productcode",	4,	consoleInfoFormatPsCode },
	{ "id",			32,	consoleInfoFormatOpenPsId },
	{ "nd",			1,	consoleInfoFormatTelephony },
	{ "nc",			1,	consoleInfoNsFormatNetwork },
	{ "idu",		1,	consoleInfoNsFormatIdu },
};

/* (0x810E821C) */
ConsoleInfoNs::ConsoleInfoNs(int entrySize)
	: ConsoleInfoRecord(CONSOLE_INFO_TYPE_NS, entrySize)
{
}

/* (0x810E8238, 0x810E8258) the record's folder removed */
ConsoleInfoNs::~ConsoleInfoNs()
{
	RemoveTempDir();
}

/* (0x810E827C) the entry, its header only */
int ConsoleInfoNs::Write(void *data)
{
	int ret = OpenNewEntry();

	if (ret != 0)
		return ret;
	ret = WriteHeader();
	CloseEntry();
	*(int *)data = m_count;
	return ret;
}

/* (0x81409E2C: one "return 0" the build shares) */
int ConsoleInfoNs::Commit()
{
	return 0;
}

/* (0x810E82A4) */
const ConsoleInfoRecord::Field *ConsoleInfoNs::GetFields()
{
	return s_nsFields;
}

/* (0x81010C24: a "return 7" the build shares) */
unsigned int ConsoleInfoNs::GetFieldCount()
{
	return sizeof(s_nsFields) / sizeof(s_nsFields[0]);
}

/* (0x810E82AE) */
static int consoleInfoNsFormatModel(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	ScePsCode code;

	sce_paf_memset(&code, 0, sizeof(code));

	int ret = vshSblAimgrGetPscode(&code);

	if (ret != 0)
		return ret;
	sce_paf_snprintf(buf, 5, "%04x", code.product_sub_code);
	*len = 4;
	return 0;
}

/* (0x810E8306) */
static int consoleInfoNsFormatNetwork(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	SceUInt8 flag1 = 0;

	consoleInfoFileReadFlag(CONSOLE_INFO_NS_FLAG_INDEX_1, &flag1, 0);

	SceUInt8 flag2 = 0;

	consoleInfoFileReadFlag(CONSOLE_INFO_NS_FLAG_INDEX_2, &flag2, 0);
	if (flag2 == 1 && flag1 == 1)
		buf[0] = '1';
	else if (flag2 == 2 && flag1 == 1)
		buf[0] = '2';
	else if (flag2 == 1 && flag1 == 3)
		buf[0] = '3';
	else if (flag2 == 3 && flag1 == 1)
		buf[0] = '4';
	else if (flag2 == 3 && flag1 == 3)
		buf[0] = '5';
	else
		buf[0] = '0';
	buf[1] = '\0';
	*len = 1;
	return 0;
}

/* (0x810E83A2) */
static int consoleInfoNsFormatIdu(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	int idu = vshSysconIsIduMode();

	if (idu < 0)
		buf[0] = '0';
	else if (idu != 0)
		buf[0] = '1';
	else
		buf[0] = '0';
	buf[1] = '\0';
	*len = 1;
	return 0;
}

/* (0x810E83C8) the service's restriction */
int consoleInfoGetTelephony(int *restriction)
{
	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyService *service = iface->getService();

	if (service == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	int ret = service->vtable->getRestriction(service, restriction).GetCode();

	if (ret < 0)
		return ret;
	return 0;
}
