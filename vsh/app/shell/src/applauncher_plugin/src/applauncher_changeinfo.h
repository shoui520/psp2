/*
 * applauncher_changeinfo.h: what the change information's reader (applauncher_changeinfo.cpp)
 * and its SAX handler (applauncher_changeinfo_handler.cpp) give each other. Names ours
 * (PLACEHOLDER): no record or string of the 3.6x shell names them.
 */
#ifndef _APPLAUNCHER_CHANGEINFO_H
#define _APPLAUNCHER_CHANGEINFO_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <bXCe/bxce_internal.h>
#include "applauncher_home.h"

/*
 * PLACEHOLDER: the handler bXCe's parser calls while the change information is read: each
 * <changes app_ver="1.23"> newer than the version given, its CDATA text kept in the change
 * information (0x810CAC4C-; vtable 0x8152F4E8)
 */
class ApplauncherChangeInfoHandler : public bXCe::Sax::DocumentHandler {
public:
	ApplauncherChangeInfoHandler(ApplauncherChangeInfo *info, SceUInt32 since);

	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int skippedText(void *userData, int tokenType, const char *text, int length);

	/* the <changes> being read: its app_ver read (0, or an error), and the version */
	struct Changes {
		int result;
		SceUInt32 version;
	};

	ApplauncherChangeInfo *m_info;
	SceUInt32 m_since;
	Changes m_changes;
	bool m_newer;			/* the version is after m_since: its text is kept */
};

/*
 * PLACEHOLDER: the entries sorted by version, newest first: a merge sort of first..last through
 * the work array (0x812B046C). It is the code of the shell's other change information reader
 * (0x812B053C), not converted yet; its first argument is that reader's object, unused.
 */
void changeInfoSortEntries(void *owner, paf::vector<ApplauncherChangeInfoEntry *> *entries,
			   int first, int last, ApplauncherChangeInfoEntry **work);

#endif
