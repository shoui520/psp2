/*
 * driver_controller.cpp: DriverController (GPS/WiFi/3G/emulation power state switcher;
 * 0x81193078-0x811932A4, sti 0x81193232). Sony's class name ("DriverController::SetPowerStatus")
 * comes from its log string.
 */
#include "location_internal.h"
#include <std/string.h>

/* 0x81193078 */
int DriverController::SetEmulationFile(const char *path)
{
	sce_paf_memset(m_emuFile, 0, sizeof(m_emuFile));
	sce_paf_memcpy(m_emuFile, path, sizeof(m_emuFile) - 1);
	m_emuFileDirty = 1;
	return 0;
}

/* 0x811930A4 */
DriverController::DriverController(PositionManager *posMgr)
	: m_posMgr(posMgr)
	, m_powerStatus(0)
{
	SetEmulationFile("ux0:/data/gpsdata/gpsdata.nma");
}

/* 0x811930E6 */
void DriverController::OnUnused()
{
}

/* 0x811930FC / 0x811930E8 */
DriverController::~DriverController()
{
}

/* 0x8119310A */
int DriverController::SetPowerStatus(unsigned char status)
{
	int ret = 0;
	int emu = status & 8;

	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "DriverController::SetPowerStatus(0x%x) current[0x%x]\n",
				      status, m_powerStatus);

	if ((m_powerStatus & 8) == 0 || emu == 0 || m_emuFileDirty != 0) {
		DriverManager *dev = locationDeviceGetInstance();
		if (emu != 0) {
			int r = dev->SetGpsPower(0);
			if (r != 0)
				ret = r;
			r = dev->SetWifiPower(0);
			if (r != 0)
				ret = r;
			r = dev->Set3GPower(0);
			if (r != 0)
				ret = r;
			dev->SetAssistPower(0);
			r = dev->SetEmuFile(m_emuFile);
			if (r != 0) {
				ret = r;
			} else {
				m_emuFileDirty = 0;
				r = dev->SetEmuPower(1);
				if (r != 0)
					ret = r;
			}
		} else {
			int r = dev->SetEmuPower(0);
			if (r != 0)
				ret = r;
			r = dev->SetGpsPower((status & 1) != 0);
			if (r != 0)
				ret = r;
			r = dev->SetWifiPower((status & 2) != 0);
			if (r != 0)
				ret = r;
			r = dev->Set3GPower((status & 4) != 0);
			if (r != 0)
				ret = r;
			dev->SetAssistPower((status & 0x10) != 0);
		}
		if (ret == 0)
			m_powerStatus = status;
	}
	return ret;
}
