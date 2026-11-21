/*
 * eseparam.cpp: a task's param file, and the key the secrets kept in it are scrambled with.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <timer/timer.h>

#include "../include/esebase.hpp"
#include "../include/esetask.hpp"
#include "../include/eseparam.hpp"
#include "../include/eseutil.hpp"
#include "../include/esehttp.hpp"

#include "src/livespace_db/livespace_db_internal.h"

using namespace paf;

#define PARAMXOR_PATH		"ur0:bgdl/w/paramxor.bin"

/* the param files' lock (PLACEHOLDER name) */
class BgdlParamLock {
public:
	/* (0x81416758) */
	__attribute__((noinline)) BgdlParamLock()
	{
		m_id = sceKernelCreateMutex("SceBgdlRWLockEseLock", 0x2000,
		    0, NULL);
	}

	/* (0x81416778) */
	__attribute__((noinline)) ~BgdlParamLock()
	{
		sceKernelDeleteMutex(m_id);
	}

	SceUID m_id;
};

static BgdlParamLock s_paramLock;
static SceUInt64 s_paramKey;		/* the scramble key (PLACEHOLDER name) */

int EseParamFile::close(void)
{
	if (result != 0)
		return BGDL_ERROR_PARAM_CLOSED;
	result = BGDL_ERROR_PARAM_CLOSED;
	{
		BgdlFileCloser closer(file);
	}
	return 0;
}

EseParamFile::~EseParamFile()
{
	close();
}

/* the file left just past the entry's head */
int EseParamFile::find(int key, EseParamEntry *entry)
{
	SceUInt64 size;
	SceUInt64 pos;
	int ret;

	size = file->GetFileSize();
	pos = file->Seek(4, SCE_SEEK_SET);
	while (pos < size) {
		ret = readData(entry, sizeof(*entry));
		if (ret != 0)
			return ret;
		if (entry->key == key)
			return 0;
		pos = file->Seek(entry->capacity, SCE_SEEK_CUR);
	}
	return BGDL_ERROR_INVALID_ARGUMENT;
}

int EseParamFile::readCount(void)
{
	int value;
	int ret = BGDL_ERROR_PARAM_IO;

	if (file.get() != NULL) {
		file->Seek(0, SCE_SEEK_SET);
		ret = readData(&value, sizeof(value));
		if (ret == 0)
			count = value;
		file->Seek(0, SCE_SEEK_SET);
	}
	return ret;
}

/* (0x81121D70) a new key made from the time and written to paramxor.bin (PLACEHOLDER name) */
static __attribute__((noinline)) int paramKeyWrite(SceUInt64 *key)
{
	int ret;

	*key = get_time_internal() | 0xB51F724A89DE4746ULL;
	if (Dir::IsExist(PARAMXOR_PATH) == true) {
		ret = LocalFile::RemoveFile(PARAMXOR_PATH);
		if (ret != 0)
			return ret;
	}
	common::SharedPtr<File> file = LocalFile::Open(PARAMXOR_PATH,
	    SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);
	if (ret != 0)
		return ret;
	BgdlFileCloser closer(file);
	ret = file->Write(key, sizeof(*key)) == sizeof(*key) ? 0 : BGDL_ERROR_PARAM_IO;
	return ret;
}

/* (0x81121F8E) the key read from paramxor.bin (PLACEHOLDER name) */
static __attribute__((noinline)) int paramKeyRead(SceUInt64 *key)
{
	int ret;

	common::SharedPtr<File> file = LocalFile::Open(PARAMXOR_PATH, SCE_O_RDONLY | 0x10000, 6, &ret);
	if (ret != 0)
		return ret;
	BgdlFileCloser closer(file);
	ret = file->Read(key, sizeof(*key)) == sizeof(*key) ? 0 : BGDL_ERROR_PARAM_IO;
	return ret;
}

int Bgdl_81122168(bool clean)
{
	SceUInt64 key = 0;
	int ret = 0;

	s_paramKey = 0x8FB97BFC3FF87EAFULL;
	if (clean == true || (ret = paramKeyRead(&key)) != 0)
		ret = paramKeyWrite(&key);
	s_paramKey = key;
	return ret;
}

