/*
 * tel_sms_manager_registory_attribute.cpp: IdStorage, INI, Registry, and
 * temporary memory attribute stores for telephony_plugin
 * (3.65-CEX: 0x8124F5D8..0x812506FB).
 */
#include <std/string.h>
#include <scetypes.h>
#include <regmgr/regmgr.h>
#include "../tel_internal.h"

extern "C" {
SceUInt32 vshIdStorageReadLeaf(SceSize leaf, void *buf);
SceUInt32 vshSysconHasWWAN(void);
}

namespace telephony {

/* IdStorage leaf buffer layouts (512 bytes per leaf) */
struct TelIdStorageLeaf113 {
	char modemId[32];
	SceUInt8 headerExtra[20];
	SceUInt32 antennaDispType;		/* +0x34 */
	SceUInt8 middleBytes[80];
	SceInt32 targetOperatorCode;		/* +0x88 */
	SceUInt8 tailBytes[372];
};

struct TelIdStorageLeaf115 {
	SceUInt8 prefixBytes[15];
	SceUInt8 operatorDigitAscii;		/* +0x0F ('1'..'9') */
	SceUInt8 tailBytes[496];
};

static TelSmsManagerIdStorageAttribute *s_idStorageAttrInstance = NULL;
static TelSmsManagerIniFileAttribute *s_iniFileAttrInstance = NULL;
static TelSmsManagerRegistoryAttribute *s_registoryAttrInstance = NULL;
static TelSmsManagerMemoryAttribute *s_memoryAttrInstance = NULL;

/* 0x81515344: 8 INI attribute mappings */
static const TelAttributeEntry s_iniAttributeTable[] = {
	{ "vs0:vsh/shell/telephony/tel_registory.ini",    TEL_ATTR_ENABLE_WWAN,
	  "enable_wwan",                    TEL_ATTR_STORE_INT,    0 },
	{ "vs0:vsh/shell/telephony/tel_config.ini",       TEL_ATTR_LOOPBACK_AUTO_RECV,
	  "BBMC_LOOPBACK_AUTO_RECV",        TEL_ATTR_STORE_INT,    0 },
	{ "vs0:vsh/shell/telephony/tel_config.ini",       TEL_ATTR_LOOPBACK_PIN_STATUS,
	  "BBMC_LOOPBACK_PIN_STATUS",       TEL_ATTR_STORE_INT,    0 },
	{ "vs0:vsh/shell/telephony/tel_config.ini",       TEL_ATTR_LOOPBACK_SIM_LOCK_STATE,
	  "BBMC_LOOPBACK_SIM_LOCK_STATE",   TEL_ATTR_STORE_INT,    0 },
	{ "vs0:vsh/shell/telephony/tel_config.ini",       TEL_ATTR_LOOPBACK_SIM_BLOCK_TYPE,
	  "BBMC_LOOPBACK_SIM_BLOCK_TYPE",   TEL_ATTR_STORE_INT,    0 },
	{ "vs0:vsh/shell/telephony/tel_config.ini",       TEL_ATTR_LOOPBACK_INTERRUPT_TIME,
	  "BBMC_LOOPBACK_INTERRUPT_TIME",   TEL_ATTR_STORE_INT,    0 },
	{ "vs0:vsh/shell/telephony/tel_config.ini",       TEL_ATTR_LOOPBACK_INTERRUPT_EVENT,
	  "BBMC_LOOPBACK_INTERRUPT_EVENT",  TEL_ATTR_STORE_INT,    0 },
	{ "vs0:vsh/shell/telephony/tel_antenna_info.ini", TEL_ATTR_ANTENNA_LEVEL_MAP,
	  "ANTENNA_LEVEL_MAP",              TEL_ATTR_STORE_STRING, 0 }
};

/* 0x81515560: 26 registry attribute mappings */
static const TelAttributeEntry s_registoryAttributeTable[] = {
	{ "/CONFIG/SYSTEM", TEL_ATTR_FLIGHT_MODE,             "flight_mode",             TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_USE_DEBUG_SETTINGS,      "use_debug_settings",      TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_IS_COM_EXIST,            "is_com_exist",            TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_ENABLE_WWAN,             "enable_wwan",             TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_IMSI,                    "imsi",                    TEL_ATTR_STORE_STRING, 16  },
	{ "/CONFIG/TEL",    TEL_ATTR_APN_NAME,                "apn_name",                TEL_ATTR_STORE_STRING, 101 },
	{ "/CONFIG/TEL",    TEL_ATTR_APN_USER_ID,             "apn_user_id",             TEL_ATTR_STORE_STRING, 128 },
	{ "/CONFIG/TEL",    TEL_ATTR_APN_PASSWORD,            "apn_password",            TEL_ATTR_STORE_STRING, 128 },
	{ "/CONFIG/TEL",    TEL_ATTR_IS_APN_INIT,             "is_apn_init",             TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_MCC,                     "mcc",                     TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_MNC,                     "mnc",                     TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_NETWORK_SELECTION,       "network_selection",       TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_ENABLE_DATA_ROAMING,     "enable_data_roaming",     TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_COM_OPERATION_MODE,      "com_operation_mode",      TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_PRODUCT_TARGET_OPERATOR, "product_target_operator", TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_RAT,                     "rat",                     TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_MAX_TEMP,                "max_temp",                TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_RECOVER_TEMP,            "recover_temp",            TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_ANTENNA_DISP_TYPE,       "antenna_disp_type",       TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_USB_SETTING,             "usb_setting",             TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_DBG_LOG_TEL,             "dbg_log_tel",             TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_DBG_LOG_BBMC,            "dbg_log_bbmc",            TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_DBG_LOG_NET,             "dbg_log_net",             TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_SIM_UNIQUE_ID,           "sim_unique_id",           TEL_ATTR_STORE_STRING, 21  },
	{ "/CONFIG/TEL",    TEL_ATTR_BILLING_PLAN,            "billing_plan",            TEL_ATTR_STORE_INT,    0   },
	{ "/CONFIG/TEL",    TEL_ATTR_FORBIDDEN_PHONE,         "forbidden_phone0",        TEL_ATTR_STORE_INT,    0   }
};

/* ------------------------------------------------------------------
 * TelSmsManagerIdStorageAttribute (0x8124F5D8..0x8124FD37)
 * ------------------------------------------------------------------ */

TelSmsManagerIdStorageAttribute::TelSmsManagerIdStorageAttribute()
	: m_mutex("SceTelIdStorageRegistryAttr", 0)
{
	s_idStorageAttrInstance = this;
}

TelSmsManagerIdStorageAttribute::~TelSmsManagerIdStorageAttribute()
{
	s_idStorageAttrInstance = NULL;
	m_cache.clear();
}

TelephonyResult TelSmsManagerIdStorageAttribute::ReadAndCacheAttribute(TelAttributeValue *outAttr)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();

