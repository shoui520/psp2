/* Cached notification fields populated by the native GPIO completion path. */
#include "syscon_private.h"

int SceSysconForDriver_012B57B3(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_POWER_BIT14];
}

int SceSysconForDriver_03C50DC3(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N2_BIT4];
}

int SceSysconForDriver_142D5E82(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_HEADPHONE];
}

int ksceSysconIsLowBatteryInhibitUpdateReboot(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_LOW_BATTERY_INHIBIT_UPDATE_REBOOT];
}

int SceSysconForDriver_1E08A40F(void)
{
	return s_sysconControl.command885State;
}

int ksceSysconIsLowBatteryInhibitUpdateDownload(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_LOW_BATTERY_INHIBIT_UPDATE_DOWNLOAD];
}

int SceSysconForDriver_27758A64(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_POWER_SOURCE];
}

int SceSysconForDriver_29CF4335(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N3_BIT1];
}

int SceSysconForDriver_2E4A2198(void)
{
	return s_sysconControl.command884State;
}

int SceSysconForDriver_32B2DB3D(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_POWER_BIT12];
}

int SceSysconForDriver_490C5548(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_MOTION_DEV];
}

int SceSysconForDriver_4A184B7C(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N2_BIT2];
}

int SceSysconForDriver_4BC63A40(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N1_BIT0];
}

int SceSysconForDriver_50CAE242(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_THERMAL_ALERT];
}

int SceSysconForDriver_63B14156(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N2_BIT6];
}

int SceSysconForDriver_86BAAF7D(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N0_BIT3];
}

int SceSysconForDriver_92D2C6A4(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N1_BIT6];
}

int SceSysconForDriver_99A254A9(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N1_BIT1];
}

int SceSysconForDriver_9A4F4B7C(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N1_BIT2];
}

int ksceSysconIsDownLoaderMode(void)
{
	return (s_sysconControl.statusFlags >> 6) & 1;
}

int SceSysconForDriver_9DA2A5AB(void)
{
	return (s_sysconControl.statusFlags >> 3) & 1;
}

int SceSysconForDriver_9F4042F8(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N3_BIT2];
}

int SceSysconForDriver_A57B5433(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N0_BIT4];
}

int SceSysconForDriver_ACC7F71E(void)
{
	int value = s_sysconControl.notificationState[SYSCON_STATE_MULTI_CN];

	return value < 0 ? 0 : value;
}

int SceSysconForDriver_ACEE1C70(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N2_BIT3];
}

int SceSysconForDriver_B7BCC638(void)
{
	return s_sysconControl.notificationBits;
}

int SceSysconForDriver_B832B72C(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N0_BIT2];
}

int SceSysconForDriver_B9EA2FA8(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N1_BIT7];
}

int SceSysconForDriver_BFDA5590(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_USB_POWER];
}

int SceSysconForDriver_C0F215B7(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_WLAN_BT];
}

int SceSysconForDriver_C3504ADE(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N0_BIT1];
}

int SceSysconForDriver_C50568E9(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_N3_BIT3];
}

int ksceSysconGetHardwareInfo(void)
{
	return s_sysconControl.hardwareInfo;
}

int SceSysconForDriver_D7F5A797(void)
{
	return s_sysconControl.notificationState[SYSCON_STATE_LOW_BATTERY];
}

int SceSysconForDriver_EBE3262C(void)
{
	return s_sysconControl.statusFlags;
}

int ksceSysconGetBaryonVersion(void)
{
	return s_sysconControl.baryonVersion;
}

