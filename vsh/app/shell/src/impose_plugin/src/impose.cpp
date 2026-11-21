/*
 * impose.cpp: impose_plugin's object, its interface and its pages (0x81140CF0-0x81141400).
 * The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <system/system.h>
#include <common/main_thread_call_list.h>
#include "../../livespace_db/lsdb_keys.h"
#include "impose.h"

using namespace paf;

/* (0x81564B3C, 0x81564B40, 0x81564B44) */
ImposePluginInterface *g_imposeInterface;
Plugin *g_imposePlugin;
static ImposePluginInterface s_interface;

/* the plugin's object (0x81564BB0) */
Impose *g_impose;

/* (0x81140CF0) */
void imposeInterfaceRegister(Plugin *plugin)
{
	g_imposePlugin = plugin;
	g_imposeInterface = &s_interface;
	plugin->SetInterface(1, &s_interface);
}

static const wchar_t *imposeGetString(const IDParam &id);
static void Impose_8114115A(void);
static void Impose_81141170(void);
static int Impose_81141186(void);
static void Impose_8114119C(bool arg);
static void Impose_811411B4(bool arg);
static void Impose_811411CC(void);
static void Impose_811411E0(int arg1, int arg2);
static void imposeZoomRefuse(void);
static void Impose_81141210(int arg);
static void Impose_81141228(int arg);
static void Impose_81141240(void);
static void imposeZoomBegin(void);
static void imposeZoomEnd(void);
static void imposeZoomPrepare(void);
static void imposeZoomShow(int x, int y);
static void imposeZoomHide(int x, int y);
static int imposeZoomIsBlocked(void);
static void Impose_811412E2(void);
static void Impose_811412F8(void);
static void Impose_8114130E(int arg);
static void Impose_81141326(void);
static void Impose_8114133C(int arg);
static void Impose_81141354(void);
static void imposeUpdate(void *userdata);

/* the parts made, and the interface's functions given (0x81140D18) */
Impose::Impose(Plugin *plugin)
	: m_plugin(plugin), m_page(NULL), m_volume(plugin), m_notification(plugin),
	  m_screenshot(plugin), m_part8115013C(plugin), m_part81150574(plugin),
	  m_parentalControl(plugin), m_part8114B0DC(plugin), m_part8114F348(plugin),
	  m_part8114FD14(plugin), m_part8115000C(plugin), m_touchEmulation(plugin)
{
	g_impose = this;

	g_imposeInterface->getString = imposeGetString;
	g_imposeInterface->reserved0[0] = Impose_8114115A;
	g_imposeInterface->reserved0[1] = Impose_81141170;
	g_imposeInterface->reserved1[1] = (void (*)(void))Impose_81141186;
	g_imposeInterface->reserved0[2] = (void (*)(void))Impose_8114119C;
	g_imposeInterface->reserved1[2] = (void (*)(void))Impose_811411B4;
	g_imposeInterface->reserved2[6] = Impose_811411CC;
	g_imposeInterface->reserved2[7] = (void (*)(void))Impose_811411E0;
	g_imposeInterface->zoomRefuse = imposeZoomRefuse;
	g_imposeInterface->reserved1[0] = (void (*)(void))Impose_81141210;
	g_imposeInterface->reserved1[3] = (void (*)(void))Impose_81141228;
	g_imposeInterface->reserved1[4] = Impose_81141240;
	g_imposeInterface->zoomBegin = imposeZoomBegin;
	g_imposeInterface->zoomEnd = imposeZoomEnd;
	g_imposeInterface->zoomShow = imposeZoomShow;
	g_imposeInterface->zoomHide = imposeZoomHide;
	g_imposeInterface->zoomPrepare = imposeZoomPrepare;
	g_imposeInterface->zoomIsBlocked = imposeZoomIsBlocked;
	g_imposeInterface->reserved2[0] = Impose_811412E2;
	g_imposeInterface->reserved2[1] = Impose_811412F8;
	g_imposeInterface->reserved2[2] = (void (*)(void))Impose_8114130E;
	g_imposeInterface->reserved2[3] = Impose_81141326;
	g_imposeInterface->reserved2[4] = (void (*)(void))Impose_8114133C;
	g_imposeInterface->reserved2[5] = Impose_81141354;
}

/* (0x81140ED6) */
void Impose::Create(Plugin *plugin)
{
	new Impose(plugin);
}

