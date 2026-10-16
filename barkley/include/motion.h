/*
 * Build selection: the image (MOTION_IMAGE, 0 for 00, 1 for 01), given by the Makefile.
 */
#ifndef MOTION_H
#define MOTION_H

#ifndef MOTION_IMAGE
#error "MOTION_IMAGE not set"
#endif

/* image 00 also drives an AMI304 magnetometer */
#define MOTION_HAS_AMI304	(MOTION_IMAGE == 0)

#endif
