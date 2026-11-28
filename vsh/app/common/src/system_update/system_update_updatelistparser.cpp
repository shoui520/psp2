/*
 * system_update_updatelistparser.cpp: whether an update applies to this console, and the
 * update list's XML parser (0x81135CA0-0x81135E8C; a file of ours: no record or string names
 * Sony's): bXCe's SAX parser fed the list as it is downloaded, with the list's processor as
 * its handler.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/new>
#include <bXCe/bxce_internal.h>
#include <updater_core/updater_core.h>
#include <vshbridge/vshbridge.h>

#include "system_update_registry.h"
#include "system_update_updatelistparser.h"

#define BXCE_RESULT_PARSE_IN_PROGRESS	0x00851001	/* PLACEHOLDER name: XML_ParseInProgress */

/* whether the list's own package may be installed: always */
int Bgdl_81135CA0(bool *ok)
{
	*ok = true;
	return 0;
}

/* whether an update of the version type applies: on a retail console only to another version */
int Bgdl_81135CA8(int type, bool *ok)
{
	SceUpdaterDeviceInfo info;
	bool any = false;
	int ret;

	*ok = false;
	ret = vshSblAimgrIsCEX();
	if (ret >= 0) {
		if (ret == 1) {
			ret = sceUpdaterGetDeviceInfo(&info);
			any = false;
			if (ret == 0)
				any = info.m_qaf != 1;
		} else {
			any = true;
		}
	}
	if (ret >= 0) {
		if (any) {
			*ok = true;
		} else if ((SceUInt32)type == info.m_version) {
			*ok = false;
			return 0;
		} else {
			*ok = true;
		}
	}
	return ret;
}

/* whether the version type is newer than the console's */
int Bgdl_81135D24(int type, bool *ok)
{
	SceUpdaterDeviceInfo info;
	int ret;

	*ok = false;
	ret = sceUpdaterGetDeviceInfo(&info);
	if (ret == 0 && (SceUInt32)type > info.m_version)
		*ok = true;
	return ret;
}

UpdateListParser::UpdateListParser()
{
	m_reserved = false;
}

UpdateListParser::~UpdateListParser()
{
}

SystemUpdateResult UpdateListParser::Bgdl_81135D9C(UpdateListProcessor *handler)
{
	SystemUpdateResult result;
	int error = 0;

	if (handler == NULL) {
		result.detail = 0;
		result.error = SYSTEM_UPDATE_ERROR;
		return result;
	}
	m_parser = NULL;
	m_handler = handler;
	m_parser = new bXCe::Sax::Parser();
	if (m_parser == NULL) {
		error = SYSTEM_UPDATE_ERROR;
	} else if (m_parser->initialize(NULL) != 0) {
		error = SYSTEM_UPDATE_ERROR;
	} else {
		m_parser->setResolveEntity(false);
		m_parser->setSkipIgnorableWhiteSpace(true);
		m_parser->setDocumentHandler(m_handler);
	}
	result.error = error;
	result.detail = 0;
	return result;
}

void UpdateListParser::Bgdl_81135E1A(void)
{
	if (m_parser != NULL) {
		m_parser->terminate();
		delete m_parser;
	}
}

SystemUpdateResult UpdateListParser::Bgdl_81135E32(const char *data, int size)
{
	SystemUpdateResult result;
	int error = 0;
	int detail = 0;

	if (m_parser == NULL) {
		result.detail = 0;
		result.error = SYSTEM_UPDATE_ERROR;
		return result;
	}
	int ret = m_parser->parse(data, size, false);

	if (ret != BXCE_RESULT_PARSE_IN_PROGRESS) {
		error = SYSTEM_UPDATE_ERROR;
		detail = ret;
		ret = m_handler->Bgdl_8113329E();
		if (ret != 0)
			detail = ret;
	}
	result.error = error;
	result.detail = detail;
	return result;
}
