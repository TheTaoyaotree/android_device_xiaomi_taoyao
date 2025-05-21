/*
 * Copyright (C) 2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define MAX_BUF_SIZE 256
#define THP_CMD_BASE 1000

enum MODE_CMD {
	SET_CUR_VALUE = 0,
	GET_CUR_VALUE = 1,
};

enum MODE_TYPE {
	TOUCH_FOD_ENABLE       		= 10,
	THP_FOD_DOWNUP_CTL      	= THP_CMD_BASE + 1,
};
