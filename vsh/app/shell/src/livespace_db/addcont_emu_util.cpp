/*
 * addcont_emu_util.cpp: LSDB addcont parent-title lookup helpers and
 * ur0:emu_list/list.dat PSP emulator title filter (0x81371C70..0x81372BEF).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "livespace_db_internal.h"

#define ADDCONT_DIR_KEY_COUNT		8
#define ADDCONT_TITLE_ID_COPY_LEN	10
#define ADDCONT_TIMESTAMP_LEN		24
#define PSPEMU_LIST_MAX_ENTRIES		10000
#define PSPEMU_LIST_ENTRY_LEN		10
#define PSPEMU_LIST_BUFFER_SIZE		(PSPEMU_LIST_MAX_ENTRIES * PSPEMU_LIST_ENTRY_LEN)

extern "C" {
int sceRegMgrGetKeyIntForUser(const char *category, const char *name, int *outVal);
}

struct EmuListEntry {
	char titleId[PSPEMU_LIST_ENTRY_LEN];
};

/* (0x815C6FF0..0x815C700C, initialized at 0x81372B06) */
static unsigned int s_addcontDirKeys[ADDCONT_DIR_KEY_COUNT] = {
	LsdbKeyHash("INSTALL_DIR_ADDCONT"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_1"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_2"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_3"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_4"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_5"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_6"),
	LsdbKeyHash("INSTALL_DIR_ADDCONT_ADD_7"),
};

/* (0x81371C70) Checks whether an addcont title is registered under any app in LSDB */
int shellAddcontCheckInstalled(const char *addcontTitleId, int *outOnMemoryCard)
{
	if (addcontTitleId == NULL)
		return 0;

	paf::vector<paf::string> matchedTitles;
	AppInfoQClient client;
	SceLsdb_A6D8772C(&client);

	int found = 0;
	if (SceLsdb_E90B81CD(&client) == 0) {
		for (int keyIndex = 0; keyIndex < ADDCONT_DIR_KEY_COUNT; ++keyIndex) {
			LsdbQueryCondition cond;
			cond.key = s_addcontDirKeys[keyIndex];
			cond.op = LSDB_QUERY_OP_EQUAL;
			cond.mask = 0;
			cond.next = NULL;

			LsdbValue targetVal(addcontTitleId);
			cond.values.push_back(targetVal);

			SceLsdb_76B75713(&client, &cond, &matchedTitles, 0);
			if (matchedTitles.size() != 0) {
				if (outOnMemoryCard != NULL)
					*outOnMemoryCard = 1;

				LsdbAppInfoPtr appInfo(new LsdbAppInfo());
				if (SceLsdb_D239031A(&client, &matchedTitles[0], &appInfo) == 0) {
					const char *orgPath = LsdbValueText((*appInfo)[LSDB_KEY_ORG_PATH]);
					if (orgPath != NULL &&
					    sce_paf_strncmp(orgPath, "gro0:", sce_paf_strlen("gro0:")) == 0) {
						LsdbValue &attrVal = (*appInfo)[LSDB_KEY_ATTRIBUTE];
						SceInt64 attrBits = (attrVal.type == LSDB_VALUE_INTEGER)
							? attrVal.integer : 0;
						if ((attrBits & 1) == 0 && outOnMemoryCard != NULL)
							*outOnMemoryCard = 0;
					}
				}
				found = 1;
				break;
			}
		}
	}

	SceLsdb_E2101B03(&client);
	return found;
}

/* (0x81371F32) Returns 1 if addcontTitleId matches any INSTALL_DIR_ADDCONT* of appTitleId */
int shellAddcontMatchAppTitle(const char *addcontTitleId, const char *appTitleId)
{
	if (addcontTitleId == NULL || appTitleId == NULL)
		return 0;

	AppInfoQClient client;
	SceLsdb_A6D8772C(&client);

	int matched = 0;
	if (SceLsdb_E90B81CD(&client) >= 0) {
		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		paf::string appTitleStr(appTitleId);
		if (SceLsdb_D239031A(&client, &appTitleStr, &appInfo) >= 0) {
			for (int keyIndex = 0; keyIndex < ADDCONT_DIR_KEY_COUNT; ++keyIndex) {
				const char *dirTitle = LsdbValueText((*appInfo)[s_addcontDirKeys[keyIndex]]);
				if (dirTitle != NULL && dirTitle[0] != '\0' &&
				    sce_paf_strncmp(dirTitle, addcontTitleId, ADDCONT_TITLE_ID_COPY_LEN) == 0) {
					matched = 1;
					break;
				}
			}
		}
	}

	SceLsdb_E2101B03(&client);
	return matched;
}

