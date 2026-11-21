/*
 * eseparam.hpp: a task's param file (eseparam.cpp; 0.990's records name its open, read, write
 * and commit, and the task-number wrappers get and set). Made, it holds the param files' lock
 * until it is destroyed. The class names are ours (PLACEHOLDER).
 *
 * The file is d0.pdb or d1.pdb in the task's folder, f0.pdb saying which: read from d1 while
 * f0 is there, else from d0; written to the other, then f0 made or removed to switch them. It
 * holds a count, then entries of a key, the room its value has and the value's size, each
 * followed by its value.
 */
#ifndef _ESECRON_ESEPARAM_HPP
#define _ESECRON_ESEPARAM_HPP

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/file.h>

#define BGDL_ERROR_PARAM_CLOSED		0x80103A11	/* PLACEHOLDER name: the param file is not open */
#define BGDL_ERROR_PARAM_IO		0x80103A12	/* PLACEHOLDER name: a param file read or write fell short */

class BgdlTaskPath;

/* an entry's head in the param file (PLACEHOLDER names) */
struct EseParamEntry {
	int key;			/* 0 for an entry given up */
	SceSize capacity;		/* the room its value has */
	SceSize size;			/* its value's size */
};

/* the param file (PLACEHOLDER name) */
class EseParamFile {
public:
	EseParamFile(SceUInt32 taskId) : id(taskId), result(BGDL_ERROR_PARAM_CLOSED), count(0)
	{
	}

	~EseParamFile();					/* (0x81121BDA) */
	int close(void);					/* (0x81121AEC) */
	int find(int key, EseParamEntry *entry);		/* (0x81121C36) */
	int readCount(void);					/* (0x81121CF2) */

	/* 1 when fewer than size bytes were read */
	int readData(void *buf, SceSize size)
	{
		SceSSize n = 0;

		if (file.get() != NULL)
			n = file->Read(buf, size);
		if (n != (SceSSize)size)
			return 1;
		return n == (SceSSize)size ? 0 : BGDL_ERROR_PARAM_IO;
	}

	/* 1 when fewer than size bytes were written */
	int writeData(const void *buf, SceSize size)
	{
		SceSSize n = 0;

		if (file.get() != NULL)
			n = file->Write(buf, size);
		if (n != (SceSSize)size)
			return 1;
		return n == (SceSSize)size ? 0 : BGDL_ERROR_PARAM_IO;
	}

	SceUInt32 id;
	int result;			/* 0x80103A11 until it is opened */
	paf::common::SharedPtr<paf::File> file;
	int count;			/* the file's first word */
};

class EseParam : public EseParamFile {
public:
	__attribute__((noinline)) EseParam(SceUInt32 id);				/* (0x811222B6) */
	~EseParam();					/* (0x811222E4) */
	int open(void);						/* (0x81122304) for reading */
	int openWrite(void);					/* (0x81122844) PLACEHOLDER name */
	int read(int key, void *value, SceSize size);		/* (0x81122476) a number */
	int read(int key, BgdlTaskPath *value);			/* (0x811224F2) a string */
	int read(int key, paf::string *value);			/* (0x811225B6) */
	int commit(void);					/* (0x81122F8E) */
	int write(int key, const void *value, SceSize size);	/* (0x811231E8) */
	int write(int key, const BgdlTaskPath *value);		/* (0x81123396) a string */
};

/* a task's param file opened, read or written and closed (0.990's records: get and set) */
int Bgdl_8112260C(SceUInt32 id, int key, void *value, SceSize size);		/* read */
int Bgdl_8112278A(SceUInt32 id, int key, paf::string *value);		/* read a string */
int Bgdl_81122700(SceUInt32 id, int key, BgdlTaskPath *value);		/* read a string into a path */
int Bgdl_811233A4(SceUInt32 id, int key, const void *value, SceSize size);	/* write */
/* (0x81122168) the key secrets are scrambled with, read from ur0:bgdl/w/paramxor.bin or, when
   clean is set or it cannot be read, made anew from the time and written there (PLACEHOLDER) */
int Bgdl_81122168(bool clean);
/* (0x811221E0) a secret stored scrambled, read back into out (PLACEHOLDER name) */
int Bgdl_811221E0(const char *src, int max, paf::string *out);

#endif
