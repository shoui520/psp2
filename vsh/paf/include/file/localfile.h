/* paf::LocalFile: a file of the file system (C view in src/file/localfile.h). */
#ifndef PAF_FILE_LOCALFILE_H
#define PAF_FILE_LOCALFILE_H

#include <file/file.h>
#include <common/shared_ptr.h>

namespace paf {

class LocalFile : public File {
public:
	class OpenArg : public File::OpenArg {
	public:
		const char *filename;
		uint32_t flag;
		uint32_t mode;
	};

	static common::SharedPtr<LocalFile> Open(const char *path, uint32_t flag, uint32_t mode,
						 int32_t *error);
};

}

#endif
