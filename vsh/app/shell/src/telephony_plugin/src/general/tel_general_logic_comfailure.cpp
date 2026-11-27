/*
 * tel_general_logic_comfailure.cpp: COM modem failure rolling-window counter
 * and Low Power Mode (LPM) state manager
 * (3.65-CEX: 0x812693C4..0x81269BFF).
 */
#include <kernel/processmgr.h>
#include "tel_general_internal.h"

extern "C" {
SceInt64 sceKernelGetSystemTimeWide(void);
}

namespace telephony {

/* ------------------------------------------------------------------
 * TelGeneralComFailureCounter (0x812693C4..0x8126958B)
 * ------------------------------------------------------------------ */

/* 0x812693C4 */
TelGeneralComFailureCounter::TelGeneralComFailureCounter()
	: m_windowUsec(0),
	  m_maxFailures(0),
	  m_windowStartUsec(0),
	  m_thresholdExceeded(false)
{
}

/* 0x812693DC */
TelGeneralComFailureCounter::~TelGeneralComFailureCounter()
{
	m_failureCodes.clear();
}

/* 0x812693F8 */
void TelGeneralComFailureCounter::SetThreshold(int maxFailures, int windowSeconds)
{
	m_maxFailures = (unsigned int)maxFailures;
	m_windowUsec = (SceUInt64)windowSeconds * 1000000ULL;
}

/* 0x81269424 */
TelephonyResult TelGeneralComFailureCounter::RecordFailure(int reasonCode)
{
	m_failureCodes.push_back(reasonCode);

	SceUInt64 nowUsec = (SceUInt64)sceKernelGetSystemTimeWide();
	if (m_windowStartUsec != 0 && (nowUsec - m_windowStartUsec) <= m_windowUsec) {
		if (m_failureCodes.size() >= m_maxFailures)
			m_thresholdExceeded = true;
	} else {
		m_windowStartUsec = nowUsec;
		m_failureCodes.clear();
		m_failureCodes.push_back(reasonCode);
		m_thresholdExceeded = false;
	}

	return TelephonyResult(TEL_OK);
}

/* 0x81269546 */
void TelGeneralComFailureCounter::Reset()
{
	m_windowStartUsec = 0;
	m_thresholdExceeded = false;
	m_failureCodes.clear();
}

/* 0x81269568 */
int TelGeneralComFailureCounter::GetFailureCodeAt(unsigned int index) const
{
	if (index < m_failureCodes.size())
		return m_failureCodes[index];
	return 0;
}

/* ------------------------------------------------------------------
 * TelGeneralLpmManager (0x8126958C..0x81269BFF)
 * ------------------------------------------------------------------ */

/* 0x8126958C */
TelGeneralLpmManager::TelGeneralLpmManager(TelBbmcAdapterInterface *bbmcAdapter)
	: TelObservable("SceTelGenLpmMgr"),
	  m_bbmcAdapter(bbmcAdapter),
	  m_stateMutex("SceTelGenLpmMgrSt", 0),
	  m_forcedLpm(false),
	  m_currentLpm(true)
{
}

/* 0x812695FC */
TelGeneralLpmManager::~TelGeneralLpmManager()
{
}

/* 0x812696A0 */
TelephonyResult TelGeneralLpmManager::SetBbmcLpm(bool enableLpm, bool syncCall)
{
	TelephonyResult res(TEL_OK);
	if (m_bbmcAdapter != NULL) {
		if (syncCall)
			res = m_bbmcAdapter->SetLowPowerModeSync(enableLpm);
		else
			res = m_bbmcAdapter->SetLowPowerModeAsync(enableLpm);
	}

	m_stateMutex.Lock();
	m_currentLpm = enableLpm;
	m_stateMutex.Unlock();
	return res;
}

/* 0x81269740 */
TelephonyResult TelGeneralLpmManager::ApplyWwanEnableChange(bool wwanEnabled)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue wwanAttr(TEL_ATTR_ENABLE_WWAN);
	TelephonyResult res = attrMgr->GetAttribute(&wwanAttr);
	if (!res.IsSuccess())
		return res;

	bool currentWwanEnabled = (wwanAttr.intValue != 0);
	if (currentWwanEnabled == wwanEnabled)
		return res;

	TelAttributeValue flightAttr(TEL_ATTR_FLIGHT_MODE);
	res = attrMgr->GetAttribute(&flightAttr);
	if (!res.IsSuccess())
		return res;

	bool flightModeOn = (flightAttr.intValue != 0);

	m_stateMutex.Lock();
	bool forcedLpm = m_forcedLpm;
	m_stateMutex.Unlock();

	bool targetLpm = true;
	if (wwanEnabled && !flightModeOn)
		targetLpm = forcedLpm;

	return SetBbmcLpm(targetLpm, false);
}

/* 0x812698D0 */
TelephonyResult TelGeneralLpmManager::ApplyFlightModeChange(bool flightModeOn)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue wwanAttr(TEL_ATTR_ENABLE_WWAN);
	TelephonyResult res = attrMgr->GetAttribute(&wwanAttr);
	if (!res.IsSuccess())
		return res;

	bool wwanEnabled = (wwanAttr.intValue != 0);

	m_stateMutex.Lock();
	bool forcedLpm = m_forcedLpm;
	m_stateMutex.Unlock();

	bool targetLpm = true;
	if (wwanEnabled && !flightModeOn)
		targetLpm = forcedLpm;

	return SetBbmcLpm(targetLpm, true);
}

/* 0x812699D2 */
TelephonyResult TelGeneralLpmManager::ApplyCurrentAttributes()
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue wwanAttr(TEL_ATTR_ENABLE_WWAN);
	TelephonyResult res = attrMgr->GetAttribute(&wwanAttr);
	if (!res.IsSuccess())
		return res;

	TelAttributeValue flightAttr(TEL_ATTR_FLIGHT_MODE);
	res = attrMgr->GetAttribute(&flightAttr);
	if (!res.IsSuccess())
		return res;

	bool wwanEnabled = (wwanAttr.intValue != 0);
	bool flightModeOn = (flightAttr.intValue != 0);

	m_stateMutex.Lock();
	bool forcedLpm = m_forcedLpm;
	m_stateMutex.Unlock();

	bool targetLpm = true;
	if (wwanEnabled && !flightModeOn)
		targetLpm = forcedLpm;

	return SetBbmcLpm(targetLpm, false);
}

/* 0x81269B6A */
TelephonyResult TelGeneralLpmManager::SetLowPowerModeImmediate(bool enableLpm)
{
	return SetBbmcLpm(enableLpm, false);
}

/* 0x81269BBC */
TelephonyResult TelGeneralLpmManager::SetForcedLpm(bool forced)
{
	m_stateMutex.Lock();
	m_forcedLpm = forced;
	m_stateMutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x81269BDC */
TelephonyResult TelGeneralLpmManager::GetCurrentLpmState(bool *outIsLpm)
{
	m_stateMutex.Lock();
	*outIsLpm = m_currentLpm;
	m_stateMutex.Unlock();
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