	int attrId = outAttr->attributeId;
	paf::map<int, TelAttributeValue>::iterator it = m_cache.find(attrId);
	if (it != m_cache.end()) {
		*outAttr = it->second;
		m_mutex.Unlock();
		return res;
	}

	if (attrId == TEL_ATTR_ANTENNA_DISP_TYPE) {
		TelIdStorageLeaf113 leaf;
		if (vshIdStorageReadLeaf(0x113U, &leaf) == 0 && leaf.antennaDispType <= 5U)
			outAttr->SetInt((int)leaf.antennaDispType);
		else
			outAttr->SetInt(0);
	} else if (attrId == TEL_ATTR_PRODUCT_TARGET_OPERATOR) {
		outAttr->SetInt(0);
		TelIdStorageLeaf115 leaf115;
		bool resolved = false;

		if (vshIdStorageReadLeaf(0x115U, &leaf115) == 0) {
			if (leaf115.operatorDigitAscii >= '1' && leaf115.operatorDigitAscii <= '9') {
				outAttr->SetInt((int)(leaf115.operatorDigitAscii - '0'));
				resolved = true;
			}
		}

		if (!resolved) {
			TelIdStorageLeaf113 leaf113;
			if (vshIdStorageReadLeaf(0x113U, &leaf113) == 0) {
				if (leaf113.targetOperatorCode == 1 ||
				    leaf113.targetOperatorCode == 2) {
					outAttr->SetInt(leaf113.targetOperatorCode);
					resolved = true;
				}
			}
		}

		if (!resolved) {
			int mappedOp = 3;
			switch (sceRegMgrSrvGetRegion()) {
			case 2:
				mappedOp = 5;
				break;
			case 3:
			case 4:
			case 5:
			case 9:
				mappedOp = 3;
				break;
			case 6:
			case 7:
			case 8:
				mappedOp = 4;
				break;
			case 10:
				mappedOp = 6;
				break;
			default:
				mappedOp = 3;
				break;
			}
			outAttr->SetInt(mappedOp);
		}
	} else if (attrId == TEL_ATTR_IDSTORAGE_MODEM_ID) {
		char leafBuf[512];
		SceUInt32 leafErr = vshIdStorageReadLeaf(0x113U, leafBuf);
		if (leafErr != 0) {
			res.SetCode((int)leafErr);
		} else {
			char modemId[33];
			sce_paf_memset(modemId, 0, sizeof(modemId));
			sce_paf_strncpy(modemId, leafBuf, 32);
			outAttr->SetString(paf::string(modemId));
		}
	} else {
		res.SetCode(TEL_ERR_NOT_SUPPORTED);
	}

