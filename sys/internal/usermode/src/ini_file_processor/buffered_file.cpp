/*
 * SceIniFileProcessor: the temporary file with a write buffer.
 */

#include "ini_internal.h"

IniBufferedFile::IniBufferedFile(IniAllocatorRef *allocator, SceSize capacity) : IniTempFile(allocator)
{
	m_capacity = capacity;
	m_buffer = NULL;
	m_used = 0;
}

IniBufferedFile::~IniBufferedFile()
{
	if (m_buffer != NULL) {
		iniFree(m_allocator, m_buffer);
		m_buffer = NULL;
	}
}

int IniBufferedFile::write(const void *buf, SceSize size)
{
	int result;

	if (m_buffer != NULL || (m_buffer = (char *)iniAlloc(m_allocator, m_capacity)) != NULL)
		result = 0;
	else
		result = SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	if (result == 0) {
		SceSize used = m_used;

		if (m_capacity < size) {
			if (used != 0) {
				result = IniFile::write(m_buffer, used);
				if (result != 0)
					return result;
				m_used = 0;
			}
			result = IniFile::write(buf, size);
			if (result == 0)
				return 0;
			return result;
		}
		if (m_capacity < used + size) {
			if (used != 0) {
				result = IniFile::write(m_buffer, used);
				if (result != 0)
					return result;
				used = 0;
				m_used = 0;
			}
		}
		sceClibMemcpy_safe(m_buffer + used, buf, size);
		m_used += size;
		return 0;
	}
	return result;
}

/* writes what the buffer holds and gives the buffer back */
int IniBufferedFile::sync()
{
	int result;

	if (m_used != 0) {
		result = IniFile::write(m_buffer, m_used);
		if (result != 0)
			return result;
		m_used = 0;
	}
	if (m_buffer != NULL) {
		iniFree(m_allocator, m_buffer);
		m_buffer = NULL;
	}
	return 0;
}
