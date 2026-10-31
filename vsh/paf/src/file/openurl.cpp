#include <file/openurl.h>

namespace paf {

common::SharedPtr<File> OpenUrl::Open(const char *path, uint32_t flag, uint32_t mode,
				      int32_t *error)
{
	bool ignored = false;
	int32_t ret = 0;
	common::SharedPtr<File> file;
	list<OpenFunc>::iterator it = m_open_func_list.begin();

	while (it != m_open_func_list.end()) {
		if (*it != NULL) {
			file = (*it)(path, flag, mode, &ret, &ignored);
			if (!ignored) {
				if (error != NULL)
					*error = ret;
				return file;
			}
		}
		++it;
	}
	if (error != NULL)
		*error = -1;
	return common::SharedPtr<File>();
}

bool OpenUrl::IsRegisteredOpenFunc(OpenFunc func) const
{
	list<OpenFunc> &funcs = const_cast<list<OpenFunc> &>(m_open_func_list);

	for (list<OpenFunc>::iterator it = funcs.begin(); it != funcs.end(); ++it) {
		if (*it == func)
			return true;
	}
	return false;
}

int32_t OpenUrl::RegisterOpenFunc(OpenFunc func)
{
	if (func == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	if (IsRegisteredOpenFunc(func))
		return 0;
	m_open_func_list.push_back(func);
	return 0;
}

int32_t OpenUrl::UnregisterOpenFunc(OpenFunc func)
{
	if (func == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	for (list<OpenFunc>::iterator it = m_open_func_list.begin();
	     it != m_open_func_list.end(); ++it) {
		if (*it == func) {
			m_open_func_list.erase(it);
			return 0;
		}
	}
	return 0;
}

OpenUrl::OpenUrl()
{
}

OpenUrl::~OpenUrl()
{
}

}
