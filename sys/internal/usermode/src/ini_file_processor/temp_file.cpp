/*
 * SceIniFileProcessor: the temporary file a processor writes before it replaces the real file.
 */

#include "ini_internal.h"

static char g_tempName[64];	/* 0x81005050 */

IniTempFile::IniTempFile(IniAllocatorRef *allocator) : IniFile(allocator)
{
	m_autoDelete = true;
	iniStringAssignChars(&m_path, NULL, 0);
	m_reserved = 1;
}

IniTempFile::~IniTempFile()
{
	if (m_fd >= 0) {
		close();
		m_fd = -1;
	}
	if (m_autoDelete) {
		sceIoRemove(m_path.data);
		m_autoDelete = false;
	}
}

/* the name of the temporary file for a path: its directory (device and "temp/auto_delete/ini" under it), then its
 * file name, "_ini_", a number made from the time and ".tmp" */
int iniMakeTempName(IniFileBase *file, const char *path, IniString *name)
{
	SceRtcTick tick;
	char number[16];
	SceSize length = 0;
	SceSize device;
	const char *fileName;
	SceUInt64 ticks;

	if (path != NULL)
		length = sceClibStrnlen(path, 0x100000);
	iniStringAssignChars(name, path, length);
	if (sceRtcGetCurrentTick(&tick) == 0)
		ticks = tick.tick;
	else
		ticks = 0xdeadbeefULL;
	sceClibSnprintf(number, 16, "%6d", (int)(ticks % 1000000));
	fileName = iniPathFileName(path);
	device = iniPathDeviceLength(path);
	if (device != 0) {
		IniAllocatorRef *allocator = file->m_allocator;
		IniAllocatorOps *ops;
		IniAllocatorOps *context;
		IniString base;
		IniString part;

		if (allocator != NULL) {
			ops = allocator->ops;
			context = (IniAllocatorOps *)allocator->context;
		} else {
			ops = iniDefaultAllocator();
			context = iniDefaultAllocator();
		}
		base.data = g_iniEmptyString;
		base.length = 0;
		base.ops = ops;
		base.context = context;
		iniStringSubstr(&part, name, 0, device);
		iniStringAssign(&base, &part);
		if (part.data != g_iniEmptyString)
			part.ops->free(part.data, part.context);
		iniStringAssign(name, &base);
		iniStringAppend(name, ":", sceClibStrnlen(":", 0x100000));
		iniStringAppend(name, "temp", sceClibStrnlen("temp", 0x100000));
		sceIoMkdir(name->data, 6);
		iniStringAppend(name, "/auto_delete", sceClibStrnlen("/auto_delete", 0x100000));
		sceIoMkdir(name->data, 6);
		iniStringAppend(name, "/ini", sceClibStrnlen("/ini", 0x100000));
		sceIoMkdir(name->data, 6);
		if (fileName != NULL) {
			IniString leaf;
			IniString cut;

			leaf.data = g_iniEmptyString;
			leaf.length = 0;
			leaf.ops = ops;
			leaf.context = context;
			iniStringAssignChars(&leaf, fileName, sceClibStrnlen(fileName, 0x100000));
			iniStringAppend(name, "/", sceClibStrnlen("/", 0x100000));
			iniStringSubstr(&cut, &leaf, 0, 32);
			iniStringAppend(name, cut.data, cut.length);
			if (cut.data != g_iniEmptyString)
				cut.ops->free(cut.data, cut.context);
			if (leaf.data != g_iniEmptyString)
				leaf.ops->free(leaf.data, leaf.context);
		}
		iniStringAppend(name, "_ini_", sceClibStrnlen("_ini_", 0x100000));
		iniStringAppend(name, number, sceClibStrnlen(number, 0x100000));
		iniStringAppend(name, ".tmp", sceClibStrnlen(".tmp", 0x100000));
		if (base.data != g_iniEmptyString)
			base.ops->free(base.data, base.context);
	} else {
		SceSize n = 0;

		if (path != NULL)
			n = sceClibStrnlen(path, 0x100000);
		iniStringAssignChars(name, path, n);
		iniStringAppend(name, "_ini_file.tmp", sceClibStrnlen("_ini_file.tmp", 0x100000));
	}
	return 0;
}

int IniTempFile::open(const char *path, int mode, int flags)
{
	IniAllocatorRef *allocator = m_allocator;
	IniAllocatorOps *ops;
	IniAllocatorOps *context;
	IniString file;
	int result;

	close();
	if (allocator != NULL) {
		ops = allocator->ops;
		context = (IniAllocatorOps *)allocator->context;
	} else {
		ops = iniDefaultAllocator();
		context = iniDefaultAllocator();
	}
	file.data = g_iniEmptyString;
	file.length = 0;
	file.ops = ops;
	file.context = context;
	if (path != NULL) {
		if ((flags & 2) != 0) {
			iniMakeTempName(this, path, &file);
		} else {
			iniStringAssignChars(&file, path, sceClibStrnlen(path, 0x100000));
		}
		sceIoRemove(file.data);
		result = IniFile::open(file.data, 5, flags);
		m_autoDelete = (flags & 1) == 0;
	} else {
		sceClibStrncpy(g_tempName, "sd0:_ini_file.tmp", 0x40);
		result = IniFile::open(g_tempName, 5, flags);
	}
	if (file.data != g_iniEmptyString)
		file.ops->free(file.data, file.context);
	return result;
}

/* copies what the stream has into the file, through the buffer, in at most limit bytes of chunks */
int IniTempFile::copyFrom(IniFileBase *source, SceOff offset, SceSize limit, IniBuffer *buffer)
{
	int result = source->seek(offset, 0);
	SceSize count = 0;
	SceSSize got;

	if (result == 0) {
		SceSize size = buffer->size();
		void *data = buffer->data();

		for (;;) {
			result = source->read(data, size, &got);
			if (result == INI_RESULT_STREAM_END) {
				result = 0;
				break;
			}
			if (result == 0 && got != 0) {
				result = write(data, got);
				if (result == 0 && ++count <= limit / size)
					continue;
			}
			break;
		}
	}
	return result;
}
