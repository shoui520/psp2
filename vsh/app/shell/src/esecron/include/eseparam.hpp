/*
 * eseparam.hpp: a task's param file (eseparam.cpp; 0.990's records name its open, read, write
 * and commit). Made, it holds the param files' lock until it is destroyed. The class name is
 * ours (PLACEHOLDER); its functions are not written yet.
 */
#ifndef _ESECRON_ESEPARAM_HPP
#define _ESECRON_ESEPARAM_HPP

#include <stddef.h>
#include <scetypes.h>
#include <std/string>

class BgdlTaskPath;

class EseParam {
public:
	EseParam(SceUInt32 id);					/* (0x811222B6) */
	~EseParam();						/* (0x811222E4) */
	int open(void);						/* (0x81122304) for reading */
	int openWrite(void);					/* (0x81122844) PLACEHOLDER name */
	int read(int key, void *value, SceSize size);		/* (0x81122476) a number */
	int read(int key, BgdlTaskPath *value);			/* (0x811224F2) a string */
	int read(int key, paf::string *value);			/* (0x811225B6) */
	int commit(void);					/* (0x81122F8E) */
	int write(int key, const void *value, SceSize size);	/* (0x811231E8) */
	int write(int key, const BgdlTaskPath *value);		/* (0x81123396) a string */

	SceUInt32 id;
	int result;			/* 0x80103A11 until it is opened */
	int reserved[4];		/* PLACEHOLDER: named with eseparam.cpp */
};

/* a task's param file opened, read or written and closed (PLACEHOLDER names: not written yet) */
int Bgdl_8112260C(SceUInt32 id, int key, void *value, SceSize size);		/* read */
int Bgdl_811233A4(SceUInt32 id, int key, const void *value, SceSize size);	/* write */
int Bgdl_8112278A(SceUInt32 id, int key, paf::string *value);		/* read a string */
int Bgdl_81122700(SceUInt32 id, int key, BgdlTaskPath *value);		/* read a string into a path */

#endif
