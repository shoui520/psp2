/*
 * tel_ini_file_attribute_base.cpp: INI-backed cached attribute store base
 * (3.65-CEX: 0x813DE1E8..0x813DF26F).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../tel_common.h"

namespace telephony {

/* 0x813DE1E8 */
TelIniFileAttributeBase::TelIniFileAttributeBase()
	: m_mutex("SceTelCmnIniFileAttribute", 0)
{
}

/* 0x813DE242 */
TelephonyResult TelIniFileAttributeBase::Initialize()
{
	return m_accessor.Initialize();
}

/* 0x813DE24C */
TelephonyResult TelIniFileAttributeBase::Terminate()
{
	TelephonyResult res = m_accessor.Terminate();
	m_cache.clear();
	return res;
}

/* 0x813DE292 */
TelephonyResult TelIniFileAttributeBase::SetAttribute(const TelAttributeValue &attr)
{
	const TelAttributeEntry *entry = FindEntry(attr.attributeId);
	if (entry == NULL)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_mutex.Lock();
	paf::string filePath(entry->categoryOrPath);
	TelephonyResult res = m_accessor.Open(filePath, true);
	if (res.GetCode() != TEL_OK) {
		m_mutex.Unlock();
		return res;
	}

	paf::string keyName(entry->keyName);
	if (entry->storageType == TEL_ATTR_STORE_INT)
		res = m_accessor.SetInt(keyName, attr.intValue);
	else if (entry->storageType == TEL_ATTR_STORE_FLOAT)
		res = m_accessor.SetFloat(keyName, attr.floatValue);
	else if (entry->storageType == TEL_ATTR_STORE_STRING)
		res = m_accessor.SetString(keyName, attr.stringValue);
	else
		res = TelephonyResult(TEL_ERR_IO_FAILED);

	if (res.GetCode() != TEL_OK) {
		m_accessor.Close();
	} else {
		m_accessor.Flush();
		res = m_accessor.Close();
	}

	if (res.GetCode() == TEL_OK) {
		m_cache[attr.attributeId] = attr;
	}

	m_mutex.Unlock();
	return res;
}

/* 0x813DE600 */
TelephonyResult TelIniFileAttributeBase::GetAttribute(TelAttributeValue *outAttr)
{
	m_mutex.Lock();
	paf::map<int, TelAttributeValue>::iterator cached = m_cache.find(outAttr->attributeId);
	if (cached != m_cache.end()) {
		*outAttr = cached->second;
		m_mutex.Unlock();
		return TelephonyResult(TEL_OK);
	}

	const TelAttributeEntry *entry = FindEntry(outAttr->attributeId);
	if (entry == NULL) {
		m_mutex.Unlock();
		return TelephonyResult(TEL_ERR_NOT_FOUND);
	}

	paf::string filePath(entry->categoryOrPath);
	TelephonyResult res = m_accessor.Open(filePath, false);
	if (res.GetCode() != TEL_OK) {
		m_mutex.Unlock();
		return res;
	}

	paf::string keyName(entry->keyName);
	if (entry->storageType == TEL_ATTR_STORE_INT) {
		int value = 0;
		res = m_accessor.GetInt(keyName, &value);
		outAttr->SetInt(value);
	} else if (entry->storageType == TEL_ATTR_STORE_FLOAT) {
		float value = 0.0f;
		res = m_accessor.GetFloat(keyName, &value);
		outAttr->SetFloat(value);
	} else if (entry->storageType == TEL_ATTR_STORE_STRING) {
		paf::string value;
		res = m_accessor.GetString(keyName, &value);
		outAttr->SetString(value);
	} else {
		res = TelephonyResult(TEL_ERR_IO_FAILED);
	}

	if (res.GetCode() != TEL_OK)
		m_accessor.Close();
	else
		res = m_accessor.Close();

	if (res.GetCode() == TEL_OK)
		m_cache[outAttr->attributeId] = *outAttr;

	m_mutex.Unlock();
	return res;
}

