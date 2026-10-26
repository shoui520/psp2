/*
 * Build selection: the image (MOTION_IMAGE, 0 for 00, 1 for 01), given by the Makefile.
 * Image 00 is Sony's branch U0652, image 01 the trunk.
 */
#ifndef MOTION_H
#define MOTION_H

#ifndef MOTION_IMAGE
#error "MOTION_IMAGE not set"
#endif

/* the image's version, at its head */
#define MOTION_USER_VERSION	(MOTION_IMAGE == 0 ? 0x0652 : 0x1010)

#endif
