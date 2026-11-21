/*
 * system_update_updatelistparser.h: whether an update applies to this console, and the update
 * list's XML parser (system_update_updatelistparser.cpp, a file of ours: no record or string
 * names Sony's). Every name is ours (PLACEHOLDER).
 */
#ifndef _SYSTEM_UPDATE_UPDATELISTPARSER_H
#define _SYSTEM_UPDATE_UPDATELISTPARSER_H

#include <stddef.h>
#include <scetypes.h>
#include <bXCe/bxce_internal.h>
#include <system_update/system_update_job.h>
#include <system_update/system_update_updatelistprocessor.h>

/* the update list's XML parser (vtable 0x815307A8) */
class UpdateListParser {
public:
	UpdateListParser();
	virtual ~UpdateListParser();

	SystemUpdateResult Bgdl_81135D9C(UpdateListProcessor *handler);	/* made */
	void Bgdl_81135E1A(void);					/* ended */
	SystemUpdateResult Bgdl_81135E32(const char *data, int size);	/* fed */

	bool m_reserved;		/* PLACEHOLDER: only cleared */
	bXCe::Sax::Parser *m_parser;
	UpdateListProcessor *m_handler;
};

int Bgdl_81135CA0(bool *ok);
int Bgdl_81135CA8(int type, bool *ok);
int Bgdl_81135D24(int type, bool *ok);

#endif
