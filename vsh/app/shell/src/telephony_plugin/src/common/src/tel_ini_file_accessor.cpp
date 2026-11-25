/*
 * tel_ini_file_accessor.cpp: TelIniFileAccessor and TelApnIniFileAccessor
 * (3.65-CEX: 0x813DD774..0x813DE1E7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "../tel_common.h"

namespace telephony {

namespace {

/* 0x813DDF26 */
static void *iniAllocCallback(SceSize size, void *context)
{
	(void)context;
	return telMalloc(size);
}

/* 0x813DDF2E */
static void iniFreeCallback(void *ptr, void *context)
{
	(void)context;
	telFree(ptr);
}

} /* namespace */

/* 0x813DD774 */
TelIniFileAccessor::TelIniFileAccessor()
	: m_isOpen(false), m_allocatorOps(NULL)
{
}

/* 0x813DD798 */
TelIniFileAccessor::~TelIniFileAccessor()
{
}

/* 0x813DD7DA */
TelephonyResult TelIniFileAccessor::Initialize()
{
	IniConfig config;
	IniAllocatorOps *ops = (IniAllocatorOps *)telMalloc(sizeof(IniAllocatorOps));
	m_allocatorOps = ops;
	if (ops == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);

	ops->alloc = iniAllocCallback;
	ops->free = iniFreeCallback;
	config.ops = ops;
	if (m_processor.initialize(&config) != 0)
		return TelephonyResult(TEL_ERR_FAILED);
	return TelephonyResult(TEL_OK);
}

