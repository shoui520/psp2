/*
 * driver_controller.cpp: DriverController (GPS/WiFi/3G/emulation power state switcher;
 * 0x81193078-0x811932A4, sti 0x81193232). Sony's class name ("DriverController::SetPowerStatus")
 * comes from its log string.
 */
#include "location_service.h"
#include <std/string.h>

extern void *g_locationLogger;				/* 0x815496B8 */
extern void locationLoggerPrintf(void *logger, int level, const char *fmt, ...);

extern void *locationDeviceGetInstance(void);		/* 0x811935AE */
extern int locationDeviceSetGpsPower(void *dev, unsigned char on); /* 0x811935BA */
extern int locationDeviceSetWifiPower(void *dev, unsigned char on); /* 0x81193606 */
extern int locationDeviceSet3GPower(void *dev, unsigned char on); /* 0x81193646 */
extern int locationDeviceSetEmuPower(void *dev, unsigned char on); /* 0x81193692 */
extern int locationDeviceSetEmuFile(void *dev, const char *path); /* 0x81193A28 */
extern int locationDeviceSetExtraPower(void *dev, unsigned char on); /* 0x81193C28 */

class DriverController {
public:
	DriverController(void *posMgr);			/* 0x811930A4 */
	virtual ~DriverController();			/* 0x811930FC / 0x811930E8 */
	virtual void OnUnused();			/* 0x811930E6 */

	int SetEmulationFile(const char *path);		/* 0x81193078 */
	int SetPowerStatus(unsigned char status);	/* 0x8119310A */

	void *m_posMgr;					/* +4 */
	unsigned char m_powerStatus;			/* +8 */
	char m_emuFile[256];				/* +9 */
	unsigned char m_emuFileDirty;			/* +265 */
};

/* 0x81193078 */
int DriverController::SetEmulationFile(const char *path)
{
	sce_paf_memset(m_emuFile, 0, sizeof(m_emuFile));
	sce_paf_memcpy(m_emuFile, path, sizeof(m_emuFile) - 1);
	m_emuFileDirty = 1;
	return 0;
}

/* 0x811930A4 */
DriverController::DriverController(void *posMgr)
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
		locationLoggerPrintf(g_locationLogger, 8,
				     "DriverController::SetPowerStatus(0x%x) current[0x%x]\n",
				     status, m_powerStatus);

	if ((m_powerStatus & 8) == 0 || emu == 0 || m_emuFileDirty != 0) {
		if (emu != 0) {
			int r = locationDeviceSetGpsPower(locationDeviceGetInstance(), 0);
			if (r != 0)
				ret = r;
			r = locationDeviceSetWifiPower(locationDeviceGetInstance(), 0);
			if (r != 0)
				ret = r;
			r = locationDeviceSet3GPower(locationDeviceGetInstance(), 0);
			if (r != 0)
				ret = r;
			locationDeviceSetExtraPower(locationDeviceGetInstance(), 0);
			r = locationDeviceSetEmuFile(locationDeviceGetInstance(), m_emuFile);
			if (r != 0) {
				ret = r;
			} else {
				m_emuFileDirty = 0;
				r = locationDeviceSetEmuPower(locationDeviceGetInstance(), 1);
				if (r != 0)
					ret = r;
			}
		} else {
			int r = locationDeviceSetEmuPower(locationDeviceGetInstance(), 0);
			if (r != 0)
				ret = r;
			r = locationDeviceSetGpsPower(locationDeviceGetInstance(),
						      (status & 1) != 0);
			if (r != 0)
				ret = r;
			r = locationDeviceSetWifiPower(locationDeviceGetInstance(),
						       (status & 2) != 0);
			if (r != 0)
				ret = r;
			r = locationDeviceSet3GPower(locationDeviceGetInstance(),
						     (status & 4) != 0);
			if (r != 0)
				ret = r;
			locationDeviceSetExtraPower(locationDeviceGetInstance(),
						    (status & 0x10) != 0);
		}
		if (ret == 0)
			m_powerStatus = status;
	}
	return ret;
}
