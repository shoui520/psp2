/*
 * tel_general_logic_netctl.cpp: product target operator specifications
 * (AT&T, NTT Docomo, Vodafone/EU, Canada, alternate region, and default),
 * download size/roaming policies, operator app title IDs, and NetCtl factory
 * (3.65-CEX: 0x8125F374..0x8125F4AF, 0x81260038..0x81261067).
 */
#include <std/new>
#include <std/string.h>
#include "tel_general_internal.h"

namespace telephony {

static const char *s_defaultUpdateListUrl = "http://upd.bf1.jp/update3/psp2-updatelist.xml";
static const char *s_canadaUpdateListUrl = "http://upd.bf1.jp/update3/psp2-updatelist-ca.xml";

static const SceInt64 kDownloadLimitUnlimited = 0x7FFFFFFFFFFFFFFFLL;
static const SceInt64 kDownloadLimit20MiB = 20971520LL;
static const SceInt64 kDownloadLimit50MiB = 52428800LL;

struct TelPlmnPair {
	SceUInt16 mcc;
	SceUInt16 mnc;
};

/* 0x81515FE4: AT&T domestic roaming PLMN allowlist (10 MCC/MNC pairs) */
static const TelPlmnPair s_attDomesticRoamingPlmns[] = {
	{ 311, 180 },
	{ 310, 410 },
	{ 310, 150 },
	{ 310, 170 },
	{ 310, 380 },
	{ 310, 560 },
	{ 310, 680 },
	{ 310,  70 },
	{ 310, 980 },
	{ 310,  90 }
};

/* 0x81260038: read TEL_ATTR_PRODUCT_TARGET_OPERATOR (0x60002) */
TelephonyResult telGetProductTargetOperator(int *outOperator)
{
	TelephonyResult res(TEL_OK);
	*outOperator = TEL_OPERATOR_DEFAULT;

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return res;

	TelAttributeValue opAttr(TEL_ATTR_PRODUCT_TARGET_OPERATOR);
	res = attrMgr->GetAttribute(&opAttr);
	if (res.IsSuccess())
		*outOperator = opAttr.intValue;

	return res;
}

/* 0x8126013A: load the supported operator PLMN list for this device's target operator */
TelephonyResult telLoadSupportedOperatorPlmns(paf::vector<TelephonyPlmn> *outPlmns)
{
	outPlmns->clear();

	int targetOperator = TEL_OPERATOR_DEFAULT;
	TelephonyResult res = telGetProductTargetOperator(&targetOperator);
	if (!res.IsSuccess())
		return res;

	TelephonyPlmn emptyPlmn;
	TelOperatorProfileFactory factory(targetOperator, emptyPlmn, 1);
	paf::vector<TelOperatorProfile> profiles;
	res = factory.LoadProfiles(&profiles);
	if (!res.IsSuccess()) {
		res.SetCode(TEL_OK);
		return res;
	}

	res = factory.LoadSupportedPlmns(outPlmns);
	if (!res.IsSuccess() || outPlmns->empty())
		res.SetCode(TEL_OK);

	return res;
}

/* 0x81260350: check whether a PLMN matches any operator profile for operatorId */
bool telIsPlmnSupportedByOperator(int operatorId, const TelephonyPlmn &plmn)
{
	TelOperatorProfileFactory factory(operatorId, plmn, 0);
	paf::vector<TelOperatorProfile> profiles;
	TelephonyResult res = factory.LoadProfiles(&profiles);
	return res.IsSuccess() && !profiles.empty();
}

/* 0x81260420: check whether apnName appears in any APN list for the given PLMNs */
bool telIsApnInOperatorPlmnList(const paf::string &apnName,
				const paf::vector<TelephonyPlmn> &plmns)
{
	TelApnInfoListFactory listFactory;
	for (SceSize idx = 0; idx < plmns.size(); ++idx) {
		TelApnInfoList apnList;
		TelephonyResult res = listFactory.Create(plmns[idx], &apnList);
		if (res.IsSuccess() && apnList.ContainsApnName(apnName))
			return true;
	}
	return false;
}

/* ------------------------------------------------------------------
 * Base product operator spec implementation (0x81260678..0x8126076B)
 * ------------------------------------------------------------------ */

class TelGeneralLogicNetCtlBase : public TelGeneralLogicNetCtl {
public:
	explicit TelGeneralLogicNetCtlBase(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLogicNetCtlBase();

	virtual TelephonyResult SetBbmcAdapter(TelBbmcAdapterInterface *bbmcAdapter);
	virtual TelephonyResult Initialize();
	virtual TelephonyResult SetSimActivationState(const int *activationState);
	virtual TelephonyResult GetSimActivationState(int *outActivationState);
	virtual bool IsSimCardMatchOperator(const TelephonyPlmn &plmn);
	virtual TelephonyResult IsApnMatchOperator(const paf::vector<TelephonyPlmn> &plmns,
						   bool *outMatched);
	virtual TelephonyResult IsOperatorRestrictedApn(const paf::vector<TelephonyPlmn> &plmns,
							bool *outRestricted);
	virtual TelephonyResult UpdateRoamingOverride();
	virtual TelephonyResult IsDomesticRoamingAllowed(bool *outAllowed);
	virtual TelephonyResult IsApnEditAllowed(bool *outAllowed);
	virtual TelephonyResult IsManualOperatorSelectionAllowed(bool *outAllowed);
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType);
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes);
	virtual TelephonyResult CheckRoamingDownloadAllowed(unsigned int serviceType);
	virtual TelephonyResult GetUpdateListUrl(paf::string *outUrl);

protected:
	TelBbmcAdapterInterface *m_bbmcAdapter;
	int m_simActivationState;
	bool m_domesticRoamingMatched;
};

