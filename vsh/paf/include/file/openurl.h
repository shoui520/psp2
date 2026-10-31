/*
 * paf::OpenUrl: a list of open functions, each a way to open a URL as a File.
 * Open asks each in turn until one does not set *ignored.
 */
#ifndef PAF_FILE_OPENURL_H
#define PAF_FILE_OPENURL_H

#include <file/file.h>
#include <common/shared_ptr.h>
#include <std/list>

namespace paf {

class OpenUrl {
public:
	typedef common::SharedPtr<File> (*OpenFunc)(const char *path, uint32_t flag,
						    uint32_t mode, int32_t *error,
						    bool *ignored);

	OpenUrl();
	~OpenUrl();

	int32_t RegisterOpenFunc(OpenFunc func);
	int32_t UnregisterOpenFunc(OpenFunc func);
	bool IsRegisteredOpenFunc(OpenFunc func) const;
	common::SharedPtr<File> Open(const char *path, uint32_t flag, uint32_t mode,
				     int32_t *error);

private:
	list<OpenFunc> m_open_func_list;
};

}

#endif