/* 0x813DEA40 */
TelephonyResult TelIniFileAttributeBase::SetInt(int attrId, int value)
{
	const TelAttributeEntry *entry = FindEntry(attrId);
	if (entry == NULL)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_mutex.Lock();
	TelephonyResult res = m_accessor.Open(paf::string(entry->categoryOrPath), true);
	if (res.GetCode() == TEL_OK) {
		res = m_accessor.SetInt(paf::string(entry->keyName), value);
		if (res.GetCode() != TEL_OK)
			m_accessor.Close();
		else
			res = m_accessor.Close();
	}
	m_mutex.Unlock();
	return res;
}

/* 0x813DEBA4 */
TelephonyResult TelIniFileAttributeBase::GetInt(int attrId, int *outValue)
{
	const TelAttributeEntry *entry = FindEntry(attrId);
	if (entry == NULL)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_mutex.Lock();
	TelephonyResult res = m_accessor.Open(paf::string(entry->categoryOrPath), false);
	if (res.GetCode() == TEL_OK) {
		res = m_accessor.GetInt(paf::string(entry->keyName), outValue);
		if (res.GetCode() != TEL_OK)
			m_accessor.Close();
		else
			res = m_accessor.Close();
	}
	m_mutex.Unlock();
	return res;
}

/* 0x813DECFE */
TelephonyResult TelIniFileAttributeBase::SetFloat(int attrId, float value)
{
	const TelAttributeEntry *entry = FindEntry(attrId);
	if (entry == NULL)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_mutex.Lock();
	TelephonyResult res = m_accessor.Open(paf::string(entry->categoryOrPath), true);
	if (res.GetCode() == TEL_OK) {
		res = m_accessor.SetFloat(paf::string(entry->keyName), value);
		if (res.GetCode() != TEL_OK)
			m_accessor.Close();
		else
			res = m_accessor.Close();
	}
	m_mutex.Unlock();
	return res;
}

/* 0x813DEE62 */
TelephonyResult TelIniFileAttributeBase::GetFloat(int attrId, float *outValue)
{
	const TelAttributeEntry *entry = FindEntry(attrId);
	if (entry == NULL)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_mutex.Lock();
	TelephonyResult res = m_accessor.Open(paf::string(entry->categoryOrPath), false);
	if (res.GetCode() == TEL_OK) {
		res = m_accessor.GetFloat(paf::string(entry->keyName), outValue);
		if (res.GetCode() != TEL_OK)
			m_accessor.Close();
		else
			res = m_accessor.Close();
	}
	m_mutex.Unlock();
	return res;
}

/* 0x813DEFBC */
TelephonyResult TelIniFileAttributeBase::SetString(int attrId, const paf::string &value)
{
	const TelAttributeEntry *entry = FindEntry(attrId);
	if (entry == NULL)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_mutex.Lock();
	TelephonyResult res = m_accessor.Open(paf::string(entry->categoryOrPath), true);
	if (res.GetCode() == TEL_OK) {
		res = m_accessor.SetString(paf::string(entry->keyName), value);
		if (res.GetCode() != TEL_OK)
			m_accessor.Close();
		else
			res = m_accessor.Close();
	}
	m_mutex.Unlock();
	return res;
}

/* 0x813DF116 */
TelephonyResult TelIniFileAttributeBase::GetString(int attrId, paf::string *outValue)
{
	const TelAttributeEntry *entry = FindEntry(attrId);
	if (entry == NULL)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_mutex.Lock();
	TelephonyResult res = m_accessor.Open(paf::string(entry->categoryOrPath), false);
	if (res.GetCode() == TEL_OK) {
		res = m_accessor.GetString(paf::string(entry->keyName), outValue);
		if (res.GetCode() != TEL_OK)
			m_accessor.Close();
		else
			res = m_accessor.Close();
	}
	m_mutex.Unlock();
	return res;
}

} /* namespace telephony */
