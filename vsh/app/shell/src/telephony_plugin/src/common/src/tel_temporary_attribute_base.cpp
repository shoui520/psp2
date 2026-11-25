/*
 * tel_temporary_attribute_base.cpp: In-memory temporary attribute store base
 * (3.65-CEX: 0x813E3714..0x813E446F).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../tel_common.h"

namespace telephony {

/* 0x813E3714 */
TelTemporaryAttributeBase::TelTemporaryAttributeBase()
	: m_mutex("SceTelCmnTemporaryAttribute", 0)
{
}

/* 0x813E3768 */
TelephonyResult TelTemporaryAttributeBase::Terminate()
{
	m_values.clear();
	return TelephonyResult(TEL_OK);
}

/* 0x813E3778 */
TelephonyResult TelTemporaryAttributeBase::SetAttribute(const TelAttributeValue &attr)
{
	m_mutex.Lock();
	m_values[attr.attributeId] = attr;
	m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x813E3894 */
TelephonyResult TelTemporaryAttributeBase::GetAttribute(TelAttributeValue *outAttr)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	paf::map<int, TelAttributeValue>::iterator it = m_values.find(outAttr->attributeId);
	if (it == m_values.end())
		res = TelephonyResult(TEL_ERR_NOT_FOUND);
	else
		*outAttr = it->second;
	m_mutex.Unlock();
	return res;
}

/* 0x813E3936 */
TelephonyResult TelTemporaryAttributeBase::AddAttribute(const TelAttributeValue &delta)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	paf::map<int, TelAttributeValue>::iterator it = m_values.find(delta.attributeId);
	if (it == m_values.end()) {
		res = TelephonyResult(TEL_ERR_NOT_FOUND);
	} else {
		TelAttributeValue updated;
		if (delta.valueType == TEL_ATTR_VAL_INT) {
			updated = it->second;
			updated.SetInt(it->second.intValue + delta.intValue);
		} else if (delta.valueType == TEL_ATTR_VAL_FLOAT) {
			updated = it->second;
			updated.SetFloat(it->second.floatValue + delta.floatValue);
		} else if (delta.valueType == TEL_ATTR_VAL_STRING) {
			updated = it->second;
			paf::string combined = it->second.stringValue;
			combined.append(delta.stringValue);
			updated.SetString(combined);
		}
		it->second = updated;
	}
	m_mutex.Unlock();
	return res;
}

/* 0x813E3CDE */
TelephonyResult TelTemporaryAttributeBase::SetInt(int attrId, int value)
{
	TelAttributeValue attr(attrId);
	attr.SetInt(value);
	return SetAttribute(attr);
}

/* 0x813E3DBC */
TelephonyResult TelTemporaryAttributeBase::GetInt(int attrId, int *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = GetAttribute(&attr);
	*outValue = attr.intValue;
	return res;
}

/* 0x813E3E50 */
TelephonyResult TelTemporaryAttributeBase::AddInt(int attrId, int delta)
{
	TelAttributeValue attr(attrId);
	attr.SetInt(delta);
	return AddAttribute(attr);
}

/* 0x813E3F2E */
TelephonyResult TelTemporaryAttributeBase::SetFloat(int attrId, float value)
{
	TelAttributeValue attr(attrId);
	attr.SetFloat(value);
	return SetAttribute(attr);
}

/* 0x813E400E */
TelephonyResult TelTemporaryAttributeBase::GetFloat(int attrId, float *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = GetAttribute(&attr);
	*outValue = attr.floatValue;
	return res;
}

/* 0x813E40A4 */
TelephonyResult TelTemporaryAttributeBase::AddFloat(int attrId, float delta)
{
	TelAttributeValue attr(attrId);
	attr.SetFloat(delta);
	return AddAttribute(attr);
}

/* 0x813E4184 */
TelephonyResult TelTemporaryAttributeBase::SetString(int attrId, const paf::string &value)
{
	TelAttributeValue attr(attrId);
	attr.SetString(value);
	return SetAttribute(attr);
}

/* 0x813E422E */
TelephonyResult TelTemporaryAttributeBase::GetString(int attrId, paf::string *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = GetAttribute(&attr);
	*outValue = attr.stringValue;
	return res;
}

/* 0x813E42F6 */
TelephonyResult TelTemporaryAttributeBase::SetBool(int attrId, bool value)
{
	TelAttributeValue attr(attrId);
	attr.SetBool(value);
	return SetAttribute(attr);
}

/* 0x813E43D8 */
TelephonyResult TelTemporaryAttributeBase::GetBool(int attrId, bool *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = GetAttribute(&attr);
	*outValue = attr.boolValue;
	return res;
}

} /* namespace telephony */