int Bgdl_811221E0(const char *src, int max, paf::string *out)
{
	SceUInt64 key;
	char *buf;
	int len;
	int ret;
	int i;
	int j;

	if (src == NULL || sce_paf_strlen(src) == 0)
		return BGDL_ERROR_NO_KEY;
	len = sce_paf_strlen(src);
	if (len > max)
		len = max;
	buf = new char[len + 4];
	key = s_paramKey;
	if (buf != NULL) {
		sce_paf_memset(buf, 0, len + 4);
		sce_paf_strncpy(buf, src, len);
		ret = 0;
		for (i = 0, j = 0; i < len; i++) {
			if (j >= 8)
				j = 0;
			buf[i] ^= ((const char *)&key)[j++];
		}
		out->assign(buf);
	} else {
		ret = BGDL_ERROR_NO_MEMORY;
	}
	if (buf != NULL)
		delete[] buf;
	return ret;
}

EseParam::EseParam(SceUInt32 id) : EseParamFile(id)
{
	sceKernelLockMutex(s_paramLock.m_id, 1, NULL);
}

EseParam::~EseParam()
{
	sceKernelUnlockMutex(s_paramLock.m_id, 1);
}

int EseParam::open(void)
{
	BgdlTaskPath path;
	const char *name;
	int ret;

	ret = path.initialize(id, 0x1B);
	if (ret != 0)
		return ret;
	if (BgdlTaskPath::Exists(path.SetName("/f0.pdb")))
		name = path.SetName("/d1.pdb");
	else
		name = path.SetName("/d0.pdb");
	file = LocalFile::Open(name, SCE_O_RDONLY | 0x10000, 6, &result);
	if (result != 0)
		return result;
	readCount();
	return 0;
}

int EseParam::read(int key, void *value, SceSize size)
{
	EseParamEntry entry;
	int ret;

	ret = find(key, &entry);
	if (ret != 0)
		return ret;
	if (size < entry.size)
		return BGDL_ERROR_NO_KEY;
	ret = readData(value, entry.size);
	if (ret == 0)
		return 0;
	return ret;
}

int EseParam::read(int key, BgdlTaskPath *value)
{
	EseParamEntry entry;
	int ret;

	ret = find(key, &entry);
	if (ret != 0)
		return ret;
	ret = value->Reserve(entry.size);
	if (ret == 0)
		ret = readData(value->m_begin, entry.size);
	return ret;
}

int EseParam::read(int key, paf::string *value)
{
	BgdlTaskPath tmp;
	int ret;

	ret = read(key, &tmp);
	if (ret == 0) {
		SceSize len = 0;

		if (tmp.m_begin != NULL)
			len = sce_paf_strlen(tmp.m_begin);
		value->assign(tmp.m_begin, len);
	}
	return ret;
}

int Bgdl_8112260C(SceUInt32 id, int key, void *value, SceSize size)
{
	EseParam param(id);
	int ret;

	ret = param.open();
	if (ret != 0)
		return ret;
	ret = param.read(key, value, size);
	if (ret != 0)
		return ret;
	return 0;
}

int Bgdl_81122700(SceUInt32 id, int key, BgdlTaskPath *value)
{
	EseParam param(id);
	int ret;

	ret = param.open();
	if (ret != 0)
		return ret;
	ret = param.read(key, value);
	if (ret != 0)
		return ret;
	return 0;
}

int Bgdl_8112278A(SceUInt32 id, int key, paf::string *value)
{
	EseParam param(id);
	int ret;

	ret = param.open();
	if (ret != 0)
		return ret;
	ret = param.read(key, value);
	if (ret != 0)
		return ret;
	return 0;
}

