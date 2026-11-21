/*
 * SceVideoProfiler's calls other modules make (content_operation's deletion of a video): a
 * profile of a video file opened by path, with the files that belong to it. No SDK header and
 * no debug record names them; the names are ours, from what content_operation does with them.
 */
#ifndef LIBVIDEOPROFILER_VIDEOPROFILER_H
#define LIBVIDEOPROFILER_VIDEOPROFILER_H

#include <scetypes.h>

/* the files a video's profile names (each path is the caller's, freed with delete[]) */
class VideoProfilerFileList {
public:
	virtual void Reserved0() = 0;
	virtual void Reserved1() = 0;
	virtual void Reserved2() = 0;
	virtual int GetCount() = 0;
	virtual void Reserved4() = 0;
	virtual void GetPath(int index, char **path) = 0;
};

/* a video's profile */
class VideoProfilerContent {
public:
	virtual void Reserved0() = 0;
	virtual void Reserved1() = 0;
	virtual void Reserved2() = 0;
	virtual void Reserved3() = 0;
	virtual void Reserved4() = 0;
	virtual void Reserved5() = 0;
	virtual void GetFileList(VideoProfilerFileList **list) = 0;
	virtual void ReleaseFileList(VideoProfilerFileList *list) = 0;
};

/* the profiler, a stateless object of the caller's */
class VideoProfiler {
};

#ifdef __cplusplus
extern "C" {
#endif

/* the context the profiler works in (content_operation passes its caller's) */
int SceVideoProfiler_2DBE786B(void *context);
/* opens the profile of the video at path */
int SceVideoProfiler_4DBB7596(VideoProfiler *profiler, const char *path,
			      VideoProfilerContent **content, int flags);
/* the ID sceAppMgrLoopBackMount mounts empr0: under */
int SceVideoProfiler_B327FFA8(char *id);
/* closes a profile */
int SceVideoProfiler_E5B10A6B(VideoProfiler *profiler, VideoProfilerContent *content);

#ifdef __cplusplus
}
#endif

#endif /* LIBVIDEOPROFILER_VIDEOPROFILER_H */
