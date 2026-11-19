/*
 * The free-trial license files (ftt_license.cpp): the license they hold, and the functions that
 * read them and change them under their lock.
 */
#ifndef _FTT_LICENSE_H
#define _FTT_LICENSE_H

#include <scetypes.h>
#include <common/shared_ptr.h>
#include <file/file.h>

/* a free-trial license, the 80 bytes after its file's header; the seed makes the MAC's key */
struct GameFttLicense {
	SceUInt64 macSeed;
	char contentId[48];
	SceUInt64 time1;
	SceUInt64 time2;
	int otherField;
	SceUInt32 flags;	/* bit 0: GameFttInfo's flag */
};

/* a free-trial license file read, the size of the data after it, a flag set */
int gameFttLicenseRead(paf::common::SharedPtr<paf::File> file, GameFttLicense *license);
int gameFttLicenseGetDataSize(paf::common::SharedPtr<paf::File> file, SceSize *size);
int gameFttLicenseSetFlag(paf::common::SharedPtr<paf::File> file, SceUInt32 flag);

#endif /* _FTT_LICENSE_H */