/* 0x81260678 */
TelGeneralLogicNetCtlBase::TelGeneralLogicNetCtlBase(TelBbmcAdapterInterface *bbmcAdapter)
	: m_bbmcAdapter(bbmcAdapter),
	  m_simActivationState(0),
	  m_domesticRoamingMatched(false)
{
}

TelGeneralLogicNetCtlBase::~TelGeneralLogicNetCtlBase()
{
}

TelephonyResult TelGeneralLogicNetCtlBase::SetBbmcAdapter(TelBbmcAdapterInterface *bbmcAdapter)
{
	m_bbmcAdapter = bbmcAdapter;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlBase::Initialize()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlBase::SetSimActivationState(const int *activationState)
{
	if (activationState != NULL)
		m_simActivationState = *activationState;
	return TelephonyResult(TEL_OK);
}

/* 0x812606D0 */
TelephonyResult TelGeneralLogicNetCtlBase::GetSimActivationState(int *outActivationState)
{
	*outActivationState = m_simActivationState;
	return TelephonyResult(TEL_OK);
}

bool TelGeneralLogicNetCtlBase::IsSimCardMatchOperator(const TelephonyPlmn &plmn)
{
	(void)plmn;
	return true;
}

/* 0x812606E0 */
TelephonyResult TelGeneralLogicNetCtlBase::IsApnMatchOperator(
	const paf::vector<TelephonyPlmn> &plmns, bool *outMatched)
{
	(void)plmns;
	*outMatched = true;
	return TelephonyResult(TEL_OK);
}

/* 0x812606EE */
TelephonyResult TelGeneralLogicNetCtlBase::IsOperatorRestrictedApn(
	const paf::vector<TelephonyPlmn> &plmns, bool *outRestricted)
{
	(void)plmns;
	*outRestricted = false;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlBase::UpdateRoamingOverride()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlBase::IsDomesticRoamingAllowed(bool *outAllowed)
{
	*outAllowed = false;
	return TelephonyResult(TEL_OK);
}

/* 0x812606FC */
TelephonyResult TelGeneralLogicNetCtlBase::IsApnEditAllowed(bool *outAllowed)
{
	*outAllowed = true;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlBase::IsManualOperatorSelectionAllowed(bool *outAllowed)
{
	*outAllowed = true;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlBase::GetOperatorAppTitleId(const char **outTitleId)
{
	*outTitleId = "NPXS10040";
	return TelephonyResult(TEL_OK);
}

/* 0x8126071E: allow service types 1..5 */
TelephonyResult TelGeneralLogicNetCtlBase::CheckContentDownloadAllowed(int serviceType)
{
	switch (serviceType) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		return TelephonyResult(TEL_OK);
	default:
		return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
	}
}

TelephonyResult TelGeneralLogicNetCtlBase::GetDownloadSizeLimit(unsigned int serviceType,
								SceInt64 *outLimitBytes)
{
	if (serviceType >= 2 && serviceType <= 5) {
		*outLimitBytes = kDownloadLimitUnlimited;
		return TelephonyResult(TEL_OK);
	}
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

TelephonyResult TelGeneralLogicNetCtlBase::CheckRoamingDownloadAllowed(unsigned int serviceType)
{
	if (serviceType >= 2 && serviceType <= 5)
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

/* 0x8126074A */
TelephonyResult TelGeneralLogicNetCtlBase::GetUpdateListUrl(paf::string *outUrl)
{
	*outUrl = s_defaultUpdateListUrl;
	return TelephonyResult(TEL_OK);
}

/* ------------------------------------------------------------------
 * Operator 4: Vodafone / EU (NPXS10040, unlimited, editable APN)
 * ------------------------------------------------------------------ */

class TelGeneralLogicNetCtlVodafone : public TelGeneralLogicNetCtlBase {
public:
	explicit TelGeneralLogicNetCtlVodafone(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLogicNetCtlVodafone();
};

/* 0x8126076C */
TelGeneralLogicNetCtlVodafone::TelGeneralLogicNetCtlVodafone(TelBbmcAdapterInterface *bbmcAdapter)
	: TelGeneralLogicNetCtlBase(bbmcAdapter)
{
}

TelGeneralLogicNetCtlVodafone::~TelGeneralLogicNetCtlVodafone()
{
}

/* ------------------------------------------------------------------
 * Operator 5: Canada (NPXS10044, Canada update URL, locked APN/scan)
 * ------------------------------------------------------------------ */

class TelGeneralLogicNetCtlCanada : public TelGeneralLogicNetCtlBase {
public:
	explicit TelGeneralLogicNetCtlCanada(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLogicNetCtlCanada();

	virtual TelephonyResult IsApnEditAllowed(bool *outAllowed);
	virtual TelephonyResult IsManualOperatorSelectionAllowed(bool *outAllowed);
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
	virtual TelephonyResult GetUpdateListUrl(paf::string *outUrl);
};

/* 0x812607F8 */
TelGeneralLogicNetCtlCanada::TelGeneralLogicNetCtlCanada(TelBbmcAdapterInterface *bbmcAdapter)
	: TelGeneralLogicNetCtlBase(bbmcAdapter)
{
}

TelGeneralLogicNetCtlCanada::~TelGeneralLogicNetCtlCanada()
{
}

TelephonyResult TelGeneralLogicNetCtlCanada::IsApnEditAllowed(bool *outAllowed)
{
	*outAllowed = false;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlCanada::IsManualOperatorSelectionAllowed(bool *outAllowed)
{
	*outAllowed = false;
	return TelephonyResult(TEL_OK);
}

/* 0x812607C2 */
TelephonyResult TelGeneralLogicNetCtlCanada::GetOperatorAppTitleId(const char **outTitleId)
{
	*outTitleId = "NPXS10044";
	return TelephonyResult(TEL_OK);
}

/* 0x812607D6 */
TelephonyResult TelGeneralLogicNetCtlCanada::GetUpdateListUrl(paf::string *outUrl)
{
	*outUrl = s_canadaUpdateListUrl;
	return TelephonyResult(TEL_OK);
}

/* ------------------------------------------------------------------
 * Default operator (NPXS10043, 20 MiB cap, serviceType 2 denied)
 * ------------------------------------------------------------------ */

class TelGeneralLogicNetCtlDefault : public TelGeneralLogicNetCtlBase {
public:
	explicit TelGeneralLogicNetCtlDefault(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLogicNetCtlDefault();

	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType);
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes);
};

TelGeneralLogicNetCtlDefault::TelGeneralLogicNetCtlDefault(TelBbmcAdapterInterface *bbmcAdapter)
	: TelGeneralLogicNetCtlBase(bbmcAdapter)
{
}

TelGeneralLogicNetCtlDefault::~TelGeneralLogicNetCtlDefault()
{
}

/* 0x8126084E */
TelephonyResult TelGeneralLogicNetCtlDefault::GetOperatorAppTitleId(const char **outTitleId)
{
	*outTitleId = "NPXS10043";
	return TelephonyResult(TEL_OK);
}

/* 0x81260F88: deny serviceType 2 */
TelephonyResult TelGeneralLogicNetCtlDefault::CheckContentDownloadAllowed(int serviceType)
{
	switch (serviceType) {
	case 1:
	case 3:
	case 4:
	case 5:
		return TelephonyResult(TEL_OK);
	case 2:
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	default:
		return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
	}
}

/* 0x81260FC0: 20 MiB cap for serviceType 3..5, denied for 2 */
TelephonyResult TelGeneralLogicNetCtlDefault::GetDownloadSizeLimit(unsigned int serviceType,
								   SceInt64 *outLimitBytes)
{
	if (serviceType == 2) {
		*outLimitBytes = 0;
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	}
	if (serviceType >= 3 && serviceType <= 5) {
		*outLimitBytes = kDownloadLimit20MiB;
		return TelephonyResult(TEL_OK);
	}
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

/* ------------------------------------------------------------------
 * Operator 2: NTT Docomo (pre.dcm.ne.jp 50 MiB cap vs hs.pre.dcm.ne.jp)
 * ------------------------------------------------------------------ */

class TelGeneralLogicNetCtlDocomo : public TelGeneralLogicNetCtlBase {
public:
	explicit TelGeneralLogicNetCtlDocomo(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLogicNetCtlDocomo();

	virtual TelephonyResult IsApnEditAllowed(bool *outAllowed);
	virtual TelephonyResult IsManualOperatorSelectionAllowed(bool *outAllowed);
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType);
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes);
	virtual TelephonyResult CheckRoamingDownloadAllowed(unsigned int serviceType);

private:
	bool CheckDocomoApn(bool *outIsPrepaidStandard);
};

/* 0x81260884 */
TelGeneralLogicNetCtlDocomo::TelGeneralLogicNetCtlDocomo(TelBbmcAdapterInterface *bbmcAdapter)
	: TelGeneralLogicNetCtlBase(bbmcAdapter)
{
}

TelGeneralLogicNetCtlDocomo::~TelGeneralLogicNetCtlDocomo()
{
}

TelephonyResult TelGeneralLogicNetCtlDocomo::IsApnEditAllowed(bool *outAllowed)
{
	*outAllowed = false;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlDocomo::IsManualOperatorSelectionAllowed(bool *outAllowed)
{
	*outAllowed = false;
	return TelephonyResult(TEL_OK);
}

/* 0x812608DA */
TelephonyResult TelGeneralLogicNetCtlDocomo::GetOperatorAppTitleId(const char **outTitleId)
{
	*outTitleId = NULL;
	return TelephonyResult(TEL_NETCTL_ERR_DENIED);
}

/* 0x81260A84 */
TelephonyResult TelGeneralLogicNetCtlDocomo::CheckContentDownloadAllowed(int serviceType)
{
	bool isPrepaidPlan = false;
	CheckDocomoApn(&isPrepaidPlan);

	switch (serviceType) {
	case 1:
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	case 2:
		if (isPrepaidPlan)
			return TelephonyResult(TEL_NETCTL_ERR_DENIED);
		return TelephonyResult(TEL_OK);
	case 3:
	case 4:
	case 5:
		return TelephonyResult(TEL_OK);
	default:
		return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
	}
}

/* 0x81260AFC */
TelephonyResult TelGeneralLogicNetCtlDocomo::GetDownloadSizeLimit(unsigned int serviceType,
								  SceInt64 *outLimitBytes)
{
	bool isPrepaidPlan = false;
	CheckDocomoApn(&isPrepaidPlan);

	if (serviceType == 2) {
		if (isPrepaidPlan) {
			*outLimitBytes = 0;
			return TelephonyResult(TEL_NETCTL_ERR_DENIED);
		}
		*outLimitBytes = kDownloadLimitUnlimited;
		return TelephonyResult(TEL_OK);
	}
	if (serviceType >= 3 && serviceType <= 5) {
		*outLimitBytes = isPrepaidPlan ? kDownloadLimit50MiB : kDownloadLimitUnlimited;
		return TelephonyResult(TEL_OK);
	}
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

/* 0x81260BA6 */
TelephonyResult TelGeneralLogicNetCtlDocomo::CheckRoamingDownloadAllowed(unsigned int serviceType)
{
	bool isPrepaidPlan = false;
	bool isDocomoApn = CheckDocomoApn(&isPrepaidPlan);

	if (serviceType >= 2 && serviceType <= 5) {
		return TelephonyResult(isDocomoApn ? (int)TEL_NETCTL_ERR_DENIED : (int)TEL_OK);
	}
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

/* 0x812608F6: inspect current APN name for Docomo prepaid plans */
bool TelGeneralLogicNetCtlDocomo::CheckDocomoApn(bool *outIsPrepaidStandard)
{
	paf::string apnName("");
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL) {
		TelAttributeValue apnAttr(TEL_ATTR_APN_NAME);
		TelephonyResult res = attrMgr->GetAttribute(&apnAttr);
		if (res.IsSuccess())
			apnName = apnAttr.stringValue;
	}

	const char *rawApn = apnName.c_str();
	if (rawApn[0] == 'p' && sce_paf_strcmp(rawApn, "pre.dcm.ne.jp") == 0) {
		*outIsPrepaidStandard = true;
		return true;
	}
	if (rawApn[0] == 'h' && sce_paf_strcmp(rawApn, "hs.pre.dcm.ne.jp") == 0) {
		*outIsPrepaidStandard = false;
		return true;
	}
	*outIsPrepaidStandard = false;
	return false;
}

/* ------------------------------------------------------------------
 * Operator 6: Alternate region (NPXS10045, unlimited, editable APN)
 * ------------------------------------------------------------------ */

class TelGeneralLogicNetCtlRegionAlt : public TelGeneralLogicNetCtlBase {
public:
	explicit TelGeneralLogicNetCtlRegionAlt(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLogicNetCtlRegionAlt();

	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
};

/* 0x81260C38 */
TelGeneralLogicNetCtlRegionAlt::TelGeneralLogicNetCtlRegionAlt(
	TelBbmcAdapterInterface *bbmcAdapter)
	: TelGeneralLogicNetCtlBase(bbmcAdapter)
{
}

TelGeneralLogicNetCtlRegionAlt::~TelGeneralLogicNetCtlRegionAlt()
{
}

/* 0x81260C8E */
TelephonyResult TelGeneralLogicNetCtlRegionAlt::GetOperatorAppTitleId(const char **outTitleId)
{
	*outTitleId = "NPXS10045";
	return TelephonyResult(TEL_OK);
}

/* ------------------------------------------------------------------
 * Operator 1: AT&T (NPXS10041, 20 MiB cap, domestic roaming PLMN list)
 * ------------------------------------------------------------------ */

class TelGeneralLogicNetCtlAtt : public TelGeneralLogicNetCtlBase {
public:
	explicit TelGeneralLogicNetCtlAtt(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLogicNetCtlAtt();

	virtual bool IsSimCardMatchOperator(const TelephonyPlmn &plmn);
	virtual TelephonyResult IsApnMatchOperator(const paf::vector<TelephonyPlmn> &plmns,
						   bool *outMatched);
	virtual TelephonyResult IsOperatorRestrictedApn(const paf::vector<TelephonyPlmn> &plmns,
							bool *outRestricted);
	virtual TelephonyResult UpdateRoamingOverride();
	virtual TelephonyResult IsDomesticRoamingAllowed(bool *outAllowed);
	virtual TelephonyResult IsApnEditAllowed(bool *outAllowed);
	virtual TelephonyResult IsManualOperatorSelectionAllowed(bool *outAllowed);
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType);
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes);
};

/* 0x81260CC4 */
TelGeneralLogicNetCtlAtt::TelGeneralLogicNetCtlAtt(TelBbmcAdapterInterface *bbmcAdapter)
	: TelGeneralLogicNetCtlBase(bbmcAdapter)
{
	m_simActivationState = 1;
}

TelGeneralLogicNetCtlAtt::~TelGeneralLogicNetCtlAtt()
{
}

/* 0x81260D3E */
bool TelGeneralLogicNetCtlAtt::IsSimCardMatchOperator(const TelephonyPlmn &plmn)
{
	return telIsPlmnSupportedByOperator(TEL_OPERATOR_ATT, plmn);
}

/* 0x81260D80 */
TelephonyResult TelGeneralLogicNetCtlAtt::IsApnMatchOperator(
	const paf::vector<TelephonyPlmn> &plmns, bool *outMatched)
{
	*outMatched = false;

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue apnAttr(TEL_ATTR_APN_NAME);
	TelephonyResult res = attrMgr->GetAttribute(&apnAttr);
	if (res.IsSuccess()) {
		if (telIsApnInOperatorPlmnList(apnAttr.stringValue, plmns))
			*outMatched = true;
	}
	return res;
}

TelephonyResult TelGeneralLogicNetCtlAtt::IsOperatorRestrictedApn(
	const paf::vector<TelephonyPlmn> &plmns, bool *outRestricted)
{
	(void)plmns;
	*outRestricted = true;
	return TelephonyResult(TEL_OK);
}

/* 0x81260EB4: check current serving PLMN against the 10-entry AT&T allowlist */
TelephonyResult TelGeneralLogicNetCtlAtt::UpdateRoamingOverride()
{
	m_domesticRoamingMatched = false;
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_OK);

	TelephonyPlmn servingPlmn;
	TelephonyResult res = m_bbmcAdapter->GetServingPlmn(&servingPlmn);
	if (!res.IsSuccess())
		return res;

	int mcc = servingPlmn.GetMcc();
	int mnc = servingPlmn.GetMnc();
	SceSize count = sizeof(s_attDomesticRoamingPlmns) / sizeof(s_attDomesticRoamingPlmns[0]);
	for (SceSize idx = 0; idx < count; ++idx) {
		if (s_attDomesticRoamingPlmns[idx].mcc == mcc &&
		    s_attDomesticRoamingPlmns[idx].mnc == mnc) {
			m_domesticRoamingMatched = true;
			break;
		}
	}
	return res;
}

/* 0x81260F62 */
TelephonyResult TelGeneralLogicNetCtlAtt::IsDomesticRoamingAllowed(bool *outAllowed)
{
	*outAllowed = m_domesticRoamingMatched;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlAtt::IsApnEditAllowed(bool *outAllowed)
{
	*outAllowed = false;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicNetCtlAtt::IsManualOperatorSelectionAllowed(bool *outAllowed)
{
	*outAllowed = false;
	return TelephonyResult(TEL_OK);
}

/* 0x81260F74 */
TelephonyResult TelGeneralLogicNetCtlAtt::GetOperatorAppTitleId(const char **outTitleId)
{
	*outTitleId = "NPXS10041";
	return TelephonyResult(TEL_OK);
}

/* 0x81260F88 */
TelephonyResult TelGeneralLogicNetCtlAtt::CheckContentDownloadAllowed(int serviceType)
{
	switch (serviceType) {
	case 1:
	case 3:
	case 4:
	case 5:
		return TelephonyResult(TEL_OK);
	case 2:
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	default:
		return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
	}
}

/* 0x81260FC0 */
TelephonyResult TelGeneralLogicNetCtlAtt::GetDownloadSizeLimit(unsigned int serviceType,
							       SceInt64 *outLimitBytes)
{
	if (serviceType == 2) {
		*outLimitBytes = 0;
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	}
	if (serviceType >= 3 && serviceType <= 5) {
		*outLimitBytes = kDownloadLimit20MiB;
		return TelephonyResult(TEL_OK);
	}
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

/* ------------------------------------------------------------------
 * TelGeneralLogicNetCtlFactory (0x8125F374..0x8125F4AF)
 * ------------------------------------------------------------------ */

/* 0x8125F374 */
TelephonyResult TelGeneralLogicNetCtlFactory::Create(TelBbmcAdapterInterface *bbmcAdapter,
						     TelGeneralLogicNetCtl **outNetCtl)
{
	int targetOperator = TEL_OPERATOR_DEFAULT;
	TelephonyResult res = telGetProductTargetOperator(&targetOperator);
	if (!res.IsSuccess())
		targetOperator = TEL_OPERATOR_DEFAULT;

	TelGeneralLogicNetCtl *instance = NULL;
	switch (targetOperator) {
	case TEL_OPERATOR_ATT: {
		void *mem = telMalloc(sizeof(TelGeneralLogicNetCtlAtt));
		if (mem != NULL)
			instance = new (mem) TelGeneralLogicNetCtlAtt(bbmcAdapter);
		break;
	}
	case TEL_OPERATOR_DOCOMO: {
		void *mem = telMalloc(sizeof(TelGeneralLogicNetCtlDocomo));
		if (mem != NULL)
			instance = new (mem) TelGeneralLogicNetCtlDocomo(bbmcAdapter);
		break;
	}
	case TEL_OPERATOR_VODAFONE: {
		void *mem = telMalloc(sizeof(TelGeneralLogicNetCtlVodafone));
		if (mem != NULL)
			instance = new (mem) TelGeneralLogicNetCtlVodafone(bbmcAdapter);
		break;
	}
	case TEL_OPERATOR_CANADA: {
		void *mem = telMalloc(sizeof(TelGeneralLogicNetCtlCanada));
		if (mem != NULL)
			instance = new (mem) TelGeneralLogicNetCtlCanada(bbmcAdapter);
		break;
	}
	case TEL_OPERATOR_REGION_ALT: {
		void *mem = telMalloc(sizeof(TelGeneralLogicNetCtlRegionAlt));
		if (mem != NULL)
			instance = new (mem) TelGeneralLogicNetCtlRegionAlt(bbmcAdapter);
		break;
	}
	default: {
		void *mem = telMalloc(sizeof(TelGeneralLogicNetCtlDefault));
		if (mem != NULL)
			instance = new (mem) TelGeneralLogicNetCtlDefault(bbmcAdapter);
		break;
	}
	}

	*outNetCtl = instance;
	return res;
}

/* 0x8125F494 */
TelephonyResult TelGeneralLogicNetCtlFactory::Destroy(TelGeneralLogicNetCtl *netCtl)
{
	if (netCtl != NULL) {
		netCtl->~TelGeneralLogicNetCtl();
		telFree(netCtl);
	}
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
