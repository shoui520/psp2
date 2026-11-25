/*
 * tel_operator_apn_info.cpp: TelOperatorApnInfo (3.65-CEX: 0x813DC290..0x813DC710).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include "../tel_common.h"

namespace telephony {

/* 0x813DC290 */
TelOperatorApnInfo::TelOperatorApnInfo()
	: TelephonyOperator(), m_contractStringId(""), m_billingPlan(0),
	  m_autoSelectable(false)
{
}

/* 0x813DC2BE */
TelOperatorApnInfo::TelOperatorApnInfo(const TelephonyOperator &baseOp)
	: TelephonyOperator(baseOp), m_contractStringId(""), m_billingPlan(0),
	  m_autoSelectable(false)
{
}

/* 0x813DC328 */
TelOperatorApnInfo::~TelOperatorApnInfo()
{
}

/* 0x814536DC */
void *TelOperatorApnInfo::CloneObject() const
{
	return new TelOperatorApnInfo(*this);
}

/* 0x813DC394 */
TelephonyResult TelOperatorApnInfo::SetApnCredentials(const TelephonyOperator &credentials)
{
	return SetCredentials(credentials.GetName(), credentials.GetUserId(),
			      credentials.GetPassword());
}

/* 0x813DC3F6 */
TelephonyResult TelOperatorApnInfo::GetApnCredentials(TelephonyOperator *outCredentials) const
{
	paf::string name;
	paf::string userId;
	paf::string password;
	TelephonyResult res = GetCredentials(&name, &userId, &password);
	outCredentials->SetCredentials(name, userId, password);
	return res;
}

/* 0x813DC4C4 */
TelephonyResult TelOperatorApnInfo::SetContractStringId(const paf::string &contractStringId)
{
	m_contractStringId = contractStringId;
	return TelephonyResult(TEL_OK);
}

/* 0x813DC4DC */
void TelOperatorApnInfo::SetBillingPlan(int billingPlan)
{
	m_billingPlan = billingPlan;
}

/* 0x813DC4E2 */
const int &TelOperatorApnInfo::GetBillingPlan() const
{
	return m_billingPlan;
}

/* 0x813DC4E6 */
void TelOperatorApnInfo::SetAutoSelectable()
{
	m_autoSelectable = true;
}

/* 0x813DC4F0 */
void TelOperatorApnInfo::UnpackFromRaw(const TelOperatorApnRawRecord *raw)
{
	TelephonyOperator credentials;
	paf::string apnName(raw->apnName);
	paf::string userId(raw->userId);
	paf::string password(raw->password);
	credentials.SetCredentials(apnName, userId, password);
	SetApnCredentials(credentials);

	m_contractStringId = raw->contractStringId;
	m_billingPlan = raw->billingPlan;
	m_autoSelectable = (raw->autoSelectable != 0);
}

/* 0x813DC66E */
void TelOperatorApnInfo::PackToRaw(TelOperatorApnRawRecord *raw) const
{
	TelephonyOperator credentials;
	GetApnCredentials(&credentials);
	sce_paf_strncpy(raw->apnName, credentials.GetName().c_str(), 100);
	sce_paf_strncpy(raw->userId, credentials.GetUserId().c_str(), 126);
	sce_paf_strncpy(raw->password, credentials.GetPassword().c_str(), 126);
	sce_paf_strncpy(raw->contractStringId, m_contractStringId.c_str(), 127);
	raw->billingPlan = m_billingPlan;
	raw->autoSelectable = m_autoSelectable ? 1 : 0;
}

} /* namespace telephony */
