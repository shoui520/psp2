/*
 * The library's checks: a message with the line, function and file, which the release build
 * prints nowhere (0x81010c60 is empty). No binary names it; the names are ours.
 */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_DEBUG_H
#define NP_ACTIVITY_COMMON_ACTIVITY_DEBUG_H

void activityDebugPrint(int level, int line, const char *function, const char *file);

/* Sony's build tree: the file names its checks print */
#define ACTIVITY_SRC_DIR	"W:\\Build\\J01509657\\vsh\\app\\common\\src\\np\\activity\\"

#define ACTIVITY_CHECK(function)	activityDebugPrint(0, __LINE__, function, ACTIVITY_FILE)

#endif
