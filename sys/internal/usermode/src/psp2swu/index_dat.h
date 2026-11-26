/*
 * vs0:vsh/etc/index.dat reader and build-number verification
 * (sys/internal/usermode/src/psp2swu/index_dat.h):
 * decrypts and authenticates vs0:vsh/etc/index.dat, parses its key/value
 * rows, verifies the release version against SceSblSsUpdateMgr, and reads the
 * installed device version and VSH build number.
 */
#ifndef PSP2SWU_INDEX_DAT_H
#define PSP2SWU_INDEX_DAT_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>

#define INDEX_DAT_MAX_ENTRIES	20

struct IndexDatEntry {
	paf::string key;
	paf::string value;
	paf::string detail;
};

void SplitIndexString(paf::vector<paf::string> &outParts, const paf::string &input,
		      const paf::string &delimiter, SceUInt32 maxParts);
SceInt32 ReadIndexDat(IndexDatEntry entries[INDEX_DAT_MAX_ENTRIES]);
SceInt32 ReadDeviceAndVshBuildNumbers(SceUInt32 *outDeviceVersion, SceUInt32 *outVshBuildNumber);
SceInt32 GetPupDiffBaseInfo(const char *pupPath, bool *outIsDiff, SceUInt32 *outBaseBuildNo,
			    SceUInt32 *outBaseVersion);

#endif