	if (res.IsSuccess())
		m_cache[attrId] = *outAttr;

	m_mutex.Unlock();
	return res;
}

TelephonyResult TelSmsManagerIdStorageAttribute::GetAttribute(TelAttributeValue *outAttr)
{
	return ReadAndCacheAttribute(outAttr);
}

TelephonyResult TelSmsManagerIdStorageAttribute::GetInt(int attrId, int *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = ReadAndCacheAttribute(&attr);
	if (res.IsSuccess())
		*outValue = attr.intValue;
	return res;
}

TelephonyResult TelSmsManagerIdStorageAttribute::GetFloat(int attrId, float *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = ReadAndCacheAttribute(&attr);
	if (res.IsSuccess())
		*outValue = attr.floatValue;
	return res;
}

TelephonyResult TelSmsManagerIdStorageAttribute::GetString(int attrId, paf::string *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = ReadAndCacheAttribute(&attr);
	if (res.IsSuccess())
		outValue->append(attr.stringValue);
	return res;
}

/* ------------------------------------------------------------------
 * TelSmsManagerIniFileAttribute (0x8124FD38..0x8124FDF3)
 * ------------------------------------------------------------------ */

TelSmsManagerIniFileAttribute::TelSmsManagerIniFileAttribute()
{
	s_iniFileAttrInstance = this;
}

TelSmsManagerIniFileAttribute::~TelSmsManagerIniFileAttribute()
{
	s_iniFileAttrInstance = NULL;
}

const TelAttributeEntry *TelSmsManagerIniFileAttribute::FindEntry(int attrId) const
{
	SceSize count = sizeof(s_iniAttributeTable) / sizeof(s_iniAttributeTable[0]);
	for (SceSize idx = 0; idx < count; ++idx) {
		if (s_iniAttributeTable[idx].attributeId == attrId)
			return &s_iniAttributeTable[idx];
	}
	return NULL;
}

/* ------------------------------------------------------------------
 * TelSmsManagerRegistoryAttribute (0x8124FDF4..0x81250583)
 * ------------------------------------------------------------------ */

TelSmsManagerRegistoryAttribute::TelSmsManagerRegistoryAttribute()
	: m_cacheMutex("SceTelExcludedRegistryAttr", 0)
{
	s_registoryAttrInstance = this;
}

TelSmsManagerRegistoryAttribute::~TelSmsManagerRegistoryAttribute()
{
	s_registoryAttrInstance = NULL;
	m_cache.clear();
}

const TelAttributeEntry *TelSmsManagerRegistoryAttribute::FindEntry(int attrId) const
{
	SceSize count = sizeof(s_registoryAttributeTable) / sizeof(s_registoryAttributeTable[0]);
	for (SceSize idx = 0; idx < count; ++idx) {
		if (s_registoryAttributeTable[idx].attributeId == attrId)
			return &s_registoryAttributeTable[idx];
	}
	return NULL;
}

TelephonyResult TelSmsManagerRegistoryAttribute::SetAttribute(const TelAttributeValue &attr)
{
	return TelRegistoryAttributeBase::SetAttribute(attr);
}

