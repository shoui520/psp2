/*
 * SceContentOperation (vs0:vsh/common/content_operation.suprx): the deletion of music, photo and
 * video content from the media database, files included, for the music, photo and video
 * applications. An operation object of the caller's opens on the caller's database connection;
 * the exports are its constructor, destructor and methods. Exported by NID only: no SDK header and
 * no debug record names a type, a function or a value, so every name here is ours.
 */
#ifndef CONTENT_OPERATION_H
#define CONTENT_OPERATION_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>

/* the module's errors */
#define CONTENT_OPERATION_ERROR_UNSUPPORTED	(-2146412799)	/* 0x80105701: no handler, bad ID */
#define CONTENT_OPERATION_ERROR_NO_MEMORY	(-2146412798)	/* 0x80105702 */
#define CONTENT_OPERATION_ERROR_INVALID		(-2146412797)	/* 0x80105703: not open, bad type */
#define CONTENT_OPERATION_ERROR_UNAVAILABLE	(-2146412795)	/* 0x80105705: not open, no mount */
#define CONTENT_OPERATION_ERROR_NOT_FOUND	(-2146412788)	/* 0x8010570C */
#define CONTENT_OPERATION_ERROR_CANCELED	(-2146412787)	/* 0x8010570D */
#define CONTENT_OPERATION_ERROR_READ_FAILED	(-2146412786)	/* 0x8010570E */
#define CONTENT_OPERATION_ERROR_ALREADY_OPEN	(-2146412784)	/* 0x80105710 */

/* a content's type: the handlers and the table that list it */
#define CONTENT_OPERATION_TYPE_PHOTO	1	/* tbl_vpcontent */
#define CONTENT_OPERATION_TYPE_MUSIC	2	/* tbl_music */
#define CONTENT_OPERATION_TYPE_VIDEO	3	/* tbl_vpcontent */

namespace dbutil {
class Connection;
}

class ContentDatabase;
class ContentHandlerSet;

/* what a deletion's callback is told after each content */
typedef struct ContentOperationProgressInfo {
	int status;		/* the operation's: CONTENT_OPERATION_ERROR_CANCELED once canceled */
	SceUInt32 total;	/* the contents to delete */
	SceUInt32 done;
	SceUInt32 failed;
} ContentOperationProgressInfo;

typedef void (*ContentOperationCallback)(const ContentOperationProgressInfo *info,
					 void *userdata);

/* a deletion's progress */
struct ContentOperationProgress {
	ContentOperationProgress() : status(0), callback(NULL), callbackParam(NULL), userdata(NULL)
	{
	}

	/* count more done: the callback is told; returns the status */
	__attribute__((noinline)) int Advance(SceUInt32 count);

	int status;
	SceUInt32 total;
	SceUInt32 done;
	paf::vector<SceInt64> failed;
	ContentOperationCallback callback;
	void *callbackParam;		/* the caller's, unused */
	void *userdata;
};

/* how a deletion runs */
typedef struct ContentOperationOption {
	bool transaction;	/* the rows are deleted in one transaction */
	bool modulesLoaded;	/* the video modules are loaded already */
} ContentOperationOption;

/* a content the database lists: its ID and file */
struct ContentOperationEntry {
	ContentOperationEntry() : id(0)
	{
	}

	SceInt64 id;		/* mrid */
	paf::string path;	/* content_path */
};

/* what a read of the content to find a copy of is given */
typedef struct ContentOperationReadParam {
	SceUInt64 offset;
	SceUInt32 size;
	void *arg;
} ContentOperationReadParam;

/* reads the content to find a copy of; nonzero for an error */
typedef int (*ContentOperationReadFunc)(const ContentOperationReadParam *param,
					paf::vector<unsigned char> *data);

/* the content to find a copy of (SceContentOperation_52DCED70) */
typedef struct ContentOperationQuery {
	int type;			/* CONTENT_OPERATION_TYPE_* */
	SceUInt64 size;			/* the content's bytes */
	ContentOperationReadFunc read;	/* NULL: the first content of the size is taken */
	void *readArg;
	SceInt64 listId;		/* 0: in any list */
	SceUInt32 maxCount;		/* the contents compared at most */
	int codecType;			/* a photo's codec; 0 for any */
} ContentOperationQuery;

/* the caller's operation object */
typedef struct ContentOperation {
	ContentDatabase *database;
	ContentOperationProgress *progress;
	ContentHandlerSet *handlers;
} ContentOperation;

extern "C" {

/* constructs an operation */
ContentOperation *SceContentOperation_668CB1E5(ContentOperation *op);
/* destroys it */
ContentOperation *SceContentOperation_AF89DF37(ContentOperation *op);
/* opens it on a connection to the media database, its video deletions working in context */
int SceContentOperation_1A67467D(ContentOperation *op, dbutil::Connection *connection,
				 void *context);
/*
 * Deletes the contents ids of type in database, their files first; ids keeps the ones whose
 * files were deleted. The lists of the photos and videos deleted are deleted once empty.
 */
int SceContentOperation_70B406BD(ContentOperation *op, paf::vector<SceInt64> *ids,
				 const paf::string &database, int type,
				 const ContentOperationOption *option);
/* deletes the lists of lists that have no item left */
int SceContentOperation_16112947(ContentOperation *op, const paf::vector<SceInt64> *lists);
/* cancels the deletion running */
int SceContentOperation_7EB57B7D(ContentOperation *op);
/* the callback a deletion tells of its progress */
void SceContentOperation_F0AC6849(ContentOperation *op, ContentOperationCallback callback,
				  void *param, void *userdata);
/* the list titled title */
int SceContentOperation_CDCC7DBD(ContentOperation *op, const paf::string &title,
				 SceInt64 *listId);
/* a list title from title that no list has, at most 128 characters, "-<n>" appended */
int SceContentOperation_0D35D892(ContentOperation *op, const paf::string &title,
				 paf::string *unique);
/* the content that is a copy of query's (entry->id 0: none) */
int SceContentOperation_52DCED70(ContentOperation *op, const ContentOperationQuery *query,
				 ContentOperationEntry *entry);

}

#endif /* CONTENT_OPERATION_H */
