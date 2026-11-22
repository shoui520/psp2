/*
 * indicator_telephony.cpp: the information bar's telephony listener. telephony_plugin's modem
 * gives its general state whenever it changes; what changed is handed to the main thread as
 * events, each given to the function the item showing it set (0x811685C0-0x81168DF0). The
 * file's and the names are ours (PLACEHOLDER; the events' queue names are Sony's).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <thread/thread.h>
#include "../../../plugins.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "indicator_item.h"

using namespace paf;

/* the SIM card's conditions (PLACEHOLDER names): usable, and the four it is not */
#define INDICATOR_SIM_READY		1
#define INDICATOR_SIM_CONDITION_2	2
#define INDICATOR_SIM_CONDITION_3	3
#define INDICATOR_SIM_CONDITION_4	4
#define INDICATOR_SIM_CONDITION_5	5

/* the registration's value when registered */
#define INDICATOR_TEL_REGISTERED	1

void (*g_indicatorSignalLevelCB)(int level, void *arg) = NULL;		/* (0x81544A10) */
void *g_indicatorSignalLevelArg = NULL;
void (*g_indicatorNetworkTypeCB)(int type, void *arg) = NULL;			/* (0x81544A18) */
void *g_indicatorNetworkTypeArg = NULL;
void (*g_indicatorRoamingCB)(int roaming, void *arg) = NULL;			/* (0x81544A20) */
void *g_indicatorRoamingArg = NULL;
void (*g_indicatorOperatorNameCB)(TelephonyOperatorNames names, void *arg) = NULL;
void *g_indicatorOperatorNameArg = NULL;					/* (0x81544A28) */
void (*g_indicatorSimConditionCB)(int condition, void *arg) = NULL;		/* (0x81544A30) */
void *g_indicatorSimConditionArg = NULL;

/* (0x81544A38) read by nothing */
int g_indicatorTelReserved = 1;

/* the SIM card usable, as its condition last told (0x81544A3C) */
static bool s_simReady = true;

/* the modem's general state last told (0x81566940) */
static TelephonyGeneralInfo s_lastInfo;

/* an event handed to the main thread (PLACEHOLDER names) */
class IndicatorTelEvent {
public:
	virtual ~IndicatorTelEvent()
	{
	}

	virtual void Run() = 0;
};

class IndicatorTelSignalLevel : public IndicatorTelEvent {
public:
	IndicatorTelSignalLevel(int level) : m_level(level)
	{
	}

	virtual void Run();

	int m_level;
};

class IndicatorTelNetworkType : public IndicatorTelEvent {
public:
	IndicatorTelNetworkType(int type) : m_type(type)
	{
	}

	virtual void Run();

	int m_type;
};

class IndicatorTelRoamingInfo : public IndicatorTelEvent {
public:
	IndicatorTelRoamingInfo(int roaming) : m_roaming(roaming)
	{
	}

	virtual void Run();

	int m_roaming;
};

class IndicatorTelOperatorNameEvent : public IndicatorTelEvent {
public:
	IndicatorTelOperatorNameEvent(const TelephonyOperatorNames &names) : m_names(names)
	{
	}

	virtual void Run();

	TelephonyOperatorNames m_names;
};

class IndicatorTelSimCondition : public IndicatorTelEvent {
public:
	IndicatorTelSimCondition(int condition) : m_condition(condition)
	{
	}

	virtual void Run();

	int m_condition;
};

/* an event run on the main thread, and deleted (0x811685C0) */
static void indicatorTelEventRun(void *data)
{
	IndicatorTelEvent *event = (IndicatorTelEvent *)data;

	event->Run();
	delete event;
}

/* (0x811685D8) */
void IndicatorTelSignalLevel::Run()
{
	if (g_indicatorSignalLevelCB != NULL)
		g_indicatorSignalLevelCB(m_level, g_indicatorSignalLevelArg);
}

/* the telephony listener's general network type event: its value handed to the main thread
   (0x811685F8) */
void indicatorTelGeneralNetworkTypeCB(int event, int result, const int *value, int size)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("TelGeneralNetworkType",
							 indicatorTelEventRun,
							 new IndicatorTelNetworkType(*value));
}

/* (0x8116863C) */
void IndicatorTelNetworkType::Run()
{
	if (g_indicatorNetworkTypeCB != NULL)
		g_indicatorNetworkTypeCB(m_type, g_indicatorNetworkTypeArg);
}

/* (0x8116865E) */
void IndicatorTelRoamingInfo::Run()
{
	if (g_indicatorRoamingCB != NULL)
		g_indicatorRoamingCB(m_roaming, g_indicatorRoamingArg);
}

/* (0x8116867E) */
void IndicatorTelOperatorNameEvent::Run()
{
	if (g_indicatorOperatorNameCB != NULL)
		g_indicatorOperatorNameCB(m_names, g_indicatorOperatorNameArg);
}

/* (0x8116871C) */
void IndicatorTelSimCondition::Run()
{
	if (g_indicatorSimConditionCB != NULL)
		g_indicatorSimConditionCB(m_condition, g_indicatorSimConditionArg);
}

/* an event handed to the main thread under its queue name */
#define INDICATOR_TEL_POST(name, event)							\
	thread::CallQueue::main_thread_callqueue.Enqueue(name, indicatorTelEventRun, event)

