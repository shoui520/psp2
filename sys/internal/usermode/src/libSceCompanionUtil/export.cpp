/*
 * The library's exports: each passes its arguments to the instance's code.
 */
#include "companion_util_internal.h"

using namespace SceCompanionUtil;

extern "C" {

int sceCompanionUtilInit(const CompanionUtilInitParam *param)
{
	return CCompanionUtil::Init(param);
}

int sceCompanionUtilTerm(void)
{
	return CCompanionUtil::Term();
}

int sceCompanionUtilStop(void)
{
	return CCompanionUtil::Abort();
}

int SceCompanionUtil_7AB433EC(SceUInt16 *status)
{
	return CCompanionUtil::QueryStatus(status);
}

int sceCompanionUtilSetCallbackHandlerInternal(SceCompanionUtilEventHandler handler, int index)
{
	return CCompanionUtil::SetCallbackHandler(handler, index);
}

int sceCompanionUtilSearchToOrbis(void)
{
	return CCompanionUtil::Search();
}

int SceCompanionUtil_53445DBF(CompanionUtilTarget *target)
{
	return CCompanionUtil::UpdateTarget(target);
}

int SceCompanionUtil_2B0EF5F8(const CompanionUtilTarget *target,
			      const CompanionUtilHostIdList *list)
{
	return CCompanionUtil::ConnectToOrbis(target, list);
}

int SceCompanionUtil_C93BB8F0(const CompanionUtilTarget *target)
{
	return CCompanionUtil::LaunchOrbis(target);
}

int sceCompanionUtilPowerOnOrbisForRemotePlay(const CompanionUtilTarget *target)
{
	return CCompanionUtil::PowerOnOrbisForRemotePlay(target);
}

int SceCompanionUtil_52AA755B(void)
{
	return CCompanionUtil::RequestText();
}

int SceCompanionUtil_04BC85DC(int composeStart, int composeLength, SceUInt32 caret,
			      int replaceStart, int replaceLength, const SceWChar16 *text)
{
	return CCompanionUtil::InputText(composeStart, composeLength, caret, replaceStart,
					 replaceLength, text);
}

int SceCompanionUtil_18D13C74(int entered)
{
	return CCompanionUtil::CloseText(entered);
}

int SceCompanionUtil_CC52810C(int arg0, int arg1)
{
	return CCompanionUtil::SendValue(arg0, arg1);
}

} /* extern "C" */
