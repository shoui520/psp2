/*
 * system_update_mtphttpfile.h: a paf::File that reads a file the PC serves over the MTP
 * connection (system_update_mtphttpfile.cpp, a file name of ours: no record names it). Every
 * name is ours (PLACEHOLDER) except the lock's, "MtpHttpFileLock".
 */
#ifndef _SYSTEM_UPDATE_MTPHTTPFILE_H
#define _SYSTEM_UPDATE_MTPHTTPFILE_H

#include <stddef.h>
#include <scetypes.h>
#include <file/file.h>
#include <thread/thread.h>

#include "../mtp/mtpr2/mtpr2.h"

#define MTP_HTTP_FILE_TYPE		257	/* PLACEHOLDER names */
#define MTP_HTTP_FILE_CAPABILITY	87

/* what an MtpHttpFile is opened with */
struct MtpHttpFileOpenArg : public paf::File::OpenArg {
	int kind;			/* 1 or 2, the download's kind */
	const char *url;
	int reserved;
	SceInt64 offset;
	int bufferSize;
	int bufferPad;
	SceUInt32 options[3];		/* PLACEHOLDER: kept as they are */
};

/* the file's states */
enum MtpHttpFileState {
	MTP_HTTP_FILE_CLOSED,
	MTP_HTTP_FILE_OPENING,
	MTP_HTTP_FILE_OPENED
};

class MtpHttpFile : public paf::File {
public:
	MtpHttpFile();

	virtual int32_t GetFileType() const;
	virtual int32_t GetCapability() const;
	virtual int64_t GetFileSize() const;
	virtual int32_t Open(const OpenArg *param);
	virtual int32_t OpenAsync(const OpenArg *param);
	virtual int32_t Close();
	virtual int32_t CloseAsync();
	virtual bool IsOpened() const;
	virtual bool IsEof();
	virtual int32_t Abort();
	virtual int32_t Read(void *buf, size_t nbyte);
	virtual int32_t ReadAsync(void *buf, size_t nbyte);
	virtual int32_t Write(const void *buf, size_t nbyte);
	virtual int32_t WriteAsync(const void *buf, size_t nbyte);
	virtual int64_t Seek(int64_t offset, int32_t whence);
	virtual int32_t SeekAsync(int64_t offset, int32_t whence);
	virtual int32_t Flush();
	virtual int32_t FlushAsync();
	virtual int32_t WaitAsync(int64_t *result);
	virtual int32_t PollAsync(int64_t *result);
	virtual int32_t Getstat(paf::FileStat *stat);
	virtual ~MtpHttpFile();

	int Bgdl_81134E8A(const MtpHttpFileOpenArg *param);		/* one try to open */

	int m_state;			/* MtpHttpFileState */
	paf::thread::RMutex m_lock;
	MTPR2Args m_request;
	int m_client;			/* the download's file handle, -1 none */
	MTPR2FileAttributes m_info;
	SceUInt32 m_offset;		/* where the next read starts */
	int m_error;			/* the MTP error Open mapped, 0 none */
	bool m_aborted;

private:
	int connect(void);
	int fetchInfo(void);
};


#endif