/*
 * The modem's general state told: what changed since it was told last handed on as events.
 * A SIM card that is not usable is no service; while it is usable, the registration gives the
 * network attached or detached and the signal level (no service while the modem is too hot),
 * the operator's names are given every time, and leaving or taking the flight mode is told as
 * both the signal and the network type (0x8116873E).
 */
static void indicatorTelGeneralListener(int event, int result, const TelephonyGeneralInfo *data,
					int size)
{
	TelephonyGeneralInfo info = *data;

	if (info.simCondition != s_lastInfo.simCondition) {
		INDICATOR_TEL_POST("TelGeneralSimCondition",
				   new IndicatorTelSimCondition(info.simCondition));
		if (info.simCondition == INDICATOR_SIM_CONDITION_2) {
			s_simReady = false;
			INDICATOR_TEL_POST("TelGeneralSignalLevel",
					   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_NO_SERVICE));
		} else if (info.simCondition == INDICATOR_SIM_CONDITION_3) {
			s_simReady = false;
			INDICATOR_TEL_POST("TelGeneralSignalLevel",
					   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_NO_SERVICE));
		} else if (info.simCondition == INDICATOR_SIM_CONDITION_4 ||
			   info.simCondition == INDICATOR_SIM_CONDITION_5) {
			s_simReady = false;
			INDICATOR_TEL_POST("TelGeneralSignalLevel",
					   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_NO_SERVICE));
		} else if (info.simCondition == INDICATOR_SIM_READY) {
			s_simReady = true;
		}

	}
	if (s_simReady) {
		if (info.registration == INDICATOR_TEL_REGISTERED) {
			if (info.heatCondition == 0)
				INDICATOR_TEL_POST("TelGeneralSignalLevel",
						   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_REGISTERED));
			INDICATOR_TEL_POST("TelGeneralNetworkType",
					   new IndicatorTelNetworkType(INDICATOR_NETWORK_ATTACHED));
		} else {
			if (info.signalLevel > 0 && info.heatCondition == 0)
				INDICATOR_TEL_POST("TelGeneralSignalLevel",
						   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_UNREGISTERED));
			INDICATOR_TEL_POST("TelGeneralNetworkType",
					   new IndicatorTelNetworkType(INDICATOR_NETWORK_DETACHED));
		}
	}
	if (s_simReady) {
		if (info.heatCondition == 0)
			INDICATOR_TEL_POST("TelGeneralSignalLevel",
					   new IndicatorTelSignalLevel(info.signalLevel));
		else
			INDICATOR_TEL_POST("TelGeneralSignalLevel",
					   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_NO_SERVICE));
	}
	if (info.signalLevel == 0)
		INDICATOR_TEL_POST("TelGeneralNetworkType",
				   new IndicatorTelNetworkType(INDICATOR_NETWORK_HIDE));
	if (info.networkType != s_lastInfo.networkType)
		INDICATOR_TEL_POST("TelGeneralNetworkType",
				   new IndicatorTelNetworkType(info.networkType));
	if (info.roaming != s_lastInfo.roaming)
		INDICATOR_TEL_POST("TelGeneralRoamingInfo", new IndicatorTelRoamingInfo(info.roaming));
	if (s_simReady)
		INDICATOR_TEL_POST("TelGeneralOperatorName",
				   new IndicatorTelOperatorNameEvent(info.operatorNames));
	if (info.flightMode != s_lastInfo.flightMode) {
		if (info.flightMode == 1) {
			INDICATOR_TEL_POST("TelGeneralSignalLevel",
					   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_FLIGHT_ON));
			INDICATOR_TEL_POST("TelGeneralNetworkType",
					   new IndicatorTelNetworkType(INDICATOR_NETWORK_TYPE_103));
		}
		if (info.flightMode == 0)
			INDICATOR_TEL_POST("TelGeneralSignalLevel",
					   new IndicatorTelSignalLevel(INDICATOR_SIGNAL_FLIGHT_OFF));
	}
	s_lastInfo = info;
}

/* the modem's general state listened to, everything told the first time (0x81168D24) */
void indicatorTelephonyListen(void)
{
	TelephonyPluginInterface *tel =
		(TelephonyPluginInterface *)applauncherPluginGetInterface("telephony_plugin", 1);

	if (tel == NULL || tel->getModem() == NULL)
		return;

	TelephonyModem *modem = tel->getModem();

	if (!modem->vtable->needsInitialCheck(modem))
		return;
	s_lastInfo.networkType = -1;
	s_lastInfo.registration = -1;
	s_lastInfo.signalLevel = -1;
	s_lastInfo.roaming = -1;
	s_lastInfo.simCondition = -1;
	s_lastInfo.flightMode = -1;
	s_lastInfo.heatCondition = -1;
	modem = tel->getModem();

	if (modem->vtable->addGeneralListener(modem, indicatorTelGeneralListener, NULL) != 0)
		sce_paf_printf_dummy();
}

/* (0x81168D86) */
void indicatorTelephonyUnlisten(void)
{
	TelephonyPluginInterface *tel =
		(TelephonyPluginInterface *)applauncherPluginGetInterface("telephony_plugin", 1);

	if (tel == NULL || tel->getModem() == NULL)
		return;

	TelephonyModem *modem = tel->getModem();

	if (!modem->vtable->needsInitialCheck(modem))
		return;
	modem = tel->getModem();
	modem->vtable->removeGeneralListener(modem, indicatorTelGeneralListener, NULL);
}