/* (0x81372130) Returns 1 if addcontTitleId matches any INSTALL_DIR_ADDCONT* in an appInfo map */
int shellAddcontMatchAppInfo(const char *addcontTitleId, const LsdbAppInfoPtr *appInfo)
{
	if (addcontTitleId == NULL || appInfo == NULL || appInfo->get() == NULL)
		return 0;

	for (int keyIndex = 0; keyIndex < ADDCONT_DIR_KEY_COUNT; ++keyIndex) {
		const char *dirTitle = LsdbValueText((**appInfo)[s_addcontDirKeys[keyIndex]]);
		if (dirTitle != NULL && dirTitle[0] != '\0' &&
		    sce_paf_strncmp(dirTitle, addcontTitleId, ADDCONT_TITLE_ID_COPY_LEN) == 0) {
			return 1;
		}
	}
	return 0;
}

/* (0x81372196) Resolves the owning parent application title ID for an addcont title ID */
int shellAddcontResolveParentTitle(const char *addcontTitleId, char *outParentTitleId)
{
	if (addcontTitleId == NULL || outParentTitleId == NULL)
		return 0;

	paf::vector<paf::string> candidates;
	AppInfoQClient client;
	SceLsdb_A6D8772C(&client);

	if (SceLsdb_E90B81CD(&client) != 0) {
		SceLsdb_E2101B03(&client);
		return 0;
	}

	/* First search the base INSTALL_DIR_ADDCONT key */
	LsdbQueryCondition baseCond;
	baseCond.key = s_addcontDirKeys[0];
	baseCond.op = LSDB_QUERY_OP_EQUAL;
	baseCond.mask = 0;
	baseCond.next = NULL;
	LsdbValue baseVal(addcontTitleId);
	baseCond.values.push_back(baseVal);

	SceLsdb_76B75713(&client, &baseCond, &candidates, 0);
	if (candidates.size() == 1) {
		sce_paf_strncpy(outParentTitleId, candidates[0].c_str(), ADDCONT_TITLE_ID_COPY_LEN);
		SceLsdb_E2101B03(&client);
		return 1;
	}

	if (candidates.size() > 1) {
		char latestLaunchTime[ADDCONT_TIMESTAMP_LEN];
		sce_paf_memset(latestLaunchTime, 0, sizeof(latestLaunchTime));
		unsigned int bestIndex = 0;

		for (unsigned int curIndex = 0; curIndex < candidates.size(); ++curIndex) {
			LsdbAppInfoPtr appInfo(new LsdbAppInfo());
			if (SceLsdb_D239031A(&client, &candidates[curIndex], &appInfo) == 0) {
				const char *lltime = LsdbValueText((*appInfo)[LSDB_KEY_LLTIME]);
				if (sce_paf_strncmp(lltime, latestLaunchTime, sizeof(latestLaunchTime)) > 0) {
					sce_paf_memcpy(latestLaunchTime, lltime, sizeof(latestLaunchTime));
					bestIndex = curIndex;
				}
			}
		}

		sce_paf_strncpy(outParentTitleId, candidates[bestIndex].c_str(), ADDCONT_TITLE_ID_COPY_LEN);
		SceLsdb_E2101B03(&client);
		return 1;
	}

	/* Otherwise search INSTALL_DIR_ADDCONT_ADD_1..7 */
	for (int keyIndex = 1; keyIndex < ADDCONT_DIR_KEY_COUNT; ++keyIndex) {
		paf::vector<paf::string> extraMatches;
		LsdbQueryCondition extraCond;
		extraCond.key = s_addcontDirKeys[keyIndex];
		extraCond.op = LSDB_QUERY_OP_EQUAL;
		extraCond.mask = 0;
		extraCond.next = NULL;
		LsdbValue extraVal(addcontTitleId);
		extraCond.values.push_back(extraVal);

		SceLsdb_76B75713(&client, &extraCond, &extraMatches, 0);
		for (unsigned int matchIndex = 0; matchIndex < extraMatches.size(); ++matchIndex)
			candidates.push_back(extraMatches[matchIndex]);
	}

	if (candidates.size() == 1) {
		sce_paf_strncpy(outParentTitleId, candidates[0].c_str(), ADDCONT_TITLE_ID_COPY_LEN);
		SceLsdb_E2101B03(&client);
		return 1;
	}

	if (candidates.size() == 0) {
		SceLsdb_E2101B03(&client);
		return 0;
	}

	char latestLaunchTime[ADDCONT_TIMESTAMP_LEN];
	sce_paf_memset(latestLaunchTime, 0, sizeof(latestLaunchTime));
	unsigned int bestIndex = 0;

	for (unsigned int curIndex = 0; curIndex < candidates.size(); ++curIndex) {
		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		if (SceLsdb_D239031A(&client, &candidates[curIndex], &appInfo) == 0) {
			const char *lltime = LsdbValueText((*appInfo)[LSDB_KEY_LLTIME]);
			if (sce_paf_strncmp(lltime, latestLaunchTime, sizeof(latestLaunchTime)) > 0) {
				sce_paf_memcpy(latestLaunchTime, lltime, sizeof(latestLaunchTime));
				bestIndex = curIndex;
			}
		}
	}

	sce_paf_strncpy(outParentTitleId, candidates[bestIndex].c_str(), ADDCONT_TITLE_ID_COPY_LEN);
	SceLsdb_E2101B03(&client);
	return 1;
}

