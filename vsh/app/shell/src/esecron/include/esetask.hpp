/*
 * esetask.hpp: the background download library's task (Sony's EseTask, "PAF_NEW EseTask" in
 * the 0.990 prototype): what the manager keeps of each download (304
 * bytes, made in 0x811000D4, filled from the task's param file in 0x810FF6F4), kept in a
 * BgdlTaskList. The param file's keys are Sony's numbers (100-116, 206, 208, 217, 220, 500,
 * 501, 505, 5002 on); no record names the other types or the fields: the names are ours
 * (PLACEHOLDER), and a field known only by the key it is read from is named by the key.
 */
#ifndef ESECRON_ESETASK_HPP
#define ESECRON_ESETASK_HPP
#include <stddef.h>
#include <scetypes.h>
#include <std/string>

#include "esebase.hpp"

/* a task's state as the manager shows it (the manager copies 60 bytes of it) */
struct BgdlTaskStatus {
	SceUInt32 id;
	int reserved;			/* PLACEHOLDER: padding before the 64-bit fields */
	SceUInt64 total;		/* key 206 */
	SceUInt64 current;		/* key 208 */
	SceUInt64 filesTotal;		/* key 505 */
	SceUInt64 filesCurrent;
	SceUInt64 filesBase;		/* the sizes of the files already done (keys 5002 on) */
	SceUInt16 fileCount;		/* key 501, plus 1 */
	SceUInt16 fileNumber;		/* key 500 */
	int remaining;			/* seconds left (BgdlProgress::GetRemaining) */
	int filesRemaining;
};

/* libmtp's background copy, as the task keeps it (PLACEHOLDER names) */
struct BgdlTaskCopy {
	paf::string name;
	int state[5];			/* PLACEHOLDER: copied from libmtp's progress, not read yet */
};

class EseTask {
public:
	SceUInt32 id;			/* the task's number, its folder's name */
	int result;			/* key 104; 0x80103A1A until it is read */
	SceUInt64 reserved0;		/* PLACEHOLDER: only cleared so far */
	BgdlProgress transfer;		/* keys 206 and 208 */
	BgdlProgress files;		/* keys 505 and 5002 on */
	bool reserved1;			/* PLACEHOLDER: only cleared so far */
	SceUInt32 type : 4;		/* key 100 */
	SceUInt32 key107 : 4;
	SceUInt32 key101 : 2;
	SceUInt32 key102 : 1;
	SceUInt32 ready : 1;		/* key 104 and key 113 both 0 */
	SceUInt32 key108 : 2;
	SceUInt32 key109 : 3;
	SceUInt32 key112 : 1;
	SceUInt32 key113 : 1;
	SceUInt32 key110 : 1;
	SceUInt32 key111 : 3;
	SceUInt32 kind : 3;		/* key 114 (BgdlTaskInfo::kind) */
	SceUInt32 key115 : 1;
	SceUInt32 key116 : 1;
	SceUInt32 flagsReserved : 4;	/* PLACEHOLDER: kept as they are */
	paf::string key220;
	paf::string key217;
	int reserved2;			/* PLACEHOLDER: only cleared so far */
	int reserved3;
	BgdlTaskStatus status;
	int copyResult;			/* from libmtp's progress */
	BgdlTaskCopy copy;
};

#endif
