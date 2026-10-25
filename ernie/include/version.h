/*
 * The version blocks at 0xf00 of each program's first 4 KiB: the application's, and the
 * updater's ("Ernie Downloader", at 0x1f00), which starts with the downloader's version.
 * Kermit reads the versions with commands 0x0001 (sceSysconGetErnieVersion), 0x0005
 * (hardware info) and 0x1100 (sceSysconGetErnieDLVersion).
 */
#ifndef VERSION_H
#define VERSION_H

struct AppVersionInfo {
	unsigned char	hardwareInfo[4];	/* the hardware ID, ERNIE_HW */
	unsigned char	version[4];		/* ERNIE_FW */
};

struct UpdaterVersionInfo {
	unsigned char	dlVersion[4];
	unsigned char	hardwareInfo[4];
	unsigned char	version[4];
};

#define UPDATER_VERSION_INFO	((const struct UpdaterVersionInfo __far *)0x1f00)

extern const struct AppVersionInfo appVersionInfo;

extern void		versionInit(void);
extern void		getErnieVersion(unsigned char *buf);
extern void		getHardwareInfo(unsigned char *buf);
extern unsigned char	getHardwareInfoFlags(void);
extern void		setHardwareInfoFlags(unsigned char flags);
extern void		clearHardwareInfoFlags(unsigned char flags);
extern void		setDlFlags(unsigned char flags);
extern void		getErnieDlVersion(unsigned char *buf);

#endif