TelephonyResult TelSmsManagerRegistoryAttribute::ReadWithOverride(TelAttributeValue *outAttr)
{
	TelephonyResult res(TEL_OK);
	m_cacheMutex.Lock();

	int attrId = outAttr->attributeId;
	paf::map<int, TelAttributeValue>::iterator it = m_cache.find(attrId);
	if (it != m_cache.end()) {
		*outAttr = it->second;
		m_cacheMutex.Unlock();
		return res;
	}

	if (attrId == TEL_ATTR_ANTENNA_DISP_TYPE || attrId == TEL_ATTR_PRODUCT_TARGET_OPERATOR) {
		int useDebug = 0;
		TelRegistoryAttributeBase::GetInt(TEL_ATTR_USE_DEBUG_SETTINGS, &useDebug);
		if (useDebug != 0)
			res = TelRegistoryAttributeBase::GetAttribute(outAttr);
		else
			res.SetCode(TEL_ERR_NOT_SUPPORTED);
	} else if (attrId == TEL_ATTR_IS_COM_EXIST) {
		int useDebug = 0;
		TelRegistoryAttributeBase::GetInt(TEL_ATTR_USE_DEBUG_SETTINGS, &useDebug);
		if (useDebug != 0) {
			res = TelRegistoryAttributeBase::GetAttribute(outAttr);
		} else {
			outAttr->SetInt(vshSysconHasWWAN() != 0 ? 1 : 0);
			m_cache[attrId] = *outAttr;
		}
	} else {
		res = TelRegistoryAttributeBase::GetAttribute(outAttr);
	}

	m_cacheMutex.Unlock();
	return res;
}

TelephonyResult TelSmsManagerRegistoryAttribute::GetAttribute(TelAttributeValue *outAttr)
{
	return ReadWithOverride(outAttr);
}

TelephonyResult TelSmsManagerRegistoryAttribute::SetInt(int attrId, int value)
{
	return TelRegistoryAttributeBase::SetInt(attrId, value);
}

TelephonyResult TelSmsManagerRegistoryAttribute::GetInt(int attrId, int *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = ReadWithOverride(&attr);
	*outValue = attr.intValue;
	return res;
}

TelephonyResult TelSmsManagerRegistoryAttribute::AddInt(int attrId, int delta)
{
	int current = 0;
	TelephonyResult res = TelRegistoryAttributeBase::GetInt(attrId, &current);
	if (res.IsSuccess())
		res = TelRegistoryAttributeBase::SetInt(attrId, current + delta);
	return res;
}

TelephonyResult TelSmsManagerRegistoryAttribute::SetFloat(int attrId, float value)
{
	return TelRegistoryAttributeBase::SetFloat(attrId, value);
}

TelephonyResult TelSmsManagerRegistoryAttribute::GetFloat(int attrId, float *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = ReadWithOverride(&attr);
	*outValue = attr.floatValue;
	return res;
}

TelephonyResult TelSmsManagerRegistoryAttribute::SetString(int attrId, const paf::string &value)
{
	return TelRegistoryAttributeBase::SetString(attrId, value);
}

TelephonyResult TelSmsManagerRegistoryAttribute::GetString(int attrId, paf::string *outValue)
{
	TelAttributeValue attr(attrId);
	TelephonyResult res = ReadWithOverride(&attr);
	outValue->append(attr.stringValue);
	return res;
}

/* ------------------------------------------------------------------
 * TelSmsManagerMemoryAttribute (0x81250584..0x812506FB)
 * ------------------------------------------------------------------ */

/* 0x81250584 */
TelAttributeStore *telGetBbmcAttributeStore(void)
{
	return s_memoryAttrInstance;
}

TelSmsManagerMemoryAttribute::TelSmsManagerMemoryAttribute()
{
	s_memoryAttrInstance = this;
}

TelSmsManagerMemoryAttribute::~TelSmsManagerMemoryAttribute()
{
	s_memoryAttrInstance = NULL;
}

TelephonyResult TelSmsManagerMemoryAttribute::Initialize()
{
	m_mutex.Lock();
	for (int offset = 0; offset < TEL_ATTR_MEMORY_COUNT; ++offset) {
		int attrId = TEL_ATTR_MEMORY_FIRST + offset;
		TelAttributeValue emptyVal;
		m_values[attrId] = emptyVal;
	}
	m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
