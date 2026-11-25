/*
 * tel_general_logic_action_event_param.cpp: current event parameter holder
 * embedded in TelGeneralLogicAction (3.65-CEX: 0x8126933C..0x812693C3).
 */
#include "tel_general_internal.h"

namespace telephony {

/* 0x8126933C */
TelGeneralLogicActionEventParam::TelGeneralLogicActionEventParam()
	: m_event(0, -1, -1)
{
}

/* 0x81269364 */
TelGeneralLogicActionEventParam::~TelGeneralLogicActionEventParam()
{
}

/* 0x812693A0 */
void TelGeneralLogicActionEventParam::SetEvent(const TelEventInfo &event)
{
	m_event = event;
}

/* 0x812693B2 */
void TelGeneralLogicActionEventParam::GetEvent(TelEventInfo *outEvent) const
{
	if (outEvent != NULL)
		*outEvent = m_event;
}

} /* namespace telephony */
