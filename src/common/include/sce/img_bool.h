/*
 * Copyright (C) Imagination Technologies Ltd. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Pristine declaration from img-sgx-ddk-1.7.862890/include4/img_types.h.
 * The complete upstream copyright notice and license are retained there.
 */
#pragma once

typedef enum tag_img_bool {
	IMG_FALSE = 0,
	IMG_TRUE = 1,
	IMG_FORCE_ALIGN = 0x7fffffff
} IMG_BOOL, *IMG_PBOOL;
