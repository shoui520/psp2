/*
 * The library's checks: a message with the line, function and file, which the release build
 * prints nowhere (0x81010c60 is empty). No binary names it; the names are ours.
 */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_DEBUG_H
#define NP_ACTIVITY_COMMON_ACTIVITY_DEBUG_H

#include <fw.h>

void activityDebugPrint(int level, int line, const char *function, const char *file);

/* Sony's build tree: the file names its checks print, under one build per firmware */
#if defined(FW_TYPE_CEX)
#define ACTIVITY_BUILD		"J01509657"
#elif FW_VERSION == 0x03630011
#define ACTIVITY_BUILD		"J01508051"
#else
#define ACTIVITY_BUILD		"J01509658"
#endif
#define ACTIVITY_SRC_DIR	"W:\\Build\\" ACTIVITY_BUILD "\\vsh\\app\\common\\src\\np\\activity\\"

/* a condition the release build only passes on (1: it holds) */
#define ACTIVITY_ASSERT(condition, function) \
	activityDebugPrint((condition), __LINE__, function, ACTIVITY_FILE)

#define ACTIVITY_CHECK(function)	activityDebugPrint(0, __LINE__, function, ACTIVITY_FILE)

#endif
