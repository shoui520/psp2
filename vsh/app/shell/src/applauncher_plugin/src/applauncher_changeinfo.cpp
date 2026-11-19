/*
 * applauncher_changeinfo.cpp: an application's change information (0x810C9E24-0x810CAC4A), its
 * sce_sys/changeinfo/changeinfo_XX.xml (or changeinfo.xml) read into the game plugin's work pool
 * and parsed with bXCe's SAX parser, each newer version's text kept and given back as the
 * change log's marked-up text. The file's name and the names here are ours (PLACEHOLDER): no
 * record of the 3.6x shell names them; "AppLauncherChangeLog" is Sony's name for the pool's
 * user.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <file/localfile.h>
#include <job/job.h>
#include <memory/pool_allocator.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../../plugins.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_changeinfo.h"

using namespace paf;

/* the largest change information read: one block of the game plugin's pool */
#define APPLAUNCHER_CHANGE_INFO_MAX_SIZE	0x10000

/* the heading over a version's text, "Version %1" */
#define APPLAUNCHER_CHANGE_INFO_MSG_VERSION	0xEC28B1FE

/* empty in this build (0x810C9E24) */
void applauncherChangeInfoReserved(void)
{
}

/* (0x810C9E26) */
ApplauncherChangeInfo::~ApplauncherChangeInfo()
{
	vector<ApplauncherChangeInfoEntry *>::iterator it = m_entries.begin();

	while (it != m_entries.end()) {
		delete *it;
		it = m_entries.erase(it);
	}
}

/* (0x810C9EB6) */
int ApplauncherChangeInfo::Parse(const char *buf, int size, SceUInt32 since)
{
	bXCe::Sax::Parser *parser = new bXCe::Sax::Parser;

	if (parser == NULL)
		return APPLAUNCHER_ERROR_NO_MEMORY;

	int ret = parser->initialize(NULL);

	if (ret == 0) {
		parser->setResolveEntity(false);
		parser->setSkipIgnorableWhiteSpace(true);

		ApplauncherChangeInfoHandler *handler = new ApplauncherChangeInfoHandler(this, since);

		if (handler != NULL) {
			parser->setDocumentHandler(handler);
			ret = APPLAUNCHER_ERROR_CHANGE_INFO_PARSE;
			if (parser->parse(buf, size, true) == 0) {
				ret = 0;

				ApplauncherChangeInfoEntry **work =
					new ApplauncherChangeInfoEntry *[m_entries.size()];

				changeInfoSortEntries(this, &m_entries, 0, m_entries.size() - 1, work);
				delete[] work;
			}
		} else {
			ret = APPLAUNCHER_ERROR_NO_MEMORY;
		}
		if (handler != NULL)
			delete handler;
	}
	parser->terminate();
	delete parser;

	return ret;
}

/* (0x810C9F82) */
int ApplauncherChangeInfo::Load(job::JobItem *job, const char *dir, int language, void *arg)
{
	int err = 0;
	Plugin *plugin = Plugin::Find("game_plugin");

	if (plugin == NULL)
		return APPLAUNCHER_ERROR_NO_LIVEAREA;

	GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

	if (game == NULL)
		return APPLAUNCHER_ERROR_NO_LIVEAREA;
	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;

	char path[256];

	sce_paf_snprintf(path, sizeof(path), "%s/changeinfo_%02u.xml", dir, language);

	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &err);

	if (err < 0 && err != APPLAUNCHER_ERROR_NOT_FOUND) {
		err = APPLAUNCHER_ERROR_CHANGE_INFO_PARSE;
		return err;
	}
	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;
	if (err == APPLAUNCHER_ERROR_NOT_FOUND) {
		sce_paf_snprintf(path, sizeof(path), "%s/changeinfo.xml", dir);
		file = LocalFile::Open(path, SCE_O_RDONLY, 0, &err);
		if (err < 0) {
			err = APPLAUNCHER_ERROR_CHANGE_INFO_PARSE;
			return err;
		}
	}
	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;

	common::SharedPtr<memory::PoolAllocator> pool;
	SceOff size = file->GetFileSize();

	if (size == 0)
		return APPLAUNCHER_ERROR_NOT_FOUND;
	if (size < 0)
		return (int)size;

	void *buf = NULL;

	if (job->IsCanceled()) {
		err = APPLAUNCHER_ERROR_CANCELED;
	} else {
		if (size > APPLAUNCHER_CHANGE_INFO_MAX_SIZE)
			return APPLAUNCHER_ERROR_CHANGE_INFO_READ;
		err = ((int (*)(const char *, common::SharedPtr<memory::PoolAllocator> *))
		       game->func[GAME_PLUGIN_FUNC_GET_POOL])("AppLauncherChangeLog", &pool);
		if (err < 0) {
			file->Close();
			return err;
		}
		buf = pool->Alloc();
		if (buf == NULL) {
			err = APPLAUNCHER_ERROR_NO_MEMORY;
		} else if (job->IsCanceled()) {
			err = APPLAUNCHER_ERROR_CANCELED;
		} else {
			int ret = file->Read(buf, (size_t)size);

			if (ret < 0) {
				err = APPLAUNCHER_ERROR_CHANGE_INFO_PARSE;
			} else {
				err = ret;
				file->Close();
				file.reset();
				if (job->IsCanceled()) {
					err = APPLAUNCHER_ERROR_CANCELED;
				} else {
					err = Parse((const char *)buf, (int)size, (SceUInt32)arg);
					if (err >= 0) {
						if (job->IsCanceled())
							err = APPLAUNCHER_ERROR_CANCELED;
						else
							err = 0;
					}
				}
			}
		}
	}

	if (file.get() != NULL)
		file->Close();
	if (buf != NULL)
		pool->Free(buf);
	((void (*)(common::SharedPtr<memory::PoolAllocator> *))
	 game->func[GAME_PLUGIN_FUNC_RELEASE_POOL])(&pool);

	return err;
}

/* (0x810CA79C) */
int ApplauncherChangeInfo::AddEntry(SceUInt32 version, const string &text)
{
	ApplauncherChangeInfoEntry *entry = new ApplauncherChangeInfoEntry(version, text);

	return m_entries.push_back(entry);
}

/* (0x810CA80A) */
string ApplauncherChangeInfo::FormatVersion(const string &version)
{
	string text = version;
	Plugin *plugin = g_applauncherHome->m_plugin;

	if (plugin != NULL) {
		wstring wversion;

		common::Utf8ToUtf16(version, &wversion, 0);

		wstring heading = shellFormatText(plugin->GetString(IDParam(s_empty_name,
			APPLAUNCHER_CHANGE_INFO_MSG_VERSION)), wversion.c_str());

		common::Utf16ToUtf8(heading, &text);
	}
	return text;
}

/* (0x810CA930) */
void ApplauncherChangeInfo::GetText(string *text)
{
	string version;
	vector<ApplauncherChangeInfoEntry *>::iterator it;

	for (it = m_entries.begin(); it != m_entries.end(); ++it) {
		if (it != m_entries.begin())
			*text += "<br>";
		if (applauncherChangeInfoVersionToString((*it)->version, &version) >= 0) {
			version = FormatVersion(version);
			*text += "<h1>";
			text->append(version);
			*text += "</h1>";
			text->append((*it)->text);
		}
	}
}