/* (0x81140EEE) */
Impose::~Impose()
{
	g_impose = NULL;
}

/* (0x81140F6A) */
void Impose::Destroy()
{
	delete g_impose;
}

/* the plugin's two pages opened, and the parts that need them told (0x81140F82) */
void Impose::OpenPages()
{
	{
		Plugin::PageOpenParam param;

		m_page = m_plugin->PageOpen(IDParam(0x5FC861D1), param);
	}
	{
		Plugin::PageOpenParam param;

		param.overwrite_draw_priority = 7;
		m_page2 = m_plugin->PageOpen(IDParam(0xF61972E0), param);
	}
	m_screenshot.Open();
	m_part8114F348.Impose_8114F4BE();
}

/* (0x811410AE) */
void Impose::Start()
{
	g_impose->OpenPages();
	g_impose->m_volume.Start();
	g_impose->m_part8115013C.Impose_81150164();
	g_impose->m_part81150574.Impose_81150630();
	if (system::Model::GetType() == 1)
		g_impose->m_touchEmulation.Start();
	common::MainThreadCallList::Register(imposeUpdate, NULL);
}

/* (0x81141100) */
void Impose::Stop()
{
	common::MainThreadCallList::Unregister(imposeUpdate, NULL);
	g_impose->m_part8115013C.Impose_811501D0();
	g_impose->m_part81150574.Impose_811508FE();
	if (system::Model::GetType() == 1)
		g_impose->m_touchEmulation.Stop();
}

/* the interface's functions, each passed to the part that does it (0x81141144-0x81141366) */
static const wchar_t *imposeGetString(const IDParam &id)
{
	return g_impose->m_plugin->GetString(id);
}

static void Impose_8114115A(void)
{
	g_impose->m_part8114B0DC.Impose_8114C460();
}

static void Impose_81141170(void)
{
	g_impose->m_part8114B0DC.Impose_8114AA16();
}

static int Impose_81141186(void)
{
	return g_impose->m_part8114B0DC.Impose_81135260();
}

static void Impose_8114119C(bool arg)
{
	g_impose->m_part8114B0DC.Impose_8114F232(arg);
}

static void Impose_811411B4(bool arg)
{
	g_impose->m_part8114B0DC.Impose_8114DDBE(arg);
}

static void Impose_811411CC(void)
{
	g_impose->m_volume.IsFixed();
}

static void Impose_811411E0(int arg1, int arg2)
{
	g_impose->m_part8114FD14.Impose_8114FD50(arg1, arg2);
}

static void imposeZoomRefuse(void)
{
	g_impose->m_part8115000C.Impose_8115002C();
}

static void Impose_81141210(int arg)
{
	g_impose->m_part8115013C.Impose_811503CE(arg);
}

static void Impose_81141228(int arg)
{
	g_impose->m_part811516E4.Impose_811518CA(arg);
}

static void Impose_81141240(void)
{
	g_impose->m_part811516E4.Impose_8115171C();
}

static void imposeZoomBegin(void)
{
	g_impose->m_zoom.Impose_81151DC0();
}

static void imposeZoomEnd(void)
{
	g_impose->m_zoom.Impose_81152072();
}

static void imposeZoomPrepare(void)
{
	g_impose->m_zoom.Impose_81152130();
}

static int imposeZoomIsBlocked(void)
{
	return g_impose->m_zoom.Impose_81152718();
}

static void imposeZoomShow(int x, int y)
{
	g_impose->m_zoom.Impose_811521DC(x, y);
}

static void imposeZoomHide(int x, int y)
{
	g_impose->m_zoom.Impose_81152468(x, y);
}

static void Impose_811412E2(void)
{
	g_impose->m_touchEmulation.Lock();
}

static void Impose_811412F8(void)
{
	g_impose->m_touchEmulation.Unlock();
}

static void Impose_8114130E(int arg)
{
	g_impose->m_parentalControl.Open(arg);
}

static void Impose_81141326(void)
{
	g_impose->m_parentalControl.Close();
}

static void Impose_8114133C(int arg)
{
	g_impose->m_parentalControl.ShowNotice(arg);
}

static void Impose_81141354(void)
{
	g_impose->m_notification.MakeSpace();
}

/* each frame on the main thread (0x81141368) */
static void imposeUpdate(void *userdata)
{
	g_impose->m_notification.Update();
	g_impose->m_part8114F348.Impose_8114FAC4();
	g_impose->m_part81150574.Impose_81151192();
}
