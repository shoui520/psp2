/*
 * The build paf was made in: Sony's build folder, which the paths compiled into paf name
 * (__FILE__ in Sony's build). Each firmware's paf is a build of its own.
 */
#ifndef PAF_PAF_BUILD_H
#define PAF_PAF_BUILD_H

#if defined(FW_TYPE_CEX)
#define PAF_BUILD "J01509657"
#elif defined(FW_TYPE_TOOL)
#define PAF_BUILD "J01509658"
#else
#define PAF_BUILD "J01508051"
#endif

/* the folder paf's sources were in, as Sony's build wrote it */
#define PAF_BUILD_ROOT "W:\\Build\\" PAF_BUILD "\\vsh\\paf\\"

#endif