/* the file not read from opened for writing, with the other's entries copied into it */
int EseParam::openWrite(void)
{
	BgdlTaskPath buf;
	BgdlTaskPath path;
	const char *name;
	SceUInt64 size;
	SceInt64 chunk;
	bool flip;
	int ret;

	ret = buf.Reserve(1024);
	if (ret != 0)
		return ret;
	ret = path.initialize(id, 0x1B);
	if (ret != 0)
		return ret;
	if (!BgdlTaskPath::Exists(path.ClearName())) {
		ret = Dir::Create(path.ClearName(), 6);
		if (ret != 0)
			return ret;
	}
	flip = BgdlTaskPath::Exists(path.SetName("/f0.pdb"));
	if (flip) {
		path.SetName("/d0.pdb");
		name = path.SetName("/d0.pdb");
	} else {
		path.SetName("/d1.pdb");
		name = path.SetName("/d1.pdb");
	}
	file = LocalFile::Open(name, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &result);
	if (result != 0)
		return result;
	if (flip)
		path.SetName("/d1.pdb");
	else
		path.SetName("/d0.pdb");
	if (!path.Exists()) {
		int value = 0;

		count = 0;
		ret = writeData(&value, sizeof(value));
		if (ret != 0)
			return ret;
		return 0;
	}
	common::SharedPtr<File> in = LocalFile::Open(path.m_begin, SCE_O_RDONLY | 0x10000, 6, &ret);
	if (ret != 0)
		return ret;
	BgdlFileCloser closer(in);
	size = in->GetFileSize();
	while (size != 0) {
		chunk = size;
		if (size >= 1024)
			chunk = 1024;
		if (in->Read(buf.m_begin, chunk) != chunk)
			return BGDL_ERROR_PARAM_IO;
		ret = writeData(buf.m_begin, chunk);
		if (ret != 0)
			return ret;
		size -= chunk;
	}
	readCount();
	return 0;
}

/* the file written made the one read from */
int EseParam::commit(void)
{
	BgdlTaskPath path;
	BgdlEvent event;
	const char *name;
	int ret;

	ret = close();
	if (ret != 0)
		return ret;
	ret = path.initialize(id, 0x1B);
	if (ret != 0)
		return ret;
	name = path.SetName("/f0.pdb");
	if (path.Exists()) {
		ret = LocalFile::RemoveFile(name);
		if (ret != 0)
			return ret;
	} else {
		common::SharedPtr<File> flag = LocalFile::Open(name,
		    SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);
		if (ret != 0)
			return ret;
		BgdlFileCloser closer(flag);
	}
	event.type = 2;
	event.id = id;
	event.result = 0;
	event.reserved = 0;
	event.value = 0;
	bgdlManagerGet()->NotifyListeners(&event);
	return 0;
}

int EseParam::write(int key, const void *value, SceSize size)
{
	EseParamEntry entry;
	int ret;

	ret = find(key, &entry);
	if (ret == BGDL_ERROR_INVALID_ARGUMENT) {
		entry.key = key;
		entry.capacity = size;
		entry.size = size;
		ret = writeData(&entry, sizeof(entry));
		if (ret != 0)
			return ret;
	} else if (ret != 0) {
		return ret;
	} else if (size != entry.capacity) {
		file->Seek(-(SceInt64)sizeof(entry), SCE_SEEK_CUR);
		if (size < entry.capacity) {
			entry.size = size;
			ret = writeData(&entry, sizeof(entry));
			if (ret != 0)
				return ret;
		} else {
			entry.key = 0;
			ret = writeData(&entry, sizeof(entry));
			if (ret != 0)
				return ret;
			file->Seek(0, SCE_SEEK_END);
			entry.key = key;
			entry.capacity = size;
			entry.size = size;
			ret = writeData(&entry, sizeof(entry));
			if (ret != 0)
				return ret;
		}
	}
	return writeData(value, size);
}

int EseParam::write(int key, const BgdlTaskPath *value)
{
	return write(key, value->m_begin, value->m_end - value->m_begin);
}

int Bgdl_811233A4(SceUInt32 id, int key, const void *value, SceSize size)
{
	EseParam param(id);
	int ret;

	ret = param.openWrite();
	if (ret != 0)
		return ret;
	ret = param.write(key, value, size);
	if (ret != 0)
		return ret;
	ret = param.commit();
	if (ret != 0)
		return ret;
	return 0;
}