/* (0x813728C8) Checks whether a PSP title ID is permitted by ur0:emu_list/list.dat */
bool shellPspEmuCheckList(const char *titleId)
{
	if (titleId == NULL)
		return false;

	int enableList = 0;
	if (sceRegMgrGetKeyIntForUser("/CONFIG/PSPEMU", "enable_list", &enableList) == 0 &&
	    enableList == 0) {
		return true;
	}

	paf::vector<EmuListEntry> entries;
	int openErr = 0;
	paf::common::SharedPtr<paf::File> file =
		paf::LocalFile::Open("ur0:emu_list/list.dat", 1, 0, &openErr);
	if (file.get() == NULL)
		return false;

	int listMode = 0;
	char *rawBuffer = static_cast<char *>(sce_paf_malloc(PSPEMU_LIST_BUFFER_SIZE));
	if (rawBuffer != NULL) {
		sce_paf_memset(rawBuffer, 0, PSPEMU_LIST_BUFFER_SIZE);
		if (file->Read(&listMode, sizeof(listMode)) != 0) {
			unsigned int bytesRead =
				static_cast<unsigned int>(file->Read(rawBuffer, PSPEMU_LIST_BUFFER_SIZE));
			if (bytesRead != 0) {
				unsigned int count = bytesRead / PSPEMU_LIST_ENTRY_LEN;
				const char *cursor = rawBuffer;
				for (unsigned int curIndex = 0; curIndex < count; ++curIndex) {
					EmuListEntry record;
					sce_paf_memcpy(record.titleId, cursor, PSPEMU_LIST_ENTRY_LEN);
					entries.push_back(record);
					cursor += PSPEMU_LIST_ENTRY_LEN;
				}
			} else {
				openErr = -1;
			}
		} else {
			openErr = -1;
		}
	} else {
		openErr = -1;
	}

	file->Close();
	if (rawBuffer != NULL)
		sce_paf_free(rawBuffer);

	if (openErr < 0)
		return false;

	for (unsigned int curIndex = 0;
	     curIndex < entries.size() && curIndex < PSPEMU_LIST_MAX_ENTRIES;
	     ++curIndex) {
		if (sce_paf_strncmp(entries[curIndex].titleId, titleId, PSPEMU_LIST_ENTRY_LEN) == 0)
			return listMode == 0;
	}

	return listMode != 0;
}