/* 0x813DD864 */
TelephonyResult TelIniFileAccessor::Terminate()
{
	m_processor.terminate();
	if (m_allocatorOps != NULL) {
		telFree(m_allocatorOps);
		m_allocatorOps = NULL;
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813DD880 */
TelephonyResult TelIniFileAccessor::Open(const paf::string &path, bool writable)
{
	paf::string mode(writable ? "rw" : "r");
	if (m_processor.open(path.c_str(), mode.c_str(), 0) != 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);
	m_isOpen = true;
	return TelephonyResult(TEL_OK);
}

/* 0x813DD944 */
TelephonyResult TelIniFileAccessor::Close()
{
	if (!m_isOpen)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	m_processor.close();
	m_isOpen = false;
	return TelephonyResult(TEL_OK);
}

/* 0x813DD970 */
TelephonyResult TelIniFileAccessor::Flush()
{
	if (!m_isOpen)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	if (m_processor.flush() != 0)
		return TelephonyResult(TEL_ERR_IO_FAILED);
	return TelephonyResult(TEL_OK);
}

/* 0x813DD9AA */
TelephonyResult TelIniFileAccessor::GetString(const paf::string &key, paf::string *outValue)
{
	if (!m_isOpen)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	char valueBuf[256];
	sce_paf_memset(valueBuf, 0, sizeof(valueBuf));
	if (m_processor.get(key.c_str(), valueBuf, sizeof(valueBuf), 0) != 0)
		return TelephonyResult(TEL_ERR_IO_FAILED);

	*outValue = telFormatString("%s", valueBuf);
	return TelephonyResult(TEL_OK);
}

/* 0x813DDA58 */
TelephonyResult TelIniFileAccessor::GetInt(const paf::string &key, int *outValue)
{
	paf::string text;
	TelephonyResult res = GetString(key, &text);
	if (res.GetCode() == TEL_OK)
		*outValue = sce_paf_atoi(text.c_str());
	return res;
}

/* 0x813DDAF0 */
TelephonyResult TelIniFileAccessor::GetFloat(const paf::string &key, float *outValue)
{
	paf::string text;
	TelephonyResult res = GetString(key, &text);
	if (res.GetCode() == TEL_OK)
		*outValue = (float)sce_paf_atof(text.c_str());
	return res;
}

/* 0x813DDB8E */
TelephonyResult TelIniFileAccessor::SetString(const paf::string &key, const paf::string &value)
{
	if (!m_isOpen)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	char existing[256];
	sce_paf_memset(existing, 0, sizeof(existing));
	int ret;
	if (m_processor.get(key.c_str(), existing, sizeof(existing), 0) != 0)
		ret = m_processor.add(key.c_str(), value.c_str());
	else
		ret = m_processor.set(key.c_str(), value.c_str());

	if (ret != 0)
		return TelephonyResult(TEL_ERR_IO_FAILED);
	return TelephonyResult(TEL_OK);
}

/* 0x813DDC36 */
TelephonyResult TelIniFileAccessor::SetInt(const paf::string &key, int value)
{
	paf::string text = telFormatString("%d", value);
	return SetString(key, text);
}

/* 0x813DDC9A */
TelephonyResult TelIniFileAccessor::SetFloat(const paf::string &key, float value)
{
	paf::string text = telFormatString("%f", (double)value);
	return SetString(key, text);
}

/* 0x813DDD06 */
TelephonyResult TelIniFileAccessor::GetNextString(const paf::string &key, paf::string *outValue)
{
	if (!m_isOpen)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	char valueBuf[256];
	sce_paf_memset(valueBuf, 0, sizeof(valueBuf));
	if (m_processor.next((char *)key.c_str(), valueBuf, sizeof(valueBuf)) != 0)
		return TelephonyResult(TEL_ERR_IO_FAILED);

	*outValue = telFormatString("%s", valueBuf);
	return TelephonyResult(TEL_OK);
}

/* 0x813DDDB0 */
TelephonyResult TelIniFileAccessor::GetNextInt(const paf::string &key, int *outValue)
{
	paf::string text;
	TelephonyResult res = GetNextString(key, &text);
	if (res.GetCode() == TEL_OK)
		*outValue = sce_paf_atoi(text.c_str());
	return res;
}

/* 0x813DDE48 */
TelephonyResult TelIniFileAccessor::GetNextFloat(const paf::string &key, float *outValue)
{
	paf::string text;
	TelephonyResult res = GetNextString(key, &text);
	if (res.GetCode() == TEL_OK)
		*outValue = (float)sce_paf_atof(text.c_str());
	return res;
}

/* 0x813DDEE6 */
TelephonyResult TelIniFileAccessor::RemoveKey(const paf::string &key)
{
	if (!m_isOpen)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	if (m_processor.remove(key.c_str(), 0) != 0)
		return TelephonyResult(TEL_ERR_IO_FAILED);
	return TelephonyResult(TEL_OK);
}

/* 0x813DDF36 */
TelApnIniFileAccessor::TelApnIniFileAccessor()
{
	TelIniFileAccessor::Initialize();
}

/* 0x813DE10E */
TelApnIniFileAccessor::~TelApnIniFileAccessor()
{
	TelIniFileAccessor::Flush();
	TelIniFileAccessor::Close();
	TelIniFileAccessor::Terminate();
}

/* 0x813DE106 */
TelephonyResult TelApnIniFileAccessor::Open(const paf::string &path, bool writable)
{
	return TelIniFileAccessor::Open(path, writable);
}

/* 0x813DDFEC */
TelephonyResult TelApnIniFileAccessor::OpenForMcc(SceUInt16 mcc)
{
	int bucketDigit = (int)(mcc / 100U);
	char halfLetter = (((mcc / 10U) & 1U) != 0) ? 'b' : 'a';

	paf::string path = telFormatString("ur0:shell/telephony/tel_apn_list%d%c.ini",
					   bucketDigit, (int)halfLetter);
	TelephonyResult res = TelIniFileAccessor::Open(path, false);
	if (res.GetCode() != TEL_OK) {
		path = telFormatString("vs0:vsh/shell/telephony/apn/tel_apn_list%d%c.ini",
				       bucketDigit, (int)halfLetter);
		res = TelIniFileAccessor::Open(path, false);
	}
	return res;
}

} /* namespace telephony */
