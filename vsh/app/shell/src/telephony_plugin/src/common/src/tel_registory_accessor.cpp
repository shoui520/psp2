/*
 * tel_registory_accessor.cpp: SceRegistryMgr user accessor wrapper
 * (3.65-CEX: 0x813E2590..0x813E284B).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include "../tel_common.h"

extern "C" {
extern int sceRegMgrGetKeyIntForUser(const char *category, const char *name, int *buf);
extern int sceRegMgrGetKeyStrForUser(const char *category, const char *name, char *buf,
				     SceSize size);
extern int sceRegMgrGetKeyBinForUser(const char *category, const char *name, void *buf,
				     SceSize size);
extern int sceRegMgrSetKeyIntForUser(const char *category, const char *name, int buf);
extern int sceRegMgrSetKeyStrForUser(const char *category, const char *name, const char *buf,
				     SceSize size);
extern int sceRegMgrSetKeyBinForUser(const char *category, const char *name, const void *buf,
				     SceSize size);
}

extern void shellRegMgrLogError(int err, int flag);		/* 0x81345038 */

namespace telephony {

/* 0x813E2590 */
TelRegistoryAccessor::TelRegistoryAccessor()
	: m_category("")
{
}

/* 0x813E25AC */
TelRegistoryAccessor::~TelRegistoryAccessor()
{
}

/* 0x813E260E */
TelephonyResult TelRegistoryAccessor::Open(const paf::string &category)
{
	m_category = category;
	return TelephonyResult(TEL_OK);
}

/* 0x813E2622 */
TelephonyResult TelRegistoryAccessor::Close()
{
	m_category = "";
	return TelephonyResult(TEL_OK);
}

/* 0x813E2638 */
TelephonyResult TelRegistoryAccessor::GetInt(const paf::string &key, int *outValue)
{
	int err = sceRegMgrGetKeyIntForUser(m_category.c_str(), key.c_str(), outValue);
	if (err != 0) {
		shellRegMgrLogError(err, 1);
		return TelephonyResult(TEL_ERR_FAILED);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E2670 */
TelephonyResult TelRegistoryAccessor::GetFloat(const paf::string &key, float *outValue)
{
	return GetBinary(key, outValue, sizeof(float));
}

/* 0x813E2682 */
TelephonyResult TelRegistoryAccessor::GetString(const paf::string &key, paf::string *outValue)
{
	char *buf = (char *)telMalloc(256);
	if (buf == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);

	sce_paf_memset(buf, 0, 256);
	int err = sceRegMgrGetKeyStrForUser(m_category.c_str(), key.c_str(), buf, 256);
	TelephonyResult res(TEL_OK);
	if (err != 0) {
		shellRegMgrLogError(err, 1);
		res = TelephonyResult(TEL_ERR_FAILED);
	} else {
		*outValue = telFormatString("%s", buf);
	}
	telFree(buf);
	return res;
}

/* 0x813E274A */
TelephonyResult TelRegistoryAccessor::GetBinary(const paf::string &key, void *outBuf, SceSize size)
{
	int err = sceRegMgrGetKeyBinForUser(m_category.c_str(), key.c_str(), outBuf, size);
	if (err != 0) {
		shellRegMgrLogError(err, 1);
		return TelephonyResult(TEL_ERR_FAILED);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E2786 */
TelephonyResult TelRegistoryAccessor::SetInt(const paf::string &key, int value)
{
	int err = sceRegMgrSetKeyIntForUser(m_category.c_str(), key.c_str(), value);
	if (err != 0) {
		shellRegMgrLogError(err, 1);
		return TelephonyResult(TEL_ERR_FAILED);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E27BE */
TelephonyResult TelRegistoryAccessor::SetFloat(const paf::string &key, float value)
{
	return SetBinary(key, &value, sizeof(float));
}

/* 0x813E27D0 */
TelephonyResult TelRegistoryAccessor::SetString(const paf::string &key, const paf::string &value,
						SceSize maxSize)
{
	int err = sceRegMgrSetKeyStrForUser(m_category.c_str(), key.c_str(), value.c_str(),
					    maxSize);
	if (err != 0) {
		shellRegMgrLogError(err, 1);
		return TelephonyResult(TEL_ERR_FAILED);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E280E */
TelephonyResult TelRegistoryAccessor::SetBinary(const paf::string &key, const void *buf,
						SceSize size)
{
	int err = sceRegMgrSetKeyBinForUser(m_category.c_str(), key.c_str(), buf, size);
	if (err != 0) {
		shellRegMgrLogError(err, 1);
		return TelephonyResult(TEL_ERR_FAILED);
	}
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
