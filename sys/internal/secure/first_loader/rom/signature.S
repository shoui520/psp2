/*
 * The ROM's last 0x80 bytes: the copyright notice, and the build date as BCD
 * (FL_BUILD_DATE, YYYYMMDD) in the final word.
 */

#include "first_loader.h"

	.section .signature, "a"
	.ascii	"Copyright (C) 2011 Sony Computer Entertainment Inc. All rights reserved."
	.org	0x7c, 0
	.word	FL_BUILD_DATE
